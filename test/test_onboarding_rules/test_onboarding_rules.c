#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "cJSON.h"
#include "onboarding_rules.h"

#define SSID_SLOT 33
#define PASSWORD_SLOT 65

void setUp(void)
{
}

void tearDown(void)
{
}

static cJSON *parse_or_fail(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL_MESSAGE(root, json);
    return root;
}

void test_copy_field_copies_present_string(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"wifiSsid\":\"HomeNet\"}");
    char ssid[SSID_SLOT] = {0};

    // Act.
    bool ok = onboarding_copy_field(root, "wifiSsid", ssid, sizeof(ssid));

    // Assert.
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("HomeNet", ssid);
    cJSON_Delete(root);
}

void test_copy_field_keeps_stale_value_when_key_absent(void)
{
    // Arrange: absent key is "no change", not an error.
    cJSON *root = parse_or_fail("{}");
    char ssid[SSID_SLOT] = {0};
    snprintf(ssid, sizeof(ssid), "OldNet");

    // Act.
    bool ok = onboarding_copy_field(root, "wifiSsid", ssid, sizeof(ssid));

    // Assert.
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("OldNet", ssid);
    cJSON_Delete(root);
}

void test_copy_field_rejects_present_non_string(void)
{
    // Arrange: explicit null / number must fail, not silently keep stale creds.
    cJSON *null_value = parse_or_fail("{\"wifiSsid\":null}");
    cJSON *number_value = parse_or_fail("{\"wifiSsid\":42}");
    char ssid[SSID_SLOT] = {0};

    // Act + assert.
    TEST_ASSERT_FALSE(onboarding_copy_field(null_value, "wifiSsid", ssid, sizeof(ssid)));
    TEST_ASSERT_FALSE(onboarding_copy_field(number_value, "wifiSsid", ssid, sizeof(ssid)));
    cJSON_Delete(null_value);
    cJSON_Delete(number_value);
}

void test_copy_field_rejects_overlong_value(void)
{
    // Arrange: 33 chars cannot fit the 32-byte SSID slot beside the NUL.
    char long_ssid[SSID_SLOT + 1];
    memset(long_ssid, 'x', sizeof(long_ssid) - 1);
    long_ssid[sizeof(long_ssid) - 1] = '\0';
    char body[128];
    snprintf(body, sizeof(body), "{\"wifiSsid\":\"%s\"}", long_ssid);
    cJSON *root = parse_or_fail(body);
    char ssid[SSID_SLOT] = {0};

    // Act.
    bool ok = onboarding_copy_field(root, "wifiSsid", ssid, sizeof(ssid));

    // Assert: rejected instead of truncated into unauthenticatable creds.
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_STRING("", ssid);
    cJSON_Delete(root);
}

void test_copy_field_accepts_slot_filling_value(void)
{
    // Arrange: 32 chars exactly fill the 33-byte destination with its NUL.
    char full[SSID_SLOT];
    memset(full, 'x', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
    char body[128];
    snprintf(body, sizeof(body), "{\"wifiPassword\":\"%s\"}", full);
    cJSON *root = parse_or_fail(body);
    char password[PASSWORD_SLOT] = {0};

    // Act.
    bool ok = onboarding_copy_field(root, "wifiPassword", password, sizeof(password));

    // Assert.
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING(full, password);
    cJSON_Delete(root);
}

void test_ssid_valid_enforces_joinable_bounds(void)
{
    // Arrange: 31 chars is the longest SSID with room for the driver NUL.
    char longest[32];
    char full[33];
    memset(longest, 'x', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    memset(full, 'x', sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';

    // Act + assert.
    TEST_ASSERT_TRUE(onboarding_ssid_valid("A"));
    TEST_ASSERT_TRUE(onboarding_ssid_valid(longest));
    TEST_ASSERT_FALSE(onboarding_ssid_valid(""));
    TEST_ASSERT_FALSE(onboarding_ssid_valid(NULL));
    TEST_ASSERT_FALSE(onboarding_ssid_valid(full));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_copy_field_copies_present_string);
    RUN_TEST(test_copy_field_keeps_stale_value_when_key_absent);
    RUN_TEST(test_copy_field_rejects_present_non_string);
    RUN_TEST(test_copy_field_rejects_overlong_value);
    RUN_TEST(test_copy_field_accepts_slot_filling_value);
    RUN_TEST(test_ssid_valid_enforces_joinable_bounds);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
