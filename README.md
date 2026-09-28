# GrowMate Pods (ESP32-CAM, indoor)

Indoor plant pod: an ESP32-CAM with OV2640 camera monitors a single
plant pot plus its water tank and drives two actuators — a 5 V micro
pump and a full-spectrum LED strip. Six sensing channels (tank level,
light, soil moisture, humidity, temperature, camera) and two command
channels (pump timed in ms, latched grow light). Sense → POST → the
server commands pump / light (never both at once — server-paced
interlock) and serves JPEG stills.

Status: pre-alpha. Device↔server contract is frozen at v1; hardware
is carrier rev A. The matching outdoor unit lives in
`rpi-growmate-tech` (same v1 contract, different iron).

## Features

- Raw-only telemetry (ADC codes + native DHT) with server-side
  calibration; camera JPEG upload on a 15-minute period.
- Timed pump doses (30 s cap, dry-tank interlock) and latched grow
  light (24 h failsafe auto-off), both server-commanded.
- HTTPS OTA with semver gating; MAC-birth identity + server claim flow.
- Full KiCad hierarchy, BOM, and release PDFs in [`schematic/`](schematic/)
  ([pods.pdf](schematic/mfg/pods.pdf)); net-by-net text reference in
  [`docs/04-wiring.md`](docs/04-wiring.md).

## Requirements

- Hardware: AI-Thinker ESP32-CAM module, carrier PCB (see schematic),
  5 V micro pump, 20 cm LED strip, probes, DHT22, BLP673 LiPo pack.
- Host: Python 3 + [PlatformIO](https://platformio.org/) (`pip install
  platformio`), plus a USB-UART adapter for flashing (the module has
  no auto-reset circuit).
- Optional for schematics: KiCad 10 CLI + Inkscape (see
  [`tools/sch_plot.sh`](tools/sch_plot.sh)).

## Installation

```bash
pip install platformio
pio run                    # build .pio/build/growmate-pods/firmware.bin
```

## Usage

Flash (hold BOOT at power-on for download mode, release + reset after):

```bash
pio run --target upload    # uses tools/flash.sh: no_reset/no-stub, 115200
pio device monitor         # or: tools/monitor.sh
```

Onboard: join AP `GrowMate-<last6 of device ID>` with password
`GrowMate-<last6 of WiFi MAC>` from the manufacturing claim sheet
(never the boot log) → `http://192.168.4.1` → home WiFi. Then
calibrate each probe (raw ends stored server-side). Full procedure:
[`docs/07-operations.md`](docs/07-operations.md).

## Tests

```bash
tools/test_all.sh          # Unity suite (PlatformIO native, no hardware)
                           # + host contract checks
```

116 Unity tests, ~98% line coverage of firmware logic
(`pio test -e native` for detail). If the suite and the device ever
disagree, the device wins — then fix the test the same day.

## Docs

`00-overview`, `01-pinout`, `02-power`, `03-sensors-actuators`,
`04-wiring`, `05-firmware`, `06-api`, `07-operations`,
`08-server-changes` — in [`docs/`](docs/). Start at
[`docs/00-overview.md`](docs/00-overview.md); bring-up order lives in
`04-wiring.md`; the server team's contract is `06-api.md` +
`08-server-changes.md`.

## Roadmap

- First field units on rev A carriers (soak + calibration validation).
- Server backend completing the `08-server-changes` contract
  (calibration store, XOR pacing, OTA hosting).
- Rev 2 notes collected in issues: real power-path charger
  (MCP73871 or load-share), probe ESD/TVS.

## Contributing

Private pre-alpha: coordinate before large changes. Rules that keep
the project consistent:

- Tests must stay green (`tools/test_all.sh`); new behavior needs tests.
- Docs change with code — pinout, wiring, power, and API docs are
  normative, not commentary.
- SKiDL (`schematic/pods.py`) is the hardware source of truth; the
  placed sheets follow it via `tools/sch_gate.sh`, never hand edits.

## License

GPLv3 — see LICENSE.
