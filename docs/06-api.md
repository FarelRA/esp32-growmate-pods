# Server contract (Pods)

Two endpoints, canonical versioned paths. Base URL, device ID, firmware
version, and token are build-time constants (`src/app_build_config.h`).

| Channel | Method + path | Body |
|---|---|---|
| sensors | `POST /api/v1/sensors` | `application/json` envelope, every report interval (default 15 s) |
| camera | `POST /api/v1/camera` | `image/jpeg` raw frame when due (default 900 s) |

## Auth

`Authorization: Bearer <token>` on both endpoints when a token is
configured at build time. Camera POSTs additionally carry
`X-Device-Id: <deviceId>`. A `401` means unknown, expired, or missing
token: do not retry the request; keep sensing and surface the device
for re-provisioning.

## POST sensors — request envelope

```json
{
  "deviceId": "IAET01",
  "firmwareVersion": "2.0.0",
  "snapshotId": "B12-345",
  "ageMs": 1800,
  "appliedConfigRev": 7,
  "sensors": [
    {"kind": "soil", "unit": "%", "raw": 2048},
    {"kind": "light", "unit": "%", "raw": 3200},
    {"kind": "water", "unit": "%", "raw": 3800},
    {"kind": "temperature", "unit": "C", "value": 24.6},
    {"kind": "air", "unit": "%", "value": 61.2}
  ],
  "acceptedCommandIds": ["cmd-9f2"],
  "currentState": {"pumpEnabled": false, "lightEnabled": true},
  "health": {"heapFree": 81234, "rssi": -61, "uptimeS": 86412,
             "bootCount": 12, "resetReason": "power-on",
             "dhtFails": 1, "adcFails": 0}
}
```

| Field | Type | Bounds / notes |
|---|---|---|
| `deviceId` | string | build-time identity, also the allowlist key |
| `firmwareVersion` | string | semver, e.g. `"2.0.0"` |
| `snapshotId` | string | `"B<bootCount>-<seq>"`, unique per sample; server dedup key |
| `ageMs` | int | `>= 0`, sampling-to-POST latency; server reconstructs sample time as `receivedAt - ageMs` |
| `appliedConfigRev` | int | `>= 0`, last config rev applied (0 = none) |
| `sensors[]` | array | raw-only analog + native DHT entries (kinds below); unavailable sensors omitted, never null |
| `acceptedCommandIds[]` | string[] | ids of commands applied since the previous POST (empty is valid) |
| `currentState` | object | `{pumpEnabled: bool, lightEnabled: bool}` latch state at POST time |
| `health` | object | `{heapFree: bytes int, rssi: dBm int, uptimeS: int, bootCount: int, resetReason: string, dhtFails: int, adcFails: int}` cumulative counters |

Sensor kinds:

| kind | Shape | Notes |
|---|---|---|
| `soil`, `light`, `water` | `{kind, unit: "%", raw: 0–4095}` | RAW-ONLY. No percent math on device; server maps raw→percent via stored per-probe ends. Light scale inverted (dark ≈ 4095). |
| `temperature` | `{kind, unit: "C", value: float}` | native DHT22 °C, no `raw` |
| `air` | `{kind, unit: "%", value: float}` | native DHT22 %RH, no `raw` |

## POST sensors — response

```json
{"commands": [{"kind": "pump", "durationMs": 5000, "id": "cmd-9f3"},
              {"kind": "light", "enabled": true, "id": "cmd-9f4"}],
 "config": {"rev": 8, "reportIntervalSec": 30}}
```

Both keys optional; either may be absent. Unknown command `kind`
values are ignored.

### Command validation (device rule)

| Command | Accepted iff | Effect |
|---|---|---|
| `{kind: "pump", durationMs, id?}` | `0 < durationMs <= 30000` AND water channel readable | pump FET on for `durationMs` (one-shot, 30 s cap); `id` echoed in `acceptedCommandIds` |
| `{kind: "light", enabled, id?}` | `enabled` is bool or number (0 = off, nonzero = on) | latch grow light; `id` echoed in `acceptedCommandIds` |

`id` is a server-generated string echoed verbatim. Commands without
`id` are applied but cannot be acked — always send `id`.

