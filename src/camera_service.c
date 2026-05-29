#include "camera_service.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "camera";
static bool s_camera_initialized;

static void camera_service_cleanup_driver_state(void)
{
    esp_camera_deinit();
}

esp_err_t camera_service_init(const board_profile_t *profile)
{
    if (s_camera_initialized) {
        return ESP_OK;
    }

    camera_config_t config = board_profile_build_camera_config(profile);
    config.frame_size = esp_psram_is_initialized() ? FRAMESIZE_SVGA : FRAMESIZE_VGA;
    config.fb_count = 1;
    config.jpeg_quality = esp_psram_is_initialized() ? 12 : 14;

    if (config.pin_pwdn != GPIO_NUM_NC) {
        gpio_config_t pwdn_config = {
            .pin_bit_mask = 1ULL << config.pin_pwdn,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&pwdn_config));
        gpio_set_level(config.pin_pwdn, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(config.pin_pwdn, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    camera_service_cleanup_driver_state();

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        camera_service_cleanup_driver_state();
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_camera_initialized = true;
    ESP_LOGI(TAG, "Camera initialized");
    return ESP_OK;
}

void camera_service_deinit(void)
{
    if (!s_camera_initialized) {
        return;
    }

    camera_service_cleanup_driver_state();
    s_camera_initialized = false;
}

camera_fb_t *camera_service_capture(void)
{
    if (!s_camera_initialized) {
        return NULL;
    }

    return esp_camera_fb_get();
}
