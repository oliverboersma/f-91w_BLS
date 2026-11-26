# Rev A pre-order items: status (2026-10-03)

| Item | Status | Result |
|---|---|---|
| MAXM86161 footprint | Done; oliver confirmed it against the ADI drawing | `lib/f91w.pretty/MAXM86161_OLGA-14.kicad_mod`, assigned to U2 on the PPG flex |
| MLPF-WB-01E3 footprint | Checked, OK | Matches ST's pitch, pad size and bump map |
| Q1 piezo MOSFET | Chosen | Diodes **DMN313DLT-7** |
| LSE crystal and load caps | Chosen | NDK **NX2012SA-32.768KHZ-EXS00A-MU00646** (CL 9 pF), with C18/C19 = **15 pF** |
| Antenna tuning | Needs the first assembled board | Procedure below |
| Human layout review | Open | |
| PPG flex layout | Open; the schematic and footprints are ready | |

## MAXM86161 (U2 on the PPG flex)

The package is OLGA-14 (F142A4+1), 2.9 × 4.3 × 1.4 mm, with ADI land pattern 90-100106. ADI's site blocks downloads from this environment, so I read the drawing's dimensions from its text: 0.400, 0.660, 1.900, 3.600 and 0.600 mm. I drew the footprint from those numbers:

- 14 pads of 0.40 × 0.66 mm in two columns of 7 at 0.60 mm pitch. The span from the first to the last pad centre is 3.60 mm, which matches 6 × 0.60.
- Pins 1–7 run down the left column and 8–14 run up the right column (top view, counter-clockwise).
- **Confirmed (oliver, 2026-10-03):** 1.90 mm is the centre-to-centre distance between the two columns, and 3.6 mm is the centre-to-centre span of each column. Both match the footprint, and the 2.9 × 4.3 mm body matches the datasheet.
- The optical windows are on the top of the package, so U2 has to sit on the skin side of the flex.

## MLPF-WB-01E3 (FL1)

[ST datasheet](https://www.st.com/resource/en/datasheet/mlpf-wb-01e3.pdf): the die is 1.0 × 1.6 mm. Bump pitch is 500 µm in X (columns A and B) and 587 µm in Y (rows 1–3), and the pads are 200 µm. The top view shows A1 = OUT (antenna), A3 = IN (RF1) and A2/B1/B2/B3 = GND. The footprint and the board wiring match all of this: A1 is on /RF_50R and A3 on /RF1. The one difference is that ST's figure shows square pads where the footprint uses round 0.2 mm pads, which makes no practical difference.

## Q1: DMN313DLT-7

The [datasheet](https://www.mouser.com/datasheet/2/115/DMN313DLT-3213929.pdf) gives:

- SOT-523, 30 V, ±20 V gate, 0.27 A
- V<sub>GS(th)</sub> 0.5–1.5 V
- R<sub>DS(on)</sub> 3.2 Ω typical at 2.5 V
- Gate ESD protection to 2 kV
- Pinout G = 1, S = 2, D = 3, which matches the Q_NMOS_GSD symbol and the board (pad 1 = BUZZER, pad 2 = GND, pad 3 = BUZZER_HV)

Its maximum threshold of 1.5 V is above the ≤1.2 V target. That still works, because the gate is driven from VDD (about 3 V from the coin cell), and a piezo drive only needs a few mA. I could not find a 30 V SOT-523 part with a lower guaranteed threshold. Measure the L3 flyback spike on the first board and keep it under 30 V.

## LSE: crystal and C18/C19

C = 2 × (CL − C<sub>stray</sub>). With about 2 pF of stray capacitance on this short layout, the options are:

| NX2012SA variant | CL | C18/C19 | Stock seen at LCSC |
|---|---|---|---|
| EXS00A-MU00527 | 6 pF | 8.2 pF | out of stock |
| **EXS00A-MU00646** | **9 pF** | **15 pF** | 5.6k in stock |
| EXS00A-MU00644 | 12.5 pF | 22 pF | 8.5k in stock |

I picked the 9 pF part. With the family's 80 kΩ maximum ESR and about 1 pF C0, the critical gm is 4 × ESR × (2πf)² × (C0 + CL)² ≈ 1.4 µA/V. The 12.5 pF part would need about 2.5 µA/V. **Firmware should start the LSE with `LSEDRV` = high** and can step it down once the oscillator has started. Confirm the STM32WB55 datasheet's gm<sub>critmax</sub> values for each LSEDRV setting.

The schematic, BOM and board now use 15p for C18/C19, DMN313DLT-7 for Q1 and the MU00646 crystal.

## Antenna tuning (needs the first assembled board)

The network runs FL1 OUT (/RF_50R) to C20 (shunt) to R3 (series, 0R) to C21 (shunt) to the antenna (/ANT_FEED). Build the first board with R3 = 0R and C20/C21 not fitted.

1. **With a VNA:** remove R3. Solder a pigtail of 0.8 mm semi-rigid or U.FL coax from the R3 antenna-side pad to ground, and calibrate at the pad. Measure S11 from 2.40 to 2.48 GHz **inside the closed watch, with the battery, LCD and caseback fitted and the watch on a wrist or a wrist phantom.** A chip antenna on such a small ground plane will usually resonate low and need a series inductor in the R3 position (0201, roughly 1–3 nH) plus a shunt C. Johanson's own evaluation board needed a 1.2 pF capacitor and 2.7 nH and 3.3 nH inductors ([datasheet](https://www.johansontechnology.com/datasheets/2450AT18B100/2450AT18B100.pdf)). Fit the values from a Smith chart, then re-measure.
2. **Without a VNA:** run the firmware TX at a fixed power on channel 19 (2.440 GHz), log the RSSI on a phone 1 m away for each combination of R3 ∈ {0R, 1.0 nH, 2.2 nH} and C21 ∈ {DNP, 0.5 pF, 1.0 pF}, and keep the best one. This is coarse but works.
