#include "device_identity.h"

#include <stdio.h>
#include <string.h>

#include "app_build_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"

static const char *TAG = "identity";

void device_identity_hwid(char out_hwid13[13])
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGW(TAG, "MAC read failed, using zeros");
    }

    snprintf(out_hwid13, 13, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *device_effective_id(const app_config_t *config)
{
    if (config != NULL && config->device_id[0] != '\0') {
        return config->device_id;
    }
    return APP_DEVICE_ID;
}

void device_onboarding_ap_credentials(const app_config_t *config,
                                      char ssid_out33[33],
                                      char pass_out64[64])
{
    const char *id = device_effective_id(config);
    size_t id_len = strlen(id);
    const char *id_tail = id + (id_len > 6 ? id_len - 6 : 0);

    char hwid[13];
    device_identity_hwid(hwid);

    snprintf(ssid_out33, 33, "GrowMate-%s", id_tail);
    snprintf(pass_out64, 64, "GrowMate-%s", hwid + 6);
}
