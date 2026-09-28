# Pods carrier rev A — release notes (also placed on-sheet)

## Operating rules (binding — defined in docs/, repeated on-sheet)
- Pump/LED XOR, charge idle/low-duty, fuses, GPIO12, probe care:
  `docs/01-pinout.md`, `docs/02-power.md`, `docs/04-wiring.md`. The
  binding instances also sit as on-sheet notes.

## Files
- `pods.py` — SKiDL source of truth (netlist/BOM/ERC). Pin `skidl==2.3.0`.
- `mfg/pods.net` — PCB import. `mfg/pods.xml` — BOM input.
  `mfg/bom.csv` — 41 items.
- `sheets/pods.kicad_sch` + `sheets/pods_s_*.kicad_sch` — hierarchy
  (root + 11 sheets).
- `render.py` + `layout_pods.py` + `pinmap.json` — deterministic
  placement pass (no hand edits in KiCad without back-annotating here).
- `mfg/pods.pdf` — plotted release drawing.

## Release gate (ERC — KiCad 10.0.6 CLI, SKiDL 2.3.0)
0 errors on all 11 sheets. Waived:
1. `isolated_pin_label` — single-ended global labels are the
   inter-sheet ports (e.g. `PUMP_GATE` on `s_mcu`/`s_pump`); mating
   label on the sibling sheet, same nets as `docs/06-api.md`.
2. `lib_symbol_mismatch` on `Diode:1N5819` (pump sheet only) —
   flattened copy of the installed lib's own `1N5819`+`SB120` units;
   connectivity proven independently (zero pin errors, netlist shows
   D1.1 on `PUMP_5V` with M1.1/C11.1, D1.2 on `PUMP_LO` with the rest).
Zero (must stay zero): `pin_not_connected`,
`unconnected_wire_endpoint`, `wire_dangling`, `multiple_net_names`,
`pin_to_pin`, `power_pin_not_driven`, `endpoint_off_grid`,
`no_net_danger`. `footprint_link_issues`/`lib_symbol_issues` appear
only without GUI lib tables (env-only).
Netlist proof: every sheet netlist machine-compared pin-for-pin vs
`mfg/pods.net` — zero mismatches.

## Manual review checklist (signed per release)
- [x] No wire/wire, wire/text, or refdes/symbol overlaps on any sheet
      (visual pass over all 11 PNGs, 2026-09-28).
- [x] Power top, GND bottom, signal left->right on every sheet.
- [x] Every fuse/diode/MOSFET orientation matches 04-wiring (D1 K to
      PUMP_5V branch, F1/F2/F3 feed direction, Q1/Q2 drains to loads —
      confirmed in netlist: D1.2/M1.2/Q1.2 share PUMP_LO).
- [x] `mfg/bom.csv` refs/values/footprints match the placed schematic
      (full netlist compare zero; 12/12 footprints resolve).
- [x] ERC log matches the waiver classes in the Release gate section above.
