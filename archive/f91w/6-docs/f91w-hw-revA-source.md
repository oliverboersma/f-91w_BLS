# F-91W BLE hardware rev A - generator + firmware source

Full source of the package `f91w-hw-revA.zip` so a later session can regenerate it.

Rebuild: put these files in `gen/` (and the hr files in `fw/` or `firmware/hr/`), install KiCad (kicad-cli) and a clone of https://github.com/joeycastillo/Sensor-Watch, then run

`SENSOR_WATCH_REPO=/path/to/Sensor-Watch python3 gen/build_all.py <outdir>`.


## `gen/sexp.py`

```python
"""Minimal S-expression parser/serializer for KiCad files."""
import re

class Q(str):
    """A quoted string atom."""
    pass

_tok = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)

def parse(text):
    pos = 0
    stack = [[]]
    n = len(text)
    while pos < n:
        m = _tok.match(text, pos)
        if not m:
            if text[pos:].strip() == '':
                break
            raise ValueError("parse error at %d: %r" % (pos, text[pos:pos+40]))
        pos = m.end()
        if m.group(1):
            stack.append([])
        elif m.group(2):
            lst = stack.pop()
            stack[-1].append(lst)
        elif m.group(3) is not None:
            s = m.group(3)
            s = s.replace('\\"', '"').replace('\\n', '\n').replace('\\\\', '\\')
            stack[-1].append(Q(s))
        else:
            stack[-1].append(m.group(4))
    assert len(stack) == 1, "unbalanced parens"
    return stack[0][0] if len(stack[0]) == 1 else stack[0]

def _atom(a):
    if isinstance(a, Q):
        return '"' + a.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'
    if isinstance(a, bool):
        return 'yes' if a else 'no'
    if isinstance(a, float):
        s = ('%.4f' % a).rstrip('0').rstrip('.')
        return '0' if s in ('-0', '') else s
    return str(a)

def dump(e, indent=0):
    if not isinstance(e, list):
        return _atom(e)
    # short lists on one line
    if all(not isinstance(x, list) for x in e):
        return '(' + ' '.join(_atom(x) for x in e) + ')'
    pad = '  ' * (indent + 1)
    parts = []
    head = []
    for x in e:
        if not isinstance(x, list) and not parts:
            head.append(_atom(x))
        else:
            parts.append(dump(x, indent + 1))
    s = '(' + ' '.join(head)
    for p in parts:
        s += '\n' + pad + p
    s += ')'
    return s

def find(e, key):
    """first child list whose head == key"""
    for x in e:
        if isinstance(x, list) and x and x[0] == key:
            return x
    return None

def findall(e, key):
    return [x for x in e if isinstance(x, list) and x and x[0] == key]

```


## `gen/kicadlib.py`

```python
"""Load KiCad symbols (with 'extends' flattening) and extract pins."""
import copy, os, re
import sexp
from sexp import Q, find, findall

SYS_SYM_DIR = '/usr/share/kicad/symbols'
_cache = {}
_extra_libs = {}   # nickname -> path

def register_lib(nick, path):
    _extra_libs[nick] = path
    _cache.pop(nick, None)

def _load_lib(nick):
    if nick not in _cache:
        path = _extra_libs.get(nick) or os.path.join(SYS_SYM_DIR, nick + '.kicad_sym')
        tree = sexp.parse(open(path).read())
        _cache[nick] = {s[1]: s for s in findall(tree, 'symbol')}
    return _cache[nick]

def get_symbol(lib_id):
    """Return a flattened symbol s-expr named 'Lib:Name' (for lib_symbols)."""
    nick, name = lib_id.split(':', 1)
    lib = _load_lib(nick)
    sym = copy.deepcopy(lib[name])
    ext = find(sym, 'extends')
    if ext:
        parent = copy.deepcopy(get_symbol(nick + ':' + ext[1]))
        pname = ext[1]
        # child properties override parent's
        child_props = {p[1]: p for p in findall(sym, 'property')}
        out = [parent[0], Q(lib_id)]
        for item in parent[2:]:
            if isinstance(item, list) and item and item[0] == 'property' and item[1] in child_props:
                out.append(child_props.pop(item[1]))
            elif isinstance(item, list) and item and item[0] == 'symbol':
                sub = copy.deepcopy(item)
                sub[1] = Q(str(sub[1]).replace(pname + '_', name + '_', 1))
                out.append(sub)
            else:
                out.append(item)
        # remaining child-only properties: insert after last property
        idx = max(i for i, x in enumerate(out) if isinstance(x, list) and x and x[0] == 'property')
        for p in child_props.values():
            idx += 1
            out.insert(idx, p)
        return out
    sym[1] = Q(lib_id)
    # sub-symbol names keep bare name (KiCad convention)
    return sym

def pins(sym):
    """List of dicts: number, name, type, x, y, angle, length, unit, hidden"""
    res = []
    base = str(sym[1]).split(':', 1)[-1]
    for sub in findall(sym, 'symbol'):
        m = re.match(r'.*_(\d+)_(\d+)$', str(sub[1]))
        unit = int(m.group(1)) if m else 1
        for p in findall(sub, 'pin'):
            at = find(p, 'at'); ln = find(p, 'length')
            nm = find(p, 'name'); nu = find(p, 'number')
            res.append(dict(type=p[1], shape=p[2], x=float(at[1]), y=float(at[2]),
                            angle=int(float(at[3])) if len(at) > 3 else 0,
                            length=float(ln[1]) if ln else 0.0,
                            name=str(nm[1]) if nm else '', number=str(nu[1]) if nu else '',
                            unit=unit, hidden=('hide' in p)))
    return res

def prop(sym, key):
    for p in findall(sym, 'property'):
        if p[1] == key:
            return str(p[2])
    return None

```


## `gen/sch.py`

```python
"""Tiny KiCad 7 schematic writer: places library symbols and connects pins by net labels."""
import uuid as _uuid, datetime, math
from collections import defaultdict
import sexp, kicadlib as K
from sexp import Q

GRID = 1.27
def snap(v): return round(round(v / GRID) * GRID, 4)
def U(): return str(_uuid.uuid4())
def font(size=1.27): return ['font', ['size', size, size]]

POWER_NETS = {'GND': 'power:GND', 'VDD': 'power:VDD'}

def _body_rect(sym):
    """union of rectangles in the symbol graphics (lib coords): (x0,y0,x1,y1)"""
    import sexp as _s
    rs = []
    for sub in _s.findall(sym, 'symbol'):
        for r in _s.findall(sub, 'rectangle'):
            a = _s.find(r, 'start'); b = _s.find(r, 'end')
            xa, ya, xb, yb = float(a[1]), float(a[2]), float(b[1]), float(b[2])
            rs.append((min(xa, xb), min(ya, yb), max(xa, xb), max(ya, yb)))
    if not rs:
        return None
    return (min(r[0] for r in rs), min(r[1] for r in rs), max(r[2] for r in rs), max(r[3] for r in rs))

class Part:
    def __init__(self, sch, lib_id, ref, value, at, rot, sym):
        self.sch, self.lib_id, self.ref, self.value = sch, lib_id, ref, value
        self.x, self.y, self.rot = snap(at[0]), snap(at[1]), rot
        self.sym = sym
        self.pins = K.pins(sym)
    def pin(self, key):
        """find pin by number, or by unique name"""
        c = [p for p in self.pins if p['number'] == str(key)]
        if not c:
            c = [p for p in self.pins if p['name'] == str(key)]
        if len(c) != 1:
            raise KeyError(f"{self.ref}: pin {key!r} -> {len(c)} matches")
        return c[0]
    def endpoint(self, p):
        # lib coords are y-up; schematic y-down; only rot 0 supported for pin geometry
        assert self.rot == 0, "pin geometry only supported for rot=0"
        return snap(self.x + p['x']), snap(self.y - p['y'])

class Sch:
    def __init__(self, project, title, paper='A3', rev='A', company='', comments=()):
        self.project, self.title, self.paper = project, title, paper
        self.rev, self.company, self.comments = rev, company, list(comments)
        self.uuid = U()
        self.items = []
        self.libsyms = {}
        self.parts = []
        self.netmap = defaultdict(set)    # net -> {(ref,pin)}
        self.nc = set()                   # (ref,pin)
        self.pwr_n = 0
        self.flag_n = 0
        self.points = {}                  # (x,y) -> net   (detect accidental shorts)

    # ---- primitives -------------------------------------------------
    def _lib(self, lib_id):
        if lib_id not in self.libsyms:
            self.libsyms[lib_id] = K.get_symbol(lib_id)
        return self.libsyms[lib_id]

    def place(self, lib_id, ref, value, at, footprint=None, rot=0, fields=None, dnp=False,
              in_bom=True, on_board=True, show_value=True):
        sym = self._lib(lib_id)
        p = Part(self, lib_id, ref, value, at, rot, sym)
        fp = footprint if footprint is not None else (K.prop(sym, 'Footprint') or '')
        ds = K.prop(sym, 'Datasheet') or '~'
        x, y = p.x, p.y
        # reference/value text placed to the right of symbol origin by default
        xs = [q['x'] for q in p.pins] or [0]; ys = [q['y'] for q in p.pins] or [0]
        right = max(xs) if xs else 0
        top = max(ys) if ys else 0
        vertical2 = len(p.pins) <= 3 and all(abs(q['x']) < 0.01 for q in p.pins)
        body = _body_rect(sym)
        if vertical2:
            rx, ry = snap(x + 2.54), snap(y - 1.27)
        elif body:
            (bx0, by0, bx1, by1) = body
            has_top = any(q['angle'] == 270 for q in p.pins)
            if has_top:   # right of the body, above the first right-side pin
                rx, ry = snap(x + bx1 + 1.27), snap(y - by1 - 3.81)
            else:         # above the body, left aligned
                rx, ry = snap(x + bx0), snap(y - by1 - 3.81)
        else:
            rx, ry = snap(x + max(xs) + 1.27), snap(y - max(ys) - 3.81)
        e = ['symbol', ['lib_id', Q(lib_id)], ['at', x, y, rot], ['unit', 1],
             ['in_bom', in_bom], ['on_board', on_board], ['dnp', dnp], ['uuid', U()],
             ['property', Q('Reference'), Q(ref), ['at', rx, ry, 0], ['effects', font(), ['justify', 'left']]],
             ['property', Q('Value'), Q(value), ['at', rx, snap(ry + 2.54), 0],
                 ['effects', font(), ['justify', 'left']] + ([] if show_value else ['hide'])],
             ['property', Q('Footprint'), Q(fp), ['at', x, y, 0], ['effects', font(), 'hide']],
             ['property', Q('Datasheet'), Q(ds), ['at', x, y, 0], ['effects', font(), 'hide']]]
        for k, v in (fields or {}).items():
            e.append(['property', Q(k), Q(v), ['at', x, y, 0], ['effects', font(), 'hide']])
        for q in p.pins:
            e.append(['pin', Q(q['number']), ['uuid', U()]])
        e.append(['instances', ['project', Q(self.project),
                  ['path', Q('/' + self.uuid), ['reference', Q(ref)], ['unit', 1]]]])
        self.items.append(e)
        p.fp = fp; p.fields = fields or {}; p.dnp = dnp; p.in_bom = in_bom
        self.parts.append(p)
        return p

    def label(self, net, x, y, angle):
        just = {0: ['justify', 'left', 'bottom'], 180: ['justify', 'right', 'bottom'],
                90: ['justify', 'left', 'bottom'], 270: ['justify', 'right', 'bottom']}[angle]
        self.items.append(['label', Q(net), ['at', snap(x), snap(y), angle], ['fields_autoplaced'],
                           ['effects', font(), just], ['uuid', U()]])

    def power_symbol(self, lib_id, x, y, rot=0):
        sym = self._lib(lib_id)
        val = K.prop(sym, 'Value')
        is_flag = lib_id.endswith('PWR_FLAG')
        if is_flag:
            self.flag_n += 1; ref = '#FLG%02d' % self.flag_n
        else:
            self.pwr_n += 1; ref = '#PWR%03d' % self.pwr_n
        e = ['symbol', ['lib_id', Q(lib_id)], ['at', snap(x), snap(y), rot], ['unit', 1],
             ['in_bom', False], ['on_board', True], ['dnp', False], ['uuid', U()],
             ['property', Q('Reference'), Q(ref), ['at', snap(x), snap(y), 0], ['effects', font(), 'hide']],
             ['property', Q('Value'), Q(val), ['at', snap(x), snap(y + (3.81 if (val == 'GND') else -3.81)), 0], ['effects', font()]],
             ['property', Q('Footprint'), Q(''), ['at', snap(x), snap(y), 0], ['effects', font(), 'hide']],
             ['property', Q('Datasheet'), Q(''), ['at', snap(x), snap(y), 0], ['effects', font(), 'hide']],
             ['pin', Q('1'), ['uuid', U()]],
             ['instances', ['project', Q(self.project),
                 ['path', Q('/' + self.uuid), ['reference', Q(ref)], ['unit', 1]]]]]
        self.items.append(e)

    def noconnect(self, x, y):
        self.items.append(['no_connect', ['at', snap(x), snap(y)], ['uuid', U()]])

    def text(self, s, x, y, size=1.27, bold=False):
        f = font(size) + (['bold'] if bold else [])
        self.items.append(['text', Q(s), ['at', snap(x), snap(y), 0], ['effects', f, ['justify', 'left', 'bottom']], ['uuid', U()]])

    def wire(self, x1, y1, x2, y2):
        self.items.append(['wire', ['pts', ['xy', snap(x1), snap(y1)], ['xy', snap(x2), snap(y2)]],
                           ['stroke', ['width', 0], ['type', 'default']], ['uuid', U()]])

    # ---- connectivity helpers ----------------------------------------
    def conn(self, part, pin, net):
        """connect a pin to a net: power symbol for GND/VDD where it reads well, else a label"""
        p = part.pin(pin)
        x, y = part.endpoint(p)
        a = p['angle']
        self.netmap[net].add((part.ref, p['number']))
        prev = self.points.get((x, y))
        if prev is not None:
            if prev != net:
                raise ValueError(f"short: {part.ref}.{pin} at {(x,y)} net {net} vs {prev}")
            return   # already connected at this point (stacked pins)
        self.points[(x, y)] = net
        # power symbols only where they read naturally (GND below, VDD above); labels on side pins
        # (KiCad merges a local label named GND/VDD into the power net - verified via netlist export)
        if net == 'GND' and a == 90:
            self.power_symbol('power:GND', x, y, 0)
            return
        if net == 'VDD' and a == 270:
            self.power_symbol('power:VDD', x, y, 0)
            return
        la = {0: 180, 180: 0, 270: 90, 90: 270}[a]
        self.label(net, x, y, la)

    def conn_group(self, part, pins, net, at_index=0):
        """Connect several pins on one side of a part to one net: wire segments between
        consecutive endpoints + a single power symbol / label. Refuses if a foreign pin
        endpoint lies between them."""
        ps = [part.pin(k) for k in pins]
        angs = {p['angle'] for p in ps}
        assert len(angs) == 1, 'pins must be on one side'
        pts = sorted({part.endpoint(p) for p in ps})
        xs = {pt[0] for pt in pts}; ys = {pt[1] for pt in pts}
        assert len(xs) == 1 or len(ys) == 1, 'pins not collinear'
        mine = {part.endpoint(p) for p in ps}
        for q in part.pins:
            e = part.endpoint(q)
            if e in mine:
                continue
            if len(ys) == 1 and e[1] == pts[0][1] and pts[0][0] < e[0] < pts[-1][0]:
                raise ValueError(f'foreign pin {part.ref}.{q["number"]} inside group')
            if len(xs) == 1 and e[0] == pts[0][0] and pts[0][1] < e[1] < pts[-1][1]:
                raise ValueError(f'foreign pin {part.ref}.{q["number"]} inside group')
        for a, b in zip(pts, pts[1:]):
            self.wire(a[0], a[1], b[0], b[1])
        for p in ps:
            self.netmap[net].add((part.ref, p['number']))
        for pt in pts:
            prev = self.points.get(pt)
            if prev not in (None, net):
                raise ValueError(f'short at {pt}: {net} vs {prev}')
            self.points[pt] = net
        # symbol / label at chosen endpoint
        x, y = pts[at_index]
        a = ps[0]['angle']
        if net == 'GND' and a == 90:
            self.power_symbol('power:GND', x, y, 0)
        elif net == 'VDD' and a == 270:
            self.power_symbol('power:VDD', x, y, 0)
        else:
            self.label(net, x, y, {0: 180, 180: 0, 270: 90, 90: 270}[a])

    def nc_pin(self, part, pin):
        p = part.pin(pin)
        x, y = part.endpoint(p)
        self.nc.add((part.ref, p['number']))
        if (x, y) in self.points:
            raise ValueError(f"NC on connected point {part.ref}.{pin}")
        self.points[(x, y)] = '__NC__'
        self.noconnect(x, y)

    def flag(self, net, x, y):
        """PWR_FLAG attached to a net through a short wire stub"""
        x, y = snap(x), snap(y)
        self.power_symbol('power:PWR_FLAG', x, y, 0)
        self.wire(x, y, x, y + 2.54)
        if net == 'GND':
            self.power_symbol('power:GND', x, y + 2.54, 0)
        elif net == 'VDD':
            self.power_symbol('power:VDD', x, y + 2.54, 180)
        else:
            self.label(net, x, y + 2.54, 270)
        self.netmap[net].add(('#FLG', str(self.flag_n)))

    # ---- output -------------------------------------------------------
    def save(self, path):
        tb = ['title_block', ['title', Q(self.title)], ['date', Q(datetime.date.today().isoformat())],
              ['rev', Q(self.rev)], ['company', Q(self.company)]]
        for i, c in enumerate(self.comments[:9], 1):
            tb.append(['comment', i, Q(c)])
        root = ['kicad_sch', ['version', 20230121], ['generator', 'eeschema'], ['uuid', self.uuid],
                ['paper', Q(self.paper)], tb, ['lib_symbols'] + list(self.libsyms.values())]
        root += self.items
        root.append(['sheet_instances', ['path', Q('/'), ['page', Q('1')]]])
        with open(path, 'w') as f:
            f.write(sexp.dump(root) + '\n')

```


