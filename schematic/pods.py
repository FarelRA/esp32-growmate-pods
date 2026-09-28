#!/usr/bin/env python3
"""GrowMate Pods carrier — SKiDL source of truth (pre-alpha, rev A).

Generates: pods.net (PCB import), pods.xml (BOM input), pods.kicad_sch
(draft -> manual KiCad placement pass -> release), bom.csv.

Micro-sheets (2-6 parts each): the SKiDL auto-placer only produces
clean output on small blocks, so each functional leg is its own
subcircuit/sheet. IEEE flow (power top, GND bottom, left-to-right) is
enforced in the KiCad pass; this script guarantees connectivity + ERC.

Build truth: docs/04-wiring.md. Fuses for measured loads (pump 1 A /
2 A stall, LED 20 cm 2 A): F1 5 A (2920), F2 1.5 A, F3 2.5 A (1812).

OPERATING RULE (on-sheet note + 02-power): pump and LED NEVER on
together (server paces; combined load exceeds any single-boost budget
and browns out the rail). Charge idle/low-duty (TP4056 no load-share).

Run:  KICAD10_SYMBOL_DIR=/usr/share/kicad/symbols \
       /tmp/opencode/venv/bin/python pods.py
"""

from skidl import *

set_default_tool(KICAD10)
lib_search_paths[KICAD10].append("/usr/share/kicad/symbols")

# ---------------------------------------------------------------- nets
gnd = Net("GND")
gnd.drive = POWER
# Cell return: BT1-2 + CHG1 B- ONLY. This net must never touch the GND
# star: the DW01A/FS8205A low-side FET sits between B- and OUT-, and
# commoning them defeats over-discharge protection. GND star = OUT-.
cell_neg = Net("CELL_NEG")
p5v = Net("+5V")
p5v.drive = POWER
p3v3 = Net("+3V3")
p3v3.drive = POWER

pack_p = Net("PACK_P")
sys_pack = Net("SYS_PACK")
bst_in = Net("BST_IN")
pump_5v = Net("PUMP_5V")
led_5v = Net("LED_5V")
u0rxd = Net("U0RXD")
u0txd = Net("U0TXD")
boot_io0 = Net("BOOT_IO0")
pump_gate = Net("PUMP_GATE")
pump_gate_q = Net("PUMP_GATE_Q")
pump_lo = Net("PUMP_LO")
light_gate = Net("LIGHT_GATE")
light_gate_q = Net("LIGHT_GATE_Q")
led_lo = Net("LED_LO")
dht_data = Net("DHT_DATA")
water_ao = Net("WATER_AO")
water_adc = Net("WATER_ADC")
soil_ao = Net("SOIL_AO")
soil_adc = Net("SOIL_ADC")
light_mod_ao = Net("LIGHT_MOD_AO")
light_ao = Net("LIGHT_AO")
tank_vcc = Net("TANK_VCC")
gpio12_nc = Net("GPIO12_NC")
gpio12_nc.do_erc = False  # MTDI strapping, intentionally unconnected

BOM = []


def mk(part, ref, value, footprint, function):
    part.ref = ref
    part.value = value
    part.footprint = footprint
    BOM.append((ref, value, footprint, function))
    return part


def R(ref, value, function, horiz=False):
    p = mk(Part("Device", "R", value=value,
                footprint="Resistor_SMD:R_0603_1608Metric"),
           ref, value, "Resistor_SMD:R_0603_1608Metric", function)
    if horiz:
        p.symtx = "H"
    return p


def C(ref, value, function):
    return mk(Part("Device", "C", value=value,
                   footprint="Capacitor_SMD:C_0603_1608Metric"),
              ref, value, "Capacitor_SMD:C_0603_1608Metric", function)


HDR3 = "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical"
HDR2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"


@subcircuit
def s_charge():
    chg1 = mk(Part("Connector_Generic", "Conn_01x04"),
              "CHG1", "TP4056-TypeC-DW01A",
              "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
              "charger module (5 V in via on-board USB-C; load on OUT)")
    chg1[1] += pack_p
    chg1[2] += cell_neg
    chg1[3] += sys_pack
    chg1[4] += gnd
    bt1 = mk(Part("Connector_Generic", "Conn_01x02"), "BT1",
             "BLP673-4.2Ah", HDR2,
             "LiPo pouch BLP673 3.85 V 4230 mAh (cell return = CELL_NEG)")
    bt1[1] += pack_p
    bt1[2] += cell_neg


@subcircuit
def s_feed_fuse():
    f1 = mk(Part("Device", "Polyfuse", value="5A-hold",
                 footprint="Fuse:Fuse_2920_7451Metric"),
            "F1", "Polyfuse-5A-hold", "Fuse:Fuse_2920_7451Metric",
            "pack overcurrent (interlocked max ~5.5 A in at 3.0 V)")
    f1[1] += sys_pack
    f1[2] += bst_in


