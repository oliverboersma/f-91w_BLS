# F-91W mod: heart rate + steps + programmable — build plan v1

_Researched 2026-10-02._

> **Update 2026-10-02 (later the same day): Bluetooth added, which changes the architecture.**
>
> - **New base:** a custom **STM32WB55RGV6 mainboard**. It drives the stock LCD, has BLE built in, and keeps the Sensor-Watch-compatible 9-pin sensor connector. It replaces the Sensor Watch Pro.
> - **What's superseded:** sections 1 and 6.1 below (the Sensor Watch Pro base).
> - **What still applies:** the PPG flex, MAXM86161 choice, caseback approach, power reasoning and HR plan.
> - **Rev A schematics, verification, BOMs, pin map and a tested HR algorithm** are in `claude/f91w-ble-hardware-revA.md`.
> - **Generator + firmware source** is in `claude/f91w-hw-revA-source.md`.
> - **Package delivered to Oliver:** `f91w-hw-revA.zip`.

## TL;DR

- **Programmable:** Sensor Watch Pro, a drop-in replacement for the F-91W module PCB. Available off the shelf.
- **Steps:** LIS2DW12 accelerometer + `step_counter_face` from the voloved/second-movement fork. Available off the shelf.
- **Heart rate:** this is the actual engineering. It needs a custom flex PCB (MAXM86161 PPG + boost + LIS2DW12) on the 9-pin sensor connector, plus a custom caseback with a sensor window. Aim for **on-demand spot checks**, because continuous HR won't work on a CR2016.

---

## 1. Base board: Sensor Watch Pro

- Crowd Supply: board $59, accelerometer board $12, optional custom 92-segment LCD $16. In stock as of 2026-10-02.
- Microchip SAM L22: Cortex-M0+, up to 32 MHz, 256 KB flash, 32 KB RAM. Its on-chip segment LCD controller drives the stock F-91W LCD.
- USB micro-B with a UF2 bootloader: double-tap reset and a `WATCHBOOT` drive appears. **Remove the coin cell before plugging in USB** (the board silkscreen says so too).
- Runs on a CR2016. Original Sensor Watch figures: ~4 µA asleep, ~18 µA awake, estimated 430–667 days per cell.
- Buzzer: it keeps using the caseback piezo. You move the original buzzer clip over to the new board with a soldering iron.

### 9-pin sensor connector (Hirose FH19C-9S-0.5SH, 0.5 mm pitch)

| Pin | MCU | Capabilities |
|---|---|---|
| 3V | battery direct | 3.08 V nominal, ~2.7 V near end of life |
| GND | | |
| SCL / SDA | SERCOM1 | I²C, pull-ups on main board |
| A0 | PB04 | EXTINT4, AIN12 |
| A1 | PB01 | EXTINT1, AIN9, SPI SCK, UART RX |
| A2 | PB02 | EXTINT2, AIN10, SPI MOSI, UART TX/RX, RTC wake |
| A3 | PB03 | EXTINT3, AIN11, SPI CS |
| A4 | PB00 | EXTINT0, AIN8, SPI MISO, UART TX/RX, RTC wake |

- Sensor-board envelope: **5.7 × 5.7 × 1 mm**, on an L-shaped flex (0.19 mm flex, with a stiffener at the contacts). Only QFN/LGA parts fit.
- Official accelerometer board wiring, read from the OSO-SWAB-B1-00 KiCad nets: LIS2DW12 at I²C 0x19, **INT1 → A4, INT2 → A3**, A1/A2 brought out as UART RX/TX test pads, A0 unused.

---

## 2. Custom "Motion + PPG" board (v1 proposal)

There is only one connector, so this board **replaces** the accelerometer board and has to carry the accelerometer itself.

| Function | Part | Wiring | Why this part |
|---|---|---|---|
| Accel / steps | LIS2DW12 (LGA-12, 2×2×0.7 mm) | I²C 0x19, INT1→A4, INT2→A3 | Wired the same as the official board, so the existing step code runs unchanged |
| PPG | MAXM86161 (OLGA-14, 2.9×4.3×1.4 mm) | I²C 0x62, INTB→A1 (open-drain, use the internal pull-up) | Green 520–535 nm, red and IR LEDs plus photodiode and AFE in one part. Single supply VLED 3.0–5.5 V with an internal 1.8 V LDO. SDA/SCL/INTB rated to 6 V abs max, so it's fine on the 3 V bus and safe when unpowered. <10 µA AFE at 25 sps, 128-sample FIFO, 0.05–0.3 µA shutdown |
| LED rail | TPS61099 boost (WCSP 1.23×0.88 mm), set to ~3.6–4.0 V | EN ← A0 | The coin cell gives 2.7–3.08 V, which is below the MAXM86161's 3.0 V minimum, and the green LED needs headroom. ~1 µA Iq, true load disconnect when off |
| Energy buffer | ≥47 µF on VLED; 10–22 Ω + 22–47 µF between the 3V pin and the boost input | — | A 50 mA × 117 µs LED pulse drops VLED ~125 mV on 47 µF (~60 mV on 100 µF). The RC keeps boost and LED current peaks off the coin cell so the MCU doesn't brown out |
| Debug | A2 as a UART TX test pad | — | printf debugging |

