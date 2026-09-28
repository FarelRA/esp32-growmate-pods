#!/usr/bin/env python3
"""Pods production layouts: explicit overlap-free placement per sheet.

Conventions: 2.54 mm grid (KiCad ERC-clean), signal left->right, power
symbols at pins (no power rails), one PWR_FLAG per rail per sheet,
global labels only for inter-sheet nets, X markers on open pins,
notes bottom-left.
"""
import sys
sys.path.insert(0, ".")
from render import Sheet, load_pinmap

PM = load_pinmap()
H3 = "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical"
H4 = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
H8 = "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical"
H2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"


def stub_vcc(sh, lib, x, y_pin):
    """Short vertical stub from a pin up to a rail power symbol."""
    sh.power(lib, x, y_pin - 10.16)
    sh.wire([(x, y_pin - 10.16), (x, y_pin)])


def stub_gnd(sh, x, y_pin):
    """Short vertical stub from a pin down to a GND symbol."""
    sh.power("power:GND", x, y_pin + 10.16)
    sh.wire([(x, y_pin), (x, y_pin + 10.16)])


# ================================================================ charge
# CHG1 pins (rot 0, left side x=71.12): 1 B+ (78.74), 2 B- (76.2),
# 3 OUT+ (73.66), 4 OUT- (71.12). BT1 rot 180 at x=35.56, pins right.
sh = Sheet("pods_s_charge1.kicad_sch", PM)
sh.place("CHG1", "Connector_Generic:Conn_01x04", "TP4056-TypeC-DW01A", H4,
         76.2, 76.2, rot=0)
sh.place("BT1", "Connector_Generic:Conn_01x02", "BLP673-4.2Ah", H2,
         35.56, 76.2, rot=180)
sh.emit_symbol("CHG1", lab="side")
sh.emit_symbol("BT1", lab="left")
# PACK_P: CHG1.1 -> left, up over, down to BT1.1.
p = sh.pin_xy("CHG1", 1)
b1 = sh.pin_xy("BT1", 1)
sh.wire([p, (55.88, p[1]), (55.88, 68.58), (40.64, 68.58), (40.64, b1[1]), b1])
# Returns split: CHG1.4 (OUT-) joins the GND star rail at y=96.52;
# CHG1.2 (B-) + BT1.2 ride a dedicated CELL_NEG rail at y=91.44 so the
# DW01A low-side FET is never shorted. Header order B+/B-/OUT+/OUT-
# (pin1 = cell+ shared with BT1.1, pin3 = load+ to F1): VERIFY on the
# physical module before soldering, clones vary.
# (BT1.2 routes via x=48.26: the PACK_P vertical at x=40.64 must not
# touch it — that would short PACK_P to CELL_NEG in the netlist.)
g1 = sh.pin_xy("CHG1", 2)
g2 = sh.pin_xy("CHG1", 4)
b2 = sh.pin_xy("BT1", 2)
sh.wire([g1, (63.5, g1[1]), (63.5, 91.44)])
sh.junction(63.5, 91.44)
sh.wire([b2, (48.26, b2[1]), (48.26, 91.44)])
sh.junction(48.26, 91.44)
sh.wire([(48.26, 91.44), (63.5, 91.44)])
sh.wire([g2, (68.58, g2[1]), (68.58, 96.52)])
sh.wire([(55.88, 96.52), (73.66, 96.52)])
sh.junction(68.58, 96.52)
stub_gnd(sh, 55.88, 96.52)
sh.junction(55.88, 96.52)
sh.pwrflag(73.66, 96.52)
# OUT+ to sheet edge.
o = sh.pin_xy("CHG1", 3)
sh.wire([o, (111.76, o[1])])
sh.glabel("SYS_PACK", 111.76, o[1])
sh.note(["CHG1: 5V enters via on-module USB-C.",
         "Load hangs on OUT+-, never B+- (02-power idle-charge rule)."],
        25.4, 139.7)
sh.write("pods_s_charge1.kicad_sch")
print("s_charge written")

# ============================================================= feed fuse
# F1 rot 0: pin1 top (72.39), pin2 bottom (80.01). No rails on sheet.
sh = Sheet("pods_s_feed_fuse1.kicad_sch", PM)
sh.place("F1", "Device:Polyfuse", "Polyfuse-5A-hold",
         "Fuse:Fuse_2920_7451Metric", 101.6, 76.2, rot=0)
