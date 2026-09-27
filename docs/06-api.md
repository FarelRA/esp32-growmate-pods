# Server contract (Pods)

Two endpoints, canonical versioned paths. Base URL, firmware
version, and token are build-time constants (`src/app_build_config.h`);
`deviceId` is the effective identity (server-claimed pod ID persisted in
NVS, else the build default) plus the immutable MAC `hardwareId`.

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
  "hardwareId": "7C87CE1A2B3C",
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
| `deviceId` | string | effective identity: server-assigned pod ID after claiming, else build default; also the allowlist key |
| `hardwareId` | string | birth identity: uppercase WiFi-MAC hex, always sent, never changes; claim/review key for unclaimed units |
| `firmwareVersion` | string | semver, e.g. `"2.0.0"` |
| `snapshotId` | string | `"B<bootCount>-<seq>"`, unique per sample; server dedup key |
| `ageMs` | int | `>= 0`, sampling-to-POST latency (includes WiFi join, up to ~12 s, by design); server reconstructs sample time as `receivedAt - ageMs` |
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
| `{kind: "pump", durationMs, id}` | `0 < durationMs <= 30000` (finite) AND water channel readable AND non-empty `id` (< 32 chars) | pump FET on for `durationMs` (one-shot, 30 s cap); `id` echoed in `acceptedCommandIds` |
| `{kind: "light", enabled, id}` | `enabled` is bool or finite number (0 = off, nonzero = on) AND non-empty `id` (< 32 chars) | latch grow light; `id` echoed in `acceptedCommandIds` |

`id` is a server-generated string echoed verbatim and is REQUIRED.
Id-less or overlong-id commands are ignored (an applied-but-unackable
command would leave server and device permanently disagreed).

### Config push

`config.rev` is an int, strictly increasing. `config.reportIntervalSec`
is 10–3600 s. The device persists rev + interval (NVS v1), applies the
interval to the sensor cycle, and echoes `appliedConfigRev` on the next
POST. Stale revs (`rev <= appliedConfigRev`) are ignored;
out-of-range intervals are clamped to 10–3600 s (rev still advances).
The camera period stays build-time (900 s).

## POST camera — when due, `Content-Type: image/jpeg`

Headers `Authorization` (when configured) + `X-Device-Id: <deviceId>`,
body = raw JPEG (typically 20–100 KB, 45 s timeout). Any 2xx = success;
a failed frame retries at the next due slot.

## Status codes

| Status | Device behavior |
|---|---|
| 2xx | ok; apply commands + config |
| 429 | wait `Retry-After` (cap 60 s), then skip to next cycle; never retried in-cycle, never counted |
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

## Claim + OTA (live)

- Claim: answer any sensor POST with `claim: {deviceId: "<pod-id>"}`
  (`[A-Za-z0-9_-]`, ≤31 chars). The device validates, persists (NVS),
  and reports the new `deviceId` from the next POST; the onboarding AP
  name follows it. Repeat sends are no-ops. See `08-server-changes` §10.
- OTA: offer with `minFirmware: "x.y.z"` + `firmwareUrl: "https://…"`.
  The device upgrades only when the offer is newer than its own build,
  never mid pump-dose, then restarts into the new image (dual OTA slots,
  1.5 MB each — images must fit). No rollback slot: keep the previous
  binary to re-offer on regression. A latched grow light goes dark
  across the reboot; the server should refresh the latch afterwards.

## Test against a stub

```bash
curl -X POST "$SENSOR_URL" -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer TEST-TOKEN' -d \
  '{"deviceId":"TEST01","hardwareId":"7C87CE1A2B3C","firmwareVersion":"2.0.0","snapshotId":"B1-1", \
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

## Implementation notes (verified in `src/`, current firmware)

- Posts to the compiled `/api/v1/sensors` and `/api/v1/camera`
  (`APP_SENSOR_API_URL`, `APP_CAMERA_API_URL`); `Authorization` sent only
  when a token is compiled in, otherwise allowlist on `deviceId`.
- Analog entries carry `raw` only (no device percent math, no endpoints
  in firmware); DHT entries carry native floats. Unavailable omitted.
- Envelope carries `deviceId` (+`hardwareId`), `snapshotId`, `ageMs`,
  `appliedConfigRev`, `acceptedCommandIds`, `health` (NVS v1).
  Report interval is server-pushed (rev discipline); camera stays 900 s.
- Pump needs `0 < durationMs <= 30000` plus a readable water channel and
  a non-empty `id`; light takes bool or finite number plus `id`;
  accepted ids echo next POST.
- 2xx ok; 429 waited out then skipped to next cycle (no count);
  other 4xx fatal-no-count; 5xx/transport counted.
  Retries: 2 tries 1.5 s apart per cycle; fatal/429 break after the first try.
- OTA + claim as above; light auto-offs after 24 h without a refresh.
