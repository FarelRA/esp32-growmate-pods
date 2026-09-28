#!/usr/bin/env bash
# Full chip erase for ESP32-CAM via external USB-UART.
# Same discipline as tools/flash.sh: no DTR/RTS reset, no stub loader.
# Usage: tools/erase.sh PORT [BAUD] [ESPTOOL_PY]
#   ESPTOOL_PY defaults to ~/.platformio/packages/tool-esptoolpy/esptool.py
set -euo pipefail
PORT="${1:?usage: erase.sh PORT [BAUD] [ESPTOOL_PY]}"
BAUD="${2:-115200}"
ESPTOOL="${3:-$HOME/.platformio/packages/tool-esptoolpy/esptool.py}"
[ -f "$ESPTOOL" ] || { echo "erase.sh: esptool not found at $ESPTOOL"; exit 1; }
echo "erase.sh: erasing $PORT (hold BOOT if needed, then reset) ..."
python "$ESPTOOL" --chip esp32 --port "$PORT" --baud "$BAUD" \
    --before no_reset --after no_reset --no-stub erase_flash
echo "erase.sh: done"
