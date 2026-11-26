# F-91W heart-rate (PPG) flex

## Rev B (2026-10-09): fold section so the flex reaches the caseback

Rev A could not connect to the mainboard. Rev B adds a 14.0 mm fold section between the FFC tail and the body. Nothing else moves, so the BOM and pos files still apply.

**Why.** J1 on the mainboard is an FH19C on the LCD side, at 6 o'clock, rotated 180°. Hirose's FH19C STEP model shows that the FPC slot opens on the side away from the solder leads. On this board that side faces the board centre.
- The tail goes in from the centre side, flat, with the fingers down (bottom contact). Pin 1 then lands on pin 1, so the rev A finger order is right. Sensor Watch uses the same connector placement and pinout.
- Rev A put the body only 0.55 mm past the tail stiffener. That isn't enough flex to leave the LCD side, and lying flat there, the body would sit on the mainboard's U1 and the sensor head on the LCD zebra pads.

**Route** (checked with a paper strip around the 6 o'clock edge):
1. The tail sits in J1.
2. Fold 1 turns the flex back over J1.
3. It runs about 5 mm toward the edge.
4. Fold 2 wraps it around the 6 o'clock board edge, at x = −0.5 to 4.5 mm, which is the straight part of the edge, clear of the buzzer spring.
5. It runs about 2.4 mm under the board beside the battery.
6. It steps down past the battery into the caseback bowl.

Two 180° folds leave B.Cu facing the skin again, so U2 and the fingers both stay on B.Cu. **Both folds have the plain side (F.Cu, the stiffener side) on the inside.**

The path needs about 13–15 mm of flex, depending on the bend radii. With 14.0 mm, the body lands about where the caliper-based rev A placement put it, with U2 near (−2.8, 7.4) mm from the case centre (KiCad coordinates).

**What changed** (`scripts/add_fold_section.py`):
- **Moved 14.0 mm:** the tail, the fingers, stiffener 1 and its note.
- **Fold section:** the same 4.97 mm width as the tail. It carries six straight B.Cu traces: VDD 0.25, GND 0.25, PPG_INT, SDA, SCL and BOOST_EN at 0.12 mm.
- **Kept clear:** no vias, no pour on either layer, no F.Cu copper, no mask openings and no silkscreen in the section. The traces run perpendicular to the folds.
- **New GND trace:** rev A fed the GND finger only through the pours, so a 0.25 mm trace now runs from the existing stitching via to finger 2.
- **Drawing notes:** fold markers and folding notes on User.Drawings. The revision is B, and the silkscreen says "rev B".
- **Resolved:** the rev A vias that sat 0.30–0.44 mm from the stiffener edge are now about 14 mm from it. The job file lists every layer.

**Verification.**
- **Conversion:** the KiCad 10 rev A board was converted to KiCad 7 (`scripts/k10to7.py`), because only KiCad 7.0.11 runs here. A re-plot of the converted rev A matched the verified Y9 gerbers exactly: zero XOR area on every layer, and identical drills.
- **DRC:** 0 violations and 0 unconnected pads.
- **Gerber check of rev B:**
  - No shorts. No opens apart from the `N/C` pins.
  - Minimum clearance is 0.104 mm.
  - All copper is at least 0.3 mm from the edge, except the FFC fingers at 0.21 mm (as in rev A).
  - The body and the tail are identical to rev A, apart from refill differences under 0.05 mm² and the silkscreen revision text.

**Files.**
- `2-ppg-flex/gerbers/f91w-ppg-flex-gerbers-revB.zip`: the rev B gerbers.
- `2-ppg-flex/f91w-ppg-flex-revB.drc.rpt`: the DRC report.
- `2-ppg-flex/f91w-ppg-flex-revB-fitcheck.pdf`: A4 at 1:1. Print it at 100%, cut it out and fold it around the real board.
- `kicad/f91w-ppg-flex.kicad_pcb`: rev B, saved in KiCad 7 format. KiCad 10 opens it, and the project and rule files are still the KiCad 10 ones.
- The rev A STEP and the parts-side SVG are out of date.

**Before ordering:**
- **Paper fit check:** fold the 1:1 print through the real path (over J1, around the edge, down beside the battery) and check where the body and U2 land. If it's too short or too long, rerun `scripts/add_fold_section.py` with another length; it's a parameter.
- **Body over the piezo:** in the bowl, the body sits over most of the piezo disc. Your measurements give about 0.9 mm (0.6 mm with the 7.50 reading) from the battery to the bowl, less the disc thickness. The 0.8 mm L1 doesn't fit over the disc, so check that clearance in the fit check. This was already the case in rev A.
- **Order:** replace the Y9 flex upload with the rev B zip. Keep Gold Fingers = Yes, 0.20 mm thickness, and the 0.1 mm PI stiffener.

---

# Rev A notes (2026-10-03 to 2026-10-08)

The flex is routed: 0 unconnected nets. DRC shows 9 errors, and all of them are the expected FFC fingers running to the tail edge (see below). Finished 2026-10-03; sensor pad placed 2026-10-08.

To open it, unzip `../f91w-kicad-projects.zip` with **Extract All** and open `f91w-ppg-flex/f91w-ppg-flex.kicad_pro`. Don't open the board straight from the zip, or KiCad falls back to its default rules. The board's footprints are linked to the schematic symbols, and a check found 0 net or footprint mismatches.

## Shape

| Region | What's there |
|---|---|
| Tail and pocket (bottom) | Copied from Sensor Watch's OSO-SWAB-B1-00 accelerometer board, so the FFC fingers and the pocket outline are ones that already fit the watch. It holds the TPS610995 3.6 V boost, L1, the input RC (R1, C1, C2), the VLED bulk caps (C3–C5), the EN pull-down (R2) and the INT pull-up (R3) |
| Arm | 2.6 mm wide and 6.2 mm long. It carries the signals on B.Cu over unbroken GND on F.Cu |
| Sensor pad (top) | 8 × 6.8 mm. It holds U2 (MAXM86161) plus C6, C7, C8, R4 and the VLED test pad TP1 |

All parts, and the FFC fingers, are on **B.Cu**, which is the side facing the caseback, the same as on Sensor Watch. That puts the MAXM86161's optical windows toward the skin. F.Cu is a GND pour.

**Sensor position (updated 2026-10-08, from oliver's caliper photos).** The flex is mapped onto the case through the J1 connector pins, so mainboard (0, 0) is the case centre. The caseback bowl is 23.99 mm across inside. The piezo disc is about 15 mm across and sits about 3 mm off-centre toward 6 o'clock, so U2 sits in the free 12 o'clock part of the bowl, at about (−2.8, −7.4) mm from the case centre and about 1 mm clear of the piezo. The arm is 6.2 mm long and the pad is 8 × 6.8 mm, with its top-left corner trimmed to the bowl. The disc size and position, and which way up the caseback was in the photo, are estimates. Check them before ordering by laying a printout of `f91w-ppg-flex-parts-side.svg` in the caseback.

**Height.** oliver confirmed that the battery face sits flush with the case's back rim. The case is 7.20 mm thick without the back and 8.63 mm with it, the caseback sheet is 0.50 mm and the bowl is 2.02 mm tall. That leaves about 0.9 mm from the battery to the inside of the bowl (about 0.6 mm if the 7.50 reading applies), and less where the piezo is. The MAXM86161 plus the flex is about 1.5 mm, so **the caseback needs a hole of about 3.2 × 4.6 mm** over U2. The sensor then sits in the 0.5 mm sheet and stands about 0.1 to 0.4 mm proud of the back, which helps skin contact. Seal it with clear epoxy and leave out the stiffener under U2. The alternative is a printed caseback about 1 mm thicker. The pocket parts (L1 in particular) must be 0.8 mm tall or less.

## Fab notes

- 2-layer polyimide flex, about 0.11 mm thick, ENIG. Rules: 0.1 mm clearance, 0.12 mm signal tracks, 0.25 mm power tracks, 0.4/0.2 mm vias, **0.3 mm copper-to-edge** (JLC's minimum for laser-cut flex outlines; raised from 0.2 mm on 2026-10-08).
- **Stiffener 1:** polyimide on the F side under the FFC fingers, for a **total tail thickness of 0.20 mm**. FH19C-9S-0.5SH(10) takes 0.20 mm FPC (the 0.30 mm part is FH19SC); Sensor Watch's sensor flex is 0.19 mm. On the JLC order set Gold Fingers = Yes, Gold Fingers Thickness = 0.20 mm, and use the PI stiffener JLC's calculator gives (about 0.1 mm).
- **Stiffener 2:** none under U2 if it goes through a caseback hole. Use 0.1 mm polyimide only with a thicker caseback.
- **2026-10-08 fix:** the exported pours ran to the board edge (the headless KiCad 7 zone fill ignored the edge clearance); they are now refilled 0.3 mm back. /PPG_INT and its three neighbours in the arm (I2C_SCL, I2C_SDA, VLED) were moved left, and the PPG_INT run along the cutout was moved down, so all tracks clear the edge by 0.3 mm. C7 moved 0.06 mm and two GND vias moved slightly for the same reason. The old files are in `../archive/pre-edge-fix-2026-10-08/`.
- Both stiffeners are drawn on the `Dwgs.User` layer, which is included in the gerbers zip.
- The FFC fingers run to the tail edge by design, which is the source of the 9 `copper_edge_clearance` DRC errors. Each finger has its own mask opening. The tiny mask webs between them won't survive fab, which leaves an open finger window like Sensor Watch's.

## Files

- `f91w-ppg-flex-gerbers.zip`: gerbers, drill files and drill maps
- `f91w-ppg-flex-pos.csv`: pick-and-place, 15 parts, all on the bottom side. J1 is the fingers, so it isn't placed
- `f91w-ppg-flex-BOM.csv`: BOM
- `f91w-ppg-flex-copper.png`: F.Cu and B.Cu with pours
- `*.svg`, `.step`: renders and the 3D model
- `f91w-ppg-flex.drc.rpt`: DRC report
- `kicad/`: the KiCad project, unzipped

## Pipeline

`scripts/flex_run.sh [passes]` runs the whole flow:
1. `flex_build.py`: outline, placement, netclasses and zones
2. Specctra DSN export
3. Freerouting
4. `flex_post.py`: imports the SES, stitches GND fragments, and uses `gnd_islands.py` to join any GND islands left on the two layers
5. DRC

## Known compromises (rev A)

- The boost's SW node (L1 to U1) takes a detour around U1. It works for a 50 mA LED rail, but it should be tightened by hand if the boost shows noise.
- Freerouting doesn't give the same result on every run. The delivered board is the one checked here.