### Config push

`config.rev` is an int, strictly increasing. `config.reportIntervalSec`
is 10–3600 s. The device persists rev + interval (NVS v5), applies the
interval to the sensor cycle, and echoes `appliedConfigRev` on the next
POST. Stale revs (`rev <= appliedConfigRev`) and out-of-range intervals
are ignored. The camera period stays build-time (900 s).

## POST camera — when due, `Content-Type: image/jpeg`

Headers `Authorization` (when configured) + `X-Device-Id: <deviceId>`,
body = raw JPEG (typically 20–100 KB, 45 s timeout). Any 2xx = success;
a failed frame retries at the next due slot.

## Status codes

| Status | Device behavior |
|---|---|
| 2xx | ok; apply commands + config |
| 429 | honor `Retry-After`, back off, retry next cycle |
| other 4xx (incl. 401) | fatal for the request: no retry, NO portal reopen; keep sensing, surface for operator |
| 5xx / transport error | retryable with backoff; counts toward the failure threshold |

## Retry / ack / dedup / time correction

- Telemetry is at-least-once: the server dedups on `snapshotId`
  (`B<boot>-<seq>`, one id per sample, never reused).
- Commands are at-least-once the other way: the server re-delivers any
  command whose `id` is absent from `acceptedCommandIds` until it is
  acked or superseded. Command handling is idempotent on both sides.
- The device has no RTC: `ageMs` is the only sample-timing truth.
  Server timestamp = `receivedAt - ageMs`; never use arrival time raw.
- `currentState` + `acceptedCommandIds` together let the server
  distinguish "command lost" from "command applied, ack lost".

## Reserved OTA fields (future, ignored by the device today)

`minFirmware` (string) and `firmwareUrl` (string) are reserved response
fields for a future OTA flow. The device logs and ignores them. There
is no OTA in firmware v2.0.0 (single-app partition, no rollback slot).

## Test against a stub

```bash
curl -X POST "$SENSOR_URL" -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer TEST-TOKEN' -d \
  '{"deviceId":"TEST01","firmwareVersion":"2.0.0","snapshotId":"B1-1", \
    "ageMs":100,"appliedConfigRev":0, \
    "sensors":[{"kind":"soil","unit":"%","raw":2048}], \
    "acceptedCommandIds":[], \
    "currentState":{"pumpEnabled":false,"lightEnabled":false}, \
    "health":{"heapFree":80000,"rssi":-60,"uptimeS":10,"bootCount":1, \
              "resetReason":"power-on","dhtFails":0,"adcFails":0}}'
curl -X POST "$CAM_URL" -H 'Content-Type: image/jpeg' \
  -H 'X-Device-Id: TEST01' -H 'Authorization: Bearer TEST-TOKEN' \
  --data-binary @test.jpg
```

## Firmware v2.0.0 implementation notes (verified in `src/`)

- Posts to the compiled unversioned paths `/api/sensors` and
  `/api/camera` (`APP_SENSOR_API_URL`, `APP_CAMERA_API_URL`); no
  `Authorization` header is sent. Until token auth is flashed in, the
  server must accept the `deviceId` allowlist and serve both the
  unversioned and the `/api/v1` path styles.
- Analog entries carry both `value` (on-device percent from
  `APP_*_RAW_*`, `lround`) and `raw`; temperature `value` is rounded
  to int. The server treats `raw` as authoritative and ignores `value`.
- The envelope carries only `deviceId`, `firmwareVersion`, `sensors`,
  `currentState`. No `snapshotId` / `ageMs` / `appliedConfigRev` /
  `acceptedCommandIds` / `health`, no Bearer token, no config-push
  handling (NVS v4, WiFi-only). Intervals are fixed at build time:
  sensors 15 s, camera 900 s.
- Command parse accepts a pump command for any numeric
  `durationMs > 0` (no 30 s clamp, no water-readability check) and a
  light command only for strict JSON bool. Unknown kinds ignored.
  No ids are echoed.
- Every non-2xx is retried twice (1.5 s apart) and counts toward the
  5-consecutive-failure portal reopen — including 4xx. `Retry-After`
  is not honored.
