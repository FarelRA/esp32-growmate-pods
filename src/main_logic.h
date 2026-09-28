#pragma once

#include <stdbool.h>
#include <stdint.h>

// Main-loop scheduling helpers shared by app_main and the host suite.
// No RTOS/network calls: safe to compile on host.
uint32_t main_logic_report_interval_sec(uint32_t configured_sec);
bool main_logic_camera_slot_due(bool camera_available,
                                uint32_t sensor_interval_sec,
                                uint32_t *loops_since_camera);
bool main_logic_should_reopen_portal(uint32_t consecutive_failures);
