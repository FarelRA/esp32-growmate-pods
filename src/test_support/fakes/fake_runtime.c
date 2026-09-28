// Host-only fake HAL: deterministic seams for NVS, MAC, GPIO, ADC, DHT and
// esp_timer. Compiled in the native test env only; the firmware never links
// this file (it is absent from src/CMakeLists.txt SRCS).
#ifdef PIO_UNIT_TEST

#include "fakes/fake_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dht.h"
#include "esp_mac.h"
#include "nvs.h"

#define FAKE_GPIO_PIN_COUNT 40
#define FAKE_NVS_MAX_ENTRIES 8
#define FAKE_NVS_MAX_BLOB 256
#define FAKE_TIMER_MAX 4
#define FAKE_ADC_CHANNEL_COUNT 10

// --- esp_err ---

const char *esp_err_to_name(esp_err_t err)
{
    (void) err;
    return "fake-err";
}

const char *fake_err_name(esp_err_t err)
{
    return esp_err_to_name(err);
}

// --- NVS ---

typedef struct {
    bool used;
    char ns[16];
    char key[32];
    uint8_t blob[FAKE_NVS_MAX_BLOB];
    size_t length;
} fake_nvs_entry_t;

static fake_nvs_entry_t s_nvs_entries[FAKE_NVS_MAX_ENTRIES];
static bool s_nvs_open_fails;
static bool s_nvs_get_fails;
static bool s_nvs_set_fails;
static bool s_nvs_commit_fails;

static fake_nvs_entry_t *nvs_find(const char *key)
{
    for (size_t i = 0; i < FAKE_NVS_MAX_ENTRIES; ++i) {
        if (s_nvs_entries[i].used && strcmp(s_nvs_entries[i].key, key) == 0) {
            return &s_nvs_entries[i];
        }
    }
    return NULL;
}

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    // Handles are opaque on host; the store is global like the flash.
    (void) namespace_name;
    if (s_nvs_open_fails) {
        return ESP_FAIL;
    }
    if (open_mode == NVS_READONLY && fake_nvs_entry_count() == 0) {
        //Like real NVS: opening a never-written namespace read-only reports
        // NOT_FOUND so callers install defaults.
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    (void) handle;
    if (s_nvs_get_fails) {
        return ESP_FAIL;
    }
    // Namespace is fixed by the caller sequence in these tests; match by key.
    fake_nvs_entry_t *found = nvs_find(key);
    if (found != NULL) {
        if (*length < found->length) {
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(out_value, found->blob, found->length);
        *length = found->length;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    (void) handle;
    if (s_nvs_set_fails) {
        return ESP_FAIL;
    }
    fake_nvs_entry_t *entry = nvs_find(key);
    if (entry == NULL) {
        for (size_t i = 0; i < FAKE_NVS_MAX_ENTRIES; ++i) {
            if (!s_nvs_entries[i].used) {
                entry = &s_nvs_entries[i];
                break;
            }
        }
    }
    if (entry == NULL || length > FAKE_NVS_MAX_BLOB) {
        return ESP_ERR_NO_MEM;
    }
    entry->used = true;
    snprintf(entry->ns, sizeof(entry->ns), "%s", "growmate");
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    memcpy(entry->blob, value, length);
    entry->length = length;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void) handle;
    return s_nvs_commit_fails ? ESP_FAIL : ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    (void) handle;
}

void fake_nvs_set_fail_mode(bool open_fails, bool get_fails, bool set_fails, bool commit_fails)
{
    s_nvs_open_fails = open_fails;
    s_nvs_get_fails = get_fails;
    s_nvs_set_fails = set_fails;
    s_nvs_commit_fails = commit_fails;
}

esp_err_t fake_nvs_inject_blob(const char *ns, const char *key, const void *data, size_t length)
{
    if (length > FAKE_NVS_MAX_BLOB) {
        return ESP_ERR_INVALID_SIZE;
    }
    fake_nvs_entry_t *entry = nvs_find(key);
    if (entry == NULL) {
        for (size_t i = 0; i < FAKE_NVS_MAX_ENTRIES; ++i) {
            if (!s_nvs_entries[i].used) {
                entry = &s_nvs_entries[i];
                break;
            }
        }
    }
    if (entry == NULL) {
        return ESP_ERR_NO_MEM;
    }
    entry->used = true;
    snprintf(entry->ns, sizeof(entry->ns), "%s", ns);
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    memcpy(entry->blob, data, length);
    entry->length = length;
    return ESP_OK;
}

size_t fake_nvs_entry_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < FAKE_NVS_MAX_ENTRIES; ++i) {
        count += s_nvs_entries[i].used ? 1 : 0;
    }
    return count;
}

// --- MAC ---

static uint8_t s_mac[6] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
static bool s_mac_read_fails;