## `gen/customlib.py`

```python
"""Generate the project symbol library f91w.kicad_sym."""
import sexp
from sexp import Q

PIN_LEN = 2.54
STEP = 2.54

def _font(): return ['effects', ['font', ['size', 1.27, 1.27]]]

def make_symbol(name, ref, value, footprint, datasheet, description,
                left=(), right=(), top=(), bottom=(), width=None, keywords='', min_height=0, hide_numbers=False):
    """pins: lists of (number, name, type) or None for a gap. Returns symbol s-expr."""
    import math
    def n(side): return len(side)
    def zone(side):
        names = [p[1] for p in side if p]
        if not names:
            return 0.0
        return math.ceil((max(len(x) for x in names) * 1.0 + 1.5) / 2.54) * 2.54
    rows = max(n(left), n(right), 1)
    cols = max(n(top), n(bottom), 0)
    longest = max([len(p[1]) for p in (*left, *right) if p] + [4])
    if width is None:
        width = max(2 * 2.54 * round((longest * 1.27 * 0.75 + 2.54) / 2.54), (cols + 1) * STEP, 10.16)
    width = round(width / 2.54) * 2.54
    tz, bz = zone(top), zone(bottom)
    height = max(tz + rows * STEP + bz + STEP, 2 * STEP, min_height)
    hw = round(width / 2 / 2.54) * 2.54
    hh = math.ceil(height / 2 / 2.54) * 2.54
    pins = []
    def pin(num, nm, typ, x, y, ang):
        pins.append(['pin', typ, 'line', ['at', x, y, ang], ['length', PIN_LEN],
                     ['name', Q(nm), _font()], ['number', Q(num), _font()]])
    y0 = hh - (tz if tz else 0) - STEP
    for i, p in enumerate(left):
        if p: pin(p[0], p[1], p[2], -hw - PIN_LEN, round(y0 - i * STEP, 2), 0)
    for i, p in enumerate(right):
        if p: pin(p[0], p[1], p[2], hw + PIN_LEN, round(y0 - i * STEP, 2), 180)
    x0 = -((n(top) - 1) * STEP) / 2
    x0 = round(x0 / 2.54) * 2.54
    for i, p in enumerate(top):
        if p: pin(p[0], p[1], p[2], round(x0 + i * STEP, 2), hh + PIN_LEN, 270)
    x0 = -((n(bottom) - 1) * STEP) / 2
    x0 = round(x0 / 2.54) * 2.54
    for i, p in enumerate(bottom):
        if p: pin(p[0], p[1], p[2], round(x0 + i * STEP, 2), -hh - PIN_LEN, 90)
    sym = ['symbol', Q(name)] + ([['pin_numbers', 'hide']] if hide_numbers else []) + [['in_bom', 'yes'], ['on_board', 'yes'],
           ['property', Q('Reference'), Q(ref), ['at', -hw, hh + 1.27, 0], ['effects', ['font', ['size', 1.27, 1.27]], ['justify', 'left']]],
           ['property', Q('Value'), Q(value), ['at', hw, hh + 1.27, 0], ['effects', ['font', ['size', 1.27, 1.27]], ['justify', 'right']]],
           ['property', Q('Footprint'), Q(footprint), ['at', 0, -hh - 5.08, 0], ['effects', ['font', ['size', 1.27, 1.27]], 'hide']],
           ['property', Q('Datasheet'), Q(datasheet), ['at', 0, 0, 0], ['effects', ['font', ['size', 1.27, 1.27]], 'hide']],
           ['property', Q('ki_keywords'), Q(keywords or name), ['at', 0, 0, 0], ['effects', ['font', ['size', 1.27, 1.27]], 'hide']],
           ['property', Q('ki_description'), Q(description), ['at', 0, 0, 0], ['effects', ['font', ['size', 1.27, 1.27]], 'hide']],
           ['symbol', Q(name + '_0_1'),
            ['rectangle', ['start', -hw, hh], ['end', hw, -hh], ['stroke', ['width', 0.254], ['type', 'default']], ['fill', ['type', 'background']]]],
           ['symbol', Q(name + '_1_1')] + pins]
    return sym

P, PI, PO, PW, NC, IN, OUT, OC = 'passive', 'power_in', 'power_out', 'power_in', 'no_connect', 'input', 'output', 'open_collector'
BI = 'bidirectional'

def build(path):
    syms = []
    # --- Casio F-91W module interface (LCD zebra pads, buttons, piezo spring, battery contacts)
    lcd_names = ['SEG23','SEG22','SEG21','SEG20','SEG19','SEG18','COM0','COM1','COM2'] + ['SEG%d' % i for i in range(17, -1, -1)]
    lcd = [(str(i + 1), 'p%d LCD_%s' % (i + 1, nm), P) for i, nm in enumerate(lcd_names)]
    syms.append(make_symbol('F91W_MODULE', 'M', 'F-91W module', 'f91w:CASIO-F91W-OUTLINE-USB',
        'https://github.com/joeycastillo/Sensor-Watch',
        'Casio F-91W module interface: 27 LCD zebra pads, 3 button contacts, piezo spring, battery contacts',
        left=lcd,
        right=[('VCC1', 'BAT+ (VCC1)', PO), ('VCC2', 'BAT+ (VCC2)', P), None, ('GND', 'BAT- (GND)', P), ('GND1', 'BAT- (GND1)', P), ('GND2', 'BAT- (GND2)', P), None,
               ('ALARM', 'ALARM btn', P), ('LIGHT', 'LIGHT btn', P), ('MODE', 'MODE btn', P), None, ('BUZZER', 'PIEZO spring', P)],
        width=30.48, hide_numbers=True))
    syms.append(make_symbol('LIS2DW12', 'U', 'LIS2DW12', 'f91w:LGA12R50P4X4_200X200X70',
        'https://www.st.com/resource/en/datasheet/lis2dw12.pdf', '3-axis ultra-low-power accelerometer, I2C/SPI, LGA-12 2x2mm',
        left=[('1', 'SCL/SPC', IN), ('4', 'SDA/SDI/SDO', BI)],
        right=[('12', 'INT1', OUT), ('11', 'INT2', OUT)],
        top=[('9', 'VDD', PW), ('10', 'VDD_IO', PW), ('2', 'CS', IN), ('3', 'SDO/SA0', BI)],
        bottom=[('5', 'NC', P), ('6', 'GND', PW), ('7', 'RES', P), ('8', 'GND', P)], width=20.32, min_height=17.78))
    syms.append(make_symbol('MLPF-WB-01E3', 'FL', 'MLPF-WB-01E3', 'f91w:ST_MLPF-WB-01E3_CSP-6',
        'https://www.st.com/resource/en/datasheet/mlpf-wb-01e3.pdf', 'ST IPD: STM32WB RF matching + harmonic filter, 50 ohm antenna side',
        left=[('A3', 'IN', P)], right=[('A1', 'OUT', P)],
        bottom=[('B3', 'GND1', P), ('B2', 'GND2', P), ('B1', 'GND3', P), ('A2', 'GND4', P)], width=15.24, min_height=12.7))
    syms.append(make_symbol('QBLP655-RGB', 'D', 'QBLP655-RGB', 'f91w:LED_QBLP655_RGB',
        'https://www.qt-brightek.com', 'RGB LED, common anode (backlight)',
        left=[('1', 'R_K', P), ('4', 'G_K', P), ('3', 'B_K', P)], top=[('2', 'A', P)], width=10.16))
    syms.append(make_symbol('MAXM86161', 'U', 'MAXM86161', 'f91w:MAXM86161_OLGA-14_TODO',
        'https://www.analog.com/media/en/technical-documentation/data-sheets/maxm86161.pdf',
        'Optical PPG module: green/red/IR LEDs + PD + AFE, I2C, single supply VLED 3.0-5.5V',
        left=[('1', 'SDA', BI), ('2', 'SCL', IN), ('14', 'INTB', OC), ('13', 'GPIO', BI), ('3', 'LDO_EN', IN)],
        right=[('4', 'VLDO', PO), ('12', 'VREF', P), None, ('6', 'NC_LED3', NC), ('7', 'NC_LED2', NC), ('8', 'NC_LED1', NC)],
        top=[('5', 'VLED', PW)], bottom=[('9', 'PGND', PW), ('10', 'GND_DIG', PW), ('11', 'GND_ANA', PW)], width=20.32))
    syms.append(make_symbol('FH19C-9S-0.5SH', 'J', 'FH19C-9S-0.5SH(10)', 'f91w:FH19C9S05SH10',
        'https://www.hirose.com/product/document?clcode=CL0580-1255-6-10&productname=FH19C-9S-0.5SH(10)&series=FH19C&documenttype=2DDrawing&lang=en&documentid=0001163317',
        '9-pin 0.5mm FPC connector (Sensor Watch sensor-board pinout)',
        left=[('1', '3V', P), ('2', 'GND', P), ('3', 'A4', P), ('4', 'A3', P), ('5', 'A2', P), ('6', 'A1', P), ('7', 'SDA', P), ('8', 'SCL', P), ('9', 'A0', P)],
        bottom=[('MP1', 'MP', P), ('MP2', 'MP', P)], width=10.16))
    syms.append(make_symbol('FPC_TAIL_9P', 'J', 'FPC tail 9P', 'f91w:FH19C9S05SH10-FFC',
        '', 'Flex-PCB tail with 9 gold fingers mating FH19C-9S-0.5SH (Sensor Watch sensor-board pinout)',
        right=[('1', '3V', P), ('2', 'GND', P), ('3', 'A4', P), ('4', 'A3', P), ('5', 'A2', P), ('6', 'A1', P), ('7', 'SDA', P), ('8', 'SCL', P), ('9', 'A0', P)],
        width=10.16))
    lib = ['kicad_symbol_lib', ['version', 20220914], ['generator', 'f91w_gen']] + syms
    with open(path, 'w') as f:
        f.write(sexp.dump(lib) + '\n')
    return [s[1] for s in syms]

if __name__ == '__main__':
    import sys
    print(build(sys.argv[1] if len(sys.argv) > 1 else 'f91w.kicad_sym'))

```


