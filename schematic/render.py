#!/usr/bin/env python3
"""Deterministic KiCad schematic renderer (placement pass).

Reads a SKiDL-generated sheet, keeps its header/lib_symbols/title/uuids,
and replaces symbols/wires/labels with an explicit, overlap-free layout.

Layout spec (per sheet): parts {ref: dict(lib, value, fp, x, y, rot)},
wires as pin-to-pin routes with waypoints, global labels, power symbols,
notes, no-connect markers. Rotation is in degrees; pin coordinates are
resolved from pinmap.json (symbol-local hotspots).

Usage: python3 render.py layout_pods.py
"""
import json
import math
import re
import sys
import uuid


def u():
    return str(uuid.uuid4())


PWR_FLAG_BLOCK = None
_LIB_BLOCK_CACHE = {}


def lib_block(lib, short):
    """Extract a top-level symbol definition from a KiCad system library
    and re-label it as Lib:Short for embedding in a sheet's lib_symbols."""
    import re
    key = (lib, short)
    if key in _LIB_BLOCK_CACHE:
        return _LIB_BLOCK_CACHE[key]
    path = f"/usr/share/kicad/symbols/{lib}.kicad_sym"
    txt = open(path).read()
    # Top-level entries sit at the shallowest indent; unit subsymbols
    # nest deeper. Match the exact name at minimal indent.
    best = None
    for m in re.finditer(r'(?m)^([ \t]*)\(symbol "' + re.escape(short) + r'"', txt):
        indent = len(m.group(1).replace("\t", "    "))
        if best is None or indent < best[0]:
            best = (indent, m.start() + len(m.group(1)))
    assert best is not None, f"{short} not found in {lib}"
    bol = best[1]
    depth = 0
    for j, ch in enumerate(txt[bol:]):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                block = txt[bol:bol + j + 1]
                break
    block = block.replace(f'(symbol "{short}"', f'(symbol "{lib}:{short}"', 1)
    _LIB_BLOCK_CACHE[key] = block
    return block


def _balanced_spans(text, start_pat):
    """Yield (start, end) spans of balanced paren blocks starting with pat."""
    import re
    for m in re.finditer(start_pat, text):
        depth = 0
        k = m.start()
        while k < len(text):
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        yield (m.start(), k + 1)


def _unit_spans(block, short):
    """Map unit-suffix ('_0_1', ...) -> span text for a symbol block."""
    import re
    out = {}
    for um, ue in _balanced_spans(
            block, r'\(symbol "' + re.escape(short) + r"_[0-9]+_[0-9]+\""):
        uname = re.match(r'\(symbol "([^"]+)"', block[um:]).group(1)
        out[uname[len(short):]] = block[um:ue]
    return out


def _drop_unit_spans(block, short):
    import re
    spans = [s for s in _balanced_spans(
        block, r'\(symbol "' + re.escape(short) + r"_[0-9]+_[0-9]+\"")]
    for um, ue in reversed(spans):
        block = block[:um] + block[ue:]
    return block


def flat_block(lib, short, _seen=None):
    """Self-contained symbol definition with extends-parents inlined.

    Derived symbols (IRLZ44N->BUZ11, 1N5819->SB120, ...) carry no pins of
    their own; embedding the shell alone breaks pin resolution AND even
    rendering. Flattening merges the (recursively resolved) parent units
    under the child name, so the sheet carries zero external deps beyond
    the pinned KiCad version. See ERC-REVIEW.md."""
    import re
    if _seen is None:
        _seen = set()
    if (lib, short) in _seen:
        return ""
    _seen.add((lib, short))
    child = lib_block(lib, short)
    parents = re.findall(r'\(extends\s+"([^"]+)"', child)
    shell = re.sub(r"[ \t]*\(extends\s+\"[^\"]+\"\)\n?", "", child)
    own = _unit_spans(shell, short)
    shell = _drop_unit_spans(shell, short)
    merged = "".join(own.values())
    seen_suffix = set(own)
    for p in parents:
        for suffix, ublock in _unit_spans(flat_block(lib, p, _seen), p).items():
            if suffix in seen_suffix:
                continue
            seen_suffix.add(suffix)
            merged += ublock.replace(f'(symbol "{p}{suffix}"',
                                     f'(symbol "{short}{suffix}"', 1)
    h = shell.rstrip()
    assert h.endswith(")")
    return h[:-1] + merged + ")\n"


def pwr_flag_block():
    """power:PWR_FLAG definition for embedding in lib_symbols."""
    return lib_block("power", "PWR_FLAG")


def load_pinmap(path="pinmap.json"):
    return json.load(open(path))


def split_sections(text):
    """Return (head, tail): head = everything through lib_symbols close."""
    start = text.find("(lib_symbols")
    assert start > 0
    depth = 0
    started = False
    pos = start
    lines = text[start:].splitlines(keepends=True)
    idx = 0
    for i, ln in enumerate(lines):
        if not started:
            if "(lib_symbols" in ln:
                started = True
                depth = ln.count("(") - ln.count(")")
            continue
        depth += ln.count("(") - ln.count(")")
        if depth <= 0:
            idx = i
            break
    head = text[:start + sum(len(l) for l in lines[:idx + 1])]
    tail = text[start + sum(len(l) for l in lines[:idx + 1]):]
    return head, tail


