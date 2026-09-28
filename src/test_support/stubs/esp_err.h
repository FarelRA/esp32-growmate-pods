// Host-only stub of ESP-IDF esp_err.h for the PlatformIO native test env.
// Never on the firmware include path: only visible via the [env:native]
// build_flags -I entry. Values only need ESP_OK == 0 and distinctness.
#pragma once

#include <stdint.h>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL 0x101
#define ESP_ERR_NO_MEM 0x102
#define ESP_ERR_INVALID_ARG 0x103
#define ESP_ERR_INVALID_STATE 0x104
#define ESP_ERR_INVALID_SIZE 0x105
#define ESP_ERR_NOT_FOUND 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_VERSION 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_WIFI_NOT_INIT 0x3000
#define ESP_ERR_WIFI_NOT_STARTED 0x3001
#define ESP_ERR_NVS_NOT_FOUND 0x1101
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1102
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1103

const char *esp_err_to_name(esp_err_t err);
