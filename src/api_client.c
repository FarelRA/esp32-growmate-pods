#include "api_client.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>

#include "app_build_config.h"
#include "cJSON.h"
#include "device_identity.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_service.h"

#define HTTP_RESPONSE_BUFFER_SIZE 3072
#define SENSOR_POST_TIMEOUT_MS 12000
#define IMAGE_POST_TIMEOUT_MS 45000
#define ACCEPTED_ID_RING_SIZE 16
#define ACCEPTED_ID_LEN 32
#define RETRY_AFTER_MAX_SEC 60
#define CONFIG_REPORT_INTERVAL_MIN_SEC 10
#define CONFIG_REPORT_INTERVAL_MAX_SEC 3600

static const char *TAG = "api_client";

static char s_accepted_ids[ACCEPTED_ID_RING_SIZE][ACCEPTED_ID_LEN];
static size_t s_accepted_count = 0;
static size_t s_accepted_head = 0;

typedef struct
{
    char *buffer;
    size_t buffer_size;
    size_t data_length;
    bool truncated;
    int retry_after_sec;
    bool has_retry_after;
} http_response_buffer_t;

static void accepted_ids_append(const char *id)
{
    if (id == NULL || id[0] == '\0')
    {
        return;
    }

    size_t index = 0;
    if (s_accepted_count < ACCEPTED_ID_RING_SIZE)
    {
        index = (s_accepted_head + s_accepted_count) % ACCEPTED_ID_RING_SIZE;
        s_accepted_count++;
    }
    else
    {
        index = s_accepted_head;
        s_accepted_head = (s_accepted_head + 1) % ACCEPTED_ID_RING_SIZE;
    }

    snprintf(s_accepted_ids[index], ACCEPTED_ID_LEN, "%s", id);
}

static void accepted_ids_clear(void)
{
    for (size_t i = 0; i < ACCEPTED_ID_RING_SIZE; ++i)
    {
        s_accepted_ids[i][0] = '\0';
    }
    s_accepted_count = 0;
    s_accepted_head = 0;
}

static void maybe_set_auth_header(esp_http_client_handle_t client)
{
    if (APP_API_TOKEN[0] == '\0')
    {
        return;
    }

    char value[320];
    snprintf(value, sizeof(value), "Bearer %s", APP_API_TOKEN);
    esp_http_client_set_header(client, "Authorization", value);
}