@subcircuit
def s_boost():
    bst1 = mk(Part("Connector_Generic", "Conn_01x04"), "BST1",
              "XL6009-5V-4A",
              "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
              "boost module 4 A (3.0-4.2 V in; heatsink on for 3 A+ sustained)")
    bst1[1] += bst_in
    bst1[2] += gnd
    bst1[3] += p5v
    bst1[4] += gnd
    c2 = mk(Part("Device", "C", value="1000u",
                 footprint="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm"),
            "C2", "1000u-10V", "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm",
            "5 V bulk at module entry")
    c2[1] += p5v
    c2[2] += gnd
    c3 = C("C3", "100n-50V", "5 V ceramic at module entry")
    c3[1] += p5v
    c3[2] += gnd


@subcircuit
def s_branches():
    f2 = mk(Part("Device", "Polyfuse", value="1.5A-hold",
                 footprint="Fuse:Fuse_1812_4532Metric"),
            "F2", "Polyfuse-1.5A-hold", "Fuse:Fuse_1812_4532Metric",
            "pump branch (1 A run / 2 A stall)")
    f2[1] += p5v
    f2[2] += pump_5v
    f3 = mk(Part("Device", "Polyfuse", value="2.5A-hold",
                 footprint="Fuse:Fuse_1812_4532Metric"),
            "F3", "Polyfuse-2.5A-hold", "Fuse:Fuse_1812_4532Metric",
            "LED branch (2 A strip, 1.25x rule)")
    f3[1] += p5v
    f3[2] += led_5v


@subcircuit
def s_mcu():
    u1a = mk(Part("Connector_Generic", "Conn_01x08"), "U1A",
             "ESP32-CAM-ODD",
             "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
             "AI-Thinker module, odd row (U1-1..15)")
    u1a[1] += p5v
    u1a[2] += gnd
    u1a[3] += u0rxd
    u1a[4] += u0txd
    u1a[5] += boot_io0
    u1a[6] += p3v3
    u1a[7] += gnd
    u1a[8] += gnd
    u1b = mk(Part("Connector_Generic", "Conn_01x08"), "U1B",
             "ESP32-CAM-EVEN",
             "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
             "AI-Thinker module, even row (U1-2..16)")
    u1b[1] += pump_gate
    u1b[2] += light_gate
    u1b[3] += gpio12_nc
    u1b[4] += soil_adc
    u1b[5] += light_ao
    u1b[6] += dht_data
    u1b[7] += water_adc
    u1b[8] += p5v
    r8 = R("R8", "10K", "BOOT pull-up (SPI boot HIGH)")
    r8[1] += p3v3
    r8[2] += boot_io0
    sw1 = mk(Part("Switch", "SW_Push",
                  footprint="Button_Switch_THT:SW_PUSH-12mm"),
             "SW1", "BOOT-TACT", "Button_Switch_THT:SW_PUSH-12mm",
             "IO0 to GND for download mode")
    sw1[1] += boot_io0
    sw1[2] += gnd
    j4 = mk(Part("Connector_Generic", "Conn_01x04"), "J4", "PROG-1x04",
            "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
            "flash header (adapter TX->J4-1, RX<-J4-2)")
    j4[1] += u0rxd
    j4[2] += u0txd
    j4[3] += gnd
    j4[4] += p5v


@subcircuit
def s_water():
    j1 = mk(Part("Connector_Generic", "Conn_01x03"), "J1", "WATER-1x03",
            HDR3, "tank probe (VCC/AO/GND)")
    j1[1] += tank_vcc
    j1[2] += water_ao
    j1[3] += gnd
    r7 = R("R7", "330", "tank VCC series (dead-short -> 10 mA)")
    r7[1] += p3v3
    r7[2] += tank_vcc
    c6 = C("C6", "100n-50V", "tank header decoupling")
    c6[1] += tank_vcc
    c6[2] += gnd
    r9 = R("R9", "10K", "water pulldown (open probe ~0 = EMPTY)")
    r9[1] += water_ao
    r9[2] += gnd
    r11 = R("R11", "1K", "water input series", horiz=True)
    r11[1] += water_ao
    r11[2] += water_adc
    c10 = C("C10", "100n-50V", "water input filter")
    c10[1] += water_adc
    c10[2] += gnd


@subcircuit
def s_soil():
    j2 = mk(Part("Connector_Generic", "Conn_01x03"), "J2", "SOIL-1x03",
            HDR3, "soil probe (3V3/AO/GND)")
    j2[1] += p3v3
    j2[2] += soil_ao
    j2[3] += gnd
    c7 = C("C7", "100n-50V", "soil header decoupling")
    c7[1] += p3v3
    c7[2] += gnd
    r10 = R("R10", "1K", "soil input series", horiz=True)
    r10[1] += soil_ao
    r10[2] += soil_adc
    c9 = C("C9", "100n-50V", "soil input filter")
    c9[1] += soil_adc
    c9[2] += gnd


