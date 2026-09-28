#include <math.h>
#include <stdint.h>

#include <unity.h>

#include "board_profile.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "fakes/fake_runtime.h"
#include "sensors.h"

#define ADC_RAW_SOIL 2000
#define ADC_RAW_LIGHT 3000
#define ADC_RAW_WATER 1500
#define DHT_TEMPERATURE_C 24.5f
#define DHT_HUMIDITY_PCT 60.0f
#define ADC_SAMPLE_COUNT_FIRMWARE 8

static const board_profile_t *s_profile;

void setUp(void)
{
    fake_runtime_reset();
    s_profile = board_profile_get(BOARD_PROFILE_AI_THINKER_ESP32_CAM);
    sensors_init(s_profile);
}

void tearDown(void)
{
}

static void arrange_all_channels_ok(void)
{
    const int soil[1] = {ADC_RAW_SOIL};
    const int light[1] = {ADC_RAW_LIGHT};
    const int water[1] = {ADC_RAW_WATER};
    fake_adc_set_samples(ADC_CHANNEL_4, soil, 1);
    fake_adc_set_samples(ADC_CHANNEL_6, light, 1);
    fake_adc_set_samples(ADC_CHANNEL_5, water, 1);
    fake_dht_set_ok(DHT_TEMPERATURE_C, DHT_HUMIDITY_PCT);
}

void test_read_all_reports_all_measurements_when_healthy(void)
{
    // Arrange.
    arrange_all_channels_ok();
    sensor_snapshot_t snapshot = {0};

    // Act.
    esp_err_t err = sensors_read_all(s_profile, &snapshot);

    // Assert.
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_TRUE(snapshot.soil.available);
    TEST_ASSERT_EQUAL_INT(ADC_RAW_SOIL, snapshot.soil.raw);
    TEST_ASSERT_TRUE(snapshot.light.available);
    TEST_ASSERT_EQUAL_INT(ADC_RAW_LIGHT, snapshot.light.raw);
    TEST_ASSERT_TRUE(snapshot.water.available);
    TEST_ASSERT_EQUAL_INT(ADC_RAW_WATER, snapshot.water.raw);
    TEST_ASSERT_TRUE(snapshot.temperature.available);
    TEST_ASSERT_EQUAL_FLOAT(DHT_TEMPERATURE_C, snapshot.temperature.value);
    TEST_ASSERT_TRUE(snapshot.air.available);
    TEST_ASSERT_EQUAL_FLOAT(DHT_HUMIDITY_PCT, snapshot.air.value);
}

