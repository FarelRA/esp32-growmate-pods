#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_camera.h"

typedef enum {
    BOARD_PROFILE_AI_THINKER_ESP32_CAM = 0,
} board_profile_id_t;

typedef struct {
    board_profile_id_t id;
    const char *slug;
    const char *display_name;
    gpio_num_t pump_gpio;
    int pump_active_level;
    gpio_num_t grow_light_gpio;
    int light_active_level;
    gpio_num_t water_level_gpio;
    gpio_num_t soil_moisture_gpio;
    gpio_num_t light_sensor_gpio;
    gpio_num_t dht_gpio;
    adc_unit_t analog_unit;
    adc_channel_t water_level_channel;
    adc_channel_t soil_moisture_channel;
    adc_channel_t light_sensor_channel;
    gpio_num_t camera_pwdn;
    gpio_num_t camera_reset;
    gpio_num_t camera_xclk;
    gpio_num_t camera_sda;
    gpio_num_t camera_scl;
    gpio_num_t camera_d7;
    gpio_num_t camera_d6;
    gpio_num_t camera_d5;
    gpio_num_t camera_d4;
    gpio_num_t camera_d3;
    gpio_num_t camera_d2;
    gpio_num_t camera_d1;
    gpio_num_t camera_d0;
    gpio_num_t camera_vsync;
    gpio_num_t camera_href;
    gpio_num_t camera_pclk;
    bool has_camera;
} board_profile_t;

const board_profile_t *board_profile_get(board_profile_id_t id);
camera_config_t board_profile_build_camera_config(const board_profile_t *profile);
bool board_profile_gpio_conflicts_with_camera(const board_profile_t *profile, gpio_num_t gpio);
