# Wiring and bring-up

## Wire table (module header → carrier)

| U1 | Goes to | Wire |
|---|---|---|
| 5V (1, 16) | MT3608 5V out, pump +, LED strip + | red, star at U1 |
| GND (3, 13, 15) | common ground star | black |
| 3V3 (11) | J1-1, J2-1, J3-1, U2-VDD | orange |
| GPIO13 (8) | J2-2 soil AO | yellow |
| GPIO14 (10) | R6 → J3-2 light AO | yellow |
| GPIO12 (6) | unconnected — MTDI safety (float + weak pulldown = LOW) | — |
| GPIO33 (14) | J1-2 water AO; R9 to GND | yellow |
| GPIO15 (12) | U2-DATA; R1 to 3V3 | green |
| GPIO2 (2) | R2 gate network | blue |
| GPIO4 (4) | R4 gate network | blue |
| IO0 (9) | R8 to 3V3 + SW1 to GND | — |
| U0RXD (5), U0TXD (7) | J4 PROG header to flashing adapter (crossed: adapter TX → J4-1, adapter RX ← J4-2) | — |

## J4 PROG header (1×4, west of U1)

| J4 | Net | Goes to |
|---|---|---|
| 1 | U0RXD | U1-5 (GPIO3); adapter TX |
| 2 | U0TXD | U1-7 (GPIO1); adapter RX |
| 3 | GND | ground star |
| 4 | +5V | 5V rail (powers the module while flashing) |

Straight 1:1 copper to U1 (no crossings). Flash with IO0 held LOW
(SW1), then release for SPI boot.

## Complete net list (every net, every node)

Notation: `U1-5` = carrier header U1 pin 5. `Q1-G/D/S` = gate/drain/source.
`D1-A/K` = anode/cathode. U2 pins are VDD/DATA/GND (pin 3 is NC, open).

| Net | Nodes |
|---|---|
| +5V | MT3608 5V out → U1-1, U1-16, J4-4, M1-1, D1-K, D2-A, C2-1, C3-1 |
| +3V3 | U1-11 (module AMS1117 out) → J1-1, J2-1, J3-1, U2-VDD, R1-1, R8-1 |
| GND | star → U1-3, U1-13, U1-15, J4-3, J1-3, J2-3, J3-3, U2-GND, Q1-S, Q2-S, R3-2, R5-2, R9-2, SW1-2, C1-2, C2-2, C3-2 |
| U0RXD | U1-5 ↔ J4-1 (adapter TX) |
| U0TXD | U1-7 ↔ J4-2 (adapter RX) |
| BOOT_IO0 | U1-9 ↔ R8-2; U1-9 ↔ SW1-1 (SW1-2 → GND). R8-1 → +3V3 (10k PU = SPI boot HIGH) |
| PUMP_GATE | U1-2 (GPIO2) ↔ R2-1 |
| PUMP_GATE_Q | R2-2 ↔ R3-1 ↔ Q1-G (R3-2 → GND, 100k PD = OFF at boot) |
| PUMP_LO | Q1-D ↔ M1-2 ↔ D1-A |
| LIGHT_GATE | U1-4 (GPIO4) ↔ R4-1 |
| LIGHT_GATE_Q | R4-2 ↔ R5-1 ↔ Q2-G (R5-2 → GND, 100k PD = OFF at boot) |
| LED_LO | Q2-D ↔ D2-K |
| DHT_DATA | U1-12 (GPIO15) ↔ U2-DATA ↔ R1-2 (R1-1 → +3V3, 4.7k PU = MTDO HIGH) |
| WATER_AO | U1-14 (GPIO33) ↔ J1-2 ↔ R9-1 (R9-2 → GND, 10k PD; open probe reads ~0) |
| GPIO12_NC | U1-6 unconnected (float + weak pulldown = MTDI LOW, strapping safe) |
| SOIL_AO | U1-8 (GPIO13) ↔ J2-2 |
| LIGHT_MOD_AO | J3-2 ↔ R6-1 |
| LIGHT_AO | U1-10 (GPIO14) ↔ R6-2 ↔ C1-1 (C1-2 → GND, 1k + 100n filter) |

U2 pin 3: No-Connect, left open.

## Parts list (23 refs)

| Ref | Value | Footprint | Function |
|---|---|---|---|
| U1 | ESP32-CAM | PinHeader_2x08_P2.54mm | AI-Thinker module (header) |
| U2 | DHT22 | DHT22 | temp + humidity |
| J1 | WATER | PinHeader_1x03_P2.54mm | tank probe: 1 VCC, 2 AO, 3 GND |
| J2 | SOIL | PinHeader_1x03_P2.54mm | soil probe: 1 3V3, 2 AO, 3 GND |
| J3 | LIGHT-SENS | PinHeader_1x03_P2.54mm | light module: 1 3V3, 2 AO, 3 GND |
| J4 | PROG | PinHeader_1x04_P2.54mm | flash header: 1 RX, 2 TX, 3 GND, 4 5V |
| SW1 | BOOT | SW_Tactile_SPST | IO0 → GND for download mode |
| Q1, Q2 | IRLZ44N | TO-220-3 | low-side N-MOSFETs, pump + light |
| M1 | PUMP-5V | Motor_DC | 5 V micro submersible pump |
| D1 | 1N5819 | DO-41 | pump flyback (K→+5V, A→PUMP_LO) |
| D2 | GROW-LED-5V | LED_Strip | full-spectrum strip (A→+5V, K→LED_LO) |
| R1 | 4.7k | R_0603 | DHT pull-up (MTDO-HIGH strapping) |
| R2, R4 | 220 | R_0603 | MOSFET gate stoppers |
| R3, R5 | 100k | R_0603 | gate pulldowns (OFF at boot) |
| R6 | 1k | R_0603 | light-AO series (boot-PWM contention) |
| R8 | 10k | R_0603 | BOOT pull-up (SPI boot) |
| R9 | 10k | R_0603 | water-AO pulldown (open probe reads ~0 = EMPTY) |
| C1 | 100n | C_0603 | light-AO filter |
| C2 | 1000u | CP_Radial_D8_P3.5 | 5V bulk at module entry |
| C3 | 100n | C_0603 | 5V ceramic at module entry |

Color code: red 5V, orange 3V3, black GND, yellow analog, green digital,
blue gate drive.

## Bring-up order (do not skip)

1. **Bare module first.** Flash `factory` (07-operations), connect serial,
   confirm boot log and onboarding AP. No sensors yet.
2. **Power.** Pack → boost → 5V at U1-1/16 (4.9–5.2 V), 3V3 at U1-11.
   Confirm no heat, no reboot loop.
3. **One sensor at a time** (soil → light → water → DHT), checking each
   reading in the log before adding the next.
4. **Actuators with no load**: gate nodes switch 0/3.3 V on command;
   then connect pump, then LED strip.
5. **Soak**: 30 min of 15 s cycles, WiFi on, camera every 15 min.
   Watch `consecutive_failures` and heap.

## Hard rules

- No wire to camera pins (01-pinout forbidden list). The firmware aborts
  on overlap — treat an abort here as a wiring bug, not a firmware bug.
- SD slot stays empty and uninitialized.
- GPIO12 (U1-6) stays unconnected, **always** — it is the MTDI strapping
  pin and must float LOW at boot. If the board fails to boot after wiring,
  a stray connection on U1-6 is suspect #1.
- Keep pump/LED high-current loops short and away from DHT/sensor leads.
