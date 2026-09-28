#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "app_build_config.h"
#include "app_config.h"
#include "esp_err.h"
#include "fakes/fake_runtime.h"

void setUp(void)
{
    fake_runtime_reset();
}

void tearDown(void)
{
}

void test_set_defaults_initializes_version_and_identity(void)
{
    // Arrange.
    app_config_t config;
    memset(&config, 0xA5, sizeof(config));

    // Act.
    app_config_set_defaults(&config);

    // Assert.
    TEST_ASSERT_EQUAL_UINT16(APP_CONFIG_VERSION, config.version);
    TEST_ASSERT_FALSE(config.provisioned);
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, config.device_id);
    TEST_ASSERT_EQUAL_STRING("", config.wifi_ssid);
    TEST_ASSERT_EQUAL_UINT32(0, config.boot_count);
}

void test_sanitize_trims_ssid_and_device_id(void)
{
    // Arrange.
    app_config_t config = {0};
    snprintf(config.wifi_ssid, sizeof(config.wifi_ssid), "  HomeNet  ");
    snprintf(config.device_id, sizeof(config.device_id), "\tPOD-1\n");

    // Act.
    app_config_sanitize(&config);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("HomeNet", config.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("POD-1", config.device_id);
}

void test_sanitize_preserves_password_whitespace(void)
{
    // Arrange: leading/trailing spaces are legal in WPA2 passphrases.
    app_config_t config = {0};
    snprintf(config.wifi_password, sizeof(config.wifi_password), "  s3cret  ");

    // Act.
    app_config_sanitize(&config);

    // Assert.
    TEST_ASSERT_EQUAL_STRING("  s3cret  ", config.wifi_password);
}

void test_sanitize_restores_default_device_id_and_version(void)
{
    // Arrange.
    app_config_t config = {0};
    config.version = 0;

    // Act.
    app_config_sanitize(&config);

    // Assert.
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, config.device_id);
    TEST_ASSERT_EQUAL_UINT16(APP_CONFIG_VERSION, config.version);
}

void test_sanitize_terminates_overlong_buffers(void)
{
    // Arrange: hostile buffers with no NUL in range.
    app_config_t config;
    memset(&config, 'A', sizeof(config));

    // Act: must not read or write out of bounds.
    app_config_sanitize(&config);

    // Assert.
    TEST_ASSERT_EQUAL_CHAR('\0', config.wifi_ssid[APP_CONFIG_MAX_WIFI_SSID_LEN]);
    TEST_ASSERT_EQUAL_CHAR('\0', config.wifi_password[APP_CONFIG_MAX_WIFI_PASSWORD_LEN]);
    TEST_ASSERT_EQUAL_CHAR('\0', config.device_id[APP_CONFIG_MAX_DEVICE_ID_LEN]);
}

void test_is_complete_requires_provisioned_ssid(void)
{
    // Arrange.
    app_config_t unprovisioned = {.provisioned = false, .wifi_ssid = "HomeNet"};
    app_config_t no_ssid = {.provisioned = true, .wifi_ssid = ""};
    app_config_t complete = {.provisioned = true, .wifi_ssid = "HomeNet"};

    // Act + assert.
    TEST_ASSERT_FALSE(app_config_is_complete(&unprovisioned));
    TEST_ASSERT_FALSE(app_config_is_complete(&no_ssid));
    TEST_ASSERT_TRUE(app_config_is_complete(&complete));
}

void test_load_returns_defaults_when_namespace_missing(void)
{
    // Arrange: empty flash, nothing stored.
    app_config_t config;
    memset(&config, 0xA5, sizeof(config));

    // Act.
    esp_err_t err = app_config_load(&config);

    // Assert: defaults installed so provisioning + claim can start clean.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_EQUAL_UINT16(APP_CONFIG_VERSION, config.version);
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, config.device_id);
}

