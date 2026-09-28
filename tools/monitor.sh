#!/usr/bin/env bash
# Serial monitor for ESP32-CAM via external USB-UART (no auto-reset
# circuit: never assert DTR/RTS or the module resets mid-session).
# Usage: tools/monitor.sh [PORT] [BAUD]   (defaults below)
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"
exec pio device monitor --port "$PORT" --baud "$BAUD" \
  --rts 0 --dtr 0
