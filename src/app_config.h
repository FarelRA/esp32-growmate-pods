#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#define APP_CONFIG_VERSION 4
#define APP_CONFIG_NAMESPACE "growmate"
#define APP_CONFIG_STORAGE_KEY "settings"

#define APP_CONFIG_MAX_WIFI_SSID_LEN 32
#define APP_CONFIG_MAX_WIFI_PASSWORD_LEN 64

typedef struct {
    uint16_t version;
    bool provisioned;
    char wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN + 1];
    char wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN + 1];
} app_config_t;

void app_config_set_defaults(app_config_t *config);
void app_config_sanitize(app_config_t *config);
bool app_config_is_complete(const app_config_t *config);
esp_err_t app_config_load(app_config_t *config);
esp_err_t app_config_save(const app_config_t *config);
