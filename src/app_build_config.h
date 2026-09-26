#pragma once

// Developer-managed constants. These are compiled into the firmware and are
// not user-configurable through onboarding or stored in NVS.
#define APP_DEVICE_ID "IAET01"
#define APP_FIRMWARE_VERSION "2.0.0"
#define APP_SENSOR_API_URL "https://avid-mammoth-766.convex.site/api/v1/sensors"
#define APP_CAMERA_API_URL "https://avid-mammoth-766.convex.site/api/v1/camera"
#define APP_API_TOKEN ""
#define APP_MAX_PUMP_DURATION_MS 30000
#define APP_MAX_LIGHT_ON_MS (24ULL * 3600ULL * 1000ULL)
#define APP_OTA_ENABLED 1

#define APP_BOARD_PROFILE 0
#define APP_CAMERA_ENABLED 1

#define APP_SENSOR_SOIL_ENABLED 1
#define APP_SENSOR_LIGHT_ENABLED 1
#define APP_SENSOR_WATER_ENABLED 1
#define APP_SENSOR_TEMPERATURE_ENABLED 1
#define APP_SENSOR_AIR_ENABLED 1

#define APP_SENSOR_INTERVAL_SEC 15
#define APP_CAMERA_INTERVAL_SEC 900

#define APP_ONBOARDING_FAILURE_THRESHOLD 5
