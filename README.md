# GrowMate ESP32-CAM

**Low-cost IoT plant monitoring system for ESP32-CAM devices**

GrowMate ESP32-CAM is embedded firmware that transforms a $10 ESP32-CAM module into a complete plant monitoring system with sensors, camera, and automated control. Monitor soil moisture, light levels, temperature, and humidity while capturing photos of your plants—all uploaded to your cloud API for remote monitoring and automated watering.

## Features

- **Multi-Sensor Monitoring** - Tracks soil moisture, light levels, water reservoir, temperature, and humidity
- **Camera Integration** - Captures and uploads 2MP JPEG images every 15 minutes
- **Automated Control** - Water pump and grow light control via cloud commands
- **WiFi Onboarding** - Web-based setup portal eliminates hardcoded credentials
- **Automatic Recovery** - Reopens setup portal after repeated connection failures
- **Low Power** - Runs on ~500mA, suitable for battery operation
- **Low Cost** - Complete system for under $15 in hardware
- **Fast Boot** - 2-3 second boot time for quick recovery

## Hardware You'll Need

- ESP32-CAM module (AI Thinker or compatible) - $8
- Soil moisture sensor (analog) - $2
- Light sensor (photoresistor) - $1
- DHT22 temperature/humidity sensor - $3
- Water level sensor (analog) - $2
- 2-channel relay module - $3
- USB-to-serial adapter (for flashing) - $3

**Total cost: ~$22** (plus pump and grow light if needed)

See [docs/HARDWARE.md](docs/HARDWARE.md) for detailed parts list and wiring.

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- USB-to-serial adapter (FTDI, CP2102, or CH340)
- ESP32-CAM module

### Flash Firmware

1. **Clone the repository:**
   ```bash
   git clone https://github.com/FarelRA/esp32-growmate-pods.git
   cd esp32-growmate-pods
   ```

2. **Connect ESP32-CAM to USB-to-serial adapter:**
   - ESP32-CAM GND → Adapter GND
   - ESP32-CAM 5V → Adapter 5V
   - ESP32-CAM U0R → Adapter TX
   - ESP32-CAM U0T → Adapter RX
   - ESP32-CAM IO0 → GND (for flashing mode)

3. **Build and flash:**
   ```bash
   pio run --target upload
   ```

4. **Remove IO0-to-GND connection and press reset**

For detailed flashing instructions, see [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)

## Setup

After flashing, the device enters setup mode:

1. Connect to WiFi network: `GrowMate-XXXXXX`
2. Open browser and navigate to: `http://192.168.4.1`
3. Fill in the setup form:
   - WiFi credentials
   - Device name and ID
   - API endpoints
   - Sensor intervals
   - Calibration values (optional)
4. Click "Save Configuration"

The device will connect to your WiFi and start monitoring.

## Configuration

### Build-Time Configuration

Edit `src/app_build_config.h` before building:

```c
#define APP_DEVICE_ID "IAET01"
#define APP_SENSOR_API_URL "https://api.example.com/sensors"
#define APP_CAMERA_API_URL "https://api.example.com/camera"
#define APP_SENSOR_INTERVAL_SEC 15
#define APP_CAMERA_INTERVAL_SEC 900
```

### Runtime Configuration

All settings can be changed through the onboarding portal without reflashing:
- WiFi credentials
- API endpoints
- Sensor intervals
- Calibration values

Configuration is stored in NVS (Non-Volatile Storage) and persists across reboots.

For advanced configuration, see [docs/CONFIGURATION.md](docs/CONFIGURATION.md)

## Usage

### Monitor Serial Output

```bash
pio device monitor
```

You'll see:
- Sensor readings every 15 seconds
- Camera captures every 15 minutes
- Upload status
- Command execution

### Re-enter Setup Mode

If you need to reconfigure:
1. Power off the device
2. Connect IO0 to GND
3. Power on
4. The setup portal will open automatically

Or trigger it by causing 5 consecutive connection failures (disconnect WiFi router).

## API Format

The device sends sensor data as JSON:

```json
{
  "deviceId": "IAET01",
  "firmwareVersion": "2.0.0",
  "sensors": [
    {"kind": "soil", "value": 45, "unit": "%", "raw": 2048},
    {"kind": "light", "value": 78, "unit": "%", "raw": 3200},
    {"kind": "temperature", "value": 25, "unit": "C"}
  ]
}
```

And receives commands:

```json
{
  "commands": [
    {"kind": "pump", "durationMs": 5000},
    {"kind": "light", "enabled": true}
  ]
}
```

Camera images are uploaded as JPEG with device ID in the header.

For complete API documentation, see [docs/API.md](docs/API.md)

## Troubleshooting

**Device won't enter flash mode?**
- Ensure IO0 is connected to GND before powering on
- Try a different USB-to-serial adapter
- Check all connections

**Can't connect to setup portal?**
- Look for `GrowMate-XXXXXX` network in WiFi list
- Try `http://192.168.4.1` in different browser
- Reset device and try again

**Sensors reading incorrectly?**
- Calibrate sensors through setup portal
- Check wiring connections
- Verify sensor power (3.3V for most sensors)

For more help, see [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Test on actual hardware
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

### Development Setup

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone https://github.com/FarelRA/esp32-growmate-pods.git
cd esp32-growmate-pods

# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed development guide.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

```
GrowMate ESP32-CAM - IoT plant monitoring firmware
Copyright (C) 2024  FarelRA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
```

## Support

- **Documentation:** [docs/](docs/)
- **Report bugs:** [GitHub Issues](https://github.com/FarelRA/esp32-growmate-pods/issues)
- **Ask questions:** [GitHub Discussions](https://github.com/FarelRA/esp32-growmate-pods/discussions)

## Comparison with Raspberry Pi Version

| Feature | ESP32-CAM | Raspberry Pi |
|---------|-----------|--------------|
| Cost | ~$15 | ~$70 |
| Power | 500mA | 1-2A |
| Boot Time | 2-3 seconds | 30-60 seconds |
| Memory | 520KB RAM | 512MB+ RAM |
| Offline Storage | No | Yes (24 hours) |
| Development | C (ESP-IDF) | Python |
| Updates | Reflash firmware | Git pull |

**Choose ESP32-CAM when:**
- Cost is a primary concern
- Power efficiency matters (battery operation)
- Space is limited
- Deploying multiple units

**Choose Raspberry Pi when:**
- Offline data storage is needed
- Easier development is preferred
- More processing power is required

See the [Raspberry Pi version](https://github.com/FarelRA/rpi-growmate-pods) for comparison.

---

**Made for plants and makers** 🌱