sh.emit_symbol("F1", lab="side")
a = sh.pin_xy("F1", 1)
sh.wire([(78.74, a[1]), a])
sh.glabel("SYS_PACK", 78.74, a[1])
b = sh.pin_xy("F1", 2)
sh.wire([b, (b[0], 93.98), (119.38, 93.98)])
sh.glabel("BST_IN", 119.38, 93.98)
sh.note(["F1 5A hold: interlocked max ~5.5A in at 3.0V pack;",
         "trips on dead short only."], 25.4, 139.7)
sh.write("pods_s_feed_fuse1.kicad_sch")
print("s_feed_fuse written")

# ================================================================== boost
# BST1 rot 0 pins left x=55.88: 1 IN+ (73.66), 2 IN- (76.2),
# 3 OUT+ (78.74), 4 OUT- (81.28).
sh = Sheet("pods_s_boost1.kicad_sch", PM)
sh.place("BST1", "Connector_Generic:Conn_01x04", "XL6009-5V-4A", H4,
         60.96, 76.2, rot=0)
sh.place("C2", "Device:C", "1000u-10V",
         "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm", 129.54, 73.66, rot=0)
sh.place("C3", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 144.78, 88.9, rot=0)
sh.emit_symbol("BST1", lab="side")
sh.emit_symbol("C2", lab="side")
sh.emit_symbol("C3", lab="side")
bi = sh.pin_xy("BST1", 1)
sh.wire([(35.56, bi[1]), bi])
sh.glabel("BST_IN", 35.56, bi[1])
# IN- / OUT- drops to GND rail y=96.52.
sh.wire([sh.pin_xy("BST1", 2), (50.8, 76.2), (50.8, 96.52)])
sh.wire([sh.pin_xy("BST1", 4), (63.5, 81.28), (63.5, 96.52)])
sh.wire([(50.8, 96.52), (149.86, 96.52)])
for jx in (50.8, 63.5):
    sh.junction(jx, 96.52)
sh.wire([(101.6, 96.52), (101.6, 106.68)])
sh.power("power:GND", 101.6, 106.68)
sh.junction(101.6, 96.52)
for jx in (129.54, 144.78):
    sh.junction(jx, 96.52)
sh.pwrflag(149.86, 96.52)
# OUT+ up and right along y=63.5; +5V symbol + C taps.
bo = sh.pin_xy("BST1", 3)
sh.wire([bo, (73.66, bo[1]), (73.66, 63.5), (160.02, 63.5)])
sh.power("power:+5V", 88.9, 53.34)
sh.wire([(88.9, 53.34), (88.9, 63.5)])
sh.junction(88.9, 63.5)
sh.pwrflag(160.02, 63.5)
c2t = sh.pin_xy("C2", 1)
sh.wire([(c2t[0], 63.5), c2t])
sh.junction(c2t[0], 63.5)
sh.wire([sh.pin_xy("C2", 2), (c2t[0], 96.52)])
c3t = sh.pin_xy("C3", 1)
sh.wire([(c3t[0], 63.5), c3t])
sh.junction(c3t[0], 63.5)
sh.wire([sh.pin_xy("C3", 2), (c3t[0], 96.52)])
sh.note(["BST1 XL6009 4A (heatsink on): pump XOR LED server-paced,",
         "2.8A worst allowed state. F1 5A rides 30s stall transients."],
        25.4, 139.7)
sh.write("pods_s_boost1.kicad_sch")
print("s_boost written")

# =============================================================== branches
# F2/F3 rot 0: pin1 top (72.39), pin2 bottom (80.01).
sh = Sheet("pods_s_branches1.kicad_sch", PM)
sh.place("F2", "Device:Polyfuse", "Polyfuse-1.5A-hold",
         "Fuse:Fuse_1812_4532Metric", 88.9, 76.2, rot=0)
sh.place("F3", "Device:Polyfuse", "Polyfuse-2.5A-hold",
         "Fuse:Fuse_1812_4532Metric", 139.7, 76.2, rot=0)
sh.emit_symbol("F2", lab="side")
sh.emit_symbol("F3", lab="side")
sh.wire([(83.82, 67.31), (139.7, 67.31)])
sh.wire([(88.9, 67.31), sh.pin_xy("F2", 1)])
sh.wire([(139.7, 67.31), sh.pin_xy("F3", 1)])
for jx in (88.9, 139.7):
    sh.junction(jx, 67.31)