@subcircuit
def s_lightsens():
    j3 = mk(Part("Connector_Generic", "Conn_01x03"), "J3",
            "LIGHT-SENS-1x03", HDR3, "light module (3V3/AO/GND)")
    j3[1] += p3v3
    j3[2] += light_mod_ao
    j3[3] += gnd
    c8 = C("C8", "100n-50V", "light header decoupling")
    c8[1] += p3v3
    c8[2] += gnd
    r6 = R("R6", "1K", "light series (boot-PWM contention)", horiz=True)
    r6[1] += light_mod_ao
    r6[2] += light_ao
    c1 = C("C1", "100n-50V", "light-AO filter")
    c1[1] += light_ao
    c1[2] += gnd


@subcircuit
def s_dht():
    u2 = mk(Part("Sensor", "DHT11"), "U2", "DHT22",
            "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
            "temp+humidity (DHT11 symbol, DHT22 pin-compatible)")
    u2["VDD"] += p3v3
    u2["DATA"] += dht_data
    u2["GND"] += gnd
    r1 = R("R1", "4.7K", "DHT pull-up (MTDO-HIGH strapping)")
    r1[1] += p3v3
    r1[2] += dht_data
    c4 = C("C4", "100n-50V", "DHT VCC decoupling at U2")
    c4[1] += p3v3
    c4[2] += gnd
    c5 = C("C5", "10u-6V3", "+3V3 bulk at U1-11")
    c5[1] += p3v3
    c5[2] += gnd


@subcircuit
def s_pump():
    r2 = R("R2", "220", "pump gate stopper", horiz=True)
    r2[1] += pump_gate
    r2[2] += pump_gate_q
    r3 = R("R3", "10K", "pump gate pulldown (OFF at boot)")
    r3[1] += pump_gate_q
    r3[2] += gnd
    q1 = mk(Part("Transistor_FET", "IRLZ44N",
                 footprint="Package_TO_SOT_THT:TO-220-3_Vertical"),
            "Q1", "IRLZ44N",
            "Package_TO_SOT_THT:TO-220-3_Vertical",
            "pump low-side N-MOSFET (logic-level)")
    q1["G"] += pump_gate_q
    q1["D"] += pump_lo
    q1["S"] += gnd
    m1 = mk(Part("Connector_Generic", "Conn_01x02"), "M1",
            "PUMP-5V-1A", HDR2,
            "micro pump (1 A run / 2 A stall, off-board)")
    m1[1] += pump_5v
    m1[2] += pump_lo
    d1 = mk(Part("Diode", "1N5819",
                 footprint="Diode_THT:D_DO-41_SOD81_P7.62mm_Horizontal"),
            "D1", "1N5822",
            "Diode_THT:D_DO-41_SOD81_P7.62mm_Horizontal",
            "pump flyback 3A class (K to PUMP_5V branch)")
    d1["K"] += pump_5v
    d1["A"] += pump_lo
    c11 = C("C11", "100n-50V", "pump terminal RF shunt")
    c11[1] += pump_5v
    c11[2] += pump_lo


@subcircuit
def s_led():
    r4 = R("R4", "220", "light gate stopper", horiz=True)
    r4[1] += light_gate
    r4[2] += light_gate_q
    r5 = R("R5", "10K", "light gate pulldown (OFF at boot)")
    r5[1] += light_gate_q
    r5[2] += gnd
    q2 = mk(Part("Transistor_FET", "IRLZ44N",
                 footprint="Package_TO_SOT_THT:TO-220-3_Vertical"),
            "Q2", "IRLZ44N",
            "Package_TO_SOT_THT:TO-220-3_Vertical",
            "light low-side N-MOSFET (logic-level)")
    q2["G"] += light_gate_q
    q2["D"] += led_lo
    q2["S"] += gnd
    d2 = mk(Part("Connector_Generic", "Conn_01x02"), "D2",
            "GROW-LED-5V-20CM-2A", HDR2,
            "full-spectrum strip (A to fused +5V, K to LED_LO)")
    d2[1] += led_5v
    d2[2] += led_lo


s_charge()
s_feed_fuse()
s_boost()
s_branches()
s_mcu()
s_water()
s_soil()
s_lightsens()
s_dht()
s_pump()
s_led()

# ---------------------------------------------------------------- out
ERC()
generate_netlist()
generate_xml()
generate_schematic(title="GrowMate Pods carrier rev A", auto_stub=True,
                    flatness=1.0)  # flat draft: hierarchical mode would
# clobber the placed pods_s_*.kicad_sch sheets (same filenames).

with open("bom.csv", "w") as f:
    f.write("ref,value,footprint,function\n")
    for ref, value, fp, fn in sorted(BOM):
        f.write(f'{ref},"{value}","{fp}","{fn}"\n')
print(f"BOM: {len(BOM)} line items -> bom.csv")
