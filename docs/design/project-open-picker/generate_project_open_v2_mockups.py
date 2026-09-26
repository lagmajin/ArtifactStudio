"""Project Open Picker v2 mockups (concept).

Draws NEW image files only. The existing approved reference
`project-open-picker-dcc-concept-2026-09-12.png` is read-only and is never
overwritten, resized on disk, or deleted.

Outputs
  project-open-v2-candidate-2026-09-26.png   the proposed v2 screen
  project-open-v2-before-after-2026-09-26.png current runtime vs v2
"""

from PIL import Image, ImageDraw, ImageFont
from pathlib import Path

OUT = Path(__file__).parent
W, H = 1586, 992

BG = (17, 19, 22)
PANEL = (26, 29, 33)
PANEL_2 = (33, 37, 42)
PANEL_3 = (42, 46, 52)
LINE = (58, 63, 70)
TEXT = (231, 233, 236)
DIM = (206, 210, 215)
MUTED = (148, 154, 162)
FAINT = (110, 116, 124)
AMBER = (240, 174, 66)
ICE = (150, 220, 235)
SEL = (32, 45, 68)
GREEN = (76, 186, 120)
RED = (222, 88, 82)
CYAN = (86, 190, 214)
PURPLE = (150, 106, 214)
YELLOW = (232, 200, 96)

REG = [Path('C:/Windows/Fonts/segoeui.ttf'), Path('C:/Windows/Fonts/arial.ttf')]
SBD = [Path('C:/Windows/Fonts/seguisb.ttf'), Path('C:/Windows/Fonts/arialbd.ttf')]


def font(size, bold=False):
    for p in (SBD if bold else REG):
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


F9, F10, F11, F12, F13 = [font(x) for x in (9, 10, 11, 12, 13)]
B10, B11, B12, B13, B15, B17 = [font(x, True) for x in (10, 11, 12, 13, 15, 17)]


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


def checkbox(d, x, y, label, on, color=ICE, f=F12, label_fill=None):
    if on:
        d.rounded_rectangle((x, y, x + 14, y + 14), 3, fill=(38, 58, 74), outline=color)
        d.line([(x + 3, y + 7), (x + 6, y + 10)], fill=color, width=2)
        d.line([(x + 6, y + 10), (x + 11, y + 4)], fill=color, width=2)
    else:
        d.rounded_rectangle((x, y, x + 14, y + 14), 3, fill=(26, 29, 33),
                            outline=(78, 84, 92))
    txt(d, (x + 20, y + 7), label, label_fill or (DIM if on else MUTED), f, 'lm')


def chevron(d, x, y, color=MUTED, right=True):
    if right:
        d.line([(x - 2, y - 4), (x + 2, y), (x - 2, y + 4)], fill=color, width=1)
    else:
        d.line([(x + 2, y - 4), (x - 2, y), (x + 2, y + 4)], fill=color, width=1)


