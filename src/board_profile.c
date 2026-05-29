#include "board_profile.h"

#include <stddef.h>

static const board_profile_t BOARD_PROFILES[] = {
    {
        .id = BOARD_PROFILE_AI_THINKER_ESP32_CAM,
        .slug = "ai-thinker-esp32-cam",
        .display_name = "AI Thinker ESP32-CAM",
        .pump_gpio = GPIO_NUM_2,
        .pump_active_level = 1,
        .grow_light_gpio = GPIO_NUM_4,
        .light_active_level = 1,
        .water_level_gpio = GPIO_NUM_13,
        .soil_moisture_gpio = GPIO_NUM_14,
        .light_sensor_gpio = GPIO_NUM_15,
        .dht_gpio = GPIO_NUM_12,
        .analog_unit = ADC_UNIT_2,
        .water_level_channel = ADC_CHANNEL_4,
        .soil_moisture_channel = ADC_CHANNEL_6,
        .light_sensor_channel = ADC_CHANNEL_3,
        .camera_pwdn = GPIO_NUM_32,
        .camera_reset = GPIO_NUM_NC,
        .camera_xclk = GPIO_NUM_0,
        .camera_sda = GPIO_NUM_26,
        .camera_scl = GPIO_NUM_27,
        .camera_d7 = GPIO_NUM_35,
        .camera_d6 = GPIO_NUM_34,
        .camera_d5 = GPIO_NUM_39,
        .camera_d4 = GPIO_NUM_36,
        .camera_d3 = GPIO_NUM_21,
        .camera_d2 = GPIO_NUM_19,
        .camera_d1 = GPIO_NUM_18,
        .camera_d0 = GPIO_NUM_5,
        .camera_vsync = GPIO_NUM_25,
        .camera_href = GPIO_NUM_23,
        .camera_pclk = GPIO_NUM_22,
        .has_camera = true,
    },
};

const board_profile_t *board_profile_get(board_profile_id_t id)
{
    for (size_t i = 0; i < sizeof(BOARD_PROFILES) / sizeof(BOARD_PROFILES[0]); ++i) {
        if (BOARD_PROFILES[i].id == id) {
            return &BOARD_PROFILES[i];
        }
    }

    return &BOARD_PROFILES[0];
}

camera_config_t board_profile_build_camera_config(const board_profile_t *profile)
{
    return (camera_config_t) {
        .pin_pwdn = profile->camera_pwdn,
        .pin_reset = profile->camera_reset,
        .pin_xclk = profile->camera_xclk,
        .pin_sscb_sda = profile->camera_sda,
        .pin_sscb_scl = profile->camera_scl,
        .pin_d7 = profile->camera_d7,
        .pin_d6 = profile->camera_d6,
        .pin_d5 = profile->camera_d5,
        .pin_d4 = profile->camera_d4,
        .pin_d3 = profile->camera_d3,
        .pin_d2 = profile->camera_d2,
        .pin_d1 = profile->camera_d1,
        .pin_d0 = profile->camera_d0,
        .pin_vsync = profile->camera_vsync,
        .pin_href = profile->camera_href,
        .pin_pclk = profile->camera_pclk,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_UXGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_LATEST,
    };
}

bool board_profile_gpio_conflicts_with_camera(const board_profile_t *profile, gpio_num_t gpio)
{
    const gpio_num_t camera_pins[] = {
        profile->camera_pwdn,
        profile->camera_reset,
        profile->camera_xclk,
        profile->camera_sda,
        profile->camera_scl,
        profile->camera_d7,
        profile->camera_d6,
        profile->camera_d5,
        profile->camera_d4,
        profile->camera_d3,
        profile->camera_d2,
        profile->camera_d1,
        profile->camera_d0,
        profile->camera_vsync,
        profile->camera_href,
        profile->camera_pclk,
    };

    for (size_t i = 0; i < sizeof(camera_pins) / sizeof(camera_pins[0]); ++i) {
        if (camera_pins[i] == gpio) {
            return true;
        }
    }

    return false;
}