sh.power("power:+5V", 114.3, 57.15)
sh.wire([(114.3, 57.15), (114.3, 67.31)])
sh.junction(114.3, 67.31)
sh.pwrflag(83.82, 67.31)
f2b = sh.pin_xy("F2", 2)
sh.wire([f2b, (f2b[0], 93.98), (104.14, 93.98)])
sh.glabel("PUMP_5V", 104.14, 93.98)
f3b = sh.pin_xy("F3", 2)
sh.wire([f3b, (f3b[0], 93.98), (154.94, 93.98)])
sh.glabel("LED_5V", 154.94, 93.98)
sh.note(["F2 1.5A: pump 1A run / 2A stall. F3 2.5A: 2A strip x1.25."],
        25.4, 139.7)
sh.write("pods_s_branches1.kicad_sch")
print("s_branches written")

# ==================================================================== mcu
# Y-flip mapping (lib y-up -> sheet y-down), ERC-oracle verified:
# U1A pins left x=43.18: 1:+5V(68.58) 2:GND(71.12) 3:RX(73.66) 4:TX(76.2)
# 5:BOOT(78.74) 6:+3V3(81.28) 7:GND(83.82) 8:GND(86.36).
# U1B pins left x=129.54: 1:PUMP(68.58) 2:LIGHT(71.12) 3:NC(73.66)
# 4:SOIL(76.2) 5:LIGHT_AO(78.74) 6:DHT(81.28) 7:WATER(83.82) 8:+5V(86.36).
# J4 rot180 pins right x=180.34: 1:RX(78.74) 2:TX(76.2) 3:GND(73.66) 4:+5V(71.12).
sh = Sheet("pods_s_mcu1.kicad_sch", PM)
sh.place("U1A", "Connector_Generic:Conn_01x08", "ESP32-CAM-ODD", H8,
         48.26, 76.2, rot=0)
sh.place("U1B", "Connector_Generic:Conn_01x08", "ESP32-CAM-EVEN", H8,
         134.62, 76.2, rot=0)
sh.place("R8", "Device:R", "10K",
         "Resistor_SMD:R_0603_1608Metric", 91.44, 73.66, rot=90)
sh.place("SW1", "Switch:SW_Push", "BOOT-TACT",
         "Button_Switch_THT:SW_PUSH-12mm", 116.84, 93.98, rot=0)
sh.place("J4", "Connector_Generic:Conn_01x04", "PROG-1x04", H4,
         175.26, 76.2, rot=180)
sh.emit_symbol("U1A", lab="below")
sh.emit_symbol("U1B", lab="below")
sh.emit_symbol("R8", lab="side")
sh.emit_symbol("SW1", lab="left")
sh.emit_symbol("J4", lab="below")
# +5V: U1A.1 stub up; U1B.8 jog right + down + flag.
sh.power("power:+5V", 43.18, 58.42)
sh.wire([(43.18, 58.42), (43.18, 68.58)])
sh.wire([(129.54, 86.36), (144.78, 86.36), (144.78, 96.52)])
sh.power("power:+5V", 144.78, 96.52)
sh.pwrflag(149.86, 91.44)
sh.wire([(144.78, 91.44), (149.86, 91.44)])
sh.junction(144.78, 91.44)
# +3V3: U1A.6 right, down to R8.1; symbol stub + flag mid-way.
sh.wire([(43.18, 81.28), (83.82, 81.28), (83.82, 73.66), (87.63, 73.66)])
sh.power("power:+3V3", 68.58, 71.12)
sh.wire([(68.58, 71.12), (68.58, 81.28)])
sh.junction(68.58, 81.28)
sh.pwrflag(73.66, 76.2)
sh.wire([(68.58, 76.2), (73.66, 76.2)])
sh.junction(68.58, 76.2)
# GND rail x=30.48 with three left taps + symbol + flag.
sh.wire([(43.18, 71.12), (30.48, 71.12), (30.48, 111.76)])
sh.wire([(43.18, 83.82), (30.48, 83.82)])
sh.junction(30.48, 83.82)
sh.wire([(43.18, 86.36), (30.48, 86.36)])
sh.junction(30.48, 86.36)
sh.power("power:GND", 30.48, 116.84)
sh.wire([(30.48, 111.76), (30.48, 116.84)])
sh.pwrflag(35.56, 111.76)
sh.wire([(30.48, 111.76), (35.56, 111.76)])
# BOOT: R8.2 to rail x=104.14, down to SW1.1; U1A.5 tap; label tap.
sh.wire([(95.25, 73.66), (104.14, 73.66), (104.14, 93.98),
         sh.pin_xy("SW1", 1)])
