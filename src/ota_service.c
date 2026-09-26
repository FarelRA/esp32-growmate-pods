#include "ota_service.h"

#include <stdio.h>
#include <string.h>

#include "actuators.h"
#include "app_build_config.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

void ota_update_clear(ota_update_t *update)
{
    if (update != NULL) {
        memset(update, 0, sizeof(*update));
    }
}

static bool parse_version(const char *text, int parts[3])
{
    parts[0] = parts[1] = parts[2] = 0;
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    int matched = sscanf(text, "%d.%d.%d", &parts[0], &parts[1], &parts[2]);
    return matched > 0 && parts[0] >= 0 && parts[1] >= 0 && parts[2] >= 0;
}

static bool version_is_newer(const char *current, const char *candidate)
{
    int cur[3];
    int next[3];
    if (!parse_version(current, cur) || !parse_version(candidate, next)) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (next[i] != cur[i]) {
            return next[i] > cur[i];
        }
    }
    return false;
}

void ota_service_run_if_needed(const ota_update_t *update)
{
    if (APP_OTA_ENABLED == 0) {
        return;
    }
    if (update == NULL || !update->available || update->url[0] == '\0') {
        return;
    }
    if (!version_is_newer(APP_FIRMWARE_VERSION, update->version)) {
        return;
    }
    // Never reboot mid-dose: the restart would cut the pump timer short
    // and leave the server blind. Next cycle retries.
    if (actuators_is_pump_enabled()) {
        ESP_LOGW(TAG, "OTA %s deferred: pump running", update->version);
        return;
    }

    ESP_LOGW(TAG, "OTA %s -> %s", APP_FIRMWARE_VERSION, update->version);

    esp_http_client_config_t http_config = {
        .url = update->url,
        .timeout_ms = 30000,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
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