## `gen/footprints.py`

```python
"""Assemble lib/f91w.pretty: reused Sensor Watch footprints (CC-BY-SA 4.0) + generated ones."""
import os, shutil
# Path to a clone of https://github.com/joeycastillo/Sensor-Watch (only needed to refresh reused footprints)
SW = os.path.join(os.environ.get('SENSOR_WATCH_REPO', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'sw-hw')), 'PCB')
REUSE = {
    'CASIO-F91W-OUTLINE-USB.kicad_mod': SW + '/Main Boards/OSO-SWAT-C1/OSO-SWAT-C1.pretty/CASIO-F91W-OUTLINE-USB.kicad_mod',
    'FH19C9S05SH10.kicad_mod': SW + '/Main Boards/OSO-SWAT-C1/OSO-SWAT-C1.pretty/FH19C9S05SH10.kicad_mod',
    'ELT3KN.kicad_mod': SW + '/Main Boards/OSO-SWAT-C1/OSO-SWAT-C1.pretty/ELT3KN.kicad_mod',
    'LED_QBLP655_RGB.kicad_mod': SW + '/Main Boards/OSO-SWAT-C1/OSO-SWAT-C1.pretty/LED_QBLP655_RGB.kicad_mod',
    'FH19C9S05SH10-FFC.kicad_mod': SW + '/Sensor Boards/Kicad/OSO-SWAB-B1-00 Sensor Watch Accelerometer/OSO-SWAB-B1-00 Sensor Watch Accelerometer.pretty/FH19C9S05SH10-FFC.kicad_mod',
    'LGA12R50P4X4_200X200X70.kicad_mod': SW + '/Sensor Boards/Kicad/OSO-SWAB-B1-00 Sensor Watch Accelerometer/OSO-SWAB-B1-00 Sensor Watch Accelerometer.pretty/LGA12R50P4X4_200X200X70.kicad_mod',
}

MLPF = '''(footprint "ST_MLPF-WB-01E3_CSP-6" (version 20221018) (generator pcbnew)
  (layer "F.Cu")
  (descr "ST MLPF-WB-01E3 bumpless CSP, 6 pads, 0.5 mm (X) x 0.587 mm (Y) pitch, 0.2 mm pads. GENERATED FROM DATASHEET TEXT - VERIFY AGAINST ST LAND PATTERN BEFORE FAB")
  (tags "STM32WB IPD RF filter")
  (attr smd)
  (fp_text reference "REF**" (at 0 -1.5) (layer "F.SilkS") (effects (font (size 0.5 0.5) (thickness 0.1))))
  (fp_text value "MLPF-WB-01E3" (at 0 1.5) (layer "F.Fab") (effects (font (size 0.4 0.4) (thickness 0.08))))
  (fp_text user "VERIFY" (at 0 0 90) (layer "F.Fab") (effects (font (size 0.25 0.25) (thickness 0.05))))
  (fp_rect (start -0.5 -0.8) (end 0.5 0.8) (stroke (width 0.05) (type default)) (fill none) (layer "F.Fab"))
  (fp_rect (start -0.75 -1.05) (end 0.75 1.05) (stroke (width 0.05) (type default)) (fill none) (layer "F.CrtYd"))
  (fp_circle (center -0.75 -0.95) (end -0.7 -0.95) (stroke (width 0.1) (type default)) (fill solid) (layer "F.SilkS"))
  (pad "A1" smd circle (at -0.25 -0.587) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "B1" smd circle (at 0.25 -0.587) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "A2" smd circle (at -0.25 0) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "B2" smd circle (at 0.25 0) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "A3" smd circle (at -0.25 0.587) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "B3" smd circle (at 0.25 0.587) (size 0.2 0.2) (layers "F.Cu" "F.Paste" "F.Mask"))
)
'''

def build(pretty_dir):
    os.makedirs(pretty_dir, exist_ok=True)
    for name, src in REUSE.items():
        dst = os.path.join(pretty_dir, name)
        if os.path.exists(src):
            shutil.copy(src, dst)
        elif not os.path.exists(dst):
            print('WARNING: %s not found; set SENSOR_WATCH_REPO to a Sensor-Watch clone' % name)
    open(os.path.join(pretty_dir, 'ST_MLPF-WB-01E3_CSP-6.kicad_mod'), 'w').write(MLPF)
    open(os.path.join(pretty_dir, 'LICENSE_FOOTPRINTS.txt'), 'w').write(
        'CASIO-F91W-OUTLINE-USB, FH19C9S05SH10, ELT3KN, LED_QBLP655_RGB, FH19C9S05SH10-FFC and\n'
        'LGA12R50P4X4_200X200X70 are copied unmodified from the Sensor Watch project by Joey Castillo\n'
        '(https://github.com/joeycastillo/Sensor-Watch), licensed CC-BY-SA 4.0. Boards using them are\n'
        'derivative works and must be shared under CC-BY-SA 4.0 with attribution.\n'
        'ST_MLPF-WB-01E3_CSP-6 was generated from the MLPF-WB-01E3 datasheet text and MUST be checked\n'
        'against ST\'s recommended land pattern before fabrication.\n'
        'MAXM86161 (OLGA-14) has NO footprint yet: draw it from ADI land pattern 90-100106.\n')
    return sorted(os.listdir(pretty_dir))

if __name__ == '__main__':
    print(build('test/f91w.pretty'))

```


## `gen/main_board.py`