sh.wire([(43.18, 78.74), (104.14, 78.74)])
sh.junction(104.14, 78.74)
sh.wire([(104.14, 83.82), (111.76, 83.82)])
sh.junction(104.14, 83.82)
sh.glabel("BOOT_IO0", 111.76, 83.82)
s2 = sh.pin_xy("SW1", 2)
sh.wire([s2, (s2[0], 101.6)])
sh.power("power:GND", s2[0], 111.76)
sh.wire([(s2[0], 101.6), (s2[0], 111.76)])
# RX/TX short stubs + labels (J4 mates carry the same label names).
sh.wire([(43.18, 73.66), (53.34, 73.66)])
sh.glabel("U0RXD", 53.34, 73.66)
sh.wire([(43.18, 76.2), (53.34, 76.2)])
sh.glabel("U0TXD", 53.34, 76.2)
# J4 right side: RX/TX stubs + labels, GND drop, +5V jog.
sh.wire([(180.34, 78.74), (190.5, 78.74)])
sh.glabel("U0RXD", 190.5, 78.74)
sh.wire([(180.34, 76.2), (190.5, 76.2)])
sh.glabel("U0TXD", 190.5, 76.2)
sh.wire([(180.34, 73.66), (180.34, 63.5)])
sh.power("power:GND", 180.34, 63.5, rot=180)
sh.wire([(180.34, 71.12), (170.18, 71.12), (170.18, 81.28)])
sh.power("power:+5V", 170.18, 81.28, rot=180)
# U1B signals to edge labels; GPIO12 X marker.
for up, nm in ((1, "PUMP_GATE"), (2, "LIGHT_GATE"), (4, "SOIL_ADC"),
               (5, "LIGHT_AO"), (6, "DHT_DATA"), (7, "WATER_ADC")):
    a = sh.pin_xy("U1B", up)
    sh.wire([a, (160.02, a[1])])
    sh.glabel(nm, 160.02, a[1])
nc = sh.pin_xy("U1B", 3)
sh.wire([nc, (nc[0] + 5.08, nc[1])])
sh.noconn(nc[0] + 5.08, nc[1])
sh.note(["U1-6 GPIO12 stays unconnected (MTDI strapping LOW).",
         "J4: adapter TX->RX, RX<-TX. SW1 = download mode."],
        25.4, 139.7)
sh.write("pods_s_mcu1.kicad_sch")
print("s_mcu written")

# ================================================================== water
# J1 rot 180 at x=35.56, pins right: 1 VCC(76.2) 2 AO(73.66) 3 GND(71.12).
sh = Sheet("pods_s_water1.kicad_sch", PM)
sh.place("J1", "Connector_Generic:Conn_01x03", "WATER-1x03", H3,
         35.56, 73.66, rot=180)
sh.place("R7", "Device:R", "330",
         "Resistor_SMD:R_0603_1608Metric", 68.58, 63.5, rot=90)
sh.place("C6", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 68.58, 83.82, rot=0)
sh.place("R9", "Device:R", "10K",
         "Resistor_SMD:R_0603_1608Metric", 101.6, 83.82, rot=0)
sh.place("R11", "Device:R", "1K",
         "Resistor_SMD:R_0603_1608Metric", 119.38, 73.66, rot=90)
sh.place("C10", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 139.7, 88.9, rot=0)
for r in ("J1", "R7", "C6", "R9", "R11", "C10"):
    sh.emit_symbol(r, lab="side" if r in ("R7", "C6", "R9", "R11", "C10") else "left")