**Layout:** the connector tail and pocket region (LIS2DW12, boost, passives) stay inside the official outline. A second flex arm carries the MAXM86161 on a stiffener down to the caseback, with a service loop. Start from Joey's KiCad project `PCB/Sensor Boards/Kicad/OSO-SWAB-B1-00 Sensor Watch Accelerometer`, which already has the outline and the FH19C mating contacts. Get it fabbed and assembled as flex PCBA at JLCPCB or PCBWay. MAXM86161EFD+ is stocked at DigiKey and listed at LCSC (C3178309).

**PPG parts considered and rejected:**
- MAX30101 / MAX30102: 1.55 mm thick and need two rails (1.8 V VDD plus 3.1 V+ VLED). The MAX30102 has no green LED.
- MAXM86146: 0.88 mm thin and wrist-optimized, but it needs external LEDs and a 1.8 V AFE rail, comes in a 38-pin OLGA, and expects a KX122 accelerometer on SPI for motion compensation.
- ROHM BH1792GLC: would run straight off the cell (2.5–3.6 V), but it's at Last Time Buy.

---

## 3. Mechanical

- The PPG module is 1.4 mm thick (over the 1 mm envelope) and has to touch skin through the back. It goes **in the caseback**, not in the sensor-board pocket.
- Build a custom caseback with a sensor bump, like every smartwatch has:
  - Two apertures, one for the LED and one for the PD, separated by opaque material. If they share one window, light bounces off it straight into the photodiode.
  - The sensor sits flush or slightly proud, for contact pressure. The F-91W is light and usually worn loose, which is bad for PPG.
- Prototype in black SLA resin, since it's cheap and quick to iterate. Leave the apertures open or fill them with optically clear epoxy. Final version can be resin or CNC aluminum.
- Re-glue the piezo disc (or a smaller one) away from the window so the buzzer still works.
- **Measure on the real watch before CAD:**
  - caseback thickness
  - piezo diameter and position
  - where the sensor pocket sits relative to the caseback
  - where the flex can get past the module frame and battery clip without touching the cell (it may need a small notch)
  - total thickness budget

---

## 4. Power budget (CR2016, 90 mAh, 85% treated as usable)

Assumptions:
- **Baseline:** taken from Joey's 430–667-day estimate, which works out to 5.6–8.7 µA.
- **Steps:** 1.5 µA idle, plus 10 µA while moving, with 2–4 h of movement per day.
- **HR measurement, nominal (~150 µA):** green LED at 20 mA, 117 µs, 25 sps; boost at 3.6 V and 85% efficiency; plus 50 µA for the MCU.
- **HR measurement, pessimistic (~540 µA):** 50 mA, 50 sps, plus 100 µA for the MCU.

| Configuration | Avg current | Battery life |
|---|---|---|
| Watch only | 5.6–8.7 µA | ~12–19 months |
| + step counting | 8.0–11.9 µA | ~9–13 months |
| + 20 HR spot checks/day (30 s each) | 9.0–15.7 µA | ~7–12 months |
| + background HR (30 s every 10 min) | 15–39 µA | ~3–7 months |
| Continuous HR | 156–555 µA | ~1–3 weeks, and CR2016 is rated for 0.1 mA standard discharge |

A rechargeable LIR2016 doesn't help. It holds 15 mAh at 3.6 V, about 54 mWh, versus ~270 mWh for a CR2016 (5× less energy). It also charges to 4.2 V, which is above the SAM L22's 3.63 V max.

---

## 5. Firmware

- **Second Movement** (C, arm-none-eabi-gcc):
  - Build: `make BOARD=sensorwatch_pro DISPLAY=classic`, then `make install` with the watch in UF2 mode.
  - Emulator (emscripten): `emmake make BOARD=sensorwatch_red DISPLAY=classic`.
- **Steps:** build from **voloved/second-movement**, where `step_counter_face` is in the default face list.
  - LIS2DW12 runs at 12.5 Hz. The FIFO is drained every second and the samples go through the Espruino stepcount algorithm.
  - The count resets daily at 23:59 and 36 days of history are kept.
  - A setting controls when counting runs.
  - The fork also supports the LIS2DUX12, which has a hardware pedometer.
- **HR (to be written):**
  1. A `maxm86161` driver on the watch I²C API. References: the MikroE Heart Rate 2 Click library and the Silicon Labs third-party MAXM86161 driver.
  2. A `heart_rate_face` that runs this sequence:
     - ALARM press → A0 high (boost on)
     - configure the green LED at 25 sps, TINT 117 µs, starting around 20 mA
     - auto-adjust LED current to hit a target DC level
     - collect 10–15 s of samples from the FIFO on INTB interrupts
     - band-pass ~0.7–3.5 Hz
     - find the autocorrelation peak → BPM. The fork's `lib/embedded_pedometer` already does fixed-point autocorrelation, and the same approach works here.
     - reject the reading if the accelerometer shows motion
     - display and log the result, then turn the boost off
  3. Optional later: sample in the background every N minutes, only while the accelerometer says you're still.
