#pragma once

#include "board_profile.h"
#include "esp_camera.h"
#include "esp_err.h"

esp_err_t camera_service_init(const board_profile_t *profile);
void camera_service_deinit(void);
camera_fb_t *camera_service_capture(void);
