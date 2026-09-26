#include "actuators.h"

#include <stdbool.h>
#include <stdlib.h>

#include "app_build_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "actuators";
static bool s_pump_enabled;
static bool s_light_enabled;
static int64_t s_pump_deadline_us;
static esp_timer_handle_t s_pump_timer = NULL;
static const board_profile_t *s_profile = NULL;
static esp_timer_handle_t s_light_timer = NULL;

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

static void pump_safety_timer_callback(void *arg)
{
    (void)arg;
    if (s_profile == NULL) {
        s_pump_enabled = false;
        s_pump_deadline_us = 0;
        ESP_LOGI(TAG, "Pump safety timer expired, pump OFF");
        return;
    }
    gpio_set_level(s_profile->pump_gpio, !s_profile->pump_active_level);
    s_pump_enabled = false;
    s_pump_deadline_us = 0;
    ESP_LOGI(TAG, "Pump safety timer expired, pump OFF");
}

static void light_failsafe_timer_callback(void *arg)
{
    (void)arg;
    if (s_profile == NULL) {
        s_light_enabled = false;
        ESP_LOGI(TAG, "Light failsafe timer expired, light OFF");
        return;
    }
    gpio_set_level(s_profile->grow_light_gpio, !s_profile->light_active_level);
    s_light_enabled = false;
    ESP_LOGI(TAG, "Light failsafe timer expired, light OFF");
}

void actuators_init(const board_profile_t *profile)
{
    configure_output_pins(profile);
    s_profile = profile;
    s_pump_enabled = false;
    s_light_enabled = false;
    s_pump_deadline_us = 0;
    if (s_pump_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = pump_safety_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "pump_safety",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_pump_timer));
    } else {
        (void)esp_timer_stop(s_pump_timer);
    }
    if (s_light_timer == NULL) {
        esp_timer_create_args_t light_args = {
            .callback = light_failsafe_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "light_failsafe",
        };
        ESP_ERROR_CHECK(esp_timer_create(&light_args, &s_light_timer));
    } else {
        (void)esp_timer_stop(s_light_timer);
    }
    apply_outputs(profile);
}

void actuators_apply_commands(const board_profile_t *profile, const device_commands_t *commands)
{
    if (commands->has_light_command) {
        // Failsafe: a lost "off" must not leave the strip on forever.
        // Re-sent "on" commands refresh the window; expiry only fires
        // after a full day of server silence.
        (void)esp_timer_stop(s_light_timer);
        if (commands->light_enabled) {
            ESP_ERROR_CHECK(esp_timer_start_once(s_light_timer, APP_MAX_LIGHT_ON_MS * 1000ULL));
        }
        if (commands->light_enabled != s_light_enabled) {
            s_light_enabled = commands->light_enabled;
            set_output_level(profile->grow_light_gpio, profile->light_active_level, s_light_enabled);
            ESP_LOGI(TAG, "Grow light %s", s_light_enabled ? "enabled" : "disabled");
        }
    }

    if (!commands->has_pump_command) {
        return;
    }

    if (commands->pump_duration_ms <= 0 || commands->pump_duration_ms > APP_MAX_PUMP_DURATION_MS) {
        ESP_LOGW(TAG, "Pump command %d ms rejected (limit %d ms), pump remains OFF",
            commands->pump_duration_ms, APP_MAX_PUMP_DURATION_MS);
        return;
    }

    set_output_level(profile->pump_gpio, profile->pump_active_level, true);
    s_pump_enabled = true;
    s_pump_deadline_us = esp_timer_get_time() + ((int64_t) commands->pump_duration_ms * 1000LL);
    (void)esp_timer_stop(s_pump_timer);
    ESP_ERROR_CHECK(esp_timer_start_once(s_pump_timer, (uint64_t) commands->pump_duration_ms * 1000ULL));
    ESP_LOGI(TAG, "Pump ON for %d ms (safety timer armed)", commands->pump_duration_ms);
}

void actuators_tick(const board_profile_t *profile)
{
    if (s_pump_enabled && esp_timer_get_time() >= s_pump_deadline_us) {
        set_output_level(profile->pump_gpio, profile->pump_active_level, false);
        s_pump_enabled = false;
        s_pump_deadline_us = 0;
        (void)esp_timer_stop(s_pump_timer);
        ESP_LOGI(TAG, "Pump timeout reached, pump OFF");
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
