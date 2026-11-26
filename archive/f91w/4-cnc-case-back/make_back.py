"""Parametric CNC case back for the F-91W BLE/PPG mod (CadQuery).
Frame: viewed from the wrist side is NOT used; X/Y are the case frame seen from the dial
(+X = 3 o'clock, +Y = 12 o'clock), origin = case centre, Z = 0 at the case rim, -Z = toward wrist.
Run: python3 make_back.py  -> f91w-cnc-back.step + f91w-cnc-back-drawing.pdf/png
Values marked EST are not measured yet: caliper them on the stock back before ordering.
"""
import cadquery as cq

# --- outline + screws (EST: measure stock back) ---
W, H, R = 31.5, 30.5, 4.0          # plate outline, corner radius            (EST)
SCREW_X, SCREW_Y = 12.6, 11.6      # screw hole centres (+/-)                 (EST)
SCREW_D = 1.6                      # clearance for the stock ~M1.4 screws     (EST)
CSK_D = 2.6                        # 90 deg countersink, flush screw heads    (EST)

# --- measured (oliver's calipers, 2026-10-08) ---
BOWL_ID = 23.99                    # stock bowl inside diameter
INNER_DEPTH = 0.93                 # rim -> inside of stock bowl (8.63 - 7.20 - 0.50)

# --- design ---
FLANGE_T = 0.60                    # plate under the screws
FLOOR_T = 0.60                     # back floor under the battery / piezo
BOSS_D = BOWL_ID + 2 * 1.0         # outside boss, 1 mm wall around the pocket
POCKET_D = BOWL_ID                 # inside pocket = stock bowl, piezo/spring geometry unchanged
TOTAL_T = INNER_DEPTH + FLOOR_T    # 1.53 mm (stock is ~1.43)
EDGE_R = 0.3                       # outside edge break on the boss

# MAXM86161 (U2) window, from pcb-layout/flex/README.md
U2_X, U2_Y = -2.8, 7.4             # case frame (KiCad -7.4 = 12 o'clock)
WIN_W, WIN_H = 3.2, 4.6            # through-cut, module 2.9 x 4.3 + clearance
WIN_R = 0.5                        # 1 mm end mill friendly corners
U2_ROT = 0                         # long side along Y; set to 90 if the flex rotates it

flange = (cq.Workplane("XY").sketch().rect(W, H).vertices().fillet(R).finalize()
          .extrude(-FLANGE_T))
boss = (cq.Workplane("XY").workplane(offset=-FLANGE_T).circle(BOSS_D / 2)
        .extrude(-(TOTAL_T - FLANGE_T)))
body = flange.union(boss)
try:
    body = body.faces("<Z").edges().fillet(EDGE_R)
except Exception:
    pass
body = body.cut(cq.Workplane("XY").circle(POCKET_D / 2).extrude(-INNER_DEPTH))
win = (cq.Workplane("XY").center(U2_X, U2_Y)
       .sketch().rect(WIN_W, WIN_H, angle=U2_ROT).vertices().fillet(WIN_R).finalize()
       .extrude(-TOTAL_T - 1, both=True))
body = body.cut(win)
pts = [(sx * SCREW_X, sy * SCREW_Y) for sx in (-1, 1) for sy in (-1, 1)]
# clearance holes + 90 deg countersinks opening on the wrist side of the flange (z = -FLANGE_T)
body = body.cut(cq.Workplane("XY").pushPoints(pts).circle(SCREW_D / 2).extrude(-3, both=True))
for x, y in pts:
    cone = cq.Solid.makeCone(CSK_D / 2, 0, CSK_D / 2, pnt=cq.Vector(x, y, -FLANGE_T),
                             dir=cq.Vector(0, 0, 1))
    body = body.cut(cq.Workplane().add(cone))

if __name__ == "__main__":
    import os
    d = os.path.dirname(os.path.abspath(__file__))
    cq.exporters.export(body, os.path.join(d, "f91w-cnc-back.step"))
    bb = body.val().BoundingBox()
    print("bbox", round(bb.xlen, 2), round(bb.ylen, 2), round(bb.zlen, 2), "vol mm3", round(body.val().Volume(), 1))
