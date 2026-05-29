#pragma once

#include <stddef.h>

#include "app_config.h"
#include "esp_err.h"

#define WIFI_CONNECTED_BIT (1U << 0)
#define WIFI_FAIL_BIT (1U << 1)

#define NETWORK_MANAGER_MAX_SCAN_RESULTS 12

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t auth_mode;
} network_scan_result_t;

esp_err_t network_manager_init(void);
esp_err_t network_manager_start_station(const app_config_t *config, uint32_t timeout_ms);
esp_err_t network_manager_start_onboarding_ap(const char *ap_name, const char *ap_password);
esp_err_t network_manager_stop(void);
esp_err_t network_manager_scan(network_scan_result_t *results, size_t max_results, size_t *result_count);
