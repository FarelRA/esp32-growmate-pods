#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "app_build_config.h"
#include "app_config.h"
#include "device_identity.h"
#include "fakes/fake_runtime.h"

static const uint8_t TEST_MAC[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};

void setUp(void)
{
    fake_runtime_reset();
}

void tearDown(void)
{
}

void test_effective_id_prefers_assigned_over_default(void)
{
    // Arrange.
    app_config_t assigned = {0};
    snprintf(assigned.device_id, sizeof(assigned.device_id), "POD-9_abc");
    app_config_t unclaimed = {0};

    // Act + assert.
    TEST_ASSERT_EQUAL_STRING("POD-9_abc", device_effective_id(&assigned));
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, device_effective_id(&unclaimed));
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, device_effective_id(NULL));
}

void test_hwid_formats_mac_as_uppercase_hex(void)
{
    // Arrange.
    fake_mac_set(TEST_MAC);
    char hwid[13] = {0};

    // Act.
    device_identity_hwid(hwid);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("AABBCC112233", hwid);
}

void test_hwid_falls_back_to_zeros_when_mac_unreadable(void)
{
    // Arrange.
    fake_mac_set_read_fails(true);
    char hwid[13] = {0};

    // Act.
    device_identity_hwid(hwid);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("000000000000", hwid);
}

void test_ap_credentials_derive_from_identity_and_hwid(void)
{
    // Arrange: assigned pod id with a 6-char tail, MAC with a 6-char tail.
    app_config_t config = {0};
    snprintf(config.device_id, sizeof(config.device_id), "POD-9_abc");
    fake_mac_set(TEST_MAC);
    char ssid[33] = {0};
    char pass[64] = {0};

    // Act.
    device_onboarding_ap_credentials(&config, ssid, pass);

    // Assert: SSID carries the id tail, password the MAC tail (never logged).
    TEST_ASSERT_EQUAL_STRING("GrowMate--9_abc", ssid);
    TEST_ASSERT_EQUAL_STRING("GrowMate-112233", pass);
}

void test_ap_credentials_use_whole_short_id(void)
{
    // Arrange: ids shorter than six chars keep the full id, not garbage.
    app_config_t config = {0};
    snprintf(config.device_id, sizeof(config.device_id), "AB");
    char ssid[33] = {0};
    char pass[64] = {0};

    // Act.
    device_onboarding_ap_credentials(&config, ssid, pass);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("GrowMate-AB", ssid);
}

void test_ap_credentials_fall_back_to_build_default(void)
{
    // Arrange: unclaimed device advertises the build identity.
    app_config_t config = {0};
    char ssid[33] = {0};
    char pass[64] = {0};

    // Act.
    device_onboarding_ap_credentials(&config, ssid, pass);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("GrowMate-" APP_DEVICE_ID, ssid);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_effective_id_prefers_assigned_over_default);
    RUN_TEST(test_hwid_formats_mac_as_uppercase_hex);
    RUN_TEST(test_hwid_falls_back_to_zeros_when_mac_unreadable);
    RUN_TEST(test_ap_credentials_derive_from_identity_and_hwid);
    RUN_TEST(test_ap_credentials_use_whole_short_id);
    RUN_TEST(test_ap_credentials_fall_back_to_build_default);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
