# JLCPCB upload set

**Rev A1 update (2026-10-09).** See `../REV-A1-CHANGES.md`.

- The mainboard is now `f91w-ble-main-gerbers-revA1.zip`. The JLC order still holds rev A; replace that upload if JLC allows it.
- Set **Surface Finish = ENIG** on the mainboard. The cart showed OSP, which isn't suited to the LCD zebra pads, battery contacts and spring pads.
- Edge holes (JLC engineering query, 2026-10-09): **Option 2**, keep the outline exactly as designed, no castellation. JLC flagged only the LIGHT, MODE and ALARM holes, and the risk of torn barrel copper there is accepted because they're spring contacts. When the boards arrive, deburr those three edges, check that each button pad isn't shorted to GND or VDD (a press connects the pad to BAT+, so a GND short there would short the cell), and check that the pad copper hasn't lifted where the spring lands.
- **Flex: replace the Y9 upload with `f91w-ppg-flex-gerbers-revB.zip`.** Rev A can't reach the caseback from J1. Rev B adds a 14 mm fold section, so the board is now 14.5 × 37.5 mm. The settings are unchanged: Gold Fingers = Yes, 0.20 mm thickness, 0.1 mm PI stiffener. Order it only after the 1:1 paper fold check (`2-ppg-flex/f91w-ppg-flex-revB-fitcheck.pdf`).

These files are ready for JLCONE or jlcpcb.com. Nothing has been ordered.

| Board | Gerbers | Assembly |
|---|---|---|
| Mainboard | `f91w-ble-main-gerbers.zip`: 4 layers, 0.6 mm FR4, 0.09 mm track and space, 0.2 mm vias (advanced process) | No PCBA files are included; this pack is for hand assembly |
| PPG flex | `f91w-ppg-flex-gerbers.zip`: 2-layer flex (FPC), 0.11 mm, ENIG, with the stiffeners drawn on User.Drawings | No PCBA files are included; this pack is for hand assembly |

- There are no JLC BOM/CPL uploads in this pack. The component CSVs are supplier carts for hand assembly; do not start a JLC PCBA order from them.
- **Flex finger tail: set Gold Fingers = Yes and Gold Fingers Thickness = 0.20 mm** (FH19C takes 0.20 mm FPC). Set the Polyimide stiffener to 0.10 mm; don't use 0.20 mm PI.
- The Gerber zips were re-exported on 2026-10-08 with the pours pulled back from the edge (0.2 mm on the mainboard, 0.3 mm on the flex). Re-upload both zips if an older upload is in a cart.
- **Order the flex only after the printout fit check in the caseback.**

Cart cleanup completed on 2026-10-08: the stale `f91w-ble-main-gerbers-v2-silk_Y6` mainboard upload and the superseded `f91w-ppg-flex-gerbers-v2_Y8` flex upload were removed. The cart now shows four items (replacement flex PCB, mainboard, main stencil, flex stencil). The replacement flex upload is `f91w-ppg-flex-gerbers-verified_Y9`; its live details match this pack. Do not pay until the preview matches this pack.
