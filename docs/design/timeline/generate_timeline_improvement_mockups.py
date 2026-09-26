from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

"""Timeline improvement mockups (variant A / B / C) - concept only.

Writes NEW image files only. The approved mockups in this folder are treated as
read-only references: never overwritten, never resized on disk, never deleted.
"""

OUT = Path(__file__).parent
W, H = 1832, 858

TOP = 88          # first row below the tool bar
HEAD = 30         # column header height
RULER, CACHE, WORK = 26, 10, 16
ROWS_TOP = TOP + HEAD + RULER + CACHE + WORK
BOTTOM = H - 64   # top edge of the transport bar
NAV_H = 96

F0, F1, FPS = 0, 288, 24
CURRENT = 96

BG = (17, 19, 22)
BODY = (19, 21, 25)
PANEL = (26, 29, 33)
PANEL_ALT = (24, 27, 31)
PANEL_2 = (33, 37, 42)
TRACK = (22, 24, 28)
TRACK_SEL = (30, 40, 58)
ROW_LINE = (40, 44, 50)
LINE = (58, 63, 70)
TEXT = (231, 233, 236)
TEXT_DIM = (186, 191, 198)
MUTED = (148, 154, 162)
AMBER = (240, 174, 66)
ICE = (150, 220, 235)
SILVER = (196, 201, 207)
SEL_BG = (32, 45, 68)

REG = [Path('C:/Windows/Fonts/segoeui.ttf'), Path('C:/Windows/Fonts/arial.ttf')]
SBD = [Path('C:/Windows/Fonts/seguisb.ttf'), Path('C:/Windows/Fonts/arialbd.ttf')]


def font(size, bold=False):
    for p in (SBD if bold else REG):
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


F10, F11, F12, F13 = [font(x) for x in (10, 11, 12, 13)]
B10, B11, B12, B13, B15, B18 = [font(x, True) for x in (10, 11, 12, 13, 15, 18)]


