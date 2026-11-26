# F-91W BLE watch firmware

Firmware for the rev A STM32WB55REV6 (512 KB) replacement mainboard (see the hardware notes in the project
files). It keeps time on the stock F-91W LCD, counts steps with the LIS2DW12, takes heart-rate
spot checks with the MAXM86161 flex, and syncs over BLE on demand.

Pins come from `board/board_f91w_ble.h`, which was generated from the routed board
(`pcb-layout/fab`). The pin table in the older hardware README predates layout and is out of
date; use this one:

| Function | Pin | Notes |
|---|---|---|
| MODE / LIGHT / ALARM buttons | PC13 / PA11 / PA0 | |
| Buzzer | PC1 | LPTIM1_OUT, drives L3/Q1 |
| PPG boost EN (flex A0) | PC0 | TPS610995 EN, 1M pull-down on the flex |
| PPG INT (flex A1) | PC2 | MAXM86161 INTB, open drain, 100k pull-up on the flex |
| Flex A2 | PB2 | spare |
| I2C SCL / SDA | PB8 / PB9 | LIS2DW12 0x19, MAXM86161 0x62 |
| Accelerometer INT1 / INT2 | PA5 / PE4 | |
| LED R / G / B (active low) | PA12 / PD0 / PD1 | |
| LCD bias (VLCD) | PC3 | |
| LSE 32.768 kHz | PC14 / PC15 | |
| SWDIO / SWCLK | PA13 / PA14 | on J2, see Flashing |
| BOOT0 | PH3 | |

## Using the watch

| Screen | Shows | ALARM |
|---|---|---|
| TIME | weekday, date, `HH:MM:SS` | hold: 12/24 h |
| STEPS | `St` + steps today | – |
| HEART | `HR` + last reading, or countdown while measuring | press: start a reading (keep still ~11 s) |
| BLE | `bL` + `oFF` / `AdV` / `Conn` | press: advertise, stop, or disconnect |

- **MODE** cycles the screens. Other screens fall back to TIME after a minute.
- **LIGHT** turns the backlight on for 3 s. Holding it for 1.5 s starts BLE advertising from any screen.
- While pairing, the watch shows `PA` and the 6-digit passkey to type on the phone or PC.
- Heart-rate errors: `no Con` (no skin contact), `MovEd`, `retry`, `no PPG` (flex not answering).

Steps reset at local midnight; the last 7 days are kept. Steps, history, settings and the last
reading live in RTC backup registers, so they survive a reset but not a battery change.

## Companion

- `companion/index.html`: Web Bluetooth page (Chrome or Edge on desktop/Android). Open it from a
  local file or any HTTPS host: connect, set the clock, see steps and history, measure heart rate,
  change settings, find the watch.
- `companion/f91w.py`: command line (`pip install bleak`). Same features, plus
  `f91w.py ppg out.csv` to log raw PPG samples during a reading, which is the data needed to
  tune the HR algorithm on real wrists.

The protocol is in [docs/BLE.md](docs/BLE.md).

## Building

```sh
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi   # or Arm GNU Toolchain
scripts/fetch_cubewb.sh        # STM32CubeWB v1.24.0 into ../third_party (sparse, ~250 MB)
make                           # build/f91w.hex, linked at 0x08000000 for SWD
make OTA=1 BUILD=build-ota     # linked at 0x08007000 for ST's BLE_Ota loader
make NO_LED=1 NO_BUZZER=1      # first boards without D1 / L3 (the default build also tolerates them missing)
make test                      # host tests: step counter, LCD font, calendar, HR algorithm
```

Set `CUBEWB=/path/to/STM32CubeWB` if you already have it.

## Flashing (SWD on J2 + ST-Link)

J2 is not a Tag-Connect footprint: it is 5 bare 0.8 mm pads at 1.0 mm pitch at (-9.3, 2.6) mm,
on the battery side. Use pogo pins or tack fine wires on, and wire them to the ST-Link:

| J2 pad | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| Signal | VDD (target sense) | SWDIO (PA13) | SWCLK (PA14) | NRST | GND |

The STM32WB has two cores. CPU2 runs ST's BLE stack, which has to be installed once with
STM32CubeProgrammer before this app can use BLE:

1. Install the BLE stack on CPU2 (from `third_party/STM32CubeWB/Projects/STM32WB_Copro_Wireless_Binaries/STM32WB5x`).
   On the 512 KB REV6 the v1.24.0 full stack installs at 0x0805C000 (the "STM32WB5xxE(512K)" column of the
   release notes):
   `STM32_Programmer_CLI -c port=SWD mode=UR -fwupgrade stm32wb5x_BLE_Stack_full_fw.bin 0x0805C000 firstinstall=0`
   Upgrade the FUS first if the release notes ask for it (FUS v2.2.0 installs at 0x0807A000 on 512 KB parts).
   The app's linker script stops at 0x08050000, below every stack version listed there.
2. Flash the app: `make flash` (connects under reset, because the app sleeps in Stop2 and the SWD
   port is off while it sleeps).

Without the CPU2 stack the watch still runs (time, steps, heart rate); BLE just never starts.

### BLE OTA (optional)

1. Flash ST's `BLE_Ota` loader (from the same CubeWB package) at 0x08000000.
2. Flash `make OTA=1` at 0x08007000.
3. Later updates: `companion/f91w.py ota` reboots into the loader, then send `build-ota/f91w.bin`
   with ST BLE Toolbox (or STM32CubeMonitor-RF).

ST's loader is built for the Nucleo and sets up its LED and button pins (PB0, PB1, PB5, PC4, PD0, PD1).
On this board PB5 and PC4 are LCD lines and PD0/PD1 are the green/blue backlight, so check it behaves on
the first board, and rebuild it without the BSP calls if it does not.

## Layout

| Path | What |
|---|---|
| `app/` | watch UI (`watch_app.c`), LCD font/segment map, step counter, calendar |
| `drivers/` | board GPIO, LCD controller, I2C, LIS2DW12, MAXM86161, buzzer (LPTIM1), battery (VREFINT) |
| `hr/` | heart-rate algorithm (portable C, host test) |
| `STM32_WPAN/App/` | BLE app (from ST's BLE_HeartRate example) and `watch_service.c` (our GATT services) |
| `Core/` | startup, clocks, low-power and timer server, adapted from ST's BLE_HeartRate example |
| `tests/` | host tests |

ST-derived files keep their ST licence headers. The LCD font and segment map are from
second-movement (MIT, Joey Castillo).

## Power notes

The app sleeps in Stop2 between events. Wake-ups: the 1 Hz RTC alarm (display), the LIS2DW12
FIFO every 2 s (12.5 Hz low-power mode), buttons, and BLE. The LCD runs from the LSE in Stop2.
Advertising stops after 60 s. Measure the real current on the first boards; the hardware notes
estimate 7-16 µA average with steps, 20 HR checks a day and on-demand sync.

## Verify on the first board

These are written from datasheets and have only been compiled, not run on hardware:

- LCD: contrast level (default 4 of 0-7) and that every segment maps right (`disp_text(0, "8888888888")`).
- MAXM86161: register values (cross-checked against MikroE's Heart Rate 2 driver), LED current
  (default 20 mA), and the A_FULL interrupt on A1.
- Buzzer tone and loudness through L3/Q1 (LPTIM1 on PC1, 2-4 kHz from the 32 kHz LSE).
- LIS2DW12 step counter on a real wrist (the thresholds are tuned on synthetic data).
- RF: TX power is -0.15 dBm (`CFG_TX_POWER`); revisit after the antenna is tuned.
