#!/bin/bash
# Plot the mainboard gerbers and drill files with kicad-cli 7.0.11, matching the layer set
# and formats of the ordered rev A zip (Protel extensions, X2, absolute origin, decimal mm drills).
#   scripts/plot_gerbers.sh kicad/f91w-ble-main.kicad_pcb out/
set -e
B="$1"; O="$2"; mkdir -p "$O"
kicad-cli pcb export gerbers --layers F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts -o "$O/" "$B"
kicad-cli pcb export drill --format excellon --drill-origin absolute --excellon-zeros-format decimal \
  --excellon-units mm --excellon-separate-th --generate-map --map-format gerberx2 -o "$O/" "$B"
