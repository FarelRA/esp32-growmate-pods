# Sensors and actuators

Reference designators match the parts list in [04-wiring](04-wiring.md).

## Analog inputs (all 12-bit, 12 dB attenuation, 8-sample average)

| Net | Pin | Module power | Notes |
|---|---|---|---|
| SOIL_AO (J2-2) | GPIO13 ADC2_CH4 | continuous 3V3 | capacitive v1.2, 3V3-safe. Calibrate dry/wet raw. |
| LIGHT_AO (J3-2) | GPIO14 ADC2_CH6 | continuous 3V3 | photodiode module AO → R6 1k → GPIO, C1 100 n to GND. Inverted scale (dark ≈ 4095). |
| WATER_AO (J1-2) | GPIO33 ADC1_CH5 | continuous 3V3 | resistive probe. R9 10k PD to GND (open probe reads ~0 = EMPTY). ADC1 is WiFi-safe. Continuous DC electrolyzes the traces in days–weeks (fertilizer water fastest) — treat it as a consumable: rinse/dry between tanks, recalibrate after moving, stock spares. No spare header pin exists rev1 for switched excitation. Strapping-safe at any water level by construction (GPIO12 unused). |

## Calibration (server-side)

The device reports raw ADC codes and owns no calibration endpoints.
Percent mapping lives on the SERVER, per probe, per install:

| Channel | Server stores | Scale |
|---|---|---|
| water | `EMPTY` (probe in air) vs `FULL` (fully submerged) raw | rising |
| soil | `DRY` (probe in air) vs `WET` (saturated soil) raw | rising |
| light | `DARK` (covered module) vs `BRIGHT` (grow light at canopy) raw | inverted (dark ≈ 4095) |

Procedure (see [07-operations](07-operations.md)): read the raw codes
from a telemetry POST at both ends, store them in the device's
server-side config, then sanity-check a mid-point reading. Moving a
probe or changing its supply invalidates its ends — recalibrate from
scratch. Storage and mapping rules are a server-team contract, detailed
in [08-server-changes](08-server-changes.md).

Firmware sends raw ADC codes only; percent mapping is server-side
(see the calibration section above).

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
and auto-offs via an authoritative `esp_timer` one-shot (plus a 250 ms
`actuators_tick` backup in `delay_with_housekeeping`); the contract caps
doses at 30 s (`0 < durationMs <= 30000`, water channel must read — see
[06-api](06-api.md)). Light command latches on/off with a 24 h failsafe
auto-off refreshed by re-sent "on". Both report back
in `currentState`, and command ids are echoed in `acceptedCommandIds`.

GPIO4 also drives the onboard flash LED (voltage-divider fed, glows dimly
even "off"). Kept deliberately: the camera flash still works. If the glow
bothers a dark grow cycle, remove R11 on the module (see AI-Thinker
schematic) — do not rewire GPIO4.
