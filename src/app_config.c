#include "app_config.h"

#include <ctype.h>
#include <string.h>

#include "app_build_config.h"
#include "nvs.h"
#include "nvs_flash.h"

// NVS v5 layout (pre-claim): identical to app_config_t minus device_id.
// Kept only for OTA migration; new writes are always v6.
typedef struct
{
    uint16_t version;
    bool provisioned;
    char wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN + 1];
    char wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN + 1];
    uint32_t boot_count;
    uint32_t report_interval_sec;
    uint32_t applied_config_rev;
} app_config_v5_t;

static void ensure_terminated(app_config_t *config)
{
    config->wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN] = '\0';
    config->wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN] = '\0';
    config->device_id[APP_CONFIG_MAX_DEVICE_ID_LEN] = '\0';
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
    strlcpy(config->device_id, APP_DEVICE_ID, sizeof(config->device_id));
}

void app_config_sanitize(app_config_t *config)
{
    ensure_terminated(config);
    trim_ascii(config->wifi_ssid);
    trim_ascii(config->device_id);

    if (config->device_id[0] == '\0') {
        strlcpy(config->device_id, APP_DEVICE_ID, sizeof(config->device_id));
    }
    // NOTE: wifi_password is intentionally NOT trimmed. Leading/trailing
    // spaces are legal in WPA2 passphrases; stripping them breaks auth
    // with a misleading "portal doesn't work" symptom.
    ensure_terminated(config);

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
    if (err != ESP_OK) {
        nvs_close(handle);
        app_config_set_defaults(config);
        return err;
    }

    // Migrate a v5 blob (same layout minus device_id) instead of wiping
    // provisioning on OTA. Size mismatch otherwise means corruption.
    if (required_size != sizeof(*config)) {
        esp_err_t migrate_err = ESP_ERR_INVALID_SIZE;
        if (required_size == sizeof(app_config_v5_t)) {
            app_config_v5_t legacy = {0};
            size_t legacy_size = sizeof(legacy);
            if (nvs_get_blob(handle, APP_CONFIG_STORAGE_KEY, &legacy, &legacy_size) == ESP_OK &&
                legacy_size == sizeof(legacy)) {
                nvs_close(handle);
                memset(config, 0, sizeof(*config));
                config->version = APP_CONFIG_VERSION;
                config->provisioned = legacy.provisioned;
                memcpy(config->wifi_ssid, legacy.wifi_ssid, sizeof(config->wifi_ssid));
                memcpy(config->wifi_password, legacy.wifi_password, sizeof(config->wifi_password));
                config->boot_count = legacy.boot_count;
                config->report_interval_sec = legacy.report_interval_sec;
                config->applied_config_rev = legacy.applied_config_rev;
                strlcpy(config->device_id, APP_DEVICE_ID, sizeof(config->device_id));
                app_config_sanitize(config);
                (void) app_config_save(config);
                return ESP_OK;
            }
        }
        nvs_close(handle);
        app_config_set_defaults(config);
        return migrate_err;
    }
    nvs_close(handle);

    // Same-size blob from an older version: preserve credentials, bump.
    if (config->version != APP_CONFIG_VERSION) {
        app_config_sanitize(config);
        (void) app_config_save(config);
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
