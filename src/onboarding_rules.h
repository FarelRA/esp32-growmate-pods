#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

// Onboarding portal validation, shared by onboarding.c and the host suite.
// No HTTP/NVS side effects: safe to compile on host.
bool onboarding_copy_field(const cJSON *root,
                           const char *key,
                           char *destination,
                           size_t destination_size);
bool onboarding_ssid_valid(const char *ssid);