esp_err_t esp_read_mac(uint8_t mac[6], esp_mac_type_t type)
{
    (void) type;
    if (s_mac_read_fails) {
        return ESP_FAIL;
    }
    memcpy(mac, s_mac, sizeof(s_mac));
    return ESP_OK;
}

void fake_mac_set(const uint8_t mac[6])
{
    memcpy(s_mac, mac, sizeof(s_mac));
}

void fake_mac_set_read_fails(bool fails)
{
    s_mac_read_fails = fails;
}

// --- GPIO ---

static int s_gpio_levels[FAKE_GPIO_PIN_COUNT];
static unsigned int s_gpio_writes[FAKE_GPIO_PIN_COUNT];
static unsigned int s_gpio_configs;

esp_err_t gpio_config(const gpio_config_t *config)
{
    (void) config;
    s_gpio_configs++;
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    if (gpio_num >= 0 && gpio_num < FAKE_GPIO_PIN_COUNT) {
        s_gpio_levels[gpio_num] = (int) level;
        s_gpio_writes[gpio_num]++;
    }
    return ESP_OK;
}

int fake_gpio_level(gpio_num_t gpio)
{
    if (gpio < 0 || gpio >= FAKE_GPIO_PIN_COUNT) {
        return -1;
    }
    return s_gpio_levels[gpio];
}

unsigned int fake_gpio_write_count(gpio_num_t gpio)
{
    if (gpio < 0 || gpio >= FAKE_GPIO_PIN_COUNT) {
        return 0;
    }
    return s_gpio_writes[gpio];
}

unsigned int fake_gpio_config_count(void)
{
    return s_gpio_configs;
}

// --- ADC ---

static int s_adc_scripts[FAKE_ADC_CHANNEL_COUNT][FAKE_ADC_MAX_SCRIPT];
static int s_adc_script_len[FAKE_ADC_CHANNEL_COUNT];
static int s_adc_script_pos[FAKE_ADC_CHANNEL_COUNT];
static bool s_adc_always_fail[FAKE_ADC_CHANNEL_COUNT];
static int s_adc_reads[FAKE_ADC_CHANNEL_COUNT];

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_config,
                               adc_oneshot_unit_handle_t *ret_handle)
{
    static int next_token = 1;
    (void) init_config;
    *ret_handle = (adc_oneshot_unit_handle_t) (uintptr_t) next_token++;
    return ESP_OK;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config)
{
    (void) handle;
    (void) channel;
    (void) config;
    return ESP_OK;
}

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t channel, int *out_raw)
{
    (void) handle;
    if (channel < 0 || channel >= FAKE_ADC_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    s_adc_reads[channel]++;
    if (s_adc_always_fail[channel] || s_adc_script_len[channel] == 0) {
        return ESP_FAIL;
    }
    int pos = s_adc_script_pos[channel];
    *out_raw = s_adc_scripts[channel][pos];
    if (pos + 1 < s_adc_script_len[channel]) {
        s_adc_script_pos[channel] = pos + 1;
    }
    return ESP_OK;
}

void fake_adc_set_samples(adc_channel_t channel, const int *samples, int count)
{
    if (channel < 0 || channel >= FAKE_ADC_CHANNEL_COUNT || count <= 0) {
        return;
    }
    if (count > FAKE_ADC_MAX_SCRIPT) {
        count = FAKE_ADC_MAX_SCRIPT;
    }
    memcpy(s_adc_scripts[channel], samples, (size_t) count * sizeof(int));
    s_adc_script_len[channel] = count;
    s_adc_script_pos[channel] = 0;
    s_adc_always_fail[channel] = false;
}

void fake_adc_set_always_fail(adc_channel_t channel, bool fails)
{
    if (channel >= 0 && channel < FAKE_ADC_CHANNEL_COUNT) {
        s_adc_always_fail[channel] = fails;
    }
}

int fake_adc_read_count(adc_channel_t channel)
{
    if (channel < 0 || channel >= FAKE_ADC_CHANNEL_COUNT) {
        return 0;
    }
    return s_adc_reads[channel];
}

// --- DHT ---

static bool s_dht_ok = true;
static float s_dht_temperature = 23.5f;
static float s_dht_humidity = 55.0f;
static int s_dht_reads;

esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature)
{
    (void) sensor_type;
    (void) pin;
    s_dht_reads++;
    if (!s_dht_ok) {
        return ESP_FAIL;
    }
    *humidity = s_dht_humidity;
    *temperature = s_dht_temperature;
    return ESP_OK;
}

void fake_dht_set_ok(float temperature_c, float humidity_pct)
{
    s_dht_ok = true;
    s_dht_temperature = temperature_c;
    s_dht_humidity = humidity_pct;
}

