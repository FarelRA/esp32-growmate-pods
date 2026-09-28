#include "network_rules.h"

#include <string.h>

#include "app_config.h"
#include "network_manager.h"

bool network_ap_name_valid(const char *ap_name)
{
    return ap_name != NULL && ap_name[0] != '\0';
}

bool network_ap_password_valid(const char *ap_password)
{
    // WPA2-only by contract: an open setup AP would expose the home WiFi
    // password typed into the portal to anyone in range.
    return ap_password != NULL && strlen(ap_password) >= 8;
}

bool network_station_args_valid(const app_config_t *config)
{
    return config != NULL;
}

bool network_scan_args_valid(const network_scan_result_t *results,
                             const size_t *result_count,
                             size_t max_results)
{
    return results != NULL && result_count != NULL && max_results > 0;
}

size_t network_scan_limit(size_t requested)
{
    return requested > NETWORK_MANAGER_MAX_SCAN_RESULTS
        ? NETWORK_MANAGER_MAX_SCAN_RESULTS
        : requested;
}