static esp_err_t classify_status(const char *url, int status_code, http_response_buffer_t *response)
{
    if (status_code == 429)
    {
        int wait_sec = 0;
        if (response != NULL && response->has_retry_after)
        {
            wait_sec = response->retry_after_sec;
        }
        if (wait_sec < 0)
        {
            wait_sec = 0;
        }
        if (wait_sec > RETRY_AFTER_MAX_SEC)
        {
            wait_sec = RETRY_AFTER_MAX_SEC;
        }
        if (wait_sec > 0)
        {
            ESP_LOGW(TAG, "HTTP %s returned 429, waiting %d s", url, wait_sec);
            vTaskDelay(pdMS_TO_TICKS((uint32_t) wait_sec * 1000U));
        }
        else
        {
            ESP_LOGW(TAG, "HTTP %s returned 429", url);
        }
        return ESP_FAIL;
    }

    if (status_code >= 400 && status_code < 500)
    {
        ESP_LOGE(TAG, "HTTP %s returned status %d", url, status_code);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGE(TAG, "HTTP %s returned status %d", url, status_code);
    return ESP_FAIL;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    http_response_buffer_t *response = (http_response_buffer_t *) event->user_data;
    // This handler runs on the caller's task during perform(): feed the
    // watchdog so slow-but-alive transfers don't trip it.
    esp_task_wdt_reset();
    if (response == NULL)
    {
        return ESP_OK;
    }

    if (event->event_id == HTTP_EVENT_ON_HEADER)
    {
        if (event->header_key != NULL && event->header_value != NULL &&
            strcasecmp(event->header_key, "Retry-After") == 0)
        {
            long secs = strtol(event->header_value, NULL, 10);
            if (secs < 0)
            {
                secs = 0;
            }
            if (secs > RETRY_AFTER_MAX_SEC)
            {
                secs = RETRY_AFTER_MAX_SEC;
            }
            response->retry_after_sec = (int) secs;
            response->has_retry_after = true;
        }
        return ESP_OK;
    }

    if (event->event_id != HTTP_EVENT_ON_DATA || response->buffer == NULL || event->data_len <= 0)
    {
        return ESP_OK;
    }

    size_t writable = 0;
    if (response->data_length < response->buffer_size)
    {
        writable = response->buffer_size - response->data_length - 1;
    }
    size_t incoming = (size_t) event->data_len;
    if (incoming > writable)
    {
        response->truncated = true;
    }

    size_t copy_len = incoming > writable ? writable : incoming;
    if (copy_len > 0)
    {
        memcpy(response->buffer + response->data_length, event->data, copy_len);
        response->data_length += copy_len;
        response->buffer[response->data_length] = '\0';
    }

    return ESP_OK;
}

static esp_err_t perform_json_post(const char *url,
                                   const char *payload,
                                   int timeout_ms,
                                   http_response_buffer_t *response)
{
    esp_http_client_config_t config =
    {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = timeout_ms,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    maybe_set_auth_header(client);
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP %s request failed: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code >= 200 && status_code < 300)
    {
        if (response != NULL && response->truncated)
        {
            ESP_LOGE(TAG, "HTTP %s response truncated, discarding", url);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    return classify_status(url, status_code, response);
}

static esp_err_t perform_binary_post(const char *url,
                                     const uint8_t *payload,
                                     size_t payload_len,
                                     const char *content_type,
                                     const char *device_id,
                                     int timeout_ms)
{
    http_response_buffer_t response =
    {
        .buffer = NULL,
        .buffer_size = 0,
        .data_length = 0,
        .truncated = false,
        .retry_after_sec = 0,
        .has_retry_after = false,
    };

    esp_http_client_config_t config =
    {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = timeout_ms,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_header(client, "X-Device-Id", device_id);
    maybe_set_auth_header(client);
    esp_http_client_set_post_field(client, (const char *) payload, payload_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP %s request failed: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code >= 200 && status_code < 300)
    {
        return ESP_OK;
    }

    return classify_status(url, status_code, &response);
}

static void sensor_array_add_raw(cJSON *array,
                                 const char *kind,
                                 const char *unit,
                                 const sensor_measurement_t *measurement)
{
    if (!measurement->available || measurement->raw < 0)
    {
        return;
    }

    cJSON *entry = cJSON_CreateObject();
    if (entry == NULL)
    {
        return;
    }

    cJSON_AddStringToObject(entry, "kind", kind);
    cJSON_AddStringToObject(entry, "unit", unit);
    cJSON_AddNumberToObject(entry, "raw", measurement->raw);
    cJSON_AddItemToArray(array, entry);
}

static void sensor_array_add_dht(cJSON *array,
                                 const char *kind,
                                 const char *unit,
                                 const sensor_measurement_t *measurement)
{
    if (!measurement->available || !isfinite((double) measurement->value))
    {
        return;
    }

    cJSON *entry = cJSON_CreateObject();
    if (entry == NULL)
    {
        return;
    }

    cJSON_AddStringToObject(entry, "kind", kind);
    cJSON_AddStringToObject(entry, "unit", unit);
    cJSON_AddNumberToObject(entry, "value", (double) measurement->value);
    cJSON_AddItemToArray(array, entry);
}

// Command ids are mandatory: an id-less command would be applied but
// never ackable, leaving server and device permanently disagreed.
static bool copy_command_id(char out[ACCEPTED_ID_LEN], const cJSON *command)
{
    out[0] = '\0';

    cJSON *id = cJSON_GetObjectItemCaseSensitive(command, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0' ||
        strlen(id->valuestring) >= ACCEPTED_ID_LEN)
    {
        return false;
    }
    snprintf(out, ACCEPTED_ID_LEN, "%s", id->valuestring);
    return true;
}

static void parse_commands(const cJSON *root, const sensor_snapshot_t *snapshot, device_commands_t *commands)
{
    cJSON *command_array = cJSON_GetObjectItemCaseSensitive(root, "commands");
    if (!cJSON_IsArray(command_array))
    {
        return;
    }

    cJSON *command = NULL;
    cJSON_ArrayForEach(command, command_array)
    {
        cJSON *kind = cJSON_GetObjectItemCaseSensitive(command, "kind");
        if (!cJSON_IsString(kind) || kind->valuestring == NULL)
        {
            continue;
        }

        if (strcmp(kind->valuestring, "pump") == 0)
        {
            cJSON *duration_ms = cJSON_GetObjectItemCaseSensitive(command, "durationMs");
            if (!cJSON_IsNumber(duration_ms) ||
                !isfinite(duration_ms->valuedouble) ||
                duration_ms->valuedouble <= 0.0 ||
                duration_ms->valuedouble > (double) APP_MAX_PUMP_DURATION_MS)
            {
                continue;
            }
            if (!snapshot->water.available)
            {
                ESP_LOGW(TAG, "Pump command rejected: tank level unavailable");
                continue;
            }
            char id[ACCEPTED_ID_LEN] = {0};
            if (!copy_command_id(id, command))
            {
                ESP_LOGW(TAG, "Pump command ignored: missing or overlong id");
                continue;
            }
            commands->has_pump_command = true;
            commands->pump_duration_ms = (int) duration_ms->valuedouble;
            snprintf(commands->pump_id, sizeof(commands->pump_id), "%s", id);
            accepted_ids_append(id);
        }
        else if (strcmp(kind->valuestring, "light") == 0)
        {
            cJSON *enabled = cJSON_GetObjectItemCaseSensitive(command, "enabled");
            bool value = false;
            if (cJSON_IsBool(enabled))
            {
                value = cJSON_IsTrue(enabled);
            }
            else if (cJSON_IsNumber(enabled) && isfinite(enabled->valuedouble))
            {
                value = enabled->valuedouble != 0.0;
            }
            else
            {
                continue;
            }
            char id[ACCEPTED_ID_LEN] = {0};
            if (!copy_command_id(id, command))
            {
                ESP_LOGW(TAG, "Light command ignored: missing or overlong id");
                continue;
            }
            commands->has_light_command = true;
            commands->light_enabled = value;
            snprintf(commands->light_id, sizeof(commands->light_id), "%s", id);
            accepted_ids_append(id);
        }
    }
}

static void apply_config_push(const cJSON *root, app_config_t *config)
{
    cJSON *pushed = cJSON_GetObjectItemCaseSensitive(root, "config");
    if (!cJSON_IsObject(pushed))
    {
        return;
    }

    cJSON *rev = cJSON_GetObjectItemCaseSensitive(pushed, "rev");
    if (!cJSON_IsNumber(rev) || !isfinite(rev->valuedouble) || rev->valuedouble < 1.0)
    {
        return;
    }

    uint32_t new_rev = (uint32_t) rev->valuedouble;
    if (new_rev <= config->applied_config_rev)
    {
        return;
    }

    uint32_t new_interval = config->report_interval_sec;
    cJSON *interval = cJSON_GetObjectItemCaseSensitive(pushed, "reportIntervalSec");
    if (cJSON_IsNumber(interval) && isfinite(interval->valuedouble))
    {
        double value = interval->valuedouble;
        if (value < (double) CONFIG_REPORT_INTERVAL_MIN_SEC)
        {
            value = (double) CONFIG_REPORT_INTERVAL_MIN_SEC;
        }
        if (value > (double) CONFIG_REPORT_INTERVAL_MAX_SEC)
        {
            value = (double) CONFIG_REPORT_INTERVAL_MAX_SEC;
        }
        new_interval = (uint32_t) value;
    }

    config->report_interval_sec = new_interval;
    config->applied_config_rev = new_rev;

    esp_err_t err = app_config_save(config);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to save pushed config rev %u: %s", (unsigned int) new_rev, esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Applied config rev %u, report interval %u s",
                 (unsigned int) new_rev, (unsigned int) new_interval);
    }
}

static void apply_claim(const cJSON *root, app_config_t *config)
{
    cJSON *claim = cJSON_GetObjectItemCaseSensitive(root, "claim");
    if (!cJSON_IsObject(claim))
    {
        return;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(claim, "deviceId");
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0')
    {
        return;
    }
    if (strcmp(device_effective_id(config), id->valuestring) == 0)
    {
        return;
    }

    bool sane = strlen(id->valuestring) <= APP_CONFIG_MAX_DEVICE_ID_LEN;
    for (const char *p = id->valuestring; sane && *p != '\0'; ++p)
    {
        sane = isalnum((unsigned char) *p) || *p == '-' || *p == '_';
    }
    if (!sane)
    {
        ESP_LOGW(TAG, "Claim with bad deviceId ignored");
        return;
    }

    char previous[APP_CONFIG_MAX_DEVICE_ID_LEN + 1];
    snprintf(previous, sizeof(previous), "%s", device_effective_id(config));
    snprintf(config->device_id, sizeof(config->device_id), "%s", id->valuestring);
    if (app_config_save(config) != ESP_OK)
    {
        ESP_LOGW(TAG, "Claimed as %s but NVS save failed", config->device_id);
        return;
    }
    ESP_LOGW(TAG, "Claimed: %s -> %s", previous, config->device_id);
}

static void parse_firmware_offer(const cJSON *root, ota_update_t *ota)
{
    if (ota == NULL)
    {
        return;
    }

    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "minFirmware");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "firmwareUrl");
    if (!cJSON_IsString(version) || version->valuestring == NULL || version->valuestring[0] == '\0' ||
        !cJSON_IsString(url) || url->valuestring == NULL || url->valuestring[0] == '\0')
    {
        return;
    }

    // Reject overlong offers instead of truncating into a corrupt URL.
    if (strlen(version->valuestring) >= sizeof(ota->version) ||
        strlen(url->valuestring) >= sizeof(ota->url))
    {
        ESP_LOGW(TAG, "Firmware offer overlong, ignored");
        return;
    }

    snprintf(ota->version, sizeof(ota->version), "%s", version->valuestring);
    snprintf(ota->url, sizeof(ota->url), "%s", url->valuestring);
    ota->available = true;
}

static const char *reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "ext";
        case ESP_RST_SW: return "sw";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int-wdt";
        case ESP_RST_TASK_WDT: return "task-wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        case ESP_RST_USB: return "usb";
        case ESP_RST_JTAG: return "jtag";
        case ESP_RST_EFUSE: return "efuse";
        case ESP_RST_PWR_GLITCH: return "power-glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu-lockup";
        case ESP_RST_UNKNOWN:
        default: return "unknown";
    }
}

