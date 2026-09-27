#!/usr/bin/env python3
"""Extract pin geometry from KiCad symbol S-expr (lib_symbols sections).

Usage: python3 pinmap.py <kicad_sch files...> > pinmap.json
Output: {lib_id: {pin_number: {"x":.., "y":.., "angle":.., "name":..}}}
Coordinates are symbol-local mm; the pin hotspot (wire attach point)
is exactly (x, y). The `angle` gives the pin's exit direction.
"""
import json
import re
import sys


def parse_lib_symbols(text):
    """Yield (lib_id, pin_num, x, y, angle, name) for every pin found."""
    # Find lib_symbols section bounds (first "(lib_symbols" to matching depth).
    start = text.find("(lib_symbols")
    if start < 0:
        return
    lines = text[start:].splitlines()
    depth = 0
    cur_base = None  # canonical "Lib:Name" of the enclosing symbol
    i = 0
    started = False
    while i < len(lines):
        ln = lines[i]
        opens = ln.count("(")
        closes = ln.count(")")
        if not started:
            if "(lib_symbols" in ln:
                started = True
                depth = opens - closes
            i += 1
            continue
        m = re.match(r'\s*\(symbol\s+"([^"]+)"\s*$', ln)
        if m:
            name = m.group(1)
            if name.count(":") == 1 and not re.search(r"_\d+_\d+$", name):
                cur_base = name  # top-level "Lib:Name" header
        if "(pin " in ln and cur_base:
            chunk = "\n".join(lines[i:i + 8])
            at = re.search(r"\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?", chunk)
            nm = re.search(r'\(name\s+"([^"]*)"', chunk)
            nb = re.search(r'\(number\s+"([^"]*)"', chunk)
            if at and nb:
                ang = float(at.group(3)) if at.group(3) else 0.0
                yield (cur_base, nb.group(1), float(at.group(1)),
                       float(at.group(2)), ang,
                       nm.group(1) if nm else "")
        depth += opens - closes
        if depth <= 0:
            break
        i += 1


def main(files):
    out = {}
    for path in files:
        text = open(path).read()
        for lib_id, num, x, y, ang, name in parse_lib_symbols(text):
            out.setdefault(lib_id, {})[num] = {
                "x": x, "y": y, "angle": ang, "name": name,
            }
    json.dump(out, sys.stdout, indent=1)


if __name__ == "__main__":
    main(sys.argv[1:])