void test_save_then_load_roundtrips_config(void)
{
    // Arrange.
    app_config_t saved = {0};
    app_config_set_defaults(&saved);
    saved.provisioned = true;
    snprintf(saved.wifi_ssid, sizeof(saved.wifi_ssid), "HomeNet");
    saved.boot_count = 3;
    saved.applied_config_rev = 8;

    // Act.
    TEST_ASSERT_EQUAL_INT(ESP_OK, app_config_save(&saved));
    app_config_t loaded = {0};
    esp_err_t err = app_config_load(&loaded);

    // Assert.
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_TRUE(loaded.provisioned);
    TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.wifi_ssid);
    TEST_ASSERT_EQUAL_UINT32(3, loaded.boot_count);
    TEST_ASSERT_EQUAL_UINT32(8, loaded.applied_config_rev);
}

void test_load_resets_to_defaults_on_size_mismatch(void)
{
    // Arrange: a stale-layout blob from a pre-alpha build.
    const char short_blob[4] = {'a', 'b', 'c', 'd'};
    TEST_ASSERT_EQUAL_INT(ESP_OK,
                          fake_nvs_inject_blob(APP_CONFIG_NAMESPACE, APP_CONFIG_STORAGE_KEY,
                                               short_blob, sizeof(short_blob)));
    app_config_t config;
    memset(&config, 0xA5, sizeof(config));

    // Act.
    esp_err_t err = app_config_load(&config);

    // Assert: corrupt layout forces fresh provisioning + claim.
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT16(APP_CONFIG_VERSION, config.version);
    TEST_ASSERT_EQUAL_STRING(APP_DEVICE_ID, config.device_id);
}

void test_load_resets_to_defaults_on_version_mismatch(void)
{
    // Arrange: a full-size blob stamped with a foreign version.
    app_config_t foreign = {0};
    app_config_set_defaults(&foreign);
    foreign.version = APP_CONFIG_VERSION + 1;
    TEST_ASSERT_EQUAL_INT(ESP_OK,
                          fake_nvs_inject_blob(APP_CONFIG_NAMESPACE, APP_CONFIG_STORAGE_KEY,
                                               &foreign, sizeof(foreign)));
    app_config_t config;
    memset(&config, 0xA5, sizeof(config));

    // Act.
    esp_err_t err = app_config_load(&config);

    // Assert.
    TEST_ASSERT_EQUAL_INT(ESP_ERR_INVALID_VERSION, err);
    TEST_ASSERT_EQUAL_UINT16(APP_CONFIG_VERSION, config.version);
}

void test_load_propagates_open_failure(void)
{
    // Arrange: flash unreadable.
    fake_nvs_set_fail_mode(true, false, false, false);
    app_config_t config = {0};

    // Act.
    esp_err_t err = app_config_load(&config);

    // Assert.
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
}

void test_save_propagates_write_failure(void)
{
    // Arrange: flash read-only.
    app_config_t config = {0};
    app_config_set_defaults(&config);
    fake_nvs_set_fail_mode(false, false, true, false);

    // Act.
    esp_err_t err = app_config_save(&config);

    // Assert.
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_UINT(0, fake_nvs_entry_count());
}

void test_save_propagates_open_failure(void)
{
    // Arrange: flash unavailable before any write.
    app_config_t config = {0};
    app_config_set_defaults(&config);
    fake_nvs_set_fail_mode(true, false, false, false);

    // Act.
    esp_err_t err = app_config_save(&config);

    // Assert.
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_defaults_initializes_version_and_identity);
    RUN_TEST(test_sanitize_trims_ssid_and_device_id);
    RUN_TEST(test_sanitize_preserves_password_whitespace);
    RUN_TEST(test_sanitize_restores_default_device_id_and_version);
    RUN_TEST(test_sanitize_terminates_overlong_buffers);
    RUN_TEST(test_is_complete_requires_provisioned_ssid);
    RUN_TEST(test_load_returns_defaults_when_namespace_missing);
    RUN_TEST(test_save_then_load_roundtrips_config);
    RUN_TEST(test_load_resets_to_defaults_on_size_mismatch);
    RUN_TEST(test_load_resets_to_defaults_on_version_mismatch);
    RUN_TEST(test_load_propagates_open_failure);
    RUN_TEST(test_save_propagates_write_failure);
    RUN_TEST(test_save_propagates_open_failure);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
