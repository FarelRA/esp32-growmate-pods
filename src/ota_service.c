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

static bool parse_version(const char *text, int parts[3])
{
    parts[0] = parts[1] = parts[2] = 0;
    if (text == NULL || text[0] == '\0' || strlen(text) >= 32) {
        return false;
    }
    // Strict semver-ish: digits and dots only, at least one digit, and
    // the whole string consumed (rejects "1..2", "2.1.0.4", "1.").
    bool has_digit = false;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p >= '0' && *p <= '9') {
            has_digit = true;
        } else if (*p != '.') {
            return false;
        }
    }
    if (!has_digit) {
        return false;
    }
    int end = 0;
    int matched = sscanf(text, "%d.%d.%d%n", &parts[0], &parts[1], &parts[2], &end);
    if (matched < 1 || end != (int) strlen(text)) {
        return false;
    }
    // Unmatched trailing parts keep their zero init ("2.1" == 2.1.0).
    return parts[0] >= 0 && parts[1] >= 0 && parts[2] >= 0;
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
    if (strncmp(update->url, "https://", 8) != 0) {
        ESP_LOGW(TAG, "OTA refused: URL must be https");
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
