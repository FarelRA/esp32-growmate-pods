#include <string.h>

#include <unity.h>

#include "actuators.h"
#include "app_build_config.h"
#include "board_profile.h"
#include "fakes/fake_runtime.h"

#define PUMP_DOSE_MS 5000
#define PUMP_DOSE_US ((int64_t) PUMP_DOSE_MS * 1000LL)
#define OVER_CAP_DOSE_MS (APP_MAX_PUMP_DURATION_MS + 1)
#define MAX_DOSE_MS APP_MAX_PUMP_DURATION_MS

static const board_profile_t *s_profile;

void setUp(void)
{
    fake_runtime_reset();
    s_profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);
    actuators_init(s_profile);
}

void tearDown(void)
{
}

static device_commands_t pump_command(int duration_ms)
{
    device_commands_t commands = {0};
    commands.has_pump_command = true;
    commands.pump_duration_ms = duration_ms;
    return commands;
}

static device_commands_t light_command(bool enabled)
{
    device_commands_t commands = {0};
    commands.has_light_command = true;
    commands.light_enabled = enabled;
    return commands;
}

void test_init_drives_pump_and_light_off(void)
{
    // Arrange + act: setUp already initialized.

    // Assert: MOSFET gates rest at the inactive level through boot.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
    TEST_ASSERT_FALSE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->pump_gpio));
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->grow_light_gpio));
}

void test_pump_command_valid_turns_pump_on_and_arms_safety(void)
{
    // Arrange.
    device_commands_t commands = pump_command(PUMP_DOSE_MS);

    // Act.
    actuators_apply_commands(s_profile, &commands);

    // Assert.
    TEST_ASSERT_TRUE(actuators_is_pump_enabled());
    TEST_ASSERT_EQUAL_INT(1, fake_gpio_level(s_profile->pump_gpio));
    TEST_ASSERT_EQUAL_INT(1, fake_timer_armed_count());
}

void test_pump_stays_on_before_deadline_and_off_after(void)
{
    // Arrange.
    device_commands_t commands = pump_command(PUMP_DOSE_MS);
    actuators_apply_commands(s_profile, &commands);

    // Act: just before the deadline the pump must still run...
    fake_timer_set_time(PUMP_DOSE_US - 1);
    actuators_tick(s_profile);

    // Assert.
    TEST_ASSERT_TRUE(actuators_is_pump_enabled());

    // Act: ...and at the deadline the tick path shuts it down.
    fake_timer_set_time(PUMP_DOSE_US);
    actuators_tick(s_profile);

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->pump_gpio));
}

void test_pump_safety_callback_forces_pump_off(void)
{
    // Arrange.
    device_commands_t commands = pump_command(PUMP_DOSE_MS);
    actuators_apply_commands(s_profile, &commands);

    // Act: the esp_timer one-shot fires independently of the tick path.
    fake_timer_fire_all_armed();

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->pump_gpio));
}

void test_pump_command_rejected_when_over_cap_or_non_positive(void)
{
    // Arrange.
    device_commands_t over = pump_command(OVER_CAP_DOSE_MS);
    device_commands_t zero = pump_command(0);
    device_commands_t negative = pump_command(-100);
    unsigned int writes_before = fake_gpio_write_count(s_profile->pump_gpio);

    // Act.
    actuators_apply_commands(s_profile, &over);
    actuators_apply_commands(s_profile, &zero);
    actuators_apply_commands(s_profile, &negative);

    // Assert: the pump never moves and no safety timer is armed.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
    TEST_ASSERT_EQUAL_UINT(writes_before, fake_gpio_write_count(s_profile->pump_gpio));
    TEST_ASSERT_EQUAL_INT(0, fake_timer_armed_count());
}

void test_pump_recommand_refreshes_deadline(void)
{
    // Arrange: first dose starts at t=0 with a 5000 ms window.
    device_commands_t first = pump_command(PUMP_DOSE_MS);
    actuators_apply_commands(s_profile, &first);

    // Act: a second command at t=1000 extends the window from its own start.
    fake_timer_set_time(1000);
    device_commands_t second = pump_command(PUMP_DOSE_MS);
    actuators_apply_commands(s_profile, &second);
    fake_timer_set_time(PUMP_DOSE_US + 999);
    actuators_tick(s_profile);

    // Assert: past the original deadline but inside the refreshed window...
    TEST_ASSERT_TRUE(actuators_is_pump_enabled());

    // Act: ...and off once the refreshed window elapses.
    fake_timer_set_time(PUMP_DOSE_US + 1000);
    actuators_tick(s_profile);

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
}

