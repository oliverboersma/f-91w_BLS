# Parts and stencils for hand assembly

Written 2026-10-08, updated the same day with live DigiKey stock read in Chrome (see `/mnt/project-files/stock-check.md`).

## Where to buy

**Two orders: DigiKey for almost everything, plus a small LCSC order for three parts DigiKey can't supply.** LCSC can't be the single source either, because the replacement heart-rate sensor and some of the specialty parts were only confirmed at DigiKey.

- [`digikey-cart.csv`](digikey-cart.csv): upload it in DigiKey's BOM Manager. It is well above DigiKey's free-shipping threshold for the EU.
- [`lcsc-cart.csv`](lcsc-cart.csv): 11 lines. Upload it with LCSC's BOM tool, which matches by manufacturer part number. Lines without a C number rely on that match, so check each one it picks. Since the DigiKey checkout, this cart also covers the antenna (AE1) and seven capacitor lines that DigiKey had on backorder or didn't carry: 100 nF and 1 µF 0201, 22 µF 0603, 4.7 µF, 1 µF and 100 nF 0402. They are Samsung parts with the same value, size and voltage rating (or higher).

Both carts include spares: about 2 sets of ICs and 10–20 of each 0201/0402 passive, because these get lost from tweezers. Generic passives use Murata (caps) and Yageo (resistors) part numbers that match the BOM values, rated 6.3 V or higher.

## Changes from the design BOM

| Part | Problem (live check) | In the cart now |
|---|---|---|
| **U2 MAXM86161EFD+T** (flex) | Obsolete at DigiKey, none left. | **MAXM86161AEFD+T** (DigiKey, 3,369 in stock, $12.99). ADI has published no formal equivalence statement. It has the same OLGA-14 package and supplies, so it fits the footprint, but **check its register behaviour on a sample** (the firmware's driver was written for the original). |
| **U1 STM32WB55RGV6** (main) | Not a problem, but the RGV6 costs $8.48 at DigiKey. | **STM32WB55REV6** ($7.29): same VFQFPN68 pinout and 256 KB RAM, with 512 KB flash instead of 1 MB. The BLE full stack (about 180 KB) plus FUS (about 24 KB) leave roughly 300 KB, and the BLE_Ota loader takes 28 KB of that. The app is a BLE_HeartRate-sized program, an estimate of tens of KB, so it fits. Two things change: install the wireless stack at the **512 KB address** from ST's CubeWB release notes, not the 1 MB one; and a wireless-stack update *over the air* needs about 180 KB free under the stack. Updating the stack over SWD is always fine. |
| **L3 ELT-3KN004B** (buzzer coil, main) | Obsolete, none at DigiKey. | **ELT-3KN137C** from LCSC (C20013105). It is 4 mH instead of 14 mH, so the buzzer will be quieter and its resonance changes. I have not checked that its pads match the ELT3KN footprint. The watch works without L3; only the buzzer is affected. |
| **L1 DFE201610E-2R2M** (flex boost inductor) | 1.0 mm tall, but the flex pocket allows only 0.8 mm. | **TFM201608ALC-2R2MTAA** (0.80 mm max), on the LCSC cart because DigiKey doesn't stock it. Fallback: keep the DFE part and print the thicker caseback. |
| **L1 LQM21FN100M70L** (main, STM32 SMPS inductor) | Out of stock at DigiKey. | **TDK MLZ2012M100HT000**: also 10 µH in 0805, so the same footprint, with a 300 mA rating instead of 100 mA and slightly lower resistance. The STM32 SMPS draws only tens of mA, so it is a safe swap. It is the same part used for the JLC order. |
| **Q1 DMN313DLT-7** | DigiKey had only 8. The cart takes 5. | If DigiKey runs short, LCSC has it (C155524). |
| **U2 LIS2DW12TR** (main) | DigiKey has none (backorder). | LCSC C189624 (about 19.7k in stock). |
| **Y2 NX2012SA 32.768 kHz** | DigiKey has neither MU00646 nor MU00543. | The original MU00646 from LCSC (C5136771). |
| **U1 TPS610995DRVR** (flex) | DigiKey stocks it as TPS610995DRVT. | TPS610995DRVT, the same chip on a smaller reel. |
| **AE1 2450AT18B100E** | DigiKey orders it as 2450AT18B0100001E. | 2450AT18B0100001E. |

MLPF-WB-01E3 is in stock at DigiKey (2,485), so it stays as designed. Stock for the remaining DigiKey lines wasn't checked live; BOM Manager shows it on upload.

## Stencils

Order from JLCPCB as a **stencil-only order**, uploading the existing gerber zips (`fab/f91w-ble-main-gerbers.zip` and `flex/f91w-ppg-flex-gerbers.zip`). Choose frameless, electropolished, and "custom size" (smaller and cheaper). Nothing in the current JLC board cart needs to change; if that order hasn't been paid yet, adding the stencils to it saves one shipment.

| Stencil | Paste layer | Pads | Thickness |
|---|---|---|---|
| Mainboard top | F_Paste | 177 (34 parts) | **0.10 mm** (ST recommends 0.10 mm for the MLPF-WB-01E3; fine for the 0201s and the 0.4 mm pitch QFN-68) |
| Mainboard bottom | B_Paste | 13 (C15, C24, Q1, R4, R8, R9) | 0.10 mm (on the same sheet as the top), or skip it and hand-solder these 6 |
| PPG flex | B_Paste only (F_Paste is empty) | 48 (15 parts); the FFC fingers have no paste, which is correct | 0.10 mm |

JLC can put mainboard top and bottom on one stencil sheet. Build order: reflow the mainboard top side with the stencil and a hot plate or oven, then do the 6 bottom parts with paste and hot air (a hot plate would reflow the top parts again). Tape the flex flat to a scrap board with Kapton before stencilling it.

## Parts that are hard to stencil and place by hand

- **U2 LIS2DW12** (LGA-12, 2×2 mm, 0.5 mm pitch, no leads): you can't inspect or touch it up with an iron. Use the stencil, place it carefully and reflow; check for shorts with a meter.
- **U2 MAXM86161** (OLGA-14 on the flex): same as above, and it has an optical window. Keep flux and paste off the top, and don't use hot air directly on the window; reflow from below on a hot plate.
- **FL1 MLPF-WB-01E3** (6-bump chip-scale package, about 1 mm across): the hardest part on the board. Stencil it, place it under magnification, and check orientation against the pin-1 mark.
- **U1 STM32WB55** (QFN-68, 0.4 mm pitch, large thermal pad): bridges are likely. Use a light paste print; fix bridges with flux and solder wick.
- **The 0201 passives** (17 on the mainboard): fine with a stencil but need tweezers and a microscope or loupe. The cart has plenty of spares.
- **Q1** (SOT-523) and **Y1/Y2** (2016/2012 crystals, pads underneath): stencil them; the crystals can't be fixed with an iron.

Tools worth having: a stereo microscope or 10× loupe, fine curved tweezers, no-clean flux, Type 4 or 5 low-temperature paste (Sn42/Bi58 is easier on a hot plate; SAC305 if you use an oven), and a hot plate or reflow oven plus a hot-air station.
