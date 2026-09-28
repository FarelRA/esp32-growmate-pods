#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "network_manager.h"

// SoftAP credential guards shared by network_manager.c and the host suite.
// No WiFi calls: safe to compile on host.
bool network_ap_name_valid(const char *ap_name);
bool network_ap_password_valid(const char *ap_password);
// Station/scan argument guards: NULL sinks and empty scans never reach the
// WiFi driver.
bool network_station_args_valid(const app_config_t *config);
bool network_scan_args_valid(const network_scan_result_t *results,
                             const size_t *result_count,
                             size_t max_results);
size_t network_scan_limit(size_t requested);
