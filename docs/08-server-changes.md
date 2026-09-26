# Server-team requirements (GrowMate Pods)

Authoritative backend contract for the Pod device family. Device-side
behavior is specified in [06-api](06-api.md); this document lists what
the server must implement, store, and enforce.

## 1. Auth enforcement + 401 semantics

- Require `Authorization: Bearer <token>` on `POST /api/v1/sensors`
  and `POST /api/v1/camera` once per-device tokens ship. Tokens are
  build-time constants on the device; provisioning/rotation is
  reflash-based until a claim flow exists (§8).
- Until then, gate on an explicit `deviceId` allowlist. Unknown IDs
  get `401`, never silent ingestion.
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
  not double-count.
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
(`ageMs` larger than the report interval × 3) and flag the sample.

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
- Ignore the transitional device-side `value` field where present;
  `raw` is authoritative. DHT entries (`temperature` °C,
  `air` %RH) arrive native and need no mapping.

## 6. Config push schema + rev discipline

- Response field `config: {rev: int, reportIntervalSec: int}`.
  `rev` strictly increasing per device; `reportIntervalSec` 10–3600 s.
- The device persists rev + interval, applies the interval to the
  sensor cycle, and echoes `appliedConfigRev`. Never re-send a
  `rev <= appliedConfigRev`. Validate 10–3600 server-side before send.
- Camera period is NOT configurable (build-time 900 s); do not attempt
  to pace imaging via `reportIntervalSec`.

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
| `resetReason` | anything except `power-on` / `watchdog` baseline — investigate |

Dashboard per device: last `snapshotId`, `appliedConfigRev` vs sent
`rev` (drift = config not landing), pending unacked command ids.

## 10. Claim / OTA-reserved futures

- Claiming: `deviceId`/`firmwareVersion` are compile-time constants
  today. Until a per-device claim flow exists, onboarding a new pod =
  adding its ID (and token, once issued) to the allowlist.
- OTA-reserved response fields `minFirmware` / `firmwareUrl`: the
  device ignores them today (single-app partition, no OTA). The server
  MAY start sending them for fleet bookkeeping, but must not gate
  telemetry ingestion on firmware version until OTA ships.
- Per-device onboarding-AP password is future: today every pod's
  setup AP shares one password over plaintext HTTP (see 07-operations
  threat note). Commission pods in a trusted location.
