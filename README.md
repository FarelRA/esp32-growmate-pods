# GrowMate Pods (ESP32-CAM, indoor)

Indoor plant pod: ESP32-CAM + OV2640, 5 V micro pump + tank, LED strip,
water/light/soil analog sensors, DHT22, camera. Sense → POST → server
commands pump / light / both. See [docs/00-overview](docs/00-overview.md).

## Hardware rev1 (BEST header-only rotation)

- PUMP GPIO2, LIGHT GPIO4 (low-side IRLZ44N, 220R gate + 100k PD, 1N5819).
- SOIL GPIO13, LIGHT_AO GPIO14 (1k + 100n), WATER GPIO33 (ADC1, continuous
  3V3, 10k PD), DHT GPIO15 (4.7k PU). GPIO12 left unconnected (MTDI safety).
- No SD, PSRAM kept, no eFuse burn. Full proof: [docs/01-pinout](docs/01-pinout.md).
- Wiring: complete net-by-net text reference in
  [docs/04-wiring](docs/04-wiring.md) (no CAD files in this repo).

## Build and flash

```bash
pip install platformio
pio run                    # build
pio run --target upload    # IO0-GND at power-on, /dev/ttyACM0, 115200
pio device monitor
```

Adapter: GND→GND, 5V→5V, TX→U0R, RX→U0T. Remove IO0–GND, reset, join the
per-device AP (`GrowMate-<last6 of ID>`, password `GrowMate-<last6 of MAC>`
from the boot log) → `http://192.168.4.1` → home WiFi.
Details: [docs/07-operations](docs/07-operations.md).

## Docs

00-overview, 01-pinout, 02-power, 03-sensors-actuators, 04-wiring,
05-firmware, 06-api, 07-operations, 08-server-changes — in [docs/](docs/).

## License

GPLv3 — see LICENSE.
