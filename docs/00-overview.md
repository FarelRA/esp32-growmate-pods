# GrowMate Pods — overview

Indoor pod device. One ESP32-CAM (AI-Thinker, ESP32-S + OV2640) monitors a
single plant pot plus its water tank and drives two actuators. Six sensing
channels, two command channels.

| Direction | Channel | Sensor / actuator |
|---|---|---|
| sense | water level in tank (analog) | resistive probe |
| sense | light (analog) | photodiode module |
| sense | soil moisture (analog) | capacitive v1.2 |
| sense | humidity | DHT22 |
| sense | temperature | DHT22 |
| sense | camera | OV2640, JPEG upload |
| command | water | 5 V micro submersible pump, timed ms |
| command | lighting | full-spectrum LED strip, latched on/off |

Server loop (`POST /api/v1/sensors` → `{commands: [{pump durationMs},
{light enabled}]}`, `POST /api/v1/camera` JPEG) is specified in
[06-api](06-api.md); backend requirements live in
[08-server-changes](08-server-changes.md).

## Documents

| File | Content |
|---|---|
| [01-pinout](01-pinout.md) | BEST header-only pin map, strapping proof, forbidden pins |
| [02-power](02-power.md) | battery, charger, boost, rails, decoupling |
| [03-sensors-actuators](03-sensors-actuators.md) | per-channel circuits and part values |
| [04-wiring](04-wiring.md) | complete net-by-net wiring + parts list (text) and bring-up order |
| [05-firmware](05-firmware.md) | firmware map, ADC-before-WiFi rule, config |
| [06-api](06-api.md) | server contract |
| [07-operations](07-operations.md) | flash, onboard, calibrate, troubleshoot |
| [08-server-changes](08-server-changes.md) | backend requirements: auth, acks, calibration storage, config push, limits |

## Hardware source of truth

- [04-wiring](04-wiring.md) — the complete net list (every net, every
  node), parts list, wire table, bring-up order.
- [01-pinout](01-pinout.md) — pin map with strapping proof.
- `src/board_profile.c` — the pin map in code, plus a camera-bus
  conflict guard that aborts init on overlap (`sensors_init`,
  `actuators_init`). A boot abort here means a wiring bug, not a
  firmware bug.
