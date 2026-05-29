#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "api_client.h"
#include "board_profile.h"

void actuators_init(const board_profile_t *profile);
void actuators_apply_commands(const board_profile_t *profile, const device_commands_t *commands);
void actuators_tick(const board_profile_t *profile);
bool actuators_is_pump_enabled(void);
bool actuators_is_light_enabled(void);