```python
"""F-91W BLE mainboard (rev A) — STM32WB55RGV6 + LIS2DW12 + stock F-91W LCD/buttons/piezo.
Generates f91w-ble-main.kicad_sch. Run: python3 main_board.py <outdir>"""
import sys, os, json
import sch, kicadlib as K

R0402 = 'Resistor_SMD:R_0402_1005Metric'
C0402 = 'Capacitor_SMD:C_0402_1005Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
L0402 = 'Inductor_SMD:L_0402_1005Metric'
L0805 = 'Inductor_SMD:L_0805_2012Metric'

# ---- MCU pin assignment (STM32WB55RGV6, VFQFPN68) ------------------------------------
# 24 LCD-capable pins -> glass SEG0..SEG23 (any LCD_SEGx pin can drive any glass line; the
# firmware segment map follows this table). COM0-2 are fixed to PA8/PA9/PA10.
LCD_SEG_PINS = [  # (MCU pin name, MCU LCD_SEG index)  -> glass SEG0, SEG1, ...
    ('PC0', 18), ('PC2', 20), ('PA1', 0), ('PA3', 2), ('PA4', 5), ('PA6', 3), ('PA7', 4),
    ('PC4', 22), ('PC5', 23), ('PB10', 10), ('PB11', 11), ('PB12', 12), ('PB13', 13),
    ('PB14', 14), ('PB15', 15), ('PC6', 24), ('PA15', 17), ('PC10', 28), ('PC11', 29),
    ('PC12', 30), ('PB4', 8), ('PB5', 9), ('PB6', 6), ('PB7', 21)]
MCU_NETS = {
    'PA8': 'LCD_COM0', 'PA9': 'LCD_COM1', 'PA10': 'LCD_COM2',
    'PC3': 'VLCD',
    'PB8': 'I2C_SCL', 'PB9': 'I2C_SDA',
    'PC13': 'BTN_ALARM', 'PA0': 'BTN_MODE', 'PB2': 'BTN_LIGHT',
    'PE4': 'ACC_INT1', 'PA5': 'ACC_INT2',
    'PB0': 'LED_R_N', 'PD0': 'LED_G_N', 'PD1': 'LED_B_N',
    'PA11': 'BUZZER',
    'PB1': 'A0', 'PA12': 'A1', 'PA2': 'A2', 'PB3': 'A3', 'PC1': 'A4',
    'PA13': 'SWDIO', 'PA14': 'SWCLK',
    'PC14': 'LSE_IN', 'PC15': 'LSE_OUT', 'PH3': 'BOOT0',
    'NRST': 'NRST', 'OSC_IN': 'HSE_IN', 'OSC_OUT': 'HSE_OUT', 'RF1': 'RF1',
    'VLXSMPS': 'SMPS_LX', 'VFBSMPS': 'VFBSMPS', 'VREF+': 'VDDA',
}
for i, (pin, segidx) in enumerate(LCD_SEG_PINS):
    MCU_NETS[pin] = 'LCD_SEG%d' % i

def build(outdir, libdir=None):
    K.register_lib('f91w', os.path.join(libdir or outdir, 'f91w.kicad_sym'))
    S = sch.Sch('f91w-ble-main', 'F-91W BLE mainboard (STM32WB55)', paper='A3', rev='A',
                company='f91w sensors project',
                comments=['Outline/pads derived from Sensor Watch Pro (CC-BY-SA 4.0)',
                          'Rev A first pass - not yet reviewed or laid out',
                          'CR2016 direct; SWD + BLE OTA (no USB)'])

    def cap(ref, val, x, y, top, bot='GND', fp=C0402, **kw):
        c = S.place('Device:C', ref, val, (x, y), fp, **kw); S.conn(c, 1, top); S.conn(c, 2, bot); return c
    def res(ref, val, x, y, top, bot, fp=R0402, **kw):
        r = S.place('Device:R', ref, val, (x, y), fp, **kw); S.conn(r, 1, top); S.conn(r, 2, bot); return r
    def ind(ref, val, x, y, top, bot, fp=L0402, **kw):
        l = S.place('Device:L', ref, val, (x, y), fp, **kw); S.conn(l, 1, top); S.conn(l, 2, bot); return l

    # ---------------- MCU ----------------
    S.text('MCU: STM32WB55RGV6 (BLE + LCD driver)', 106.68, 76.2, 1.8, bold=True)
    u1 = S.place('MCU_ST_STM32WB:STM32WB55RGVx', 'U1', 'STM32WB55RGV6', (139.7, 142.24),
                 fields={'MPN': 'STM32WB55RGV6', 'Manufacturer': 'STMicroelectronics'})
    used = set()
    S.conn_group(u1, ['1', '30', '45', '68'], 'VDD', at_index=0)      # VBAT + VDD x3
    S.conn_group(u1, ['33', '44', '55'], 'VDD', at_index=2)           # VDDRF, VDDSMPS, VDDUSB
    S.conn(u1, '14', 'VDDA')
    S.conn_group(u1, ['69', '32', '42'], 'GND', at_index=0)           # VSS(EP), VSSRF, VSSSMPS
    for p in u1.pins:
        nm, num = p['name'], p['number']
        if p['hidden']:
            continue  # AT0/AT1 factory-test pins (hidden no_connect)
        if nm in ('VBAT', 'VDD', 'VDDRF', 'VDDSMPS', 'VDDUSB', 'VDDA', 'VSS', 'VSSRF', 'VSSSMPS'):
            continue
        elif nm in MCU_NETS:
            S.conn(u1, num, MCU_NETS[nm])
        else:
            raise SystemExit('unassigned MCU pin %s %s' % (num, nm))
        used.add(nm)

    # ---------------- Decoupling row ----------------
    S.text('Supply decoupling: place each 100 nF at its pin. CR2016 feeds VDD directly; C15/C16 buffer BLE TX bursts.', 12.7, 22.86, 1.5, bold=True)
    x0, dx, yc = 15.24, 7.62, 38.1
    row = [('C1', '4.7u', 'VDD', 'bulk, U1 VDD'), ('C2', '100n', 'VDD', 'U1 pin 30'), ('C3', '100n', 'VDD', 'U1 pin 45'),
           ('C4', '100n', 'VDD', 'U1 pin 68'), ('C5', '100n', 'VDD', 'U1 VBAT pin 1'), ('C9', '100n', 'VDD', 'U1 VDDRF pin 33'),
           ('C10', '100p', 'VDD', 'U1 VDDRF pin 33'), ('C11', '4.7u', 'VDD', 'U1 VDDSMPS pin 44'), ('C12', '100n', 'VDD', 'U1 VDDSMPS pin 44'),
           ('C14', '100n', 'VDD', 'U1 VDDUSB pin 55'), ('C15', '22u', 'VDD', 'coin-cell buffer'), ('C16', '22u', 'VDD', 'coin-cell buffer')]
    for i, (ref, val, net, note) in enumerate(row):
        fp = C0603 if val == '22u' else C0402
        cap(ref, val, x0 + i * dx, yc, net, fp=fp, fields={'Placement': note})
    # VDDA filter
    xa = x0 + len(row) * dx + 7.62
    res('R1', '0R', xa, yc, 'VDD', 'VDDA', fields={'Notes': 'VDDA filter: 0R, or ferrite bead (e.g. BLM15AG) if ADC noise matters'})
    cap('C6', '100n', xa + dx, yc, 'VDDA', fields={'Placement': 'U1 VDDA pin 14'})
    cap('C7', '1u', xa + 2 * dx, yc, 'VDDA', fields={'Placement': 'U1 VDDA pin 14'})
    cap('C8', '100n', xa + 3 * dx, yc, 'VDDA', fields={'Placement': 'U1 VREF+ pin 13'})
    S.flag('VDD', 12.7, 55.88); S.flag('GND', 33.02, 55.88); S.flag('VDDA', 53.34, 55.88)

    # ---------------- SMPS ----------------
    S.text('SMPS (4 MHz mode): VLXSMPS -> 10 uH -> 10 nH -> VFBSMPS (ST AN5165)', 12.7, 68.58, 1.5, bold=True)
    ind('L1', '10u', 17.78, 82.55, 'SMPS_LX', 'SMPS_LX2', fp=L0805,
        fields={'MPN': 'LQM21FN100M70L', 'Manufacturer': 'Murata'})
    ind('L2', '10n', 30.48, 82.55, 'SMPS_LX2', 'VFBSMPS', fields={'MPN': 'LQG15HS10NJ02D', 'Manufacturer': 'Murata', 'Notes': 'RF-harmonic filter in series with L1'})
    S.flag('SMPS_LX', 58.42, 76.2)   # VLXSMPS is typed power_in in the KiCad symbol; it is the SMPS switch node
    cap('C13', '4.7u', 43.18, 82.55, 'VFBSMPS', fields={'MPN': 'GRM155R60J475KE19', 'Placement': 'at U1 VFBSMPS pin 41'})

    # ---------------- Clocks / reset / boot ----------------
    S.text('Clocks, reset, boot', 12.7, 99.06, 1.5, bold=True)
    y1 = S.place('Device:Crystal_GND24', 'Y1', '32MHz', (22.86, 113.03), 'Crystal:Crystal_SMD_2016-4Pin_2.0x1.6mm',
                 fields={'MPN': 'NX2016SA-32M-EXS00A-CS06654', 'Manufacturer': 'NDK', 'Notes': 'HSE: no external load caps (HSE trimming caps internal)'})
    S.conn(y1, 1, 'HSE_IN'); S.conn(y1, 3, 'HSE_OUT'); S.conn(y1, 2, 'GND'); S.conn(y1, 4, 'GND')
    y2 = S.place('Device:Crystal', 'Y2', '32.768kHz', (48.26, 113.03), 'Crystal:Crystal_SMD_2012-2Pin_2.0x1.2mm',
                 fields={'MPN': 'NX2012SA-32.768K (CL ~7 pF)', 'Manufacturer': 'NDK', 'Notes': 'LSE; check C18/C19 against chosen CL'})
    S.conn(y2, 1, 'LSE_IN'); S.conn(y2, 2, 'LSE_OUT')
    cap('C18', '10p', 63.5, 113.03, 'LSE_IN'); cap('C19', '10p', 71.12, 113.03, 'LSE_OUT')
    cap('C17', '100n', 17.78, 132.08, 'NRST', fields={'Placement': 'near U1 NRST pin 8'})
    res('R2', '10k', 30.48, 132.08, 'BOOT0', 'GND', fields={'Notes': 'BOOT0 pull-down (boot from flash)'})
    tp1 = S.place('Connector:TestPoint', 'TP1', 'BOOT0', (43.18, 129.54), 'TestPoint:TestPoint_Pad_D1.0mm', in_bom=False)
    S.conn(tp1, 1, 'BOOT0')

    # ---------------- RF ----------------
    S.text('RF: RF1 -> MLPF-WB-01E3 -> pi-match -> chip antenna', 12.7, 149.86, 1.5, bold=True)
    S.text('RF1->IPD trace 62 ohm; IPD->antenna 50 ohm.', 12.7, 153.67, 1.27)
    S.text('Tune C20/R3/C21 with a VNA inside the closed watch.', 12.7, 157.48, 1.27)
    fl1 = S.place('f91w:MLPF-WB-01E3', 'FL1', 'MLPF-WB-01E3', (25.4, 172.72), fields={'MPN': 'MLPF-WB-01E3', 'Manufacturer': 'STMicroelectronics'})
    S.conn(fl1, 'A3', 'RF1'); S.conn(fl1, 'A1', 'RF_50R')
    S.conn_group(fl1, ['B3', 'B2', 'B1', 'A2'], 'GND', at_index=0)
    cap('C20', 'DNP', 48.26, 172.72, 'RF_50R', dnp=True, fields={'Notes': 'pi-match shunt 1 (tune)'})
    res('R3', '0R', 58.42, 172.72, 'RF_50R', 'ANT_FEED', fields={'Notes': 'pi-match series (0R default; L or C after tuning)'})
    cap('C21', 'DNP', 68.58, 172.72, 'ANT_FEED', dnp=True, fields={'Notes': 'pi-match shunt 2 (tune)'})
    ae1 = S.place('Device:Antenna_Chip', 'AE1', '2450AT18B100', (83.82, 175.26), 'RF_Antenna:Johanson_2450AT18x100',
                  fields={'MPN': '2450AT18B100E', 'Manufacturer': 'Johanson Technology'})
    S.conn(ae1, 1, 'ANT_FEED'); S.nc_pin(ae1, 2)

    # ---------------- F-91W module interface ----------------
    S.text('F-91W module (LCD, buttons, piezo, battery)', 205.74, 76.2, 1.5, bold=True)
    m1 = S.place('f91w:F91W_MODULE', 'M1', 'Casio F-91W module', (238.76, 128.27), in_bom=False,
                 fields={'Notes': 'Footprint = Sensor Watch Pro outline (CC-BY-SA 4.0). Its USB pads are unused here: delete them in layout (good antenna spot).'})
    for p in m1.pins:
        nm, num = p['name'], p['number']
        if ' LCD_' in nm:                       # pin names are 'p<pad> LCD_<glass line>'
            S.conn(m1, num, nm.split(' ', 1)[1])
    S.conn_group(m1, ['VCC1', 'VCC2'], 'VDD')
    S.conn_group(m1, ['GND', 'GND1', 'GND2'], 'GND')
    S.conn(m1, 'ALARM', 'BTN_ALARM'); S.conn(m1, 'LIGHT', 'BTN_LIGHT'); S.conn(m1, 'MODE', 'BTN_MODE')
    S.conn(m1, 'BUZZER', 'BUZZER_HV')
    cap('C22', '1u', 279.4, 156.21, 'VLCD', fields={'Notes': 'LCD step-up (VLCD) reservoir on PC3/LCD_VLCD'})

    # ---------------- Piezo driver ----------------
    S.text('Piezo driver (as Sensor Watch Pro), PWM on PA11', 299.72, 165.1, 1.5, bold=True)
    q1 = S.place('Device:Q_NMOS_GSD', 'Q1', 'NMOS 30V LL', (317.5, 187.96), 'Package_TO_SOT_SMD:SOT-523',
                 fields={'Notes': 'Pick logic-level NMOS rated >=30 V (inductive kick from L3); verify Vds against measured spike'})
    S.conn(q1, 'G', 'BUZZER'); S.conn(q1, 'S', 'GND'); S.conn(q1, 'D', 'BUZZER_HV')
    ind('L3', '14mH', 353.06, 182.88, 'VDD', 'BUZZER_HV', fp='f91w:ELT3KN', fields={'MPN': 'ELT-3KN004B', 'Manufacturer': 'Panasonic'})
    res('R4', '10k', 302.26, 193.04, 'BUZZER', 'GND', fields={'Notes': 'gate pull-down'})

    # ---------------- Backlight LED ----------------
    S.text('Backlight RGB LED (active-low)', 299.72, 124.46, 1.5, bold=True)
    d1 = S.place('f91w:QBLP655-RGB', 'D1', 'QBLP655-RGB', (350.52, 143.51), fields={'MPN': 'QBLP655-RGB', 'Manufacturer': 'QT Brightek'})
    S.conn(d1, 'A', 'VDD'); S.conn(d1, '1', 'LED_RK'); S.conn(d1, '4', 'LED_GK'); S.conn(d1, '3', 'LED_BK')
    res('R5', '100R', 307.34, 143.51, 'LED_R_N', 'LED_RK')
    res('R6', '1k', 317.5, 143.51, 'LED_G_N', 'LED_GK')
    res('R7', '100R', 327.66, 143.51, 'LED_B_N', 'LED_BK')

    # ---------------- Accelerometer ----------------
    S.text('Accelerometer LIS2DW12 (I2C 0x19, steps)', 299.72, 25.4, 1.5, bold=True)
    u2 = S.place('f91w:LIS2DW12', 'U2', 'LIS2DW12TR', (330.2, 50.8), fields={'MPN': 'LIS2DW12TR', 'Manufacturer': 'STMicroelectronics'})
    S.conn(u2, '1', 'I2C_SCL'); S.conn(u2, '4', 'I2C_SDA')
    S.conn_group(u2, ['9', '10', '2', '3'], 'VDD', at_index=0)    # VDD, VDD_IO, CS high (I2C), SA0 high (0x19)
    S.conn_group(u2, ['5', '6', '7', '8'], 'GND', at_index=1)     # NC (tied low as on Sensor Watch), GND, RES, GND
    S.conn(u2, '12', 'ACC_INT1'); S.conn(u2, '11', 'ACC_INT2')
    cap('C23', '100n', 360.68, 50.8, 'VDD', fields={'Placement': 'U2 VDD'})
    cap('C24', '10u', 368.3, 50.8, 'VDD', fp=C0603, fields={'Placement': 'U2 VDD'})
    res('R8', '10k', 304.8, 76.2, 'VDD', 'I2C_SCL', fields={'Notes': 'I2C pull-up (also serves PPG flex)'})
    res('R9', '10k', 314.96, 76.2, 'VDD', 'I2C_SDA', fields={'Notes': 'I2C pull-up (also serves PPG flex)'})

    # ---------------- Sensor connector (PPG caseback flex) ----------------
    S.text('J1: PPG flex (A0 = BOOST_EN, A1 = PPG_INT)', 299.72, 99.06, 1.5, bold=True)
    j1 = S.place('f91w:FH19C-9S-0.5SH', 'J1', 'FH19C-9S-0.5SH(10)', (386.08, 76.2), fields={'MPN': 'FH19C-9S-0.5SH(10)', 'Manufacturer': 'Hirose'})
    for num, net in (('1', 'VDD'), ('2', 'GND'), ('3', 'A4'), ('4', 'A3'), ('5', 'A2'), ('6', 'A1'), ('7', 'I2C_SDA'), ('8', 'I2C_SCL'), ('9', 'A0')):
        S.conn(j1, num, net)
    S.nc_pin(j1, 'MP1'); S.nc_pin(j1, 'MP2')

    # ---------------- Debug ----------------
    S.text('SWD (Tag-Connect TC2030-NL, Cortex 6-pin)', 299.72, 213.36, 1.5, bold=True)
    j2 = S.place('Connector_Generic:Conn_02x03_Odd_Even', 'J2', 'TC2030-SWD', (322.58, 229.87),
                 'Connector:Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical', in_bom=False)
    for num, net in (('1', 'VDD'), ('2', 'SWDIO'), ('3', 'NRST'), ('4', 'SWCLK'), ('5', 'GND'), ('6', 'A3')):
        S.conn(j2, num, net)
    S.text('pin 6 = SWO (PB3, shared with A3)', 335.28, 241.3, 1.27)

    # ---------------- Notes ----------------
    notes = [
        'NOTES',
        '1. Buttons are active-high: the case springs tie the pads to BAT+. Use internal pull-downs + EXTI rising edge (wakes from Stop2).',
        '2. LCD: 1/3 duty, 1/3 bias. COM0-2 = PA8/PA9/PA10. Glass SEGn -> MCU LCD_SEG mapping is in DESIGN_NOTES.md (re-assign freely during layout).',
        '3. VDDUSB tied to VDD: required because PA11/PA12 (piezo, A1) are supplied from VDDUSB.',
        '4. EXTI lines used: 0 MODE, 2 LIGHT, 4 ACC_INT1, 5 ACC_INT2, 12 A1/PPG_INT, 13 ALARM. A0 (PB1) and A4 (PC1) share EXTI1.',
        '5. No USB on this rev: flash/debug through J2 (ST-Link + TC2030-NL cable); field updates via BLE OTA.',
        '6. Keep a solid ground plane on layer 2; RF section and HSE crystal on top layer next to U1 (AN5165).',
    ]
    for i, t in enumerate(notes):
        S.text(t, 12.7, 200.66 + i * 4.445, 1.5 if i == 0 else 1.27, bold=(i == 0))

    path = os.path.join(outdir, 'f91w-ble-main.kicad_sch')
    S.save(path)
    return S

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'out'
    os.makedirs(out, exist_ok=True)
    import customlib
    customlib.build(os.path.join(out, 'f91w.kicad_sym'))
    S = build(out)
    d = {k: sorted(map(list, v)) for k, v in S.netmap.items()}
    d['__NC__'] = sorted(map(list, S.nc))
    json.dump(d, open(os.path.join(out, 'main.intended.json'), 'w'), indent=1)
    print('parts:', len(S.parts), 'nets:', len(S.netmap))

```


## `gen/ppg_flex.py`