# TANK_VCC: +3V3 -> R7 -> J1.1, C6 tap.
sh.power("power:+3V3", 68.58, 48.26)
sh.wire([(68.58, 48.26), (68.58, 63.5), sh.pin_xy("R7", 1)])
sh.pwrflag(63.5, 48.26)
sh.wire([(63.5, 48.26), (68.58, 48.26)])
j1v = sh.pin_xy("J1", 1)
r7b = sh.pin_xy("R7", 2)
sh.wire([r7b, (r7b[0], j1v[1]), j1v])
sh.junction(r7b[0], j1v[1])
sh.wire([(r7b[0], j1v[1]), (r7b[0], sh.pin_xy("C6", 1)[1]), sh.pin_xy("C6", 1)])
c6b = sh.pin_xy("C6", 2)
sh.wire([c6b, (c6b[0], 96.52)])
sh.junction(c6b[0], 96.52)
j13 = sh.pin_xy("J1", 3)
sh.wire([j13, (j13[0], 96.52)])
sh.wire([(j13[0], 96.52), (101.6, 96.52)])
sh.junction(101.6, 96.52)
sh.wire([(101.6, 96.52), (101.6, 106.68)])
sh.power("power:GND", 101.6, 106.68)
sh.pwrflag(106.68, 96.52)
sh.wire([(101.6, 96.52), (106.68, 96.52)])
# WATER_AO: J1.2 -> R9 tap + R11 -> WATER_ADC; C10 filter.
j1a = sh.pin_xy("J1", 2)
r9t = sh.pin_xy("R9", 1)
sh.wire([j1a, (r9t[0], j1a[1]), r9t])
sh.junction(r9t[0], j1a[1])
sh.wire([sh.pin_xy("R9", 2), (r9t[0], 96.52)])
sh.junction(r9t[0], 96.52)
r11a = sh.pin_xy("R11", 1)
sh.wire([(r9t[0], j1a[1]), (r11a[0], j1a[1]), r11a])
r11b = sh.pin_xy("R11", 2)
sh.wire([r11b, (130.81, r11b[1]), (130.81, 85.09), sh.pin_xy("C10", 1)])
sh.wire([(139.7, 85.09), (152.4, 85.09)])
sh.glabel("WATER_ADC", 152.4, 85.09)
c10b = sh.pin_xy("C10", 2)
sh.wire([c10b, (c10b[0], 101.6)])
sh.power("power:GND", c10b[0], 111.76)
sh.wire([(c10b[0], 101.6), (c10b[0], 111.76)])
sh.note(["R7 330R: tank-probe VCC series (dead-short -> 10mA).",
         "R9 10K: open probe reads ~0 = EMPTY. Continuous-DC probe",
         "is a consumable (rinses + spares, see 02-power)."],
        25.4, 139.7)
sh.write("pods_s_water1.kicad_sch")
print("s_water written")

# ================================================================== soil
# J2 rot180 pins right x=40.64: 1:VCC BOTTOM (76.20), 2:AO (73.66),
# 3:GND TOP (71.12). Y-flip verified: rot180 (x,y)->(-x,+y).
sh = Sheet("pods_s_soil1.kicad_sch", PM)
sh.place("J2", "Connector_Generic:Conn_01x03", "SOIL-1x03", H3,
         35.56, 73.66, rot=180)
sh.place("R10", "Device:R", "1K",
         "Resistor_SMD:R_0603_1608Metric", 68.58, 73.66, rot=90)
sh.place("C9", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 88.9, 88.9, rot=0)
sh.place("C7", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 53.34, 83.82, rot=0)
for r in ("J2", "R10", "C9", "C7"):
    sh.emit_symbol(r, lab="left" if r == "J2" else "side")
# +3V3: J2.1 stub down + symbol; C7 top shares y=81.28; flag mid-wire.
sh.power("power:+3V3", 40.64, 86.36)
sh.wire([(40.64, 76.20), (40.64, 86.36)])
sh.wire([(40.64, 80.01), (53.34, 80.01)])
sh.junction(40.64, 80.01)
sh.pwrflag(45.72, 80.01)
sh.junction(45.72, 80.01)
# SOIL_AO rail J2.2 -> R10.1; R10.2 jog to C9 + label.
sh.wire([(40.64, 73.66), (64.77, 73.66)])
sh.wire([(72.39, 73.66), (80.01, 73.66), (80.01, 85.09), (88.9, 85.09)])
sh.wire([(88.9, 85.09), (101.6, 85.09)])
sh.glabel("SOIL_ADC", 101.6, 85.09)
# GND: J2.3 left+down, rail, C9 drop + symbol + flag.
sh.wire([(40.64, 71.12), (35.56, 71.12), (35.56, 96.52)])
sh.wire([(35.56, 96.52), (88.9, 96.52)])
sh.junction(88.9, 96.52)
sh.wire([(88.9, 92.71), (88.9, 101.6)])
sh.power("power:GND", 88.9, 111.76)
sh.wire([(88.9, 101.6), (88.9, 111.76)])
sh.pwrflag(93.98, 96.52)
sh.wire([(88.9, 96.52), (93.98, 96.52)])
# C7 bottom to rail.
sh.wire([(53.34, 87.63), (53.34, 96.52)])
sh.junction(53.34, 96.52)
sh.note(["Soil: DRY (air) vs WET (saturated) raw ends, server-side."],
        25.4, 139.7)
