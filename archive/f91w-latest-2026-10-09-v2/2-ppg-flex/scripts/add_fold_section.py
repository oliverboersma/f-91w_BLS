#!/usr/bin/env python3
"""PPG flex rev B: add a fold section between the FFC tail and the body (KiCad 7.0.11).

    python3 scripts/add_fold_section.py kicad/f91w-ppg-flex.kicad_pcb [length_mm]

Why: J1 on the mainboard is an FH19C (bottom contact) on the LCD side at 6 o'clock, and its
slot opens toward the board centre. The tail goes in from the centre side, fingers down.
To reach the caseback the flex folds back over J1, wraps around the 6 o'clock board edge,
runs under the board beside the battery and steps down into the bowl. Two 180-degree folds
leave B.Cu (fingers, U2) facing the skin, so no parts change sides. Rev A had only 0.55 mm
of flex between the tail stiffener and the body, so it could not make that path.

What it does (KiCad coordinates, y down):
- Moves everything of the tail below Y_CUT down by LENGTH: J1 (the fingers), the tail
  traces, stiffener 1 and its note, the tail outline. Body, arm and head do not move, so
  the pos file still applies.
- Splits the B.Cu traces that cross Y_CUT and bridges them with straight runs.
- Adds a 0.25 mm GND trace (rev A fed the GND finger only through the pours).
- Adds pour keepouts over the fold section on both layers: traces only, on B.Cu, no vias,
  all perpendicular to the fold lines.
- Notes the fold section on User.Drawings, sets the title-block revision to B, then refills
  the zones and writes a DRC report in a fresh interpreter (KiCad 7's SWIG proxies break
  after Python creates board items).
"""
import subprocess
import sys

import pcbnew

Y_CUT = 106.62        # copper/footprint/drawing split line, just above stiffener 1 (106.6461)
Y_EDGE = 107.0        # outline/zone vertices below this move; keeps the body's corner fillet
LENGTH = 14.0         # fold-section length, mm (adjust after the paper fit check)
X_LEFT, X_RIGHT = 150.7363, 155.7147   # tail/fold-section outline

MM = pcbnew.FromMM


def mm(v):
    return pcbnew.ToMM(v)