def sheet_uuid(text):
    m = re.search(r"\(kicad_sch\n(?:.*\n){1,6}  \(uuid ([0-9a-f-]+)\)", text)
    if m:
        return m.group(1)
    m = re.search(r"\(uuid ([0-9a-f-]+)\)", text)
    return m.group(1)


def rot_pt(x, y, rot):
    """Symbol-local (y-up library frame) -> sheet (y-down) coordinates.

    KiCad rotates in the library frame then flips Y. Calibrated against
    kicad-cli ERC pin-position reports for rot 0/90/180/270."""
    r = math.radians(rot % 360)
    c, s = math.cos(r), math.sin(r)
    return (x * c - y * s, -(x * s + y * c))


class Sheet:
    def __init__(self, src_path, pinmap):
        self.src = open(src_path).read()
        self.head, _ = split_sections(self.src)
        self.uuid = sheet_uuid(self.src)
        m = re.search(r'\(path "([^"]+)"', self.src)
        self.inst_path = m.group(1) if m else "/" + self.uuid
        self.pinmap = pinmap
        self.parts = {}   # ref -> dict(lib, value, fp, x, y, rot)
        self.items = []   # emitted s-expr strings

    # -- placement ----------------------------------------------------
    def place(self, ref, lib, value, fp, x, y, rot=0, desc=""):
        gx, gy = round(x / 2.54), round(y / 2.54)
        assert abs(x - gx * 2.54) < 1e-6 and abs(y - gy * 2.54) < 1e-6, \
            f"{ref} off-grid: KiCad ERC flags off-grid wire ends"
        self.parts[ref] = {"lib": lib, "value": value, "fp": fp,
                           "x": x, "y": y, "rot": rot, "desc": desc}

    def pin_xy(self, ref, pin):
        p = self.parts[ref]
        pm = self.pinmap[p["lib"]][str(pin)]
        lx, ly = rot_pt(pm["x"], pm["y"], p["rot"])
        return (round(p["x"] + lx, 2), round(p["y"] + ly, 2))

    # -- emission ------------------------------------------------------
    def emit_symbol(self, ref, lab="tb"):
        """lab: 'tb' = ref above / value below (horizontal parts);
        'side' = both right of body (vertical parts, keeps text off wires);
        'left' = both left of body (left-edge parts);
        'below' = both under body (tall connectors)."""
        p = self.parts[ref]
        x, y, rot = p["x"], p["y"], p["rot"]
        pins = self.pinmap[p["lib"]]
        if lab == "side":
            ref_at = (round(x + 4.5, 2), round(y - 1.2, 2))
            val_at = (round(x + 4.5, 2), round(y + 2.6, 2))
            just = "left"
        elif lab == "left":
            ref_at = (round(x - 4.5, 2), round(y - 1.2, 2))
            val_at = (round(x - 4.5, 2), round(y + 2.6, 2))
            just = "right"
        elif lab == "below":
            ref_at = (x, round(y + 22.14, 2))
            val_at = (x, round(y + 25.68, 2))
            just = "left"
        else:
            ref_at = (x, round(y - 4.2, 2))
            val_at = (x, round(y + 4.2, 2))
            just = "left"
        lines = [f'  (symbol',
                 f'    (lib_id "{p["lib"]}")',
                 f'    (at {x} {y} {rot})',
                 f'    (unit 1)',
                 f'    (exclude_from_sim no)',
                 f'    (in_bom yes)',
                 f'    (on_board yes)',
                 f'    (dnp no)',
                 f'    (fields_autoplaced no)',
                 f'    (uuid {u()})',
                 f'    (property "Reference" "{ref}"',
                 f'      (at {ref_at[0]} {ref_at[1]} 0)',
                 f'      (effects (font (size 1.27 1.27)) (justify {just})))',
                 f'    (property "Value" "{p["value"]}"',
                 f'      (at {val_at[0]} {val_at[1]} 0)',
                 f'      (effects (font (size 1.27 1.27)) (justify {just})))',
                 f'    (property "Footprint" "{p["fp"]}"',
                 f'      (at {x} {y} 0)',
                 f'      (effects (font (size 1.27 1.27)) (hide yes)))']
        if p["desc"]:
            lines.append(
                 f'    (property "Description" "{p["desc"]}"\n'
                 f'      (at {x} {y} 0)\n'
                 f'      (effects (font (size 1.27 1.27)) (hide yes)))')
        for num in pins:
            lines.append(f'    (pin "{num}"\n      (uuid {u()}))')
        lines.append(
                 f'    (instances\n      (project "GrowMate"\n'
                 f'        (path "{self.inst_path}"\n'
                 f'          (reference "{ref}")\n          (unit 1)))))')
        self.items.append("\n".join(lines))

    def wire(self, pts):
        # KiCad 10 stores one segment per (wire) object: split polylines.
        pts = [(round(x, 2), round(y, 2)) for x, y in pts]
        for a, b in zip(pts[:-1], pts[1:]):
            if a == b:
                continue
            self.items.append(
                "  (wire\n    (pts\n"
                f"      (xy {a[0]} {a[1]})\n      (xy {b[0]} {b[1]})\n"
                "    )\n    (stroke (width 0.1524) (type default))\n"
                f"    (uuid {u()}))")

    def route(self, *pins, via=()):
        """Route through pin endpoints (ref, pin) with optional waypoints."""
        pts = []
        for i, (ref, pin) in enumerate(pins):
            pts.append(self.pin_xy(ref, pin))
            if i < len(via):
                pts.append(via[i])
        self.wire(pts)
        return pts

    def junction(self, x, y):
        self.items.append(f"  (junction (at {round(x,2)} {round(y,2)}) "
                          f"(diameter 0) (color 0 0 0 0)\n    (uuid {u()}))")

    def glabel(self, text, x, y, rot=0, shape="bidirectional"):
        just = "left" if rot in (0, 180) else "top"
        self.items.append(
            f'  (global_label "{text}"\n    (shape {shape})\n'
            f'    (at {x} {y} {rot})\n'
            f'    (effects (font (size 1.27 1.27)) (justify {just}))\n'
            f'    (uuid {u()}))')

    def power(self, lib, x, y, rot=0):
        name = lib.split(":")[1]
        n = getattr(self, "_pwr_n", 0) + 1
        self._pwr_n = n
        ref = f"#{name}{n:02d}"
        self.items.append(
            f'  (symbol\n    (lib_id "{lib}")\n    (at {x} {y} {rot})\n'
            f'    (unit 1)\n    (exclude_from_sim no)\n    (in_bom no)\n'
            f'    (on_board yes)\n    (dnp no)\n    (fields_autoplaced no)\n'
            f'    (uuid {u()})\n'
            f'    (property "Reference" "{ref}"\n'
            f'      (at {x} {y} 0)\n'
            f'      (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f'    (property "Value" "{name}"\n'
            f'      (at {x} {y} 0)\n'
            f'      (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f'    (pin "1"\n      (uuid {u()}))\n'
            f'    (instances\n      (project "GrowMate"\n'
            f'        (path "{self.inst_path}"\n'
            f'          (reference "{ref}")\n          (unit 1)))))')

    def noconn(self, x, y):
        self.items.append(f"  (no_connect (at {round(x,2)} {round(y,2)})\n"
                          f"    (uuid {u()}))")

    def note(self, lines, x, y, size=1.0):
        for i, ln in enumerate(lines):
            self.items.append(
                f'  (text "{ln}"\n    (at {x} {round(y + i * 2.2, 2)} 0)\n'
                f'    (effects (font (size {size} {size})) (justify left))\n'
                f'    (uuid {u()}))')

    def pwrflag(self, x, y):
        """PWR_FLAG marker wired to a rail point: satisfies KiCad ERC
        power_pin_not_driven on sheets whose rails have no output driver."""
        n = getattr(self, "_pwr_n", 0) + 1
        self._pwr_n = n
        ref = f"#PWR{n:02d}"
        self.items.append(
            f'  (symbol\n    (lib_id "power:PWR_FLAG")\n'
            f'    (at {x} {y} 0)\n    (unit 1)\n'
            f'    (exclude_from_sim no)\n    (in_bom no)\n'
            f'    (on_board yes)\n    (dnp no)\n    (fields_autoplaced no)\n'
            f'    (uuid {u()})\n'
            f'    (property "Reference" "{ref}"\n'
            f'      (at {x} {y} 0)\n'
            f'      (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f'    (pin "1"\n      (uuid {u()}))\n'
            f'    (instances\n      (project "GrowMate"\n'
            f'        (path "{self.inst_path}"\n'
            f'          (reference "{ref}")\n          (unit 1)))))')
        self._needs_pwrflag_lib = True

    def write(self, path):
        import re
        # Rebuild lib_symbols from current system libraries: every placed
        # part plus the full extends-closure of derived symbols. SKiDL's
        # embedded copies may predate the installed KiCad (mismatch) or
        # omit parents (broken extends chains), so nothing is reused.
        used = {p["lib"] for p in self.parts.values()}
        if getattr(self, "_needs_pwrflag_lib", False):
            used.add("power:PWR_FLAG")
        used |= {m for it in self.items
                 for m in re.findall(r'\(lib_id "(power:[^"]+)"', it)}
        ordered = {}
        for lib_id in sorted(used):
            lib, short = lib_id.split(":", 1)
            block = flat_block(lib, short)
            ordered[(lib, short)] = block
        libs_body = ""
        for (plib, pshort), block in ordered.items():
            libs_body += "  " + block.replace("\n", "\n  ").rstrip() + "\n"
        pre = self.head.split("(lib_symbols")[0]
        head = pre + "(lib_symbols\n" + libs_body + "  )\n"
        with open(path, "w") as f:
            f.write(head)
            f.write("\n")
            f.write("\n".join(self.items))
            f.write("\n)\n")