sh.write("pods_s_soil1.kicad_sch")
print("s_soil written")

# ============================================================= lightsens
# J3 rot180 pins right x=40.64: 1:+3V3 (76.20), 2:AO (73.66), 3:GND (71.12).
sh = Sheet("pods_s_lightsens1.kicad_sch", PM)
sh.place("J3", "Connector_Generic:Conn_01x03", "LIGHT-SENS-1x03", H3,
         35.56, 73.66, rot=180)
sh.place("R6", "Device:R", "1K",
         "Resistor_SMD:R_0603_1608Metric", 68.58, 73.66, rot=90)
sh.place("C1", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 88.9, 88.9, rot=0)
sh.place("C8", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 53.34, 83.82, rot=0)
for r in ("J3", "R6", "C1", "C8"):
    sh.emit_symbol(r, lab="left" if r == "J3" else "side")
sh.power("power:+3V3", 40.64, 86.36)
sh.wire([(40.64, 76.20), (40.64, 86.36)])
sh.wire([(40.64, 80.01), (53.34, 80.01)])
sh.junction(40.64, 80.01)
sh.pwrflag(45.72, 80.01)
sh.junction(45.72, 80.01)
sh.wire([(40.64, 73.66), (64.77, 73.66)])
sh.wire([(72.39, 73.66), (80.01, 73.66), (80.01, 85.09), (88.9, 85.09)])
sh.wire([(88.9, 85.09), (101.6, 85.09)])
sh.glabel("LIGHT_AO", 101.6, 85.09)
sh.wire([(40.64, 71.12), (35.56, 71.12), (35.56, 96.52)])
sh.wire([(35.56, 96.52), (88.9, 96.52)])
sh.junction(88.9, 96.52)
sh.wire([(88.9, 92.71), (88.9, 101.6)])
sh.power("power:GND", 88.9, 111.76)
sh.wire([(88.9, 101.6), (88.9, 111.76)])
sh.pwrflag(93.98, 96.52)
sh.wire([(88.9, 96.52), (93.98, 96.52)])
sh.wire([(53.34, 87.63), (53.34, 96.52)])
sh.junction(53.34, 96.52)
sh.note(["Light scale INVERTED (dark ~= full-scale). R6 tames",
         "GPIO14 boot-PWM contention. DARK vs BRIGHT ends server-side."],
        25.4, 139.7)
sh.write("pods_s_lightsens1.kicad_sch")
print("s_lightsens written")

# ================================================================== dht
# U2 DHT11-symbol rot0 at (60.96,76.2): VDD top (60.96,68.58),
# DATA right (68.58,76.2), GND bottom (60.96,83.82). Pin 3 NC open.
sh = Sheet("pods_s_dht1.kicad_sch", PM)
sh.place("U2", "Sensor:DHT11", "DHT22",
         "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
         60.96, 76.2, rot=0)
sh.place("R1", "Device:R", "4.7K",
         "Resistor_SMD:R_0603_1608Metric", 81.28, 63.5, rot=0)
sh.place("C4", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 48.26, 83.82, rot=0)
sh.place("C5", "Device:C", "10u-6V3",
         "Capacitor_SMD:C_0603_1608Metric", 106.68, 83.82, rot=0)
