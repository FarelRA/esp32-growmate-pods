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
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "main_logic.h"
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
        esp_task_wdt_reset();
        TickType_t current_step = remaining > step ? step : remaining;
        vTaskDelay(current_step);
        remaining -= current_step;
    }
}

static esp_err_t upload_sensor_snapshot(app_config_t *config,
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
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            break;
        }
        // Keep the pump safety net serviced even while backing off.
        delay_with_housekeeping(profile, 1500);
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
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            break;
        }
        actuators_tick(profile);
        esp_task_wdt_reset();
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

    // The main loop legitimately blocks (12 s WiFi join, 45 s JPEG POST,
    // OTA download, indefinite portal). A 5 s watchdog would false-trip,
    // so subscribe app_main with a 60 s window; every blocking path
    // above feeds it at least once per second.
    ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&(esp_task_wdt_config_t) {
        .timeout_ms = 60000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true,
    }));
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    app_config_t config;
    app_config_load(&config);
    app_config_sanitize(&config);
    config.boot_count++;
    bool boot_save_failed = false;
    if (app_config_save(&config) != ESP_OK) {
        // A failed boot-count save reuses the previous boot's snapshotIds
        // (B<boot>-<seq>), which the server dedups as duplicates. Retry
        // before the first POST; the boot still runs but may gap ingestion.
        ESP_LOGE(TAG, "Failed to save boot count, will retry before first POST");
        boot_save_failed = true;
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

    uint32_t sensor_interval_sec = main_logic_report_interval_sec(config.report_interval_sec);
    uint32_t loops_since_camera = APP_CAMERA_INTERVAL_SEC / sensor_interval_sec;
    uint32_t consecutive_failures = 0;
    static uint32_t seq = 0;

    while (true) {
        sensor_interval_sec = main_logic_report_interval_sec(config.report_interval_sec);

        actuators_tick(profile);

        sensor_snapshot_t snapshot = {0};
        bool camera_due = false;
        bool station_started = false;
        ota_update_t ota;
        ota_update_clear(&ota);

        snapshot.seq = seq++;
        // ageMs = sampling-to-POST latency. Stamp the sample time BEFORE
        // the (slow) ADC+DHT read; the value is filled in right before
        // the POST so sampleTime = receivedAt - ageMs holds server-side.
        int64_t sample_t_us = esp_timer_get_time();

        err = sensors_read_all(profile, &snapshot);
        if (err != ESP_OK) {
            // sensors_read_all returns ESP_OK or ESP_FAIL only.
            consecutive_failures++;
            ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(err));
        }

        camera_due = main_logic_camera_slot_due(APP_CAMERA_ENABLED != 0 && profile->has_camera,
                                                sensor_interval_sec,
                                                &loops_since_camera);

        if (err == ESP_OK) {
            if (boot_save_failed) {
                if (app_config_save(&config) == ESP_OK) {
                    boot_save_failed = false;
                } else {
                    ESP_LOGE(TAG, "Boot-count re-save failed, snapshotIds may dedup-drop");
                }
            }
            err = network_manager_start_station(&config, WIFI_CONNECT_TIMEOUT_MS);
            if (err != ESP_OK) {
                // Station join returns OK/FAIL/TIMEOUT, all countable:
                // no link means no telemetry either way.
                consecutive_failures++;
                ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
            } else {
                station_started = true;

                int64_t now_us = esp_timer_get_time();
                snapshot.age_ms = now_us >= sample_t_us
                    ? (uint32_t) ((now_us - sample_t_us) / 1000LL)
                    : 0;
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
                // Camera-only failure must not force the portal: telemetry
                // is healthy, the still retries at the next due slot.
                if (err != ESP_ERR_INVALID_STATE) {
                    ESP_LOGW(TAG, "Camera frame failed, not counting toward portal: %s",
                             esp_err_to_name(err));
                } else {
                    ESP_LOGE(TAG, "Camera cycle failed: %s", esp_err_to_name(err));
                }
            }
        }

        if (station_started) {
            // OTA runs on the station link before it goes down. A success
            // restarts the device; anything else continues the cycle.
            ota_service_run_if_needed(&ota);
            network_manager_stop();
        }

        if (main_logic_should_reopen_portal(consecutive_failures)) {
            ESP_LOGW(TAG, "Repeated network failures detected, reopening onboarding portal");
            ESP_ERROR_CHECK(onboarding_run(&config));
            consecutive_failures = 0;
            loops_since_camera = 0;
        }

        delay_with_housekeeping(profile, sensor_interval_sec * 1000);
    }
}
