# F-91W BLE: hardware rev A (schematics)

A replacement board for the Casio F-91W with Bluetooth LE, a step counter (accelerometer) and a spot-check heart-rate sensor. It is programmable over SWD and BLE OTA.

**Status: rev A schematics, first pass.** Everything here has been checked by a script (see Verification), but no engineer has reviewed it yet and it has not been laid out. Do the open items below before ordering anything.

## What's in here

| Path | What |
|---|---|
| `mainboard/` | KiCad project for the STM32WB55 mainboard: the drop-in replacement for the F-91W module PCB |
| `ppg-flex/` | KiCad project for the heart-rate flex (MAXM86161 + 3.6 V boost), which sits in the caseback and plugs into the mainboard |
| `lib/` | Project symbols (`f91w.kicad_sym`) and footprints (`f91w.pretty`, **KiCad 8+**) |
| `docs/` | Schematic PDFs, BOM CSVs, KiCad netlists, intended netlists and the verification report |
| `firmware/board_f91w_ble.h` | Pin map and LCD segment translation table, generated from the same data as the schematic |
| `firmware/hr/` | Spot-check heart-rate algorithm in portable C, with a host test on synthetic PPG (`make test`) |
| `gen/` | Python generator: `python3 gen/build_all.py <outdir>` rebuilds everything (needs `kicad-cli`) |

The schematics are written in KiCad 7 format, so KiCad 7, 8 and 9 can all open them. KiCad 8/9 will offer to upgrade the file on save. The footprint library needs KiCad 8 or newer, because two of the reused Sensor Watch footprints are in v8 format.

## Architecture

```
                 F-91W LCD (3 COM x 24 SEG, zebra strip)
                                |
 CR2016 ──VDD (2.7-3.1 V, no regulator)──┬──────────────────────────────────┐
                                         │                                  │
   buttons x3 (active-high) ──► ┌────────┴─────────┐ ◄── LIS2DW12 (I2C 0x19, │
   piezo ◄─ 14 mH + NMOS ◄───── │   STM32WB55RGV6  │      INT1/INT2: steps)  │
   RGB backlight ◄───────────── │ BLE + LCD + M4F  │ ◄── SWD (Tag-Connect)   │
   chip antenna ◄─ π ◄─ MLPF ◄─ │ RF1   SMPS 10 µH │                         │
                                └────────┬─────────┘                         │
                                    I2C + A0/A1                              │
                                         │  J1: 9-pin FPC (Sensor Watch pinout)
                                         ▼                                   │
                PPG flex: 47 Ω/22 µF ─► TPS610995 (3.6 V, EN=A0) ─► MAXM86161 (INT -> A1)
```

Design decisions:

- **One chip does everything.** The STM32WB55 handles BLE 5.4, drives the stock segment LCD directly (1/3 duty, 1/3 bias) and runs the application on its Cortex-M4F. Ollee's spec ("Cortex-M4/M0+ with BLE") suggests it uses the same chip family.
- **No USB.** It would cost 2 pins plus a power path, and the 49 I/O pins are fully used. Instead:
  - Flash and debug over SWD, using an ST-Link and a TC2030-NL cable.
  - Update in the field over BLE OTA.
- **Same 9-pin sensor connector as Sensor Watch.** The PPG flex can also be bench-tested on a Sensor Watch Pro.
- **The accelerometer is on the mainboard.** It's wired the same way as the Sensor Watch accelerometer board, so step-counter code ports easily.

## MCU pin map (STM32WB55RGV6, VFQFPN68)

