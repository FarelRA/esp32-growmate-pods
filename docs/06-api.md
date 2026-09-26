# Server contract (Pods)

Two endpoints, configured at build time (`APP_SENSOR_API_URL`,
`APP_CAMERA_API_URL`). Same shape as the Tech device family.

## POST sensors — every 15 s, `Content-Type: application/json`

```json
{
  "deviceId": "IAET01",
  "firmwareVersion": "2.0.0",
  "sensors": [
    {"kind": "soil", "value": 45, "unit": "%", "raw": 2048},
    {"kind": "light", "value": 78, "unit": "%", "raw": 3200},
    {"kind": "water", "value": 92, "unit": "%", "raw": 3800},
    {"kind": "temperature", "value": 25, "unit": "C"},
    {"kind": "air", "value": 60, "unit": "%"}
  ],
  "currentState": {"pumpEnabled": false, "lightEnabled": false}
}
```

- Unavailable sensors are **omitted** (never sent as null).
- Analog entries carry `raw` (0–4095); DHT entries carry no `raw`.
- `temperature.value` is rounded to int by the firmware (`lround`).

Response — commands, applied immediately:

```json
{"commands": [{"kind": "pump", "durationMs": 5000},
              {"kind": "light", "enabled": true}]}
```

Unknown `kind` values are ignored. `durationMs <= 0` is ignored.

## POST camera — when due, `Content-Type: image/jpeg`

Headers `X-Device-Id: IAET01`, body = raw JPEG (typically 20–100 KB,
45 s timeout). Any 2xx = success.

## Test against a stub

```bash
curl -X POST "$SENSOR_URL" -H 'Content-Type: application/json' -d \
 '{"deviceId":"TEST01","firmwareVersion":"2.0.0", \
   "sensors":[{"kind":"soil","value":45,"unit":"%","raw":2048}], \
   "currentState":{"pumpEnabled":false,"lightEnabled":false}}'
curl -X POST "$CAM_URL" -H 'Content-Type: image/jpeg' \
  -H 'X-Device-Id: TEST01' --data-binary @test.jpg
```