sh.emit_symbol("U2", lab="below")
sh.emit_symbol("R1", lab="side")
sh.emit_symbol("C4", lab="side")
sh.emit_symbol("C5", lab="side")
# VDD stub + flag; C4 top joins VDD pin.
sh.power("power:+3V3", 60.96, 58.42)
sh.wire([(60.96, 58.42), (60.96, 68.58)])
sh.pwrflag(66.04, 63.5)
sh.wire([(60.96, 63.5), (66.04, 63.5)])
sh.junction(60.96, 63.5)
sh.wire([(48.26, 80.01), (48.26, 68.58), (60.96, 68.58)])
# DATA rail with R1 pulldown tap + label.
sh.wire([(68.58, 76.2), (83.82, 76.2)])
sh.glabel("DHT_DATA", 83.82, 76.2)
sh.wire([(81.28, 67.31), (81.28, 76.2)])
sh.junction(81.28, 76.2)
sh.power("power:+3V3", 81.28, 50.8)
sh.wire([(81.28, 50.8), (81.28, 59.69)])
# GND: U2 drop, rail, C4 drop, symbol, flag.
sh.wire([(60.96, 83.82), (60.96, 96.52)])
sh.wire([(48.26, 87.63), (48.26, 96.52)])
sh.wire([(48.26, 96.52), (60.96, 96.52)])
sh.junction(60.96, 96.52)
sh.power("power:GND", 53.34, 106.68)
sh.wire([(53.34, 96.52), (53.34, 106.68)])
sh.junction(53.34, 96.52)
sh.pwrflag(60.96, 96.52)
# C5 bulk across +3V3/GND on the right.
sh.wire([(106.68, 80.01), (106.68, 59.69), (81.28, 59.69)])
sh.wire([(106.68, 87.63), (106.68, 101.6)])
sh.power("power:GND", 106.68, 111.76)
sh.wire([(106.68, 101.6), (106.68, 111.76)])
sh.note(["DHT polled with 2 attempts >= 2s apart; failures omit",
         "temperature/air rather than blocking."],
        25.4, 139.7)
sh.write("pods_s_dht1.kicad_sch")
print("s_dht written")

# ================================================================== pump
# R2 rot90 at (73.66,73.66): pins (69.85,73.66),(77.47,73.66).
# Q1 rot0 at (96.52,73.66): G(91.44,73.66) D(99.06,68.58) S(99.06,78.74).
# R3 rot0 at (86.36,88.9): pins (86.36,85.09),(86.36,92.71).
# M1 rot0 at (129.54,73.66): pins (124.46,73.66)=+5V,(124.46,76.2)=LO.
# D1 rot270 at (139.7,71.12): K top (139.7,67.31), A (139.7,74.93).
# C11 rot0 at (154.94,71.12): pins (154.94,67.31),(154.94,74.93).
sh = Sheet("pods_s_pump1.kicad_sch", PM)
sh.place("R2", "Device:R", "220",
         "Resistor_SMD:R_0603_1608Metric", 73.66, 73.66, rot=90)
sh.place("R3", "Device:R", "10K",
         "Resistor_SMD:R_0603_1608Metric", 86.36, 88.9, rot=0)
sh.place("Q1", "Transistor_FET:IRLZ44N", "IRLZ44N",
         "Package_TO_SOT_THT:TO-220-3_Vertical", 96.52, 73.66, rot=0)
sh.place("M1", "Connector_Generic:Conn_01x02", "PUMP-5V-1A", H2,
         129.54, 73.66, rot=0)
sh.place("D1", "Diode:1N5819", "1N5822",
         "Diode_THT:D_DO-41_SOD81_P7.62mm_Horizontal", 139.7, 71.12, rot=270)
sh.place("C11", "Device:C", "100n-50V",
         "Capacitor_SMD:C_0603_1608Metric", 154.94, 71.12, rot=0)
