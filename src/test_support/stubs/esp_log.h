// Host-only stub of ESP-IDF esp_log.h: firmware log calls compile to no-ops
// in the native test env so suites stay deterministic and quiet.
#pragma once

#define ESP_LOGI(tag, format, ...) ((void) 0)
#define ESP_LOGW(tag, format, ...) ((void) 0)
#define ESP_LOGE(tag, format, ...) ((void) 0)
#define ESP_LOGD(tag, format, ...) ((void) 0)
#define ESP_LOGV(tag, format, ...) ((void) 0)