| MCU pin | Net | Function |
|---|---|---|
| PA0 | BTN_MODE | button, EXTI0, WKUP1 |
| PA1 | LCD_SEG2 | glass SEG2 via LCD_SEG0 |
| PA2 | A2 | J1 A2 spare, ADC1_IN7, LPUART1_TX |
| PA3 | LCD_SEG3 | glass SEG3 via LCD_SEG2 |
| PA4 | LCD_SEG4 | glass SEG4 via LCD_SEG5 |
| PA5 | ACC_INT2 | LIS2DW12 INT2, EXTI5 |
| PA6 | LCD_SEG5 | glass SEG5 via LCD_SEG3 |
| PA7 | LCD_SEG6 | glass SEG6 via LCD_SEG4 |
| PA8 | LCD_COM0 | LCD common (fixed: LCD_COM0-2) |
| PA9 | LCD_COM1 | LCD common (fixed: LCD_COM0-2) |
| PA10 | LCD_COM2 | LCD common (fixed: LCD_COM0-2) |
| PA11 | BUZZER | piezo PWM, TIM1_CH4 |
| PA12 | A1 | J1 A1 -> PPG_INT, EXTI12, LPUART1_RX |
| PA13 | SWDIO | SWD |
| PA14 | SWCLK | SWD |
| PA15 | LCD_SEG16 | glass SEG16 via LCD_SEG17 |
| PB0 | LED_R_N | backlight red (active low) |
| PB1 | A0 | J1 A0 -> PPG BOOST_EN |
| PB2 | BTN_LIGHT | button, EXTI2 |
| PB3 | A3 | J1 A3 spare, EXTI3, SWO |
| PB4 | LCD_SEG20 | glass SEG20 via LCD_SEG8 |
| PB5 | LCD_SEG21 | glass SEG21 via LCD_SEG9 |
| PB6 | LCD_SEG22 | glass SEG22 via LCD_SEG6 |
| PB7 | LCD_SEG23 | glass SEG23 via LCD_SEG21 |
| PB8 | I2C_SCL | I2C1_SCL |
| PB9 | I2C_SDA | I2C1_SDA |
| PB10 | LCD_SEG9 | glass SEG9 via LCD_SEG10 |
| PB11 | LCD_SEG10 | glass SEG10 via LCD_SEG11 |
| PB12 | LCD_SEG11 | glass SEG11 via LCD_SEG12 |
| PB13 | LCD_SEG12 | glass SEG12 via LCD_SEG13 |
| PB14 | LCD_SEG13 | glass SEG13 via LCD_SEG14 |
| PB15 | LCD_SEG14 | glass SEG14 via LCD_SEG15 |
| PC0 | LCD_SEG0 | glass SEG0 via LCD_SEG18 |
| PC1 | A4 | J1 A4 spare, ADC1_IN2 |
| PC2 | LCD_SEG1 | glass SEG1 via LCD_SEG20 |
| PC3 | VLCD | LCD_VLCD step-up cap (1 uF) |
| PC4 | LCD_SEG7 | glass SEG7 via LCD_SEG22 |
| PC5 | LCD_SEG8 | glass SEG8 via LCD_SEG23 |
| PC6 | LCD_SEG15 | glass SEG15 via LCD_SEG24 |
| PC10 | LCD_SEG17 | glass SEG17 via LCD_SEG28 |
| PC11 | LCD_SEG18 | glass SEG18 via LCD_SEG29 |
| PC12 | LCD_SEG19 | glass SEG19 via LCD_SEG30 |
| PC13 | BTN_ALARM | button, EXTI13, WKUP2 |
| PC14 | LSE_IN | 32.768 kHz crystal |
| PC15 | LSE_OUT | 32.768 kHz crystal |
| PD0 | LED_G_N | backlight green (active low) |
| PD1 | LED_B_N | backlight blue (active low) |
| PE4 | ACC_INT1 | LIS2DW12 INT1, EXTI4 |
| PH3 | BOOT0 | 10k pull-down + test pad |
| NRST | NRST | 100 nF + SWD |
| OSC_IN | HSE_IN | 32 MHz crystal |
| OSC_OUT | HSE_OUT | 32 MHz crystal |
| RF1 | RF1 | RF -> MLPF-WB-01E3 |
| VFBSMPS | VFBSMPS | SMPS output, 4.7 uF |
| VLXSMPS | SMPS_LX | SMPS switch -> 10 uH |
| VREF+ | VDDA | VREF+ tied to VDDA |

**LCD segment mapping:**

- `LCD_SEGn` is the same physical zebra pad that Sensor Watch calls `SEGn`. Movement's `(com, seg)` segment maps can therefore be reused: translate `seg` through `LCD_GLASS_SEG_TO_MCU_SEG[]` in `firmware/board_f91w_ble.h`.
- Any LCD-capable pin can drive any glass line, so reassign SEG pins freely during layout. Then regenerate the header. COM0-2 must stay on PA8/PA9/PA10.

**EXTI lines in use:**

- 0: MODE
- 2: LIGHT
- 4: ACC_INT1
- 5: ACC_INT2
- 12: A1/PPG_INT
- 13: ALARM

A0 (PB1) and A4 (PC1) share EXTI1, so only one of them can be an interrupt input.

## Power budget (CR2016 ≈ 90 mAh, 85 % usable). These are estimates to measure on the first boards.

| Configuration | Avg current | Battery life |
|---|---|---|
| Watch only (Stop2 + RTC + LCD) | 3.1-6.1 µA | 17-34 months |
| + step counting | 5.9-10.8 µA | 10-18 months |
| + 20 HR spot checks/day | 6.9-14.5 µA | 7-15 months |
| + BLE sync on demand (button) | 7.2-16.0 µA | 7-15 months |
| + BLE always advertising instead | 11.5-23.5 µA | 4-9 months |