```python
"""F-91W PPG caseback flex (rev A): MAXM86161 + TPS610995 3.6 V boost, plugs into the 9-pin sensor connector."""
import sys, os, json
import sch, kicadlib as K

R0402 = 'Resistor_SMD:R_0402_1005Metric'
C0402 = 'Capacitor_SMD:C_0402_1005Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'

def build(outdir, libdir):
    K.register_lib('f91w', os.path.join(libdir, 'f91w.kicad_sym'))
    S = sch.Sch('f91w-ppg-flex', 'F-91W PPG caseback flex', paper='A4', rev='A',
                company='f91w sensors project',
                comments=['Plugs into J1 of the mainboard (Sensor Watch 9-pin pinout)',
                          'Rev A first pass - not yet reviewed or laid out',
                          'MAXM86161 footprint TODO (ADI land pattern 90-100106)'])
    def cap(ref, val, x, y, top, bot='GND', fp=C0402, **kw):
        c = S.place('Device:C', ref, val, (x, y), fp, **kw); S.conn(c, 1, top); S.conn(c, 2, bot); return c
    def res(ref, val, x, y, top, bot, fp=R0402, **kw):
        r = S.place('Device:R', ref, val, (x, y), fp, **kw); S.conn(r, 1, top); S.conn(r, 2, bot); return r

    # ---- connector tail ----
    S.text('Flex tail -> mainboard J1', 12.7, 30.48, 1.5, bold=True)
    j1 = S.place('f91w:FPC_TAIL_9P', 'J1', 'FPC tail 9P', (25.4, 55.88), in_bom=False, fields={'Notes': '0.5 mm pitch gold fingers, 0.3 mm total thickness with stiffener'})
    for num, net in (('1', 'VDD'), ('2', 'GND'), ('6', 'PPG_INT'), ('7', 'I2C_SDA'), ('8', 'I2C_SCL'), ('9', 'BOOST_EN')):
        S.conn(j1, num, net)
    for num in ('3', '4', '5'):
        S.nc_pin(j1, num)
    S.text('A4/A3/A2 unused on this flex', 12.7, 76.2, 1.27)

    # ---- input isolation + boost ----
    S.text('LED rail: coin cell -> RC isolation -> TPS610995 (3.6 V fixed, true shutdown)', 63.5, 30.48, 1.5, bold=True)
    res('R1', '47R', 68.58, 48.26, 'VDD', 'VIN_B', fields={'Notes': 'caps boost inrush drawn from the CR2016 (<=64 mA worst case); tune 22-100R by measuring VDD dip at BOOST_EN'})
    cap('C1', '22u', 78.74, 48.26, 'VIN_B', fp=C0603, fields={'Notes': 'reservoir; low-leakage X5R'})
    cap('C2', '10u', 86.36, 48.26, 'VIN_B', fields={'Placement': 'at U1 VI'})
    l1 = S.place('Device:L', 'L1', '2.2u', (96.52, 48.26), 'Inductor_SMD:L_Murata_DFE201610P',
                 fields={'MPN': 'DFE201610E-2R2M=P2', 'Manufacturer': 'Murata', 'Notes': 'Isat >= 1 A (boost current limit 0.8 A min)'})
    S.conn(l1, 1, 'VIN_B'); S.conn(l1, 2, 'SW')
    u1 = S.place('Regulator_Switching:TPS610995DRV', 'U1', 'TPS610995DRV', (127.0, 50.8),
                 fields={'MPN': 'TPS610995DRVR', 'Manufacturer': 'Texas Instruments'})
    S.conn(u1, '6', 'VIN_B'); S.conn(u1, '5', 'SW'); S.conn(u1, '2', 'VLED'); S.conn(u1, '3', 'GND')
    S.conn(u1, '4', 'BOOST_EN'); S.conn(u1, '1', 'GND'); S.conn(u1, '7', 'GND')
    res('R2', '1M', 109.22, 71.12, 'BOOST_EN', 'GND', fields={'Notes': 'keeps boost off while MCU pin is Hi-Z'})
    cap('C3', '22u', 152.4, 48.26, 'VLED', fp=C0603, fields={'Notes': 'boost output >= 20 uF nominal (TI)'})
    cap('C4', '22u', 160.02, 48.26, 'VLED', fp=C0603)
    tp1 = S.place('Connector:TestPoint', 'TP1', 'VLED', (170.18, 43.18), 'TestPoint:TestPoint_Pad_1.0x1.0mm', in_bom=False)
    S.conn(tp1, 1, 'VLED')
    S.flag('VDD', 20.32, 93.98); S.flag('GND', 35.56, 93.98); S.flag('VIN_B', 50.8, 93.98); S.flag('SW', 66.04, 93.98)

    # ---- PPG sensor ----
    S.text('PPG sensor (sits in the caseback window, LEDs/PD facing skin)', 63.5, 96.52, 1.5, bold=True)
    u2 = S.place('f91w:MAXM86161', 'U2', 'MAXM86161EFD+', (127.0, 132.08),
                 fields={'MPN': 'MAXM86161EFD+', 'Manufacturer': 'Analog Devices', 'LCSC': 'C3178309'})
    S.conn(u2, '1', 'I2C_SDA'); S.conn(u2, '2', 'I2C_SCL'); S.conn(u2, '14', 'PPG_INT')
    S.nc_pin(u2, '13')                       # GPIO unused
    S.conn(u2, '3', 'LDO_EN')
    S.conn(u2, '5', 'VLED')
    S.conn(u2, '4', 'VLDO'); S.conn(u2, '12', 'VREF')
    for num in ('6', '7', '8'):
        S.nc_pin(u2, num)                    # internally tied to LED cathodes: isolated pads only
    S.conn_group(u2, ['9', '10', '11'], 'GND', at_index=0)
    res('R4', '0R', 88.9, 132.08, 'VLED', 'LDO_EN', fields={'Notes': 'LDO on whenever VLED is up; move to a GPIO if needed'})
    res('R3', '100k', 78.74, 132.08, 'VDD', 'PPG_INT', fields={'Notes': 'INTB pull-up (or use MCU internal pull-up and DNP)'})
    cap('C5', '10u', 165.1, 119.38, 'VLED', fields={'Placement': 'at U2 VLED pin 5 (datasheet: 10 uF to PGND)'})
    cap('C6', '100n', 172.72, 119.38, 'VLED', fields={'Placement': 'at U2 VLED pin 5'})
    cap('C7', '1u', 165.1, 144.78, 'VLDO', fields={'Placement': 'at U2 VLDO pin 4 (to GND_ANA)'})
    cap('C8', '1u', 172.72, 144.78, 'VREF', fields={'Placement': 'at U2 VREF pin 12 (to GND_ANA)'})

    notes = ['NOTES',
             '1. Power: boost is OFF (true disconnect, ~0 uA) unless BOOST_EN (mainboard A0/PB1) is high. Spot-check HR: enable, wait ~5 ms, configure over I2C.',
             '2. MAXM86161 I2C/INTB pins are 6 V tolerant to GND (no clamp to VLED), so the shared 3 V bus is safe while U2 is unpowered.',
             '3. Optics: two apertures in the caseback (LED / photodiode) separated by opaque material; sensor face flush with the outer surface.',
             '4. Layout: U2 on a stiffener in the caseback window; boost section near the tail; service loop so the caseback can be removed.',
             '5. Pins 6/7/8 of U2 connect internally to LED cathodes: solder to isolated pads, never to GND or other nets.']
    for i, t in enumerate(notes):
        S.text(t, 12.7, 165.1 + i * 4.445, 1.5 if i == 0 else 1.27, bold=(i == 0))
    path = os.path.join(outdir, 'f91w-ppg-flex.kicad_sch')
    S.save(path)
    return S

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'out'
    os.makedirs(out, exist_ok=True)
    S = build(out, out)
    d = {k: sorted(map(list, v)) for k, v in S.netmap.items()}
    d['__NC__'] = sorted(map(list, S.nc))
    json.dump(d, open(os.path.join(out, 'flex.intended.json'), 'w'), indent=1)
    print('parts:', len(S.parts), 'nets:', len(S.netmap))

```


## `gen/verify.py`

```python
"""Verify a generated schematic using KiCad's own connectivity (kicad-cli netlist export).
Checks: (1) KiCad nets == intended nets, (2) every non-hidden symbol pin is connected or NC-flagged,
(3) no single-node nets, (4) basic pin-type sanity (power_in pins have a source on the net,
no two outputs on one net), (5) footprints assigned."""
import json, subprocess, sys, os, collections
import sexp
from sexp import find, findall

def kicad_netlist(sch_path, tmp):
    subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '-o', tmp, sch_path], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t = sexp.parse(open(tmp).read())
    comps = {}
    for c in findall(find(t, 'components'), 'comp'):
        ref = str(find(c, 'ref')[1]); val = str(find(c, 'value')[1])
        fp = find(c, 'footprint'); comps[ref] = dict(value=val, footprint=str(fp[1]) if fp else '')
    nets = {}
    for n in findall(find(t, 'nets'), 'net'):
        name = str(find(n, 'name')[1])
        nodes = []
        for nd in findall(n, 'node'):
            ref = str(find(nd, 'ref')[1]); pin = str(find(nd, 'pin')[1])
            pt = find(nd, 'pintype'); pf = find(nd, 'pinfunction')
            nodes.append((ref, pin, str(pt[1]) if pt else '', str(pf[1]) if pf else ''))
        nets[name] = nodes
    return comps, nets

def check(sch_path, intended_json, Sobj=None):
    tmp = sch_path + '.verify.net'
    comps, nets = kicad_netlist(sch_path, tmp)
    os.remove(tmp)
    intended = json.load(open(intended_json))
    nc_set = {tuple(x) for x in intended.pop('__NC__', [])}
    errors, warnings = [], []
    # normalise names: '/NET' -> 'NET'
    norm = {}
    for name, nodes in nets.items():
        key = name[1:] if name.startswith('/') else name
        norm[key] = {(r, p) for (r, p, t, f) in nodes}
    want = {k: {tuple(x) for x in v if not x[0].startswith('#')} for k, v in intended.items()}
    # 1) compare
    for net, nodes in want.items():
        got = norm.get(net)
        if got is None:
            errors.append(f'net {net} missing in KiCad netlist (wanted {sorted(nodes)})')
        elif got != nodes:
            errors.append(f'net {net} mismatch: kicad-only {sorted(got - nodes)} intended-only {sorted(nodes - got)}')
    for net in norm:
        if net not in want and not net.startswith('unconnected-') and not net.startswith('Net-('):
            errors.append(f'unexpected KiCad net {net}: {sorted(norm[net])}')
    # 2) unconnected pins: KiCad names them unconnected-(...)
    unconn = [n for n in nets if n.startswith('unconnected-')]
    nc_expected = set()
    if Sobj is not None:
        nc_expected = {f'{r}-Pad{p}' for (r, p) in Sobj.nc}
    for n in unconn:
        nodes = nets[n]
        r, p = nodes[0][0], nodes[0][1]
        if (r, p) in nc_set:
            continue
        # KiCad netlists also list no_connect pins as unconnected-(...); hidden NC pins too
        if nodes[0][2] == 'no_connect':
            continue
        errors.append(f'unconnected pin {r}.{p} ({nodes[0][3]})')
    # 3) single-node nets (excluding test points)
    for net, nodes in nets.items():
        if net.startswith('unconnected-'):
            continue
        if len(nodes) < 2 and not nodes[0][0].startswith('TP'):
            errors.append(f'single-node net {net}: {nodes}')
    # 4) pin-type sanity
    for net, nodes in nets.items():
        types = [t for (_, _, t, _) in nodes]
        outs = [n for n in nodes if n[2] == 'output']
        if len(outs) > 1:
            errors.append(f'net {net}: multiple outputs {outs}')
        if any(t == 'power_in' for t in types):
            key = net[1:] if net.startswith('/') else net
            has_src = any(t in ('power_out',) for t in types) or any(x[0] == '#FLG' for x in intended.get(key, []))
            if not has_src:
                warnings.append(f'net {net}: power_in pins without power_out/PWR_FLAG source: {[(r,p,f) for r,p,t,f in nodes if t=="power_in"]}')
    # 5) footprints
    for ref, c in comps.items():
        if not c['footprint']:
            errors.append(f'{ref} has no footprint')
    return comps, nets, errors, warnings

if __name__ == '__main__':
    comps, nets, errors, warnings = check(sys.argv[1], sys.argv[2])
    print(f'{len(comps)} components, {len(nets)} nets')
    for w in warnings: print('WARN', w)
    for e in errors: print('ERR ', e)
    print('RESULT:', 'PASS' if not errors else 'FAIL (%d)' % len(errors))

def pin_pad_check(sch_path, project_pretty, std_fp_dir='/usr/share/kicad/footprints'):
    """every symbol pin must have a pad of the same number in its footprint (TODO footprints skipped)"""
    import re
    import kicadlib as K
    t = sexp.parse(open(sch_path).read())
    libsyms = {str(s[1]): s for s in findall(find(t, 'lib_symbols'), 'symbol')}
    problems, checked, skipped = [], 0, []
    for sym in findall(t, 'symbol'):
        lid = str(find(sym, 'lib_id')[1])
        if lid.startswith('power:'):
            continue
        props = {str(p[1]): str(p[2]) for p in findall(sym, 'property')}
        fp = props.get('Footprint', '')
        if not fp or 'TODO' in fp:
            skipped.append(props.get('Reference', '?')); continue
        lib, name = fp.split(':')
        path = os.path.join(project_pretty, name + '.kicad_mod') if lib == 'f91w' else os.path.join(std_fp_dir, lib + '.pretty', name + '.kicad_mod')
        if not os.path.exists(path):
            problems.append(f'{props["Reference"]}: footprint file missing {fp}'); continue
        pads = set(m.strip('"') for m in re.findall(r'\(pad ("[^"]*"|[^\s()]+)', open(path).read()))
        pins = {p['number'] for p in K.pins(libsyms[lid]) if not p['hidden']}
        checked += 1
        if pins - pads:
            problems.append(f'{props["Reference"]} {fp}: pins without pads {sorted(pins - pads)}')
    return checked, skipped, problems

```


