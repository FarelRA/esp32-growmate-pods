# Troubleshooting Guide - ESP32-CAM

Common issues and solutions for GrowMate ESP32-CAM.

## Table of Contents

- [Flashing Issues](#flashing-issues)
- [Boot Issues](#boot-issues)
- [WiFi Issues](#wifi-issues)
- [Sensor Issues](#sensor-issues)
- [Camera Issues](#camera-issues)
- [Serial Monitor](#serial-monitor)

## Flashing Issues

### Can't Enter Flash Mode

**Symptoms:** Upload fails with "Failed to connect to ESP32"

**Solutions:**

1. **Connect IO0 to GND before powering on:**
   ```
   1. Disconnect power
   2. Connect IO0 to GND
   3. Connect power
   4. Start upload
   5. Disconnect IO0 after upload starts
   ```

2. **Check USB-to-serial adapter:**
   - Verify TX/RX are crossed (TX→RX, RX→TX)
   - Try different adapter (FTDI, CP2102, CH340)
   - Check driver installation

3. **Check power supply:**
   - Must provide stable 5V
   - Minimum 500mA current
   - Try different USB port or power supply

4. **Press reset while uploading:**
   - Start upload in PlatformIO
   - Press and hold reset button
   - Release when "Connecting..." appears

### Upload Fails Midway

**Symptoms:** Upload starts but fails at 50-80%

**Solutions:**

1. **Reduce upload speed:**
   ```ini
   # In platformio.ini
   upload_speed = 115200  ; Instead of 921600
   ```

2. **Check USB cable:**
   - Use short, quality cable
   - Avoid USB hubs
   - Try different USB port

3. **Power issues:**
   - Use external 5V power supply
   - Add 1000µF capacitor near ESP32-CAM

### Wrong Board Selected

**Symptoms:** Upload succeeds but device doesn't work

**Solution:**
```ini
# Verify platformio.ini has correct board
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = espidf
```

## Boot Issues

### Device Reboots Continuously

**Symptoms:** Serial monitor shows repeated boot messages

**Causes and Solutions:**

1. **Insufficient power:**
   - Use 2A power supply
   - Add 1000µF capacitor
   - Shorten USB cable

2. **Brownout detector:**
   ```
   Brownout detector was triggered
   ```
   - Increase power supply capacity
   - Check for short circuits
   - Remove power-hungry peripherals

3. **Watchdog timer:**
   ```
   Task watchdog got triggered
   ```
   - Check for infinite loops in code
   - Reflash firmware
   - Reset NVS: connect IO0 to GND on boot

4. **Corrupted NVS:**
   - Erase flash completely:
     ```bash
     pio run --target erase
     pio run --target upload
     ```

### Stuck at Boot

**Symptoms:** Device boots but hangs before WiFi

**Solutions:**

1. **Check serial output:**
   ```bash
   pio device monitor
   ```
   Look for error messages

2. **Camera initialization failure:**
   - Check camera ribbon cable
   - Verify camera is OV2640
   - Disable camera in build config temporarily

3. **Sensor initialization failure:**
   - Disconnect all sensors
   - Flash firmware
   - Reconnect sensors one by one

## WiFi Issues

### Can't See Setup Portal

**Symptoms:** GrowMate-XXXXXX network not visible

**Solutions:**

1. **Force onboarding mode:**
   - Connect IO0 to GND
   - Power on device
   - Setup portal should appear

2. **Check serial output:**
   ```bash
   pio device monitor
   ```
   Look for "Starting AP mode" message

3. **WiFi channel conflict:**
   - Change router channel to 1-11
   - ESP32 doesn't support channels 12-14

4. **Antenna issues:**
   - Ensure antenna is connected (if external)
   - Check for physical damage

### Can't Connect to Home WiFi

**Symptoms:** Device stays in AP mode after configuration

**Solutions:**

1. **Wrong credentials:**
   - Re-enter WiFi password carefully
   - SSID is case-sensitive
   - Check for special characters

2. **WiFi signal too weak:**
   ```bash
   # Check signal strength in serial monitor
   RSSI: -XX dBm
   ```
   - Move closer to router (RSSI > -70 dBm)
   - Use WiFi extender
   - Improve antenna

3. **Router compatibility:**
   - ESP32 only supports 2.4GHz (not 5GHz)
   - Disable WiFi 6 (802.11ax) on router
   - Try WPA2 instead of WPA3

4. **MAC filtering:**
   - Check router MAC filter settings
   - Add ESP32-CAM MAC address to whitelist

### Frequent Disconnections

**Symptoms:** Device connects but disconnects repeatedly

**Solutions:**

1. **Power issues:**
   - Use stable 2A power supply
   - Add capacitor near ESP32-CAM

2. **WiFi interference:**
   - Move away from microwave, Bluetooth devices
   - Change router channel
   - Use 5GHz router for other devices

3. **Router issues:**
   - Increase DHCP lease time
   - Disable power saving on router
   - Update router firmware

## Sensor Issues

### Sensors Not Reading

**Symptoms:** Serial monitor shows 0 or invalid values

**Solutions:**

1. **Check wiring:**
   ```
   Sensor VCC → ESP32-CAM 3.3V
   Sensor GND → ESP32-CAM GND
   Sensor OUT → Correct GPIO
   ```

2. **Verify GPIO pins:**
   - Soil: GPIO 14
   - Light: GPIO 15
   - Water: GPIO 13
   - DHT22: GPIO 12

3. **Check power:**
   - Measure 3.3V at sensor VCC pin
   - Ensure good ground connection

4. **Test sensors:**
   - Measure sensor output with multimeter
   - Should vary between 0-3.3V
   - Replace if defective

### Incorrect Sensor Values

**Symptoms:** Readings don't match reality

**Solutions:**

1. **Calibrate sensors:**
   - Access setup portal
   - Enter calibration values:
     - Soil: dry (in air) and wet (in water)
     - Light: dark (covered) and bright (sunlight)
     - Water: empty and full

2. **Check sensor placement:**
   - Soil sensor: 2-3 inches deep
   - Light sensor: above canopy, facing up
   - Water sensor: vertical in reservoir

3. **Sensor degradation:**
   - Resistive sensors corrode over time
   - Replace with capacitive sensors
   - Clean sensor contacts

### DHT22 Not Working

**Symptoms:** Temperature/humidity show 0 or error

**Solutions:**

1. **Check pull-up resistor:**
   - Must have 10kΩ resistor between VCC and DATA
   - Without it, readings will fail

2. **Verify wiring:**
   ```
   DHT22 Pin 1 (VCC)  → 3.3V
   DHT22 Pin 2 (DATA) → GPIO 12 + 10kΩ to VCC
   DHT22 Pin 4 (GND)  → GND
   ```

3. **GPIO conflict:**
   - GPIO 12 must not conflict with camera
   - Check board profile configuration

4. **Sensor failure:**
   - DHT22 can fail after 1-2 years
   - Try different DHT22 sensor
   - Consider BME280 as alternative

## Camera Issues

### Camera Not Initializing

**Symptoms:** Serial monitor shows "Camera init failed"

**Solutions:**

1. **Check ribbon cable:**
   - Disconnect and reconnect firmly
   - Ensure correct orientation
   - Check for damage

2. **PSRAM not detected:**
   ```
   PSRAM not found
   ```
   - Some ESP32-CAM boards lack PSRAM
   - Reduce camera resolution in config
   - Use QVGA instead of UXGA

3. **Power issues:**
   - Camera needs stable power
   - Use 2A power supply
   - Add capacitor

4. **Wrong camera model:**
   - Firmware expects OV2640
   - Check camera chip marking
   - Modify board profile if different

### Camera Images Are Black

**Symptoms:** Camera captures but images are all black

**Solutions:**

1. **Remove lens cap:**
   - Check for protective film on lens

2. **Insufficient light:**
   - Camera needs some light to function
   - Add grow light or room lighting

3. **Camera settings:**
   - Exposure may be too low
   - Adjust in camera_service.c if needed

### Camera Images Are Corrupted

**Symptoms:** Images have artifacts or wrong colors

**Solutions:**

1. **PSRAM issues:**
   - Verify PSRAM is enabled
   - Reduce image resolution

2. **Power instability:**
   - Use quality power supply
   - Add capacitor

3. **Interference:**
   - Keep camera cable away from power wires
   - Use shielded cable if possible

## Serial Monitor

### No Output in Serial Monitor

**Solutions:**

1. **Check baud rate:**
   ```bash
   pio device monitor --baud 115200
   ```

2. **Check USB connection:**
   - Verify TX/RX connections
   - Try different USB port

3. **Device not booting:**
   - Check power LED
   - Verify 5V power supply

### Garbled Output

**Solutions:**

1. **Wrong baud rate:**
   - Try 115200, 9600, or 74880
   - ESP32 boot messages are at 74880

2. **Loose connections:**
   - Check TX/RX wiring
   - Ensure good ground connection

### Useful Serial Commands

Monitor sensor readings:
```bash
pio device monitor | grep "Sensor"
```

Monitor WiFi status:
```bash
pio device monitor | grep "WiFi"
```

Monitor uploads:
```bash
pio device monitor | grep "Upload"
```

## Getting Help

If you can't resolve your issue:

1. **Collect diagnostic information:**
   ```bash
   pio device monitor > debug.log
   ```
   Run for 2-3 minutes to capture boot and operation

2. **Check existing issues:**
   - [GitHub Issues](https://github.com/FarelRA/esp32-growmate-pods/issues)

3. **Open new issue with:**
   - ESP32-CAM board variant
   - Power supply specifications
   - Serial monitor output
   - Photos of wiring
   - Steps to reproduce

4. **Additional resources:**
   - [HARDWARE.md](HARDWARE.md) - Wiring guide
   - [DEVELOPMENT.md](DEVELOPMENT.md) - Build and flash
   - [CONFIGURATION.md](CONFIGURATION.md) - Settings
