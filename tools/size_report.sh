#!/usr/bin/env bash
# Build + firmware size report vs partitions.csv (catch flash overflow
# before flashing: factory image lives at ota_0 0x20000).
# Usage: tools/size_report.sh [PIO_ENV]   (default: growmate-pods)
set -euo pipefail
cd "$(dirname "$0")/.."
ENV="${1:-growmate-pods}"
pio run -e "$ENV" -t size
echo "--- partitions.csv ---"
cat partitions.csv
