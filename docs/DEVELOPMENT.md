# Development Guide - ESP32-CAM

Complete guide for developing and contributing to GrowMate ESP32-CAM firmware.

## Table of Contents

- [Development Setup](#development-setup)
- [Building](#building)
- [Flashing](#flashing)
- [Debugging](#debugging)
- [Project Structure](#project-structure)
- [Adding Features](#adding-features)
- [Testing](#testing)

## Development Setup

### Prerequisites

**Required:**
- Python 3.7 or newer
- PlatformIO Core or PlatformIO IDE
- USB-to-serial adapter (FTDI, CP2102, or CH340)
- ESP32-CAM module

**Optional:**
- Visual Studio Code with PlatformIO extension
- Git for version control

### Install PlatformIO

**Option 1: PlatformIO Core (CLI)**
```bash
pip install platformio
```

**Option 2: PlatformIO IDE (VS Code)**
1. Install Visual Studio Code
2. Install PlatformIO IDE extension
3. Restart VS Code

### Clone Repository

```bash
git clone https://github.com/FarelRA/esp32-growmate-pods.git
cd esp32-growmate-pods
```

### Install Dependencies

PlatformIO automatically installs dependencies on first build:
```bash
pio run
```

Dependencies include:
- ESP-IDF 5.5.0
- esp32-camera library
- DHT22 sensor library
- cJSON library

## Building

### Build Firmware

```bash
# Build for default environment (esp32cam)
pio run

# Build with verbose output
pio run -v

# Clean build
pio run --target clean
pio run
```

### Build Output

Firmware binary is created at:
```
.pio/build/esp32cam/firmware.bin
```

Size information:
```
RAM:   [====      ]  40.2% (used 131876 bytes from 327680 bytes)
Flash: [======    ]  60.5% (used 1987234 bytes from 3145728 bytes)
```

### Build Configuration

Edit `platformio.ini` to customize build:

```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = espidf

; Upload settings
upload_speed = 921600
monitor_speed = 115200

; Build flags
build_flags = 
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM
```

## Flashing

### Hardware Setup

1. **Connect USB-to-serial adapter:**
   ```
   Adapter → ESP32-CAM
   GND     → GND
   5V      → 5V
   TX      → U0R (RX)
   RX      → U0T (TX)
   ```

2. **Enter flash mode:**
   - Connect IO0 to GND
   - Power on or press reset
   - IO0 must be LOW during boot

### Flash Firmware

```bash
# Flash firmware
pio run --target upload

# Flash and monitor
pio run --target upload && pio device monitor

# Erase flash completely (if needed)
pio run --target erase
pio run --target upload
```

### Flash Troubleshooting

**Upload fails:**
- Verify IO0 is connected to GND
- Check TX/RX are crossed (TX→RX, RX→TX)
- Try lower upload speed: `upload_speed = 115200`
- Use different USB port or cable

**Device not detected:**
- Install USB-to-serial driver (CH340, CP2102, FTDI)
- Check device manager (Windows) or `ls /dev/tty*` (Linux/Mac)
- Verify power supply provides stable 5V

## Debugging

### Serial Monitor

**Start monitor:**
```bash
pio device monitor
```

**Monitor with filters:**
```bash
# Show only errors
pio device monitor | grep ERROR

# Show sensor readings
pio device monitor | grep "Sensor"

# Show WiFi events
pio device monitor | grep "WiFi"
```

**Monitor settings:**
```bash
# Custom baud rate
pio device monitor --baud 115200

# Raw mode (no filters)
pio device monitor --raw

# Exit: Ctrl+C
```

### Debug Output Levels

Set in `platformio.ini`:
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=0  ; None
    -DCORE_DEBUG_LEVEL=1  ; Error
    -DCORE_DEBUG_LEVEL=2  ; Warning
    -DCORE_DEBUG_LEVEL=3  ; Info
    -DCORE_DEBUG_LEVEL=4  ; Debug
    -DCORE_DEBUG_LEVEL=5  ; Verbose
```

### Common Debug Messages

**Boot sequence:**
```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:6664
...
```

**Normal operation:**
```
[INFO] WiFi connected: 192.168.1.100
[INFO] Sensor reading: soil=45%, light=78%, temp=25C
[INFO] Upload successful
[INFO] Camera capture: 1600x1200, 45KB
```

**Errors:**
```
[ERROR] Camera init failed
[ERROR] WiFi connection failed
[ERROR] Upload failed: HTTP 500
```

### JTAG Debugging

For advanced debugging with JTAG:
1. Connect JTAG adapter (ESP-Prog, J-Link)
2. Configure OpenOCD
3. Use GDB for breakpoints and stepping

See ESP-IDF documentation for JTAG setup.

## Project Structure

```
esp32-growmate-pods/
├── src/                      # Source code
│   ├── main.c               # Main application loop
│   ├── app_config.h/c       # Configuration management
│   ├── app_build_config.h   # Build-time constants
│   ├── board_profile.h/c    # Hardware abstraction
│   ├── network_manager.h/c  # WiFi management
│   ├── onboarding.h/c       # Web setup portal
│   ├── sensors.h/c          # Sensor reading
│   ├── camera_service.h/c   # Camera control
│   ├── api_client.h/c       # HTTPS upload
│   └── actuators.h/c        # Pump/light control
├── components/              # External libraries
│   ├── esp32-camera/        # Camera driver
│   ├── DHT22/               # DHT22 sensor
│   └── esp-idf-lib/         # ESP-IDF components
├── docs/                    # Documentation
├── platformio.ini           # Build configuration
├── CMakeLists.txt          # ESP-IDF project config
├── partitions.csv          # Flash partition table
└── sdkconfig.esp32cam      # ESP-IDF SDK config
```

### Key Files

**main.c:**
- Application entry point
- Main loop orchestration
- Failure recovery logic

**app_config.h/c:**
- NVS storage management
- Configuration defaults
- Validation and sanitization

**board_profile.h/c:**
- GPIO pin mappings
- ADC channel assignments
- Camera pin configuration

**network_manager.h/c:**
- WiFi station/AP modes
- Network scanning
- Connection management

**onboarding.h/c:**
- HTTP server
- Web UI (embedded HTML/CSS/JS)
- Configuration endpoints

## Adding Features

### Add New Sensor

1. **Define GPIO in board_profile.h:**
   ```c
   #define BOARD_GPIO_NEW_SENSOR 16
   ```

2. **Add reading function in sensors.c:**
   ```c
   int read_new_sensor(void) {
       // Read ADC or digital sensor
       return value;
   }
   ```

3. **Add to sensor payload in api_client.c:**
   ```c
   cJSON_AddItemToArray(sensors, 
       create_sensor_object("new_sensor", value, "%", raw));
   ```

4. **Update build config if needed:**
   ```c
   #define APP_SENSOR_NEW_ENABLED 1
   ```

### Add New Board Profile

1. **Define profile in board_profile.h:**
   ```c
   #define BOARD_PROFILE_NEW_BOARD 1
   ```

2. **Add configuration in board_profile.c:**
   ```c
   static const board_profile_t profiles[] = {
       [BOARD_PROFILE_NEW_BOARD] = {
           .name = "New Board",
           .gpio_soil = 14,
           .gpio_light = 15,
           // ... other pins
       }
   };
   ```

3. **Set in app_build_config.h:**
   ```c
   #define APP_BOARD_PROFILE BOARD_PROFILE_NEW_BOARD
   ```

### Modify Web UI

Web UI is embedded in `onboarding.c`:

```c
static const char* html_page = 
    "<!DOCTYPE html>"
    "<html>"
    // ... HTML content
    "</html>";
```

To modify:
1. Edit HTML/CSS/JS in `onboarding.c`
2. Keep it minimal (limited flash space)
3. Test in browser before embedding
4. Rebuild and flash firmware

## Testing

### Unit Testing

Currently no unit tests. Contributions welcome!

Suggested framework: Unity (ESP-IDF component)

### Hardware Testing

**Test sensors:**
```bash
# Monitor sensor readings
pio device monitor | grep "Sensor"

# Verify values change when conditions change
# - Soil: insert in water vs air
# - Light: cover vs expose to light
# - DHT22: breathe on sensor (humidity increases)
```

**Test camera:**
```bash
# Monitor camera captures
pio device monitor | grep "Camera"

# Verify image size is reasonable (20-100KB)
# Check upload success
```

**Test actuators:**
```bash
# Monitor relay activation
pio device monitor | grep "Actuator"

# Listen for relay click
# Verify pump/light turns on
```

### Integration Testing

1. **Flash firmware**
2. **Complete onboarding**
3. **Monitor for 1 hour:**
   - Sensor readings every 15 seconds
   - Camera captures every 15 minutes
   - Uploads succeed
   - Commands execute

4. **Test failure recovery:**
   - Disconnect WiFi router
   - Wait for 5 consecutive failures
   - Verify onboarding portal reopens

### Performance Testing

**Memory usage:**
```bash
# Monitor free heap
pio device monitor | grep "Free heap"
```

**Upload timing:**
```bash
# Monitor upload duration
pio device monitor | grep "Upload took"
```

**Boot time:**
```bash
# Measure from reset to first sensor reading
# Should be 2-3 seconds
```

## Code Style

### C Style Guidelines

- **Indentation:** 4 spaces (no tabs)
- **Braces:** K&R style
- **Naming:**
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_CASE`
  - Types: `snake_case_t`

**Example:**
```c
typedef struct {
    int value;
    char* name;
} sensor_data_t;

static int read_sensor(int gpio_pin) {
    if (gpio_pin < 0) {
        return -1;
    }
    
    int value = adc_read(gpio_pin);
    return value;
}
```

### Comments

- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document all public functions
- Explain complex logic

### Error Handling

- Check return values
- Log errors with ESP_LOGE
- Return error codes
- Clean up resources on failure

## Contributing

### Workflow

1. Fork repository
2. Create feature branch
3. Make changes
4. Test on hardware
5. Commit with clear message
6. Push to fork
7. Open pull request

### Pull Request Guidelines

- Describe what changed and why
- Include test results
- Reference related issues
- Keep changes focused

### Code Review

All PRs require:
- Code review by maintainer
- Successful build
- Hardware testing (if applicable)
- Documentation updates

## Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32-CAM Datasheet](https://github.com/raphaelbs/esp32-cam-ai-thinker)
- [OV2640 Camera Datasheet](https://www.uctronics.com/download/cam_module/OV2640DS.pdf)

## Support

- [HARDWARE.md](HARDWARE.md) - Hardware setup
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues
- [CONFIGURATION.md](CONFIGURATION.md) - Configuration options
- [GitHub Issues](https://github.com/FarelRA/esp32-growmate-pods/issues)
