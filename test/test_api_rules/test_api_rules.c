#include <math.h>
#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "app_build_config.h"
#include "app_config.h"
#include "api_rules.h"

#define COMMAND_ID_MAX_LEN 31
#define CLAIM_ID_MAX_LEN APP_CONFIG_MAX_DEVICE_ID_LEN
#define DOSE_MIN_MS 1
#define INTERVAL_MIN_SEC API_RULE_REPORT_INTERVAL_MIN_SEC
#define INTERVAL_MAX_SEC API_RULE_REPORT_INTERVAL_MAX_SEC
#define RETRY_AFTER_MAX_SEC API_RULE_RETRY_AFTER_MAX_SEC
#define APPLIED_CONFIG_REV 7u

void setUp(void)
{
}

void tearDown(void)
{
}

// --- command ids ---

void test_command_id_accepts_typical_and_boundary_ids(void)
{
    TEST_ASSERT_TRUE(api_rule_id_valid("cmd-9f3", COMMAND_ID_MAX_LEN));
    TEST_ASSERT_TRUE(api_rule_id_valid("A", COMMAND_ID_MAX_LEN));
}

void test_command_id_accepts_max_length_id(void)
{
    // Arrange: exactly 31 chars, the longest id the ring slot NUL-terminates.
    char id[COMMAND_ID_MAX_LEN + 1];
    memset(id, 'x', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_TRUE(api_rule_id_valid(id, COMMAND_ID_MAX_LEN));
}

void test_command_id_rejects_empty_null_and_overlong(void)
{
    // Arrange: 32 chars no longer fit beside the NUL.
    char overlong[COMMAND_ID_MAX_LEN + 2];
    memset(overlong, 'x', sizeof(overlong) - 1);
    overlong[sizeof(overlong) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_FALSE(api_rule_id_valid("", COMMAND_ID_MAX_LEN));
    TEST_ASSERT_FALSE(api_rule_id_valid(NULL, COMMAND_ID_MAX_LEN));
    TEST_ASSERT_FALSE(api_rule_id_valid(overlong, COMMAND_ID_MAX_LEN));
}

// --- dose caps ---

void test_dose_ms_accepts_window_edges(void)
{
    TEST_ASSERT_TRUE(api_rule_dose_ms_valid((double) DOSE_MIN_MS));
    TEST_ASSERT_TRUE(api_rule_dose_ms_valid((double) APP_MAX_PUMP_DURATION_MS));
}

void test_dose_ms_rejects_zero_negative_and_over_cap(void)
{
    TEST_ASSERT_FALSE(api_rule_dose_ms_valid(0.0));
    TEST_ASSERT_FALSE(api_rule_dose_ms_valid(-100.0));
    TEST_ASSERT_FALSE(api_rule_dose_ms_valid((double) APP_MAX_PUMP_DURATION_MS + 1.0));
}

void test_dose_ms_rejects_non_finite(void)
{
    TEST_ASSERT_FALSE(api_rule_dose_ms_valid(NAN));
    TEST_ASSERT_FALSE(api_rule_dose_ms_valid(INFINITY));
}

// --- config-rev discipline (mirror of tests/host_test.py) ---

void test_config_rev_applies_strictly_newer_rev(void)
{
    TEST_ASSERT_TRUE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, 8.0));
}

void test_config_rev_rejects_stale_duplicate_and_zero(void)
{
    TEST_ASSERT_FALSE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, 7.0));
    TEST_ASSERT_FALSE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, 6.0));
    TEST_ASSERT_FALSE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, 0.0));
}

void test_config_rev_rejects_non_finite(void)
{
    TEST_ASSERT_FALSE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, NAN));
    TEST_ASSERT_FALSE(api_rule_config_rev_should_apply(APPLIED_CONFIG_REV, INFINITY));
}

// --- report-interval clamp ---

void test_report_interval_passes_through_sane_value(void)
{
    // Arrange.
    const double sane_interval = 300.0;

    // Act.
    uint32_t clamped = api_rule_clamp_report_interval(sane_interval);

    // Assert.
    TEST_ASSERT_EQUAL_UINT32((uint32_t) sane_interval, clamped);
}

void test_report_interval_clamps_to_window_edges(void)
{
    TEST_ASSERT_EQUAL_UINT32(INTERVAL_MIN_SEC, api_rule_clamp_report_interval(0.0));
    TEST_ASSERT_EQUAL_UINT32(INTERVAL_MIN_SEC, api_rule_clamp_report_interval(9.9));
    TEST_ASSERT_EQUAL_UINT32(INTERVAL_MIN_SEC, api_rule_clamp_report_interval((double) INTERVAL_MIN_SEC));
    TEST_ASSERT_EQUAL_UINT32(INTERVAL_MAX_SEC, api_rule_clamp_report_interval((double) INTERVAL_MAX_SEC));
    TEST_ASSERT_EQUAL_UINT32(INTERVAL_MAX_SEC, api_rule_clamp_report_interval(99999.0));
}

// --- Retry-After clamp ---

void test_retry_after_passes_through_sane_wait(void)
{
    TEST_ASSERT_EQUAL_INT(30, api_rule_clamp_retry_after_sec(30));
}

