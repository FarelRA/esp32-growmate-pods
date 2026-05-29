#include "app_config.h"

#include <ctype.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

static void ensure_terminated(app_config_t *config)
{
    config->wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN] = '\0';
    config->wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN] = '\0';
}

static void trim_ascii(char *value)
{
    size_t start = 0;
    size_t len = strlen(value);

    while (start < len && isspace((unsigned char) value[start])) {
        start++;
    }

    while (len > start && isspace((unsigned char) value[len - 1])) {
        len--;
    }

    if (start > 0) {
        memmove(value, value + start, len - start);
    }
    value[len - start] = '\0';
}

void app_config_set_defaults(app_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->version = APP_CONFIG_VERSION;
}

void app_config_sanitize(app_config_t *config)
{
    ensure_terminated(config);
    trim_ascii(config->wifi_ssid);

    if (config->version != APP_CONFIG_VERSION) {
        config->version = APP_CONFIG_VERSION;
    }
}

bool app_config_is_complete(const app_config_t *config)
{
    return config->provisioned &&
           strlen(config->wifi_ssid) > 0;
}

esp_err_t app_config_load(app_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        app_config_set_defaults(config);
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size = sizeof(*config);
    err = nvs_get_blob(handle, APP_CONFIG_STORAGE_KEY, config, &required_size);
    nvs_close(handle);

    if (err != ESP_OK) {
        app_config_set_defaults(config);
        return err;
    }

    if (required_size != sizeof(*config) || config->version != APP_CONFIG_VERSION) {
        app_config_set_defaults(config);
        return ESP_ERR_INVALID_VERSION;
    }

    app_config_sanitize(config);
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, APP_CONFIG_STORAGE_KEY, config, sizeof(*config));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}
