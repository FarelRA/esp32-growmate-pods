#pragma once

#include "app_config.h"

// Device identity: every unit is born with its WiFi MAC as hardware ID.
// The server may assign a pod ID via the claim flow (persisted to NVS);
// the effective ID is the assigned one, falling back to the build default.
void device_identity_hwid(char out_hwid13[13]);
const char *device_effective_id(const app_config_t *config);
void device_onboarding_ap_credentials(const app_config_t *config,
                                      char ssid_out33[33],
                                      char pass_out64[64]);
