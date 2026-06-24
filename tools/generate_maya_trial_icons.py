from __future__ import annotations

from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Artifact" / "App" / "Icon" / "MayaTrial"
SIZE = 32
SHEET_SCALE = 3


ICONS = [
    ("maya_select", "select", "#32C7C9"),
    ("maya_move", "move", "#42D77D"),
    ("maya_rotate", "rotate", "#F3B247"),
    ("maya_scale", "scale", "#65A8FF"),
    ("maya_camera", "camera", "#D9E2E8"),
    ("maya_mesh_cube", "cube", "#37C5D1"),
    ("maya_mesh_sphere", "sphere", "#31B5B8"),
    ("maya_curve", "curve", "#F5C85A"),
    ("maya_joint", "joint", "#E7D57A"),
    ("maya_keyframe", "key", "#FF9A47"),
    ("maya_graph", "graph", "#7DDC68"),
    ("maya_material", "material", "#9EDBFF"),
    ("maya_light", "light", "#FFD65E"),
    ("maya_render", "render", "#AEB8C2"),
    ("maya_uv", "uv", "#B88CFF"),
    ("maya_outliner", "outliner", "#74D9CF"),
]


def svg_header() -> list[str]:
    return [
        '<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">',
        '  <rect x="1" y="1" width="30" height="30" rx="3" fill="#252A2E"/>',
        '  <rect x="2" y="2" width="28" height="28" rx="2" fill="#33393E"/>',
        '  <path d="M3 4h26v7H3Z" fill="#444B50"/>',
    ]


def svg_footer() -> list[str]:
    return ["</svg>"]


