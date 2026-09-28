#!/usr/bin/env python3
"""Pin-for-pin netlist compare: placed KiCad sheets vs SKiDL truth.

Usage: sch_compare.py <skidl.net> <sheet1.kicad_sch> [sheet2 ...]
  Exports each sheet with kicad-cli (to a temp dir, repo untouched),
  merges same-named nets across sheets, and compares against the
  SKiDL netlist: named nets must match exactly; anonymous Net-* nets
  must sit inside exactly one SKiDL net. X-marked NC
  (unconnected-*) is skipped on the schematic side by construction.

Exit 0 on full match, 1 with mismatches printed.
Env: KICAD10_SYMBOL_DIR / KICAD10_FOOTPRINT_DIR (else kicad defaults).
Stdlib only.
"""
import os
import re
import subprocess
import sys
import tempfile


def nodes_of(blk):
    return set(re.findall(r'\(ref "([^"]+)"\)\n\s*\(pin "([^"]+)"\)', blk))


def file_nets(path):
    with open(path, errors="replace") as f:
        text = f.read()
    out = {}
    for blk in text.split("(net")[1:]:
        m = re.search(r'\(name "([^"]+)"', blk)
        if not m:
            continue
        name = m.group(1)
        if name.startswith("unconnected-"):
            continue
        out.setdefault(name, set()).update(nodes_of(blk))
    return out


def export_sheet(path, tmpdir):
    out = os.path.join(tmpdir, os.path.basename(path) + ".net")
    r = subprocess.run(
        ["kicad-cli", "sch", "export", "netlist", path, "-o", out],
        capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        print(f"sch_compare: netlist export failed for {path}\n{r.stderr}")
        sys.exit(2)
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    skidl_path, sheets = argv[1], argv[2:]
    problems = 0
    with tempfile.TemporaryDirectory(prefix="schcmp-") as tmp:
        exported = [export_sheet(s, tmp) for s in sheets]
        sch = {}
        for path in exported:
            for name, nodes in file_nets(path).items():
                sch.setdefault(name, set()).update(nodes)
        sk = file_nets(skidl_path)
        for name in sorted(sch):
            if name.startswith("Net-"):
                continue
            if name not in sk:
                print(f"SCH net {name} MISSING in SKiDL")
                problems += 1
            elif sch[name] != sk[name]:
                only_sch = sorted(sch[name] - sk[name])
                only_sk = sorted(sk[name] - sch[name])
                print(f"MISMATCH {name}: sch-only={only_sch} "
                      f"skidl-only={only_sk}")
                problems += 1
        node2net = {}
        for name, nodes in sk.items():
            for n in nodes:
                node2net.setdefault(n, set()).add(name)
        for path in exported:
            with open(path, errors="replace") as f:
                text = f.read()
            for blk in text.split("(net")[1:]:
                m = re.search(r'\(name "([^"]+)"', blk)
                if not m or not m.group(1).startswith("Net-"):
                    continue
                nets = set()
                for n in nodes_of(blk):
                    nets.update(node2net.get(n, {"??"}))
                if len(nets) != 1:
                    print(f"{path} {m.group(1)} -> {sorted(nets)}")
                    problems += 1
    print("SCH-COMPARE " + ("OK" if problems == 0 else f"{problems} PROBLEMS"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
