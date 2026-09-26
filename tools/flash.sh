#!/usr/bin/env bash
# GrowMate Pods manual flash for ESP32-CAM via an external USB-UART adapter.
#
# Why this exists: the module has no auto-reset circuit, so esptool must not
# touch DTR/RTS (`no_reset`) and cannot use its stub loader (`no-stub`).
# Hold IO0-GND (SW1) at power-on for download mode, release + reset after.
#
# Invoked by PlatformIO (`upload_command` in platformio.ini) or by hand:
#   tools/flash.sh /dev/ttyACM0 115200 .pio/build/growmate-pods <packages-dir>
set -euo pipefail

PORT="${1:?usage: flash.sh PORT BAUD BUILD_DIR PACKAGES_DIR}"
BAUD="${2:?usage: flash.sh PORT BAUD BUILD_DIR PACKAGES_DIR}"
BUILD_DIR="${3:?usage: flash.sh PORT BAUD BUILD_DIR PACKAGES_DIR}"
PACKAGES_DIR="${4:?usage: flash.sh PORT BAUD BUILD_DIR PACKAGES_DIR}"

ESPTOOL="$PACKAGES_DIR/tool-esptoolpy/esptool.py"
for f in bootloader.bin partitions.bin firmware.bin; do
    [ -f "$BUILD_DIR/$f" ] || { echo "flash.sh: missing $BUILD_DIR/$f (run: pio run)"; exit 1; }
done

python "$ESPTOOL" --chip esp32 --port "$PORT" --baud "$BAUD" \
    --before no_reset --after no_reset --no-stub write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x1000 "$BUILD_DIR/bootloader.bin" \
    0x8000 "$BUILD_DIR/partitions.bin" \
    0x10000 "$BUILD_DIR/firmware.bin"