def write_svg(name: str, kind: str, accent: str) -> None:
    s = svg_header()
    dark = "#11171A"
    pale = "#E8EEF0"
    if kind == "select":
        s += [
            f'  <path d="M9 7 23 21l-6.4 1.1-2.8 6.1L9 7Z" fill="{pale}"/>',
            f'  <path d="M15 20.5 18 28l3-1.5-3.4-7Z" fill="{accent}"/>',
            f'  <path d="M9 7 23 21l-6.4 1.1-2.8 6.1Z" fill="none" stroke="{dark}" stroke-width="2"/>',
        ]
    elif kind == "move":
        s += [
            f'  <path d="M16 5v22M5 16h22" stroke="{accent}" stroke-width="3" stroke-linecap="round"/>',
            f'  <path d="M16 5l-4 4h8ZM16 27l-4-4h8ZM5 16l4-4v8ZM27 16l-4-4v8Z" fill="{pale}"/>',
        ]
    elif kind == "rotate":
        s += [
            f'  <path d="M8 18a8 8 0 0 1 13-6" fill="none" stroke="{accent}" stroke-width="3" stroke-linecap="round"/>',
            f'  <path d="M21 8v7h-7Z" fill="{pale}"/>',
            f'  <path d="M24 15a8 8 0 0 1-13 6" fill="none" stroke="#61B7FF" stroke-width="3" stroke-linecap="round"/>',
            f'  <path d="M11 24v-7h7Z" fill="{pale}"/>',
        ]
    elif kind == "scale":
        s += [
            f'  <path d="M8 24h12V12H8Z" fill="{accent}" opacity=".88"/>',
            f'  <path d="M13 19h12V7H13Z" fill="{pale}" opacity=".95"/>',
            f'  <path d="M8 24h12V12H8ZM13 19h12V7H13Z" fill="none" stroke="{dark}" stroke-width="1.8"/>',
        ]
    elif kind == "camera":
        s += [
            f'  <path d="M6 12h14v11H6Z" fill="{pale}"/>',
            f'  <path d="M20 15l7-4v13l-7-4Z" fill="{accent}"/>',
            f'  <path d="M9 9h8l2 3H7Z" fill="#AAB5BA"/>',
            f'  <path d="M6 12h14v11H6ZM20 15l7-4v13l-7-4Z" fill="none" stroke="{dark}" stroke-width="1.8"/>',
        ]
    elif kind == "cube":
        s += [
            f'  <path d="M16 5 26 10v12l-10 5-10-5V10Z" fill="{accent}"/>',
            f'  <path d="M16 5v12l10-7M16 17 6 10M16 17v10" fill="none" stroke="{dark}" stroke-width="1.8"/>',
            '  <path d="M7 10 16 6l9 4-9 5Z" fill="#7DE6E9" opacity=".55"/>',
        ]
    elif kind == "sphere":
        s += [
            f'  <circle cx="16" cy="16" r="10" fill="{accent}"/>',
            f'  <path d="M7 16h18M16 6c4 4 4 16 0 20M16 6c-4 4-4 16 0 20" fill="none" stroke="{dark}" stroke-width="1.6"/>',
            '  <circle cx="12" cy="11" r="3" fill="#BDFBFF" opacity=".55"/>',
        ]
    elif kind == "curve":
        s += [
            f'  <path d="M6 23C8 8 20 25 26 8" fill="none" stroke="{accent}" stroke-width="3" stroke-linecap="round"/>',
            f'  <circle cx="6" cy="23" r="3" fill="{pale}"/><circle cx="16" cy="16" r="3" fill="{pale}"/><circle cx="26" cy="8" r="3" fill="{pale}"/>',
        ]
    elif kind == "joint":
        s += [
            f'  <path d="M9 24 16 8l7 16Z" fill="none" stroke="{accent}" stroke-width="3" stroke-linejoin="round"/>',
            f'  <circle cx="16" cy="8" r="4" fill="{pale}"/><circle cx="9" cy="24" r="3" fill="{pale}"/><circle cx="23" cy="24" r="3" fill="{pale}"/>',
        ]
    elif kind == "key":
        s += [
            f'  <path d="M16 5 27 16 16 27 5 16Z" fill="{accent}"/>',
            f'  <path d="M16 9 23 16 16 23 9 16Z" fill="#2D3338" stroke="{dark}" stroke-width="1.5"/>',
        ]
    elif kind == "graph":
        s += [
            f'  <path d="M5 24h22" stroke="{pale}" stroke-width="2"/>',
            f'  <path d="M7 22C12 6 18 28 25 10" fill="none" stroke="{accent}" stroke-width="3" stroke-linecap="round"/>',
            f'  <circle cx="7" cy="22" r="2.5" fill="{pale}"/><circle cx="25" cy="10" r="2.5" fill="{pale}"/>',
        ]
    elif kind == "material":
        s += [
            f'  <circle cx="16" cy="16" r="10" fill="{accent}"/>',
            '  <path d="M9 20c5 3 13-1 15-8 3 10-9 17-15 8Z" fill="#5E6C73" opacity=".45"/>',
            '  <circle cx="12" cy="11" r="3" fill="#FFFFFF" opacity=".75"/>',
        ]
    elif kind == "light":
        s += [
            f'  <path d="M16 5 26 22H6Z" fill="{accent}"/>',
            f'  <path d="M12 25h8M13 28h6" stroke="{pale}" stroke-width="2" stroke-linecap="round"/>',
            f'  <path d="M16 5 26 22H6Z" fill="none" stroke="{dark}" stroke-width="1.8"/>',
        ]
    elif kind == "render":
        s += [
            f'  <path d="M7 8h18v16H7Z" fill="{pale}"/>',
            f'  <path d="M10 12h12v8H10Z" fill="{accent}"/>',
            f'  <path d="M7 8h18v16H7Z" fill="none" stroke="{dark}" stroke-width="1.8"/>',
            '  <path d="M10 12h12v3H10Z" fill="#FFFFFF" opacity=".35"/>',
        ]
    elif kind == "uv":
        s += [
            f'  <path d="M7 7h18v18H7Z" fill="none" stroke="{accent}" stroke-width="2"/>',
            f'  <path d="M7 16h18M16 7v18M7 7l18 18M25 7 7 25" stroke="{pale}" stroke-width="1.5" opacity=".85"/>',
        ]
    elif kind == "outliner":
        s += [
            f'  <path d="M7 8h18M7 15h18M7 22h18" stroke="{pale}" stroke-width="2.4" stroke-linecap="round"/>',
            f'  <circle cx="9" cy="8" r="3" fill="{accent}"/><rect x="13" y="12" width="6" height="6" fill="{accent}"/><path d="M9 19 13 25H5Z" fill="{accent}"/>',
        ]
    s += svg_footer()
    (OUT / f"{name}.svg").write_text("\n".join(s) + "\n", encoding="utf-8")