- **Data out:** there's no radio. Logs can go out through `chirpy_tx` (the watch beeps the data and a web app decodes it). `irda_upload_face` handles data coming in.

---

## 6. Build order

1. Buy the Sensor Watch Pro and accelerometer board. Install them, flash the voloved fork, and confirm steps work. Measure baseline current with a PPK2 or µCurrent if you have one.
2. Bench HR prototype:
   - Use a MikroE Heart Rate 2 Click ($42, same MAXM86161).
   - Connect it to the watch with a 9-pin 0.5 mm FFC and an FFC-to-DIP breakout. Check which side the FFC contacts are on.
   - Run the watch on USB (3.3 V) while on the bench.
   - Write the driver and face, then check BPM against a chest strap or phone.
3. Caseback: CAD, resin prints, then test the wrist signal through each prototype.
4. Flex PCB v1 in KiCad, starting from OSO-SWAB-B1-00, then fab + assembly.
5. Integrate and profile current. Tune LED current and TINT down to the lowest power that still gives clean beats.

---

## 7. Risks and mitigations

- **Weak wrist PPG signal on a light, loose watch.** Use the sensor bump and a snug strap during readings. Fallback: take the watch off and press a fingertip to the sensor, which gives a much stronger signal.
- **MAXM86161 is marketed for in-ear.** At the wrist it should be fine for still spot checks, but expect lower SNR than wrist-optimized multi-PD modules.
- **Coin-cell pulse loads** (cold, end of life). Use the reservoir cap and RC isolation, and refuse HR readings below ~2.8 V battery.
- **Optical crosstalk.** Separate apertures with opaque material between LED and PD.
- **Flex routing and strain at the caseback.** Add a service loop and put Kapton over the battery clip.
- **Only one sensor connector.** Any future sensor has to go on this board.

---

## 8. Alternatives considered

- **Ollee Watch One** ($55 board / $99 installed).
  - Has BLE, accelerometer step counting, a phone app, and Gadgetbridge support.
  - But its "heart rate" is a manual pulsometer (you count 20 beats yourself), and the firmware is closed with no SDK. It fails on both HR and programmability.
- **F91 Kepler** (PegorK): OLED + BLE MCU, ~15 days of battery, inactive since 2022.
- **Full custom mainboard** (nRF52 + segment LCD driver): only needed if BLE phone sync is a requirement. Much bigger project. *(Superseded: BLE is now required, and the chosen path is an STM32WB55 mainboard. It has a built-in LCD driver, so no separate LCD driver chip is needed. See the update at the top.)*

---

## Sources

- Sensor Watch docs: https://www.sensorwatch.net/docs/
- Sensor board spec: https://www.sensorwatch.net/docs/sensorboards/
- Accelerometer board: https://www.sensorwatch.net/docs/sensorboards/accelerometer/
- Sensor Watch Pro (Crowd Supply): https://www.crowdsupply.com/oddly-specific-objects/sensor-watch-pro
- Power figures update: https://www.crowdsupply.com/oddly-specific-objects/sensor-watch/updates/flex-pcbs-sensor-boards-lower-low-power-and-the-solar-system-on-your-wrist
- Hardware repo (KiCad): https://github.com/joeycastillo/Sensor-Watch
- Firmware: https://github.com/joeycastillo/second-movement
- Step-counter fork: https://github.com/voloved/second-movement
- MAXM86161: https://www.analog.com/en/products/maxm86161.html (datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/maxm86161.pdf)
- MAXM86146: https://www.analog.com/media/en/technical-documentation/data-sheets/maxm86146.pdf
- BH1792GLC status: https://www.rohm.com/products/sensors-mems/optical-sensor-for-heart-rate-monitor/bh1792glc-product
- TPS61099: https://www.ti.com/product/TPS61099
- LIS2DUX12: https://www.st.com/en/mems-and-sensors/lis2dux12.html
- Heart Rate 2 Click: https://www.mikroe.com/heart-rate-2-click
- MAXM86161 at DigiKey: https://www.digikey.com/en/products/detail/analog-devices-inc-maxim-integrated/MAXM86161EFD/10258730
- CR2016 (Maxell): https://biz.maxell.com/en/primary_batteries/data_sheet/CR2016_Data_sheet_e.pdf
- CR2016 (Energizer): https://data.energizer.com/pdfs/cr2016.pdf
- LIR2016: https://www.powerstream.com/licoin.htm
- Ollee Watch: https://liliputing.com/ollee-watch-brings-smartwatch-features-to-the-casio-f-91w-with-a-mainboard-replacement/
- F91 Kepler: https://github.com/PegorK/F91_Kepler
