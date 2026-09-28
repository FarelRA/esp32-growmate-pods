#include <unity.h>

#include "app_build_config.h"
#include "camera_logic.h"
#include "esp_camera.h"
#include "main_logic.h"
#include "network_rules.h"

#define SANE_INTERVAL_SEC 15u
#define SLOW_INTERVAL_SEC 3600u
#define CAMERA_PERIOD_LOOPS (APP_CAMERA_INTERVAL_SEC / SANE_INTERVAL_SEC)

void setUp(void)
{
}

void tearDown(void)
{
}

// --- SoftAP credential guards ---

void test_ap_name_rejects_null_and_empty(void)
{
    TEST_ASSERT_TRUE(network_ap_name_valid("GrowMate-IAET01"));
    TEST_ASSERT_FALSE(network_ap_name_valid(""));
    TEST_ASSERT_FALSE(network_ap_name_valid(NULL));
}

void test_ap_password_enforces_wpa2_minimum(void)
{
    // Arrange: 8 chars is the WPA2 minimum; 7 must fail closed, never open.
    TEST_ASSERT_TRUE(network_ap_password_valid("12345678"));
    TEST_ASSERT_TRUE(network_ap_password_valid("a-long-passphrase"));
    TEST_ASSERT_FALSE(network_ap_password_valid("1234567"));
    TEST_ASSERT_FALSE(network_ap_password_valid(""));
    TEST_ASSERT_FALSE(network_ap_password_valid(NULL));
}

void test_station_args_reject_null_config(void)
{
    // Arrange.
    app_config_t config = {0};

    // Act + assert.
    TEST_ASSERT_TRUE(network_station_args_valid(&config));
    TEST_ASSERT_FALSE(network_station_args_valid(NULL));
}

void test_scan_args_reject_null_sinks_and_empty_scans(void)
{
    // Arrange.
    network_scan_result_t results[NETWORK_MANAGER_MAX_SCAN_RESULTS] = {0};
    size_t count = 0;

    // Act + assert.
    TEST_ASSERT_TRUE(network_scan_args_valid(results, &count, 4));
    TEST_ASSERT_FALSE(network_scan_args_valid(NULL, &count, 4));
    TEST_ASSERT_FALSE(network_scan_args_valid(results, NULL, 4));
    TEST_ASSERT_FALSE(network_scan_args_valid(results, &count, 0));
}

void test_scan_limit_clamps_to_driver_maximum(void)
{
    TEST_ASSERT_EQUAL_UINT(4, network_scan_limit(4));
    TEST_ASSERT_EQUAL_UINT(NETWORK_MANAGER_MAX_SCAN_RESULTS,
                           network_scan_limit(NETWORK_MANAGER_MAX_SCAN_RESULTS));
    TEST_ASSERT_EQUAL_UINT(NETWORK_MANAGER_MAX_SCAN_RESULTS,
                           network_scan_limit(NETWORK_MANAGER_MAX_SCAN_RESULTS + 1));
}

// --- camera frame selection ---

void test_camera_pick_frame_prefers_psram_quality(void)
{
    // Arrange.
    framesize_t frame = FRAMESIZE_VGA;
    int quality = 0;
    camera_fb_location_t location = CAMERA_FB_IN_DRAM;

    // Act.
    camera_logic_pick_frame(true, &frame, &quality, &location);

    // Assert.
    TEST_ASSERT_EQUAL_INT(FRAMESIZE_SVGA, frame);
    TEST_ASSERT_EQUAL_INT(12, quality);
    TEST_ASSERT_EQUAL_INT(CAMERA_FB_IN_PSRAM, location);
}

void test_camera_pick_frame_degrades_without_psram(void)
{
    // Arrange.
    framesize_t frame = FRAMESIZE_SVGA;
    int quality = 0;
    camera_fb_location_t location = CAMERA_FB_IN_PSRAM;

    // Act.
    camera_logic_pick_frame(false, &frame, &quality, &location);

    // Assert.
    TEST_ASSERT_EQUAL_INT(FRAMESIZE_VGA, frame);
    TEST_ASSERT_EQUAL_INT(14, quality);
    TEST_ASSERT_EQUAL_INT(CAMERA_FB_IN_DRAM, location);
}