## `gen/build_all.py`

```python
"""Build the complete f91w-hw package: libraries, both schematics, verification, PDFs, BOMs, pin map."""
import os, sys, json, shutil, subprocess, csv, collections, datetime
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import customlib, footprints, main_board, ppg_flex, verify, sch

def write_tables(projdir):
    open(os.path.join(projdir, 'sym-lib-table'), 'w').write(
        '(sym_lib_table\n  (lib (name "f91w")(type "KiCad")(uri "${KIPRJMOD}/../lib/f91w.kicad_sym")(options "")(descr "F-91W project symbols"))\n)\n')
    open(os.path.join(projdir, 'fp-lib-table'), 'w').write(
        '(fp_lib_table\n  (lib (name "f91w")(type "KiCad")(uri "${KIPRJMOD}/../lib/f91w.pretty")(options "")(descr "F-91W project footprints (KiCad 8+)"))\n)\n')

def write_pro(projdir, name, root_uuid):
    pro = {"meta": {"filename": name + ".kicad_pro", "version": 1},
           "boards": [], "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
           "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
           "sheets": [[root_uuid, "Root"]], "text_variables": {}}
    json.dump(pro, open(os.path.join(projdir, name + '.kicad_pro'), 'w'), indent=2)

def bom(S, path):
    groups = collections.OrderedDict()
    for p in S.parts:
        if not p.in_bom:
            continue   # board features, not purchased parts (outline pads, flex tail, test pads, Tag-Connect)
        key = (p.value, p.fp, p.fields.get('MPN', ''), p.fields.get('Manufacturer', ''), p.dnp)
        groups.setdefault(key, []).append(p.ref)
    def refkey(r):
        import re
        m = re.match(r'([A-Z]+)(\d+)', r); return (m.group(1), int(m.group(2))) if m else (r, 0)
    rows = sorted(groups.items(), key=lambda kv: refkey(sorted(kv[1], key=refkey)[0]))
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['References', 'Qty', 'Value', 'Footprint', 'MPN', 'Manufacturer', 'DNP'])
        for (val, fp, mpn, mfr, dnp), refs in rows:
            refs = sorted(refs, key=refkey)
            w.writerow([' '.join(refs), len(refs), val, fp, mpn, mfr, 'DNP' if dnp else ''])
    return len(rows)

def firmware_header(path):
    """board header generated from main_board.MCU_NETS / LCD_SEG_PINS so firmware and schematic stay in sync"""
    import re
    lines = ['/* Generated by gen/build_all.py from the mainboard schematic data - do not edit by hand. */',
             '#ifndef BOARD_F91W_BLE_H', '#define BOARD_F91W_BLE_H', '',
             '/* MCU: STM32WB55RGV6 (VFQFPN68). Pin = (GPIO port, pin number). */']
    def port_pin(name):
        m = re.match(r'P([A-H])(\d+)$', name)
        return (m.group(1), int(m.group(2))) if m else None
    order = sorted(main_board.MCU_NETS.items(), key=lambda kv: kv[1])
    for mcu, net in order:
        pp = port_pin(mcu)
        if not pp or net.startswith('LCD_'):
            continue
        lines.append('#define PIN_%-12s GPIO%s, %-2d   /* %s */' % (net, pp[0], pp[1], mcu))
    lines += ['', '/* LCD: 1/3 duty, 1/3 bias. COM0..2 = PA8, PA9, PA10 (LCD_COM0..2).',
              ' * Glass line LCD_SEGn is the same physical zebra pad Sensor Watch calls SEGn, so Movement segment',
              ' * maps (com, seg) can be reused by translating seg through this table. */',
              '#define LCD_NUM_GLASS_SEGS 24',
              'static const unsigned char LCD_GLASS_SEG_TO_MCU_SEG[LCD_NUM_GLASS_SEGS] = {']
    for i, (pin, idx) in enumerate(main_board.LCD_SEG_PINS):
        lines.append('    %2d, /* glass SEG%-2d <- %s (LCD_SEG%d) */' % (idx, i, pin, idx))
    lines += ['};', '', '/* I2C1 on PB8 (SCL) / PB9 (SDA), 10k pull-ups to VDD. */',
              '#define I2C_ADDR_LIS2DW12   0x19',
              '#define I2C_ADDR_MAXM86161  0x62   /* on the PPG flex (8-bit 0xC4/0xC5) */', '',
              '/* EXTI lines: 0 BTN_MODE, 2 BTN_LIGHT, 4 ACC_INT1, 5 ACC_INT2, 12 A1/PPG_INT, 13 BTN_ALARM.',
              ' * Buttons are active-high (pressed = BAT+): input + pull-down + rising-edge EXTI. */',
              '#define PPG_BOOST_EN   PIN_A0    /* flex: TPS610995 EN (1M pull-down on flex) */',
              '#define PPG_INT        PIN_A1    /* flex: MAXM86161 INTB (open drain, 100k pull-up on flex) */',
              '#define BUZZER_TIMER   TIM1      /* PA11 = TIM1_CH4 */',
              '', '#endif', '']
    open(path, 'w').write('\n'.join(lines))

PIN_FUNC = {
    'LCD_COM': 'LCD common (fixed: LCD_COM0-2)', 'LCD_SEG': 'LCD segment', 'VLCD': 'LCD_VLCD step-up cap (1 uF)',
    'I2C_SCL': 'I2C1_SCL', 'I2C_SDA': 'I2C1_SDA', 'BTN_ALARM': 'button, EXTI13, WKUP2', 'BTN_MODE': 'button, EXTI0, WKUP1',
    'BTN_LIGHT': 'button, EXTI2', 'ACC_INT1': 'LIS2DW12 INT1, EXTI4', 'ACC_INT2': 'LIS2DW12 INT2, EXTI5',
    'LED_R_N': 'backlight red (active low)', 'LED_G_N': 'backlight green (active low)', 'LED_B_N': 'backlight blue (active low)',
    'BUZZER': 'piezo PWM, TIM1_CH4', 'A0': 'J1 A0 -> PPG BOOST_EN', 'A1': 'J1 A1 -> PPG_INT, EXTI12, LPUART1_RX',
    'A2': 'J1 A2 spare, ADC1_IN7, LPUART1_TX', 'A3': 'J1 A3 spare, EXTI3, SWO', 'A4': 'J1 A4 spare, ADC1_IN2',
    'SWDIO': 'SWD', 'SWCLK': 'SWD', 'LSE_IN': '32.768 kHz crystal', 'LSE_OUT': '32.768 kHz crystal', 'BOOT0': '10k pull-down + test pad',
    'NRST': '100 nF + SWD', 'HSE_IN': '32 MHz crystal', 'HSE_OUT': '32 MHz crystal', 'RF1': 'RF -> MLPF-WB-01E3',
    'SMPS_LX': 'SMPS switch -> 10 uH', 'VFBSMPS': 'SMPS output, 4.7 uF', 'VDDA': 'VREF+ tied to VDDA'}

def readme(path):
    import re
    rows = []
    for mcu, net in main_board.MCU_NETS.items():
        key = 'LCD_SEG' if net.startswith('LCD_SEG') else ('LCD_COM' if net.startswith('LCD_COM') else net)
        func = PIN_FUNC.get(key, '')
        if net.startswith('LCD_SEG'):
            idx = dict(main_board.LCD_SEG_PINS)[mcu]
            func = 'glass %s via LCD_SEG%d' % (net[4:], idx)
        rows.append((mcu, net, func))
    def k(r):
        m = re.match(r'P([A-H])(\d+)$', r[0]); return (0, m.group(1), int(m.group(2))) if m else (1, r[0], 0)
    table = '\n'.join('| %s | %s | %s |' % r for r in sorted(rows, key=k))
    text = README_TEMPLATE.replace('@@PINTABLE@@', table)
    open(path, 'w').write(text)

README_TEMPLATE = open(os.path.join(HERE, 'README_TEMPLATE.md')).read() if os.path.exists(os.path.join(HERE, 'README_TEMPLATE.md')) else '@@PINTABLE@@'

def main(root):
    root = os.path.abspath(root)
    lib = os.path.join(root, 'lib'); mb = os.path.join(root, 'mainboard'); fx = os.path.join(root, 'ppg-flex')
    docs = os.path.join(root, 'docs'); gen = os.path.join(root, 'gen'); fw = os.path.join(root, 'firmware')
    for d in (mb, fx, docs):                      # generated outputs only; never touches gen/ or lib footprints
        if os.path.exists(d):
            shutil.rmtree(d)
    for d in (lib, mb, fx, docs, gen, fw):
        os.makedirs(d, exist_ok=True)
    firmware_header(os.path.join(fw, 'board_f91w_ble.h'))
    hrdir = os.path.join(fw, 'hr'); os.makedirs(hrdir, exist_ok=True)
    for src_dir in (os.path.join(HERE, '..', 'fw'), os.path.join(HERE, '..', 'firmware', 'hr')):
        if os.path.exists(os.path.join(src_dir, 'hr_spotcheck.c')):
            for f in ('hr_spotcheck.c', 'hr_spotcheck.h', 'test_hr.c', 'Makefile'):
                if os.path.abspath(os.path.join(src_dir, f)) != os.path.abspath(os.path.join(hrdir, f)):
                    shutil.copy(os.path.join(src_dir, f), hrdir)
            break
    customlib.build(os.path.join(lib, 'f91w.kicad_sym'))
    footprints.build(os.path.join(lib, 'f91w.pretty'))
    report = []
    boards = []
    for name, mod, projdir in (('f91w-ble-main', main_board, mb), ('f91w-ppg-flex', ppg_flex, fx)):
        S = mod.build(projdir, lib)
        write_tables(projdir); write_pro(projdir, name, S.uuid)
        d = {k: sorted(map(list, v)) for k, v in S.netmap.items()}; d['__NC__'] = sorted(map(list, S.nc))
        ij = os.path.join(docs, name + '.intended-netlist.json'); json.dump(d, open(ij, 'w'), indent=1)
        schp = os.path.join(projdir, name + '.kicad_sch')
        comps, nets, errors, warnings = verify.check(schp, ij)
        report.append(f'== {name}: {len(comps)} components, {len(nets)} nets -> {"PASS" if not errors else "FAIL"}')
        report += ['  WARN ' + w for w in warnings] + ['  ERR  ' + e for e in errors]
        subprocess.run(['kicad-cli', 'sch', 'export', 'pdf', '-o', os.path.join(docs, name + '.pdf'), schp], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '-o', os.path.join(docs, name + '.net'), schp], check=True, stdout=subprocess.DEVNULL)
        chk, skipped, probs = verify.pin_pad_check(schp, os.path.join(lib, 'f91w.pretty'))
        report.append(f'  pin/pad check: {chk} parts OK' if not probs else f'  pin/pad check: {len(probs)} problems')
        report += ['  ERR  ' + p for p in probs]
        if skipped:
            report.append('  footprint TODO (not checked): ' + ', '.join(skipped))
        nbom = bom(S, os.path.join(docs, name + '-BOM.csv'))
        report.append(f'  BOM lines: {nbom}')
        boards.append((name, S, nets))
    for f in ('sexp.py', 'kicadlib.py', 'sch.py', 'customlib.py', 'footprints.py', 'main_board.py', 'ppg_flex.py', 'verify.py', 'build_all.py', 'README_TEMPLATE.md'):
        if os.path.abspath(os.path.join(HERE, f)) != os.path.abspath(os.path.join(gen, f)):
            shutil.copy(os.path.join(HERE, f), gen)
    open(os.path.join(docs, 'VERIFICATION.txt'), 'w').write(
        'Generated %s. Connectivity checked with KiCad %s (kicad-cli netlist export) against the\n'
        'intended netlist; also checks unconnected pins, single-node nets, multiple outputs, power sources\n'
        'and missing footprints. ERC in the KiCad GUI is still recommended after opening in KiCad 8/9.\n\n' % (
            datetime.datetime.now().isoformat(timespec='minutes'),
            subprocess.run(['kicad-cli', 'version'], capture_output=True, text=True).stdout.strip()) + '\n'.join(report) + '\n')
    readme(os.path.join(root, 'README.md'))
    print('\n'.join(report))
    return boards

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'f91w-hw'))

```


## `gen/README_TEMPLATE.md`

````markdown
# F-91W BLE: hardware rev A (schematics)

A replacement board for the Casio F-91W with Bluetooth LE, a step counter (accelerometer) and a spot-check heart-rate sensor. It is programmable over SWD and BLE OTA.

**Status: rev A schematics, first pass.** Everything here has been checked by a script (see Verification), but no engineer has reviewed it yet and it has not been laid out. Do the open items below before ordering anything.

## What's in here

