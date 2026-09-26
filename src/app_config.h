#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define APP_CONFIG_VERSION 1
#define APP_CONFIG_NAMESPACE "growmate"
#define APP_CONFIG_STORAGE_KEY "settings"

#define APP_CONFIG_MAX_WIFI_SSID_LEN 32
#define APP_CONFIG_MAX_WIFI_PASSWORD_LEN 64
#define APP_CONFIG_MAX_DEVICE_ID_LEN 31

typedef struct
{
    uint16_t version;
    bool provisioned;
    char wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN + 1];
    char wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN + 1];
    // Assigned pod identity from the server claim flow. Empty = unclaimed,
    // in which case telemetry carries the build default in deviceId and
    // the WiFi MAC separately in hardwareId (see device_identity.h).
    char device_id[APP_CONFIG_MAX_DEVICE_ID_LEN + 1];
    uint32_t boot_count;
    uint32_t report_interval_sec;
    uint32_t applied_config_rev;
} app_config_t;

void app_config_set_defaults(app_config_t *config);
void app_config_sanitize(app_config_t *config);
bool app_config_is_complete(const app_config_t *config);
esp_err_t app_config_load(app_config_t *config);
esp_err_t app_config_save(const app_config_t *config);
