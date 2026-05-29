#include "api_client.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_build_config.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define HTTP_RESPONSE_BUFFER_SIZE 3072

static const char *TAG = "api_client";

typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t data_length;
} http_response_buffer_t;

static esp_err_t perform_binary_post(const char *url,
                                     const uint8_t *payload,
                                     size_t payload_len,
                                     const char *content_type,
                                     const char *device_id,
                                     int timeout_ms)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = timeout_ms,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_header(client, "X-Device-Id", device_id);
    esp_http_client_set_post_field(client, (const char *) payload, payload_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGE(TAG, "HTTP %s returned status %d", url, status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP %s request failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    http_response_buffer_t *response = (http_response_buffer_t *) event->user_data;

    if (event->event_id != HTTP_EVENT_ON_DATA || response == NULL || event->data_len <= 0) {
        return ESP_OK;
    }

    size_t writable = response->buffer_size - response->data_length - 1;
    size_t copy_len = event->data_len > writable ? writable : event->data_len;
    if (copy_len > 0) {
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
    esp_http_client_config_t config = {
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
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGE(TAG, "HTTP %s returned status %d", url, status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP %s request failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

static bool measurement_value_to_int(double value, int *out_value)
{
    if (!isfinite(value)) {
        return false;
    }

    *out_value = (int) lround(value);
    return true;
}

static void sensor_array_add_measurement(cJSON *array, const char *kind, int value, const char *unit, int raw)
{
    cJSON *entry = cJSON_CreateObject();
    if (entry == NULL) {
        return;
    }

    cJSON_AddStringToObject(entry, "kind", kind);
    cJSON_AddNumberToObject(entry, "value", value);
    cJSON_AddStringToObject(entry, "unit", unit);
    if (raw >= 0) {
        cJSON_AddNumberToObject(entry, "raw", raw);
    }
    cJSON_AddItemToArray(array, entry);
}

static void sensor_array_add_snapshot_measurement(cJSON *array,
                                                  const char *kind,
                                                  const char *unit,
                                                  const sensor_measurement_t *measurement)
{
    if (!measurement->available) {
        return;
    }

    int normalized_value = 0;
    if (!measurement_value_to_int(measurement->value, &normalized_value)) {
        return;
    }

    sensor_array_add_measurement(array, kind, normalized_value, unit, measurement->raw);
}

static void parse_commands(const char *response_json, device_commands_t *commands)
{
    memset(commands, 0, sizeof(*commands));
    if (response_json == NULL || response_json[0] == '\0') {
        return;
    }

    cJSON *root = cJSON_Parse(response_json);
    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse response JSON");
        return;
    }

    cJSON *command_array = cJSON_GetObjectItemCaseSensitive(root, "commands");
    if (cJSON_IsArray(command_array)) {
        cJSON *command = NULL;
        cJSON_ArrayForEach(command, command_array) {
            cJSON *kind = cJSON_GetObjectItemCaseSensitive(command, "kind");
            if (!cJSON_IsString(kind) || kind->valuestring == NULL) {
                continue;
            }

            if (strcmp(kind->valuestring, "pump") == 0) {
                cJSON *duration_ms = cJSON_GetObjectItemCaseSensitive(command, "durationMs");
                if (cJSON_IsNumber(duration_ms) && duration_ms->valueint > 0) {
                    commands->has_pump_command = true;
                    commands->pump_duration_ms = duration_ms->valueint;
                }
            } else if (strcmp(kind->valuestring, "light") == 0) {
                cJSON *enabled = cJSON_GetObjectItemCaseSensitive(command, "enabled");
                if (cJSON_IsBool(enabled)) {
                    commands->has_light_command = true;
                    commands->light_enabled = cJSON_IsTrue(enabled);
                }
            }
        }
    }

    cJSON_Delete(root);
}

esp_err_t api_client_upload_sensor_data(const app_config_t *config,
                                        const sensor_snapshot_t *snapshot,
                                        bool pump_enabled,
                                        bool light_enabled,
                                        device_commands_t *commands)
{
    (void) config;
    cJSON *root = cJSON_CreateObject();
    cJSON *sensor_array = cJSON_CreateArray();
    cJSON *current_state = cJSON_CreateObject();
    if (root == NULL || sensor_array == NULL || current_state == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(sensor_array);
        cJSON_Delete(current_state);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "deviceId", APP_DEVICE_ID);
    cJSON_AddStringToObject(root, "firmwareVersion", APP_FIRMWARE_VERSION);
    cJSON_AddItemToObject(root, "sensors", sensor_array);
    cJSON_AddItemToObject(root, "currentState", current_state);
    cJSON_AddBoolToObject(current_state, "pumpEnabled", pump_enabled);
    cJSON_AddBoolToObject(current_state, "lightEnabled", light_enabled);

    sensor_array_add_snapshot_measurement(sensor_array, "soil", "%", &snapshot->soil);
    sensor_array_add_snapshot_measurement(sensor_array, "light", "%", &snapshot->light);
    sensor_array_add_snapshot_measurement(sensor_array, "water", "%", &snapshot->water);
    sensor_array_add_snapshot_measurement(sensor_array, "temperature", "C", &snapshot->temperature);
    sensor_array_add_snapshot_measurement(sensor_array, "air", "%", &snapshot->air);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char response_buffer[HTTP_RESPONSE_BUFFER_SIZE] = {0};
    http_response_buffer_t response = {
        .buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .data_length = 0,
    };

    esp_err_t err = perform_json_post(APP_SENSOR_API_URL, payload, 12000, &response);
    free(payload);

    if (err == ESP_OK) {
        parse_commands(response_buffer, commands);
    }

    return err;
}

esp_err_t api_client_upload_image_bytes(const app_config_t *config,
                                        const uint8_t *image_data,
                                        size_t image_len)
{
    (void) config;
    if (image_data == NULL || image_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Uploading camera frame (%u bytes)", (unsigned int) image_len);

    return perform_binary_post(APP_CAMERA_API_URL,
                               image_data,
                               image_len,
                               "image/jpeg",
                               APP_DEVICE_ID,
                               45000);
}