void test_read_all_averages_adc_samples(void)
{
    // Arrange: eight distinct samples average to 3500.
    const int ramp[ADC_SAMPLE_COUNT_FIRMWARE] = {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
    const int flat[1] = {ADC_RAW_LIGHT};
    const int water[1] = {ADC_RAW_WATER};
    fake_adc_set_samples(ADC_CHANNEL_4, ramp, ADC_SAMPLE_COUNT_FIRMWARE);
    fake_adc_set_samples(ADC_CHANNEL_6, flat, 1);
    fake_adc_set_samples(ADC_CHANNEL_5, water, 1);
    fake_dht_set_ok(DHT_TEMPERATURE_C, DHT_HUMIDITY_PCT);
    sensor_snapshot_t snapshot = {0};

    // Act.
    esp_err_t err = sensors_read_all(s_profile, &snapshot);

    // Assert.
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_EQUAL_INT(3500, snapshot.soil.raw);
    TEST_ASSERT_EQUAL_INT(ADC_SAMPLE_COUNT_FIRMWARE, fake_adc_read_count(ADC_CHANNEL_4));
}

void test_read_all_tolerates_single_channel_failure(void)
{
    // Arrange: soil ADC dead, everything else healthy.
    const int light[1] = {ADC_RAW_LIGHT};
    const int water[1] = {ADC_RAW_WATER};
    fake_adc_set_always_fail(ADC_CHANNEL_4, true);
    fake_adc_set_samples(ADC_CHANNEL_6, light, 1);
    fake_adc_set_samples(ADC_CHANNEL_5, water, 1);
    fake_dht_set_ok(DHT_TEMPERATURE_C, DHT_HUMIDITY_PCT);
    sensor_snapshot_t snapshot = {0};
    uint32_t dht_before = 0;
    uint32_t adc_before = 0;
    sensors_get_fail_counts(&dht_before, &adc_before);

    // Act.
    esp_err_t err = sensors_read_all(s_profile, &snapshot);

    // Assert: one gap degrades but does not fail the cycle.
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_FALSE(snapshot.soil.available);
    TEST_ASSERT_EQUAL_INT(-1, snapshot.soil.raw);
    TEST_ASSERT_TRUE(snapshot.water.available);
    uint32_t dht_fails = 0;
    uint32_t adc_fails = 0;
    sensors_get_fail_counts(&dht_fails, &adc_fails);
    TEST_ASSERT_EQUAL_UINT32(dht_before, dht_fails);
    TEST_ASSERT_TRUE(adc_fails > adc_before);
}

void test_read_all_marks_dht_unavailable_after_retries(void)
{
    // Arrange: DHT dead, ADC healthy.
    arrange_all_channels_ok();
    fake_dht_set_fail(true);
    sensor_snapshot_t snapshot = {0};
    uint32_t dht_before = 0;
    sensors_get_fail_counts(&dht_before, NULL);

    // Act.
    esp_err_t err = sensors_read_all(s_profile, &snapshot);

    // Assert: ADC-only cycle still succeeds; exactly one DHT gap is counted.
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);
    TEST_ASSERT_FALSE(snapshot.temperature.available);
    TEST_ASSERT_FALSE(snapshot.air.available);
    TEST_ASSERT_TRUE(isnan((double) snapshot.temperature.value));
    TEST_ASSERT_EQUAL_INT(2, fake_dht_read_count());
    uint32_t dht_after = 0;
    sensors_get_fail_counts(&dht_after, NULL);
    TEST_ASSERT_EQUAL_UINT32(dht_before + 1, dht_after);
}

void test_read_all_fails_when_every_sensor_unavailable(void)
{
    // Arrange: total sensing outage.
    fake_adc_set_always_fail(ADC_CHANNEL_4, true);
    fake_adc_set_always_fail(ADC_CHANNEL_5, true);
    fake_adc_set_always_fail(ADC_CHANNEL_6, true);
    fake_dht_set_fail(true);
    sensor_snapshot_t snapshot = {0};

    // Act.
    esp_err_t err = sensors_read_all(s_profile, &snapshot);

    // Assert.
    TEST_ASSERT_EQUAL_INT(ESP_FAIL, err);
    TEST_ASSERT_FALSE(snapshot.soil.available);
    TEST_ASSERT_FALSE(snapshot.water.available);
    TEST_ASSERT_FALSE(snapshot.temperature.available);
}

void test_fail_counts_report_independently_and_tolerate_null(void)
{
    // Arrange: one failed DHT cycle on top of whatever earlier tests counted
    // (firmware fail counters are monotonic by design, so assert the delta).
    arrange_all_channels_ok();
    fake_dht_set_fail(true);
    sensor_snapshot_t snapshot = {0};
    uint32_t dht_before = 0;
    uint32_t adc_before = 0;
    sensors_get_fail_counts(&dht_before, &adc_before);
    sensors_read_all(s_profile, &snapshot);

    // Act.
    uint32_t dht_fails = 0;
    uint32_t adc_fails = 0;
    sensors_get_fail_counts(&dht_fails, &adc_fails);

    // Assert: NULL sinks never crash; counters stay independent.
    sensors_get_fail_counts(NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(dht_before + 1, dht_fails);
    TEST_ASSERT_EQUAL_UINT32(adc_before, adc_fails);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_all_reports_all_measurements_when_healthy);
    RUN_TEST(test_read_all_averages_adc_samples);
    RUN_TEST(test_read_all_tolerates_single_channel_failure);
    RUN_TEST(test_read_all_marks_dht_unavailable_after_retries);
    RUN_TEST(test_read_all_fails_when_every_sensor_unavailable);
    RUN_TEST(test_fail_counts_report_independently_and_tolerate_null);
    // NOTE: always exit 0. PIO's native runner maps any nonzero exit to a
    // bogus "Program received signal" suite error; parsed FAILED lines still
    // drive the suite status and the `pio test` exit code.
    UNITY_END();
    return 0;
}
