# F-91W BLE + heart-rate watch: latest files (2026-10-09, v2)

This folder holds only the current revision of everything. It replaces the `f91w-latest-2026-10-09` bundle. The superseded files are kept in `../f91w/` and in git history.

| Folder | What it is | Revision |
|---|---|---|
| `1-mainboard/` | STM32WB55 replacement mainboard: gerbers, KiCad project, BOM, pos, renders, STEP, DRC report, scripts | **Rev A1**, `gerbers/f91w-ble-main-gerbers-revA1.zip`: mask webs, contact-pad clearance and two vias in pasted pads fixed, design rules made to work in KiCad 7. DRC: 1 known item (the BUZZER hole touching the edge) and 0 unconnected pads. Details in `6-docs/REV-A1-CHANGES.md`. The top and bottom SVGs are rev A1. The STEP is rev A, with the same outline and parts. U1 is the STM32WB55**REV6** (512 KB); the BOM text may still say RGV6. |
| `2-ppg-flex/` | MAXM86161 heart-rate flex: gerbers, KiCad project, BOM, pos, SVG, 1:1 fold-check PDF, DRC report, scripts | **Rev B**, `gerbers/f91w-ppg-flex-gerbers-revB.zip`: a 14 mm fold section lets the flex go from J1 (LCD side) around the 6 o'clock board edge into the caseback. It has 0 DRC violations and 0 unconnected pads. Details in `6-docs/ppg-flex-README.md`. The board file is saved in KiCad 7 format, which KiCad 10 opens. There's no rev B STEP yet. U2 is the MAXM86161**AEFD+T**, L1 the TFM201608ALC. |
| `3-parts-and-stencils/` | DigiKey cart, LCSC cart, part links, stencil spec | Unchanged. In the live LCSC cart, flex C6 (100 nF) is **C71629**; the CSV still shows the earlier CL05B104KO5NNNC line. |
| `4-cnc-case-back/` | 6061 aluminium case back: STEP, drawing, CadQuery scripts | Unchanged, rev A. The outline (31.5 × 30.5) and screw spacing (25.2 × 23.2) are still estimates until the stock back is measured. |
| `5-firmware-and-apps/` | Firmware (STM32CubeWB 1.24) and companion apps | Unchanged. GitHub oliverboersma/claudetest, branch `claude/project-thread-ljxiyl` at commit cb638a3, which is still the branch head. `firmware/board/board_f91w_ble.h` matches the routed board for all 20 GPIOs and all 27 LCD lines. |
| `6-docs/` | Design notes, build plan, pre-order items, order verification, rev A1 changes, flex rev B notes | Updated 2026-10-09 |

## Left out (superseded)

- **Mainboard rev A gerbers** (`f91w-ble-main-gerbers.zip`, the upload on the JLC order): replaced by rev A1.
- **Flex rev A gerbers** (`f91w-ppg-flex-gerbers-verified.zip`, Y9 in the JLC cart, and its `-jobfix` copy): this revision can't reach the caseback, so it's replaced by rev B.
- **Flex rev A STEP and parts-side SVG:** replaced by `2-ppg-flex/f91w-ppg-flex-revB.svg` and the fold-check PDF.
- **Mainboard `copper.png`:** a rev A render.

## Still to do on the JLC order

The order holds the rev A mainboard and the rev A flex. Replacing them is your call.

- **Mainboard:** the board as ordered works. Uploading rev A1 instead gets the fixes above. Either way, set **Surface Finish = ENIG** (the cart showed OSP), because the LCD, battery and spring contacts are pressure contacts.
- **Edge holes:** the answer is Option 2: keep the outline exactly as designed, with no castellation. When the boards arrive, deburr the LIGHT, MODE and ALARM edges. Then check that each button pad isn't shorted to GND or VDD and that its copper hasn't lifted.
- **Flex:** replace the Y9 upload with rev B. The board is now 14.5 × 37.5 mm. Keep Gold Fingers = Yes, 0.20 mm thickness and the 0.1 mm PI stiffener.

## Before ordering the flex

1. **Fold check:** print `2-ppg-flex/f91w-ppg-flex-revB-fitcheck.pdf` at 100% (A4, 1:1). Cut out the outline and fold it with the printed side inside: back over J1, around the 6 o'clock board edge, then down past the battery into the caseback. Check where the body and U2 land. The fold length is a parameter of `2-ppg-flex/scripts/add_fold_section.py`.
2. **Piezo clearance:** the flex body sits over most of the piezo disc. Your measurements give about 0.9 mm (0.6 mm with the 7.50 reading) from the battery to the bowl, less the disc thickness. The 0.8 mm L1 may not fit over the disc.
3. **Three in-pad vias:** three GND vias on the mainboard still sit in pasted pads (C16, R2 and C13). Tin those pads with an iron before applying paste.
