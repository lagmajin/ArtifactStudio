from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

"""Project View improvement-version mockups (2026-09-26).

Draws NEW image files only. Existing mockups and the 2026-09-21 runtime
screenshot are read-only references: never overwritten, resized, or deleted.

These are improvement proposals on top of the current runtime layout.
They are not a replacement direction and not an implementation spec.
"""

OUT = Path(__file__).parent
W, H = 1508, 980

BG = (17, 19, 22)
PANEL = (26, 29, 33)
PANEL_2 = (33, 37, 42)
PANEL_3 = (40, 44, 50)
LINE = (56, 61, 68)
LINE_2 = (40, 44, 50)
TEXT = (231, 233, 236)
SOFT = (206, 210, 215)
MUTED = (150, 156, 164)
FAINT = (116, 122, 130)
AMBER = (240, 174, 66)
ICE = (150, 220, 235)
CYAN = (86, 190, 214)
GREEN = (108, 196, 138)
SEL = (33, 47, 72)
SEL_EDGE = (58, 82, 124)

REG = [Path('C:/Windows/Fonts/segoeui.ttf'), Path('C:/Windows/Fonts/arial.ttf')]
SBD = [Path('C:/Windows/Fonts/seguisb.ttf'), Path('C:/Windows/Fonts/arialbd.ttf')]


def font(size, bold=False):
    for p in (SBD if bold else REG):
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


F10, F11, F12, F13 = [font(x) for x in (10, 11, 12, 13)]
B10, B11, B12, B13, B15 = [font(x, True) for x in (10, 11, 12, 13, 15)]


