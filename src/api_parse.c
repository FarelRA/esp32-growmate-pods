#include "api_parse.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "api_rules.h"
#include "app_build_config.h"
#include "esp_log.h"

#define ACCEPTED_ID_LEN 32

static const char *TAG = "api_client";

// Command ids are mandatory: an id-less command would be applied but
// never ackable, leaving server and device permanently disagreed.
static bool copy_command_id(char out[ACCEPTED_ID_LEN], const cJSON *command)
{
    out[0] = '\0';

    cJSON *id = cJSON_GetObjectItemCaseSensitive(command, "id");
    if (!cJSON_IsString(id) || id->valuestring == NULL ||
        !api_rule_id_valid(id->valuestring, ACCEPTED_ID_LEN - 1))
    {
        return false;
    }
    snprintf(out, ACCEPTED_ID_LEN, "%s", id->valuestring);
    return true;
}

void api_parse_commands(const cJSON *root,
                        bool water_available,
                        device_commands_t *commands,
                        api_accepted_id_cb_t accepted_cb,
                        void *cb_ctx)
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
                !api_rule_dose_ms_valid(duration_ms->valuedouble))
            {
                continue;
            }
            if (!water_available)
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
            if (accepted_cb != NULL)
            {
                accepted_cb(id, cb_ctx);
            }
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
            if (accepted_cb != NULL)
            {
                accepted_cb(id, cb_ctx);
            }
        }
    }
}

bool api_parse_config_push(const cJSON *root,
                           uint32_t applied_rev,
                           uint32_t current_interval_sec,
                           uint32_t *out_rev,
                           uint32_t *out_interval_sec)
{
    cJSON *pushed = cJSON_GetObjectItemCaseSensitive(root, "config");
    if (!cJSON_IsObject(pushed))
    {
        return false;
    }

    cJSON *rev = cJSON_GetObjectItemCaseSensitive(pushed, "rev");
    if (!cJSON_IsNumber(rev) || !api_rule_config_rev_should_apply(applied_rev, rev->valuedouble))
    {
        return false;
    }

    uint32_t new_rev = (uint32_t) rev->valuedouble;
    uint32_t new_interval = current_interval_sec;
    cJSON *interval = cJSON_GetObjectItemCaseSensitive(pushed, "reportIntervalSec");
    if (cJSON_IsNumber(interval) && isfinite(interval->valuedouble))
    {
        new_interval = api_rule_clamp_report_interval(interval->valuedouble);
    }

    *out_rev = new_rev;
    *out_interval_sec = new_interval;
    return true;
}

bool api_parse_claim(const cJSON *root,
                     const char *current_device_id,
                     char out_device_id[APP_CONFIG_MAX_DEVICE_ID_LEN + 1])
{
    cJSON *claim = cJSON_GetObjectItemCaseSensitive(root, "claim");
    if (!cJSON_IsObject(claim))
    {
        return false;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(claim, "deviceId");
    if (!cJSON_IsString(id) || id->valuestring == NULL || id->valuestring[0] == '\0')
    {
        return false;
    }
    if (current_device_id != NULL && strcmp(current_device_id, id->valuestring) == 0)
    {
        return false;
    }

    if (!api_rule_id_valid(id->valuestring, APP_CONFIG_MAX_DEVICE_ID_LEN) ||
        !api_rule_claim_charset_valid(id->valuestring))
    {
        ESP_LOGW(TAG, "Claim with bad deviceId ignored");
        return false;
    }

    snprintf(out_device_id, APP_CONFIG_MAX_DEVICE_ID_LEN + 1, "%s", id->valuestring);
    return true;
}

bool api_parse_firmware_offer(const cJSON *root, ota_update_t *ota)
{
    if (ota == NULL)
    {
        return false;
    }

    cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "minFirmware");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "firmwareUrl");
    if (!cJSON_IsString(version) || version->valuestring == NULL || version->valuestring[0] == '\0' ||
        !cJSON_IsString(url) || url->valuestring == NULL || url->valuestring[0] == '\0')
    {
        return false;
    }

    // Reject overlong offers instead of truncating into a corrupt URL.
    if (strlen(version->valuestring) >= sizeof(ota->version) ||
        strlen(url->valuestring) >= sizeof(ota->url))
    {
        ESP_LOGW(TAG, "Firmware offer overlong, ignored");
        return false;
    }

    snprintf(ota->version, sizeof(ota->version), "%s", version->valuestring);
    snprintf(ota->url, sizeof(ota->url), "%s", url->valuestring);
    ota->available = true;
    return true;
}

void api_accepted_ring_init(api_accepted_ring_t *ring)
{
    for (size_t i = 0; i < API_ACCEPTED_ID_RING_SIZE; ++i)
    {
        ring->ids[i][0] = '\0';
    }
    ring->count = 0;
    ring->head = 0;
}

void api_accepted_ring_push(api_accepted_ring_t *ring, const char *id)
{
    if (id == NULL || id[0] == '\0')
    {
        return;
    }

    size_t index = 0;
    if (ring->count < API_ACCEPTED_ID_RING_SIZE)
    {
        index = (ring->head + ring->count) % API_ACCEPTED_ID_RING_SIZE;
        ring->count++;
    }
    else
    {
        index = ring->head;
        ring->head = (ring->head + 1) % API_ACCEPTED_ID_RING_SIZE;
    }

    snprintf(ring->ids[index], API_ACCEPTED_ID_LEN, "%s", id);
}

size_t api_accepted_ring_count(const api_accepted_ring_t *ring)
{
    return ring->count;
}

const char *api_accepted_ring_at(const api_accepted_ring_t *ring, size_t index)
{
    if (index >= ring->count)
    {
        return NULL;
    }
    return ring->ids[(ring->head + index) % API_ACCEPTED_ID_RING_SIZE];
}

void api_accepted_ring_clear(api_accepted_ring_t *ring)
{
    api_accepted_ring_init(ring);
}