def grad(size, top, bottom):
    im = Image.new('RGB', size, top)
    d = ImageDraw.Draw(im)
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        d.line((0, y, size[0], y),
               fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    return im


def rr(d, box, radius=6, fill=PANEL, outline=LINE, width=1):
    d.rounded_rectangle(box, radius, fill=fill, outline=outline, width=width)


def txt(d, xy, value, fill=TEXT, f=F12, anchor=None):
    d.text(xy, value, fill=fill, font=f, anchor=anchor)


def tc(frame, fps=FPS):
    s, f = divmod(frame, fps)
    m, s = divmod(s, 60)
    return '%02d:%02d:%02d' % (m, s, f)


def tx(x0, x1, f0, f1, frame):
    return x0 + (x1 - x0) * (frame - f0) / float(f1 - f0)


def note(d, box, value, color=AMBER):
    rr(d, box, 4, (30, 26, 18), color)
    txt(d, ((box[0] + box[2]) // 2, (box[1] + box[3]) // 2), value, color, F11, 'mm')


def chrome(d, title, subtitle, tools):
    d.rectangle((0, 0, W, 40), fill=(23, 25, 29))
    d.line((0, 39, W, 39), fill=LINE)
    txt(d, (18, 20), 'ArtifactStudio', TEXT, B13, 'lm')
    d.line((128, 10, 128, 30), fill=LINE)
    for i, t in enumerate(['Project', 'Composition Viewer', 'Inspector', 'Timeline']):
        x = 142 + i * 116
        active = (t == 'Timeline')
        txt(d, (x, 20), t, TEXT if active else MUTED, B12 if active else F12, 'lm')
        if active:
            d.line((x, 34, x + 104, 34), fill=AMBER, width=2)
    txt(d, (W - 20, 20), subtitle, MUTED, F12, 'rm')

    d.rectangle((0, 40, W, 88), fill=(29, 32, 36))
    d.line((0, 87, W, 87), fill=LINE)
    txt(d, (18, 64), title, AMBER, B13, 'lm')
    x = 18 + len(title) * 9 + 26
    d.line((x - 14, 50, x - 14, 78), fill=LINE)
    for label, on in tools:
        w = 12 + len(label) * 7
        rr(d, (x, 50, x + w, 78), 5, (56, 44, 22) if on else (34, 37, 42),
           AMBER if on else LINE)
        txt(d, (x + w // 2, 64), label, AMBER if on else TEXT, F12, 'mm')
        x += w + 8


def ruler(d, x0, x1, y, h, step=6):
    d.rectangle((x0, y, x1, y + h), fill=PANEL_2)
    d.line((x0, y + h - 1, x1, y + h - 1), fill=LINE)
    f = F0
    while f <= F1:
        x = tx(x0, x1, F0, F1, f)
        major = (f % 24 == 0)
        d.line((x, y + 5, x, y + h - 1), fill=(96, 102, 110) if major else LINE)
        if major:
            txt(d, (x + 5, y + 14), tc(f), MUTED, F11)
        f += step


def cache_bar(d, x0, x1, y, h):
    d.rectangle((x0, y, x1, y + h), fill=(23, 25, 29))
    for a, b, c in [(0, 288, (92, 60, 128)), (0, 168, (70, 106, 148))]:
        d.rectangle((tx(x0, x1, F0, F1, a), y + 1,
                     tx(x0, x1, F0, F1, b), y + h - 1), fill=c)
    d.line((x0, y + h - 1, x1, y + h - 1), fill=LINE)


def work_area(d, x0, x1, y, h, a, b):
    d.rectangle((x0, y, x1, y + h), fill=(29, 32, 37))
    xa, xb = tx(x0, x1, F0, F1, a), tx(x0, x1, F0, F1, b)
    d.rectangle((xa, y + 2, xb, y + h - 2), fill=(50, 55, 63), outline=(82, 88, 98))
    d.rectangle((xa, y + 2, xa + 5, y + h - 2), fill=SILVER)
    d.rectangle((xb - 5, y + 2, xb, y + h - 2), fill=SILVER)


def time_header(d, rail, y):
    ruler(d, rail, W, y, RULER)
    cache_bar(d, rail, W, y + RULER, CACHE)
    work_area(d, rail, W, y + RULER + CACHE, WORK, 8, 280)
    return y + RULER + CACHE + WORK


def pane_header(d, cols, rail, y=TOP):
    d.rectangle((0, y, rail, y + HEAD), fill=(31, 34, 39))
    d.line((0, y + HEAD - 1, W, y + HEAD - 1), fill=LINE)
    for x0, x1, label in cols:
        txt(d, (x0 + 8, y + HEAD // 2), label, MUTED, B11, 'lm')
        if x0:
            d.line((x0, y + 6, x0, y + HEAD - 6), fill=(70, 75, 82))
    return y + HEAD


def playhead(d, x, y0, y1):
    d.line((x, y0, x, y1), fill=ICE, width=2)
    d.polygon([(x - 8, y0 + 2), (x + 8, y0 + 2), (x + 5, y0 + 15), (x - 5, y0 + 15)],
              fill=(178, 183, 190))
    d.line((x, y0 + 5, x, y0 + 14), fill=ICE, width=2)


def keyframe(d, x, y, filled=True):
    c = AMBER if filled else (120, 98, 52)
    d.polygon([(x, y - 5), (x + 5, y), (x, y + 5), (x - 5, y)], fill=c,
              outline=(240, 242, 245) if filled else LINE)


def state_icons(d, x, y, L):
    if L.get('solo'):
        d.ellipse((x - 6, y - 6, x + 6, y + 6), outline=AMBER, width=2)
        x += 19
    if L.get('lock'):
        d.rectangle((x - 5, y - 2, x + 5, y + 6), fill=SILVER)
        d.arc((x - 4, y - 10, x + 4, y), 180, 360, fill=SILVER, width=2)
        x += 19
    if L.get('shy'):
        d.ellipse((x - 5, y - 5, x + 5, y + 5), outline=(128, 134, 142), width=2)
        x += 19
    return x



def clip_bar(d, rail, y, h, L, selected, color=None):
    xa = tx(rail, W, F0, F1, L['a'])
    xb = tx(rail, W, F0, F1, L['b'])
    rr(d, (xa, y + 4, xb, y + h - 4), 3,
       color or ((58, 64, 72) if selected else (48, 52, 58)),
       AMBER if selected else (76, 82, 90), 2 if selected else 1)
    return xa, xb


def clip_body(d, xa, xb, y, h, L, selected, chip=False):
    """Clip label plus optional value chip, laid out so the two never overlap."""
    left, right = xa + 10, xb - 8
    if chip and (right - left) > 140:
        cw = 58
        rr(d, (left, y + h // 2 - 11, left + cw, y + h // 2 + 11), 3,
           (24, 26, 30), (98, 104, 112))
        txt(d, (left + cw // 2, y + h // 2), L['val'], AMBER, F11, 'mm')
        left += cw + 16
    if (right - left) > 46:
        txt(d, (left, y + h // 2), L['name'],
            (232, 236, 240) if selected else (178, 184, 191), F12, 'lm')


def navigator(d, x0, x1, y, h, layers):
    d.rectangle((x0, y, x1, y + h), fill=(24, 26, 30))
    d.line((x0, y, x1, y), fill=LINE)
    d.line((x0, y + h - 1, x1, y + h - 1), fill=LINE)
    txt(d, (x0 + 10, y + 16), 'NAVIGATOR', MUTED, B10, 'lm')
    step = (h - 38) / float(len(layers))
    for i, L in enumerate(layers):
        yy = int(y + 28 + i * step)
        d.rectangle((tx(x0, x1, F0, F1, L['a']), yy,
                     tx(x0, x1, F0, F1, L['b']), yy + 3), fill=(100, 106, 114))
    d.rectangle((tx(x0, x1, F0, F1, 8), y + 26,
                 tx(x0, x1, F0, F1, 280), y + h - 4), outline=SILVER, width=1)
    cx = tx(x0, x1, F0, F1, CURRENT)
    d.line((cx, y + 26, cx, y + h - 4), fill=ICE, width=2)


def transport(d, y, badge=None):
    d.rectangle((0, y, W, H), fill=(23, 25, 29))
    d.line((0, y, W, y), fill=LINE)
    x = 16
    for label in ['|<', '<<', 'Play', '>>', '>|']:
        on = (label == 'Play')
        w = 20 + len(label) * 8
        rr(d, (x, y + 10, x + w, y + 46), 5, (58, 45, 22) if on else (34, 37, 42),
           AMBER if on else LINE)
        txt(d, (x + w // 2, y + 28), label, AMBER if on else TEXT, B12, 'mm')
        x += w + 6
    txt(d, (x + 26, y + 28), tc(CURRENT), AMBER, B18, 'lm')
    txt(d, (x + 112, y + 28), '24 fps', MUTED, F11, 'lm')
    d.line((x + 186, y + 10, x + 186, y + 46), fill=LINE)
    txt(d, (x + 204, y + 19), 'Loop    In 00:00:00    Out 00:00:00', MUTED, F11)
    txt(d, (x + 204, y + 37), 'Range 00:00:00 - 00:00:12', MUTED, F11)
    if badge:
        note(d, (W - 500, y + 12, W - 16, y + 44), badge)


def base(d, title, subtitle, tools, badge):
    chrome(d, title, subtitle, tools)
    d.rectangle((0, TOP, W, BOTTOM), fill=BODY)
    d.rectangle((0, BOTTOM, W, H), fill=(23, 25, 29))




LAYERS = [
    dict(name='BG_Sky', kind='Image 1920x1080', a=0, b=288, solo=False, lock=False,
         shy=True, val='0, 0'),
    dict(name='City_Glow', kind='Image 1920x1080', a=12, b=264, solo=False, lock=False,
         shy=False, val='-40, 0', pos=[(12, 0), (84, -12), (156, 0), (228, -8)],
         scale=[(12, 100), (156, 112)]),
    dict(name='Title_Main', kind='Text / Inter', a=48, b=192, solo=True, lock=True,
         shy=False, val='960, 540', pos=[(48, 960), (96, 940), (192, 960)],
         scale=[(48, 100), (96, 108)], rot=[(48, 0), (192, 0)]),
    dict(name='Logo_Loop', kind='Shape layer', a=24, b=288, solo=False, lock=False,
         shy=True, val='0, 0', rot=[(24, 0), (264, 360)]),
    dict(name='Subtitle', kind='Text / Inter', a=60, b=180, solo=False, lock=False,
         shy=False, val='960, 640', pos=[(60, 960), (180, 960)]),
    dict(name='Grain_Overlay', kind='Image 1024x1024', a=0, b=288, solo=False, lock=False,
         shy=False, val='0, 0', opa=[(0, 0), (36, 22), (252, 22), (288, 0)]),
    dict(name='Vignette', kind='Shape layer', a=0, b=288, solo=False, lock=False,
         shy=True, val='0, 0'),
    dict(name='FX_Glow', kind='Effect layer', a=96, b=240, solo=False, lock=False,
         shy=False, val='0, 0', opa=[(96, 0), (120, 100), (216, 100), (240, 0)]),
    dict(name='Matte_Wipe', kind='Matte layer', a=24, b=252, solo=False, lock=True,
         shy=False, val='0, 0', pos=[(24, 0), (252, 192)]),
    dict(name='Audio_1', kind='WAV 48kHz', a=0, b=240, solo=False, lock=False,
         shy=False, val='0, 0', vol=[(0, 0), (120, -3)]),
]

PROPS = [('Position', 'pos'), ('Scale', 'scale'), ('Rotation', 'rot')]


def row_bg(d, i, selected, y, h, rail):
    left = SEL_BG if selected else (PANEL if i % 2 == 0 else PANEL_ALT)
    right = TRACK_SEL if selected else TRACK
    d.rectangle((0, y, rail, y + h), fill=left)
    d.rectangle((rail, y, W, y + h), fill=right)
    d.line((0, y + h, W, y + h), fill=ROW_LINE)


def variant_a(path):
    """Slim left rail, the property value moves onto the clip as a chip."""
    im = grad((W, H), (22, 24, 28), (14, 15, 18))
    d = ImageDraw.Draw(im)
    base(d, 'IMPROVEMENT A  -  Slim rail / value chip',
         'concept only  -  not an approved spec',
         [('Snap', True), ('Keyframe', False), ('Ease', False), ('Marker', False)],
         'Rail 268px  -  value shown on the clip, not in the left pane')

    rail, lh, ph = 268, 40, 24
    d.rectangle((0, TOP, rail, BOTTOM), fill=PANEL)
    d.line((rail - 1, TOP, rail - 1, BOTTOM), fill=LINE)
    pane_header(d, [(0, 186, 'Layer'), (186, rail, 'State')], rail)
    time_header(d, rail, TOP + HEAD)

    y = ROWS_TOP
    for i, L in enumerate(LAYERS):
        sel = (L['name'] == 'Title_Main')
        row_bg(d, i, sel, y, lh, rail)
        txt(d, (14, y + lh // 2), L['name'], TEXT if sel else TEXT_DIM,
            B12 if sel else F12, 'lm')
        state_icons(d, 200, y + lh // 2, L)
        xa, xb = clip_bar(d, rail, y, lh, L, sel)
        clip_body(d, xa, xb, y, lh, L, sel, chip=True)
        y += lh
        if sel:
            for pname, pkey in PROPS:
                d.rectangle((0, y, rail, y + ph), fill=(22, 25, 29))
                d.rectangle((rail, y, W, y + ph), fill=BODY)
                d.line((0, y + ph, W, y + ph), fill=(36, 40, 45))
                txt(d, (32, y + ph // 2), pname, MUTED, F11, 'lm')
                for kf in L.get(pkey, []):
                    keyframe(d, tx(rail, W, F0, F1, kf[0]), y + ph // 2)
                y += ph

    d.line((rail - 1, TOP, rail - 1, BOTTOM), fill=LINE)
    navigator(d, rail, W, BOTTOM - 12 - NAV_H, NAV_H, LAYERS)
    playhead(d, tx(rail, W, F0, F1, CURRENT), TOP + HEAD, BOTTOM - 12 - NAV_H)
    transport(d, BOTTOM)
    im.save(path)


def variant_b(path):
    """Wider rail with dedicated value / parent / blend columns."""
    im = grad((W, H), (22, 24, 28), (14, 15, 18))
    d = ImageDraw.Draw(im)
    base(d, 'IMPROVEMENT B  -  Property column split',
         'concept only  -  not an approved spec',
         [('Snap', True), ('Keyframe', True), ('Ease', True), ('Link', False)],
         'Rail 356px  -  name / value / parent / blend as separate columns')

    rail, lh, ph = 356, 40, 24
    d.rectangle((0, TOP, rail, BOTTOM), fill=PANEL)
    d.line((rail - 1, TOP, rail - 1, BOTTOM), fill=LINE)
    pane_header(d, [(0, 166, 'Layer'), (166, 252, 'Value'),
                    (252, 302, 'Parent'), (302, rail, 'Blend')], rail)
    time_header(d, rail, TOP + HEAD)

    y = ROWS_TOP
    for i, L in enumerate(LAYERS):
        sel = (L['name'] == 'Title_Main')
        row_bg(d, i, sel, y, lh, rail)
        txt(d, (14, y + lh // 2), L['name'], TEXT if sel else TEXT_DIM,
            B12 if sel else F12, 'lm')
        txt(d, (172, y + lh // 2), L['val'], AMBER, F11, 'lm')
        txt(d, (258, y + lh // 2), 'Comp1', MUTED, F11, 'lm')
        rr(d, (308, y + 12, 350, y + lh - 12), 3, (34, 37, 42), LINE)
        txt(d, (329, y + lh // 2), 'Normal', MUTED, F11, 'mm')
        xa, xb = clip_bar(d, rail, y, lh, L, sel)
        clip_body(d, xa, xb, y, lh, L, sel)
        y += lh
        if sel:
            for pname, pkey in PROPS:
                d.rectangle((0, y, rail, y + ph), fill=(22, 25, 29))
                d.rectangle((rail, y, W, y + ph), fill=BODY)
                d.line((0, y + ph, W, y + ph), fill=(36, 40, 45))
                txt(d, (32, y + ph // 2), pname, MUTED, F11, 'lm')
                txt(d, (172, y + ph // 2), L['val'], (198, 202, 208), F11, 'lm')
                for kf in L.get(pkey, []):
                    keyframe(d, tx(rail, W, F0, F1, kf[0]), y + ph // 2)
                y += ph

    d.line((rail - 1, TOP, rail - 1, BOTTOM), fill=LINE)
    navigator(d, rail, W, BOTTOM - 12 - NAV_H, NAV_H, LAYERS)
    playhead(d, tx(rail, W, F0, F1, CURRENT), TOP + HEAD, BOTTOM - 12 - NAV_H)
    transport(d, BOTTOM)
    im.save(path)


def variant_c(path):
    """Exactly one row per layer; properties live in a fixed footer strip."""
    im = grad((W, H), (22, 24, 28), (14, 15, 18))
    d = ImageDraw.Draw(im)
    base(d, 'IMPROVEMENT C  -  Single row + footer strip',
         'concept only  -  not an approved spec',
         [('Snap', True), ('Keyframe', True), ('Ease', True), ('Dope', False)],
         'One row per layer  -  properties in a fixed footer, never inline')

    rail, lh = 300, 32
    foot_top = BOTTOM - 180
    nav_h = 76
    nav_top = foot_top - 12 - nav_h
    d.rectangle((0, TOP, rail, foot_top), fill=PANEL)
    d.line((rail - 1, TOP, rail - 1, foot_top), fill=LINE)
    pane_header(d, [(0, 194, 'Layer'), (194, 252, 'Type'), (252, rail, 'Keys')], rail)
    time_header(d, rail, TOP + HEAD)

    sel = LAYERS[2]
    y = ROWS_TOP
    for i, L in enumerate(LAYERS):
        on = (L['name'] == sel['name'])
        row_bg(d, i, on, y, lh, rail)
        txt(d, (14, y + lh // 2), L['name'], TEXT if on else TEXT_DIM,
            B12 if on else F12, 'lm')
        txt(d, (200, y + lh // 2), L['kind'][:10], MUTED, F10, 'lm')
        keys = sum(len(v) for v in L.values() if isinstance(v, list))
        txt(d, (rail - 10, y + lh // 2), str(keys), AMBER if keys else MUTED, F11, 'rm')
        xa, xb = clip_bar(d, rail, y, lh, L, on)
        clip_body(d, xa, xb, y, lh, L, on)
        for pkey in ('pos', 'scale', 'rot'):
            for kf in L.get(pkey, []):
                keyframe(d, tx(rail, W, F0, F1, kf[0]), y + lh - 9)
        y += lh

    d.rectangle((0, foot_top, W, BOTTOM), fill=(24, 27, 31))
    d.line((0, foot_top, W, foot_top), fill=(84, 90, 98), width=2)
    txt(d, (16, foot_top + 22), 'PROPERTIES  -  ' + sel['name'], AMBER, B11, 'lm')
    d.line((252, foot_top + 10, 252, foot_top + 34), fill=LINE)
    for i, label in enumerate(['Keyframe', 'Ease in', 'Ease out', 'Reset']):
        bx = 270 + i * 104
        rr(d, (bx, foot_top + 8, bx + 96, foot_top + 36), 4, (34, 37, 42), LINE)
        txt(d, (bx + 48, foot_top + 22), label, TEXT, F11, 'mm')
    for j, (pname, pkey) in enumerate(PROPS):
        ry = foot_top + 50 + j * 42
        d.rectangle((14, ry, W - 14, ry + 36), fill=(29, 32, 37), outline=(54, 59, 66))
        txt(d, (28, ry + 18), pname, TEXT, F12, 'lm')
        rr(d, (116, ry + 8, 244, ry + 28), 3, (22, 24, 28), (98, 104, 112))
        txt(d, (180, ry + 18), sel['val'], AMBER, F12, 'mm')
        d.line((256, ry + 8, 256, ry + 28), fill=LINE)
        txt(d, (268, ry + 18), 'X  940.0', (200, 204, 210), F11, 'lm')
        txt(d, (364, ry + 18), 'Y  540.0', (200, 204, 210), F11, 'lm')
        d.line((462, ry + 8, 462, ry + 28), fill=LINE)
        for kf in sel.get(pkey, []):
            keyframe(d, tx(470, W - 24, F0, F1, kf[0]), ry + 18)

    navigator(d, rail, W, nav_top, nav_h, LAYERS)
    playhead(d, tx(rail, W, F0, F1, CURRENT), TOP + HEAD, nav_top)
    transport(d, BOTTOM)
    im.save(path)


if __name__ == '__main__':
    variant_a(OUT / 'timeline-improve-a-slim-rail-2026-09-26.png')
    variant_b(OUT / 'timeline-improve-b-split-column-2026-09-26.png')
    variant_c(OUT / 'timeline-improve-c-footer-strip-2026-09-26.png')