def icon_button(d, box, glyph, tint=MUTED, active=False):
    rr(d, box, 5, (40, 44, 50) if active else PANEL_2,
       (92, 98, 106) if active else LINE)
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    if glyph == 'search':
        d.ellipse((cx - 4, cy - 4, cx + 3, cy + 3), outline=tint, width=1)
        d.line((cx + 3, cy + 3, cx + 7, cy + 7), fill=tint, width=1)
    elif glyph == 'grid':
        for oy in (-6, 1):
            for ox in (-7, 1):
                d.rectangle((cx + ox, cy + oy, cx + ox + 5, cy + oy + 4),
                            outline=tint, width=1)
    elif glyph == 'list':
        for oy in (-6, -1, 4):
            d.rectangle((cx - 7, cy + oy, cx - 3, cy + oy + 3), fill=tint)
            d.rectangle((cx - 1, cy + oy, cx + 7, cy + oy + 3), outline=tint, width=1)
    elif glyph == 'up':
        d.line([(cx, cy + 5), (cx, cy - 5)], fill=tint, width=1)
        d.line([(cx - 4, cy - 1), (cx, cy - 5)], fill=tint, width=1)
        d.line([(cx + 4, cy - 1), (cx, cy - 5)], fill=tint, width=1)
    elif glyph == 'back':
        d.line([(cx + 4, cy - 5), (cx - 4, cy)], fill=tint, width=1)
        d.line([(cx - 4, cy), (cx + 4, cy + 5)], fill=tint, width=1)
    elif glyph == 'fwd':
        d.line([(cx - 4, cy - 5), (cx + 4, cy)], fill=tint, width=1)
        d.line([(cx + 4, cy), (cx - 4, cy + 5)], fill=tint, width=1)
    elif glyph == 'refresh':
        d.arc((cx - 6, cy - 6, cx + 6, cy + 6), 40, 320, fill=tint, width=1)
    elif glyph == 'star':
        pts = []
        for i in range(10):
            import math
            a = -math.pi / 2 + i * math.pi / 5
            r = 7 if i % 2 == 0 else 3
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        d.line(pts + [pts[0]], fill=tint, width=1)
    elif glyph == 'clock':
        d.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), outline=tint, width=1)
        d.line((cx, cy - 3, cx, cy), fill=tint, width=1)
        d.line((cx, cy, cx + 3, cy + 2), fill=tint, width=1)
    elif glyph == 'warn':
        d.polygon([(cx, cy - 7), (cx + 7, cy + 6), (cx - 7, cy + 6)], outline=tint, width=1)
        d.line((cx, cy - 2, cx, cy + 2), fill=tint, width=1)


def search_field(d, x0, y, x1, placeholder, focused=False):
    rr(d, (x0, y, x1, y + 32), 4, (21, 23, 27), (96, 102, 110) if focused else LINE)
    d.ellipse((x0 + 12, y + 13, x0 + 19, y + 20), outline=FAINT, width=1)
    d.line((x0 + 19, y + 20, x0 + 23, y + 24), fill=FAINT, width=1)
    txt(d, (x0 + 34, y + 16), placeholder, FAINT, F12, 'lm')


def thumbnail(d, box, kind):
    """Synthetic composition preview. Never a real screenshot."""
    x0, y0, x1, y1 = box
    d.rectangle((x0, y0, x1, y1), fill=(30, 34, 40))
    w, h = x1 - x0, y1 - y0
    if kind == 'lunar':
        d.rectangle((x0, y0, x1, y0 + h * 0.55), fill=(28, 32, 44))
        d.ellipse((x0 + w * 0.16, y0 + h * 0.16, x0 + w * 0.34, y0 + h * 0.52),
                  fill=(176, 182, 196))
        d.ellipse((x0 + w * 0.20, y0 + h * 0.21, x0 + w * 0.30, y0 + h * 0.47),
                  fill=(30, 34, 44))
        d.rectangle((x0, y0 + h * 0.55, x1, y1), fill=(52, 50, 48))
        for i in range(6):
            d.line((x0, y0 + h * (0.62 + i * 0.06), x1, y0 + h * (0.60 + i * 0.06)),
                   fill=(64, 62, 60))
    elif kind == 'neon':
        d.rectangle((x0, y0, x1, y1), fill=(24, 26, 38))
        for i, c in enumerate([(96, 150, 210), (170, 90, 190), (210, 120, 170)]):
            d.rectangle((x0 + w * (0.1 + i * 0.26), y0 + h * 0.18,
                         x0 + w * (0.2 + i * 0.26), y0 + h * 0.78), fill=c)
        d.line((x0 + w * 0.5, y0 + h * 0.08, x0 + w * 0.5, y0 + h * 0.92),
               fill=(210, 220, 240), width=2)
    elif kind == 'studio':
        d.rectangle((x0, y0, x1, y1), fill=(32, 30, 34))
        d.ellipse((x0 + w * 0.30, y0 + h * 0.20, x0 + w * 0.70, y0 + h * 0.80),
                  fill=(58, 56, 62))
        d.ellipse((x0 + w * 0.36, y0 + h * 0.26, x0 + w * 0.64, y0 + h * 0.74),
                  outline=(96, 94, 100), width=2)
    elif kind == 'cliffs':
        d.rectangle((x0, y0, x1, y0 + h * 0.5), fill=(120, 138, 158))
        d.polygon([(x0, y1), (x0 + w * 0.34, y0 + h * 0.34),
                   (x0 + w * 0.58, y1)], fill=(78, 88, 82))
        d.polygon([(x0 + w * 0.5, y1), (x0 + w * 0.74, y0 + h * 0.46),
                   (x1, y1)], fill=(96, 104, 96))
        d.rectangle((x0, y0 + h * 0.5, x1, y1), fill=(96, 112, 122))
    elif kind == 'robot':
        d.rectangle((x0, y0, x1, y1), fill=(40, 38, 44))
        d.ellipse((x0 + w * 0.34, y0 + h * 0.22, x0 + w * 0.66, y0 + h * 0.72),
                  fill=(126, 130, 138))
        d.ellipse((x0 + w * 0.41, y0 + h * 0.38, x0 + w * 0.46, y0 + h * 0.46),
                  fill=(150, 220, 235))
        d.ellipse((x0 + w * 0.54, y0 + h * 0.38, x0 + w * 0.59, y0 + h * 0.46),
                  fill=(150, 220, 235))
    elif kind == 'forest':
        d.rectangle((x0, y0, x1, y0 + h * 0.42), fill=(148, 162, 150))
        d.rectangle((x0, y0 + h * 0.42, x1, y1), fill=(64, 74, 58))
        for i in range(4):
            tx = x0 + w * (0.14 + i * 0.24)
            d.polygon([(tx, y0 + h * 0.14), (tx - w * 0.10, y0 + h * 0.58),
                       (tx + w * 0.10, y0 + h * 0.58)], fill=(48, 62, 46))
            d.line((tx, y0 + h * 0.10, tx, y0 + h * 0.58), fill=(38, 48, 38), width=2)


