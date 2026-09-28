# Pods tools (run from the repo root)

Device workflow (ESP32-CAM has no auto-reset; never assert DTR/RTS):
- `flash.sh PORT BAUD BUILD_DIR PACKAGES_DIR` — manual flash (also used
  by PlatformIO `upload_command`); factory image goes to ota_0 0x20000.
- `monitor.sh [PORT] [BAUD]` — serial monitor, DTR/RTS forced off.
- `erase.sh PORT [BAUD] [ESPTOOL_PY]` — full chip erase, same discipline.
- `size_report.sh [ENV]` — build size vs `partitions.csv`.

Tests:
- `test_all.sh` — host contract suite + Unity suite (`test/`, once present).

Schematics (flow: SKiDL truth → placed sheets → ERC → compare → plot):
- `sch_gate.sh` — full release gate (fails on any non-waived ERC class).
- `sch_compare.py SKIDL.NET SHEET...` — pin-for-pin netlist compare.
- `sch_plot.sh` — SVG+PNG to `schematic/final/`, merged `schematic/mfg/pods.pdf`.
