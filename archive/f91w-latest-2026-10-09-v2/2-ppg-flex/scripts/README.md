# PPG flex rev B build

These scripts need KiCad 7.0.11 (`apt install kicad`). The rev A board comes from git history.

```sh
git show bcc231c:f91w/2-ppg-flex/kicad/f91w-ppg-flex.kicad_pcb > /tmp/revA.kicad_pcb
mkdir -p build
python3 scripts/k10to7.py /tmp/revA.kicad_pcb build/f91w-ppg-flex.kicad_pcb   # KiCad 10 -> 7 syntax
cp scripts/kicad7/f91w-ppg-flex.kicad_pro scripts/kicad7/f91w-ppg-flex.kicad_dru build/
python3 scripts/add_fold_section.py build/f91w-ppg-flex.kicad_pcb 14.0      # length in mm
```

`scripts/kicad7/` holds KiCad 7 versions of the project and rule files. KiCad 7 has no `memberOfFootprint()`, so its edge rule uses the 0.2 mm board setting for the fingers and 0.3 mm rules for pours, tracks and vias.

To plot, run `kicad-cli pcb export gerbers` with the layers `F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts,User.Drawings`, then `kicad-cli pcb export drill` with absolute origin, mm units, decimal format, separate PTH/NPTH files and a gerberx2 drill map.
