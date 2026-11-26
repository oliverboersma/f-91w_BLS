# Mainboard rev A1 (2026-10-09)

Rev A1 fixes the mainboard problems found in the gerber review. No parts moved and no nets changed, so the BOM and pos files still apply. The PPG flex only gets a corrected job file.

Rev A1 has **not** been ordered. The JLC order holds rev A (`f91w-ble-main-gerbers-v4-edge_Y7`).

## Files

| File | What it is |
|---|---|
| `1-mainboard/gerbers/f91w-ble-main-gerbers-revA1.zip` | Rev A1 gerbers, drill files, drill maps and job file |
| `1-mainboard/kicad/` | The rev A1 board and the KiCad 7 design rules |
| `1-mainboard/f91w-ble-main.drc.rpt` | The rev A1 DRC report |
| `1-mainboard/scripts/fix_rev_a1.py` | The script that turns the rev A board into rev A1 |
| `1-mainboard/scripts/plot_gerbers.sh` | The kicad-cli commands that produced the zip |
| `2-ppg-flex/gerbers/f91w-ppg-flex-gerbers-verified-jobfix.zip` | The flex zip with a corrected job file. Every gerber and drill file in it is byte-identical to the verified Y9 zip |

`f91w-ble-main-gerbers.zip` (rev A, as ordered) and `f91w-ppg-flex-gerbers-verified.zip` (Y9, as in the cart) are kept for reference.

## Mainboard changes

1. **Solder mask margin set to 0** (it was 0.0635 mm). Every other pad on the board already uses 0. The pads changed are:
   - U2 (LIS2DW12, LGA-12): the mask webs between pads were 0.008–0.023 mm and are now at least 0.1 mm.
   - J1 (FH19C): the webs were 0.073 mm and are now 0.2 mm.
   - D1 (RGB LED), the M1 LCD zebra pads 1–27, and the M1 battery pads GND1/GND2: traces of other nets sat 0.037–0.045 mm outside the mask openings. A normal mask shift would expose them next to a zebra strip or under the battery. They now sit 0.1 mm away.
2. **Pours pulled back 0.3 mm from the spring and battery contacts** (it was 0.1 mm). This covers VCC1, VCC2, LIGHT, MODE, ALARM and BUZZER, on all four layers. A spring or the battery edge landing slightly off its pad, or scratched mask, no longer meets a pour of another net right at the pad edge. VCC1 to the GND pour was the battery short-circuit path.
3. **Two GND vias moved out of pasted pads**, so solder no longer wicks into their holes. The stubs were re-routed. Coordinates are KiCad's:
   - J1 pin 2 / C24 pin 2: from (3.51, 11.09) to (3.316, 10.366), under the connector body.
   - C1 pin 2: from (−7.42, 4.35) to (−7.42, 3.95).
4. **Design rules rewritten for KiCad 7.** `memberOfFootprint()` only exists in KiCad 8. KiCad 7.0.11 treats it as false, so the old "except M1/D1" edge rule never matched anything. This is why DRC kept listing the button pads that `VERIFICATION.md` describes as excluded. The rules now match on pad numbers, which are unique to M1:
   - The button half-holes (LIGHT, MODE, ALARM) are exempt from the edge-clearance check.
   - The BUZZER hole is still reported (see below).
   - The new 0.3 mm contact clearance from change 2 is one of these rules.
5. **Revision set to "A1"**. The job file used to say "rev?".

## Verification

- **The pipeline is faithful.** Re-filling and re-plotting the unchanged rev A board with kicad-cli 7.0.11 reproduces the ordered zip exactly: zero XOR area on all 11 layers, and identical drills. So the A1 differences come only from the edits above.
- **DRC:** 1 violation (the BUZZER edge item, see below) and 0 unconnected pads. Rev A had 4 violations.
- **Independent check of the rev A1 gerbers,** using the net attributes in the gerbers:
  - No shorts and no opens. The only split net is `N/C`.
  - Minimum copper clearance is unchanged at 0.100 mm.
  - Mask webs under 0.1 mm: 0 (rev A had 24 on top).
  - Mask openings with copper of another net within 0.05 mm: 0 (rev A had 30 on top and 8 on the bottom).
  - Contact pads to the nearest pour of another net: 0.30 mm.

To rebuild, run `python3 scripts/fix_rev_a1.py kicad/f91w-ble-main.kicad_pcb` on the **rev A** board (git history has it), then `scripts/plot_gerbers.sh`. Both need KiCad 7.0.11.

## Still open

- **Surface finish:** the cart lists the mainboard as **OSP**, while the job file and the design assume ENIG. The LCD zebra pads, the battery contacts and the spring pads are pressure contacts, and OSP isn't made for those. Change the order to ENIG.
- **Three GND vias still sit in pasted pads:** C16 pin 2, R2 pin 2 and C13 pin 2. They're in tightly packed areas: LCD segment traces, the BOOT0 and LSE crystal pins, and the SMPS feedback node next to the antenna keepout. Moving them needs re-routing. For hand assembly, tin those three pads with an iron first so the via fills, then apply paste. Alternatively, order epoxy-filled and capped vias.
- **BUZZER hole:** the 0.85 mm hole pokes 0.05 mm past the edge. Sensor Watch has it in the same place. JLC didn't flag it, and the edge holes are being made without castellation (Option 2), so nothing needs to be done.
- **Unchanged:** the antenna's ground clearance and the 0.08 mm silkscreen text.
- **Renders:** the SVG, PNG and STEP files predate A1 and show the old via positions.
- **PPG flex:** see the rev B section of `ppg-flex-README.md`. A fold section lets it reach the caseback from J1, and the stiffener-edge vias and job file are fixed.
