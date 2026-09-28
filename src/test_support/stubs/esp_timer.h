// Host-only stub of ESP-IDF esp_timer.h: one-shot timers with a manually
// advanced clock, backed by fakes/fake_runtime.c. Deterministic by design:
// time only moves when a test advances it.
#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef void *esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void *arg);

typedef enum {
    ESP_TIMER_TASK = 0,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
} esp_timer_create_args_t;

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
int64_t esp_timer_get_time(void);
