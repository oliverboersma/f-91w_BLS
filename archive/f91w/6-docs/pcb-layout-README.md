# F-91W BLE mainboard layout (rev A)

The board is routed: 0 unconnected nets. DRC shows 4 errors, and all of them are expected (see below). Finished 2026-10-03.

## What's here

| Path | Contents |
|---|---|
| `f91w-kicad-projects.zip` | Self-contained KiCad projects: `f91w-ble-main/` (board and schematic, linked, with the project footprint and symbol libraries) and `f91w-ppg-flex/` (schematic only). Open `f91w-ble-main/f91w-ble-main.kicad_pro` |
| `fab/kicad/` | The same mainboard project, unzipped |
| `PRE-ORDER-ITEMS.md` | Results for the pre-order checks (MAXM86161, MLPF, Q1, LSE, antenna) |
| `fab/f91w-ble-main-gerbers.zip` | Gerbers for 4 layers (F, In1 = GND, In2 = VDD, B), mask, paste, silk, Edge.Cuts, PTH/NPTH Excellon drill files, drill maps and the Gerber job file |
| `fab/f91w-ble-main-pos.csv` | Pick-and-place file, 40 parts on both sides, mm, KiCad coordinates. M1 (case outline and contacts) and J2 (TC2030 pads) are left out |
| `fab/f91w-ble-main-BOM.csv` | BOM, the same file as the schematic BOM. Every BOM reference matches a part on the board |
| `fab/f91w-ble-main-top.svg`, `-bottom.svg`, `-copper.png`, `.step` | Renders and the 3D model |
| `fab/f91w-ble-main.drc.rpt` | Final DRC report |
| `scripts/` | The full pipeline described below |
| `replay/` | Tooling used to rebuild the old session's scratchpad from its transcript |
| `f91w-hw-revA-rebuilt.tgz` | Schematic project, docs, BOMs and netlists |

Board stack: 4 layers, 0.61 mm thick. Minimum track/space is 0.09 mm, vias are 0.4/0.2 mm, and hole-to-copper clearance is 0.2 mm. **This needs a fab that offers 0.09 mm track/space and 0.2 mm drills, such as a JLCPCB or PCBWay 4-layer advanced process.**

**2026-10-08 fix:** the exported pours ran to the board edge on all four layers, because the headless KiCad 7 zone fill ignored the edge clearance. They are now refilled 0.2 mm back from the edge (board setup and the custom rule), and the gerbers were re-exported. Only the copper layers changed. The old files are in `archive/pre-edge-fix-2026-10-08/`. When refilling from a script, run `pcbnew.WriteDRCReport` once before `ZONE_FILLER`; otherwise the filler sees an edge clearance of 0.

## Remaining DRC items (accepted)

The 4 `copper_edge_clearance` errors are the ALARM, LIGHT, MODE and BUZZER contact pads of M1. They come from the Sensor Watch F-91W outline footprint, which deliberately places their centres on or past the board edge so they touch the case springs. Sensor Watch boards ship with the same pads.

**Rev A1:** the custom rule that was meant to exempt these pads used `memberOfFootprint()`, which KiCad 7 doesn't have, so it never matched. The rules now match by pad number. The three button half-holes are exempt, and DRC reports one item: the BUZZER hole, which only grazes the edge and isn't a half-hole. See `REV-A1-CHANGES.md`.

## Pipeline

All commands run from `scripts/`. They need KiCad 7 pcbnew Python, plus shapely, matplotlib and Java with Freerouting 1.9.0 under xvfb-run.

1. `python3 board_build.py place_v8 ../route` builds the placed board from the netlist. It applies the place_v8 placement and adds the hand routes for RF, HSE, SMPS and LSE.
2. `python3 pre_route.py ../route/f91w-ble-main.kicad_pcb ../r7/f91w-ble-main.kicad_pcb` adds the button stubs and the pre-placed GND plane vias, then exports the DSN.
3. `python3 dsn_fix.py r7/f91w-ble-main.dsn r7a/f91w-ble-main.dsn` turns the zebra band into a via-only keepout and raises the clearance to 0.10 mm.
4. `./run_fr.sh r7a f91w-ble-main 40` runs Freerouting single-threaded (`-mt 1`), the same way the old session ran it.
5. `python3 post_route.py ../r7/f91w-ble-main.kicad_pcb ../r7a/f91w-ble-main.ses ../r7a/f91w-ble-main-routed.kicad_pcb` imports the SES (KiCad 7's importer needs the GUI, so `ses_import.py` replaces it), trims the stubs, adds the GND and VDD pours, and stitches orphaned GND islands.
6. `python3 finalize.py ../r7a/f91w-ble-main-routed.kicad_pcb ../r7a/f91w-ble-main-final.kicad_pcb` removes dangling router leftovers, fixes the silk width, sets the pour minimum width to 0.13 mm, re-stitches and runs DRC.
7. The kicad-cli exports (gerbers, drill, pos, svg, step) produce the files in `fab/`.

## What got routing from 120 unconnected nets to 0

- **LCD zebra keepout.** The keepout blocked tracks as well as vias, so none of the segment pads could be reached. It is now a via-only keepout.
- **Button contacts.** Their centres sit outside the outline, so Freerouting ignored them. Short locked stubs now start at ±10.65 mm. Stubs that started outside the outline made Freerouting hang.
- **GND plane vias.** Freerouting left small caps without a via to In1, so the vias are now placed up front.
- **Bottom-row congestion at U1.** C6, C17 and C22 were rotated 270°, Y2 was turned 180°, and C18 was flipped to 270° with its own GND via.
- **HSE_OUT and BTN_MODE.** HSE_OUT was rerouted to free VDDRF. BTN_MODE now escapes through a via.

## Still open before ordering

See `PRE-ORDER-ITEMS.md`. Q1, the LSE crystal and caps, and the MLPF footprint check are done. The MAXM86161 footprint is confirmed. These remain: antenna tuning on the first board, a human layout review, and the PPG flex layout.
