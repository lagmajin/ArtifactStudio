"""Measure hidden-RGB exposure at OldTV's shifted channel edges."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((128, 96), (256, 192), (512, 384))
FRINGES = (0.0, 0.1, 0.3, 0.5, 1.0)
BACKGROUNDS = {"black": (0, 0, 0), "mid-gray": (128, 128, 128), "white": (255, 255, 255)}
PATTERNS = ("uniform", "checker")
REPRESENTATIVE_WIDTH = 256


def make_pair(width: int, height: int, pattern: str) -> tuple[np.ndarray, np.ndarray]:
    scale = 4
    yy, xx = np.mgrid[0:height, 0:width]
    rgba_a = np.empty((height, width, 4), dtype=np.uint8)
    rgba_b = np.empty_like(rgba_a)
    if pattern == "uniform":
        hidden_a = np.array((255, 0, 255), dtype=np.uint8)
        hidden_b = np.array((0, 255, 32), dtype=np.uint8)
        rgba_a[:, :, :3] = hidden_a
        rgba_b[:, :, :3] = hidden_b
    else:
        tile = ((xx // 5 + yy // 7) & 1).astype(bool)
        rgba_a[:, :, :3] = np.where(tile[:, :, None], (255, 0, 255), (0, 220, 255))
        rgba_b[:, :, :3] = np.where(tile[:, :, None], (0, 255, 32), (255, 220, 0))
    rgba_a[:, :, 3] = 0
    rgba_b[:, :, 3] = 0

    local_y, local_x = np.mgrid[0:height, 0:width]
    t = local_x / max(width - 1, 1)
    v = local_y / max(height - 1, 1)
    visible_rgb = np.stack((35 + 205 * t, 215 - 155 * t, 45 + 185 * v), axis=2)
    visible_rgb = np.uint8(np.clip(visible_rgb, 0, 255))
    mask_hi = Image.new("L", (width * scale, height * scale), 0)
    points_norm = ((.07,.20),(.20,.08),(.48,.14),(.73,.05),(.94,.24),(.88,.49),
                   (.95,.73),(.76,.94),(.56,.78),(.36,.92),(.18,.76),(.08,.54))
    points = tuple((round(x * (width - 1) * scale), round(y * (height - 1) * scale))
                   for x, y in points_norm)
    draw = ImageDraw.Draw(mask_hi)
    draw.polygon(points, fill=255)
    mask = mask_hi.resize((width, height), Image.Resampling.LANCZOS)
    alpha = np.asarray(mask, dtype=np.uint8)
    rgba_a[:, :, :3] = np.where((alpha > 0)[:, :, None], visible_rgb, rgba_a[:, :, :3])
    rgba_b[:, :, :3] = np.where((alpha > 0)[:, :, None], visible_rgb, rgba_b[:, :, :3])
    # A narrow channel-coded interior line makes RGB sampling displacement legible.
    y_line = int(height * .51)
    x0, x1 = int(width * .18), int(width * .83)
    for image in (rgba_a, rgba_b):
        image[y_line:y_line + 2, x0:x1, :3] = (255, 240, 40)
    rgba_a[:, :, 3] = alpha
    rgba_b[:, :, 3] = alpha
    return rgba_a, rgba_b


def apply_oldtv_fringe(rgba: np.ndarray, fringe: float) -> np.ndarray:
    height, width = rgba.shape[:2]
    y, x = np.mgrid[0:height, 0:width]
    cx = width * 0.5
    split = fringe * 5.0 * (np.abs(x - cx) / cx)
    # C++ static_cast<int> truncates toward zero, then clamps to image bounds.
    sx_r = np.clip(np.trunc(x - split).astype(np.int64), 0, width - 1)
    sx_b = np.clip(np.trunc(x + split).astype(np.int64), 0, width - 1)
    out = rgba.copy()
    out[:, :, 0] = rgba[y, sx_r, 0]
    out[:, :, 2] = rgba[y, sx_b, 2]
    # OldTVEffect reads/writes only RGB planes; local alpha remains unchanged.
    return out


def composite_code(rgba: np.ndarray, background: tuple[int, int, int]) -> np.ndarray:
    source = rgba[:, :, :3].astype(np.float32) / 255.0
    alpha = rgba[:, :, 3:4].astype(np.float32) / 255.0
    bg = np.asarray(background, dtype=np.float32).reshape(1, 1, 3) / 255.0
    return source * alpha + bg * (1.0 - alpha)


def summarize_pair(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    if not np.any(mask):
        return {"pixel count": 0, "MAE normalized code": 0.0,
                "changed pixel ratio >1/255": 0.0, "P95 abs normalized code": 0.0}
    delta = np.abs(a[mask] - b[mask])
    return {
        "pixel count": int(np.count_nonzero(mask)),
        "MAE normalized code": float(np.mean(delta)),
        "changed pixel ratio >1/255": float(np.mean(np.max(delta, axis=1) > 1 / 255)),
        "P95 abs normalized code": float(np.percentile(delta, 95)),
    }


def edge_masks(alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    padded = np.pad(visible, 1, constant_values=False)
    boundary = np.zeros_like(visible)
    h, w = visible.shape
    for dy in range(3):
        for dx in range(3):
            if (dx, dy) != (1, 1):
                boundary |= visible != padded[dy:dy + h, dx:dx + w]
    return visible, partial, boundary


def main() -> dict[str, object]:
    fixture_dir = ROOT / "fixtures"
    capture_dir = ROOT / "captures" / "core-oldtv-alpha"
    fixture_dir.mkdir(parents=True, exist_ok=True)
    capture_dir.mkdir(parents=True, exist_ok=True)
    all_results = []
    fixture_hashes = {}

    for width, height in SIZES:
        size_rows = []
        pair_images = {pattern: make_pair(width, height, pattern) for pattern in PATTERNS}
        if width == REPRESENTATIVE_WIDTH:
            uniform_a, uniform_b = pair_images["uniform"]
            atlas = Image.new("RGBA", (width * 2, height), (0, 0, 0, 0))
            atlas.paste(Image.fromarray(uniform_a, "RGBA"), (0, 0))
            atlas.paste(Image.fromarray(uniform_b, "RGBA"), (width, 0))
            fixture_path = fixture_dir / "core-oldtv-alpha-hidden-rgb-control.png"
            atlas.save(fixture_path)
            fixture_hashes[fixture_path.name] = hashlib.sha256(fixture_path.read_bytes()).hexdigest()

        for pattern, (source_a, source_b) in pair_images.items():
            alpha = source_a[:, :, 3]
            visible, partial, boundary = edge_masks(alpha)
            visible_rgb_pair_equal = bool(np.array_equal(
                source_a[visible, :3], source_b[visible, :3]))
            alpha_pair_equal = bool(np.array_equal(source_a[:, :, 3], source_b[:, :, 3]))
            for fringe in FRINGES:
                out_a = apply_oldtv_fringe(source_a, fringe)
                out_b = apply_oldtv_fringe(source_b, fringe)
                alpha_diff = np.abs(out_a[:, :, 3].astype(np.int16) - alpha.astype(np.int16))
                row: dict[str, object] = {
                    "width": width,
                    "height": height,
                    "hidden RGB pattern": pattern,
                    "fringe": fringe,
                    "object pixels": int(np.count_nonzero(visible)),
                    "partial-alpha pixels": int(np.count_nonzero(partial)),
                    "boundary pixels": int(np.count_nonzero(boundary)),
                    "visible source RGB pair equal": visible_rgb_pair_equal,
                    "source alpha planes equal": alpha_pair_equal,
                    "alpha max difference": int(np.max(alpha_diff)),
                    "alpha changed pixel ratio": float(np.mean(alpha_diff != 0)),
                }
                for bg_name, background in BACKGROUNDS.items():
                    comp_a = composite_code(out_a, background)
                    comp_b = composite_code(out_b, background)
                    row[f"{bg_name} whole pair delta"] = summarize_pair(comp_a, comp_b, np.ones_like(visible))
                    row[f"{bg_name} boundary pair delta"] = summarize_pair(comp_a, comp_b, boundary)
                    row[f"{bg_name} partial-alpha pair delta"] = summarize_pair(comp_a, comp_b, partial)
                size_rows.append(row)

        all_results.extend(size_rows)
        if width == REPRESENTATIVE_WIDTH:
            label_h = 22
            fringe_values = (0.0, 0.3, 1.0)
            sheet = Image.new("RGB", (7 * width, len(fringe_values) * (height + label_h)), (28, 29, 33))
            draw = ImageDraw.Draw(sheet)
            font = ImageFont.load_default()
            source_a, source_b = pair_images["uniform"]
            for row_index, fringe in enumerate(fringe_values):
                out_a = apply_oldtv_fringe(source_a, fringe)
                out_b = apply_oldtv_fringe(source_b, fringe)
                panels = [
                    ("source magenta-hidden", source_a), ("source green-hidden", source_b),
                    (f"F={fringe:g} magenta", out_a), (f"F={fringe:g} green", out_b),
                ]
                mid_a = composite_code(out_a, BACKGROUNDS["mid-gray"])
                mid_b = composite_code(out_b, BACKGROUNDS["mid-gray"])
                diff = np.uint8(np.clip(np.abs(mid_a - mid_b) * 4 * 255 + .5, 0, 255))
                diff_rgba = np.dstack((diff, np.full((height, width), 255, dtype=np.uint8)))
                panels.extend((("pair diff ×4", Image.fromarray(diff_rgba, "RGBA")),))
                # Add both outputs on white to expose partially transparent boundary colors.
                white_a = Image.fromarray(
                    np.uint8(np.clip(composite_code(out_a, BACKGROUNDS["white"]) * 255 + .5, 0, 255)), "RGB")
                white_b = Image.fromarray(
                    np.uint8(np.clip(composite_code(out_b, BACKGROUNDS["white"]) * 255 + .5, 0, 255)), "RGB")
                panels.extend((("white bg magenta", white_a), ("white bg green", white_b)))
                y0 = row_index * (height + label_h)
                for col, (label, item) in enumerate(panels):
                    x0 = col * width
                    draw.text((x0 + 3, y0 + 3), label, fill=(245, 245, 245), font=font)
                    if isinstance(item, np.ndarray):
                        image = Image.fromarray(item, "RGBA")
                    else:
                        image = item
                    if image.mode == "RGBA":
                        bg = Image.new("RGBA", image.size, (128, 128, 128, 255))
                        image = Image.alpha_composite(bg, image).convert("RGB")
                    sheet.paste(image.convert("RGB"), (x0, y0 + label_h))
            sheet.save(capture_dir / "core-oldtv-alpha-boundary-sheet.png")

    report = {
        "source": "ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp",
        "fixture SHA-256": fixture_hashes,
        "source model": "Isolate fringe only: curvature=0, flicker=0, scanline density=0. splitX=fringe*5*(abs(x-cx)/cx); R samples x-splitX, B x+splitX, int conversion truncates toward zero, coordinates clamp. G and alpha stay pixel-local.",
        "input contract assumption": "Synthetic RGB is treated as straight color for display compositing because VideoFrame exposes separate planes without alpha association here. Results do not establish premultiplied-input behavior.",
        "compositing": "sRGB code-value source-over on black, mid-gray, white; normalized [0,1].",
        "limits": "Source-derived Python model only, no Core C++ runtime, GPU route, performance, product caller, or visual acceptance threshold. Hidden-RGB pair changes alpha-zero RGB only; visible source RGBA is identical.",
        "conditions": len(all_results),
        "results": all_results,
    }
    metrics_path = capture_dir / "core-oldtv-alpha-metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    manifest = {
        "origin": "Deterministic synthetic alpha silhouettes generated by core_oldtv_alpha_boundary_study.py; no external assets.",
        "sizes": [list(size) for size in SIZES],
        "fringe values": list(FRINGES),
        "hidden RGB pairs": list(PATTERNS),
        "fixture SHA-256": fixture_hashes,
        "metrics SHA-256": hashlib.sha256(metrics_path.read_bytes()).hexdigest(),
    }
    (fixture_dir / "core-oldtv-alpha-hidden-rgb-control.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {result['conditions']} OldTV alpha/fringe cases")
