// Host-only stub of ESP-IDF esp_camera.h: only the configuration types and
// enum values firmware references. Field names mirror the real driver header
// (including the sccb/sscb union aliases) so designated initializers compile
// unchanged on host and target.
#pragma once

#include <stddef.h>

typedef enum {
    LEDC_TIMER_0 = 0,
} ledc_timer_t;

typedef enum {
    LEDC_CHANNEL_0 = 0,
} ledc_channel_t;

typedef enum {
    PIXFORMAT_JPEG = 4,
} pixformat_t;

typedef enum {
    FRAMESIZE_QVGA = 4,
    FRAMESIZE_CIF = 5,
    FRAMESIZE_VGA = 6,
    FRAMESIZE_SVGA = 7,
    FRAMESIZE_XGA = 8,
} framesize_t;

typedef enum {
    CAMERA_FB_IN_PSRAM = 0,
    CAMERA_FB_IN_DRAM = 1,
} camera_fb_location_t;

typedef enum {
    CAMERA_GRAB_WHEN_EMPTY = 0,
    CAMERA_GRAB_LATEST = 1,
} camera_grab_mode_t;

typedef struct {
    int pin_pwdn;
    int pin_reset;
    int pin_xclk;
    union {
        int pin_sccb_sda;
        int pin_sscb_sda;
    };
    union {
        int pin_sccb_scl;
        int pin_sscb_scl;
    };
    int pin_d7;
    int pin_d6;
    int pin_d5;
    int pin_d4;
    int pin_d3;
    int pin_d2;
    int pin_d1;
    int pin_d0;
    int pin_vsync;
    int pin_href;
    int pin_pclk;

    int xclk_freq_hz;

    ledc_timer_t ledc_timer;
    ledc_channel_t ledc_channel;

    pixformat_t pixel_format;
    framesize_t frame_size;

    int jpeg_quality;
    size_t fb_count;
    camera_fb_location_t fb_location;
    camera_grab_mode_t grab_mode;
} camera_config_t;
