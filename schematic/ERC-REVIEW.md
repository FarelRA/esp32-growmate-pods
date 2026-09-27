# ERC review log — GrowMate Pods carrier rev A

Toolchain (pinned): KiCad 10.0.6 CLI, SKiDL 2.3.0, kicad-cli sch erc.
Release gate: 0 errors. Only the two waiver classes below remain.

## Waived (by design, each occurrence reviewed)

1. `isolated_pin_label` — single-ended global labels are the
   inter-sheet ports of this hierarchical design (e.g. `PUMP_GATE`
   driven on `s_mcu`, received on `s_pump`). The mating label lives on
   the sibling sheet; the server contract (`docs/06-api.md`) names the
   same nets. Not floaters.
2. `lib_symbol_mismatch` on `Diode:1N5819` (pump sheet only) —
   the embedded definition is a flattened copy of the installed
   library's own `1N5819`+`SB120` units (verified byte-identical
   modulo whitespace/name). Connectivity is proven independently:
   zero pin errors plus netlist shows D1.1 on `+5V` with M1.1/C11.1
   and D1.2 on `PUMP_LO` with M1.2/C11.2/Q1.2. KiCad table-comparison
   artifact on flattened `extends` symbols; harmless.

## Environment-only (not shipped, headless CI without GUI config)

- `footprint_link_issues` / `lib_symbol_issues` appear only when the
  `~/.config/kicad/10.0/*-lib-table` files are absent. With the tables
  present (as in the release flow) they are gone; footprints are
  literal strings in `pods.py`/BOM and were verified to exist in
  `/usr/share/kicad/footprints`.

## Not waived (must be zero — verified zero on all 11 sheets)

`pin_not_connected`, `unconnected_wire_endpoint`, `wire_dangling`,
`multiple_net_names`, `pin_to_pin`, `power_pin_not_driven`,
`endpoint_off_grid` (2.54 mm grid enforced in `render.py`),
`no_net_danger` and any error-severity item.
