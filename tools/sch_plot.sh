#!/usr/bin/env bash
# Pods schematic plots: SVG+PNG per sheet into schematic/final/,
# merged release drawing into schematic/mfg/pods.pdf.
# Usage: tools/sch_plot.sh   (run from the repo root)
set -euo pipefail
cd "$(dirname "$0")/../schematic"
export KICAD10_SYMBOL_DIR="${KICAD10_SYMBOL_DIR:-/usr/share/kicad/symbols}"
export KICAD10_FOOTPRINT_DIR="${KICAD10_FOOTPRINT_DIR:-/usr/share/kicad/footprints}"
mkdir -p final /tmp/gmplot-$$
trap 'rm -rf /tmp/gmplot-$$' EXIT
SHEETS="boost1 branches1 charge1 dht1 feed_fuse1 led1 lightsens1 mcu1 pump1 soil1 water1"
for s in $SHEETS; do
  kicad-cli sch export svg "sheets/pods_s_$s.kicad_sch" -o final >/dev/null
  inkscape "final/pods_s_$s.svg" --export-type=png --export-dpi=110 \
    -o "final/pods-s_$s.png" >/dev/null 2>&1
  mv "final/pods_s_$s.svg" "final/pods-s_$s.svg"
  kicad-cli sch export pdf "sheets/pods_s_$s.kicad_sch" \
    -o "/tmp/gmplot-$$/pods_s_$s.pdf" >/dev/null
done
pdfunite /tmp/gmplot-$$/pods_s_boost1.pdf /tmp/gmplot-$$/pods_s_branches1.pdf \
  /tmp/gmplot-$$/pods_s_charge1.pdf /tmp/gmplot-$$/pods_s_dht1.pdf \
  /tmp/gmplot-$$/pods_s_feed_fuse1.pdf /tmp/gmplot-$$/pods_s_led1.pdf \
  /tmp/gmplot-$$/pods_s_lightsens1.pdf /tmp/gmplot-$$/pods_s_mcu1.pdf \
  /tmp/gmplot-$$/pods_s_pump1.pdf /tmp/gmplot-$$/pods_s_soil1.pdf \
  /tmp/gmplot-$$/pods_s_water1.pdf mfg/pods.pdf
echo "SCH-PLOT OK: final/ + mfg/pods.pdf"
