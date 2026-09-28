#!/usr/bin/env bash
# Pods schematic release gate: SKiDL truth -> placed sheets -> ERC -> compare.
# Usage: tools/sch_gate.sh   (run from the repo root)
# Env: PYTHON (default python3, must import skidl),
#      KICAD10_SYMBOL_DIR / KICAD10_FOOTPRINT_DIR (default /usr/share/kicad/...).
# Waived ERC classes: isolated_pin_label, lib_symbol_mismatch (see
# schematic/NOTES.md Release gate). Anything else fails the gate.
set -euo pipefail
cd "$(dirname "$0")/../schematic"
export KICAD10_SYMBOL_DIR="${KICAD10_SYMBOL_DIR:-/usr/share/kicad/symbols}"
export KICAD10_FOOTPRINT_DIR="${KICAD10_FOOTPRINT_DIR:-/usr/share/kicad/footprints}"
PYTHON="${PYTHON:-python3}"
"$PYTHON" -c "import skidl" 2>/dev/null || { echo "sch_gate: $PYTHON cannot import skidl"; exit 2; }

"$PYTHON" pods.py
"$PYTHON" layout_pods.py

fail=0
for f in sheets/pods_s_*.kicad_sch; do
  out="$(mktemp)"
  if ! kicad-cli sch erc "$f" --output "$out" >/dev/null 2>&1; then
    echo "sch_gate: ERC run failed for $f"; fail=1; rm -f "$out"; continue
  fi
  bad="$(grep -E '^\[' "$out" | grep -vE 'isolated_pin_label|lib_symbol_mismatch' || true)"
  if [ -n "$bad" ]; then echo "sch_gate: $f non-waived violations:"; echo "$bad"; fail=1; fi
  rm -f "$out"
done
[ "$fail" -ne 0 ] && { echo "SCH-GATE FAIL (ERC)"; exit 1; }

"$PYTHON" ../tools/sch_compare.py mfg/pods.net sheets/pods_s_*.kicad_sch
