# API Documentation - ESP32-CAM

API format for GrowMate ESP32-CAM cloud integration.

## Overview

The ESP32-CAM firmware uses the same API format as the Raspberry Pi version for compatibility. This allows a single backend to support both device types.

## API Endpoints

Configure in `src/app_build_config.h`:
```c
#define APP_SENSOR_API_URL "https://api.example.com/sensors"
#define APP_CAMERA_API_URL "https://api.example.com/camera"
```

## Sensor Data Upload

**Endpoint:** `APP_SENSOR_API_URL`  
**Method:** POST  
**Content-Type:** application/json  
**Frequency:** Every 15 seconds (configurable)

### Request Format

```json
{
  "deviceId": "IAET01",
  "firmwareVersion": "2.0.0",
  "sensors": [
    {"kind": "soil", "value": 45, "unit": "%", "raw": 2048},
    {"kind": "light", "value": 78, "unit": "%", "raw": 3200},
    {"kind": "water", "value": 92, "unit": "%", "raw": 3800},
    {"kind": "temperature", "value": 25.3, "unit": "C"},
    {"kind": "air", "value": 60.5, "unit": "%"}
  ],
  "currentState": {
    "pumpEnabled": false,
    "lightEnabled": false
  }
}
```

### Response Format

```json
{
  "commands": [
    {"kind": "pump", "durationMs": 5000},
    {"kind": "light", "enabled": true}
  ]
}
```

### Field Descriptions

**Request:**
- `deviceId`: Unique device identifier
- `firmwareVersion`: Firmware version string
- `sensors[]`: Array of sensor readings
  - `kind`: Sensor type (soil, light, water, temperature, air)
  - `value`: Calibrated value (0-100% or degrees C)
  - `unit`: Unit of measurement (%, C)
  - `raw`: Raw ADC value (0-4095, only for analog sensors)
- `currentState`: Current actuator states

**Response:**
- `commands[]`: Array of commands to execute
  - Pump command: `{"kind": "pump", "durationMs": 5000}`
  - Light command: `{"kind": "light", "enabled": true}`

## Camera Image Upload

**Endpoint:** `APP_CAMERA_API_URL`  
**Method:** POST  
**Content-Type:** image/jpeg  
**Frequency:** Every 15 minutes (configurable)

### Request Headers

```
Content-Type: image/jpeg
X-Device-Id: IAET01
Content-Length: <image size>
```

### Request Body

Raw JPEG image bytes (typically 20-100KB depending on resolution and quality).

### Response

```json
{
  "status": "success"
}
```

## Command Execution

Commands received in sensor upload response are executed immediately.

### Pump Command

```json
{"kind": "pump", "durationMs": 5000}
```

- Activates water pump for specified duration
- Duration in milliseconds (1-60000)
- Pump automatically shuts off after duration

### Light Command

```json
{"kind": "light", "enabled": true}
```

- Controls grow light on/off
- State persists until next command

## Error Handling

### HTTP Status Codes

- **200 OK**: Success
- **400 Bad Request**: Invalid JSON or missing fields
- **401 Unauthorized**: Authentication failed
- **500 Server Error**: Server-side error
- **503 Service Unavailable**: Server temporarily unavailable

### Retry Behavior

- Failed uploads are retried with exponential backoff
- After 5 consecutive failures, device enters onboarding mode
- Allows reconfiguration without reflashing

## Complete API Documentation

For detailed API documentation including:
- Authentication
- Rate limiting
- Error codes
- Example server implementations
- Best practices

See the [Raspberry Pi version API documentation](https://github.com/FarelRA/rpi-growmate-pods/blob/main/docs/API.md) - the API format is identical.

## Testing

### Test Sensor Endpoint

```bash
curl -X POST https://your-api.com/sensors \
  -H "Content-Type: application/json" \
  -d '{
    "deviceId": "TEST01",
    "firmwareVersion": "2.0.0",
    "sensors": [
      {"kind": "soil", "value": 45, "unit": "%", "raw": 2048}
    ],
    "currentState": {
      "pumpEnabled": false,
      "lightEnabled": false
    }
  }'
```

### Test Camera Endpoint

```bash
curl -X POST https://your-api.com/camera \
  -H "Content-Type: image/jpeg" \
  -H "X-Device-Id: TEST01" \
  --data-binary "@test.jpg"
```

## Support

- [CONFIGURATION.md](CONFIGURATION.md) - Configuration options
- [DEVELOPMENT.md](DEVELOPMENT.md) - Development guide
- [GitHub Issues](https://github.com/FarelRA/esp32-growmate-pods/issues)