PROJECTS = [
    dict(name='Moonbase', path=r'D:\Projects\Client Work\Moonbase',
         thumb='lunar', opened='Today  14:32', res='3840 x 2160', dur='00:01:32',
         version='7', comps='12', assets='86', status='Healthy', health_color=GREEN,
         warn='', favorites=True, sel=True),
    dict(name='Title Sequence', path=r'D:\Projects\Client Work\Title Sequence',
         thumb='neon', opened='2025-04-28  11:20', res='1920 x 1080', dur='00:00:45',
         version='3', comps='4', assets='28', status='Healthy', health_color=GREEN,
         warn='', favorites=True, sel=False),
    dict(name='Product Spot', path=r'D:\Projects\Client Work\Product Spot',
         thumb='studio', opened='2025-04-26  16:03', res='3840 x 2160', dur='00:00:30',
         version='5', comps='6', assets='41', status='Healthy', health_color=GREEN,
         warn='', favorites=False, sel=False),
    dict(name='Archive_2025', path=r'D:\Projects\Client Work\Archive_2025',
         thumb='cliffs', opened='2025-04-18  09:14', res='4096 x 2160', dur='00:02:10',
         version='2', comps='31', assets='204', status='Missing sources', health_color=AMBER,
         warn='3 external sources', favorites=False, sel=False),
    dict(name='R&D', path=r'D:\Projects\Client Work\R&D',
         thumb='robot', opened='2025-04-12  13:27', res='1920 x 1080', dur='00:01:05',
         version='9', comps='18', assets='97', status='Healthy', health_color=GREEN,
         warn='', favorites=True, sel=False),
    dict(name='Environment Test', path=r'D:\Projects\Client Work\Environment Test',
         thumb='forest', opened='2025-03-30  10:09', res='3840 x 2160', dur='00:00:20',
         version='1', comps='3', assets='15', status='Healthy', health_color=GREEN,
         warn='', favorites=False, sel=False),
]

PLACES = [
    ('clock', 'Recent Projects', True),
    ('star', 'Favorites', False),
    (None, '—', None),
    ('folder', 'This PC', False),
    ('folder', 'Desktop', False),
    ('folder', 'Documents', False),
    (None, '—', None),
    ('grid', 'Network', False),
]


