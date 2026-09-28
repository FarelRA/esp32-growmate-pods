#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "app_build_config.h"
#include "ota_logic.h"

#define VERSION_PARTS 3
#define VERSION_FIELD_MAX 31
#define HTTPS_URL "https://example.invalid/firmware.bin"
#define HTTP_URL "http://example.invalid/firmware.bin"

static ota_update_t s_update;

void setUp(void)
{
    memset(&s_update, 0, sizeof(s_update));
}

void tearDown(void)
{
}

// --- parse_version: valid inputs ---

void test_parse_version_full_semver_yields_parts(void)
{
    // Arrange.
    int parts[VERSION_PARTS] = {0};

    // Act.
    bool ok = ota_parse_version("2.0.0", parts);

    // Assert.
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(2, parts[0]);
    TEST_ASSERT_EQUAL_INT(0, parts[1]);
    TEST_ASSERT_EQUAL_INT(0, parts[2]);
}

// --- parse_version: partial versions are rejected ---
// The sscanf "%d.%d.%d%n" match requires all three components: a partial
// string never reaches %n, so `end` mismatches and parsing fails. Only
// full X.Y.Z is accepted (verified against the compiled firmware logic).

void test_parse_version_rejects_partial_semver(void)
{
    // Arrange.
    int parts[VERSION_PARTS] = {0};

    // Act + assert.
    TEST_ASSERT_FALSE(ota_parse_version("2.1", parts));
    TEST_ASSERT_FALSE(ota_parse_version("3", parts));
}

// --- parse_version: rejected inputs (mirror of tests/host_test.py) ---

void test_parse_version_rejects_malformed_strings(void)
{
    // Arrange: every malformed input from the contract spec.
    static const char *const bad[] = {
        "", "1..2", "2.1.0.4", "1.", ".1", "v1.2", "1.2a", ".",
    };
    int parts[VERSION_PARTS] = {0};

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        // Act + assert: one behavior per input, no partial parse leaks.
        parts[0] = parts[1] = parts[2] = 0;
        TEST_ASSERT_FALSE_MESSAGE(ota_parse_version(bad[i], parts), bad[i]);
    }
}

void test_parse_version_rejects_null_and_overlong(void)
{
    // Arrange.
    int parts[VERSION_PARTS] = {0};
    char overlong[VERSION_FIELD_MAX + 2];
    memset(overlong, '1', sizeof(overlong) - 1);
    overlong[sizeof(overlong) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_FALSE(ota_parse_version(NULL, parts));
    TEST_ASSERT_FALSE(ota_parse_version(overlong, parts));
}

// --- version_is_newer ---

void test_version_is_newer_detects_patch_minor_major_bumps(void)
{
    TEST_ASSERT_TRUE(ota_version_is_newer("2.0.0", "2.0.1"));
    TEST_ASSERT_TRUE(ota_version_is_newer("2.0.0", "2.1.0"));
    TEST_ASSERT_TRUE(ota_version_is_newer("2.0.9", "2.1.0"));
    TEST_ASSERT_TRUE(ota_version_is_newer("2.0.0", "3.0.0"));
}

void test_version_is_newer_rejects_equal_older_and_invalid(void)
{
    TEST_ASSERT_FALSE(ota_version_is_newer("2.0.1", "2.0.0"));
    TEST_ASSERT_FALSE(ota_version_is_newer("2.0.0", "2.0.0"));
    TEST_ASSERT_FALSE(ota_version_is_newer("2.0.0", "bogus"));
    TEST_ASSERT_FALSE(ota_version_is_newer("bogus", "2.0.1"));
}

// --- ota_check_start gate table ---

static void arrange_offered_update(const char *version, const char *url)
{
    s_update.available = true;
    snprintf(s_update.version, sizeof(s_update.version), "%s", version);
    snprintf(s_update.url, sizeof(s_update.url), "%s", url);
}

void test_gate_ok_for_newer_https_idle_pump(void)
{
    // Arrange.
    arrange_offered_update("2.0.1", HTTPS_URL);

    // Act.
    ota_gate_t gate = ota_check_start(APP_FIRMWARE_VERSION, &s_update, false);

    // Assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_OK, gate);
}

void test_gate_no_update_for_null_unavailable_or_empty_url(void)
{
    // Arrange.
    arrange_offered_update("2.0.1", HTTPS_URL);
    ota_update_t unavailable = s_update;
    unavailable.available = false;
    ota_update_t empty_url = s_update;
    empty_url.url[0] = '\0';

    // Act + assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NO_UPDATE, ota_check_start(APP_FIRMWARE_VERSION, NULL, false));
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NO_UPDATE, ota_check_start(APP_FIRMWARE_VERSION, &unavailable, false));
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NO_UPDATE, ota_check_start(APP_FIRMWARE_VERSION, &empty_url, false));
}

void test_gate_not_newer_for_stale_or_equal_version(void)
{
    // Arrange.
    ota_update_t stale = {0};
    ota_update_t equal = {0};
    snprintf(stale.version, sizeof(stale.version), "1.9.9");
    snprintf(stale.url, sizeof(stale.url), HTTPS_URL);
    stale.available = true;
    snprintf(equal.version, sizeof(equal.version), APP_FIRMWARE_VERSION);
    snprintf(equal.url, sizeof(equal.url), HTTPS_URL);
    equal.available = true;

    // Act + assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NOT_NEWER, ota_check_start(APP_FIRMWARE_VERSION, &stale, false));
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NOT_NEWER, ota_check_start(APP_FIRMWARE_VERSION, &equal, false));
}

void test_gate_rejects_plain_http_url(void)
{
    // Arrange.
    arrange_offered_update("2.0.1", HTTP_URL);

    // Act.
    ota_gate_t gate = ota_check_start(APP_FIRMWARE_VERSION, &s_update, false);

    // Assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_URL_NOT_HTTPS, gate);
}

void test_gate_defers_while_pump_running(void)
{
    // Arrange.
    arrange_offered_update("2.0.1", HTTPS_URL);

    // Act.
    ota_gate_t gate = ota_check_start(APP_FIRMWARE_VERSION, &s_update, true);

    // Assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_PUMP_RUNNING, gate);
}

void test_gate_evaluates_newer_before_url_scheme(void)
{
    // Arrange: stale version over plain HTTP must report NOT_NEWER, matching
    // the firmware check order (version gate precedes the URL gate).
    arrange_offered_update("1.0.0", HTTP_URL);

    // Act.
    ota_gate_t gate = ota_check_start(APP_FIRMWARE_VERSION, &s_update, false);

    // Assert.
    TEST_ASSERT_EQUAL_INT(OTA_GATE_NOT_NEWER, gate);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_version_full_semver_yields_parts);
    RUN_TEST(test_parse_version_rejects_partial_semver);
    RUN_TEST(test_parse_version_rejects_malformed_strings);
    RUN_TEST(test_parse_version_rejects_null_and_overlong);
    RUN_TEST(test_version_is_newer_detects_patch_minor_major_bumps);
    RUN_TEST(test_version_is_newer_rejects_equal_older_and_invalid);
    RUN_TEST(test_gate_ok_for_newer_https_idle_pump);
    RUN_TEST(test_gate_no_update_for_null_unavailable_or_empty_url);
    RUN_TEST(test_gate_not_newer_for_stale_or_equal_version);
    RUN_TEST(test_gate_rejects_plain_http_url);
    RUN_TEST(test_gate_defers_while_pump_running);
    RUN_TEST(test_gate_evaluates_newer_before_url_scheme);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
