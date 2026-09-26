#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Server-offered firmware update, parsed from the sensor POST response
// (`minFirmware` + `firmwareUrl`). Consumed by the OTA service.
typedef struct {
    bool available;
    char url[256];
    char version[32];
} ota_update_t;

void ota_update_clear(ota_update_t *update);
void ota_service_run_if_needed(const ota_update_t *update);
