#include <stdbool.h>

#include "actuators.h"
#include "api_client.h"
#include "app_build_config.h"
#include "app_config.h"
#include "board_profile.h"
#include "camera_service.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "nvs_flash.h"
#include "onboarding.h"
#include "sensors.h"

#define WIFI_CONNECT_TIMEOUT_MS 12000
#define UPLOAD_RETRY_COUNT 2

static const char *TAG = "growmate";

static void delay_with_housekeeping(const board_profile_t *profile, uint32_t total_ms)
{
    const TickType_t step = pdMS_TO_TICKS(250);
    TickType_t remaining = pdMS_TO_TICKS(total_ms);

    while (remaining > 0) {
        actuators_tick(profile);
        TickType_t current_step = remaining > step ? step : remaining;
        vTaskDelay(current_step);
        remaining -= current_step;
    }
}

static esp_err_t upload_sensor_snapshot(const app_config_t *config,
                                        const sensor_snapshot_t *snapshot,
                                        const board_profile_t *profile)
{
    device_commands_t commands = {0};
    ESP_LOGI(TAG, "Starting sensor cycle");

    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < UPLOAD_RETRY_COUNT; ++attempt) {
        err = api_client_upload_sensor_data(config,
                                            snapshot,
                                            actuators_is_pump_enabled(),
                                            actuators_is_light_enabled(),
                                            &commands);
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    if (err == ESP_OK) {
        actuators_apply_commands(profile, &commands);
    }
    return err;
}

static esp_err_t upload_camera_image(const board_profile_t *profile, const app_config_t *config)
{
    if (!APP_CAMERA_ENABLED || !profile->has_camera) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting camera cycle");
    esp_err_t err = camera_service_init(profile);
    if (err != ESP_OK) {
        return err;
    }

    camera_fb_t *fb = camera_service_capture();
    if (fb == NULL) {
        camera_service_deinit();
        return ESP_FAIL;
    }

    size_t image_len = fb->len;
    uint8_t *image_data = heap_caps_malloc(image_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (image_data == NULL) {
        image_data = malloc(image_len);
    }
    if (image_data == NULL) {
        esp_camera_fb_return(fb);
        camera_service_deinit();
        return ESP_ERR_NO_MEM;
    }

    memcpy(image_data, fb->buf, image_len);

    err = ESP_FAIL;
    esp_camera_fb_return(fb);
    camera_service_deinit();

    for (int attempt = 0; attempt < UPLOAD_RETRY_COUNT; ++attempt) {
        err = api_client_upload_image_bytes(config, image_data, image_len);
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    free(image_data);
    return err;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_config_t config;
    app_config_load(&config);
    app_config_sanitize(&config);

    const board_profile_t *profile = board_profile_get((board_profile_id_t) APP_BOARD_PROFILE);
    ESP_LOGI(TAG, "Starting %s for board %s", APP_DEVICE_ID, profile->display_name);

    ESP_ERROR_CHECK(network_manager_init());
    actuators_init(profile);
    sensors_init(profile);

    if (!app_config_is_complete(&config)) {
        ESP_LOGW(TAG, "Configuration incomplete, entering onboarding mode");
        ESP_ERROR_CHECK(onboarding_run(&config));
    }

    uint32_t loops_since_camera = APP_CAMERA_INTERVAL_SEC / APP_SENSOR_INTERVAL_SEC;
    uint32_t consecutive_failures = 0;

    while (true) {
        actuators_tick(profile);

        sensor_snapshot_t snapshot = {0};
        bool camera_due = false;
        bool station_started = false;

        err = sensors_read_all(profile, &snapshot);
        if (err != ESP_OK) {
            consecutive_failures++;
            ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(err));
        }

        loops_since_camera++;
        uint32_t camera_period = APP_CAMERA_INTERVAL_SEC / APP_SENSOR_INTERVAL_SEC;
        if (camera_period == 0) {
            camera_period = 1;
        }
        camera_due = loops_since_camera >= camera_period;

        if (err == ESP_OK) {
            err = network_manager_start_station(&config, WIFI_CONNECT_TIMEOUT_MS);
            if (err != ESP_OK) {
                consecutive_failures++;
                ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
            } else {
                station_started = true;

                err = upload_sensor_snapshot(&config, &snapshot, profile);
                if (err == ESP_OK) {
                    consecutive_failures = 0;
                } else {
                    consecutive_failures++;
                    ESP_LOGE(TAG, "Sensor cycle failed: %s", esp_err_to_name(err));
                }
            }
        }

        if (station_started && camera_due) {
            err = upload_camera_image(profile, &config);
            if (err == ESP_OK) {
                loops_since_camera = 0;
                consecutive_failures = 0;
            } else {
                consecutive_failures++;
                ESP_LOGE(TAG, "Camera cycle failed: %s", esp_err_to_name(err));
            }
        }

        if (station_started) {
            network_manager_stop();
        }

        if (consecutive_failures >= APP_ONBOARDING_FAILURE_THRESHOLD) {
            ESP_LOGW(TAG, "Repeated network failures detected, reopening onboarding portal");
            ESP_ERROR_CHECK(onboarding_run(&config));
            consecutive_failures = 0;
            loops_since_camera = 0;
        }

        delay_with_housekeeping(profile, APP_SENSOR_INTERVAL_SEC * 1000);
    }
}