def draw_icon(draw: ImageDraw.ImageDraw, x: int, y: int, kind: str, accent: str, scale: int = 1) -> None:
    def p(points):
        return [(x + px * scale, y + py * scale) for px, py in points]

    d = draw
    dark = "#11171A"
    pale = "#E8EEF0"
    d.rounded_rectangle([x + scale, y + scale, x + 31 * scale, y + 31 * scale], radius=3 * scale, fill="#252A2E")
    d.rounded_rectangle([x + 2 * scale, y + 2 * scale, x + 30 * scale, y + 30 * scale], radius=2 * scale, fill="#33393E")
    d.rectangle([x + 3 * scale, y + 4 * scale, x + 29 * scale, y + 11 * scale], fill="#444B50")
    if kind == "select":
        d.polygon(p([(9, 7), (23, 21), (16, 22), (14, 28)]), fill=pale, outline=dark)
        d.polygon(p([(15, 21), (18, 28), (21, 27), (18, 20)]), fill=accent, outline=dark)
    elif kind == "move":
        d.line([x + 16 * scale, y + 5 * scale, x + 16 * scale, y + 27 * scale], fill=accent, width=3 * scale)
        d.line([x + 5 * scale, y + 16 * scale, x + 27 * scale, y + 16 * scale], fill=accent, width=3 * scale)
        for poly in [[(16, 5), (12, 9), (20, 9)], [(16, 27), (12, 23), (20, 23)], [(5, 16), (9, 12), (9, 20)], [(27, 16), (23, 12), (23, 20)]]:
            d.polygon(p(poly), fill=pale)
    elif kind == "rotate":
        d.arc([x + 8 * scale, y + 7 * scale, x + 25 * scale, y + 24 * scale], 190, 330, fill=accent, width=3 * scale)
        d.arc([x + 7 * scale, y + 8 * scale, x + 24 * scale, y + 25 * scale], 10, 150, fill="#61B7FF", width=3 * scale)
        d.polygon(p([(21, 8), (21, 15), (14, 15)]), fill=pale)
        d.polygon(p([(11, 24), (11, 17), (18, 17)]), fill=pale)
    elif kind == "scale":
        d.rectangle([x + 8 * scale, y + 12 * scale, x + 20 * scale, y + 24 * scale], fill=accent, outline=dark, width=2 * scale)
        d.rectangle([x + 13 * scale, y + 7 * scale, x + 25 * scale, y + 19 * scale], fill=pale, outline=dark, width=2 * scale)
    elif kind == "camera":
        d.rectangle([x + 6 * scale, y + 12 * scale, x + 20 * scale, y + 23 * scale], fill=pale, outline=dark, width=2 * scale)
        d.polygon(p([(20, 15), (27, 11), (27, 24), (20, 20)]), fill=accent, outline=dark)
        d.polygon(p([(7, 12), (9, 9), (17, 9), (19, 12)]), fill="#AAB5BA", outline=dark)
    elif kind == "cube":
        d.polygon(p([(16, 5), (26, 10), (26, 22), (16, 27), (6, 22), (6, 10)]), fill=accent, outline=dark)
        d.polygon(p([(7, 10), (16, 6), (25, 10), (16, 15)]), fill="#7DE6E9")
        d.line(p([(16, 6), (16, 27), (26, 10), (16, 15), (6, 10)]), fill=dark, width=2 * scale)
    elif kind == "sphere":
        d.ellipse([x + 6 * scale, y + 6 * scale, x + 26 * scale, y + 26 * scale], fill=accent, outline=dark, width=2 * scale)
        d.arc([x + 10 * scale, y + 6 * scale, x + 22 * scale, y + 26 * scale], 80, 280, fill=dark, width=1 * scale)
        d.arc([x + 10 * scale, y + 6 * scale, x + 22 * scale, y + 26 * scale], -100, 100, fill=dark, width=1 * scale)
        d.line([x + 7 * scale, y + 16 * scale, x + 25 * scale, y + 16 * scale], fill=dark, width=1 * scale)
        d.ellipse([x + 10 * scale, y + 9 * scale, x + 15 * scale, y + 14 * scale], fill="#BDFBFF")
    elif kind == "curve":
        pts = [(6, 23), (10, 10), (16, 16), (22, 23), (26, 8)]
        d.line(p(pts), fill=accent, width=3 * scale, joint="curve")
        for cx, cy in [(6, 23), (16, 16), (26, 8)]:
            d.ellipse([x + (cx - 3) * scale, y + (cy - 3) * scale, x + (cx + 3) * scale, y + (cy + 3) * scale], fill=pale, outline=dark)
    elif kind == "joint":
        d.line(p([(9, 24), (16, 8), (23, 24), (9, 24)]), fill=accent, width=3 * scale)
        for cx, cy, r in [(16, 8, 4), (9, 24, 3), (23, 24, 3)]:
            d.ellipse([x + (cx - r) * scale, y + (cy - r) * scale, x + (cx + r) * scale, y + (cy + r) * scale], fill=pale, outline=dark)
    elif kind == "key":
        d.polygon(p([(16, 5), (27, 16), (16, 27), (5, 16)]), fill=accent, outline=dark)
        d.polygon(p([(16, 9), (23, 16), (16, 23), (9, 16)]), fill="#2D3338", outline=dark)
    elif kind == "graph":
        d.line([x + 5 * scale, y + 24 * scale, x + 27 * scale, y + 24 * scale], fill=pale, width=2 * scale)
        d.line(p([(7, 22), (11, 8), (17, 24), (25, 10)]), fill=accent, width=3 * scale)
        for cx, cy in [(7, 22), (25, 10)]:
            d.ellipse([x + (cx - 2) * scale, y + (cy - 2) * scale, x + (cx + 2) * scale, y + (cy + 2) * scale], fill=pale)
    elif kind == "material":
        d.ellipse([x + 6 * scale, y + 6 * scale, x + 26 * scale, y + 26 * scale], fill=accent, outline=dark, width=2 * scale)
        d.pieslice([x + 7 * scale, y + 9 * scale, x + 27 * scale, y + 28 * scale], 20, 155, fill="#5E6C73")
        d.ellipse([x + 10 * scale, y + 9 * scale, x + 15 * scale, y + 14 * scale], fill="#FFFFFF")
    elif kind == "light":
        d.polygon(p([(16, 5), (26, 22), (6, 22)]), fill=accent, outline=dark)
        d.line([x + 12 * scale, y + 25 * scale, x + 20 * scale, y + 25 * scale], fill=pale, width=2 * scale)
        d.line([x + 13 * scale, y + 28 * scale, x + 19 * scale, y + 28 * scale], fill=pale, width=2 * scale)
    elif kind == "render":
        d.rectangle([x + 7 * scale, y + 8 * scale, x + 25 * scale, y + 24 * scale], fill=pale, outline=dark, width=2 * scale)
        d.rectangle([x + 10 * scale, y + 12 * scale, x + 22 * scale, y + 20 * scale], fill=accent, outline=dark)
        d.rectangle([x + 10 * scale, y + 12 * scale, x + 22 * scale, y + 15 * scale], fill="#FFFFFF")
    elif kind == "uv":
        d.rectangle([x + 7 * scale, y + 7 * scale, x + 25 * scale, y + 25 * scale], outline=accent, width=2 * scale)
        for a, b in [((7, 16), (25, 16)), ((16, 7), (16, 25)), ((7, 7), (25, 25)), ((25, 7), (7, 25))]:
            d.line([x + a[0] * scale, y + a[1] * scale, x + b[0] * scale, y + b[1] * scale], fill=pale, width=1 * scale)
    elif kind == "outliner":
        for yy in [8, 15, 22]:
            d.line([x + 7 * scale, y + yy * scale, x + 25 * scale, y + yy * scale], fill=pale, width=2 * scale)
        d.ellipse([x + 6 * scale, y + 5 * scale, x + 12 * scale, y + 11 * scale], fill=accent, outline=dark)
        d.rectangle([x + 13 * scale, y + 12 * scale, x + 19 * scale, y + 18 * scale], fill=accent, outline=dark)
        d.polygon(p([(9, 19), (13, 25), (5, 25)]), fill=accent, outline=dark)