sh.emit_symbol("R2", lab="tb")
sh.emit_symbol("R3", lab="side")
sh.emit_symbol("Q1", lab="side")
sh.emit_symbol("M1", lab="tb")
sh.emit_symbol("D1", lab="side")
sh.emit_symbol("C11", lab="side")
# Gate drive + pulldown tap.
sh.wire([(48.26, 73.66), (69.85, 73.66)])
sh.glabel("PUMP_GATE", 48.26, 73.66)
sh.wire([(77.47, 73.66), (91.44, 73.66)])
sh.wire([(86.36, 85.09), (86.36, 73.66)])
sh.junction(86.36, 73.66)
sh.wire([(86.36, 92.71), (86.36, 101.6)])
sh.power("power:GND", 86.36, 111.76)
sh.wire([(86.36, 101.6), (86.36, 111.76)])
# Source drop + flag.
sh.wire([(99.06, 78.74), (99.06, 96.52)])
sh.power("power:GND", 99.06, 106.68)
sh.wire([(99.06, 96.52), (99.06, 106.68)])
sh.pwrflag(104.14, 88.9)
sh.wire([(99.06, 88.9), (104.14, 88.9)])
sh.junction(99.06, 88.9)
# PUMP_5V rail top: PUMP_5V global label (F2 branch), M1.1 stub,
# D1.K + C11 taps.
sh.glabel("PUMP_5V", 124.46, 53.34)
sh.wire([(124.46, 53.34), (124.46, 73.66)])
sh.wire([(124.46, 58.42), (154.94, 58.42)])
sh.junction(124.46, 58.42)
sh.wire([(139.7, 67.31), (139.7, 58.42)])
sh.junction(139.7, 58.42)
sh.wire([(154.94, 67.31), (154.94, 58.42)])
sh.pwrflag(132.08, 58.42)
sh.junction(132.08, 58.42)
# PUMP_LO: M1.2 -> Q1.D single polyline; D1.A + C11 bottom join it.
sh.wire([(99.06, 68.58), (114.3, 68.58), (114.3, 76.2), (124.46, 76.2),
         (139.7, 76.2), (139.7, 74.93)])
sh.wire([(154.94, 74.93), (154.94, 76.2), (139.7, 76.2)])
sh.junction(139.7, 76.2)
sh.note(["Pump XOR LED interlock (server-paced): combined 3.8A run",
         "browns out any single boost. D1 K faces the fused branch."],
        25.4, 139.7)
sh.write("pods_s_pump1.kicad_sch")
print("s_pump written")

# ================================================================== led
# R4 (73.66,73.66) rot90; R5 (86.36,88.9) rot0; Q2 (96.52,73.66) rot0;
# D2 strip rot0 at (129.54,73.66): A(124.46,73.66)=LED_5V, K(124.46,76.2).
sh = Sheet("pods_s_led1.kicad_sch", PM)
sh.place("R4", "Device:R", "220",
         "Resistor_SMD:R_0603_1608Metric", 73.66, 73.66, rot=90)
sh.place("R5", "Device:R", "10K",
         "Resistor_SMD:R_0603_1608Metric", 86.36, 88.9, rot=0)
sh.place("Q2", "Transistor_FET:IRLZ44N", "IRLZ44N",
         "Package_TO_SOT_THT:TO-220-3_Vertical", 96.52, 73.66, rot=0)
sh.place("D2", "Connector_Generic:Conn_01x02", "GROW-LED-5V-20CM-2A", H2,
         129.54, 73.66, rot=0)
sh.emit_symbol("R4", lab="tb")
sh.emit_symbol("R5", lab="side")
sh.emit_symbol("Q2", lab="side")
sh.emit_symbol("D2", lab="tb")
sh.wire([(48.26, 73.66), (69.85, 73.66)])
sh.glabel("LIGHT_GATE", 48.26, 73.66)
sh.wire([(77.47, 73.66), (91.44, 73.66)])
sh.wire([(86.36, 85.09), (86.36, 73.66)])
sh.junction(86.36, 73.66)
sh.wire([(86.36, 92.71), (86.36, 101.6)])
sh.power("power:GND", 86.36, 111.76)
sh.wire([(86.36, 101.6), (86.36, 111.76)])
sh.wire([(99.06, 78.74), (99.06, 96.52)])
sh.power("power:GND", 99.06, 106.68)
sh.wire([(99.06, 96.52), (99.06, 106.68)])
sh.pwrflag(104.14, 88.9)
sh.wire([(99.06, 88.9), (104.14, 88.9)])
sh.junction(99.06, 88.9)
# LED_5V from F3 branch: global label + stub to D2.A.
sh.glabel("LED_5V", 124.46, 53.34)
sh.wire([(124.46, 53.34), (124.46, 73.66)])
sh.pwrflag(129.54, 63.5)
sh.wire([(124.46, 63.5), (129.54, 63.5)])
sh.junction(124.46, 63.5)
# LED_LO: D2.K -> Q2.D single polyline.
sh.wire([(99.06, 68.58), (114.3, 68.58), (114.3, 76.2), (124.46, 76.2)])
sh.note(["20cm 2A strip on F3 2.5A branch. 24h light failsafe",
         "refreshes on every server ON (see 05-firmware)."],
        25.4, 139.7)
sh.write("pods_s_led1.kicad_sch")
print("s_led written")
