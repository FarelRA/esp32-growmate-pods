#include "onboarding_rules.h"

#include <string.h>

#include "api_rules.h"

bool onboarding_copy_field(const cJSON *root,
                           const char *key,
                           char *destination,
                           size_t destination_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item == NULL) {
        return true;
    }
    // Present-but-not-a-string (e.g. explicit null) is a client error,
    // not "keep the stale value and report success".
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    // Reject overlong values instead of silently truncating into
    // credentials that can never authenticate.
    if (strlen(item->valuestring) >= destination_size) {
        return false;
    }
    strlcpy(destination, item->valuestring, destination_size);
    return true;
}

bool onboarding_ssid_valid(const char *ssid)
{
    return api_rule_ssid_valid(ssid);
}
