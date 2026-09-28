# Pods carrier rev A — release notes (also placed on-sheet)

## Operating rules (binding)
- Pump and LED NEVER on together (server paces). Combined 3.8 A run /
  4.8 A stall browns out the rail; the firmware obeys whatever the
  server sends. Boost XL6009 4 A (heatsink on) covers the 2.8 A
  worst allowed state.
- Charge idle / low-duty only (TP4056 has no load-share; cell floats
  at 4.20 V under load — see docs/02-power.md).
- F1 5 A hold (2920): interlocked max ~5.5 A in at 3.0 V pack; trips
  on dead short only. F2 1.5 A (pump 1 A run / 2 A stall). F3 2.5 A
  (20 cm 2 A strip x1.25).
- GPIO12 (U1-6) stays unconnected, always (MTDI strapping LOW).
- Water probe is a consumable (continuous-DC electrolysis): rinse/dry
  between tanks, recalibrate after moving, stock spares.

## Files
- `pods.py` — SKiDL source of truth (netlist/BOM/ERC). Pin `skidl==2.3.0`.
- `mfg/pods.net` — PCB import. `mfg/pods.xml` — BOM input.
  `mfg/bom.csv` — 41 items.
- `sheets/pods.kicad_sch` + `sheets/pods_s_*.kicad_sch` — hierarchy
  (root + 11 sheets).
- `render.py` + `layout_pods.py` + `pinmap.json` — deterministic
  placement pass (no hand edits in KiCad without back-annotating here).
- `mfg/pods.pdf` — plotted release drawing.

## Manual review checklist (signed per release)
- [ ] No wire/wire, wire/text, or refdes/symbol overlaps on any sheet.
- [ ] Power top, GND bottom, signal left->right on every sheet.
- [ ] Every fuse/diode/MOSFET orientation matches 04-wiring (K faces
      the fused branch; low-side drains face loads).
- [ ] `mfg/bom.csv` refs/values/footprints match the placed schematic.
- [ ] ERC log matches ERC-REVIEW.md waiver classes only.
