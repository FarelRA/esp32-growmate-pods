#pragma once

#include "board_profile.h"
#include "esp_err.h"

typedef struct {
    bool available;
    int raw;
    float value;
} sensor_measurement_t;

typedef struct {
    sensor_measurement_t soil;
    sensor_measurement_t light;
    sensor_measurement_t water;
    sensor_measurement_t temperature;
    sensor_measurement_t air;
} sensor_snapshot_t;

void sensors_init(const board_profile_t *profile);
esp_err_t sensors_read_all(const board_profile_t *profile, sensor_snapshot_t *snapshot);
