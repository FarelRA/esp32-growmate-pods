# Firmware map

ESP-IDF 5.5, PlatformIO, C17. Entry `app_main` in `src/main.c`.

## Cycle (every effective report interval, default 15 s)

```
sensors_read_all()          # ADC burst + DHT, WiFi still STOPPED
network_manager_start_station()   # connect, 12 s timeout
upload_sensor_snapshot()    # POST sensors, 2 tries, apply commands + config + claim
upload_camera_image()       # if due (build-time 900 s)
ota_service_run_if_needed() # upgrade + restart when server offers newer build
network_manager_stop()      # WiFi fully stopped again
delay_with_housekeeping()   # 250 ms ticks service the pump timer
```

The sensor interval is the server-pushed `reportIntervalSec`
(10–3600 s, persisted in NVS v1, echoed as `appliedConfigRev`) when
one has been received, else the build default
`APP_SENSOR_INTERVAL_SEC` (15 s). The camera period stays build-time
(`APP_CAMERA_INTERVAL_SEC`, 900 s) and is not server-configurable.

**ADC-before-WiFi is load-bearing.** ADC2 (GPIO13/14) is shared with
the WiFi driver on ESP32 classic: any ADC2 read between `esp_wifi_start`
and `esp_wifi_stop` fails. Water lives on ADC1 (GPIO33, WiFi-safe) but
soil/light keep the rule, so the order above is the entire workaround.
Do not "optimize" it into a persistent connection without moving to an
external ADC.

Camera init/deinit brackets each capture (PSRAM: SVGA/Q12, else VGA/Q14).
`loops_since_camera` starts at one full period so the first boot captures
immediately (intended: instant visual check after install).

5 consecutive failures (WiFi, upload, camera, or total sensor
failure) reopen the onboarding AP portal instead of wedging.

## Files

| File | Owns |
|---|---|
| `board_profile.{h,c}` | THE pin map (must match 01-pinout). Camera bus + conflict check. |
| `sensors.{h,c}` | 8-sample ADC average (ADC2 soil/light, ADC1 water), raw-only capture, DHT poll, continuous-3V3 water probe, camera-bus abort guard. |
| `actuators.{h,c}` | GPIO2/4 init (no internal pull — external network owns boot level), one-shot timed pump (30 s contract cap), latched light with 24 h failsafe auto-off, conflict abort guard. |
| `api_client.{h,c}` | sensor JSON POST (raw envelope + `health` + `acceptedCommandIds`, Bearer token when configured) with command/config parse (`pump durationMs` ≤ 30 s with water check, `light` bool-or-number), JPEG POST with `X-Device-Id` + token. |
| `app_config.{h,c}` | NVS `growmate/settings` v1: WiFi SSID/pass + provisioned flag + assigned device ID + boot count + applied config rev + report interval. Any size/version mismatch resets to defaults (pre-alpha: no migration, re-provision + reclaim). |
| `device_identity.{h,c}` | MAC birth ID, effective ID, derived onboarding AP credentials. |
| `ota_service.{h,c}` | semver compare + HTTPS OTA + restart. |
| `network_manager.{h,c}` | STA connect/scan + onboarding AP + stop. |
| `onboarding.{h,c}` | blocking portal at 192.168.4.1: `GET /`, `GET /api/config`, `POST /api/config{wifiSsid,wifiPassword}`, `GET /api/networks`. AP name `GrowMate-<last6 of device ID>`, WPA2 password `GrowMate-<last6 of MAC>` from the manufacturing claim sheet (never the log). Plaintext HTTP — commission in a trusted location (see 07-operations). |
| `camera_service.{h,c}` | PWDN pulse, init/deinit, capture. |
| `api_rules.{h,c}` | pure validators (ids, dose window, rev discipline, clamps, charsets). |
| `api_parse.{h,c}` | cJSON traversal for commands/config/claim/OTA + id ring. |
| `ota_logic.{h,c}` | semver parse/compare + upgrade gate. |
| `onboarding_rules.{h,c}` / `network_rules.{h,c}` | portal field copy, AP/station/scan guards. |
| `camera_logic.{h,c}` / `main_logic.{h,c}` | frame pick, interval scheduling. |
| `app_build_config.h` | device ID, firmware version, API URLs, token, sensor/camera intervals. Per-device values: edit + reflash. |

Unity tests live in `test/` (PlatformIO native env, no hardware);
`tools/test_all.sh` runs them. See `tools/README.md`.

## Configuration tiers

- **Build time** (`app_build_config.h`): identity, token, URLs,
  intervals — reflash to change.
- **Runtime**: WiFi credentials via portal (NVS); assigned device ID via
  server claim (NVS v1, effective immediately); report interval +
  config rev via server push (NVS v1, echoed as `appliedConfigRev`).
  Calibration lives server-side ([03-sensors-actuators](03-sensors-actuators.md)).

## Known firmware limits

- Auth: `Authorization: Bearer` is sent when a token is compiled in;
  until then the server must allowlist device IDs (`deviceId` in the
  JSON body and `X-Device-Id` on camera POSTs are identifiers, not
  credentials).
- Status handling: 2xx ok; 429 honors `Retry-After`; other 4xx are
  fatal-for-request (no retry, NO portal reopen); 5xx/transport errors
  retry with backoff.
- `deviceId`/`firmwareVersion` are compile-time defaults; the server
  assigns the pod ID via the claim flow (`claim: {deviceId}`, persisted
  NVS v1, effective immediately). Birth identity is always the WiFi MAC
  (`hardwareId`).
- OTA is live: dual 1.5 MB slots, server offers `minFirmware` +
  `firmwareUrl`, device upgrades when newer (never mid pump-dose) and
  restarts. No rollback slot — keep the previous binary to re-offer.
  A latched light goes dark across the reboot; the server refreshes it.
- Remote diagnosis beyond the `health` block is serial-only
  (`pio device monitor`, 115200).
- Portal `httpd` worker task runs with an 8 KB stack (set in code at
  server start; the handlers hold multi-KB buffers).
- Watchdog: `app_main` is subscribed to the task watchdog (60 s window).
  Every blocking path (WiFi join, HTTP POSTs, OTA download, portal,
  sensor sampling, interval wait) feeds it at least once per second, so
  a trip means a genuine wedge, not a slow network.
- No deep sleep: ~60 mA idle on USB/pack. Battery runtime ≈ pack/average
  draw; size accordingly.

## Dependencies

- `src/idf_component.yml` declares `esp-idf-lib/dht ^1.2.0` (DHT22 reads —
  stateless, no ISR service) and `espressif/esp32-camera ^2.1.8`. The
  component manager downloads them (plus `esp_idf_lib_helpers` and
  `esp_jpeg`) into `managed_components/` on every build. There is no
  `components/` dir anymore.
  NOTE: under PlatformIO only the **main-component manifest** (`src/`) is
  honored — a project-root `idf_component.yml` is silently ignored, so deps
  must stay declared in `src/`.
- The old vendored `esp32-camera` carried a local `ll_cam.c` patch (fail
  fast on GPIO-ISR-service install errors other than ALREADY_INSTALLED).
  Upstream 2.1.8 ignores the return value instead — behaviorally identical
  in the working path (deinit removes the handler but keeps the shared
  service, so re-init hits ALREADY_INSTALLED and proceeds). Patch dropped
  as not load-bearing; nothing else installs the ISR service anymore since
  the DHT driver went stateless.
- `esp_jpeg` is pulled transitively by `esp32-camera`'s manifest but never
  called directly — harmless payload, kept (removing it means forking the
  camera manifest).
