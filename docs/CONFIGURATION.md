# Configuration Guide - ESP32-CAM

Configuration options for GrowMate ESP32-CAM firmware.

## Configuration Types

### Build-Time Configuration

Set in `src/app_build_config.h` before building. Requires reflashing to change.

### Runtime Configuration

Set through web portal at `http://192.168.4.1`. Stored in NVS, persists across reboots.

## Build-Time Configuration

Edit `src/app_build_config.h`:

```c
// Device Identity
#define APP_DEVICE_ID "IAET01"
#define APP_FIRMWARE_VERSION "2.0.0"

// API Endpoints
#define APP_SENSOR_API_URL "https://api.example.com/sensors"
#define APP_CAMERA_API_URL "https://api.example.com/camera"

// Hardware
#define APP_BOARD_PROFILE 0  // 0 = AI Thinker ESP32-CAM
#define APP_CAMERA_ENABLED 1

// Sensor Enable Flags
#define APP_SENSOR_SOIL_ENABLED 1
#define APP_SENSOR_LIGHT_ENABLED 1
#define APP_SENSOR_WATER_ENABLED 1
#define APP_SENSOR_TEMPERATURE_ENABLED 1
#define APP_SENSOR_AIR_ENABLED 1

// Timing (seconds)
#define APP_SENSOR_INTERVAL_SEC 15
#define APP_CAMERA_INTERVAL_SEC 900

// Calibration (ADC raw values 0-4095)
#define APP_WATER_RAW_EMPTY 0
#define APP_WATER_RAW_FULL 4095
#define APP_SOIL_RAW_DRY 0
#define APP_SOIL_RAW_WET 4095
#define APP_LIGHT_RAW_DARK 4095
#define APP_LIGHT_RAW_BRIGHT 0

// Recovery
#define APP_ONBOARDING_FAILURE_THRESHOLD 5
```

After editing, rebuild and reflash:
```bash
pio run --target upload
```

## Runtime Configuration

Access web portal:
1. Connect to `GrowMate-XXXXXX` WiFi
2. Open `http://192.168.4.1`
3. Fill in configuration form
4. Click "Save Configuration"

### WiFi Settings

- **SSID**: WiFi network name (max 32 characters)
- **Password**: WiFi password (max 64 characters)

### Device Settings

- **Device Name**: Friendly name for device
- **Device ID**: Unique identifier (used in API)
- **Plant ID**: Plant identifier (optional)

### API Settings

- **Sensor API URL**: Endpoint for sensor data
- **Camera API URL**: Endpoint for camera images

### Timing Settings

- **Sensor Interval**: Seconds between sensor readings (default: 15)
- **Camera Interval**: Seconds between camera captures (default: 900)

### Calibration

For each sensor, enter raw ADC values:

**Soil Moisture:**
- Dry: Value when sensor is in air
- Wet: Value when sensor is in water

**Light:**
- Dark: Value when sensor is covered
- Bright: Value in bright light

**Water Level:**
- Empty: Value when sensor is out of water
- Full: Value when sensor is submerged

### DHT22 Settings

- **DHT GPIO**: GPIO pin for DHT22 data (default: 12)
- **Enable DHT**: Check to enable temperature/humidity

## NVS Storage

Configuration is stored in Non-Volatile Storage (NVS) partition.

### Reset Configuration

**Method 1: Through web portal**
- Enter onboarding mode (connect IO0 to GND on boot)
- Reconfigure through web portal

**Method 2: Erase flash**
```bash
pio run --target erase
pio run --target upload
```

### Backup Configuration

Configuration cannot be exported directly. Document your settings manually.

## Board Profiles

Board profiles define hardware-specific settings in `src/board_profile.c`.

### AI Thinker ESP32-CAM (Profile 0)

```c
.name = "AI Thinker ESP32-CAM",
.gpio_soil = 14,
.gpio_light = 15,
.gpio_water = 13,
.gpio_dht = 12,
.gpio_pump = 2,
.gpio_light_relay = 4,
// Camera pins...
```

### Adding Custom Board

1. Define new profile in `board_profile.c`
2. Assign profile ID in `board_profile.h`
3. Set `APP_BOARD_PROFILE` in `app_build_config.h`
4. Rebuild and flash

## Advanced Settings

### Camera Resolution

Edit in `camera_service.c`:
```c
config.frame_size = FRAMESIZE_UXGA;  // 1600x1200
// Options: QVGA, VGA, SVGA, XGA, SXGA, UXGA
```

### PSRAM

Enable in `platformio.ini`:
```ini
build_flags = 
    -DBOARD_HAS_PSRAM
```

### Debug Level

Set in `platformio.ini`:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=3  // 0=None, 5=Verbose
```

## Troubleshooting

**Configuration not saving:**
- Check NVS partition is not corrupted
- Erase flash and reflash firmware

**Can't access web portal:**
- Verify device is in AP mode
- Check `GrowMate-XXXXXX` network is visible
- Try `http://192.168.4.1` in different browser

**Sensors reading incorrectly:**
- Recalibrate through web portal
- Verify calibration values are correct
- Check sensor wiring

For more help, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
