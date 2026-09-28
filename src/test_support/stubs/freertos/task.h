// Host-only stub of FreeRTOS task.h (see FreeRTOS.h for the delay rationale).
#pragma once

#include "freertos/FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks)
{
    (void) ticks;
}
