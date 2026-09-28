#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Pure scalar validation rules shared by api_client/onboarding/networking and
// the host Unity suite. No ESP-IDF calls: safe to compile on host.
#define API_RULE_REPORT_INTERVAL_MIN_SEC 10
#define API_RULE_REPORT_INTERVAL_MAX_SEC 3600
#define API_RULE_RETRY_AFTER_MAX_SEC 60

// Command/claim ids: mandatory, non-empty, room for the driver NUL.
bool api_rule_id_valid(const char *id, size_t max_len);
// Pump dose window in milliseconds: (0, APP_MAX_PUMP_DURATION_MS].
bool api_rule_dose_ms_valid(double duration_ms);
// Telemetry inclusion: raw samples need availability and a real reading;
// DHT samples need availability and a finite float.
bool api_rule_raw_sample_included(bool available, int raw);
bool api_rule_dht_sample_included(bool available, float value);
// Config push discipline: finite rev >= 1 that strictly advances the applied
// rev. Stale and duplicate pushes never touch NVS.
bool api_rule_config_rev_should_apply(uint32_t applied_rev, double candidate_rev);
// Server-pushed report interval, clamped into the sane window.
uint32_t api_rule_clamp_report_interval(double value_sec);
// Retry-After backoff wait, clamped into [0, 60] s.
int api_rule_clamp_retry_after_sec(long wait_sec);
// Claim charset: [A-Za-z0-9_-], non-empty (length cap checked by the caller
// through api_rule_id_valid).
bool api_rule_claim_charset_valid(const char *id);
// Onboarding SSID bounds: the WiFi driver needs a NUL inside its 32-byte
// SSID field, so 1-31 chars or the join can never succeed.
bool api_rule_ssid_valid(const char *ssid);
