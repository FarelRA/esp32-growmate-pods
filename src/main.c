#include <stdbool.h>
#include <stdint.h>

#include "actuators.h"
#include "api_client.h"
#include "app_build_config.h"
#include "app_config.h"
#include "board_profile.h"
#include "camera_service.h"
#include "device_identity.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "nvs_flash.h"
#include "onboarding.h"
#include "ota_service.h"
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
                                        const board_profile_t *profile,
                                        ota_update_t *ota)
{
    device_commands_t commands = {0};
    ESP_LOGI(TAG, "Starting sensor cycle");

    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < UPLOAD_RETRY_COUNT; ++attempt) {
        err = api_client_upload_sensor_data(config,
                                            snapshot,
                                            actuators_is_pump_enabled(),
                                            actuators_is_light_enabled(),
                                            &commands,
                                            ota);
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

    if (fb->format != PIXFORMAT_JPEG) {
        esp_camera_fb_return(fb);
        camera_service_deinit();
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    for (int attempt = 0; attempt < UPLOAD_RETRY_COUNT; ++attempt) {
        err = api_client_upload_image_bytes(config, fb->buf, fb->len);
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    esp_camera_fb_return(fb);
    camera_service_deinit();
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
    config.boot_count++;
    if (app_config_save(&config) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save boot count");
    }

    const board_profile_t *profile = board_profile_get((board_profile_id_t) APP_BOARD_PROFILE);
    ESP_LOGI(TAG, "Starting %s for board %s", device_effective_id(&config), profile->display_name);

    ESP_ERROR_CHECK(network_manager_init());
    actuators_init(profile);
    sensors_init(profile);

    if (!app_config_is_complete(&config)) {
        ESP_LOGW(TAG, "Configuration incomplete, entering onboarding mode");
        ESP_ERROR_CHECK(onboarding_run(&config));
    }

    uint32_t sensor_interval_sec = config.report_interval_sec ? config.report_interval_sec : APP_SENSOR_INTERVAL_SEC;
    if (sensor_interval_sec == 0) {
        sensor_interval_sec = APP_SENSOR_INTERVAL_SEC;
    }
    uint32_t loops_since_camera = APP_CAMERA_INTERVAL_SEC / sensor_interval_sec;
    uint32_t consecutive_failures = 0;
    static uint32_t seq = 0;

    while (true) {
        sensor_interval_sec = config.report_interval_sec ? config.report_interval_sec : APP_SENSOR_INTERVAL_SEC;
        if (sensor_interval_sec == 0) {
            sensor_interval_sec = APP_SENSOR_INTERVAL_SEC;
        }

        actuators_tick(profile);

        sensor_snapshot_t snapshot = {0};
        bool camera_due = false;
        bool station_started = false;
        ota_update_t ota;
        ota_update_clear(&ota);

        snapshot.seq = seq++;
        snapshot.age_ms = (uint32_t) (esp_timer_get_time() / 1000);

        err = sensors_read_all(profile, &snapshot);
        if (err != ESP_OK) {
            if (err != ESP_ERR_INVALID_STATE) {
                consecutive_failures++;
            }
            ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(err));
        }

        if (APP_CAMERA_ENABLED == 0 || !profile->has_camera) {
            loops_since_camera = 0;
            camera_due = false;
        } else {
            loops_since_camera++;
            uint32_t camera_period = APP_CAMERA_INTERVAL_SEC / sensor_interval_sec;
            if (camera_period == 0) {
                camera_period = 1;
            }
            camera_due = loops_since_camera >= camera_period;
        }

        if (err == ESP_OK) {
            err = network_manager_start_station(&config, WIFI_CONNECT_TIMEOUT_MS);
            if (err != ESP_OK) {
                if (err != ESP_ERR_INVALID_STATE) {
                    consecutive_failures++;
                }
                ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
            } else {
                station_started = true;

                err = upload_sensor_snapshot(&config, &snapshot, profile, &ota);
                if (err == ESP_OK) {
                    consecutive_failures = 0;
                } else {
                    if (err != ESP_ERR_INVALID_STATE) {
                        consecutive_failures++;
                    }
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
                if (err != ESP_ERR_INVALID_STATE) {
                    consecutive_failures++;
                }
                ESP_LOGE(TAG, "Camera cycle failed: %s", esp_err_to_name(err));
            }
        }

        if (station_started) {
            // OTA runs on the station link before it goes down. A success
            // restarts the device; anything else continues the cycle.
            ota_service_run_if_needed(&ota);
            network_manager_stop();
        }

        if (consecutive_failures >= APP_ONBOARDING_FAILURE_THRESHOLD) {
            ESP_LOGW(TAG, "Repeated network failures detected, reopening onboarding portal");
            ESP_ERROR_CHECK(onboarding_run(&config));
            consecutive_failures = 0;
            loops_since_camera = 0;
        }

        delay_with_housekeeping(profile, sensor_interval_sec * 1000);
    }
}
