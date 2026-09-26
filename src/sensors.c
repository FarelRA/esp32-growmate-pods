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
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_SAMPLE_COUNT 8

static const char *TAG = "sensors";
static adc_oneshot_unit_handle_t s_adc_handle;
static adc_oneshot_unit_handle_t s_adc1_handle;
static uint32_t s_dht_fail_count;
static uint32_t s_adc_fail_count;

static void mark_measurement_unavailable(sensor_measurement_t *measurement)
{
    measurement->available = false;
    measurement->raw = -1;
    measurement->value = NAN;
}

static int read_adc_average(adc_oneshot_unit_handle_t unit, adc_channel_t channel)
{
    int total = 0;
    int success_count = 0;

    for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
        int raw = 0;
        if (adc_oneshot_read(unit, channel, &raw) == ESP_OK) {
            total += raw;
            success_count++;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (success_count == 0) {
        return -1;
    }

    return total / success_count;
}

static void read_raw_measurement(sensor_measurement_t *measurement,
                                 bool enabled,
                                 adc_oneshot_unit_handle_t unit,
                                 adc_channel_t channel)
{
    if (!enabled) {
        mark_measurement_unavailable(measurement);
        return;
    }

    measurement->raw = read_adc_average(unit, channel);
    measurement->value = NAN;
    measurement->available = measurement->raw >= 0;
    if (!measurement->available) {
        s_adc_fail_count++;
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
    float humidity = NAN;
    float temperature = NAN;
    bool read_ok = false;

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (dht_read_float_data(DHT_TYPE_AM2301, dht_gpio, &humidity,
                                &temperature) == ESP_OK) {
            read_ok = true;
            break;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (!read_ok) {
        s_dht_fail_count++;
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

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = profile->analog_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_unit_init_cfg_t init_config_1 = {
        .unit_id = profile->water_level_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config_1, &s_adc1_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, profile->water_level_channel, &channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, profile->soil_moisture_channel, &channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, profile->light_sensor_channel, &channel_config));
}

esp_err_t sensors_read_all(const board_profile_t *profile, sensor_snapshot_t *snapshot)
{
    read_raw_measurement(&snapshot->water,
                         APP_SENSOR_WATER_ENABLED,
                         s_adc1_handle,
                         profile->water_level_channel);
    read_raw_measurement(&snapshot->soil,
                         APP_SENSOR_SOIL_ENABLED,
                         s_adc_handle,
                         profile->soil_moisture_channel);
    read_raw_measurement(&snapshot->light,
                         APP_SENSOR_LIGHT_ENABLED,
                         s_adc_handle,
                         profile->light_sensor_channel);
    mark_measurement_unavailable(&snapshot->temperature);
    mark_measurement_unavailable(&snapshot->air);

    read_dht_if_enabled(profile, snapshot);

    if ((APP_SENSOR_WATER_ENABLED && !snapshot->water.available) ||
        (APP_SENSOR_SOIL_ENABLED && !snapshot->soil.available) ||
        (APP_SENSOR_LIGHT_ENABLED && !snapshot->light.available)) {
        ESP_LOGW(TAG, "One or more ADC reads failed");
    }

    ESP_LOGI(TAG, "Water=%d Soil=%d Light=%d Temp=%.1f Air=%.1f",
             snapshot->water.raw,
             snapshot->soil.raw,
             snapshot->light.raw,
             snapshot->temperature.value,
             snapshot->air.value);

    int enabled_count = 0;
    int unavailable_count = 0;

    if (APP_SENSOR_WATER_ENABLED) {
        enabled_count++;
        if (!snapshot->water.available) {
            unavailable_count++;
        }
    }
    if (APP_SENSOR_SOIL_ENABLED) {
        enabled_count++;
        if (!snapshot->soil.available) {
            unavailable_count++;
        }
    }
    if (APP_SENSOR_LIGHT_ENABLED) {
        enabled_count++;
        if (!snapshot->light.available) {
            unavailable_count++;
        }
    }
    if (APP_SENSOR_TEMPERATURE_ENABLED) {
        enabled_count++;
        if (!snapshot->temperature.available) {
            unavailable_count++;
        }
    }
    if (APP_SENSOR_AIR_ENABLED) {
        enabled_count++;
        if (!snapshot->air.available) {
            unavailable_count++;
        }
    }

    if (enabled_count > 0 && unavailable_count == enabled_count) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

void sensors_get_fail_counts(uint32_t *dht_fails, uint32_t *adc_fails)
{
    if (dht_fails != NULL) {
        *dht_fails = s_dht_fail_count;
    }
    if (adc_fails != NULL) {
        *adc_fails = s_adc_fail_count;
    }
}
