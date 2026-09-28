#include "api_client.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ctype.h>

#include "app_build_config.h"
#include "api_parse.h"
#include "api_rules.h"
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

static const char *TAG = "api_client";

static api_accepted_ring_t s_accepted_ids;

static void accepted_ids_append(const char *id)
{
    api_accepted_ring_push(&s_accepted_ids, id);
}

static void accepted_ids_clear(void)
{
    api_accepted_ring_clear(&s_accepted_ids);
}

typedef struct
{
    char *buffer;
    size_t buffer_size;
    size_t data_length;
    bool truncated;
    int retry_after_sec;
    bool has_retry_after;
} http_response_buffer_t;

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
            wait_sec = api_rule_clamp_retry_after_sec(response->retry_after_sec);
        }
        if (wait_sec > 0)
        {
            ESP_LOGW(TAG, "HTTP %s returned 429, waiting %d s", url, wait_sec);
            // Slice the wait: a 60 s Retry-After would otherwise sit exactly
            // on the task-watchdog window. Pump safety needs no tick here —
            // the esp_timer one-shot is authoritative and independent.
            for (int i = 0; i < wait_sec; ++i)
            {
                esp_task_wdt_reset();
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        else
        {
            ESP_LOGW(TAG, "HTTP %s returned 429", url);
        }
        // Backed off: retry next cycle, not 1.5 s later (a second instant
        // attempt would just earn a second 429). INVALID_STATE tells the
        // caller to break without counting toward the portal threshold.
        return ESP_ERR_INVALID_STATE;
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
            response->retry_after_sec = api_rule_clamp_retry_after_sec(secs);
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
    if (!api_rule_raw_sample_included(measurement->available, measurement->raw))
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
    if (!api_rule_dht_sample_included(measurement->available, measurement->value))
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

// Command ids feed the accepted-id ring in server order so the next POST
// can ack exactly the commands this cycle applied.
static void accepted_ids_collect(const char *id, void *ctx)
{
    (void) ctx;
    accepted_ids_append(id);
}

static void parse_commands(const cJSON *root, const sensor_snapshot_t *snapshot, device_commands_t *commands)
{
    api_parse_commands(root, snapshot->water.available, commands, accepted_ids_collect, NULL);
}

static void apply_config_push(const cJSON *root, app_config_t *config)
{
    uint32_t new_rev = 0;
    uint32_t new_interval = 0;
    if (!api_parse_config_push(root,
                               config->applied_config_rev,
                               config->report_interval_sec,
                               &new_rev,
                               &new_interval))
    {
        return;
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
    char claimed[APP_CONFIG_MAX_DEVICE_ID_LEN + 1] = {0};
    if (!api_parse_claim(root, device_effective_id(config), claimed))
    {
        return;
    }

    char previous[APP_CONFIG_MAX_DEVICE_ID_LEN + 1];
    snprintf(previous, sizeof(previous), "%s", device_effective_id(config));
    snprintf(config->device_id, sizeof(config->device_id), "%s", claimed);
    if (app_config_save(config) != ESP_OK)
    {
        // Roll back the RAM copy: without the NVS write the old ID comes
        // back on reboot, and flapping IDs poison the allowlist + dedup.
        snprintf(config->device_id, sizeof(config->device_id), "%s", previous);
        ESP_LOGW(TAG, "Claim save failed, keeping %s", previous);
        return;
    }
    ESP_LOGW(TAG, "Claimed: %s -> %s", previous, config->device_id);
}

static void parse_firmware_offer(const cJSON *root, ota_update_t *ota)
{
    api_parse_firmware_offer(root, ota);
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

esp_err_t api_client_upload_sensor_data(app_config_t *config,
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

    for (size_t i = 0; i < api_accepted_ring_count(&s_accepted_ids); ++i)
    {
        const char *acked = api_accepted_ring_at(&s_accepted_ids, i);
        cJSON *entry = cJSON_CreateString(acked);
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
    parse_response(response_buffer, snapshot, commands, config, ota);
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