static void parse_response(const char *response_json,
                           const sensor_snapshot_t *snapshot,
                           device_commands_t *commands,
                           app_config_t *config,
                           ota_update_t *ota)
{
    memset(commands, 0, sizeof(*commands));
    ota_update_clear(ota);
    if (response_json == NULL || response_json[0] == '\0')
    {
        return;
    }

    cJSON *root = cJSON_Parse(response_json);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "Failed to parse response JSON");
        return;
    }

    parse_commands(root, snapshot, commands);
    apply_config_push(root, config);
    apply_claim(root, config);
    parse_firmware_offer(root, ota);

    cJSON_Delete(root);
}

esp_err_t api_client_upload_sensor_data(const app_config_t *config,
                                        const sensor_snapshot_t *snapshot,
                                        bool pump_enabled,
                                        bool light_enabled,
                                        device_commands_t *commands,
                                        ota_update_t *ota)
{
    if (config == NULL || snapshot == NULL || commands == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(commands, 0, sizeof(*commands));

    cJSON *root = cJSON_CreateObject();
    cJSON *sensor_array = cJSON_CreateArray();
    cJSON *accepted_ids = cJSON_CreateArray();
    cJSON *current_state = cJSON_CreateObject();
    cJSON *health = cJSON_CreateObject();
    if (root == NULL || sensor_array == NULL || accepted_ids == NULL ||
        current_state == NULL || health == NULL)
    {
        cJSON_Delete(root);
        cJSON_Delete(sensor_array);
        cJSON_Delete(accepted_ids);
        cJSON_Delete(current_state);
        cJSON_Delete(health);
        return ESP_ERR_NO_MEM;
    }

    char snapshot_id[32];
    snprintf(snapshot_id, sizeof(snapshot_id), "B%u-%u",
             (unsigned int) config->boot_count, (unsigned int) snapshot->seq);

    char hwid[13];
    device_identity_hwid(hwid);

    cJSON_AddStringToObject(root, "deviceId", device_effective_id(config));
    cJSON_AddStringToObject(root, "hardwareId", hwid);
    cJSON_AddStringToObject(root, "firmwareVersion", APP_FIRMWARE_VERSION);
    cJSON_AddStringToObject(root, "snapshotId", snapshot_id);
    cJSON_AddNumberToObject(root, "ageMs", snapshot->age_ms);
    cJSON_AddNumberToObject(root, "appliedConfigRev", config->applied_config_rev);
    cJSON_AddItemToObject(root, "sensors", sensor_array);
    cJSON_AddItemToObject(root, "acceptedCommandIds", accepted_ids);
    cJSON_AddItemToObject(root, "currentState", current_state);
    cJSON_AddItemToObject(root, "health", health);

    sensor_array_add_raw(sensor_array, "soil", "%", &snapshot->soil);
    sensor_array_add_raw(sensor_array, "light", "%", &snapshot->light);
    sensor_array_add_raw(sensor_array, "water", "%", &snapshot->water);
    sensor_array_add_dht(sensor_array, "temperature", "C", &snapshot->temperature);
    sensor_array_add_dht(sensor_array, "air", "%", &snapshot->air);

    for (size_t i = 0; i < s_accepted_count; ++i)
    {
        size_t index = (s_accepted_head + i) % ACCEPTED_ID_RING_SIZE;
        cJSON *entry = cJSON_CreateString(s_accepted_ids[index]);
        if (entry == NULL)
        {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddItemToArray(accepted_ids, entry);
    }

    cJSON_AddBoolToObject(current_state, "pumpEnabled", pump_enabled);
    cJSON_AddBoolToObject(current_state, "lightEnabled", light_enabled);

    uint32_t dht_fails = 0;
    uint32_t adc_fails = 0;
    sensors_get_fail_counts(&dht_fails, &adc_fails);

    int rssi = 0;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        rssi = (int) ap_info.rssi;
    }

    cJSON_AddNumberToObject(health, "heapFree", esp_get_free_heap_size());
    cJSON_AddNumberToObject(health, "rssi", rssi);
    cJSON_AddNumberToObject(health, "uptimeS", (double) (esp_timer_get_time() / 1000000LL));
    cJSON_AddNumberToObject(health, "bootCount", config->boot_count);
    cJSON_AddStringToObject(health, "resetReason", reset_reason_str(esp_reset_reason()));
    cJSON_AddNumberToObject(health, "dhtFails", dht_fails);
    cJSON_AddNumberToObject(health, "adcFails", adc_fails);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Heap, not stack: app_main runs TLS + JSON on an 8 KB stack and a
    // 3 KB stack buffer leaves no margin for handshake spikes.
    char *response_buffer = malloc(HTTP_RESPONSE_BUFFER_SIZE);
    if (response_buffer == NULL)
    {
        free(payload);
        return ESP_ERR_NO_MEM;
    }
    response_buffer[0] = '\0';
    http_response_buffer_t response =
    {
        .buffer = response_buffer,
        .buffer_size = HTTP_RESPONSE_BUFFER_SIZE,
        .data_length = 0,
        .truncated = false,
        .retry_after_sec = 0,
        .has_retry_after = false,
    };

    esp_err_t err = perform_json_post(APP_SENSOR_API_URL, payload, SENSOR_POST_TIMEOUT_MS, &response);
    free(payload);
    if (err != ESP_OK)
    {
        free(response_buffer);
        return err;
    }

    accepted_ids_clear();
    parse_response(response_buffer, snapshot, commands, (app_config_t *) config, ota);
    free(response_buffer);

    return ESP_OK;
}

esp_err_t api_client_upload_image_bytes(const app_config_t *config,
                                        const uint8_t *image_data,
                                        size_t image_len)
{
    if (config == NULL || image_data == NULL || image_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Uploading camera frame (%u bytes)", (unsigned int) image_len);

    return perform_binary_post(APP_CAMERA_API_URL,
                               image_data,
                               image_len,
                               "image/jpeg",
                               device_effective_id(config),
                               IMAGE_POST_TIMEOUT_MS);
}