// --- main-loop scheduling ---

void test_report_interval_falls_back_to_build_default(void)
{
    TEST_ASSERT_EQUAL_UINT32(SANE_INTERVAL_SEC, main_logic_report_interval_sec(0));
    TEST_ASSERT_EQUAL_UINT32(300, main_logic_report_interval_sec(300));
}

void test_camera_slot_disabled_resets_counter_and_never_due(void)
{
    // Arrange.
    uint32_t loops = 41;

    // Act.
    bool due = main_logic_camera_slot_due(false, SANE_INTERVAL_SEC, &loops);

    // Assert.
    TEST_ASSERT_FALSE(due);
    TEST_ASSERT_EQUAL_UINT32(0, loops);
}

void test_camera_slot_due_at_end_of_period(void)
{
    // Arrange: 900 s period at 15 s cadence = 60 loops.
    uint32_t loops = CAMERA_PERIOD_LOOPS - 1;

    // Act: one more loop closes the period...
    bool due = main_logic_camera_slot_due(true, SANE_INTERVAL_SEC, &loops);

    // Assert.
    TEST_ASSERT_TRUE(due);
    TEST_ASSERT_EQUAL_UINT32(CAMERA_PERIOD_LOOPS, loops);
}

void test_camera_slot_not_due_mid_period(void)
{
    // Arrange.
    uint32_t loops = 0;

    // Act.
    bool due = main_logic_camera_slot_due(true, SANE_INTERVAL_SEC, &loops);

    // Assert.
    TEST_ASSERT_FALSE(due);
    TEST_ASSERT_EQUAL_UINT32(1, loops);
}

void test_camera_slot_clamps_degenerate_period_to_one(void)
{
    // Arrange: a 3600 s interval makes 900/3600 == 0; the slot must still fire.
    uint32_t loops = 0;

    // Act.
    bool due = main_logic_camera_slot_due(true, SLOW_INTERVAL_SEC, &loops);

    // Assert.
    TEST_ASSERT_TRUE(due);
    TEST_ASSERT_EQUAL_UINT32(1, loops);
}

void test_camera_slot_degrades_gracefully_on_zero_interval(void)
{
    // Arrange: a zero interval can never reach the slot math (the main loop
    // always substitutes the build default first); the helper guards anyway.
    uint32_t loops = 0;

    // Act.
    bool due = main_logic_camera_slot_due(true, 0, &loops);

    // Assert: default 15 s cadence, first loop not due.
    TEST_ASSERT_FALSE(due);
    TEST_ASSERT_EQUAL_UINT32(1, loops);
}

void test_portal_reopens_at_failure_threshold(void)
{
    TEST_ASSERT_FALSE(main_logic_should_reopen_portal(APP_ONBOARDING_FAILURE_THRESHOLD - 1));
    TEST_ASSERT_TRUE(main_logic_should_reopen_portal(APP_ONBOARDING_FAILURE_THRESHOLD));
    TEST_ASSERT_TRUE(main_logic_should_reopen_portal(APP_ONBOARDING_FAILURE_THRESHOLD + 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ap_name_rejects_null_and_empty);
    RUN_TEST(test_ap_password_enforces_wpa2_minimum);
    RUN_TEST(test_station_args_reject_null_config);
    RUN_TEST(test_scan_args_reject_null_sinks_and_empty_scans);
    RUN_TEST(test_scan_limit_clamps_to_driver_maximum);
    RUN_TEST(test_camera_pick_frame_prefers_psram_quality);
    RUN_TEST(test_camera_pick_frame_degrades_without_psram);
    RUN_TEST(test_report_interval_falls_back_to_build_default);
    RUN_TEST(test_camera_slot_disabled_resets_counter_and_never_due);
    RUN_TEST(test_camera_slot_due_at_end_of_period);
    RUN_TEST(test_camera_slot_not_due_mid_period);
    RUN_TEST(test_camera_slot_clamps_degenerate_period_to_one);
    RUN_TEST(test_camera_slot_degrades_gracefully_on_zero_interval);
    RUN_TEST(test_portal_reopens_at_failure_threshold);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
