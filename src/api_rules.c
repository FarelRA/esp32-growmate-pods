#include "api_rules.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "app_build_config.h"

bool api_rule_id_valid(const char *id, size_t max_len)
{
    return id != NULL && id[0] != '\0' && strlen(id) <= max_len;
}

bool api_rule_dose_ms_valid(double duration_ms)
{
    return isfinite(duration_ms) &&
           duration_ms > 0.0 &&
           duration_ms <= (double) APP_MAX_PUMP_DURATION_MS;
}

bool api_rule_raw_sample_included(bool available, int raw)
{
    return available && raw >= 0;
}

bool api_rule_dht_sample_included(bool available, float value)
{
    return available && isfinite((double) value);
}

bool api_rule_config_rev_should_apply(uint32_t applied_rev, double candidate_rev)
{
    if (!isfinite(candidate_rev) || candidate_rev < 1.0) {
        return false;
    }
    return (uint32_t) candidate_rev > applied_rev;
}

uint32_t api_rule_clamp_report_interval(double value_sec)
{
    if (value_sec < (double) API_RULE_REPORT_INTERVAL_MIN_SEC) {
        value_sec = (double) API_RULE_REPORT_INTERVAL_MIN_SEC;
    }
    if (value_sec > (double) API_RULE_REPORT_INTERVAL_MAX_SEC) {
        value_sec = (double) API_RULE_REPORT_INTERVAL_MAX_SEC;
    }
    return (uint32_t) value_sec;
}

int api_rule_clamp_retry_after_sec(long wait_sec)
{
    if (wait_sec < 0) {
        wait_sec = 0;
    }
    if (wait_sec > API_RULE_RETRY_AFTER_MAX_SEC) {
        wait_sec = API_RULE_RETRY_AFTER_MAX_SEC;
    }
    return (int) wait_sec;
}

bool api_rule_claim_charset_valid(const char *id)
{
    if (id == NULL || id[0] == '\0') {
        return false;
    }
    for (const char *p = id; *p != '\0'; ++p) {
        if (!isalnum((unsigned char) *p) && *p != '-' && *p != '_') {
            return false;
        }
    }
    return true;
}

bool api_rule_ssid_valid(const char *ssid)
{
    return ssid != NULL && strlen(ssid) >= 1 && strlen(ssid) <= 31;
}
