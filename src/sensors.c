#include "sensors.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "DHT22.h"
#include "app_build_config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_SAMPLE_COUNT 8

static const char *TAG = "sensors";
static bool s_dht_initialized;
static gpio_num_t s_dht_gpio = GPIO_NUM_NC;
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

    gpio_num_t dht_gpio = profile->dht_gpio;

    uint16_t raw_temperature = 0;
    uint16_t raw_humidity = 0;
    bool read_ok = false;

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (s_dht_initialized) {
            DHTdeinit();
            s_dht_initialized = false;
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        DHTinit(dht_gpio);
        s_dht_gpio = dht_gpio;
        s_dht_initialized = true;
        vTaskDelay(pdMS_TO_TICKS(attempt == 0 ? 120 : 180));

        if (DHTget(&raw_temperature, &raw_humidity) == 0) {
            read_ok = true;
            break;
        }
    }

    DHTdeinit();
    s_dht_initialized = false;

    if (!read_ok) {
        ESP_LOGW(TAG, "DHT read failed");
        return false;
    }

    float temperature = (raw_temperature & 0x7FFF) / 10.0f;
    if ((raw_temperature & 0x8000) != 0) {
        temperature = -temperature;
    }

    snapshot->temperature.available = APP_SENSOR_TEMPERATURE_ENABLED;
    snapshot->temperature.raw = -1;
    snapshot->temperature.value = temperature;
    snapshot->air.available = APP_SENSOR_AIR_ENABLED;
    snapshot->air.raw = -1;
    snapshot->air.value = raw_humidity / 10.0f;
    return true;
}

void sensors_init(const board_profile_t *profile)
{
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