void test_light_command_on_enables_and_arms_failsafe(void)
{
    // Arrange.
    device_commands_t commands = light_command(true);

    // Act.
    actuators_apply_commands(s_profile, &commands);

    // Assert.
    TEST_ASSERT_TRUE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_INT(1, fake_gpio_level(s_profile->grow_light_gpio));
    TEST_ASSERT_EQUAL_INT(1, fake_timer_armed_count());
}

void test_light_command_off_disables_and_disarms_failsafe(void)
{
    // Arrange.
    device_commands_t on = light_command(true);
    device_commands_t off = light_command(false);
    actuators_apply_commands(s_profile, &on);

    // Act.
    actuators_apply_commands(s_profile, &off);

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->grow_light_gpio));
    TEST_ASSERT_EQUAL_INT(0, fake_timer_armed_count());
}

void test_light_failsafe_callback_forces_light_off(void)
{
    // Arrange.
    device_commands_t on = light_command(true);
    actuators_apply_commands(s_profile, &on);

    // Act: a full day of server silence expires the failsafe window.
    fake_timer_fire_all_armed();

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->grow_light_gpio));
}

void test_light_recommand_same_state_refreshes_without_churn(void)
{
    // Arrange.
    device_commands_t on = light_command(true);
    actuators_apply_commands(s_profile, &on);
    unsigned int writes_after_first = fake_gpio_write_count(s_profile->grow_light_gpio);

    // Act: re-sent "on" only refreshes the failsafe window.
    actuators_apply_commands(s_profile, &on);

    // Assert: no redundant GPIO write, timer still armed.
    TEST_ASSERT_TRUE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_UINT(writes_after_first, fake_gpio_write_count(s_profile->grow_light_gpio));
    TEST_ASSERT_EQUAL_INT(1, fake_timer_armed_count());
}

void test_apply_without_commands_leaves_outputs_untouched(void)
{
    // Arrange.
    device_commands_t idle = {0};
    unsigned int pump_writes = fake_gpio_write_count(s_profile->pump_gpio);
    unsigned int light_writes = fake_gpio_write_count(s_profile->grow_light_gpio);

    // Act.
    actuators_apply_commands(s_profile, &idle);

    // Assert.
    TEST_ASSERT_EQUAL_UINT(pump_writes, fake_gpio_write_count(s_profile->pump_gpio));
    TEST_ASSERT_EQUAL_UINT(light_writes, fake_gpio_write_count(s_profile->grow_light_gpio));
    TEST_ASSERT_EQUAL_INT(0, fake_timer_armed_count());
}

void test_reinit_stops_timers_and_restores_safe_state(void)
{
    // Arrange: a running pump, then a fresh init (e.g. re-provisioning).
    device_commands_t commands = pump_command(MAX_DOSE_MS);
    actuators_apply_commands(s_profile, &commands);

    // Act.
    actuators_init(s_profile);

    // Assert.
    TEST_ASSERT_FALSE(actuators_is_pump_enabled());
    TEST_ASSERT_FALSE(actuators_is_light_enabled());
    TEST_ASSERT_EQUAL_INT(0, fake_timer_armed_count());
    TEST_ASSERT_EQUAL_INT(0, fake_gpio_level(s_profile->pump_gpio));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_drives_pump_and_light_off);
    RUN_TEST(test_pump_command_valid_turns_pump_on_and_arms_safety);
    RUN_TEST(test_pump_stays_on_before_deadline_and_off_after);
    RUN_TEST(test_pump_safety_callback_forces_pump_off);
    RUN_TEST(test_pump_command_rejected_when_over_cap_or_non_positive);
    RUN_TEST(test_pump_recommand_refreshes_deadline);
    RUN_TEST(test_light_command_on_enables_and_arms_failsafe);
    RUN_TEST(test_light_command_off_disables_and_disarms_failsafe);
    RUN_TEST(test_light_failsafe_callback_forces_light_off);
    RUN_TEST(test_light_recommand_same_state_refreshes_without_churn);
    RUN_TEST(test_apply_without_commands_leaves_outputs_untouched);
    RUN_TEST(test_reinit_stops_timers_and_restores_safe_state);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
