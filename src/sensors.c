#include "sensors.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "dht.h"
#include "app_build_config.h"
#include "board_profile.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_SAMPLE_COUNT 8

static const char *TAG = "sensors";
static adc_oneshot_unit_handle_t s_adc_handle;

static int clamp_int(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static void mark_measurement_unavailable(sensor_measurement_t *measurement)
{
    measurement->available = false;
    measurement->raw = -1;
    measurement->value = NAN;
}

static int raw_to_percent(int raw, int low_raw, int high_raw)
{
    if (raw < 0 || low_raw == high_raw) {
        return -1;
    }

    int pct = 0;
    if (low_raw < high_raw) {
        pct = (raw - low_raw) * 100 / (high_raw - low_raw);
    } else {
        pct = (low_raw - raw) * 100 / (low_raw - high_raw);
    }

    return clamp_int(pct, 0, 100);
}

static int read_adc_average(adc_channel_t channel)
{
    int total = 0;
    int success_count = 0;

    for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_handle, channel, &raw) == ESP_OK) {
            total += raw;
            success_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (success_count == 0) {
        return -1;
    }

    return total / success_count;
}

static int measurement_value_as_int(const sensor_measurement_t *measurement)
{
    return measurement->available ? (int) measurement->value : -1;
}

static void read_percent_measurement(sensor_measurement_t *measurement,
                                     bool enabled,
                                     adc_channel_t channel,
                                     int low_raw,
                                     int high_raw)
{
    if (!enabled) {
        mark_measurement_unavailable(measurement);
        return;
    }

    measurement->raw = read_adc_average(channel);
    measurement->value = raw_to_percent(measurement->raw, low_raw, high_raw);
    measurement->available = measurement->raw >= 0 && measurement->value >= 0;
    if (!measurement->available) {
        measurement->value = NAN;
    }
}

static bool dht_is_enabled(void)
{
    return APP_SENSOR_TEMPERATURE_ENABLED || APP_SENSOR_AIR_ENABLED;
}

static bool read_dht_if_enabled(const board_profile_t *profile, sensor_snapshot_t *snapshot)
{
    if (!dht_is_enabled()) {
        return false;
    }

    // esp-idf-lib/dht is stateless: no init/deinit, no ISR service, one
    // blocking read (~25 ms) per call. Two attempts; the DHT22 needs >=2 s
    // between samples and the 15 s cycle guarantees that.
    gpio_num_t dht_gpio = profile->dht_gpio;
    float humidity = NAN;
    float temperature = NAN;
    bool read_ok = false;

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (dht_read_float_data(DHT_TYPE_AM2301, dht_gpio, &humidity,
                                &temperature) == ESP_OK) {
            read_ok = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if (!read_ok) {
        ESP_LOGW(TAG, "DHT read failed");
        return false;
    }

    snapshot->temperature.available = APP_SENSOR_TEMPERATURE_ENABLED;
    snapshot->temperature.raw = -1;
    snapshot->temperature.value = temperature;
    snapshot->air.available = APP_SENSOR_AIR_ENABLED;
    snapshot->air.raw = -1;
    snapshot->air.value = humidity;
    return true;
}

void sensors_init(const board_profile_t *profile)
{
    // Production safety: no sensor/actuator pin may overlap the fixed
    // camera bus (0,5,18,19,21,22,23,25,26,27,32,34,35,36,39).
    const gpio_num_t used[] = {
        profile->water_level_gpio, profile->soil_moisture_gpio,
        profile->light_sensor_gpio, profile->dht_gpio,
        profile->pump_gpio, profile->grow_light_gpio,
    };
    for (size_t i = 0; i < sizeof(used) / sizeof(used[0]); ++i) {
        if (board_profile_gpio_conflicts_with_camera(profile, used[i])) {
            ESP_LOGE(TAG, "GPIO %d conflicts with fixed camera bus, aborting", (int) used[i]);
            abort();
        }
    }

    // Switched 3V3 for the resistive water probe. Default LOW (probe
    // unpowered -> GPIO12 Hi-Z -> MTDI reads LOW at boot, 3V3 flash safe).
    // If GPIO33 is not soldered this is a harmless no-op.
    gpio_config_t pwr_config = {
        .pin_bit_mask = 1ULL << profile->water_power_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pwr_config));
    ESP_ERROR_CHECK(gpio_set_level(profile->water_power_gpio, 0));

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = profile->analog_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, profile->water_level_channel, &channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, profile->soil_moisture_channel, &channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, profile->light_sensor_channel, &channel_config));
}

esp_err_t sensors_read_all(const board_profile_t *profile, sensor_snapshot_t *snapshot)
{
    // Power the resistive water probe (~60ms settle), keep it on for the
    // ~250ms ADC burst only (2% duty @15s period -> negligible corrosion).
    ESP_ERROR_CHECK(gpio_set_level(profile->water_power_gpio, 1));
    vTaskDelay(pdMS_TO_TICKS(60));

    read_percent_measurement(&snapshot->water,
                             APP_SENSOR_WATER_ENABLED,
                             profile->water_level_channel,
                             APP_WATER_RAW_EMPTY,
                             APP_WATER_RAW_FULL);
    read_percent_measurement(&snapshot->soil,
                             APP_SENSOR_SOIL_ENABLED,
                             profile->soil_moisture_channel,
                             APP_SOIL_RAW_DRY,
                             APP_SOIL_RAW_WET);
    read_percent_measurement(&snapshot->light,
                             APP_SENSOR_LIGHT_ENABLED,
                             profile->light_sensor_channel,
                             APP_LIGHT_RAW_DARK,
                             APP_LIGHT_RAW_BRIGHT);
    mark_measurement_unavailable(&snapshot->temperature);
    mark_measurement_unavailable(&snapshot->air);

    read_dht_if_enabled(profile, snapshot);

    // Probe off immediately after the ADC burst (DHT is independent).
    ESP_ERROR_CHECK(gpio_set_level(profile->water_power_gpio, 0));

    if ((APP_SENSOR_WATER_ENABLED && !snapshot->water.available) ||
        (APP_SENSOR_SOIL_ENABLED && !snapshot->soil.available) ||
        (APP_SENSOR_LIGHT_ENABLED && !snapshot->light.available)) {
        ESP_LOGW(TAG, "One or more ADC reads failed");
    }

    ESP_LOGI(TAG, "Water=%d(%d%%) Soil=%d(%d%%) Light=%d(%d%%) Temp=%d Air=%d",
             snapshot->water.raw,
             measurement_value_as_int(&snapshot->water),
             snapshot->soil.raw,
             measurement_value_as_int(&snapshot->soil),
             snapshot->light.raw,
             measurement_value_as_int(&snapshot->light),
             measurement_value_as_int(&snapshot->temperature),
             measurement_value_as_int(&snapshot->air));

    return ESP_OK;
}
