// Host-only stub of FreeRTOS task delays: delays are no-ops so suites stay
// fast and deterministic (firmware feeds the watchdog in these windows,
// which has no meaning on host).
#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
