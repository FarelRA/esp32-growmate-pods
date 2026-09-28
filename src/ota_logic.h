#pragma once

#include <stdbool.h>

#include "ota_service.h"

// Pure OTA decision logic, shared by the firmware service and the host
// Unity suite. No ESP-IDF driver calls: safe to compile on host.

// Outcome of the pre-flight gate, in firmware evaluation order.
typedef enum {
    OTA_GATE_OK = 0,
    OTA_GATE_NO_UPDATE,
    OTA_GATE_NOT_NEWER,
    OTA_GATE_URL_NOT_HTTPS,
    OTA_GATE_PUMP_RUNNING,
} ota_gate_t;

bool ota_parse_version(const char *text, int parts[3]);
bool ota_version_is_newer(const char *current, const char *candidate);
ota_gate_t ota_check_start(const char *current_version,
                           const ota_update_t *update,
                           bool pump_running);
