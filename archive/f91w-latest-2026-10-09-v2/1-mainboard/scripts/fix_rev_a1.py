#!/usr/bin/env python3
"""Rev A1 mainboard fixes (2026-10-09). Run with KiCad 7.0.11's Python, on the rev A board:

    python3 scripts/fix_rev_a1.py kicad/f91w-ble-main.kicad_pcb

Edits the board in place, refills the zones in a fresh interpreter and writes
kicad/f91w-ble-main.drc.rpt. The pour pull-back around the M1 contacts comes from the
rules in kicad/f91w-ble-main.kicad_dru, which the refill picks up.

1. Solder mask margin 0.0635 -> 0 (the board default every other pad already uses) on:
   - U2 (LIS2DW12 LGA-12) and J1 (FH19C): the mask webs were 0.008-0.023 mm and
     0.073 mm, below the ~0.1 mm fab minimum.
   - D1 (RGB LED), the M1 LCD zebra pads 1-27 and the M1 battery pads GND1/GND2:
     neighbouring traces of other nets sat 0.037-0.045 mm outside the openings, so a
     normal mask shift exposes them next to a zebra strip or under the battery.
2. Move two GND vias whose holes sat under solder paste (solder wicks into them):
   - J1 pin 2 / C24 pin 2: 0.75 mm, to under the connector body.
   - C1 pin 2: 0.4 mm.
   Their stubs are re-routed. The other three in-pad vias (C16, R2, C13) are in
   congested areas and stay; see the rev A1 notes for the hand-assembly workaround.
3. Title block revision "A1" (the job file said "rev?").
"""
import subprocess
import sys

import pcbnew

MM = pcbnew.FromMM

# (old via, new via, [(ref, pad, layer, track width)]) in KiCad coordinates (y down)
VIA_MOVES = [
    ((3.51, 11.09), (3.316, 10.366), [("J1", "2", pcbnew.F_Cu, 0.12), ("C24", "2", pcbnew.B_Cu, 0.15)]),
    ((-7.42, 4.35), (-7.42, 3.95), [("C1", "2", pcbnew.F_Cu, 0.12)]),
]
ZERO_MASK_FOOTPRINTS = ("U2", "D1", "J1")
ZERO_MASK_M1_PADS = {str(n) for n in range(1, 28)} | {"GND1", "GND2"}


def mm_xy(p):
    return (round(pcbnew.ToMM(p.x), 3), round(pcbnew.ToMM(p.y), 3))


def pad_positions(board):
    return {(fp.GetReference(), pad.GetNumber()): mm_xy(pad.GetPosition())
            for fp in board.GetFootprints() for pad in fp.Pads()}


def add_track(board, net, layer, a, b, width):
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(pcbnew.VECTOR2I(MM(a[0]), MM(a[1])))
    t.SetEnd(pcbnew.VECTOR2I(MM(b[0]), MM(b[1])))
    t.SetWidth(MM(width))
    t.SetLayer(layer)
    t.SetNet(net)
    t.SetLocked(True)
    board.Add(t)


def main(path):
    board = pcbnew.LoadBoard(path)

    # 1. mask margins
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        for pad in fp.Pads():
            if ref in ZERO_MASK_FOOTPRINTS or (ref == "M1" and pad.GetNumber() in ZERO_MASK_M1_PADS):
                pad.SetLocalSolderMaskMargin(0)

    # 3. revision
    tb = board.GetTitleBlock()
    tb.SetRevision("A1")
    board.SetTitleBlock(tb)

    # 2. via moves. Look everything up before removing items: KiCad 7's SWIG proxies
    # break after Remove().
    pads = pad_positions(board)
    tracks = list(board.GetTracks())
    vias = {mm_xy(t.GetPosition()): t for t in tracks if t.Type() == pcbnew.PCB_VIA_T}
    for old, new, links in VIA_MOVES:
        via = vias[old]
        net = via.GetNet()
        # Remove the stubs ending on the old via; J1's B.Cu stub also has a dog-leg
        # through (3.54, 11.09).
        stale = {old, (3.54, 11.09)} if old == (3.51, 11.09) else {old}
        for t in tracks:
            if t.Type() != pcbnew.PCB_VIA_T and t.GetNetname() == "GND" \
                    and stale & {mm_xy(t.GetStart()), mm_xy(t.GetEnd())}:
                board.Remove(t)
        via.SetPosition(pcbnew.VECTOR2I(MM(new[0]), MM(new[1])))
        for ref, num, layer, width in links:
            add_track(board, net, layer, pads[(ref, num)], new, width)

    # Creating PCB_TRACKs from Python breaks KiCad 7's SWIG proxies for the rest of the
    # process, so save here and refill in a fresh interpreter.
    board.Save(path)
    subprocess.run([sys.executable, __file__, "--fill", path], check=True)


def fill(path):
    board = pcbnew.LoadBoard(path)
    # Run DRC once first so the filler sees the custom rules (edge clearance,
    # contact-pad clearance); see pcb-layout README.
    pcbnew.WriteDRCReport(board, "/dev/null", pcbnew.EDA_UNITS_MILLIMETRES, True)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)
    report = path.replace(".kicad_pcb", ".drc.rpt")
    pcbnew.WriteDRCReport(board, report, pcbnew.EDA_UNITS_MILLIMETRES, True)
    print(open(report).read())


if __name__ == "__main__":
    if sys.argv[1] == "--fill":
        fill(sys.argv[2])
    else:
        main(sys.argv[1])
