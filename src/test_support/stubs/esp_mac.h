// Host-only stub of ESP-IDF esp_mac.h. The MAC bytes are scripted per test
// through fakes/fake_runtime.h.
#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    ESP_MAC_WIFI_STA,
    ESP_MAC_WIFI_SOFTAP,
    ESP_MAC_BT,
    ESP_MAC_ETH,
} esp_mac_type_t;

esp_err_t esp_read_mac(uint8_t mac[6], esp_mac_type_t type);
