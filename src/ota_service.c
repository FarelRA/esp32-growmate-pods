#include "ota_service.h"

#include <stdio.h>
#include <string.h>

#include "actuators.h"
#include "app_build_config.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_logic.h"

static const char *TAG = "ota";

static esp_err_t ota_http_event_handler(esp_http_client_event_t *event)
{
    (void) event;
    // Download progress runs on the caller's task: feed the watchdog so a
    // large image on a slow link doesn't trip it.
    esp_task_wdt_reset();
    return ESP_OK;
}

void ota_update_clear(ota_update_t *update)
{
    if (update != NULL) {
        memset(update, 0, sizeof(*update));
    }
}

void ota_service_run_if_needed(const ota_update_t *update)
{
    if (APP_OTA_ENABLED == 0) {
        return;
    }
    bool pump_running = actuators_is_pump_enabled();
    switch (ota_check_start(APP_FIRMWARE_VERSION, update, pump_running)) {
        case OTA_GATE_OK:
            break;
        case OTA_GATE_URL_NOT_HTTPS:
            ESP_LOGW(TAG, "OTA refused: URL must be https");
            return;
        case OTA_GATE_PUMP_RUNNING:
            ESP_LOGW(TAG, "OTA %s deferred: pump running", update->version);
            return;
        case OTA_GATE_NO_UPDATE:
        case OTA_GATE_NOT_NEWER:
        default:
            return;
    }

    ESP_LOGW(TAG, "OTA %s -> %s", APP_FIRMWARE_VERSION, update->version);

    esp_http_client_config_t http_config = {
        .url = update->url,
        .timeout_ms = 30000,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = ota_http_event_handler,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t err = esp_https_ota(&ota_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGW(TAG, "OTA complete, restarting");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}
