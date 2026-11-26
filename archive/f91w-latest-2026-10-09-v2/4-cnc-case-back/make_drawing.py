"""Dimensioned drawing for f91w-cnc-back (reads the parameters from make_back.py)."""
import os, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle, Rectangle
import make_back as m

fig, (a, b) = plt.subplots(1, 2, figsize=(11.7, 8.3), gridspec_kw={"width_ratios": [1.3, 1]})
def dim(ax, p, q, txt, off=(0, 0)):
    ax.annotate("", p, q, arrowprops=dict(arrowstyle="<->", lw=0.7))
    ax.text((p[0] + q[0]) / 2 + off[0], (p[1] + q[1]) / 2 + off[1], txt, ha="center", va="center",
            fontsize=8, bbox=dict(fc="white", ec="none", pad=0.5))

W, H = m.W, m.H
a.add_patch(FancyBboxPatch((-W/2+m.R, -H/2+m.R), W-2*m.R, H-2*m.R, boxstyle=f"round,pad={m.R}", fill=False, lw=1.2))
a.add_patch(Circle((0, 0), m.BOSS_D/2, fill=False, lw=1))
a.add_patch(Circle((0, 0), m.POCKET_D/2, fill=False, ls="--", lw=0.8))
for x, y in m.pts:
    a.add_patch(Circle((x, y), m.SCREW_D/2, fill=False)); a.add_patch(Circle((x, y), m.CSK_D/2, fill=False, lw=0.5))
w, h = (m.WIN_H, m.WIN_W) if m.U2_ROT in (90, 270) else (m.WIN_W, m.WIN_H)
a.add_patch(Rectangle((m.U2_X-w/2, m.U2_Y-h/2), w, h, fill=True, fc="0.85", ec="k"))
a.plot([-W/2-2, W/2+2], [0, 0], "k-.", lw=0.4); a.plot([0, 0], [-H/2-2, H/2+2], "k-.", lw=0.4)
dim(a, (-W/2, -H/2-3), (W/2, -H/2-3), f"{W:.2f} (EST)")
dim(a, (-W/2-3, -H/2), (-W/2-3, H/2), f"{H:.2f}\n(EST)", (-1.5, 0))
dim(a, (-m.SCREW_X, H/2+2.5), (m.SCREW_X, H/2+2.5), f"{2*m.SCREW_X:.2f} screw c-c (EST)")
dim(a, (W/2+2.5, -m.SCREW_Y), (W/2+2.5, m.SCREW_Y), f"{2*m.SCREW_Y:.2f}\nc-c\n(EST)", (1.8, 0))
a.text(m.SCREW_X-3, m.SCREW_Y-4.2, f"4x Ø{m.SCREW_D} thru\n90° csk Ø{m.CSK_D} wrist side", fontsize=7, ha="center")
a.text(0, -6, f"boss Ø{m.BOSS_D:.2f}\npocket Ø{m.POCKET_D:.2f} (dashed)\ndepth {m.INNER_DEPTH:.2f} from rim face", fontsize=8, ha="center")
a.annotate(f"sensor window {m.WIN_W} x {m.WIN_H} thru, R{m.WIN_R}\ncentre ({m.U2_X}, {m.U2_Y})", (m.U2_X, m.U2_Y+h/2),
           (m.U2_X-4, H/2+6), fontsize=8, arrowprops=dict(arrowstyle="->", lw=0.6))
a.text(-W/2, -H/2-7, f"Corners R{m.R} (EST). View from the dial side looking toward the wrist; +Y = 12 o'clock, origin = case centre.",
       fontsize=7)
a.set_aspect("equal"); a.set_xlim(-W/2-7, W/2+7); a.set_ylim(-H/2-9, H/2+9); a.axis("off")
a.set_title("PLAN", fontsize=10)

# section through the centre, along Y (12 o'clock up -> right)
Y0 = -H/2; T, F = m.TOTAL_T, m.FLANGE_T; S = 4  # vertical exaggeration
prof = [(-H/2, 0), (-m.POCKET_D/2, 0), (-m.POCKET_D/2, -m.INNER_DEPTH), (m.POCKET_D/2, -m.INNER_DEPTH),
        (m.POCKET_D/2, 0), (H/2, 0), (H/2, -F), (m.BOSS_D/2, -F), (m.BOSS_D/2, -T), (-m.BOSS_D/2, -T),
        (-m.BOSS_D/2, -F), (-H/2, -F), (-H/2, 0)]
b.fill([p[0] for p in prof], [p[1]*S for p in prof], fc="0.8", ec="k", lw=1)
b.add_patch(Rectangle((m.U2_Y-m.WIN_H/2, -T*S), m.WIN_H, m.INNER_DEPTH*S + (T-m.INNER_DEPTH)*S, fc="white", ec="k", lw=0.6))
b.plot([-H/2-1, H/2+1], [0, 0], "k-.", lw=0.4); b.text(H/2+1, 0.2, "case rim / battery face", fontsize=7, ha="right", va="bottom")
dim(b, (H/2+1.5, 0), (H/2+1.5, -F*S), f"{F}", (1.2, 0))
dim(b, (-H/2-1.5, 0), (-H/2-1.5, -T*S), f"{T:.2f}", (-1.5, 0))
dim(b, (-3, 0), (-3, -m.INNER_DEPTH*S), f"{m.INNER_DEPTH}", (1.2, 0))
dim(b, (-6, -m.INNER_DEPTH*S), (-6, -T*S), f"floor {m.FLOOR_T}", (2.2, 0))
b.text(m.U2_Y, -T*S-1.3, "sensor window", fontsize=7, ha="center")
b.text(-H/2, -T*S-4, f"SECTION along Y through centre (vertical x{S}). Outside edge of boss broken R{m.EDGE_R}.",
       fontsize=7)
b.set_aspect("equal"); b.set_xlim(-H/2-5, H/2+5); b.set_ylim(-T*S-6, 3); b.axis("off"); b.set_title("SECTION", fontsize=10)

fig.suptitle("F-91W CNC case back, rev A. 6061-T6, bead blast + clear anodise. mm. Tol ±0.05 unless noted. "
             "Break sharp edges. EST = measure stock back first.", fontsize=9)
d = os.path.dirname(os.path.abspath(__file__))
for ext in ("pdf", "png"):
    fig.savefig(os.path.join(d, f"f91w-cnc-back-drawing.{ext}"), dpi=150, bbox_inches="tight")
