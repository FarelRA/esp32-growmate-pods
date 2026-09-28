#include <stdio.h>
#include <string.h>

#include <unity.h>

#include "api_parse.h"
#include "app_build_config.h"
#include "cJSON.h"

#define ACCEPTED_ID_SLOT 32
#define MAX_COLLECTED_IDS 16
#define PUMP_DOSE_MS 5000
#define APPLIED_CONFIG_REV 7u
#define CURRENT_INTERVAL_SEC 15u

typedef struct {
    char ids[MAX_COLLECTED_IDS][ACCEPTED_ID_SLOT];
    int count;
} accepted_collector_t;

static accepted_collector_t s_collected;

static void collect_id(const char *id, void *ctx)
{
    accepted_collector_t *collector = (accepted_collector_t *) ctx;
    if (collector->count < MAX_COLLECTED_IDS) {
        snprintf(collector->ids[collector->count], ACCEPTED_ID_SLOT, "%s", id);
        collector->count++;
    }
}

void setUp(void)
{
    memset(&s_collected, 0, sizeof(s_collected));
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

// --- pump commands ---

void test_pump_command_valid_sets_flag_duration_and_id(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"pump\",\"durationMs\":5000,\"id\":\"cmd-1\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_TRUE(commands.has_pump_command);
    TEST_ASSERT_EQUAL_INT(PUMP_DOSE_MS, commands.pump_duration_ms);
    TEST_ASSERT_EQUAL_STRING("cmd-1", commands.pump_id);
    TEST_ASSERT_EQUAL_INT(1, s_collected.count);
    TEST_ASSERT_EQUAL_STRING("cmd-1", s_collected.ids[0]);
    cJSON_Delete(root);
}

void test_pump_command_rejected_when_dose_over_cap(void)
{
    // Arrange: 30001 ms exceeds APP_MAX_PUMP_DURATION_MS (30000).
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"pump\",\"durationMs\":30001,\"id\":\"cmd-1\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_pump_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

void test_pump_command_rejected_when_dose_missing_or_not_number(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"pump\",\"id\":\"cmd-1\"},"
                                "{\"kind\":\"pump\",\"durationMs\":\"long\",\"id\":\"cmd-2\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_pump_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

void test_pump_command_rejected_when_tank_level_unavailable(void)
{
    // Arrange: dosing blind would risk a dry-run, so the gate must hold.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"pump\",\"durationMs\":5000,\"id\":\"cmd-1\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, false, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_pump_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

void test_pump_command_rejected_when_id_missing_or_overlong(void)
{
    // Arrange: id-less commands are unackable; 32-char ids overflow the slot.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"pump\",\"durationMs\":5000},"
                                "{\"kind\":\"pump\",\"durationMs\":5000,\"id\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_pump_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

// --- light commands ---

void test_light_command_bool_true_enables(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"light\",\"enabled\":true,\"id\":\"lit-1\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_TRUE(commands.has_light_command);
    TEST_ASSERT_TRUE(commands.light_enabled);
    TEST_ASSERT_EQUAL_STRING("lit-1", commands.light_id);
    cJSON_Delete(root);
}

void test_light_command_bool_false_disables(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"light\",\"enabled\":false,\"id\":\"lit-2\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_TRUE(commands.has_light_command);
    TEST_ASSERT_FALSE(commands.light_enabled);
    cJSON_Delete(root);
}

void test_light_command_number_coerces_to_bool(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"light\",\"enabled\":1,\"id\":\"lit-3\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_TRUE(commands.has_light_command);
    TEST_ASSERT_TRUE(commands.light_enabled);
    cJSON_Delete(root);
}

void test_light_command_rejected_when_value_uncoercible(void)
{
    // Arrange: a string "enabled" is neither bool nor number.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"light\",\"enabled\":\"yes\",\"id\":\"lit-4\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_light_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

void test_light_command_rejected_when_id_missing(void)
{
    // Arrange: id-less light commands are unackable, like pump commands.
    cJSON *root = parse_or_fail("{\"commands\":["
                                "{\"kind\":\"light\",\"enabled\":true}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(root, true, &commands, collect_id, &s_collected);

    // Assert.
    TEST_ASSERT_FALSE(commands.has_light_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(root);
}

// --- command array shape ---

void test_commands_ignored_when_array_missing_or_malformed(void)
{
    // Arrange.
    cJSON *missing = parse_or_fail("{}");
    cJSON *not_array = parse_or_fail("{\"commands\":{}}");
    cJSON *no_kind = parse_or_fail("{\"commands\":[{\"id\":\"x\"}]}");
    cJSON *unknown = parse_or_fail("{\"commands\":[{\"kind\":\"fan\",\"id\":\"x\"}]}");
    device_commands_t commands = {0};

    // Act.
    api_parse_commands(missing, true, &commands, collect_id, &s_collected);
    api_parse_commands(not_array, true, &commands, collect_id, &s_collected);
    api_parse_commands(no_kind, true, &commands, collect_id, &s_collected);
    api_parse_commands(unknown, true, &commands, collect_id, &s_collected);

    // Assert: nothing applied, nothing acked.
    TEST_ASSERT_FALSE(commands.has_pump_command);
    TEST_ASSERT_FALSE(commands.has_light_command);
    TEST_ASSERT_EQUAL_INT(0, s_collected.count);
    cJSON_Delete(missing);
    cJSON_Delete(not_array);
    cJSON_Delete(no_kind);
    cJSON_Delete(unknown);
}

// --- config push ---

void test_config_push_newer_rev_yields_rev_and_interval(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"config\":{\"rev\":8,\"reportIntervalSec\":300}}");
    uint32_t out_rev = 0;
    uint32_t out_interval = 0;

    // Act.
    bool applied = api_parse_config_push(root, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                         &out_rev, &out_interval);

    // Assert.
    TEST_ASSERT_TRUE(applied);
    TEST_ASSERT_EQUAL_UINT32(8, out_rev);
    TEST_ASSERT_EQUAL_UINT32(300, out_interval);
    cJSON_Delete(root);
}

void test_config_push_rejects_stale_duplicate_and_bad_rev(void)
{
    // Arrange.
    cJSON *stale = parse_or_fail("{\"config\":{\"rev\":6}}");
    cJSON *duplicate = parse_or_fail("{\"config\":{\"rev\":7}}");
    cJSON *zero = parse_or_fail("{\"config\":{\"rev\":0}}");
    cJSON *missing = parse_or_fail("{}");
    uint32_t out_rev = 0;
    uint32_t out_interval = 0;

    // Act + assert.
    TEST_ASSERT_FALSE(api_parse_config_push(stale, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                            &out_rev, &out_interval));
    TEST_ASSERT_FALSE(api_parse_config_push(duplicate, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                            &out_rev, &out_interval));
    TEST_ASSERT_FALSE(api_parse_config_push(zero, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                            &out_rev, &out_interval));
    TEST_ASSERT_FALSE(api_parse_config_push(missing, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                            &out_rev, &out_interval));
    cJSON_Delete(stale);
    cJSON_Delete(duplicate);
    cJSON_Delete(zero);
    cJSON_Delete(missing);
}

void test_config_push_clamps_interval_and_keeps_current_when_absent(void)
{
    // Arrange.
    cJSON *low = parse_or_fail("{\"config\":{\"rev\":8,\"reportIntervalSec\":1}}");
    cJSON *high = parse_or_fail("{\"config\":{\"rev\":8,\"reportIntervalSec\":99999}}");
    cJSON *absent = parse_or_fail("{\"config\":{\"rev\":8}}");
    uint32_t out_rev = 0;
    uint32_t out_interval = 0;

    // Act + assert: clamps to [10, 3600], absent keeps the current value.
    TEST_ASSERT_TRUE(api_parse_config_push(low, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                           &out_rev, &out_interval));
    TEST_ASSERT_EQUAL_UINT32(10, out_interval);
    TEST_ASSERT_TRUE(api_parse_config_push(high, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                           &out_rev, &out_interval));
    TEST_ASSERT_EQUAL_UINT32(3600, out_interval);
    TEST_ASSERT_TRUE(api_parse_config_push(absent, APPLIED_CONFIG_REV, CURRENT_INTERVAL_SEC,
                                           &out_rev, &out_interval));
    TEST_ASSERT_EQUAL_UINT32(CURRENT_INTERVAL_SEC, out_interval);
    cJSON_Delete(low);
    cJSON_Delete(high);
    cJSON_Delete(absent);
}

// --- claim ---

void test_claim_new_valid_id_yields_device_id(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"claim\":{\"deviceId\":\"POD-9_abc\"}}");
    char out[APP_CONFIG_MAX_DEVICE_ID_LEN + 1] = {0};

    // Act.
    bool claimed = api_parse_claim(root, "IAET01", out);

    // Assert.
    TEST_ASSERT_TRUE(claimed);
    TEST_ASSERT_EQUAL_STRING("POD-9_abc", out);
    cJSON_Delete(root);
}

void test_claim_ignores_same_id_bad_charset_and_missing(void)
{
    // Arrange.
    cJSON *same = parse_or_fail("{\"claim\":{\"deviceId\":\"IAET01\"}}");
    cJSON *bad = parse_or_fail("{\"claim\":{\"deviceId\":\"bad id!\"}}");
    cJSON *empty = parse_or_fail("{\"claim\":{\"deviceId\":\"\"}}");
    cJSON *missing = parse_or_fail("{}");
    char out[APP_CONFIG_MAX_DEVICE_ID_LEN + 1] = {0};

    // Act + assert.
    TEST_ASSERT_FALSE(api_parse_claim(same, "IAET01", out));
    TEST_ASSERT_FALSE(api_parse_claim(bad, "IAET01", out));
    TEST_ASSERT_FALSE(api_parse_claim(empty, "IAET01", out));
    TEST_ASSERT_FALSE(api_parse_claim(missing, "IAET01", out));
    cJSON_Delete(same);
    cJSON_Delete(bad);
    cJSON_Delete(empty);
    cJSON_Delete(missing);
}

// --- firmware offer ---

void test_firmware_offer_sets_available_version_and_url(void)
{
    // Arrange.
    cJSON *root = parse_or_fail("{\"minFirmware\":\"2.1.0\","
                                "\"firmwareUrl\":\"https://example.invalid/f.bin\"}");
    ota_update_t ota = {0};

    // Act.
    bool offered = api_parse_firmware_offer(root, &ota);

    // Assert.
    TEST_ASSERT_TRUE(offered);
    TEST_ASSERT_TRUE(ota.available);
    TEST_ASSERT_EQUAL_STRING("2.1.0", ota.version);
    TEST_ASSERT_EQUAL_STRING("https://example.invalid/f.bin", ota.url);
    cJSON_Delete(root);
}

void test_firmware_offer_rejects_missing_overlong_and_null(void)
{
    // Arrange: 32-char version and 256-char URL no longer fit their slots.
    char long_version[33];
    char long_url[257];
    memset(long_version, 'v', sizeof(long_version) - 1);
    long_version[sizeof(long_version) - 1] = '\0';
    memset(long_url, 'u', sizeof(long_url) - 1);
    long_url[sizeof(long_url) - 1] = '\0';
    char long_version_json[128];
    char long_url_json[320];
    snprintf(long_version_json, sizeof(long_version_json),
             "{\"minFirmware\":\"%s\",\"firmwareUrl\":\"https://e.invalid/f\"}", long_version);
    snprintf(long_url_json, sizeof(long_url_json),
             "{\"minFirmware\":\"2.1.0\",\"firmwareUrl\":\"%s\"}", long_url);
    cJSON *missing = parse_or_fail("{}");
    cJSON *long_ver = parse_or_fail(long_version_json);
    cJSON *long_u = parse_or_fail(long_url_json);
    ota_update_t ota = {0};

    // Act + assert: rejected, never truncated into a corrupt offer.
    TEST_ASSERT_FALSE(api_parse_firmware_offer(missing, &ota));
    TEST_ASSERT_FALSE(api_parse_firmware_offer(long_ver, &ota));
    TEST_ASSERT_FALSE(api_parse_firmware_offer(long_u, &ota));
    TEST_ASSERT_FALSE(api_parse_firmware_offer(missing, NULL));
    TEST_ASSERT_FALSE(ota.available);
    cJSON_Delete(missing);
    cJSON_Delete(long_ver);
    cJSON_Delete(long_u);
}

void test_accepted_ring_collects_ids_in_server_order(void)
{
    // Arrange.
    api_accepted_ring_t ring;
    api_accepted_ring_init(&ring);

    // Act.
    api_accepted_ring_push(&ring, "cmd-1");
    api_accepted_ring_push(&ring, "cmd-2");

    // Assert.
    TEST_ASSERT_EQUAL_UINT(2, api_accepted_ring_count(&ring));
    TEST_ASSERT_EQUAL_STRING("cmd-1", api_accepted_ring_at(&ring, 0));
    TEST_ASSERT_EQUAL_STRING("cmd-2", api_accepted_ring_at(&ring, 1));
}

void test_accepted_ring_ignores_null_and_empty_ids(void)
{
    // Arrange.
    api_accepted_ring_t ring;
    api_accepted_ring_init(&ring);

    // Act.
    api_accepted_ring_push(&ring, NULL);
    api_accepted_ring_push(&ring, "");
    api_accepted_ring_push(&ring, "cmd-1");

    // Assert: only the real id lands in the ring.
    TEST_ASSERT_EQUAL_UINT(1, api_accepted_ring_count(&ring));
    TEST_ASSERT_EQUAL_STRING("cmd-1", api_accepted_ring_at(&ring, 0));
}

void test_accepted_ring_evicts_oldest_when_full(void)
{
    // Arrange: a 16-slot ring fed 17 ids.
    api_accepted_ring_t ring;
    api_accepted_ring_init(&ring);
    char id[16];

    // Act.
    for (int i = 0; i < API_ACCEPTED_ID_RING_SIZE + 1; ++i) {
        snprintf(id, sizeof(id), "id-%02d", i);
        api_accepted_ring_push(&ring, id);
    }

    // Assert: oldest evicted, survivors keep server order.
    TEST_ASSERT_EQUAL_UINT(API_ACCEPTED_ID_RING_SIZE, api_accepted_ring_count(&ring));
    TEST_ASSERT_EQUAL_STRING("id-01", api_accepted_ring_at(&ring, 0));
    TEST_ASSERT_EQUAL_STRING("id-16", api_accepted_ring_at(&ring, API_ACCEPTED_ID_RING_SIZE - 1));
    TEST_ASSERT_NULL(api_accepted_ring_at(&ring, API_ACCEPTED_ID_RING_SIZE));
}

void test_accepted_ring_clear_resets_to_empty(void)
{
    // Arrange.
    api_accepted_ring_t ring;
    api_accepted_ring_init(&ring);
    api_accepted_ring_push(&ring, "cmd-1");

    // Act.
    api_accepted_ring_clear(&ring);

    // Assert.
    TEST_ASSERT_EQUAL_UINT(0, api_accepted_ring_count(&ring));
    TEST_ASSERT_NULL(api_accepted_ring_at(&ring, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pump_command_valid_sets_flag_duration_and_id);
    RUN_TEST(test_pump_command_rejected_when_dose_over_cap);
    RUN_TEST(test_pump_command_rejected_when_dose_missing_or_not_number);
    RUN_TEST(test_pump_command_rejected_when_tank_level_unavailable);
    RUN_TEST(test_pump_command_rejected_when_id_missing_or_overlong);
    RUN_TEST(test_light_command_bool_true_enables);
    RUN_TEST(test_light_command_bool_false_disables);
    RUN_TEST(test_light_command_number_coerces_to_bool);
    RUN_TEST(test_light_command_rejected_when_value_uncoercible);
    RUN_TEST(test_light_command_rejected_when_id_missing);
    RUN_TEST(test_commands_ignored_when_array_missing_or_malformed);
    RUN_TEST(test_config_push_newer_rev_yields_rev_and_interval);
    RUN_TEST(test_config_push_rejects_stale_duplicate_and_bad_rev);
    RUN_TEST(test_config_push_clamps_interval_and_keeps_current_when_absent);
    RUN_TEST(test_claim_new_valid_id_yields_device_id);
    RUN_TEST(test_claim_ignores_same_id_bad_charset_and_missing);
    RUN_TEST(test_firmware_offer_sets_available_version_and_url);
    RUN_TEST(test_firmware_offer_rejects_missing_overlong_and_null);
    RUN_TEST(test_accepted_ring_collects_ids_in_server_order);
    RUN_TEST(test_accepted_ring_ignores_null_and_empty_ids);
    RUN_TEST(test_accepted_ring_evicts_oldest_when_full);
    RUN_TEST(test_accepted_ring_clear_resets_to_empty);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