def write_png(name: str, kind: str, accent: str) -> None:
    img = Image.new("RGBA", (SIZE * SHEET_SCALE, SIZE * SHEET_SCALE), (0, 0, 0, 0))
    draw_icon(ImageDraw.Draw(img), 0, 0, kind, accent, SHEET_SCALE)
    img = img.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    img.save(OUT / f"{name}.png")


def contact_sheet() -> None:
    cell_w = 88
    cell_h = 64
    cols = 4
    rows = math.ceil(len(ICONS) / cols)
    sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), "#202428")
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 10)
    except OSError:
        font = ImageFont.load_default()
    for idx, (name, kind, accent) in enumerate(ICONS):
        col = idx % cols
        row = idx // cols
        x = col * cell_w
        y = row * cell_h
        draw.rounded_rectangle([x + 5, y + 5, x + cell_w - 5, y + cell_h - 5], radius=5, fill="#2B3035", outline="#495057")
        png = Image.open(OUT / f"{name}.png").convert("RGBA").resize((32, 32), Image.Resampling.NEAREST)
        sheet.paste(png, (x + 28, y + 10), png)
        label = name.replace("maya_", "")
        bbox = draw.textbbox((0, 0), label, font=font)
        draw.text((x + (cell_w - (bbox[2] - bbox[0])) / 2, y + 46), label, fill="#DCE4E8", font=font)
    sheet.save(OUT / "_maya_trial_contact_sheet.png")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, kind, accent in ICONS:
        write_svg(name, kind, accent)
        write_png(name, kind, accent)
    contact_sheet()
    print(f"Wrote {len(ICONS)} SVG/PNG pairs and contact sheet to {OUT}")


if __name__ == "__main__":
    main()
