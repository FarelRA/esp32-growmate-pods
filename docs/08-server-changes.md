# Server-team requirements (GrowMate Pods)

Authoritative backend contract for the Pod device family. Device-side
behavior is specified in [06-api](06-api.md); this document lists what
the server must implement, store, and enforce.

## 1. Auth enforcement + 401 semantics

- Require `Authorization: Bearer <token>` on `POST /api/v1/sensors`
  and `POST /api/v1/camera` once per-device tokens ship. Tokens are
  build-time constants on the device; rotation is reflash-based (or OTA,
  §10) until a token-claim channel exists.
- Until then, gate on an explicit `deviceId` allowlist. Unknown IDs
  get `401`, never silent ingestion. Unclaimed birth IDs (`hardwareId`
  with default `deviceId`) should land in a review queue, not the void.
- `401` semantics: unknown, expired, or missing token. The device does
  NOT retry a 401 and does NOT reopen its onboarding portal over it —
  it keeps sensing and waits for the operator. Alert on repeated 401s
  from a known ID (token mismatch after reflash); alert on 401s from
  unknown IDs (spoofing/probing).

## 2. Status contract + Retry-After

| Status | When the server sends it |
|---|---|
| 2xx | accepted (also when there are no commands/config to return — always 200 with `{}` minimum) |
| 400 | malformed envelope (schema violation). Include a short error string; the device will not resend as-is |
| 401 | auth failure (§1) |
| 429 | rate limit hit (§6). MUST include a `Retry-After` header (seconds); the device honors it |
| 5xx | transient server failure; the device backs off and retries |

Never use redirects on these paths (the device does not follow them).
Keep response bodies under ~3 KB (device response buffer limit).

## 3. Ack / dedup handling

- Deduplicate telemetry on `snapshotId` (`"B<bootCount>-<seq>"`, unique
  per sample). Ingestion is at-least-once; double-POSTs of one id must
  not double-count. Note the fleet asymmetry: pods burn a `seq` on
  total-sensor-failure cycles (no POST), so pod gaps can mean failed
  reads; tech increments only when an envelope is built, so tech gaps
  mean queued-then-backfilled.
- Track per-device `acceptedCommandIds`. Re-deliver any command whose
  `id` has not been acked until it is acked or superseded by a newer
  command for the same actuator. Always attach a server-generated
  string `id` to every command.
- Use `currentState.{pumpEnabled,lightEnabled}` + `acceptedCommandIds`
  to distinguish "command lost" from "command applied, ack lost":
  if state already reflects the command, do not re-queue it.

## 4. ageMs time correction

The device has no RTC. The only sample-timing truth is
`ageMs` (sampling-to-POST latency, ms ≥ 0):

```
sampleTime = receivedAt - ageMs
```

Store `sampleTime`, not arrival time. Clamp absurd values
(`ageMs` larger than the report interval × 3, floor 45 s) and flag the
sample. Note: `ageMs` includes the WiFi join (up to 12 s) by design —
worst case ~28 s at a 10 s interval, inside the clamp, but tight.

## 5. Percent-computation ownership + calibration storage

The device sends RAW-ONLY analog entries (`{kind, unit, raw}`,
0–4095); percent mapping is server-owned, per probe, per install:

| Channel | Store per device | Mapping notes |
|---|---|---|
| water | `EMPTY` (probe in air), `FULL` (submerged) raw | rising scale |
| soil | `DRY` (air), `WET` (saturated soil) raw | rising scale |
| light | `DARK` (covered), `BRIGHT` (canopy under grow light) raw | INVERTED (dark ≈ 4095) |

Storage API needs:

- Per-device calibration record: `{channel, lowLabel, lowRaw,
  highLabel, highRaw, updatedAt}` plus write path (ops console or API)
  and audit of who changed it.
- Linear interpolation between the stored ends, clamped to 0–100%.
  Moving a probe or changing its supply invalidates its ends —
  require recalibration, do not silently keep stale ends.