def window_chrome(d, title, w):
    d.rectangle((0, 0, w, 40), fill=(23, 25, 29))
    d.line((0, 39, w, 39), fill=LINE)
    d.polygon([(18, 13), (10, 20), (18, 27)], fill=(150, 220, 235))
    d.rectangle((24, 18, 32, 22), fill=(150, 220, 235))
    txt(d, (44, 20), title, TEXT, B12, 'lm')
    for i, g in enumerate(['—', '□', '✕']):
        txt(d, (w - 96 + i * 30, 20), g, MUTED, F11, 'mm')
    d.line((0, 40, w, 40), fill=(40, 44, 50), width=1)


def nav_toolbar(d, y, w):
    d.rectangle((0, y, w, y + 46), fill=PANEL)
    d.line((0, y + 45, w, y + 45), fill=LINE)
    cy = y + 23
    for i, g in enumerate(['back', 'fwd', 'up', 'refresh']):
        bx = 16 + i * 38
        rr(d, (bx, cy - 15, bx + 30, cy + 15), 5, PANEL_2, LINE)
        x0, y0, x1, y1 = bx, cy - 15, bx + 30, cy + 15
        cxx, cyy = (x0 + x1) // 2, (y0 + y1) // 2
        if g == 'back':
            d.line([(cxx + 4, cyy - 5), (cxx - 4, cyy)], fill=MUTED, width=1)
            d.line([(cxx - 4, cyy), (cxx + 4, cyy + 5)], fill=MUTED, width=1)
        elif g == 'fwd':
            d.line([(cxx - 4, cyy - 5), (cxx + 4, cyy)], fill=FAINT, width=1)
            d.line([(cxx + 4, cyy), (cxx - 4, cyy + 5)], fill=FAINT, width=1)
        elif g == 'up':
            d.line((cxx, cyy + 5, cxx, cyy - 5), fill=MUTED, width=1)
            d.line([(cxx - 4, cyy - 1), (cxx, cyy - 5)], fill=MUTED, width=1)
            d.line([(cxx + 4, cyy - 1), (cxx, cyy - 5)], fill=MUTED, width=1)
        else:
            d.arc((cxx - 6, cyy - 6, cxx + 6, cyy + 6), 40, 320, fill=MUTED, width=1)
    d.line((166, cy - 9, 166, cy + 9), fill=LINE)
    crx = 182
    for label in ['D:\\', 'Projects', 'Client Work']:
        txt(d, (crx, cy), label, DIM, F12, 'lm')
        crx += len(label) * 7 + 14
        chevron(d, crx - 4, cy, FAINT)
        crx += 12
    rr(d, (182, cy - 16, 800, cy + 16), 4, (21, 23, 27), LINE)
    crx = 196
    for label in ['D:\\', 'Projects', 'Client Work']:
        txt(d, (crx, cy), label, TEXT, F12, 'lm')
        crx += len(label) * 7 + 14
        chevron(d, crx - 4, cy, FAINT)
        crx += 12
    search_field(d, 824, cy - 16, 1080, 'Search projects…', False)
    rr(d, (1096, cy - 16, 1128, cy + 16), 5, PANEL_2, (96, 102, 110))
    for oy in (-6, 1):
        for ox in (-7, 1):
            d.rectangle((1112 + ox, cy + oy, 1117 + ox, cy + oy + 4),
                        outline=ICE, width=1)
    rr(d, (1140, cy - 16, 1172, cy + 16), 5, PANEL_2, LINE)
    for oy in (-6, -1, 4):
        d.rectangle((1150, cy + oy, 1154, cy + oy + 3), fill=MUTED)
        d.rectangle((1157, cy + oy, 1163, cy + oy + 3), outline=MUTED, width=1)


