# Hardware Guide - ESP32-CAM

Complete hardware requirements and assembly guide for GrowMate ESP32-CAM.

## Table of Contents

- [Bill of Materials](#bill-of-materials)
- [ESP32-CAM Variants](#esp32-cam-variants)
- [Pin Assignments](#pin-assignments)
- [Wiring Guide](#wiring-guide)
- [Power Requirements](#power-requirements)
- [Assembly Tips](#assembly-tips)

## Bill of Materials

### Core Components

| Component | Specification | Cost | Where to Buy | Notes |
|-----------|--------------|------|--------------|-------|
| ESP32-CAM | AI Thinker or compatible | $8 | AliExpress, Amazon | Includes OV2640 camera |
| USB-to-Serial Adapter | FTDI, CP2102, or CH340 | $3 | Amazon, AliExpress | For flashing firmware |
| Soil Moisture Sensor | Analog capacitive | $2 | Amazon, AliExpress | Capacitive preferred |
| Light Sensor | Photoresistor module | $1 | Amazon, AliExpress | Analog output |
| DHT22 Sensor | Digital temp/humidity | $3 | Adafruit, Amazon | DHT11 also works |
| Water Level Sensor | Analog resistive | $2 | Amazon, AliExpress | For reservoir monitoring |
| 2-Channel Relay Module | 5V, optocoupler | $3 | Amazon, AliExpress | For pump and light |
| Jumper Wires | Male-to-female | $3 | Amazon, AliExpress | 20-30 wires |
| **Total** | | **~$25** | | |

### Optional Components

| Component | Purpose | Cost |
|-----------|---------|------|
| Water pump | Automated watering | $5-10 |
| Grow light | Plant lighting | $10-20 |
| 5V power supply | Stable power | $5 |
| Enclosure | Weather protection | $5-10 |

## ESP32-CAM Variants

### AI Thinker ESP32-CAM (Default)

**Specifications:**
- ESP32-S chip (dual-core 240MHz)
- 4MB flash memory
- PSRAM (optional, usually included)
- OV2640 camera (2MP)
- MicroSD card slot
- Built-in antenna

**GPIO Available:**
- GPIO 12, 13, 14, 15 (ADC2)
- GPIO 2, 4 (digital output)

**Camera Pins (Fixed):**
- GPIO 0, 5, 18, 19, 21, 22, 23, 25, 26, 27
- GPIO 32, 34, 35, 36, 39

### Other Compatible Boards

**ESP32-CAM-MB (with USB):**
- Same as AI Thinker but includes USB programmer
- No need for separate USB-to-serial adapter
- More expensive (~$12)

**M5Stack ESP32-CAM:**
- Different pin layout
- Requires custom board profile
- Better build quality

**Freenove ESP32-WROVER-CAM:**
- More GPIO available
- Better camera options
- Requires custom board profile

## Pin Assignments

### AI Thinker ESP32-CAM (Default Profile)

```
Sensors (Analog):
├─ GPIO 13 (ADC2_CH4) → Water level sensor
├─ GPIO 14 (ADC2_CH6) → Soil moisture sensor
└─ GPIO 15 (ADC2_CH3) → Light sensor

Sensors (Digital):
└─ GPIO 12 → DHT22 data pin

Actuators:
├─ GPIO 2 → Water pump relay (active HIGH)
└─ GPIO 4 → Grow light relay (active HIGH)

Camera (Fixed, do not use):
├─ GPIO 0 → XCLK
├─ GPIO 26 → SIOD (I2C SDA)
├─ GPIO 27 → SIOC (I2C SCL)
├─ GPIO 25 → VSYNC
├─ GPIO 23 → HREF
├─ GPIO 22 → PCLK
├─ GPIO 21 → D7
├─ GPIO 19 → D6
├─ GPIO 18 → D5
├─ GPIO 5 → D4
├─ GPIO 36 → D3
├─ GPIO 39 → D2
├─ GPIO 34 → D1
├─ GPIO 35 → D0
└─ GPIO 32 → PWDN

Power:
├─ 5V → Power input
└─ GND → Ground
```

### Important Notes

- **ADC2 limitation**: ADC2 channels cannot be used while WiFi is active. Sensors are read before WiFi connection.
- **Camera pins**: Do not use camera pins for other purposes or camera will fail.
- **GPIO 0**: Used for boot mode selection (flash mode when LOW).
- **GPIO 12**: Can be used for DHT22 since it's read before camera initialization.

## Wiring Guide

### Soil Moisture Sensor

```
Sensor → ESP32-CAM
VCC    → 3.3V
GND    → GND
AOUT   → GPIO 14
```

**Notes:**
- Use 3.3V power (not 5V)
- Capacitive sensors are more reliable than resistive
- Insert sensor 2-3 inches into soil

### Light Sensor (Photoresistor Module)

```
Module → ESP32-CAM
VCC    → 3.3V
GND    → GND
AO     → GPIO 15
```

**Notes:**
- Position sensor to receive ambient light
- Avoid direct sunlight on sensor
- Module usually includes voltage divider

### Water Level Sensor

```
Sensor → ESP32-CAM
VCC    → 3.3V
GND    → GND
AOUT   → GPIO 13
```

**Notes:**
- Mount vertically in water reservoir
- Keep electronics above water line
- Resistive sensors corrode over time

### DHT22 Temperature/Humidity Sensor

```
DHT22 → ESP32-CAM
VCC   → 3.3V
GND   → GND
DATA  → GPIO 12
```

**Important:** DHT22 requires a 10kΩ pull-up resistor between VCC and DATA pin.

```
        3.3V
         │
        ┌┴┐
        │ │ 10kΩ
        └┬┘
         │
    ┌────┴────┐
    │  DATA   │
    │  DHT22  │
    └─────────┘
```

### Relay Module (Pump and Light)

```
Relay Module → ESP32-CAM
VCC          → 5V
GND          → GND
IN1          → GPIO 2 (Pump)
IN2          → GPIO 4 (Light)

Relay Contacts:
COM → Power supply +
NO  → Pump/Light +
```

**Notes:**
- Relay module needs 5V power (not 3.3V)
- Use optocoupler-isolated relay module
- Connect pump/light between relay NO and ground
- Never connect high voltage AC directly to ESP32

### USB-to-Serial Adapter (For Flashing)

```
Adapter → ESP32-CAM
GND     → GND
5V      → 5V
TX      → U0R (RX)
RX      → U0T (TX)

For Flash Mode:
IO0     → GND (connect before power-on)
```

**Flashing Procedure:**
1. Connect IO0 to GND
2. Connect power (5V)
3. Upload firmware
4. Disconnect IO0 from GND
5. Press reset button

## Power Requirements

### Power Consumption

**Idle (WiFi off):**
- ESP32-CAM: 50mA
- Sensors: 10mA
- **Total: 60mA @ 5V = 0.3W**

**Active (WiFi on, camera capturing):**
- ESP32-CAM: 300mA
- Camera: 200mA
- Sensors: 10mA
- **Total: 510mA @ 5V = 2.5W**

**Peak (pump running):**
- ESP32-CAM: 300mA
- Pump: 200-500mA (depends on pump)
- **Total: 500-800mA @ 5V = 2.5-4W**

### Power Supply Recommendations

**USB Power Supply:**
- Minimum: 5V 1A (1000mA)
- Recommended: 5V 2A (2000mA)
- Use quality power supply (cheap ones cause brownouts)

**Battery Operation:**
- 18650 Li-ion battery (3.7V 3000mAh)
- With 5V boost converter
- Runtime: ~6-8 hours (depends on usage)

**Solar Power:**
- 5V 2W solar panel
- 18650 battery with charge controller
- Suitable for outdoor deployment

### Power Issues

**Symptoms of insufficient power:**
- Random reboots
- Camera initialization failures
- WiFi disconnections
- Brownout detector triggers

**Solutions:**
- Use quality 2A power supply
- Add 1000µF capacitor near ESP32-CAM power pins
- Use separate power supply for pump
- Shorten USB cable (voltage drop)

## Assembly Tips

### General Tips

1. **Test components individually** before final assembly
2. **Use breadboard** for prototyping
3. **Label all wires** with tape and marker
4. **Take photos** during assembly for reference
5. **Check polarity** before connecting power

### Soldering Tips

1. **Solder header pins** to ESP32-CAM for easier connections
2. **Use heat shrink** on exposed connections
3. **Strain relief** on all cables
4. **Test continuity** with multimeter

### Enclosure Tips

1. **Weatherproof** if used outdoors (IP65+ rating)
2. **Ventilation** to prevent condensation
3. **Cable glands** for wire entry
4. **Transparent window** for camera
5. **Access port** for USB programming

### Sensor Placement

**Soil Moisture:**
- Insert 2-3 inches deep
- Away from watering point
- Representative of pot moisture

**Light Sensor:**
- Above plant canopy
- Facing upward
- Away from direct LED light

**DHT22:**
- Above soil level
- Good air circulation
- Shaded from direct sun

**Water Level:**
- Vertical in reservoir
- Electronics above water
- Secure mounting

### Wiring Best Practices

1. **Color coding:**
   - Red: 5V power
   - Orange: 3.3V power
   - Black: Ground
   - Other colors: Signals

2. **Wire management:**
   - Bundle wires with zip ties
   - Separate power and signal wires
   - Leave slack for movement

3. **Connections:**
   - Solder for permanent installations
   - Dupont connectors for prototyping
   - Heat shrink all solder joints

## Troubleshooting Hardware

### ESP32-CAM won't boot

- Check 5V power supply (must be stable)
- Verify GND connection
- Remove IO0-to-GND connection
- Press reset button

### Camera not working

- Check camera ribbon cable connection
- Verify camera is OV2640 (not OV5640)
- Ensure PSRAM is enabled in firmware
- Check for GPIO conflicts

### Sensors reading incorrectly

- Verify 3.3V power to sensors
- Check ADC pin connections
- Calibrate sensors through web portal
- Test sensors with multimeter

### Relay not switching

- Check 5V power to relay module
- Verify GPIO connections (2 and 4)
- Test GPIO with LED
- Check relay module type (active HIGH/LOW)

### DHT22 not reading

- Verify 10kΩ pull-up resistor
- Check 3.3V power
- Verify GPIO 12 connection
- Try different DHT22 sensor

## Schematic

See `docs/growmate-schematic.png` for complete wiring diagram.

## Support

For more help:
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues
- [DEVELOPMENT.md](DEVELOPMENT.md) - Flashing and debugging
- [GitHub Issues](https://github.com/FarelRA/esp32-growmate-pods/issues)