void fake_dht_set_fail(bool fails)
{
    s_dht_ok = !fails;
}

int fake_dht_read_count(void)
{
    return s_dht_reads;
}

// --- esp_timer ---

typedef struct {
    bool used;
    esp_timer_cb_t callback;
    void *arg;
    bool armed;
    int64_t deadline_us;
} fake_timer_t;

static fake_timer_t s_timers[FAKE_TIMER_MAX];
static int64_t s_fake_now_us;

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle)
{
    for (size_t i = 0; i < FAKE_TIMER_MAX; ++i) {
        if (!s_timers[i].used) {
            s_timers[i].used = true;
            s_timers[i].callback = create_args->callback;
            s_timers[i].arg = create_args->arg;
            s_timers[i].armed = false;
            s_timers[i].deadline_us = 0;
            *out_handle = (esp_timer_handle_t) &s_timers[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    fake_timer_t *entry = (fake_timer_t *) timer;
    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->armed = true;
    entry->deadline_us = s_fake_now_us + (int64_t) timeout_us;
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    fake_timer_t *entry = (fake_timer_t *) timer;
    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    entry->armed = false;
    return ESP_OK;
}

int64_t esp_timer_get_time(void)
{
    return s_fake_now_us;
}

void fake_timer_set_time(int64_t now_us)
{
    s_fake_now_us = now_us;
}

void fake_timer_advance(int64_t delta_us)
{
    s_fake_now_us += delta_us;
    for (size_t i = 0; i < FAKE_TIMER_MAX; ++i) {
        if (s_timers[i].used && s_timers[i].armed && s_fake_now_us >= s_timers[i].deadline_us) {
            s_timers[i].armed = false;
            s_timers[i].callback(s_timers[i].arg);
        }
    }
}

void fake_timer_fire(esp_timer_handle_t timer)
{
    fake_timer_t *entry = (fake_timer_t *) timer;
    if (entry != NULL && entry->used) {
        entry->armed = false;
        entry->callback(entry->arg);
    }
}

void fake_timer_fire_all_armed(void)
{
    for (size_t i = 0; i < FAKE_TIMER_MAX; ++i) {
        if (s_timers[i].used && s_timers[i].armed) {
            s_timers[i].armed = false;
            s_timers[i].callback(s_timers[i].arg);
        }
    }
}

int fake_timer_armed_count(void)
{
    int count = 0;
    for (size_t i = 0; i < FAKE_TIMER_MAX; ++i) {
        count += (s_timers[i].used && s_timers[i].armed) ? 1 : 0;
    }
    return count;
}

bool fake_timer_is_armed(esp_timer_handle_t timer)
{
    fake_timer_t *entry = (fake_timer_t *) timer;
    return entry != NULL && entry->used && entry->armed;
}

int64_t fake_timer_deadline(esp_timer_handle_t timer)
{
    fake_timer_t *entry = (fake_timer_t *) timer;
    return entry != NULL ? entry->deadline_us : 0;
}

// --- reset ---

void fake_runtime_reset(void)
{
    memset(s_nvs_entries, 0, sizeof(s_nvs_entries));
    s_nvs_open_fails = false;
    s_nvs_get_fails = false;
    s_nvs_set_fails = false;
    s_nvs_commit_fails = false;

    const uint8_t default_mac[6] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    memcpy(s_mac, default_mac, sizeof(s_mac));
    s_mac_read_fails = false;

    memset(s_gpio_levels, 0, sizeof(s_gpio_levels));
    memset(s_gpio_writes, 0, sizeof(s_gpio_writes));
    s_gpio_configs = 0;

    memset(s_adc_scripts, 0, sizeof(s_adc_scripts));
    memset(s_adc_script_len, 0, sizeof(s_adc_script_len));
    memset(s_adc_script_pos, 0, sizeof(s_adc_script_pos));
    memset(s_adc_always_fail, 0, sizeof(s_adc_always_fail));
    memset(s_adc_reads, 0, sizeof(s_adc_reads));

    s_dht_ok = true;
    s_dht_temperature = 23.5f;
    s_dht_humidity = 55.0f;
    s_dht_reads = 0;

    // Timer registrations persist across resets like real HAL state: the
    // firmware caches esp_timer handles in statics, so wiping them here
    // would dangle those handles on re-init. Disarm only; every suite
    // re-inits its timers (disarming) in setUp anyway.
    for (size_t i = 0; i < FAKE_TIMER_MAX; ++i) {
        s_timers[i].armed = false;
    }
    s_fake_now_us = 0;
}

#else

// Firmware builds never compile this file (absent from CMake SRCS); fail
// loudly if that invariant ever breaks.
#error "fake_runtime.c is host-test-only and requires PIO_UNIT_TEST"

#endif
