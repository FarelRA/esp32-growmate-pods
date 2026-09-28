#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "api_client.h"
#include "app_config.h"
#include "cJSON.h"
#include "ota_service.h"

// Server-response parsing shared by api_client.c and the host Unity suite.
// Pure JSON traversal: persistence (NVS saves) stays in api_client.c, so
// these functions have no ESP-IDF side effects and compile on host against
// the vendored cJSON.

// Callback receiving each accepted command id, in server order.
typedef void (*api_accepted_id_cb_t)(const char *id, void *ctx);

// Fill *commands from the "commands" array. The caller zeroes *commands
// first (firmware does this in parse_response before every cycle).
void api_parse_commands(const cJSON *root,
                        bool water_available,
                        device_commands_t *commands,
                        api_accepted_id_cb_t accepted_cb,
                        void *cb_ctx);

// Fold a "config" push into out params. Returns true when the push is newer
// than applied_rev and must be persisted by the caller.
bool api_parse_config_push(const cJSON *root,
                           uint32_t applied_rev,
                           uint32_t current_interval_sec,
                           uint32_t *out_rev,
                           uint32_t *out_interval_sec);

// Fold a "claim" push into out_device_id. Returns true when the server
// assigns an id different from current_device_id that must be persisted.
bool api_parse_claim(const cJSON *root,
                     const char *current_device_id,
                     char out_device_id[APP_CONFIG_MAX_DEVICE_ID_LEN + 1]);

// Fold a "minFirmware"+"firmwareUrl" offer into *ota. Overlong offers are
// rejected, never truncated.
bool api_parse_firmware_offer(const cJSON *root, ota_update_t *ota);

// Accepted command ids: fixed ring acked on the next POST in server order.
// When full, the oldest id is evicted (the server re-sends unacked commands,
// so eviction only delays the ack by one cycle).
#define API_ACCEPTED_ID_RING_SIZE 16
#define API_ACCEPTED_ID_LEN 32

typedef struct {
    char ids[API_ACCEPTED_ID_RING_SIZE][API_ACCEPTED_ID_LEN];
    size_t count;
    size_t head;
} api_accepted_ring_t;

void api_accepted_ring_init(api_accepted_ring_t *ring);
void api_accepted_ring_push(api_accepted_ring_t *ring, const char *id);
size_t api_accepted_ring_count(const api_accepted_ring_t *ring);
const char *api_accepted_ring_at(const api_accepted_ring_t *ring, size_t index);
void api_accepted_ring_clear(api_accepted_ring_t *ring);
