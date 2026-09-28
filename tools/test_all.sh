#!/usr/bin/env bash
# All pods tests: host contract suite + on-host Unity suite (if present).
# Usage: tools/test_all.sh   (run from the repo root, no hardware needed)
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
if [ -d tests ]; then
  echo "== host contract tests =="
  if python3 -c "import pytest" 2>/dev/null; then
    python3 -m pytest tests/ -q || fail=1
  else
    python3 -m unittest discover -s tests || fail=1
  fi
fi
if [ -d test ]; then
  echo "== Unity suite (PlatformIO native) =="
  pio test -e native || fail=1
fi
[ "$fail" -eq 0 ] && echo "TEST-ALL OK" || { echo "TEST-ALL FAIL"; exit 1; }
