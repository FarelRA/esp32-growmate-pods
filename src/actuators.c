#include "actuators.h"

#include <stdbool.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "actuators";
static bool s_pump_enabled;
static bool s_light_enabled;
static int64_t s_pump_deadline_us;

static void set_output_level(gpio_num_t gpio, int active_level, bool enabled)
{
    gpio_set_level(gpio, enabled ? active_level : !active_level);
}

static void apply_outputs(const board_profile_t *profile)
{
    set_output_level(profile->pump_gpio, profile->pump_active_level, s_pump_enabled);
    set_output_level(profile->grow_light_gpio, profile->light_active_level, s_light_enabled);
}

static void configure_output_pins(const board_profile_t *profile)
{
    // Camera-bus overlap would silently kill the camera: abort early.
    if (board_profile_gpio_conflicts_with_camera(profile, profile->pump_gpio) ||
        board_profile_gpio_conflicts_with_camera(profile, profile->grow_light_gpio)) {
        ESP_LOGE(TAG, "actuator GPIO conflicts with fixed camera bus, aborting");
        abort();
    }
    // External 100k pulldown + 220R gate series holds MOSFETs OFF through
    // the ~3ms strapping window (GPIO2/4 reset state is weak pulldown).
    // No internal pull here: the external network owns the boot level.
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << profile->pump_gpio) | (1ULL << profile->grow_light_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
}

void actuators_init(const board_profile_t *profile)
{
    configure_output_pins(profile);
    s_pump_enabled = false;
    s_light_enabled = false;
    apply_outputs(profile);
}

void actuators_apply_commands(const board_profile_t *profile, const device_commands_t *commands)
{
    if (commands->has_pump_command && commands->pump_duration_ms > 0) {
            set_output_level(profile->pump_gpio, profile->pump_active_level, true);
            s_pump_enabled = true;
            s_pump_deadline_us = esp_timer_get_time() + ((int64_t) commands->pump_duration_ms * 1000LL);
            ESP_LOGI(TAG, "Pump command active for %d ms", commands->pump_duration_ms);
    }

    if (commands->has_light_command && commands->light_enabled != s_light_enabled) {
        s_light_enabled = commands->light_enabled;
        set_output_level(profile->grow_light_gpio, profile->light_active_level, s_light_enabled);
        ESP_LOGI(TAG, "Grow light %s", s_light_enabled ? "enabled" : "disabled");
    }
}

void actuators_tick(const board_profile_t *profile)
{
    if (s_pump_enabled && esp_timer_get_time() >= s_pump_deadline_us) {
        set_output_level(profile->pump_gpio, profile->pump_active_level, false);
        s_pump_enabled = false;
        s_pump_deadline_us = 0;
        ESP_LOGI(TAG, "Pump timeout reached, disabling pump");
    }
}


bool actuators_is_pump_enabled(void)
{
    return s_pump_enabled;
}

bool actuators_is_light_enabled(void)
{
    return s_light_enabled;
}
