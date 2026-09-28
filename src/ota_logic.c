#include "ota_logic.h"

#include <stdio.h>
#include <string.h>

bool ota_parse_version(const char *text, int parts[3])
{
    parts[0] = parts[1] = parts[2] = 0;
    if (text == NULL || text[0] == '\0' || strlen(text) >= 32) {
        return false;
    }
    // Strict semver-ish: digits and dots only, at least one digit, and
    // the whole string consumed (rejects "1..2", "2.1.0.4", "1.").
    bool has_digit = false;
    for (const char *p = text; *p != '\0'; ++p) {
        if (*p >= '0' && *p <= '9') {
            has_digit = true;
        } else if (*p != '.') {
            return false;
        }
    }
    if (!has_digit) {
        return false;
    }
    int end = 0;
    int matched = sscanf(text, "%d.%d.%d%n", &parts[0], &parts[1], &parts[2], &end);
    if (matched < 1 || end != (int) strlen(text)) {
        return false;
    }
    // Unmatched trailing parts keep their zero init ("2.1" == 2.1.0).
    return parts[0] >= 0 && parts[1] >= 0 && parts[2] >= 0;
}

bool ota_version_is_newer(const char *current, const char *candidate)
{
    int cur[3];
    int next[3];
    if (!ota_parse_version(current, cur) || !ota_parse_version(candidate, next)) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (next[i] != cur[i]) {
            return next[i] > cur[i];
        }
    }
    return false;
}

ota_gate_t ota_check_start(const char *current_version,
                           const ota_update_t *update,
                           bool pump_running)
{
    if (update == NULL || !update->available || update->url[0] == '\0') {
        return OTA_GATE_NO_UPDATE;
    }
    if (!ota_version_is_newer(current_version, update->version)) {
        return OTA_GATE_NOT_NEWER;
    }
    if (strncmp(update->url, "https://", 8) != 0) {
        return OTA_GATE_URL_NOT_HTTPS;
    }
    // Never reboot mid-dose: the restart would cut the pump timer short
    // and leave the server blind. Next cycle retries.
    if (pump_running) {
        return OTA_GATE_PUMP_RUNNING;
    }
    return OTA_GATE_OK;
}
