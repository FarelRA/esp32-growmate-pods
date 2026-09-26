# Operations: flash, onboard, calibrate, troubleshoot

## Flash

Wiring (USB-serial adapter, 5V-tolerant I/O at 3V3):

```
adapter GND → ESP GND | adapter 5V → ESP 5V
adapter TX  → ESP U0R | adapter RX  → ESP U0T
IO0 → GND (only while powering on for download mode)
```

```bash
pip install platformio
pio run                    # build .pio/build/growmate-pods/firmware.bin
pio run --target upload    # tools/flash.sh: esptool, no_reset/no-stub, 115200
pio device monitor           # 115200, RTS/DTR held (see monitor_* in ini)
```

Remove the IO0–GND link and reset to boot normally. `upload_port` is
`/dev/ttyACM0` (`platformio.ini`); override with
`pio run --target upload --upload-port /dev/ttyUSB0` as needed.
Single 5V source while flashing: the adapter powers the module, so
disconnect the pack/boost 5V feed first — never drive the +5V rail from
two supplies at once.
First boot captures a camera frame immediately — use it as the install
photo check.

## Onboard

1. Join AP `GrowMate-IAET01` (password `growmate`). The name derives as
   `GrowMate-` + the last 6 chars of `APP_DEVICE_ID` (full ID if ≤6 chars).
2. Open `http://192.168.4.1`, submit home WiFi SSID/pass.
3. Device reboots into station mode and starts the 15 s cycle.
4. After 5 straight failures the portal reopens by itself.

## Calibrate (per probe, per install)

For each analog channel record the raw log value at both ends and set
the matching `APP_*_RAW_*` pair in `src/app_build_config.h`, then reflash:

- water: probe in air (EMPTY) vs fully submerged (FULL). Probe moved
  from GPIO12/ADC2 (switched, 100R) to GPIO33/ADC1 (continuous 3V3, no
  series R) — old `APP_WATER_RAW_*` values are invalid, recalibrate
  from scratch.
- soil: probe in air (DRY) vs in saturated soil (WET).
- light: covered module (DARK) vs grow light at canopy (BRIGHT).
  Scale is inverted (dark ≈ 4095).

## Symptom table

| Symptom | First check |
|---|---|
| Won't boot after wiring | GPIO12 (MTDI) must float LOW — confirm U1-6 is unconnected. Then IO0–GND link left on. |
| Boot loop / brownout reset | 5V rail under load; cable, pack charge, C2 solder. |
| Camera init fails | ribbon seating; PSRAM enabled; no wire on camera pins (firmware aborts tell you which). |
| ADC reads 0/4095 stuck | wrong `APP_*_RAW_*` ends; sensor on 5V instead of 3V3; R6/C1 solder. |
| DHT NaN | 4.7k R1 present? 3V3 power? leads < 1 m? pump EMI — reroute. |
| Pump never runs | gate node 0/3.3V on command? flyback orientation (D1 K→+5V)? separate pump supply ground shared? |
| Light never latches | GPIO4 gate drive? strip polarity (+5V common)? server actually sending `light` command? |
| Portal never opens | configured AP already provisioned — erase flash to force (`pio run --target erase`). |
| Upload 4xx/5xx | URL constants, server allowlist for the compile-time `deviceId`. |

## Erase / recover

```bash
pio run --target erase && pio run --target upload
```
