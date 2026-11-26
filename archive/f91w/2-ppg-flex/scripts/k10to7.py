"""Convert the KiCad 10 PPG-flex board to KiCad 7 (file version 20221018) syntax.

Only the constructs this board uses are handled. Geometry is copied verbatim; the
conversion is checked by plotting the result with KiCad 7 and diffing the gerbers
against the verified KiCad 10 output.
"""
import sys
from sexp import parse, find, one, Q

DROP_TOP = {"embedded_fonts", "generator_version"}
DROP_ANY = {"embedded_fonts", "duplicate_pad_numbers_are_jumpers", "capping", "covering",
            "plugging", "filling", "placement", "uuid_ignored"}

LAYERS_V7 = """  (layers
    (0 "F.Cu" signal)
    (31 "B.Cu" signal)
    (32 "B.Adhes" user "B.Adhesive")
    (33 "F.Adhes" user "F.Adhesive")
    (34 "B.Paste" user)
    (35 "F.Paste" user)
    (36 "B.SilkS" user "B.Silkscreen")
    (37 "F.SilkS" user "F.Silkscreen")
    (38 "B.Mask" user)
    (39 "F.Mask" user)
    (40 "Dwgs.User" user "User.Drawings")
    (41 "Cmts.User" user "User.Comments")
    (42 "Eco1.User" user "User.Eco1")
    (43 "Eco2.User" user "User.Eco2")
    (44 "Edge.Cuts" user)
    (45 "Margin" user)
    (46 "B.CrtYd" user "B.Courtyard")
    (47 "F.CrtYd" user "F.Courtyard")
    (48 "B.Fab" user)
    (49 "F.Fab" user)
  )"""


def ser(x):
    if isinstance(x, list):
        return "(" + " ".join(ser(y) for y in x) + ")"
    if isinstance(x, Q):
        return '"' + x.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return x


class Conv:
    def __init__(self, tree):
        self.t = tree
        self.nets = {"": 0}
        self._collect(tree)

    def _collect(self, x):
        if isinstance(x, list):
            if x and x[0] == "net" and len(x) == 2 and isinstance(x[1], Q):
                self.nets.setdefault(str(x[1]), len(self.nets))
            for y in x:
                self._collect(y)

    # generic cleanup applied to every node
    def clean(self, x, ctx=""):
        if not isinstance(x, list):
            return x
        out = []
        for y in x:
            if isinstance(y, list) and y and y[0] in DROP_ANY:
                continue
            out.append(self.clean(y, x[0] if x else ""))
        if out and out[0] == "uuid":
            out[0] = "tstamp"
            out[1] = str(out[1])           # tstamp is unquoted in KiCad 7
        if out and out[0] == "fill" and len(out) >= 2 and out[1] in ("no", "yes") and ctx in (
                "gr_poly", "fp_poly", "fp_rect", "fp_circle", "gr_rect", "gr_circle"):
            out = ["fill", "solid" if out[1] == "yes" else "none"]
        if out and out[0] == "hide" and len(out) == 2:
            return "__HIDE__" if out[1] == "yes" else None
        return out

    def strip_none(self, x):
        if isinstance(x, list):
            return [self.strip_none(y) for y in x if y is not None]
        return x

    def netref(self, node, with_name):
        n = one(node, "net")
        if n is None:
            return node
        name = str(n[1])
        code = self.nets[name]
        node[node.index(n)] = ["net", str(code), Q(name)] if with_name else ["net", str(code)]
        return node

    def footprint(self, fp):
        fp = self.strip_none(self.clean(fp))
        out = [fp[0], fp[1]]
        texts = []
        for item in fp[2:]:
            if isinstance(item, list) and item[0] == "property":
                key, val = str(item[1]), item[2]
                if key in ("Reference", "Value"):
                    kind = "reference" if key == "Reference" else "value"
                    rest = [y for y in item[3:] if y != "__HIDE__"]
                    hidden = "__HIDE__" in item[3:]
                    t = ["fp_text", kind, val]
                    for y in rest:
                        if isinstance(y, list) and y[0] in ("at", "layer"):
                            t.append(y)
                    if hidden:
                        t.append("hide")
                    for y in rest:
                        if isinstance(y, list) and y[0] not in ("at", "layer"):
                            t.append(y)
                    texts.append(t)
                else:
                    out.append(["property", Q(key), val])
                continue
            if isinstance(item, list) and item[0] == "pad":
                item = self.netref(item, True)
            if isinstance(item, list) and item[0] == "fp_text":
                hidden = "__HIDE__" in item
                item = [y for y in item if y != "__HIDE__"]
                if hidden:
                    li = [i for i, y in enumerate(item) if isinstance(y, list) and y[0] == "layer"][0]
                    item.insert(li + 1, "hide")
            out.append(item)
        # KiCad 7 wants reference/value text right after the header fields
        hdr = [y for y in out[2:] if isinstance(y, list) and y[0] in ("layer", "tstamp", "at", "descr", "tags", "property", "path", "attr")]
        body = [y for y in out[2:] if y not in hdr]
        return out[:2] + hdr + texts + body

    def run(self):
        t = self.t
        lines = ["(kicad_pcb (version 20221018) (generator pcbnew)", "  (general (thickness %s))" % one(one(t, "general"), "thickness")[1],
                 "  (paper \"A4\")", LAYERS_V7,
                 "  (setup (pad_to_mask_clearance 0))"]
        for name, code in sorted(self.nets.items(), key=lambda kv: kv[1]):
            lines.append("  (net %d %s)" % (code, ser(Q(name))))
        for item in t[1:]:
            if not isinstance(item, list) or item[0] in DROP_TOP | {"version", "generator", "general", "paper", "layers", "setup"}:
                continue
            k = item[0]
            if k == "footprint":
                node = self.footprint(item)
            else:
                node = self.strip_none(self.clean(item))
                if k in ("segment", "via", "arc"):
                    node = self.netref(node, False)
                elif k == "zone":
                    n = one(node, "net")
                    name = str(n[1]) if n else ""
                    rest = [y for y in node[1:] if not (isinstance(y, list) and y[0] == "net")]
                    node = ["zone", ["net", str(self.nets[name])], ["net_name", Q(name)]] + rest
                    mt = [i for i, y in enumerate(node) if isinstance(y, list) and y[0] == "min_thickness"][0]
                    node.insert(mt + 1, ["filled_areas_thickness", "no"])
                elif k == "gr_text":
                    node = [y for y in node if y != "__HIDE__"]
            lines.append("  " + ser(node))
        lines.append(")")
        return "\n".join(lines) + "\n"


if __name__ == "__main__":
    tree = parse(open(sys.argv[1]).read())
    open(sys.argv[2], "w").write(Conv(tree).run())