def sidebar(d, x0, y0, x1, y1, active_label):
    d.rectangle((x0, y0, x1, y1), fill=(22, 25, 28))
    d.line((x1 - 1, y0, x1 - 1, y1), fill=LINE)
    txt(d, (x0 + 16, y0 + 18), 'Places', MUTED, F11, 'lm')
    y = y0 + 36
    for glyph, label, on in PLACES:
        if glyph is None:
            d.line((x0 + 14, y + 8, x1 - 14, y + 8), fill=(38, 42, 47))
            y += 18
            continue
        if on:
            rr(d, (x0 + 8, y, x1 - 10, y + 32), 4, (32, 45, 68), (72, 104, 158))
            d.rectangle((x0 + 8, y + 6, x0 + 11, y + 26), fill=ICE)
        cxx, cyy = x0 + 32, y + 16
        tint = ICE if (on or label == active_label) else MUTED
        if glyph == 'star':
            import math
            pts = []
            for i in range(10):
                a = -math.pi / 2 + i * math.pi / 5
                r = 7 if i % 2 == 0 else 3
                pts.append((cxx + r * math.cos(a), cyy + r * math.sin(a)))
            d.line(pts + [pts[0]], fill=YELLOW if label == 'Favorites' else tint, width=1)
        elif glyph == 'clock':
            d.ellipse((cxx - 6, cyy - 6, cxx + 6, cyy + 6), outline=tint, width=1)
            d.line((cxx, cyy - 3, cxx, cyy), fill=tint, width=1)
            d.line((cxx, cyy, cxx + 3, cyy + 2), fill=tint, width=1)
        elif glyph == 'folder':
            d.rectangle((cxx - 7, cyy - 2, cxx + 6, cyy + 6), outline=tint, width=1)
            d.line((cxx - 7, cyy - 2, cxx - 1, cyy - 2), fill=tint, width=1)
        elif glyph == 'grid':
            for oy in (-6, 1):
                for ox in (-7, 1):
                    d.rectangle((cxx + ox, cyy + oy, cxx + ox + 5, cyy + oy + 4),
                                outline=tint, width=1)
        txt(d, (x0 + 50, cyy), label,
            TEXT if (on or label == active_label) else DIM,
            B11 if on else F11, 'lm')
        y += 36


