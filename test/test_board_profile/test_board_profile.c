#include <unity.h>

#include "board_profile.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_get_returns_ai_thinker_profile(void)
{
    // Arrange + act.
    const board_profile_t *profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);

    // Assert.
    TEST_ASSERT_NOT_NULL(profile);
    TEST_ASSERT_EQUAL_STRING("ai-thinker-esp32-cam", profile->slug);
    TEST_ASSERT_EQUAL_STRING("AI Thinker ESP32-CAM", profile->display_name);
    TEST_ASSERT_TRUE(profile->has_camera);
}

void test_get_falls_back_to_first_profile_for_unknown_id(void)
{
    // Arrange + act.
    const board_profile_t *fallback = board_profile_get((board_profile_id_t) 99);

    // Assert: never NULL, always a bootable profile.
    TEST_ASSERT_NOT_NULL(fallback);
    TEST_ASSERT_EQUAL_PTR(board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM), fallback);
}

void test_gpio_conflict_detects_camera_bus_pins(void)
{
    // Arrange.
    const board_profile_t *profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);

    // Act + assert: known camera-bus pins report a conflict...
    TEST_ASSERT_TRUE(board_profile_gpio_conflicts_with_camera(profile, profile->camera_xclk));
    TEST_ASSERT_TRUE(board_profile_gpio_conflicts_with_camera(profile, profile->camera_d0));
    TEST_ASSERT_TRUE(board_profile_gpio_conflicts_with_camera(profile, profile->camera_pwdn));
    // ...while every actuator and sensor GPIO stays clear of the bus.
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->pump_gpio));
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->grow_light_gpio));
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->soil_moisture_gpio));
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->light_sensor_gpio));
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->water_level_gpio));
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, profile->dht_gpio));
}

void test_gpio_conflict_skips_unconnected_camera_pins(void)
{
    // Arrange: camera_reset is GPIO_NUM_NC on this board.
    const board_profile_t *profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);

    // Act + assert: the NC sentinel never reports a conflict.
    TEST_ASSERT_EQUAL_INT(GPIO_NUM_NC, profile->camera_reset);
    TEST_ASSERT_FALSE(board_profile_gpio_conflicts_with_camera(profile, GPIO_NUM_NC));
}

void test_camera_config_builder_maps_profile_pins(void)
{
    // Arrange.
    const board_profile_t *profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);

    // Act.
    camera_config_t config = board_profile_build_camera_config(profile);

    // Assert.
    TEST_ASSERT_EQUAL_INT(profile->camera_pwdn, config.pin_pwdn);
    TEST_ASSERT_EQUAL_INT(profile->camera_xclk, config.pin_xclk);
    TEST_ASSERT_EQUAL_INT(profile->camera_sda, config.pin_sscb_sda);
    TEST_ASSERT_EQUAL_INT(profile->camera_scl, config.pin_sscb_scl);
    TEST_ASSERT_EQUAL_INT(profile->camera_d7, config.pin_d7);
    TEST_ASSERT_EQUAL_INT(profile->camera_d0, config.pin_d0);
    TEST_ASSERT_EQUAL_INT(profile->camera_vsync, config.pin_vsync);
    TEST_ASSERT_EQUAL_INT(profile->camera_href, config.pin_href);
    TEST_ASSERT_EQUAL_INT(profile->camera_pclk, config.pin_pclk);
    TEST_ASSERT_EQUAL_INT(PIXFORMAT_JPEG, config.pixel_format);
    TEST_ASSERT_EQUAL_INT(FRAMESIZE_SVGA, config.frame_size);
    TEST_ASSERT_EQUAL_INT(12, config.jpeg_quality);
    TEST_ASSERT_EQUAL_UINT(1, config.fb_count);
    TEST_ASSERT_EQUAL_INT(CAMERA_GRAB_LATEST, config.grab_mode);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_returns_ai_thinker_profile);
    RUN_TEST(test_get_falls_back_to_first_profile_for_unknown_id);
    RUN_TEST(test_gpio_conflict_detects_camera_bus_pins);
    RUN_TEST(test_gpio_conflict_skips_unconnected_camera_pins);
    RUN_TEST(test_camera_config_builder_maps_profile_pins);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