def vec(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def main(path, length):
    board = pcbnew.LoadBoard(path)
    dy = MM(length)
    shift = pcbnew.VECTOR2I(0, dy)

    nets = {n: board.FindNet(n) for n in ("GND",)}
    # --- collect before editing (SWIG proxies go stale after Add/Remove) ---
    tracks = list(board.GetTracks())
    drawings = list(board.GetDrawings())
    zones = list(board.Zones())
    j1 = [f for f in board.GetFootprints() if f.GetReference() == "J1"][0]
    new_segments = []   # (net, layer, width, (x1, y1), (x2, y2))

    # footprints: only J1 (the fingers) sits on the tail
    j1.Move(shift)

    # tracks
    for t in tracks:
        if t.Type() == pcbnew.PCB_VIA_T:
            if mm(t.GetPosition().y) > Y_CUT:
                t.Move(shift)
            continue
        s, e = t.GetStart(), t.GetEnd()
        ys, ye = mm(s.y), mm(e.y)
        if ys > Y_CUT and ye > Y_CUT:
            t.Move(shift)
        elif (ys - Y_CUT) * (ye - Y_CUT) < 0:
            a, b = (s, e) if ys < ye else (e, s)          # a above the cut, b below
            xa, ya, xb, yb = mm(a.x), mm(a.y), mm(b.x), mm(b.y)
            xc = xa + (xb - xa) * (Y_CUT - ya) / (yb - ya)
            if ys < ye:
                t.SetEnd(vec(xc, Y_CUT))
            else:
                t.SetStart(vec(xc, Y_CUT))
            w = mm(t.GetWidth())
            new_segments.append((t.GetNet(), t.GetLayer(), w, (xc, Y_CUT), (xc, Y_CUT + length)))
            new_segments.append((t.GetNet(), t.GetLayer(), w, (xc, Y_CUT + length), (xb, yb + length)))

    # GND: from the stitching via at the tail/body junction, down the fold section, to finger 2
    gnd = nets["GND"]
    x_lane = 154.4
    gnd_path = [(153.571, 106.35), (x_lane, Y_CUT), (x_lane, Y_CUT + length + 0.1),
                (154.726, Y_CUT + length + 0.426), (154.726, 108.891 + length)]
    for p, q in zip(gnd_path, gnd_path[1:]):
        new_segments.append((gnd, pcbnew.B_Cu, 0.25, p, q))

    # zones: stretch outlines over the moved tail; refilled below
    for z in zones:
        outline = z.Outline()
        for i in range(outline.TotalVertices()):
            v = outline.CVertex(i)
            if mm(v.y) > Y_EDGE:
                outline.SetVertex(i, pcbnew.VECTOR2I(v.x, v.y + dy))
        z.UnFill()

    # drawings: outline vertices below the fillet, stiffener 1 and its note
    for d in drawings:
        if d.GetLayerName() == "Edge.Cuts" and d.GetClass() == "PCB_SHAPE":
            s, e = d.GetStart(), d.GetEnd()
            if mm(s.y) > Y_EDGE:
                d.SetStart(pcbnew.VECTOR2I(s.x, s.y + dy))
            if mm(e.y) > Y_EDGE:
                d.SetEnd(pcbnew.VECTOR2I(e.x, e.y + dy))
        elif d.GetLayerName() == "User.Drawings":
            if mm(d.GetBoundingBox().GetCenter().y) > Y_CUT:
                d.Move(shift)
        elif d.GetClass() == "PCB_TEXT" and "PPG flex rev A" in d.GetText():
            d.SetText(d.GetText().replace("rev A", "rev B"))

    tb = board.GetTitleBlock()
    tb.SetRevision("B")
    board.SetTitleBlock(tb)

    for net, layer, w, p, q in new_segments:
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(vec(*p))
        t.SetEnd(vec(*q))
        t.SetWidth(MM(w))
        t.SetLayer(layer)
        t.SetNet(net)
        board.Add(t)

    # pour keepouts over the fold section, both copper layers
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowCopperPour(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowTracks(layer == pcbnew.F_Cu)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        z.SetLayer(layer)
        z.SetZoneName("fold section")
        o = z.Outline()
        o.NewOutline()
        for x, y in ((X_LEFT - 0.2, Y_CUT), (X_RIGHT + 0.2, Y_CUT),
                     (X_RIGHT + 0.2, Y_CUT + length), (X_LEFT - 0.2, Y_CUT + length)):
            o.Append(MM(x), MM(y))
        board.Add(z)

    def text(s, x, y, size=0.6, right=False):
        t = pcbnew.PCB_TEXT(board)
        t.SetText(s)
        t.SetLayer(pcbnew.Dwgs_User)
        t.SetPosition(vec(x, y))
        t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
        t.SetTextThickness(MM(0.075))
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT if right else pcbnew.GR_TEXT_H_ALIGN_LEFT)
        board.Add(t)

    text(f"FOLD SECTION {length:.1f} mm: no stiffener, no pour, B.Cu traces only.\n"
         "Folded by hand at install, plain side (F) inside both folds:\n"
         "1) back over J1   2) around the 6 o'clock board edge   3) step down into the bowl.\n"
         "Bend radius >= 0.4 mm; keep bends >= 1 mm from the body.", 158.0, Y_CUT + 2.0)
    # nominal fold positions (for the paper fit check), measured from the stiffener end
    tail_end = Y_CUT + length + 0.026          # stiffener 1 starts here
    for label, d in (("fold 1 (over J1)", 0.0), ("fold 2 (6 o'clock edge), approx.", 6.3),
                     ("step down past battery, approx.", 11.0)):
        y = tail_end - d
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(vec(X_LEFT - 0.6, y))
        seg.SetEnd(vec(X_RIGHT + 0.6, y))
        seg.SetLayer(pcbnew.Dwgs_User)
        seg.SetWidth(MM(0.05))
        board.Add(seg)
        text(label, X_LEFT - 0.8, y, 0.5, right=True)

    board.Save(path)
    subprocess.run([sys.executable, __file__, "--fill", path], check=True)


def fill(path):
    board = pcbnew.LoadBoard(path)
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
        main(sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else LENGTH)