def project_tile(d, box, p, view='grid'):
    x0, y0, x1, y1 = box
    sel = p['sel']
    d.rounded_rectangle((x0, y0, x1, y1), 6,
                        fill=SEL if sel else PANEL,
                        outline=ICE if sel else (48, 53, 60), width=2 if sel else 1)
    tx0, ty0, tx1, ty1 = x0 + 10, y0 + 10, x1 - 10, y0 + 100
    thumbnail(d, (tx0, ty0, tx1, ty1), p['thumb'])
    d.rectangle((tx0, ty0, tx1, ty1), outline=(58, 63, 70), width=1)
    if p['favorites']:
        import math
        sx, sy = x1 - 28, y0 + 24
        pts = []
        for i in range(10):
            a = -math.pi / 2 + i * math.pi / 5
            r = 7 if i % 2 == 0 else 3
            pts.append((sx + r * math.cos(a), sy + r * math.sin(a)))
        d.line(pts + [pts[0]], fill=YELLOW, width=1)
    ty = y0 + 122
    txt(d, (x0 + 12, ty), p['name'], TEXT if sel else DIM,
        B12 if sel else F12, 'lm')
    txt(d, (x0 + 12, ty + 20), p['path'], FAINT, F10, 'lm')
    d.line((x0 + 12, ty + 30, x1 - 12, ty + 30), fill=(46, 50, 56))
    txt(d, (x0 + 12, ty + 44), 'Last opened', FAINT, F10, 'lm')
    txt(d, (x0 + 12, ty + 62), p['opened'], DIM, F10, 'lm')
    txt(d, ((x0 + x1) // 2, ty + 44), 'Modified', FAINT, F10, 'mm')
    txt(d, ((x0 + x1) // 2, ty + 62), p['res'], DIM, F10, 'mm')
    txt(d, (x1 - 12, ty + 44), 'Duration', FAINT, F10, 'rm')
    txt(d, (x1 - 12, ty + 62), p['dur'], DIM, F10, 'rm')
    if p['warn']:
        ww = 0
        wc = (0, 0, 0)
        sx, sy = x0 + 12, ty + 80
        d.polygon([(sx + 7, sy), (sx + 14, sy + 12), (sx, sy + 12)], outline=AMBER, width=1)
        d.line((sx + 7, sy + 4, sx + 7, sy + 8), fill=AMBER, width=1)
        txt(d, (sx + 20, sy + 6), p['warn'], AMBER, F10, 'lm')


def inspector(d, x0, y0, x1, y1, p):
    d.rectangle((x0, y0, x1, y1), fill=(23, 26, 30))
    d.line((x0, y0, x0, y1), fill=LINE)
    px, py = x0 + 20, y0 + 18
    thumbnail(d, (px, py, x1 - 20, py + 150), p['thumb'])
    d.rectangle((px, py, x1 - 20, py + 150), outline=(58, 63, 70), width=1)
    ty = py + 178
    txt(d, (px, ty), p['name'], TEXT, B15, 'lm')
    txt(d, (px, ty + 24), p['path'], FAINT, F10, 'lm')
    ty += 52
    for label, value, color in [
            ('Project Version', p['version'], DIM),
            ('Last Saved', 'Today  14:32', DIM),
            ('Compositions', p['comps'], DIM),
            ('Assets', p['assets'], DIM)]:
        txt(d, (px, ty), label, FAINT, F11, 'lm')
        txt(d, (x1 - 20, ty), value, color, F11, 'rm')
        ty += 28
    d.line((px, ty - 14, x1 - 20, ty - 14), fill=(38, 42, 47))
    cy = ty + 6
    d.ellipse((px, cy - 5, px + 10, cy + 5), fill=p['health_color'])
    txt(d, (px + 18, cy), p['status'], DIM, F11, 'lm')
    ty = cy + 30
    d.line((px, ty - 14, x1 - 20, ty - 14), fill=(38, 42, 47))
    if p['warn']:
        ay = ty
        rr(d, (px, ay - 2, x1 - 20, ay + 32), 4, (44, 38, 30), (96, 78, 56))
        icon_button(d, (px + 10, ay + 6, px + 28, ay + 24), 'warn', AMBER)
        txt(d, (px + 38, ay + 15), p['warn'], AMBER, F11, 'lm')
        chevron(d, x1 - 34, ay + 15, AMBER)
        ty = ay + 46
    else:
        ty += 10
    ay = ty
    checkbox(d, px, ay, 'Open last composition', True, ICE)
    ay += 26
    checkbox(d, px, ay, 'Validate missing sources after open', p['warn'] != '', AMBER)
    d.rectangle((px, y1 - 58, x1 - 20, y1 - 18), fill=(28, 31, 36), outline=(48, 53, 60))
    txt(d, (px + 12, y1 - 44), 'Opening stays owned by', FAINT, F10, 'lm')
    txt(d, (px + 12, y1 - 28), 'the Project Service', FAINT, F10, 'lm')


def footer(d, y, w, h, count_label):
    d.rectangle((0, y, w, h), fill=(23, 25, 29))
    d.line((0, y, w, y), fill=LINE)
    cy = y + (h - y) // 2
    rr(d, (20, cy - 16, 178, cy + 16), 4, PANEL_2, LINE)
    txt(d, (99, cy), 'Use System Picker…', DIM, F12, 'mm')
    d.line((196, cy - 12, 196, cy + 12), fill=LINE)
    txt(d, (212, cy), count_label, MUTED, F11, 'lm')
    rr(d, (w - 320, cy - 17, w - 216, cy + 17), 4, PANEL_2, LINE)
    txt(d, (w - 268, cy), 'Cancel', DIM, F12, 'mm')
    rr(d, (w - 200, cy - 18, w - 20, cy + 18), 4, (48, 96, 148), (86, 140, 196))
    txt(d, (w - 110, cy), 'Open Project', (238, 244, 250), B12, 'mm')


def build_candidate(path):
    im = grad((W, H), (22, 24, 28), (14, 15, 18))
    d = ImageDraw.Draw(im)
    window_chrome(d, 'Open Project', W)
    nav_toolbar(d, 40, W)
    d.rectangle((0, 86, W, H), fill=(20, 23, 26))
    side_x = 178
    insp_x = W - 356
    d.rectangle((side_x, 86, insp_x, H - 62), fill=(19, 22, 25))
    sidebar(d, 0, 86, side_x, H - 62, 'Recent Projects')
    txt(d, (side_x + 20, 108), 'Recent Projects', TEXT, B13, 'lm')
    txt(d, (insp_x - 20, 108), '6 projects', MUTED, F11, 'rm')
    tile_w, tile_h, gap = 386, 214, 16
    x0, y0 = side_x + 20, 128
    for i, p in enumerate(PROJECTS):
        col, row = i % 2, i // 2
        bx = x0 + col * (tile_w + gap)
        by = y0 + row * (tile_h + gap)
        if by + tile_h > H - 78:
            break
        project_tile(d, (bx, by, bx + tile_w, by + tile_h), p)
    inspector(d, insp_x, 86, W, H - 62, PROJECTS[0])
    footer(d, H - 62, W, H, '6 projects')
    im.save(path)


def build_current(size):
    """Reconstruction of the current runtime dialog (QListWidget based)."""
    w, h = size
    im = Image.new('RGB', size, (24, 25, 28))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, w, 40), fill=(30, 31, 34))
    d.line((0, 39, w, 39), fill=(52, 54, 58))
    d.polygon([(18, 13), (10, 20), (18, 27)], fill=(150, 152, 156))
    d.rectangle((24, 18, 32, 22), fill=(150, 152, 156))
    txt(d, (44, 20), 'Open Project', (222, 224, 228), B12, 'lm')
    for i, g in enumerate(['—', '□', '✕']):
        txt(d, (w - 96 + i * 30, 20), g, MUTED, F11, 'mm')
    d.rectangle((0, 40, w, 40 + 40), fill=(26, 27, 30))
    txt(d, (20, 60), 'Open Project', (230, 232, 236), B13, 'lm')
    d.rectangle((14, 92, w - 14, 124), fill=(24, 25, 28), outline=(58, 60, 64))
    txt(d, (26, 108), 'Search recent projects…', (140, 142, 148), F12, 'lm')
    ty = 140
    d.rectangle((14, ty, 178, h - 70), fill=(25, 26, 29))
    d.line((177, ty, 177, h - 70), fill=(52, 54, 58))
    for i, label in enumerate(['Recent Projects', 'System Files…']):
        yy = ty + 18 + i * 30
        txt(d, (30, yy), label, (200, 202, 206), F12, 'lm')
    vx0, vx1 = 186, w - 320
    d.rectangle((vx0, ty, vx1, h - 70), fill=(24, 25, 28))
    for i in range(4):
        yy = ty + 16 + i * 76
        d.rounded_rectangle((vx0 + 14, yy, vx0 + 50, yy + 44), 3,
                            fill=(38, 40, 44), outline=(64, 66, 70))
        txt(d, (vx0 + 32, yy + 22), 'D', (120, 122, 126), F12, 'mm')
        txt(d, (vx0 + 64, yy + 14), 'Project_%d' % (i + 1), (214, 216, 220), F12, 'lm')
        txt(d, (vx0 + 64, yy + 34), '2025-04-18 09:14', (150, 152, 158), F10, 'lm')
    ix0 = w - 306
    d.rectangle((ix0, ty, w - 14, h - 70), fill=(27, 28, 31), outline=(58, 60, 64))
    txt(d, (ix0 + 16, ty + 20), 'Project details', (222, 224, 228), B12, 'lm')
    txt(d, (ix0 + 16, ty + 52), 'No project selected', (214, 216, 220), B12, 'lm')
    for i, line in enumerate(['Select a recent project, or',
                              'choose a project file from',
                              'the system picker.']):
        txt(d, (ix0 + 16, ty + 86 + i * 18), line, (150, 152, 158), F10, 'lm')
    d.line((ix0 + 16, ty + 156, w - 30, ty + 156), fill=(46, 48, 52))
    txt(d, (ix0 + 16, ty + 176), 'Project health and', (130, 132, 138), F10, 'lm')
    txt(d, (ix0 + 16, ty + 194), 'external-source checks', (130, 132, 138), F10, 'lm')
    txt(d, (ix0 + 16, ty + 212), 'remain owned by the', (130, 132, 138), F10, 'lm')
    txt(d, (ix0 + 16, ty + 230), 'project loading service.', (130, 132, 138), F10, 'lm')
    fy = h - 70
    d.rectangle((0, fy, w, h), fill=(26, 27, 30))
    d.line((0, fy, w, fy), fill=(52, 54, 58))
    cy = fy + 26
    rr(d, (20, cy - 16, 178, cy + 16), 4, (38, 40, 44), (66, 68, 72))
    txt(d, (99, cy), 'Use System Picker…', (206, 208, 214), F12, 'mm')
    rr(d, (w - 320, cy - 16, w - 216, cy + 16), 4, (38, 40, 44), (66, 68, 72))
    txt(d, (w - 268, cy), 'Cancel', (206, 208, 214), F12, 'mm')
    rr(d, (w - 200, cy - 17, w - 20, cy + 17), 4, (52, 54, 58), (80, 82, 86))
    txt(d, (w - 110, cy), 'Open Project', (222, 224, 228), B12, 'mm')
    return im


def build_before_after(path):
    half_w = 1586
    panel_h = 700
    im = Image.new('RGB', (half_w, panel_h * 2 + 130), (14, 15, 18))
    d = ImageDraw.Draw(im)
    cur = build_current((half_w, panel_h))
    im.paste(cur, (0, 46))
    v2 = grad((half_w, panel_h), (22, 24, 28), (14, 15, 18))
    d2 = ImageDraw.Draw(v2)
    window_chrome(d2, 'Open Project', half_w)
    nav_toolbar(d2, 40, half_w)
    d2.rectangle((0, 86, half_w, panel_h - 8), fill=(20, 23, 26))
    side_x = 178
    insp_x = half_w - 356
    d2.rectangle((side_x, 86, insp_x, panel_h - 8), fill=(19, 22, 25))
    sidebar(d2, 0, 86, side_x, panel_h - 8, 'Recent Projects')
    txt(d2, (side_x + 20, 108), 'Recent Projects', TEXT, B13, 'lm')
    txt(d2, (insp_x - 20, 108), '6 projects', MUTED, F11, 'rm')
    tile_w, tile_h, gap = 470, 178, 14
    x0, y0 = side_x + 18, 126
    for i, p in enumerate(PROJECTS):
        col, row = i % 2, i // 2
        bx = x0 + col * (tile_w + gap)
        by = y0 + row * (tile_h + gap)
        if by + tile_h > panel_h - 24:
            break
        project_tile(d2, (bx, by, bx + tile_w, by + tile_h), p)
    inspector(d2, insp_x, 86, half_w, panel_h - 8, PROJECTS[0])
    footer(d2, panel_h - 60, half_w, panel_h - 8, '6 projects')
    im.paste(v2, (0, 46 + panel_h + 38))
    txt(d, (16, 22), 'CURRENT  -  one generic icon per project, no preview, no resolution, '
                     'no health, no counts, fixed-width text note in the inspector',
        RED, F12, 'lm')
    txt(d, (16, panel_h + 68), 'IMPROVED v2  -  composition preview, resolution/duration, '
                               'health, composition and asset counts, external source warning, '
                               'favorites, real Places', GREEN, F12, 'lm')
    d.line((0, panel_h + 46, half_w, panel_h + 46), fill=LINE)
    im.save(path)


if __name__ == '__main__':
    build_candidate(OUT / 'project-open-v2-candidate-2026-09-26.png')
    build_before_after(OUT / 'project-open-v2-before-after-2026-09-26.png')
    print('project open v2 mockups written to', OUT)
