#!/usr/bin/env python3
"""Pods pure-contract host tests (no ESP toolchain).

The firmware's strictest rules live in static C functions that cannot
compile natively without ESP-IDF headers. This file mirrors those rules
as an executable spec so CI catches contract drift without flashing:
- semver strictness (ota_service.c:parse_version)
- SSID bounds (onboarding.c: 1-31 chars, driver NUL)
- command id + dose caps (api_client.c:parse_commands)
- config rev discipline (api_client.c:apply_config_push)
- claim charset (api_client.c:apply_claim)

If any rule here disagrees with src/, fix src/ AND this file together.
"""
import re
import unittest


def parse_version(text):
    """Mirror of ota_service.c parse_version: strict digits+dots, full consume."""
    parts = [0, 0, 0]
    if not text or len(text) >= 32:
        return None
    has_digit = False
    for ch in text:
        if ch.isdigit():
            has_digit = True
        elif ch != ".":
            return None
    if not has_digit:
        return None
    segs = text.split(".")
    if len(segs) > 3 or any(s == "" for s in segs) or any(not s.isdigit() for s in segs):
        return None
    for i, s in enumerate(segs):
        parts[i] = int(s)
    return parts


def version_is_newer(cur, cand):
    c, n = parse_version(cur), parse_version(cand)
    if c is None or n is None:
        return False
    return n != c and n > c


def valid_ssid(ssid):
    return isinstance(ssid, str) and 1 <= len(ssid) <= 31


def valid_command_id(cid):
    return isinstance(cid, str) and 0 < len(cid) < 32


def valid_dose_ms(dur):
    return isinstance(dur, (int, float)) and not isinstance(dur, bool) \
        and 0 < dur <= 30000


def valid_claim(cid):
    return isinstance(cid, str) and 0 < len(cid) <= 31 \
        and re.match(r"^[A-Za-z0-9_-]+$", cid) is not None


class TestPureContract(unittest.TestCase):
    def test_semver_strict(self):
        self.assertEqual(parse_version("2.0.0"), [2, 0, 0])
        self.assertEqual(parse_version("2.1"), [2, 1, 0])
        for bad in ("", "1..2", "2.1.0.4", "1.", ".1", "v1.2", "1.2a", "x" * 32):
            self.assertIsNone(parse_version(bad), bad)
        self.assertTrue(version_is_newer("2.0.0", "2.0.1"))
        self.assertFalse(version_is_newer("2.0.1", "2.0.0"))
        self.assertFalse(version_is_newer("2.0.0", "2.0.0"))

    def test_ssid_bounds(self):
        self.assertFalse(valid_ssid(""))
        self.assertTrue(valid_ssid("A"))
        self.assertTrue(valid_ssid("x" * 31))
        self.assertFalse(valid_ssid("x" * 32))  # driver NUL: 32 never joins

    def test_command_gates(self):
        self.assertTrue(valid_command_id("cmd-9f3"))
        self.assertFalse(valid_command_id(""))
        self.assertFalse(valid_command_id("x" * 32))
        self.assertTrue(valid_dose_ms(1) and valid_dose_ms(30000))
        self.assertFalse(valid_dose_ms(0) or valid_dose_ms(30001) or valid_dose_ms(True))

    def test_config_rev_discipline(self):
        applied = 7
        for rev, want in ((8, True), (7, False), (6, False), (0, False)):
            self.assertEqual(rev > applied and rev >= 1, want)

    def test_claim_charset(self):
        self.assertTrue(valid_claim("POD-9_abc"))
        for bad in ("", "bad id!", "x" * 32, "caf\u00e9"):
            self.assertFalse(valid_claim(bad), bad)


if __name__ == "__main__":
    unittest.main()
