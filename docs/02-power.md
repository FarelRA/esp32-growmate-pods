# Power chain

```
USB-C 5V ──┬──► MT3608 boost ──► +5V rail ──► ESP 5V, M1 pump, D2 LED strip
           │
BLP673 3.85V 4230mAh ──► TP4056 (Type-C, DW01A) ── charge path ──► pack
           │
           └── system always runs from the pack via boost (UPS style)
```

- Charge: TP4056 to 4.2 V. System load stays on the pack output, so
  charging just takes longer under load — no load-share switcher needed
  for rev1, and charge termination stays safe.
- Boost: MT3608 5 V / 2 A setting from pack voltage (3.0–4.2 V).
  Budget: ESP peak ~800 mA (WiFi + camera) + pump 200–500 mA + LED strip
  per its module rating. Size the strip so the total stays under ~1.6 A.
- Rails: sensors from module 3V3 **only** (AMS1117). Never 5 V into a GPIO
  or a 3V3 sensor header. Pump and LED strip on +5V only.
- Decoupling: C2 1000 µ bulk + C3 100 n ceramic at the module 5V entry;
  100 n at the DHT VCC; C1 100 n doubles as the light-AO filter.
- Brownout rule: short, thick USB cable or charged pack before flashing;
  random reboots / camera-init failures / WiFi drops = power first suspect.
- Test points: +5V at U1-1/16, +3V3 at U1-11, GND at U1-3/13/15.
