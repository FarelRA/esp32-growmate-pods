#pragma once

#include <stdbool.h>

#include "esp_camera.h"

// PSRAM-dependent frame selection shared by camera_service.c and the host
// suite. No driver calls: safe to compile on host.
void camera_logic_pick_frame(bool psram_present,
                             framesize_t *frame_size,
                             int *jpeg_quality,
                             camera_fb_location_t *fb_location);