Assumptions:

- STM32WB55 Stop2 + RTC: 2.1 µA (datasheet).
- LCD: 1-4 µA (estimated, not measured).
- LIS2DW12 step counting: 2.3-3.2 µA, plus MCU wake-ups.
- HR check: 150-540 µA for 30 s.
- BLE TX: 5.2 mA at 0 dBm with the SMPS.

So: sync over BLE when the user asks for it (or a few times a day), and don't advertise all the time.

The mainboard has 2×22 µF of bulk capacitance, which buffers BLE TX bursts. On the flex, a 47 Ω series resistor keeps boost inrush off the coin cell.

## Open items before layout / fab

1. **MAXM86161 footprint.** Drawn 2026-10-03 as `f91w:MAXM86161_OLGA-14`. Confirmed against ADI 90-100106 (1.90 mm column pitch centre-to-centre).
2. **MLPF-WB-01E3 footprint.** Checked against the ST datasheet on 2026-10-03: OK.
3. **Q1 (piezo NMOS).** Diodes DMN313DLT-7 (30 V, SOT-523, V<sub>GS(th)</sub> 0.5–1.5 V). Measure the inductive spike on the first board.
4. **LSE crystal.** NDK NX2012SA-32.768KHZ-EXS00A-MU00646 (CL 9 pF) with C18/C19 = 15 pF. Run the LSE at LSEDRV high.
5. **Antenna.**
   - Place AE1 at the board edge where Sensor Watch had the USB receptacle, and delete those USB pads from the outline footprint.
   - Keep ground clearance per Johanson's layout.
   - Tune the π network (C20/R3/C21) with a VNA inside the closed watch, with the battery and caseback fitted.
6. **Stack-up.** 4-layer, ~0.6 mm, like Sensor Watch Pro.
   - Solid ground on layer 2.
   - The RF1→IPD trace should be 62 Ω and the IPD→antenna trace 50 Ω.
   - Put the HSE crystal and the SMPS loop tight to U1.
7. **Mechanical.**
   - Check component heights against the module frame and battery clip (the 14 mH inductor and the 0805 10 µH are the tall parts).
   - Route the PPG flex past the frame to the caseback window, with a service loop.
8. **ERC.** Run it in the KiCad GUI after opening. The netlist checks pass, but KiCad 7's CLI has no ERC.

## Firmware plan

- **Recommended:** STM32CubeWB (BLE stack on CPU2), with a shim that implements Movement's `watch_*` HAL so the Sensor Watch faces carry over. The pieces are:
  - segment LCD, using the table in `board_f91w_ble.h`
  - RTC
  - buttons/EXTI
  - TIM1 buzzer
  - I2C
  - ADC/VREFINT for battery voltage
  - Stop2 sleep
- **Alternative:** Zephyr, which supports STM32WB55 BLE through ST's stack. You'd still have to write the LCD driver.
- **Heart rate** (`firmware/hr/`):
  1. Drive A0 high and wait about 5 ms.
  2. Configure the MAXM86161: green LED, 25 or 50 sps, 117 µs integration, around 20 mA to start.
  3. Collect 8-10 s while the accelerometer reports stillness.
  4. Call `hr_estimate()`.
  5. Turn the boost off.

  The algorithm band-passes the signal, picks the period by autocorrelation (McLeod-style), uses a spectral octave check to avoid double/half errors, and refines the result with a Goertzel scan.

  Host test results on synthetic wrist PPG: MAE 0.2-0.6 BPM at 25 sps across noise levels up to 1× the AC amplitude, and ≤1.7 % false "ok" readings on pure noise. Real wrist data will be harder, so log raw samples from the first boards.

## Verification

`docs/VERIFICATION.txt` was produced with KiCad's own connectivity engine (`kicad-cli sch export netlist`). The check confirms that:

- every net matches the intended netlist exactly
- every pin is either connected or explicitly no-connect
- there are no single-pin nets
- no net has two outputs
- every power net has a source
- every part has a footprint

## Licence / attribution

The F-91W outline, the FH19C connector and the flex-tail footprints, the ELT3KN inductor footprint, the QBLP655 LED footprint and the LGA-12 accelerometer footprint are copied from [Sensor Watch](https://github.com/joeycastillo/Sensor-Watch) by Joey Castillo (CC-BY-SA 4.0). Boards built on them are derivative works: share them under CC-BY-SA 4.0 with attribution.
