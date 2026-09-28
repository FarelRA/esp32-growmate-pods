# BEST pin map — ESP32-CAM header-only rotation (rev1)

Verified against: ESP32 datasheet (boot configs), ESP Hardware Design
Guidelines (strapping + ADC tables), AI-Thinker fixed camera/SD/PSRAM map.
No eFuse burn, no SD card, PSRAM kept for the camera. No soldering on
the module itself (header-only, no test-point taps); the carrier's
passives, headers, and SW1 solder as normal.

## Carrier map (U1 = Conn_02x08_Odd_Even, odd = left col, even = right col)

| U1 pin | Net | ESP GPIO | ADC | Why this pin |
|---|---|---|---|---|
| 1 | +5V | 5V rail | — | module supply in |
| 3 | GND | GND | — | |
| 5 | U0RXD | GPIO3 | — | flashing UART RX (edge to adapter TX) |
| 7 | U0TXD | GPIO1 | — | flashing UART TX (edge to adapter RX) |
| 9 | BOOT_IO0 | GPIO0 | ADC2_CH1 | XCLK inside module. 10k PU (R8) = HIGH = SPI boot. Tactile SW1 to GND = flash mode. Nothing else on GPIO0, ever. |
| 11 | +3V3 | 3V3 rail | — | sensor rail out (from module AMS1117) |
| 13, 15 | GND | GND | — | |
| 2 | PUMP_GATE | GPIO2 | ADC2_CH2 | output. Weak pulldown at reset + ext 10k (R3) = MOSFET OFF through the ~3 ms strapping window. GPIO2 strapping wants LOW/float: satisfied. |
| 4 | LIGHT_GATE | GPIO4 | ADC2_CH0 | output. Weak pulldown at reset + ext 10k (R5) = OFF at boot. GPIO4 is **not** a strapping pin (official list: 0, 2, 5, 12/MTDI, 15/MTDO). Shares the onboard flash LED: documented, kept as camera flash. |
| 8 | SOIL_AO | GPIO13 | ADC2_CH4 | input. No strapping, no boot-PWM: the safest analog pin on the header. |
| 10 | LIGHT_AO | GPIO14 | ADC2_CH6 | input. Not a strapping pin. GPIO14 carries the ROM debug PWM probe at boot, so the module AO comes through a 1k series (R6) + 100n to GND (C1): no contention, filtered ADC. |
| 12 | DHT_DATA | GPIO15 | ADC2_CH3 | digital. MTDO needs HIGH at boot: DHT idles HIGH and R1 (4.7k PU) holds it there. DHT is digital so the ADC2/WiFi conflict does not apply. |
| 14 | WATER_AO | GPIO33 | ADC1_CH5 | input. No strapping, no camera bus, and ADC1 reads fine with WiFi on. NOTE: the AI-Thinker red LED hangs on GPIO33 (active-low) — it pulls the high-Z ADC input toward 3V3 against R9 10k PD, shifting EMPTY offset and compressing span (LED may glow dimly); disclosed like the GPIO4 LED, absorbed by server end-calibration, ends must be captured in situ. R9 10k PD to GND so an open (empty-tank) probe reads ~0 instead of floating. |
| 16 | +5V | 5V rail | — | second 5V entry |

## Forbidden / reserved (do not use)

| Pins | Reason |
|---|---|
| 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39 | fixed OV2640 bus (`board_profile.c`). Firmware aborts if a sensor/actuator pin overlaps (`sensors_init`, `actuators_init`). |
| 1, 3 | UART0 flash/logs. Reclaim only if you give up serial flashing. |
| 6–11 | internal SPI flash. |
| 16, 17 | PSRAM (camera needs it; GPIO16 use = boot loops). |
| 12 as output / pull-up | MTDI must read LOW or the flash drops to 1.8 V and the board bricks until power-cycle with GPIO12 LOW. |
| SD slot (2, 4, 12, 13, 14, 15 in SD_MMC mode) | firmware never inits SD. All six are carrier GPIO instead. |
