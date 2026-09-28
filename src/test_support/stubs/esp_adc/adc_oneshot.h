// Host-only stub of ESP-IDF esp_adc/adc_oneshot.h: types plus scriptable
// reads backed by fakes/fake_runtime.c.
#pragma once

#include "esp_err.h"

typedef enum {
    ADC_UNIT_1 = 1,
    ADC_UNIT_2 = 2,
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
    ADC_CHANNEL_8,
    ADC_CHANNEL_9,
} adc_channel_t;

typedef enum {
    ADC_ULP_MODE_DISABLE = 0,
} adc_ulp_mode_t;

typedef enum {
    ADC_BITWIDTH_12 = 12,
} adc_bitwidth_t;

typedef enum {
    ADC_ATTEN_DB_12 = 3,
} adc_atten_t;

typedef void *adc_oneshot_unit_handle_t;

typedef struct {
    adc_unit_t unit_id;
    adc_ulp_mode_t ulp_mode;
} adc_oneshot_unit_init_cfg_t;

typedef struct {
    adc_bitwidth_t bitwidth;
    adc_atten_t atten;
} adc_oneshot_chan_cfg_t;

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_config,
                               adc_oneshot_unit_handle_t *ret_handle);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t channel, int *out_raw);
