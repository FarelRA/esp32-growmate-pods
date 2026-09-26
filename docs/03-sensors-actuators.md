# Sensors and actuators

Reference designators match the parts list in [04-wiring](04-wiring.md).

## Analog inputs (all 12-bit, 12 dB attenuation, 8-sample average)

| Net | Pin | Module power | Notes |
|---|---|---|---|
| SOIL_AO (J2-2) | GPIO13 ADC2_CH4 | continuous 3V3 | capacitive v1.2, 3V3-safe. Calibrate dry/wet raw. |
| LIGHT_AO (J3-2) | GPIO14 ADC2_CH6 | continuous 3V3 | photodiode module AO → R6 1k → GPIO, C1 100 n to GND. Inverted scale (dark ≈ 4095). |
| WATER_AO (J1-2) | GPIO12 ADC2_CH5 | **switched**: GPIO33 → R7 100R → J1-1, HIGH ~60 ms before the ADC burst, LOW right after (~2% duty at 15 s period). | resistive probe. R9 10k PD to GND. Switching kills electrolysis corrosion and keeps MTDI LOW at boot. |

Firmware mapping raw→percent lives in `src/sensors.c`
(`read_percent_measurement`) with endpoints from `src/app_build_config.h`
(`APP_*_RAW_*`). Recalibrate per probe; defaults are placeholders.

## DHT22 (U2)

VDD 3V3, DATA → GPIO15, GND. Pin 3 is No-Connect per the DHT22
datasheet and stays open — not a wiring omission. Driver is the managed
`esp-idf-lib/dht` component (stateless reads, no ISR service, no
init/deinit dance). R1 4.7k pull-up (also the MTDO-HIGH
strapping pull). 100 nF across VDD/GND at the sensor. Short leads; keep
away from pump wiring. Polled with 2 attempts; failures mark temperature
and humidity unavailable (`NaN`) rather than blocking the cycle.

## Pump (M1) and grow light (D2) — low-side N-MOSFETs

| | Pump | Light |
|---|---|---|
| GPIO | GPIO2 → R2 220R → gate node | GPIO4 → R4 220R → gate node |
| Pulldown | R3 100k gate–GND | R5 100k gate–GND |
| FET | Q1 IRLZ44N (logic-level, Vgs(th) < 2 V), source GND, drain PUMP_LO | Q2 same, drain LED_LO |
| Load | M1 between +5V and PUMP_LO | LED strip between +5V and LED_LO |
| Protection | D1 1N5819 flyback, cathode +5V, anode PUMP_LO | strip modules are LED+resistor; no flyback needed |

Behavior (`src/actuators.c`): pump command `{durationMs}` turns the FET on
and auto-offs via `esp_timer` (serviced every 250 ms in
`delay_with_housekeeping`); light command latches on/off. Both report back
in `currentState`.

GPIO4 also drives the onboard flash LED (voltage-divider fed, glows dimly
even "off"). Kept deliberately: the camera flash still works. If the glow
bothers a dark grow cycle, remove R11 on the module (see AI-Thinker
schematic) — do not rewire GPIO4.
