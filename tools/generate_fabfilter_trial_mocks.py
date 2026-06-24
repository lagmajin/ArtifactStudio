from __future__ import annotations

from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Artifact" / "App" / "Icon" / "FabFilterTrial"
W, H = 960, 540


def font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    candidates = [
        "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf",
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass
    return ImageFont.load_default()


def glow_line(base: Image.Image, points: list[tuple[float, float]], color: str, width: int = 4) -> None:
    glow = Image.new("RGBA", base.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for w, alpha in [(18, 42), (10, 74), (6, 110)]:
        gd.line(points, fill=color + f"{alpha:02x}", width=w, joint="curve")
    glow = glow.filter(ImageFilter.GaussianBlur(4))
    base.alpha_composite(glow)
    ImageDraw.Draw(base).line(points, fill=color, width=width, joint="curve")


def rounded(draw: ImageDraw.ImageDraw, box, fill, outline=None, radius=10, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def draw_grid(draw: ImageDraw.ImageDraw, box, major="#394149", minor="#2E363D") -> None:
    x0, y0, x1, y1 = box
    for i in range(1, 12):
        x = x0 + (x1 - x0) * i / 12
        draw.line([(x, y0), (x, y1)], fill=major if i in (3, 6, 9) else minor, width=1)
    for i in range(1, 8):
        y = y0 + (y1 - y0) * i / 8
        draw.line([(x0, y), (x1, y)], fill=major if i == 4 else minor, width=1)


def curve_points(box) -> list[tuple[float, float]]:
    x0, y0, x1, y1 = box
    pts = []
    for i in range(220):
        t = i / 219
        x = x0 + t * (x1 - x0)
        ease = 0.5 - 0.5 * math.cos(t * math.pi)
        y = y1 - (0.18 + 0.68 * ease) * (y1 - y0)
        y += math.sin(t * math.pi * 4) * 18 * (1 - abs(t - 0.5) * 1.2)
        pts.append((x, y))
    return pts


def draw_tabs(draw, x, y, labels, selected=0):
    f = font(13)
    for i, label in enumerate(labels):
        w = 74 if len(label) < 7 else 92
        fill = "#323C43" if i == selected else "#252C31"
        outline = "#5E6A72" if i == selected else "#3A444B"
        rounded(draw, [x, y, x + w, y + 28], fill, outline, radius=8)
        draw.text((x + 14, y + 6), label, fill="#DCE6E8" if i == selected else "#8D9AA0", font=f)
        x += w + 8


def draw_knob(draw, cx, cy, r, label, value, color):
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill="#232A30", outline="#4B565F", width=2)
    for a in range(215, 505, 8):
        rad = math.radians(a)
        p0 = (cx + math.cos(rad) * (r - 5), cy + math.sin(rad) * (r - 5))
        p1 = (cx + math.cos(rad) * (r - 1), cy + math.sin(rad) * (r - 1))
        draw.line([p0, p1], fill="#46525A")
    rad = math.radians(215 + 290 * value)
    draw.line([(cx, cy), (cx + math.cos(rad) * (r - 8), cy + math.sin(rad) * (r - 8))], fill=color, width=3)
    draw.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=color)
    draw.text((cx - draw.textlength(label, font=font(12)) / 2, cy + r + 8), label, fill="#AEB8BE", font=font(12))


def draw_button(draw, box, label, active=False, accent="#F8C84A"):
    fill = "#2B353C" if active else "#20282E"
    outline = accent if active else "#3A454C"
    rounded(draw, box, fill, outline, radius=8, width=1)
    f = font(13, active)
    tw = draw.textlength(label, font=f)
    draw.text(((box[0] + box[2] - tw) / 2, box[1] + 8), label, fill="#F4F7F8" if active else "#8F9BA2", font=f)


def draw_hq_curve_surface(draw, img, box):
    x0, y0, x1, y1 = box
    rounded(draw, box, "#10161B", "#3F4A52", 12)
    for i in range(1, 24):
        x = x0 + (x1 - x0) * i / 24
        draw.line([(x, y0 + 8), (x, y1 - 24)], fill="#232C33" if i % 4 else "#344049", width=1)
    for i in range(1, 12):
        y = y0 + (y1 - y0 - 32) * i / 12
        draw.line([(x0 + 44, y), (x1 - 18, y)], fill="#232C33" if i % 3 else "#344049", width=1)
    draw.line([(x0 + 44, y1 - 56), (x1 - 18, y1 - 56)], fill="#51616B", width=1)
    draw.line([(x0 + 44, y0 + 20), (x0 + 44, y1 - 24)], fill="#51616B", width=1)

    bands = [
        ("Position X", "#45D0FF", 0.18, 0.58, 0.025),
        ("Position Y", "#80E66E", 0.44, 0.38, 0.018),
        ("Opacity", "#F8C84A", 0.66, 0.78, 0.022),
    ]
    for _, color, center, height, spread in bands[:2]:
        pts = []
        for i in range(260):
            t = i / 259
            x = x0 + 44 + t * (x1 - x0 - 62)
            base = 0.22 + 0.42 * (0.5 - 0.5 * math.cos(t * math.pi))
            wave = 0.08 * math.sin(t * math.pi * 3.3)
            bump = height * math.exp(-((t - center) ** 2) / spread)
            y = y1 - 56 - (base + wave + bump * 0.18) * (y1 - y0 - 90)
            pts.append((x, y))
        glow_line(img, pts, color, 2)

    pts = []
    for i in range(320):
        t = i / 319
        x = x0 + 44 + t * (x1 - x0 - 62)
        ease = 0.5 - 0.5 * math.cos(t * math.pi)
        y = y1 - 56 - (0.16 + 0.70 * ease + 0.08 * math.sin(t * math.pi * 5)) * (y1 - y0 - 90)
        y -= 44 * math.exp(-((t - 0.62) ** 2) / 0.012)
        pts.append((x, y))
    under = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ud = ImageDraw.Draw(under)
    ud.polygon(pts + [(x1 - 18, y1 - 56), (x0 + 44, y1 - 56)], fill="#F8C84A18")
    img.alpha_composite(under)
    glow_line(img, pts, "#F8C84A", 5)

    key_pts = [(132, 374), (264, 332), (408, 286), (546, 242), (654, 160), (774, 132), (862, 148)]
    key_pts = [(x0 + px - 58, y0 + py - 94) for px, py in key_pts]
    selected = 4
    for i in range(len(key_pts) - 1):
        draw.line([key_pts[i], key_pts[i + 1]], fill="#72828B", width=1)
    for idx, (kx, ky) in enumerate(key_pts):
        if idx == selected:
            draw.line([(kx - 78, ky + 42), (kx + 96, ky - 54)], fill="#D3DEE2", width=2)
            draw.ellipse([kx - 87, ky + 33, kx - 71, ky + 49], fill="#D3DEE2", outline="#11171A", width=2)
            draw.ellipse([kx + 88, ky - 62, kx + 104, ky - 46], fill="#D3DEE2", outline="#11171A", width=2)
        color = "#F8C84A" if idx == selected else "#45D0FF"
        draw.ellipse([kx - 12, ky - 12, kx + 12, ky + 12], fill="#11171A", outline=color, width=4)
        draw.ellipse([kx - 5, ky - 5, kx + 5, ky + 5], fill=color)

    kx, ky = key_pts[selected]
    rounded(draw, [kx - 116, ky - 112, kx + 154, ky - 52], "#0D1318EE", "#74828A", 10)
    draw.text((kx - 96, ky - 99), "Opacity  82.6%", fill="#F7FAFA", font=font(17, True))
    draw.text((kx - 96, ky - 76), "Frame 72  |  Auto Bezier  |  Ease-in 64%", fill="#AEBAC0", font=font(13))

    for i, label in enumerate(["0f", "12f", "24f", "36f", "48f", "60f", "72f", "84f", "96f"]):
        x = x0 + 44 + i * (x1 - x0 - 62) / 8
        draw.text((x - 10, y1 - 38), label, fill="#6E7C84", font=font(11))
    for i, label in enumerate(["100", "75", "50", "25", "0"]):
        y = y0 + 22 + i * (y1 - y0 - 84) / 4
        draw.text((x0 + 12, y - 7), label, fill="#6E7C84", font=font(11))
    draw.text((x1 - 126, y0 + 18), "Value Graph", fill="#AEBAC0", font=font(13, True))
    draw.text((x1 - 126, y0 + 38), "3 visible tracks", fill="#6E7C84", font=font(12))


def curve_editor_hq_mock() -> Path:
    img = Image.new("RGBA", (1440, 900), "#12171C")
    draw = ImageDraw.Draw(img)
    rounded(draw, [36, 28, 1404, 872], "#1C2329", "#3D4850", 18)
    draw.rectangle([36, 28, 1404, 98], fill="#232B31")
    draw.text((64, 51), "Artifact Curve Editor", fill="#F1F6F7", font=font(24, True))
    draw.text((64, 79), "Composition / Hero_Title / Transform / Opacity", fill="#84939B", font=font(13))
    draw_button(draw, [422, 48, 516, 82], "Position")
    draw_button(draw, [528, 48, 622, 82], "Scale")
    draw_button(draw, [634, 48, 728, 82], "Rotation")
    draw_button(draw, [740, 48, 834, 82], "Opacity", True)
    draw_button(draw, [1120, 48, 1220, 82], "Value", True, "#45D0FF")
    draw_button(draw, [1232, 48, 1332, 82], "Speed")

    sidebar = [58, 122, 252, 664]
    rounded(draw, sidebar, "#171E24", "#354047", 12)
    draw.text((78, 144), "Tracks", fill="#E2EAED", font=font(16, True))
    rows = [
        ("Position X", "#45D0FF", True, "7 keys"),
        ("Position Y", "#80E66E", True, "7 keys"),
        ("Scale", "#B78CFF", False, "4 keys"),
        ("Rotation", "#FF7F6A", False, "3 keys"),
        ("Opacity", "#F8C84A", True, "7 keys"),
    ]
    y = 178
    for name, color, active, meta in rows:
        fill = "#24313A" if name == "Opacity" else "#1B242A"
        rounded(draw, [74, y, 236, y + 58], fill, "#3B4850", 9)
        draw.ellipse([88, y + 19, 104, y + 35], fill=color)
        draw.text((116, y + 13), name, fill="#EDF4F5" if active else "#8E9AA1", font=font(14, active))
        draw.text((116, y + 33), meta, fill="#687982", font=font(11))
        if active:
            draw.line([(225, y + 16), (225, y + 42)], fill=color, width=3)
        y += 70

    graph = [282, 122, 1368, 664]
    draw_hq_curve_surface(draw, img, graph)

    bottom = [58, 694, 1368, 842]
    rounded(draw, bottom, "#20282F", "#3B454D", 14)
    draw.text((84, 722), "Selected Key", fill="#E9F0F2", font=font(17, True))
    draw.text((84, 749), "Frame 72  /  Value 82.6  /  Temporal influence 64%  /  Spatial continuity locked", fill="#91A0A7", font=font(13))
    for i, (label, active) in enumerate([("Auto", True), ("Flat", False), ("Linear", False), ("Step", False), ("Hold", False)]):
        draw_button(draw, [360 + i * 88, 720, 432 + i * 88, 754], label, active)
    draw_button(draw, [360, 772, 474, 810], "Mirror In")
    draw_button(draw, [486, 772, 600, 810], "Mirror Out")
    draw_button(draw, [612, 772, 726, 810], "Break", False, "#FF7F6A")

    knobs = [("Speed", .72, "#F8C84A"), ("Influence", .64, "#45D0FF"), ("Weight", .48, "#B78CFF"), ("Blend", .86, "#80E66E")]
    for i, (label, val, color) in enumerate(knobs):
        draw_knob(draw, 866 + i * 100, 758, 30, label, val, color)

    right = [1198, 710, 1340, 822]
    rounded(draw, right, "#161E24", "#3A454C", 10)
    draw.text((1216, 730), "Pointer HUD", fill="#DDE7EA", font=font(14, True))
    draw.text((1216, 754), "Drag node: move", fill="#82919A", font=font(12))
    draw.text((1216, 774), "Alt: break handles", fill="#82919A", font=font(12))
    draw.text((1216, 794), "Shift: constrain", fill="#82919A", font=font(12))

    path = OUT / "fabfilter_curve_editor_hq_mock.png"
    img.convert("RGB").save(path, quality=96)
    return path


def curve_editor_mock() -> Path:
    img = Image.new("RGBA", (W, H), "#161B20")
    draw = ImageDraw.Draw(img)
    rounded(draw, [22, 18, W - 22, H - 18], "#20262B", "#3D464D", 14)
    draw.rectangle([22, 18, W - 22, 70], fill="#252C32")
    draw.text((42, 36), "Artifact Curve Editor", fill="#E6ECEF", font=font(18, True))
    draw_tabs(draw, 260, 30, ["Position", "Opacity", "Scale", "Rotation"], 1)

    graph = [58, 94, 902, 390]
    rounded(draw, graph, "#171D22", "#3F4A52", 10)
    draw_grid(draw, graph)

    for i, label in enumerate(["0f", "24f", "48f", "72f", "96f"]):
        x = graph[0] + i * (graph[2] - graph[0]) / 4
        draw.text((x - 9, graph[3] + 8), label, fill="#68757D", font=font(11))
    for i, label in enumerate(["100", "75", "50", "25", "0"]):
        y = graph[1] + i * (graph[3] - graph[1]) / 4
        draw.text((graph[0] - 34, y - 7), label, fill="#68757D", font=font(11))

    pts = curve_points(graph)
    glow_line(img, pts, "#F7C84A", 4)

    selected = [(180, 314), (337, 245), (521, 206), (704, 150), (834, 120)]
    for i in range(len(selected) - 1):
        draw.line([selected[i], selected[i + 1]], fill="#69828C", width=1)
    for idx, (x, y) in enumerate(selected):
        color = "#F7C84A" if idx == 2 else "#7AE7EE"
        draw.ellipse([x - 9, y - 9, x + 9, y + 9], fill=color, outline="#0C1114", width=3)
        if idx == 2:
            draw.line([(x - 70, y + 28), (x + 72, y - 33)], fill="#C0D2D9", width=2)
            draw.ellipse([x - 76, y + 22, x - 64, y + 34], fill="#C0D2D9")
            draw.ellipse([x + 66, y - 39, x + 78, y - 27], fill="#C0D2D9")
            rounded(draw, [x - 92, y - 88, x + 110, y - 42], "#11171ACC", "#66737A", 8)
            draw.text((x - 76, y - 76), "Opacity  63.4%", fill="#EAF2F3", font=font(14, True))
            draw.text((x - 76, y - 58), "Auto tangent  |  Ease out", fill="#98A8AE", font=font(12))

    panel_y = 414
    rounded(draw, [58, panel_y, 902, 503], "#242B31", "#3B454D", 10)
    draw.text((78, panel_y + 18), "Selected key", fill="#DCE6E8", font=font(15, True))
    draw.text((78, panel_y + 43), "Frame 48  /  Value 63.4  /  Influence 72%", fill="#91A0A7", font=font(13))
    draw_tabs(draw, 382, panel_y + 19, ["Auto", "Flat", "Linear", "Step"], 0)
    for i, (label, val, color) in enumerate([("Speed", 0.68, "#F7C84A"), ("Weight", 0.42, "#7AE7EE"), ("Blend", 0.78, "#B68CFF")]):
        draw_knob(draw, 650 + i * 78, panel_y + 39, 20, label, val, color)

    path = OUT / "fabfilter_curve_editor_mock.png"
    img.convert("RGB").save(path, quality=95)
    return path


def effect_inspector_mock() -> Path:
    img = Image.new("RGBA", (W, H), "#161B20")
    draw = ImageDraw.Draw(img)
    rounded(draw, [22, 18, W - 22, H - 18], "#20262B", "#3D464D", 14)
    draw.rectangle([22, 18, W - 22, 70], fill="#252C32")
    draw.text((42, 36), "Artifact Effect Inspector", fill="#E6ECEF", font=font(18, True))
    draw_tabs(draw, 290, 30, ["Glow", "Color", "Distort", "Timing"], 0)

    main = [54, 94, 650, 390]
    rounded(draw, main, "#171D22", "#3F4A52", 10)
    draw_grid(draw, main)
    spectrum = []
    for i in range(180):
        t = i / 179
        x = main[0] + t * (main[2] - main[0])
        amp = 0.25 + 0.18 * math.sin(t * 23) + 0.12 * math.sin(t * 71)
        if 0.44 < t < 0.62:
            amp += 0.26 * math.sin((t - 0.44) / 0.18 * math.pi)
        y = main[3] - amp * (main[3] - main[1])
        spectrum.append((x, y))
    area = spectrum + [(main[2], main[3]), (main[0], main[3])]
    draw.polygon(area, fill="#2BC7D733")
    glow_line(img, spectrum, "#2BD6E4", 3)
    response = []
    for i in range(200):
        t = i / 199
        x = main[0] + t * (main[2] - main[0])
        bump = math.exp(-((t - 0.58) ** 2) / 0.012)
        dip = math.exp(-((t - 0.28) ** 2) / 0.006)
        y = main[3] - (0.33 + 0.42 * bump - 0.16 * dip) * (main[3] - main[1])
        response.append((x, y))
    glow_line(img, response, "#F8B84A", 5)
    for x, y, label, color in [(224, 288, "Warmth", "#7AE7EE"), (398, 226, "Halation", "#F8B84A"), (544, 180, "Bloom", "#B88CFF")]:
        draw.ellipse([x - 12, y - 12, x + 12, y + 12], fill=color, outline="#0C1114", width=3)
        rounded(draw, [x - 40, y - 52, x + 52, y - 24], "#11171ACC", "#62717A", 8)
        draw.text((x - 28, y - 45), label, fill="#EAF2F3", font=font(12, True))

    side = [680, 94, 902, 390]
    rounded(draw, side, "#242B31", "#3B454D", 10)
    draw.text((700, 116), "Live response", fill="#DCE6E8", font=font(15, True))
    meter_x = 724
    for i, (label, val, color) in enumerate([("Input", .72, "#75D7FF"), ("Glow", .58, "#F8B84A"), ("Output", .83, "#7BE06B")]):
        x = meter_x + i * 54
        draw.rounded_rectangle([x, 152, x + 22, 332], radius=7, fill="#151B20", outline="#3C4850")
        fill_h = 180 * val
        draw.rounded_rectangle([x + 3, 332 - fill_h, x + 19, 329], radius=5, fill=color)
        draw.text((x - 5, 344), label, fill="#91A0A7", font=font(11))
    draw.text((700, 376), "Peak-safe preview", fill="#91A0A7", font=font(12))

    bottom = [54, 414, 902, 503]
    rounded(draw, bottom, "#242B31", "#3B454D", 10)
    draw.text((74, 435), "Selected node", fill="#DCE6E8", font=font(15, True))
    draw.text((74, 460), "Halation  /  Radius 18.2  /  Threshold -14.0 dB", fill="#91A0A7", font=font(13))
    for i, (label, val, color) in enumerate([("Radius", 0.62, "#F8B84A"), ("Gain", 0.74, "#7AE7EE"), ("Soft Clip", 0.38, "#FF7B6E"), ("Mix", 0.81, "#B88CFF")]):
        draw_knob(draw, 452 + i * 90, 452, 22, label, val, color)

    path = OUT / "fabfilter_effect_inspector_mock.png"
    img.convert("RGB").save(path, quality=95)
    return path


def contact_sheet(paths: list[Path]) -> Path:
    sheet = Image.new("RGB", (960, 390), "#181D22")
    draw = ImageDraw.Draw(sheet)
    draw.text((32, 22), "FabFilter-inspired ArtifactStudio UI mock trials", fill="#E6ECEF", font=font(22, True))
    y = 68
    for path in paths:
        im = Image.open(path).resize((432, 243), Image.Resampling.LANCZOS)
        x = 32 if "curve" in path.name else 496
        sheet.paste(im, (x, y))
        label = "Curve Editor: direct animated graph surface" if "curve" in path.name else "Effect Inspector: graph-first parameter HUD"
        draw.text((x, y + 254), label, fill="#BFCBD1", font=font(14, True))
    draw.text((32, 354), "Original concept art only: borrows interaction principles, not FabFilter assets or layout.", fill="#7F8D95", font=font(13))
    out = OUT / "_fabfilter_trial_contact_sheet.png"
    sheet.save(out, quality=95)
    return out


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    hq = curve_editor_hq_mock()
    paths = [curve_editor_mock(), effect_inspector_mock()]
    sheet = contact_sheet(paths)
    print(f"Wrote mocks to {OUT}")
    print(hq)
    print(sheet)


if __name__ == "__main__":
    main()