| Path | What |
|---|---|
| `mainboard/` | KiCad project for the STM32WB55 mainboard: the drop-in replacement for the F-91W module PCB |
| `ppg-flex/` | KiCad project for the heart-rate flex (MAXM86161 + 3.6 V boost), which sits in the caseback and plugs into the mainboard |
| `lib/` | Project symbols (`f91w.kicad_sym`) and footprints (`f91w.pretty`, **KiCad 8+**) |
| `docs/` | Schematic PDFs, BOM CSVs, KiCad netlists, intended netlists and the verification report |
| `firmware/board_f91w_ble.h` | Pin map and LCD segment translation table, generated from the same data as the schematic |
| `firmware/hr/` | Spot-check heart-rate algorithm in portable C, with a host test on synthetic PPG (`make test`) |
| `gen/` | Python generator: `python3 gen/build_all.py <outdir>` rebuilds everything (needs `kicad-cli`) |

The schematics are written in KiCad 7 format, so KiCad 7, 8 and 9 can all open them. KiCad 8/9 will offer to upgrade the file on save. The footprint library needs KiCad 8 or newer, because two of the reused Sensor Watch footprints are in v8 format.

## Architecture

```
                 F-91W LCD (3 COM x 24 SEG, zebra strip)
                                |
 CR2016 ──VDD (2.7-3.1 V, no regulator)──┬──────────────────────────────────┐
                                         │                                  │
   buttons x3 (active-high) ──► ┌────────┴─────────┐ ◄── LIS2DW12 (I2C 0x19, │
   piezo ◄─ 14 mH + NMOS ◄───── │   STM32WB55RGV6  │      INT1/INT2: steps)  │
   RGB backlight ◄───────────── │ BLE + LCD + M4F  │ ◄── SWD (Tag-Connect)   │
   chip antenna ◄─ π ◄─ MLPF ◄─ │ RF1   SMPS 10 µH │                         │
                                └────────┬─────────┘                         │
                                    I2C + A0/A1                              │
                                         │  J1: 9-pin FPC (Sensor Watch pinout)
                                         ▼                                   │
                PPG flex: 47 Ω/22 µF ─► TPS610995 (3.6 V, EN=A0) ─► MAXM86161 (INT -> A1)
```

Design decisions:

- **One chip does everything.** The STM32WB55 handles BLE 5.4, drives the stock segment LCD directly (1/3 duty, 1/3 bias) and runs the application on its Cortex-M4F. Ollee's spec ("Cortex-M4/M0+ with BLE") suggests it uses the same chip family.
- **No USB.** It would cost 2 pins plus a power path, and the 49 I/O pins are fully used. Instead:
  - Flash and debug over SWD, using an ST-Link and a TC2030-NL cable.
  - Update in the field over BLE OTA.
- **Same 9-pin sensor connector as Sensor Watch.** The PPG flex can also be bench-tested on a Sensor Watch Pro.
- **The accelerometer is on the mainboard.** It's wired the same way as the Sensor Watch accelerometer board, so step-counter code ports easily.

## MCU pin map (STM32WB55RGV6, VFQFPN68)

| MCU pin | Net | Function |
|---|---|---|
@@PINTABLE@@

**LCD segment mapping:**

- `LCD_SEGn` is the same physical zebra pad that Sensor Watch calls `SEGn`. Movement's `(com, seg)` segment maps can therefore be reused: translate `seg` through `LCD_GLASS_SEG_TO_MCU_SEG[]` in `firmware/board_f91w_ble.h`.
- Any LCD-capable pin can drive any glass line, so reassign SEG pins freely during layout. Then regenerate the header. COM0-2 must stay on PA8/PA9/PA10.

**EXTI lines in use:**

- 0: MODE
- 2: LIGHT
- 4: ACC_INT1
- 5: ACC_INT2
- 12: A1/PPG_INT
- 13: ALARM

A0 (PB1) and A4 (PC1) share EXTI1, so only one of them can be an interrupt input.

## Power budget (CR2016 ≈ 90 mAh, 85 % usable). These are estimates to measure on the first boards.

| Configuration | Avg current | Battery life |
|---|---|---|
| Watch only (Stop2 + RTC + LCD) | 3.1-6.1 µA | 17-34 months |
| + step counting | 5.9-10.8 µA | 10-18 months |
| + 20 HR spot checks/day | 6.9-14.5 µA | 7-15 months |
| + BLE sync on demand (button) | 7.2-16.0 µA | 7-15 months |
| + BLE always advertising instead | 11.5-23.5 µA | 4-9 months |

Assumptions:

- STM32WB55 Stop2 + RTC: 2.1 µA (datasheet).
- LCD: 1-4 µA (estimated, not measured).
- LIS2DW12 step counting: 2.3-3.2 µA, plus MCU wake-ups.
- HR check: 150-540 µA for 30 s.
- BLE TX: 5.2 mA at 0 dBm with the SMPS.

So: sync over BLE when the user asks for it (or a few times a day), and don't advertise all the time.

The mainboard has 2×22 µF of bulk capacitance, which buffers BLE TX bursts. On the flex, a 47 Ω series resistor keeps boost inrush off the coin cell.

## Open items before layout / fab

1. **MAXM86161 footprint.** It doesn't exist yet. Draw it from ADI land pattern 90-100106 (OLGA-14, 2.9×4.3×1.4 mm).
2. **MLPF-WB-01E3 footprint.** It was generated from the datasheet text (6 pads, 0.2 mm, 0.5 × 0.587 mm pitch). Check it against ST's land pattern.
3. **Q1 (piezo NMOS) part number.** Still TBD. It needs ≥30 V V<sub>DS</sub>, a logic-level V<sub>GS(th)</sub> ≤ 1.2 V, and a SOT-523 package. Measure the inductive spike on the first board.
4. **LSE crystal.** Confirm the CL of the NX2012SA variant you buy, then size C18/C19 (10 pF follows AN5165 for NX2012).
5. **Antenna.**
   - Place AE1 at the board edge where Sensor Watch had the USB receptacle, and delete those USB pads from the outline footprint.
   - Keep ground clearance per Johanson's layout.
   - Tune the π network (C20/R3/C21) with a VNA inside the closed watch, with the battery and caseback fitted.
6. **Stack-up.** 4-layer, ~0.6 mm, like Sensor Watch Pro.
   - Solid ground on layer 2.
   - The RF1→IPD trace should be 62 Ω and the IPD→antenna trace 50 Ω.
   - Put the HSE crystal and the SMPS loop tight to U1.
7. **Mechanical.**
   - Check component heights against the module frame and battery clip (the 14 mH inductor and the 0805 10 µH are the tall parts).
   - Route the PPG flex past the frame to the caseback window, with a service loop.
8. **ERC.** Run it in the KiCad GUI after opening. The netlist checks pass, but KiCad 7's CLI has no ERC.

## Firmware plan

- **Recommended:** STM32CubeWB (BLE stack on CPU2), with a shim that implements Movement's `watch_*` HAL so the Sensor Watch faces carry over. The pieces are:
  - segment LCD, using the table in `board_f91w_ble.h`
  - RTC
  - buttons/EXTI
  - TIM1 buzzer
  - I2C
  - ADC/VREFINT for battery voltage
  - Stop2 sleep
- **Alternative:** Zephyr, which supports STM32WB55 BLE through ST's stack. You'd still have to write the LCD driver.
- **Heart rate** (`firmware/hr/`):
  1. Drive A0 high and wait about 5 ms.
  2. Configure the MAXM86161: green LED, 25 or 50 sps, 117 µs integration, around 20 mA to start.
  3. Collect 8-10 s while the accelerometer reports stillness.
  4. Call `hr_estimate()`.
  5. Turn the boost off.

  The algorithm band-passes the signal, picks the period by autocorrelation (McLeod-style), uses a spectral octave check to avoid double/half errors, and refines the result with a Goertzel scan.

  Host test results on synthetic wrist PPG: MAE 0.2-0.6 BPM at 25 sps across noise levels up to 1× the AC amplitude, and ≤1.7 % false "ok" readings on pure noise. Real wrist data will be harder, so log raw samples from the first boards.

## Verification

`docs/VERIFICATION.txt` was produced with KiCad's own connectivity engine (`kicad-cli sch export netlist`). The check confirms that:

- every net matches the intended netlist exactly
- every pin is either connected or explicitly no-connect
- there are no single-pin nets
- no net has two outputs
- every power net has a source
- every part has a footprint

## Licence / attribution

