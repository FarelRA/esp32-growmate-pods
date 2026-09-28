// Host-only fakes backing the stub HAL headers. Compiled into the native
// test env only (see [env:native] build_src_filter); never linked into the
// firmware. All state resets through fake_runtime_reset(), called from every
// suite's setUp().
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_timer.h"

void fake_runtime_reset(void);

// --- esp_err ---
const char *fake_err_name(esp_err_t err);

// --- NVS (in-memory blob store) ---
void fake_nvs_set_fail_mode(bool open_fails, bool get_fails, bool set_fails, bool commit_fails);
esp_err_t fake_nvs_inject_blob(const char *ns, const char *key, const void *data, size_t length);
size_t fake_nvs_entry_count(void);

// --- MAC ---
void fake_mac_set(const uint8_t mac[6]);
void fake_mac_set_read_fails(bool fails);

// --- GPIO (level recording) ---
int fake_gpio_level(gpio_num_t gpio);
unsigned int fake_gpio_write_count(gpio_num_t gpio);
unsigned int fake_gpio_config_count(void);

// --- ADC (scripted per-channel samples) ---
#define FAKE_ADC_MAX_SCRIPT 16
void fake_adc_set_samples(adc_channel_t channel, const int *samples, int count);
void fake_adc_set_always_fail(adc_channel_t channel, bool fails);
int fake_adc_read_count(adc_channel_t channel);

// --- DHT ---
void fake_dht_set_ok(float temperature_c, float humidity_pct);
void fake_dht_set_fail(bool fails);
int fake_dht_read_count(void);

// --- esp_timer (manual clock + one-shot timers) ---
void fake_timer_set_time(int64_t now_us);
void fake_timer_advance(int64_t delta_us);
void fake_timer_fire(esp_timer_handle_t timer);
void fake_timer_fire_all_armed(void);
bool fake_timer_is_armed(esp_timer_handle_t timer);
int fake_timer_armed_count(void);
int64_t fake_timer_deadline(esp_timer_handle_t timer);
