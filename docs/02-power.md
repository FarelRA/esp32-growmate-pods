# Power chain

```
USB-C 5V ──┬──► MT3608 boost ──► +5V rail ──► ESP 5V, M1 pump, D2 LED strip
           │
BLP673 3.85V 4230mAh ──► TP4056 (Type-C, DW01A) ── charge path ──► pack
           │
           └── system always runs from the pack via boost (UPS style)
```

- Charge: TP4056 to 4.2 V. WARNING: with system load on the pack
  during charge, termination taper never completes — the cell floats
  at 4.20 V indefinitely (DW01A 4.30 V backs it up, but the pouch ages
  fast). Operating rule rev1: charge with the pod idle/low-duty, and
  verify the load hangs on OUT± (never B±, or DW01A discharge
  protection is bypassed too). Rev2: real power-path (MCP73871 or
  P-FET load-share).
- Boost: MT3608 5 V / 2 A setting from pack voltage (3.0–4.2 V).
  Budget (measured loads): ESP peak ~800 mA (WiFi + camera) + pump
  1 A run / 2 A stall + LED strip 20 cm 2 A. Pump and LED are NEVER
  on together (server-paced interlock — combined 3.8 A run / 4.8 A
  stall exceeds the 2 A setting; rev2 upsize to >= 5 A boost).
  Interlocked worst case (ESP + LED) is 2.8 A @ 5 V ≈ 5.5 A in at a
  3.0 V pack: F1 is 5 A hold (2920) and the pack→boost run is 1.5 mm².
- Rails: sensors from module 3V3 **only** (AMS1117). Never 5 V into a GPIO
  or a 3V3 sensor header. Pump and LED strip on +5V only.
- Decoupling: C2 1000 µ bulk + C3 100 n ceramic at the module 5V entry;
  100 n at the DHT VCC; C1 100 n doubles as the light-AO filter.
  Add: 10 µF at +3V3 (U1-11), 100 nF at each probe header VCC/GND,
  1 kΩ + 100 nF footprints at soil (U1-8) and water (U1-14) inputs,
  100 nF across the pump terminals, 330 Ω in series with the tank-probe
  VCC feed (dead-short → 10 mA, not a rail collapse).
- Brownout rule: short, thick USB cable or charged pack before flashing;
  random reboots / camera-init failures / WiFi drops = power first suspect.
- Test points: +5V at U1-1/16, +3V3 at U1-11, GND at U1-3/13/15.
