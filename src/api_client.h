#pragma once

#include <stdbool.h>

#include "app_config.h"
#include "esp_err.h"
#include "sensors.h"

typedef struct
{
    bool has_pump_command;
    int pump_duration_ms;
    char pump_id[32];
    bool has_light_command;
    bool light_enabled;
    char light_id[32];
} device_commands_t;

// Upload return codes:
//   ESP_OK: 2xx, commands/config parsed when present.
//   ESP_ERR_INVALID_STATE: server rejected the request (other 4xx).
//     Fatal/rejected, caller must NOT count toward retry thresholds.
//   ESP_FAIL: transport error, timeout, 5xx, 429 after backoff wait,
//     or truncated 2xx response body.
esp_err_t api_client_upload_sensor_data(const app_config_t *config,
                                        const sensor_snapshot_t *snapshot,
                                        bool pump_enabled,
                                        bool light_enabled,
                                        device_commands_t *commands);
esp_err_t api_client_upload_image_bytes(const app_config_t *config,
                                        const uint8_t *image_data,
                                        size_t image_len);
