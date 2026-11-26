# F-91W order pack verification — 2026-10-08

This revision is the checked hand-assembly pack. It does not authorize payment or a PCBA order.

## Board checks

- Flex PCB DRC: **0 violations, 0 unconnected items**.
- Mainboard DRC: **0 unconnected items**. Four edge-clearance reports remain on the intentional F-91W contact/LED footprint M1; the board rule explicitly excludes those external pads. They are the same known exceptions in the previous pack.
- Flex ERC: 0 violations. Main ERC retains five schematic warnings (library-cache mismatches and the intentional power-net modelling warning); these do not indicate an unrouted PCB connection.
- Both boards were re-plotted from the included KiCad sources after the flex edit. The flex tail copper is inset 0.20 mm from the cut edge at the FFC fingers; other flex copper follows the 0.30 mm edge rule.

## Connector and fit details

The Hirose FH19C/FH19SC catalog specifies a 0.20 mm FPC, 2.5 +/- 0.3 mm exposed trace, and a 3.5 mm minimum stiffener. The flex drawing now calls out a 0.20 +/- 0.03 mm finished tail and a 3.60 mm stiffener. The nine fingers retain more than the 2.2 mm minimum exposed length after the 0.20 mm setback.

The flex fit-check SVG was regenerated from the edited board. Print it at 100% and check it in the actual caseback before ordering the flex.

## Supply checks

- The exact TDK TFM201608ALC-2R2MTAA remains in the design and is now in the LCSC cart as C2835408, quantity 5. DigiKey lists it but does not stock it.
- The buzzer coil remains a functional substitution: Panasonic lists ELT3KN137C as 4 mH, 4.5 x 3.1 x 1.4 mm, 15 mA, versus the obsolete 14 mH source part. Its smaller body fits the footprint envelope, but terminal-land overlap and buzzer output/resonance still need confirmation against the land drawing or a first board.

## JLC cart observation

The stale `f91w-ble-main-gerbers-v2-silk_Y6` mainboard upload and the superseded `f91w-ppg-flex-gerbers-v2_Y8` flex upload were removed from the live cart after confirmation. The cart now shows four items: replacement flex PCB, mainboard, main stencil, and flex stencil. The replacement flex is `f91w-ppg-flex-gerbers-verified_Y9`; Product Details show Flex, 2 layers, 0.11 mm, yellow coverlay, white silkscreen, ENIG, Gold Fingers Yes, 0.2 mm gold-finger thickness, Polyimide stiffener, and 0.1 mm polyimide thickness. Do not pay until the preview matches this pack.
