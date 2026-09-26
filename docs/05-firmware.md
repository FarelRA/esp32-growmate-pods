# Firmware map

ESP-IDF 5.5, PlatformIO, C17. Entry `app_main` in `src/main.c`.

## Cycle (every `APP_SENSOR_INTERVAL_SEC`, default 15 s)

```
sensors_read_all()          # ADC burst + DHT, WiFi still STOPPED
network_manager_start_station()   # connect, 12 s timeout
upload_sensor_snapshot()    # POST sensors, 2 tries, apply commands
upload_camera_image()       # if due (default every 900 s)
network_manager_stop()      # WiFi fully stopped again
delay_with_housekeeping()   # 250 ms ticks service the pump timer
```

**ADC-before-WiFi is load-bearing.** ADC2 (GPIO13/14) is shared with
the WiFi driver on ESP32 classic: any ADC2 read between `esp_wifi_start`
and `esp_wifi_stop` fails. Water lives on ADC1 (GPIO33, WiFi-safe) but
soil/light keep the rule, so the order above is the entire workaround.
Do not "optimize" it into a persistent connection without moving to an
external ADC.

Camera init/deinit brackets each capture (PSRAM: SVGA/Q12, else VGA/Q14).
`loops_since_camera` starts at one full period so the first boot captures
immediately (intended: instant visual check after install).

5 consecutive failures (WiFi, upload, or camera) reopen the onboarding AP
portal instead of wedging.

## Files

| File | Owns |
|---|---|
| `board_profile.{h,c}` | THE pin map (must match 01-pinout). Camera bus + conflict check. |
| `sensors.{h,c}` | 8-sample ADC average (ADC2 soil/light, ADC1 water), raw→percent, DHT poll, continuous-3V3 water probe, camera-bus abort guard. |
| `actuators.{h,c}` | GPIO2/4 init (no internal pull — external network owns boot level), timed pump, latched light, conflict abort guard. |
| `api_client.{h,c}` | sensor JSON POST + command parse (`pump durationMs`, `light enabled`), JPEG POST with `X-Device-Id`. |
| `app_config.{h,c}` | NVS `growmate/settings` v4: WiFi SSID/pass + provisioned flag only. |
| `network_manager.{h,c}` | STA connect/scan + onboarding AP + stop. |
| `onboarding.{h,c}` | blocking portal `GrowMate-IAET01` / `growmate` at 192.168.4.1: `GET /`, `/api/networks`, `POST /api/config{ssid,pass}`. |
| `camera_service.{h,c}` | PWDN pulse, init/deinit, capture. |
| `app_build_config.h` | device ID, firmware version, API URLs, intervals, calibration endpoints. Per-device values: edit + reflash. |

## Configuration tiers

- **Build time** (`app_build_config.h`): identity, URLs, intervals,
  calibration — reflash to change.
- **Runtime** (NVS via portal): WiFi credentials only. The old docs
  claimed portal-editable device IDs/URLs/calibration — that was never
  implemented and is removed from this doc set.

## Known firmware limits (honest list)

- No auth header yet (`X-Device-Id` only). Server must allowlist device IDs
  until auth lands.
- `deviceId`/`firmwareVersion` are compile-time constants, not per-flash
  provisioned. Per-device claiming is a server-side TODO.
- No deep sleep: ~60 mA idle on USB/pack. Battery runtime ≈ pack/average
  draw; size accordingly.

## Dependencies (all managed, nothing vendored)

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
