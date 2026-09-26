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
First provisioned boot captures a camera frame immediately (a fresh
device blocks in the portal first) — use it as the install photo check.

## Onboard

1. Join AP `GrowMate-<last6 of device ID>` (default `GrowMate-IAET01`).
   Password is per-device: `GrowMate-<last6 of WiFi MAC>`, printed in
   the boot log as `Onboarding AP …`. (No more shared password.)
2. Open `http://192.168.4.1`, submit home WiFi SSID/pass.
3. Device continues with the new settings (no reboot) into station mode
   and starts the 15 s cycle.
4. After 5 straight failures the portal reopens by itself (transport
   and 5xx errors; 4xx responses are fatal-for-request and never force
   the portal — see [06-api](06-api.md)).

Portal notes: the AP password is per-device (derived from the MAC, see
above) but the portal itself is plaintext HTTP — anyone in range can
sniff the home WiFi credentials you type. Commission in a trusted
location. Remote diagnosis beyond the `health` block
in telemetry is serial-only (`pio device monitor`, 115200).

## Calibrate (per probe, per install — server-side)

For each analog channel, read the raw codes from a telemetry POST at
both ends and store them in the device's SERVER-side config (mapping
rules in [08-server-changes](08-server-changes.md)):

- water: probe in air (EMPTY) vs fully submerged (FULL). No defaults
  are shipped — calibrate every install from scratch.
- soil: probe in air (DRY) vs in saturated soil (WET).
- light: covered module (DARK) vs grow light at canopy (BRIGHT).
  Scale is inverted (dark ≈ 4095).

The device holds no calibration endpoints — `raw` is authoritative and
the server owns the mapping. Moving a probe invalidates its stored ends.

## Soak test (before install)

Run the pod on pack power for at least one full camera period plus
several sensor cycles, with WiFi connected: camera init + capture +
JPEG upload is the peak-draw event (ESP + camera + WiFi, plus pump/LED
if commanded). Watch serial for brownout resets, camera-init failures,
and WiFi drops — all three mean the 5V rail sagged under load. Check
the server side too: one `snapshotId` per interval (no gaps, no
dupes), a frame per camera period, and `appliedConfigRev` tracking the
sent config rev.

## Symptom table

| Symptom | First check |
|---|---|
| Won't boot after wiring | GPIO12 (MTDI) must float LOW — confirm U1-6 is unconnected. Then IO0–GND link left on. |
| Boot loop / brownout reset | 5V rail under load; cable, pack charge, C2 solder. |
| Camera init fails | ribbon seating; PSRAM enabled; no wire on camera pins (firmware aborts tell you which). |
| ADC reads 0/4095 stuck | wrong server-side ends; sensor on 5V instead of 3V3; R6/C1 solder. |
| DHT NaN | 4.7k R1 present? 3V3 power? leads < 1 m? pump EMI — reroute. |
| Pump never runs | gate node 0/3.3V on command? flyback orientation (D1 K→+5V)? separate pump supply ground shared? |
| Light never latches | GPIO4 gate drive? strip polarity (+5V common)? server actually sending `light` command? |
| Portal never opens | configured AP already provisioned — erase flash to force (`pio run --target erase`). |
| Upload 4xx/5xx | URL/token constants, server allowlist for the effective `deviceId` (claimed pod ID, else build default); 4xx needs operator action (device will not portal-loop on it). |

## Limits

- OTA offers (`minFirmware` + `firmwareUrl`) upgrade the inactive slot
  and restart; no rollback slot, so keep the previous binary server-side
  for re-offer. The light latch does not survive a reboot.
- Pump doses cap at 30 s by contract; the server also clamps.
- Erasing NVS also clears the assigned device ID and boot count — the
  unit returns unclaimed and must be re-claimed by the server.
- Beyond the `health` telemetry block, diagnosis is serial-only.

## Erase / recover

```bash
pio run --target erase && pio run --target upload
```
