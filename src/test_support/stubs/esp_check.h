// Host-only stub of ESP-IDF esp_check.h (only the macro firmware uses on
// host-compiled translation units). Failure aborts, matching target semantics.
#pragma once

#include <stdlib.h>

#include "esp_err.h"

#define ESP_ERROR_CHECK(x) do \
    { \
        esp_err_t __host_check_err = (x); \
        if (__host_check_err != ESP_OK) { \
            abort(); \
        } \
    } while (0)
