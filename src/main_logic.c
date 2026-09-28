#include "main_logic.h"

#include "app_build_config.h"

uint32_t main_logic_report_interval_sec(uint32_t configured_sec)
{
    return configured_sec != 0 ? configured_sec : APP_SENSOR_INTERVAL_SEC;
}

bool main_logic_camera_slot_due(bool camera_available,
                                uint32_t sensor_interval_sec,
                                uint32_t *loops_since_camera)
{
    if (!camera_available) {
        *loops_since_camera = 0;
        return false;
    }
    if (sensor_interval_sec == 0) {
        sensor_interval_sec = APP_SENSOR_INTERVAL_SEC;
    }
    (*loops_since_camera)++;
    uint32_t camera_period = APP_CAMERA_INTERVAL_SEC / sensor_interval_sec;
    if (camera_period == 0) {
        camera_period = 1;
    }
    return *loops_since_camera >= camera_period;
}

bool main_logic_should_reopen_portal(uint32_t consecutive_failures)
{
    return consecutive_failures >= APP_ONBOARDING_FAILURE_THRESHOLD;
}