- Analog entries carry `raw` only (no device-side percent field).
  DHT entries (`temperature` °C, `air` %RH) arrive native and need
  no mapping.

## 6. Config push schema + rev discipline

- Response field `config: {rev: int, reportIntervalSec: int}`.
  `rev` strictly increasing per device; `reportIntervalSec` 10–3600 s.
- The device persists rev + interval, applies the interval to the
  sensor cycle, and echoes `appliedConfigRev`. Never re-send a
  `rev <= appliedConfigRev`. Validate 10–3600 server-side before send.
- Camera period is NOT configurable (build-time 900 s); do not attempt
  to pace imaging via `reportIntervalSec`. Note the coupling: with an
  interval above 900 s the camera fires every cycle (one frame per
  interval), not every 900 s.

## 7. Rate limits

Enforce per-device limits and answer with `429` + `Retry-After`:

| Channel | Suggested limit |
|---|---|
| sensors POST | not more often than the device's effective interval (burst allowance ~3) |
| camera POST | at most 1 frame per 5 min per device |
| bytes | reject camera bodies over ~512 KB with `400` |

Rate-limit by authenticated `deviceId`, not by IP (many pods share one
egress). Log sustained 429s per device — a healthy pod never hits them.

## 8. maxPumpMs agreement

The device cap is 30 s (`0 < durationMs <= 30000`, water channel must
read). The server MUST ALSO clamp: never queue `durationMs > 30000`,
never queue a pump command when the last water reading is missing or
stale (> 3 intervals old). A pump dose is fire-and-forget once sent —
the one-shot runs on-device even if connectivity drops.

## 9. Health ingestion

Ingest `health.{heapFree, rssi, uptimeS, bootCount, resetReason,
dhtFails, adcFails}` on every POST. Suggested alerts:

| Signal | Threshold |
|---|---|
| `bootCount` increments without OTA | power/brownout — check pack + cabling |
| `rssi` | `< -75 dBm` sustained — AP placement |
| `heapFree` | trending down across boots — leak, capture a report |
| `dhtFails` / `adcFails` | rising — sensor wiring/EMI (see 07-operations) |
| `resetReason` | anything except the `power-on` baseline — investigate (`sw` after OTA is normal; `panic`/`brownout`/`power-glitch`/`wdt`/`task-wdt`/`int-wdt`/`cpu-lockup` always are) |

Dashboard per device: last `snapshotId`, `appliedConfigRev` vs sent
`rev` (drift = config not landing), pending unacked command ids.

## 10. Claim flow (live) + OTA (live)

- Claiming: every unit is born with its WiFi MAC as `hardwareId`
  (uppercase hex, always sent). A unit with no assigned ID reports
  `deviceId` = build default and is ingested as *unclaimed*.
- To claim/rename: answer any sensor POST with
  `claim: {deviceId: "<pod-id>"}` (`[A-Za-z0-9_-]`, ≤31 chars). The
  device validates, persists (NVS), and reports the new `deviceId` from
  the next POST. Re-sending the same ID is a no-op. Onboarding AP name
  follows the effective ID, so it changes once at claim time.
- Unclaim = erase flash (`tools/erase.sh`); the unit returns to
  its birth identity and must be re-claimed.
- OTA is live: send `minFirmware: "x.y.z"` + `firmwareUrl: "https://…"`
  to offer an update. The device upgrades only when the offered version
  is newer than its own, never mid pump-dose, then restarts. Rules:
  host a complete signed-by-TLS `.bin` built from this repo (same
  partition scheme — ota slots are 1.5 MB, images must fit); keep old
  binaries for rollback-by-reoffer; do NOT gate telemetry on version.
- Onboarding AP password is per-device (`GrowMate-<last6 of MAC>`,
  WPA2). There is no shared password anymore; the portal itself is still
  plaintext HTTP — commission in a trusted location (see 07-operations).