The F-91W outline, the FH19C connector and the flex-tail footprints, the ELT3KN inductor footprint, the QBLP655 LED footprint and the LGA-12 accelerometer footprint are copied from [Sensor Watch](https://github.com/joeycastillo/Sensor-Watch) by Joey Castillo (CC-BY-SA 4.0). Boards built on them are derivative works: share them under CC-BY-SA 4.0 with attribution.

````


## `firmware/hr/hr_spotcheck.h`

```c
/*
 * hr_spotcheck: on-demand heart-rate estimate from a wrist PPG window (e.g. MAXM86161 green channel).
 * Portable C99 + float (STM32WB55 Cortex-M4 has an FPU). No dynamic allocation.
 *
 * Usage: hr_init() -> hr_push() each sample (PPG counts + motion in g) -> hr_estimate() once the
 * buffer holds >= HR_MIN_SECONDS of data. The user is expected to hold still (spot check).
 */
#ifndef HR_SPOTCHECK_H
#define HR_SPOTCHECK_H

#define HR_MAX_SAMPLES   400     /* 16 s at 25 sps */
#define HR_MIN_SECONDS   6.0f
#define HR_BPM_MIN       35.0f
#define HR_BPM_MAX       220.0f

typedef enum {
    HR_OK = 0,
    HR_TOO_SHORT,      /* not enough samples yet */
    HR_LOW_SIGNAL,     /* perfusion index too small: loose watch / no skin contact */
    HR_MOTION,         /* accelerometer says the wrist moved */
    HR_NO_PEAK,        /* no periodicity in the heart-rate band */
    HR_LOW_CONFIDENCE  /* periodic, but not convincingly */
} hr_status_t;

typedef struct {
    float fs;                         /* sample rate, Hz */
    int   n;                          /* samples held */
    float ppg[HR_MAX_SAMPLES];        /* raw optical counts (any unit, DC included) */
    float motion[HR_MAX_SAMPLES];     /* |a| - 1 g, in g (0 if no accelerometer) */
} hr_buffer_t;

typedef struct {
    float       bpm;
    float       confidence;           /* normalised autocorrelation at the chosen lag, 0..1 */
    float       perfusion_index;      /* AC rms / DC */
    hr_status_t status;
} hr_result_t;

typedef struct {
    float min_confidence;             /* default 0.45 */
    float min_perfusion_index;        /* default 2e-4 (0.02 %) */
    float max_motion_rms_g;           /* default 0.05 g */
} hr_config_t;

void        hr_default_config(hr_config_t *cfg);
void        hr_init(hr_buffer_t *b, float fs);
int         hr_push(hr_buffer_t *b, float ppg, float motion_g);   /* returns 1 when the buffer is full */
hr_result_t hr_estimate(const hr_buffer_t *b, const hr_config_t *cfg);
const char *hr_status_str(hr_status_t s);

#endif

```


## `firmware/hr/hr_spotcheck.c`

```c
#include "hr_spotcheck.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float b0, b1, b2, a1, a2; } biquad_t;

/* RBJ cookbook 2nd-order Butterworth (Q = 1/sqrt2) high/low-pass via bilinear transform */
static biquad_t biquad_design(float fc, float fs, int highpass) {
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float c = cosf(w0), s = sinf(w0);
    float alpha = s / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    biquad_t q;
    if (highpass) {
        q.b0 = (1.0f + c) / 2.0f; q.b1 = -(1.0f + c); q.b2 = (1.0f + c) / 2.0f;
    } else {
        q.b0 = (1.0f - c) / 2.0f; q.b1 = 1.0f - c;    q.b2 = (1.0f - c) / 2.0f;
    }
    q.a1 = -2.0f * c; q.a2 = 1.0f - alpha;
    q.b0 /= a0; q.b1 /= a0; q.b2 /= a0; q.a1 /= a0; q.a2 /= a0;
    return q;
}

/* in-place forward pass, Direct Form II transposed, state primed to the first sample's steady state */
static void biquad_run(const biquad_t *q, float *x, int n, int reverse) {
    float z1 = 0.0f, z2 = 0.0f;
    int i0 = reverse ? n - 1 : 0, step = reverse ? -1 : 1;
    /* prime with steady-state for a constant input x0 (removes start-up transient) */
    float x0 = x[i0];
    float dc_gain = (q->b0 + q->b1 + q->b2) / (1.0f + q->a1 + q->a2);
    float y0 = dc_gain * x0;
    z1 = y0 - q->b0 * x0;
    z2 = q->b2 * x0 - q->a2 * y0;
    for (int k = 0, i = i0; k < n; k++, i += step) {
        float in = x[i];
        float out = q->b0 * in + z1;
        z1 = q->b1 * in - q->a1 * out + z2;
        z2 = q->b2 * in - q->a2 * out;
        x[i] = out;
    }
}

/* power of x at frequency f (Hz) - single-bin DFT via the Goertzel recurrence */
static float goertzel_power(const float *x, int n, float f, float fs) {
    float w = 2.0f * (float)M_PI * f / fs, c = 2.0f * cosf(w);
    float s1 = 0.0f, s2 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s0 = x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (float)n;
}

static int nearest_key_max(const float *r, int kmin, int kmax, float target, int tol) {
    int best = -1;
    int t = (int)(target + 0.5f);
    for (int k = t - tol; k <= t + tol; k++) {
        if (k < kmin || k > kmax) continue;
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] > 0.0f && (best < 0 || r[k] > r[best])) best = k;
    }
    return best;
}

static float refine_lag(const float *r, int k) {
    float y0 = r[k - 1], y1 = r[k], y2 = r[k + 1];
    float den = y0 - 2.0f * y1 + y2;
    float d = (den != 0.0f) ? 0.5f * (y0 - y2) / den : 0.0f;
    if (d > 0.5f) d = 0.5f;
    if (d < -0.5f) d = -0.5f;
    return (float)k + d;
}

void hr_default_config(hr_config_t *cfg) {
    cfg->min_confidence = 0.45f;
    cfg->min_perfusion_index = 2e-4f;
    cfg->max_motion_rms_g = 0.05f;
}

void hr_init(hr_buffer_t *b, float fs) { b->fs = fs; b->n = 0; }

int hr_push(hr_buffer_t *b, float ppg, float motion_g) {
    if (b->n < HR_MAX_SAMPLES) {
        b->ppg[b->n] = ppg;
        b->motion[b->n] = motion_g;
        b->n++;
    }
    return b->n >= HR_MAX_SAMPLES;
}

const char *hr_status_str(hr_status_t s) {
    switch (s) {
        case HR_OK: return "ok";
        case HR_TOO_SHORT: return "too short";
        case HR_LOW_SIGNAL: return "low signal";
        case HR_MOTION: return "motion";
        case HR_NO_PEAK: return "no peak";
        case HR_LOW_CONFIDENCE: return "low confidence";
    }
    return "?";
}

hr_result_t hr_estimate(const hr_buffer_t *b, const hr_config_t *cfg_in) {
    static float y[HR_MAX_SAMPLES];            /* work buffer (static: keeps the stack small) */
    static float r[HR_MAX_SAMPLES];
    hr_config_t cfg;
    if (cfg_in) cfg = *cfg_in; else hr_default_config(&cfg);
    hr_result_t res = {0.0f, 0.0f, 0.0f, HR_TOO_SHORT};
    const int n = b->n;
    const float fs = b->fs;
    if (n < (int)(HR_MIN_SECONDS * fs) || fs <= 0.0f) return res;

    /* motion gate: RMS of (|a| - 1 g) over the window */
    double msum = 0.0;
    for (int i = 0; i < n; i++) msum += (double)b->motion[i] * b->motion[i];
    float motion_rms = (float)sqrt(msum / n);

    /* DC level and mean removal */
    double dc = 0.0;
    for (int i = 0; i < n; i++) dc += b->ppg[i];
    dc /= n;
    for (int i = 0; i < n; i++) y[i] = (float)(b->ppg[i] - dc);

    /* zero-phase band-pass 0.6-3.7 Hz (36-222 BPM): HP + LP, forward and backward */
    biquad_t hp = biquad_design(0.6f, fs, 1);
    biquad_t lp = biquad_design(3.7f, fs, 0);
    biquad_run(&hp, y, n, 0); biquad_run(&lp, y, n, 0);
    biquad_run(&hp, y, n, 1); biquad_run(&lp, y, n, 1);

    /* ignore 1 s at each edge (filter edge effects) */
    int e = (int)(1.0f * fs);
    const float *s = y + e;
    int m = n - 2 * e;
    double p = 0.0;
    for (int i = 0; i < m; i++) p += (double)s[i] * s[i];
    float ac_rms = (float)sqrt(p / m);
    res.perfusion_index = (dc != 0.0) ? ac_rms / (float)fabs(dc) : 0.0f;
    if (res.perfusion_index < cfg.min_perfusion_index || p <= 0.0) { res.status = HR_LOW_SIGNAL; return res; }
    if (motion_rms > cfg.max_motion_rms_g) { res.status = HR_MOTION; return res; }

    /* normalised (unbiased) autocorrelation over the heart-rate lag range */
    int kmin = (int)floorf(fs * 60.0f / HR_BPM_MAX);
    int kmax = (int)ceilf(fs * 60.0f / HR_BPM_MIN);
    if (kmin < 2) kmin = 2;
    if (kmax > m / 2) kmax = m / 2;
    double r0 = p / m;
    for (int k = kmin - 1; k <= kmax + 1; k++) {
        double acc = 0.0;
        for (int i = 0; i + k < m; i++) acc += (double)s[i] * s[i + k];
        r[k] = (float)((acc / (m - k)) / r0);
    }

    /* McLeod-style pick: among the local maxima ("key maxima") in range, take the SHORTEST lag whose
     * height is within 85 % of the tallest. Autocorrelation peaks repeat at 2T, 3T... with nearly the
     * same height (slowly varying heart rate), so "tallest" alone often lands on a multiple of T. */
    float rmax = 0.0f;
    int nkeys = 0;
    for (int k = kmin; k <= kmax; k++) {
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] > 0.0f) {
            nkeys++;
            if (r[k] > rmax) rmax = r[k];
        }
    }
    if (nkeys == 0) { res.status = HR_NO_PEAK; return res; }
    int best = -1;
    for (int k = kmin; k <= kmax; k++) {
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] >= 0.85f * rmax) { best = k; break; }
    }

    /* octave check in the spectrum: the PPG fundamental carries the most power, so compare the
     * candidate period with its octave/third neighbours and keep the one with the strongest bin. */
    int cands[4]; int nc = 0;
    cands[nc++] = best;
    int c2 = nearest_key_max(r, kmin, kmax, best / 2.0f, 1);
    int c3 = nearest_key_max(r, kmin, kmax, best / 3.0f, 1);
    int cx = nearest_key_max(r, kmin, kmax, best * 2.0f, 2);
    if (c2 > 0) cands[nc++] = c2;
    if (c3 > 0) cands[nc++] = c3;
    if (cx > 0) cands[nc++] = cx;
    float best_pow = -1.0f, lag = (float)best;
    for (int i = 0; i < nc; i++) {
        float L = refine_lag(r, cands[i]);
        float pw = goertzel_power(s, m, fs / L, fs);
        if (pw > best_pow) { best_pow = pw; best = cands[i]; lag = L; }
    }
    float y1 = r[best];

    /* frequency refinement: scan the spectrum +-6 % around the chosen period (Goertzel, 0.2 % steps)
     * - the lag grid is coarse at high heart rates (~8 samples per beat at 180 BPM, 25 sps) */
    float f0 = fs / lag, fbest = f0, pbest = -1.0f;
    for (int i = -30; i <= 30; i++) {
        float f = f0 * (1.0f + 0.002f * (float)i);
        float pw = goertzel_power(s, m, f, fs);
        if (pw > pbest) { pbest = pw; fbest = f; }
    }
    res.bpm = 60.0f * fbest;
    res.confidence = y1;
    res.status = (y1 >= cfg.min_confidence) ? HR_OK : HR_LOW_CONFIDENCE;
    return res;
}

```


## `firmware/hr/test_hr.c`

```c
/* Host test: synthetic wrist-PPG (fundamental + 2nd harmonic, HRV, respiration baseline, drift,
 * white noise) at 25 sps; checks accuracy, noise rejection, motion and low-signal gating. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hr_spotcheck.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned long long rng = 88172645463325252ULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }
static double gauss(void) { double u = urand() + 1e-12, v = urand(); return sqrt(-2.0 * log(u)) * cos(2 * M_PI * v); }

static double FS = 25.0;
static void synth(hr_buffer_t *b, double bpm, double seconds, double pi, double noise_rel, double motion_g, double harm2) {
    const double fs = FS, dc = 100000.0;
    hr_init(b, (float)fs);
    double A = pi * dc;                     /* AC amplitude from perfusion index */
    double ph = urand() * 2 * M_PI, ph2 = urand() * 2 * M_PI, phase = 0.0;
    double resp = 0.2 + 0.15 * urand(), hrv = 0.03;
    int n = (int)(seconds * fs);
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double f = bpm / 60.0 * (1.0 + hrv * sin(2 * M_PI * 0.25 * t + ph));   /* HRV / RSA */
        phase += 2 * M_PI * f / fs;
        double ac = A * (sin(phase) + harm2 * sin(2 * phase + ph2));
        double base = dc * (1.0 + 0.004 * sin(2 * M_PI * resp * t) + 0.002 * t / seconds);
        double x = base - ac + noise_rel * A * gauss();
        double mo = motion_g * gauss();
        hr_push(b, (float)x, (float)mo);
    }
}

static hr_buffer_t buf;

int main(void) {
    hr_config_t cfg; hr_default_config(&cfg);
    int fails = 0;

    /* 1) sweep heart rates at moderate noise */
    printf("1) sweep (PI 0.3%%, noise 0.3xAC, 10 s):\n");
    double rates[] = {40, 45, 55, 60, 72, 85, 100, 120, 140, 160, 180, 200};
    for (unsigned i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        synth(&buf, rates[i], 10.0, 0.003, 0.3, 0.0, 0.4);
        hr_result_t r = hr_estimate(&buf, &cfg);
        double err = r.bpm - rates[i];
        int ok = r.status == HR_OK && fabs(err) <= 3.0;
        printf("   true %5.1f  est %6.2f  conf %.2f  %s%s\n", rates[i], r.bpm, r.confidence, hr_status_str(r.status), ok ? "" : "   <-- FAIL");
        fails += !ok;
    }

    /* 2) Monte Carlo accuracy at several noise levels */
    double noises[] = {0.2, 0.5, 1.0};
    for (unsigned j = 0; j < 3; j++) {
        int trials = 300, okc = 0, within3 = 0, valid = 0; double sae = 0, maxe = 0;
        for (int t = 0; t < trials; t++) {
            double bpm = 45 + urand() * 135;
            synth(&buf, bpm, 10.0, 0.002 + 0.004 * urand(), noises[j], 0.0, 0.2 + 0.4 * urand());
            hr_result_t r = hr_estimate(&buf, &cfg);
            if (r.status == HR_OK) {
                valid++;
                double e = fabs(r.bpm - bpm); sae += e; if (e > maxe) maxe = e; if (e <= 3.0) within3++;
            }
            okc++;
        }
        printf("2) noise %.1fxAC: valid %3d/%d, of valid: MAE %.2f BPM, max %.1f, within 3 BPM %.1f%%\n",
               noises[j], valid, trials, valid ? sae / valid : 0, maxe, valid ? 100.0 * within3 / valid : 0);
        if (j == 0 && (valid < trials * 0.98 || within3 < valid * 0.98)) { printf("   <-- FAIL\n"); fails++; }
        if (j == 1 && (valid < trials * 0.90 || within3 < valid * 0.95)) { printf("   <-- FAIL\n"); fails++; }
        if (j == 2 && within3 < valid * 0.85) { printf("   <-- FAIL (valid readings must stay accurate)\n"); fails++; }
    }

    /* 2b) same at 50 sps (window must fit HR_MAX_SAMPLES: 8 s) */
    FS = 50.0;
    {
        int trials = 300, valid = 0, within3 = 0; double sae = 0, maxe = 0;
        for (int t = 0; t < trials; t++) {
            double bpm = 45 + urand() * 135;
            synth(&buf, bpm, 8.0, 0.002 + 0.004 * urand(), 1.0, 0.0, 0.2 + 0.4 * urand());
            hr_result_t r = hr_estimate(&buf, &cfg);
            if (r.status == HR_OK) { valid++; double e = fabs(r.bpm - bpm); sae += e; if (e > maxe) maxe = e; if (e <= 3.0) within3++; }
        }
        printf("2b) 50 sps, 8 s, noise 1.0xAC: valid %3d/%d, MAE %.2f BPM, max %.1f, within 3 BPM %.1f%%\n",
               valid, trials, valid ? sae / valid : 0, maxe, valid ? 100.0 * within3 / valid : 0);
        if (within3 < valid * 0.9) { printf("   <-- FAIL\n"); fails++; }
    }
    FS = 25.0;

    /* 3) pure noise must not produce confident readings */
    int fp = 0, trials = 300;
    for (int t = 0; t < trials; t++) {
        synth(&buf, 70, 10.0, 0.003, 0.0, 0.0, 0.0);              /* build timing/DC */
        for (int i = 0; i < buf.n; i++) buf.ppg[i] = 100000.0f + 300.0f * (float)gauss();
        hr_result_t r = hr_estimate(&buf, &cfg);
        fp += r.status == HR_OK;
    }
    printf("3) white noise only: %d/%d false 'ok' readings\n", fp, trials);
    if (fp > trials * 0.05) { printf("   <-- FAIL\n"); fails++; }

    /* 4) gating */
    synth(&buf, 75, 10.0, 0.003, 0.3, 0.2, 0.4);
    hr_result_t r = hr_estimate(&buf, &cfg);
    printf("4) motion 0.2 g rms -> %s\n", hr_status_str(r.status)); fails += r.status != HR_MOTION;
    synth(&buf, 75, 10.0, 0.00005, 0.3, 0.0, 0.4);
    r = hr_estimate(&buf, &cfg);
    printf("   PI 0.005%% -> %s (PI %.5f)\n", hr_status_str(r.status), r.perfusion_index); fails += r.status != HR_LOW_SIGNAL;
    synth(&buf, 75, 4.0, 0.003, 0.3, 0.0, 0.4);
    r = hr_estimate(&buf, &cfg);
    printf("   4 s window -> %s\n", hr_status_str(r.status)); fails += r.status != HR_TOO_SHORT;

    printf("\n%s (%d failing checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}

```


## `firmware/hr/Makefile`

```
# Host-side test for the heart-rate algorithm (no hardware needed)
CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra

test: test_hr
	./test_hr

test_hr: test_hr.c hr_spotcheck.c hr_spotcheck.h
	$(CC) $(CFLAGS) -o $@ test_hr.c hr_spotcheck.c -lm

clean:
	rm -f test_hr

```
