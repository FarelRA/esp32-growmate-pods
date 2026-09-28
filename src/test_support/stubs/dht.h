// Host-only stub of the ESP-IDF DHT driver header (esp-idf-lib/dht).
// Read outcomes are scripted per test through fakes/fake_runtime.h.
#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

typedef enum {
    DHT_TYPE_DHT11 = 0,
    DHT_TYPE_AM2301,
    DHT_TYPE_SI7021,
} dht_sensor_type_t;

esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
                              float *humidity, float *temperature);