void test_retry_after_clamps_to_backoff_window(void)
{
    TEST_ASSERT_EQUAL_INT(0, api_rule_clamp_retry_after_sec(-5));
    TEST_ASSERT_EQUAL_INT(0, api_rule_clamp_retry_after_sec(0));
    TEST_ASSERT_EQUAL_INT(RETRY_AFTER_MAX_SEC, api_rule_clamp_retry_after_sec(60));
    TEST_ASSERT_EQUAL_INT(RETRY_AFTER_MAX_SEC, api_rule_clamp_retry_after_sec(999));
}

// --- claim charset (mirror of tests/host_test.py) ---

void test_claim_charset_accepts_device_id_alphabet(void)
{
    TEST_ASSERT_TRUE(api_rule_claim_charset_valid("POD-9_abc"));
    TEST_ASSERT_TRUE(api_rule_claim_charset_valid("IAET01"));
}

void test_claim_charset_rejects_empty_null_and_bad_chars(void)
{
    TEST_ASSERT_FALSE(api_rule_claim_charset_valid(""));
    TEST_ASSERT_FALSE(api_rule_claim_charset_valid(NULL));
    TEST_ASSERT_FALSE(api_rule_claim_charset_valid("bad id!"));
    TEST_ASSERT_FALSE(api_rule_claim_charset_valid("caf\xc3\xa9"));
}

void test_claim_id_length_cap_matches_config_slot(void)
{
    // Arrange: 32 chars exceed the NVS device_id slot (31 + NUL).
    char overlong[CLAIM_ID_MAX_LEN + 2];
    memset(overlong, 'x', sizeof(overlong) - 1);
    overlong[sizeof(overlong) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_TRUE(api_rule_id_valid("POD-9_abc", CLAIM_ID_MAX_LEN));
    TEST_ASSERT_FALSE(api_rule_id_valid(overlong, CLAIM_ID_MAX_LEN));
}

// --- SSID bounds (mirror of tests/host_test.py) ---

void test_ssid_accepts_shortest_and_longest_joinable(void)
{
    // Arrange: 31 chars is the longest SSID with room for the driver NUL.
    char longest[32];
    memset(longest, 'x', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_TRUE(api_rule_ssid_valid("A"));
    TEST_ASSERT_TRUE(api_rule_ssid_valid(longest));
}

void test_ssid_rejects_empty_null_and_32_octets(void)
{
    // Arrange: a full 32-octet SSID can never join, so it must fail loudly.
    char full[33];
    memset(full, 'x', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_FALSE(api_rule_ssid_valid(""));
    TEST_ASSERT_FALSE(api_rule_ssid_valid(NULL));
    TEST_ASSERT_FALSE(api_rule_ssid_valid(full));
}

void test_raw_sample_included_only_when_available_and_non_negative(void)
{
    TEST_ASSERT_TRUE(api_rule_raw_sample_included(true, 0));
    TEST_ASSERT_TRUE(api_rule_raw_sample_included(true, 2048));
    TEST_ASSERT_FALSE(api_rule_raw_sample_included(true, -1));
    TEST_ASSERT_FALSE(api_rule_raw_sample_included(false, 2048));
}

void test_dht_sample_included_only_when_available_and_finite(void)
{
    TEST_ASSERT_TRUE(api_rule_dht_sample_included(true, 24.5f));
    TEST_ASSERT_FALSE(api_rule_dht_sample_included(true, NAN));
    TEST_ASSERT_FALSE(api_rule_dht_sample_included(true, INFINITY));
    TEST_ASSERT_FALSE(api_rule_dht_sample_included(false, 24.5f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_command_id_accepts_typical_and_boundary_ids);
    RUN_TEST(test_command_id_accepts_max_length_id);
    RUN_TEST(test_command_id_rejects_empty_null_and_overlong);
    RUN_TEST(test_dose_ms_accepts_window_edges);
    RUN_TEST(test_dose_ms_rejects_zero_negative_and_over_cap);
    RUN_TEST(test_dose_ms_rejects_non_finite);
    RUN_TEST(test_config_rev_applies_strictly_newer_rev);
    RUN_TEST(test_config_rev_rejects_stale_duplicate_and_zero);
    RUN_TEST(test_config_rev_rejects_non_finite);
    RUN_TEST(test_report_interval_passes_through_sane_value);
    RUN_TEST(test_report_interval_clamps_to_window_edges);
    RUN_TEST(test_retry_after_passes_through_sane_wait);
    RUN_TEST(test_retry_after_clamps_to_backoff_window);
    RUN_TEST(test_claim_charset_accepts_device_id_alphabet);
    RUN_TEST(test_claim_charset_rejects_empty_null_and_bad_chars);
    RUN_TEST(test_claim_id_length_cap_matches_config_slot);
    RUN_TEST(test_ssid_accepts_shortest_and_longest_joinable);
    RUN_TEST(test_ssid_rejects_empty_null_and_32_octets);
    RUN_TEST(test_raw_sample_included_only_when_available_and_non_negative);
    RUN_TEST(test_dht_sample_included_only_when_available_and_finite);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