def grad(size, top, bottom):
    im = Image.new('RGB', size, top)
    d = ImageDraw.Draw(im)
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        d.line((0, y, size[0], y),
               fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    return im


def rr(d, box, radius=5, fill=PANEL_2, outline=LINE, width=1):
    d.rounded_rectangle(box, radius, fill=fill, outline=outline, width=width)


def txt(d, xy, value, fill=TEXT, f=F12, anchor=None):
    d.text(xy, value, fill=fill, font=f, anchor=anchor)


def tw(d, value, f):
    return d.textlength(value, font=f)


FOLDER = 'folder'
COMP = 'composition'
IMAGE = 'image'
SEQ = 'sequence'
TEXT_L = 'text'
AUDIO = 'audio'

ROWS = [
    (0, 'NeonStreet', FOLDER, '', '18 items', '2026-09-24 18:02', True),
    (1, 'Footage', FOLDER, '', '4 items', '2026-09-24 17:41', True),
    (2, 'city_plate_4k.png', IMAGE, 'Ready', '3840x2160', '2026-09-24 17:40', False),
    (2, 'city_plate_4k_v02.png', IMAGE, 'Ready', '3840x2160', '2026-09-24 17:12', False),
    (2, 'rain_streaks_512', SEQ, 'Ready', '512x512 · 24f', '2026-09-23 11:08', False),
    (2, 'scanline_overlay.png', IMAGE, 'Draft', '1920x1080', '2026-09-22 09:55', False),
    (1, 'Titles', FOLDER, '', '3 items', '2026-09-21 20:14', True),
    (2, 'main_title', COMP, 'Ready', '1920x1080 · 6.0s', '2026-09-21 20:13', False),
    (2, 'subtitle_kit', COMP, 'Ready', '1920x1080 · 3.5s', '2026-09-20 15:30', False),
    (2, 'credit_roll', TEXT_L, 'Draft', 'Inter · 2.0s', '2026-09-19 08:22', False),
    (1, 'Audio', FOLDER, '', '2 items', '2026-09-18 13:05', True),
    (2, 'city_ambience.wav', AUDIO, 'Ready', '48kHz · 12.0s', '2026-09-18 13:04', False),
    (2, 'sub_hit_01.wav', AUDIO, 'Ready', '48kHz · 0.8s', '2026-09-18 13:02', False),
    (0, 'Library', FOLDER, '', '9 items', '2026-09-16 10:20', False),
    (0, 'scratch_plate.png', IMAGE, 'Unused', '2048x1152', '2026-09-12 22:41', False),
]

SELECTED = 'city_plate_4k.png'

STATUS_COLOR = {'Ready': GREEN, 'Draft': AMBER, 'Unused': FAINT}
TYPE_COLOR = {FOLDER: (198, 168, 96), COMP: CYAN, IMAGE: (150, 178, 210),
              SEQ: (168, 150, 214), TEXT_L: ICE, AUDIO: (206, 152, 178)}

ROW_H = 28



def glyph_folder(d, x, y, s=11):
    d.rounded_rectangle((x, y + 2, x + s, y + s - 1), 2, fill=(198, 168, 96))
    d.rectangle((x, y, x + 5, y + 3), fill=(198, 168, 96))


def glyph_doc(d, x, y, s=11, color=(150, 178, 210)):
    d.rounded_rectangle((x, y, x + s - 3, y + s), 2, outline=color, width=1)
    d.line((x + s - 3, y, x + s - 3, y + 4), fill=color)
    d.line((x + s - 3, y, x + s, y + 4), fill=color)


def glyph_stack(d, x, y, s=11, color=(168, 150, 214)):
    d.rounded_rectangle((x + 2, y, x + s, y + s - 2), 2, outline=color, width=1)
    d.rounded_rectangle((x, y + 3, x + s - 2, y + s), 2, outline=color, width=1)


def glyph_wave(d, x, y, s=11, color=(206, 152, 178)):
    d.line((x, y + s // 2, x + s, y + s // 2), fill=color)
    for i, hgt in enumerate((3, 6, 4, 7)):
        d.line((x + 1 + i * 3, y + s // 2 - hgt, x + 1 + i * 3, y + s // 2 + hgt), fill=color)


def glyph(d, x, y, kind, s=11):
    if kind == FOLDER:
        glyph_folder(d, x, y, s)
    elif kind == SEQ:
        glyph_stack(d, x, y, s, TYPE_COLOR[SEQ])
    elif kind == AUDIO:
        glyph_wave(d, x, y, s)
    elif kind in (COMP, TEXT_L):
        glyph_doc(d, x, y, s, TYPE_COLOR[kind])
    else:
        glyph_doc(d, x, y, s, TYPE_COLOR[IMAGE])


def disclosure(d, x, y, open_):
    if open_:
        d.polygon([(x, y), (x + 8, y), (x + 4, y + 5)], fill=SOFT)
    else:
        d.polygon([(x + 1, y - 1), (x + 7, y + 4), (x + 1, y + 9)], fill=SOFT)


def icon_button(d, x, y, label, size=28, active=False, tone=SOFT):
    rr(d, (x, y, x + size, y + size), 5,
       (48, 40, 24) if active else PANEL_2, AMBER if active else LINE)
    txt(d, (x + size / 2, y + size / 2 + 0.5), label, AMBER if active else tone,
        B11, 'mm')


def check_box(d, x, y, on, label):
    rr(d, (x, y, x + 15, y + 15), 3, PANEL_2 if not on else (58, 45, 22), LINE)
    if on:
        d.line((x + 4, y + 8, x + 7, y + 11), fill=AMBER, width=2)
        d.line((x + 7, y + 11, x + 12, y + 4), fill=AMBER, width=2)
    txt(d, (x + 22, y + 8), label, MUTED, F12, 'lm')


def status_dot(d, x, y, color):
    d.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)


def empty_doc(d, cx, cy, scale=1.0, color=(48, 54, 60), mark=(58, 66, 74)):
    """Small quiet document glyph, replacing the loud cyan empty-state icon."""
    w, h = 34 * scale, 44 * scale
    x, y = cx - w / 2, cy - h / 2
    d.rounded_rectangle((x, y, x + w, y + h), 4 * scale, outline=color,
                        width=max(1, int(2 * scale)))
    d.line((x + w * 0.5, y + 4 * scale, x + w * 0.5, y + h - 12 * scale), fill=mark,
           width=max(1, int(2 * scale)))
    d.line((x + w * 0.5, y + h - 8 * scale, x + w, y + h - 8 * scale), fill=mark,
           width=max(1, int(2 * scale)))


def panel_frame(d, badge):
    """Dock panel tab strip + status bar, same roles as the runtime capture."""
    d.rectangle((0, 0, W, 34), fill=(23, 25, 29))
    d.line((0, 33, W, 33), fill=LINE)
    rr(d, (6, 4, 112, 32), 5, (33, 37, 42), (72, 80, 92))
    txt(d, (18, 18), 'Project', TEXT, B12, 'lm')
    d.line((64, 12, 64, 24), fill=FAINT, width=2)
    d.line((60, 16, 68, 20), fill=FAINT, width=2)
    txt(d, (W - 16, 18), badge, MUTED, F11, 'rm')

    d.rectangle((0, H - 30, W, H), fill=(23, 25, 29))
    d.line((0, H - 30, W, H - 30), fill=LINE)


def tool_header(d, y, search='Search project, tags, type...', type_filter='All types',
                mode='Tree', unused=True):
    """Improvement: one low-density header row.

    search (flex) | fixed-width type filter | compact Tree/Tile switch |
    Unused checkbox | quiet action cluster. No second control band.
    """
    d.rectangle((0, y, W, y + 46), fill=PANEL)
    d.line((0, y + 45, W, y + 45), fill=LINE)

    sx0, sx1 = 14, 700
    rr(d, (sx0, y + 8, sx1, y + 38), 5, (20, 22, 26), (64, 70, 78))
    d.ellipse((sx0 + 11, y + 18, sx0 + 21, y + 28), outline=MUTED, width=2)
    d.line((sx0 + 21, y + 28, sx0 + 26, y + 33), fill=MUTED, width=2)
    txt(d, (sx0 + 34, y + 23), search, MUTED, F12, 'lm')

    fx0, fx1 = 712, 856
    rr(d, (fx0, y + 8, fx1, y + 38), 5, PANEL_2, LINE)
    txt(d, (fx0 + 10, y + 23), type_filter, SOFT, F12, 'lm')
    d.polygon([(fx1 - 18, y + 21), (fx1 - 11, y + 21), (fx1 - 14.5, y + 26)], fill=MUTED)

    mx0 = 868
    rr(d, (mx0, y + 8, mx0 + 118, y + 38), 5, (30, 33, 38), LINE)
    half = mx0 + 59
    d.line((half, y + 10, half, y + 36), fill=LINE)
    for i, label in enumerate(('Tree', 'Tile')):
        cx = mx0 + 30 + i * 59
        on = (label == mode)
        if on:
            rr(d, (mx0 + 3 + i * 59, y + 11, mx0 + 56 + i * 59, y + 35), 4,
               (48, 40, 24), (92, 74, 34))
        glyph_doc(d, cx - 16, y + 18, 10, AMBER if on else MUTED)
        txt(d, (cx + 4, y + 23), label, AMBER if on else MUTED, F11, 'lm')

    check_box(d, 1002, y + 15, unused, 'Unused')

    bx = W - 14
    for label in ('•••', 'R', 'F', 'C', '+'):
        w = 30 if label == '•••' else 28
        bx -= w
        icon_button(d, bx, y + 9, label, w, label == '+')
        bx -= 6
    return y + 46


def context_row(d, y, crumbs, count, selected_count):
    """Improvement: breadcrumb plus one result count, no duplicated state words."""
    d.rectangle((0, y, W, y + 38), fill=(28, 31, 36))
    d.line((0, y + 37, W, y + 37), fill=LINE_2)
    x = 14
    for i, c in enumerate(crumbs):
        last = (i == len(crumbs) - 1)
        txt(d, (x, y + 19), c, SOFT if last else MUTED, B12 if last else F12, 'lm')
        x += tw(d, c, B12 if last else F12) + 2
        if not last:
            d.polygon([(x + 3, y + 16), (x + 8, y + 16), (x + 5.5, y + 21)], fill=FAINT)
            x += 16
    right = '%d items' % count
    if selected_count:
        right += '   ·   %d selected' % selected_count
    txt(d, (W - 14, y + 19), right, MUTED, F11, 'rm')
    return y + 38


def column_header(d, y, cols, sort_col=0, sort_dir=True):
    d.rectangle((0, y, W, y + 30), fill=(31, 34, 39))
    d.line((0, y + 29, W, y + 29), fill=LINE)
    for i, (x0, x1, label) in enumerate(cols):
        if x0:
            d.line((x0, y + 6, x0, y + 23), fill=(64, 70, 77))
        anchor = 'lm' if i == 0 else 'rm'
        tx = x0 + 10 if i == 0 else x1 - 10
        txt(d, (tx, y + 15), label, SOFT if i == sort_col else MUTED,
            B11 if i == sort_col else F11, anchor)
        if i == sort_col:
            if sort_dir:
                d.polygon([(x0 + 6, y + 16), (x0 + 12, y + 16), (x0 + 9, y + 11)], fill=SOFT)
            else:
                d.polygon([(x0 + 6, y + 11), (x0 + 12, y + 11), (x0 + 9, y + 16)], fill=SOFT)
    return y + 30


def preview_tile(d, box, seed=0):
    x0, y0, x1, y1 = box
    base = [(38, 44, 52), (44, 40, 52), (36, 48, 46), (50, 44, 38)][seed % 4]
    d.rectangle(box, fill=base)
    for i in range(5):
        yy = y0 + (y1 - y0) * (0.18 + 0.15 * i)
        d.line((x0, yy, x1, yy - 6), fill=tuple(min(255, c + 16) for c in base), width=2)
    d.line(((x0 + x1) / 2, y0, (x0 + x1) / 2, y1), fill=tuple(min(255, c + 10) for c in base))
    d.rectangle(box, outline=LINE)


def tree_rows(d, y, x_end, selected=SELECTED, hover=None, cols_w=None):
    d.rectangle((0, y, x_end, H - 30), fill=(21, 23, 27))
    sel_edge = None
    for (depth, name, kind, status, size, mod, opened) in ROWS:
        if name == selected:
            row_bg = SEL
        elif name == hover:
            row_bg = (28, 31, 37)
        else:
            row_bg = (21, 23, 27) if depth % 2 else (23, 25, 29)
        d.rectangle((0, y, x_end, y + ROW_H), fill=row_bg)
        d.line((0, y + ROW_H - 1, x_end, y + ROW_H - 1), fill=LINE_2)
        x = 14 + depth * 18
        if kind == FOLDER:
            disclosure(d, x, y + 12, opened)
            x += 14
        glyph(d, x, y + 8, kind)
        x += 17
        sel = (name == selected)
        txt(d, (x, y + ROW_H // 2), name, TEXT if sel else SOFT, B12 if sel else F12, 'lm')
        if cols_w:
            tx, sx, stx, mx = cols_w
            if kind != FOLDER:
                txt(d, (tx, y + ROW_H // 2), kind, MUTED, F11, 'lm')
            if status:
                status_dot(d, stx, y + ROW_H // 2, STATUS_COLOR[status])
                txt(d, (stx + 10, y + ROW_H // 2), status, STATUS_COLOR[status], F11, 'lm')
            txt(d, (sx, y + ROW_H // 2), size, MUTED, F11, 'rm')
            txt(d, (mx, y + ROW_H // 2), mod, FAINT, F11, 'rm')
        if sel:
            sel_edge = (x_end - 2, y, x_end - 2, y + ROW_H)
        y += ROW_H
    if sel_edge:
        d.line((sel_edge[0], sel_edge[1], sel_edge[0], sel_edge[2]), fill=SEL_EDGE, width=2)
    return y


def detail_pane(d, x0, name, kind, status, size, mod, y):
    """Improvement: detail only when something is selected, on a quiet surface."""
    d.rectangle((x0, y, W, H - 30), fill=PANEL)
    d.line((x0, y, x0, H - 30), fill=LINE)
    x = x0 + 18
    preview_tile(d, (x, y + 16, x + 190, y + 124), seed=len(name))
    txt(d, (x + 206, y + 44), name, TEXT, B15)
    txt(d, (x + 206, y + 70), kind, MUTED, F11)
    ry = y + 96
    for label, value, color in (('Status', status, STATUS_COLOR.get(status, MUTED)),
                                ('Size', size, SOFT), ('Modified', mod, SOFT),
                                ('Path', 'Project/NeonStreet/Footage', FAINT),
                                ('ID', 'ast_9f2c41', FAINT)):
        txt(d, (x + 206, ry), label, MUTED, F11)
        txt(d, (x + 280, ry), value, color, F11)
        ry += 20
    by = y + 204
    for i, label in enumerate(('Reveal', 'Rename', 'Reload', 'Delete')):
        bx = x + i * 84
        rr(d, (bx, by, bx + 76, by + 28), 5, PANEL_2, LINE)
        txt(d, (bx + 38, by + 14), label, SOFT, F11, 'mm')
    return by


def status_bar(d, left, right):
    txt(d, (14, H - 15), left, MUTED, F11, 'lm')
    txt(d, (W - 14, H - 15), right, MUTED, F11, 'rm')


def empty_state(d, box, message, sub, action):
    """Improvement: one quiet empty state, small glyph, one existing action."""
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 - 26
    empty_doc(d, cx, cy, 1.0)
    txt(d, (cx, cy + 56), message, SOFT, B13, 'mm')
    txt(d, (cx, cy + 80), sub, MUTED, F11, 'mm')
    bw = 24 + len(action) * 7
    rr(d, (cx - bw / 2, cy + 100, cx + bw / 2, cy + 130), 5, (58, 45, 22), AMBER)


def improvement_a(path):
    """Improvement A - dense readable tree, one header, selection-aware detail."""
    im = grad((W, H), (22, 24, 28), (15, 16, 19))
    d = ImageDraw.Draw(im)
    panel_frame(d, 'improvement A  ·  dense tree + columns')

    y = tool_header(d, 34)
    y = context_row(d, y, ['Project', 'NeonStreet', 'All'], 18, 1)

    detail_w = 420
    tree_w = W - detail_w
    y = column_header(d, y, [(0, 640, 'Name'), (640, 760, 'Type'),
                             (760, 900, 'Status'), (900, 1010, 'Size'),
                             (1010, tree_w, 'Modified')])
    tree_rows(d, y, tree_w, hover='rain_streaks_512',
              cols_w=(650, 772, 1000, tree_w - 12))
    detail_pane(d, W - detail_w, SELECTED, IMAGE, 'Ready', '3840x2160',
                '2026-09-24 17:40', 118)
    status_bar(d, 'Ready', '0 warnings  ·  0 errors')
    im.save(path)


def improvement_b(path):
    """Improvement B - full-width tree with a fixed inline inspector strip."""
    im = grad((W, H), (22, 24, 28), (15, 16, 19))
    d = ImageDraw.Draw(im)
    panel_frame(d, 'improvement B  ·  full width + inline strip')

    y = tool_header(d, 34)
    y = context_row(d, y, ['Project', 'NeonStreet', 'All'], 18, 1)
    y = column_header(d, y, [(0, 700, 'Name'), (700, 860, 'Type'),
                             (860, 1000, 'Status'), (1000, 1140, 'Size'),
                             (1140, W - 14, 'Modified')])
    strip_h = 92
    tree_rows(d, y, W, hover='rain_streaks_512',
              cols_w=(710, 872, 1130, W - 14))
    sy = H - 30 - strip_h
    d.rectangle((0, sy, W, H - 30), fill=(28, 31, 36))
    d.line((0, sy, W, sy), fill=(66, 72, 80))
    preview_tile(d, (14, sy + 10, 106, sy + 82), seed=2)
    txt(d, (122, sy + 26), SELECTED, TEXT, B13)
    txt(d, (122, sy + 48), 'image  ·  Ready  ·  3840x2160', MUTED, F11)
    txt(d, (122, sy + 68), 'Project/NeonStreet/Footage', FAINT, F11)
    for i, label in enumerate(('Reveal', 'Rename', 'Reload', 'Delete')):
        bx = 760 + i * 92
        rr(d, (bx, sy + 30, bx + 84, sy + 58), 5, PANEL_2, LINE)
        txt(d, (bx + 42, sy + 44), label, SOFT, F11, 'mm')
    status_bar(d, 'Ready', '0 warnings  ·  0 errors')
    im.save(path)



def improvement_c(path):
    """Improvement C - tile grid for visual assets, tree kept for structure."""
    im = grad((W, H), (22, 24, 28), (15, 16, 19))
    d = ImageDraw.Draw(im)
    panel_frame(d, 'improvement C  ·  tile grid, quiet chips')

    y = tool_header(d, 34, mode='Tile', unused=False)
    y = context_row(d, y, ['Project', 'NeonStreet', 'Footage'], 4, 1)

    cards = [('city_plate_4k.png', IMAGE, 'Ready', '3840x2160', 0),
             ('city_plate_4k_v02.png', IMAGE, 'Ready', '3840x2160', 1),
             ('rain_streaks_512', SEQ, 'Ready', '512x512 · 24f', 2),
             ('scanline_overlay.png', IMAGE, 'Draft', '1920x1080', 3),
             ('main_title', COMP, 'Ready', '6.0s', 1),
             ('subtitle_kit', COMP, 'Ready', '3.5s', 0),
             ('credit_roll', TEXT_L, 'Draft', '2.0s', 2),
             ('city_ambience.wav', AUDIO, 'Ready', '48kHz · 12.0s', 3)]
    cols, cw, ch, gap = 5, 288, 218, 14
    for i, (name, kind, status, meta, seed) in enumerate(cards):
        cx = 14 + (i % cols) * (cw + gap)
        cy = y + 14 + (i // cols) * (ch + gap)
        sel = (name == SELECTED)
        d.rectangle((cx, cy, cx + cw, cy + ch), fill=SEL if sel else PANEL,
                    outline=SEL_EDGE if sel else LINE)
        if kind == AUDIO:
            d.rectangle((cx + 1, cy + 1, cx + cw - 1, cy + 128), fill=(24, 26, 30))
            glyph_wave(d, cx + cw / 2 - 22, cy + 58, 44, (206, 152, 178))
        elif kind in (COMP, TEXT_L):
            d.rectangle((cx + 1, cy + 1, cx + cw - 1, cy + 128), fill=(24, 26, 30))
            glyph_doc(d, cx + cw / 2 - 9, cy + 52, 18, TYPE_COLOR[kind])
            txt(d, (cx + cw / 2 + 22, cy + 64), kind, MUTED, F11, 'lm')
        else:
            preview_tile(d, (cx + 1, cy + 1, cx + cw - 1, cy + 128), seed=seed)
        txt(d, (cx + 12, cy + 148), name, TEXT if sel else SOFT, B12 if sel else F12)
        txt(d, (cx + 12, cy + 170), meta, MUTED, F11)
        status_dot(d, cx + 16, cy + 194, STATUS_COLOR[status])
        txt(d, (cx + 26, cy + 194), status, STATUS_COLOR[status], F11)
        txt(d, (cx + cw - 12, cy + 194), kind, FAINT, F11, 'rm')
    status_bar(d, 'Tile view  ·  Footage', '0 warnings  ·  0 errors')
    im.save(path)


def improvement_d(path):
    """Improvement D - no-project state, one message instead of two."""
    im = grad((W, H), (22, 24, 28), (15, 16, 19))
    d = ImageDraw.Draw(im)
    panel_frame(d, 'improvement D  ·  single empty state')
    y = tool_header(d, 34, unused=False)
    y = context_row(d, y, ['Project', 'No project'], 0, 0)
    d.rectangle((0, y, W, H - 30), fill=(21, 23, 27))
    empty_state(d, (0, y, W, H - 30), 'No project open',
                'Open a project or create a composition to populate Project View.',
                'Open project...')
    status_bar(d, 'No project', '0 warnings  ·  0 errors')
    im.save(path)


if __name__ == '__main__':
    improvement_a(OUT / 'project-view-improvement-a-dense-tree-2026-09-26.png')
    improvement_b(OUT / 'project-view-improvement-b-inline-strip-2026-09-26.png')
    improvement_c(OUT / 'project-view-improvement-c-tile-grid-2026-09-26.png')
    improvement_d(OUT / 'project-view-improvement-d-empty-state-2026-09-26.png')
    print('project view improvement mockups written to', OUT)

