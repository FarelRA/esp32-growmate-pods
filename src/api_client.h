#pragma once

#include <stdbool.h>

#include "app_config.h"
#include "esp_err.h"
#include "ota_service.h"
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
//   ESP_ERR_INVALID_STATE: no retry this cycle, no portal count. Covers
//     server rejections (other 4xx) AND honored 429s (waited out, retry
//     next cycle — an instant second attempt would just earn another 429).
//   ESP_FAIL: transport error, timeout, 5xx, or truncated 2xx body.
//     Retryable with backoff; counts toward the portal threshold.
esp_err_t api_client_upload_sensor_data(app_config_t *config,
                                        const sensor_snapshot_t *snapshot,
                                        bool pump_enabled,
                                        bool light_enabled,
                                        device_commands_t *commands,
                                        ota_update_t *ota);
esp_err_t api_client_upload_image_bytes(const app_config_t *config,
                                        const uint8_t *image_data,
                                        size_t image_len);
