"""Measure Core chromatic clamp response for centered and border-touching content."""

from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from core_chromatic_angle_study import (  # noqa: E402
    HEIGHT,
    WIDTH,
    apply_core_model,
    composite_code,
    round_away_from_zero,
)


LAYOUTS = {
    "center": (88, 56, "No shape touches the canvas boundary."),
    "left": (0, 56, "Shape touches the left boundary."),
    "right": (176, 56, "Shape touches the right boundary."),
    "top": (88, 0, "Shape touches the top boundary."),
    "bottom": (88, 112, "Shape touches the bottom boundary."),
    "top-left": (0, 0, "Shape touches the top and left boundaries."),
}
AMOUNTS = (1.0, 4.0, 16.0, 50.0)
ANGLES = (0.0, 45.0, 90.0, 135.0)
BACKGROUNDS = {"black": (0, 0, 0), "mid-gray": (128, 128, 128), "white": (255, 255, 255)}


def make_layout(name: str, x0: int, y0: int) -> Image.Image:
    # Hostile colors under fully transparent pixels expose sampled hidden RGB.
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    hidden = np.asarray(((255, 0, 255), (10, 245, 30), (0, 60, 255), (245, 210, 0)), dtype=np.uint8)
    rgba = np.empty((HEIGHT, WIDTH, 4), dtype=np.uint8)
    rgba[:, :, :3] = hidden[((xx // 8 + yy // 8) % len(hidden)).astype(np.int32)]
    rgba[:, :, 3] = 0

    # The same asymmetric, multicolor object is translated to six canvas positions.
    local_y, local_x = np.mgrid[0:80, 0:80]
    t = local_x.astype(np.float32) / 79.0
    v = local_y.astype(np.float32) / 79.0
    local_rgb = np.empty((80, 80, 3), dtype=np.uint8)
    local_rgb[:, :, 0] = np.uint8(np.clip(38.0 + 205.0 * t, 0, 255))
    local_rgb[:, :, 1] = np.uint8(np.clip(218.0 - 172.0 * t, 0, 255))
    local_rgb[:, :, 2] = np.uint8(np.clip(50.0 + 170.0 * v, 0, 255))
    # Narrow channel-coded bars and a diagonal make the shifted channels easy to distinguish.
    for x in (12, 24, 36, 48, 60, 72):
        local_rgb[:, x:x + 2] = (245, 45, 220)
    for y in (16, 32, 48, 64):
        local_rgb[y:y + 2, :] = (20, 225, 250)
    for i in range(80):
        local_rgb[i, i] = (250, 250, 250)
    rgba[y0:y0 + 80, x0:x0 + 80, :3] = local_rgb

    # Supersampled polygon gives the same small antialiased alpha edge in every layout.
    scale = 4
    mask_hi = Image.new("L", (WIDTH * scale, HEIGHT * scale), 0)
    points = ((0, 28), (13, 8), (34, 0), (61, 6), (78, 24),
              (71, 48), (79, 67), (57, 79), (38, 68), (21, 77), (7, 57))
    draw = ImageDraw.Draw(mask_hi)
    draw.polygon(tuple(((x0 + x) * scale, (y0 + y) * scale) for x, y in points), fill=255)
    mask = mask_hi.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    rgba[:, :, 3] = np.asarray(mask, dtype=np.uint8)
    return Image.fromarray(rgba, "RGBA")


def apply_zero_outside_model(source: Image.Image, amount: float, angle: float) -> tuple[Image.Image, dict[str, object]]:
    """Diagnostic counterfactual: use RGB=0 only for out-of-frame samples."""
    rgba = np.asarray(source.convert("RGBA"), dtype=np.float32) / 255.0
    y, x = np.mgrid[0:HEIGHT, 0:WIDTH]
    radians = math.radians(angle)
    sx = round_away_from_zero(amount * math.cos(radians))
    sy = round_away_from_zero(amount * math.sin(radians))
    red_raw_x, red_raw_y = x + sx, y + sy
    blue_raw_x, blue_raw_y = x - sx, y - sy
    red_valid = (red_raw_x >= 0) & (red_raw_x < WIDTH) & (red_raw_y >= 0) & (red_raw_y < HEIGHT)
    blue_valid = (blue_raw_x >= 0) & (blue_raw_x < WIDTH) & (blue_raw_y >= 0) & (blue_raw_y < HEIGHT)
    red_x, red_y = np.clip(red_raw_x, 0, WIDTH - 1), np.clip(red_raw_y, 0, HEIGHT - 1)
    blue_x, blue_y = np.clip(blue_raw_x, 0, WIDTH - 1), np.clip(blue_raw_y, 0, HEIGHT - 1)
    output = rgba.copy()
    output[:, :, 0] = np.where(red_valid, rgba[red_y, red_x, 0], 0.0)
    output[:, :, 2] = np.where(blue_valid, rgba[blue_y, blue_x, 2], 0.0)
    output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
    return Image.fromarray(output8, "RGBA"), {
        "shift R xy": [sx, sy],
        "shift B xy": [-sx, -sy],
        "red clamp destination ratio": float(np.mean(~red_valid)),
        "blue clamp destination ratio": float(np.mean(~blue_valid)),
        "any clamp destination ratio": float(np.mean(~(red_valid & blue_valid))),
        "any clamp destination mask": ~(red_valid & blue_valid),
    }


def summarize_delta(reference: np.ndarray, candidate: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    if not np.any(mask):
        return {"pixel count": 0, "MAE code value": 0.0, "P95 absolute channel code value": 0.0,
                "changed pixel ratio >1/255": 0.0, "channel MAE code value": [0.0, 0.0, 0.0]}
    delta = np.abs(candidate[mask] - reference[mask])
    return {
        "pixel count": int(np.count_nonzero(mask)),
        "MAE code value": float(np.mean(delta)),
        "P95 absolute channel code value": float(np.percentile(delta, 95)),
        "changed pixel ratio >1/255": float(np.mean(np.max(delta, axis=1) > 1.0 / 255.0)),
        "channel MAE code value": np.mean(delta, axis=0).tolist(),
    }


def main() -> dict[str, object]:
    output_dir = ROOT / "captures" / "core-chromatic-boundary"
    fixture_dir = ROOT / "fixtures"
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture_dir.mkdir(parents=True, exist_ok=True)

    layout_images = {name: make_layout(name, x, y) for name, (x, y, _) in LAYOUTS.items()}
    fixture_atlas = Image.new("RGBA", (WIDTH * 3, HEIGHT * 2), (0, 0, 0, 0))
    for index, image in enumerate(layout_images.values()):
        fixture_atlas.paste(image, ((index % 3) * WIDTH, (index // 3) * HEIGHT))
    fixture_path = fixture_dir / "core-chromatic-boundary-control.png"
    fixture_atlas.save(fixture_path)
    fixture_sha = hashlib.sha256(fixture_path.read_bytes()).hexdigest()
    fixture_manifest = {
        "fixture": fixture_path.name,
        "sha256": fixture_sha,
        "dimensions": "768x384 RGBA8 atlas; six 256x192 independent cases",
        "origin": "Deterministic synthetic fixture generated by tools/core_chromatic_boundary_study.py; no external assets.",
        "layouts": {name: {"shape origin xy": [x, y], "description": description} for name, (x, y, description) in LAYOUTS.items()},
        "content": "Same asymmetric RGB-gradient sprite, 4x supersampled partial-alpha silhouette, channel-coded stripes, and hostile alpha-zero hidden-RGB checker in each case.",
        "comparison": "Core-source-equivalent integer R/B shifts with edge clamp versus diagnostic RGB=0 for out-of-frame samples. The zero-outside variant is a measurement counterfactual, not a proposed effect.",
        "compositing": "Normalized sRGB code-value alpha composite over black, mid-gray, white; not linear-light.",
    }
    (fixture_dir / "core-chromatic-boundary-control.json").write_text(
        json.dumps(fixture_manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )

    rows = []
    backgrounds = {name: tuple(c / 255.0 for c in color) for name, color in BACKGROUNDS.items()}
    for layout_name, source in layout_images.items():
        src_rgba = np.asarray(source, dtype=np.uint8)
        alpha = src_rgba[:, :, 3]
        visible = alpha > 0
        partial = (alpha > 0) & (alpha < 255)
        padded = np.pad(visible, 1, constant_values=False)
        object_edge = np.zeros_like(visible)
        for dy in range(3):
            for dx in range(3):
                if dx == 1 and dy == 1:
                    continue
                object_edge |= visible != padded[dy:dy + HEIGHT, dx:dx + WIDTH]
        layout_rows = []
        for amount in AMOUNTS:
            for angle in ANGLES:
                clamped_output, core_meta = apply_core_model(source, amount, angle)
                zero_output, zero_meta = apply_zero_outside_model(source, amount, angle)
                clamp_mask = zero_meta.pop("any clamp destination mask")
                clamp_visible = clamp_mask & visible
                clamp_partial = clamp_mask & partial
                output_alpha = np.asarray(clamped_output.getchannel("A"), dtype=np.uint8)
                metrics: dict[str, object] = {
                    "amount px": amount,
                    "angle degrees": angle,
                    "shift R xy": zero_meta["shift R xy"],
                    "shift B xy": zero_meta["shift B xy"],
                    "red clamp destination ratio": zero_meta["red clamp destination ratio"],
                    "blue clamp destination ratio": zero_meta["blue clamp destination ratio"],
                    "any clamp destination ratio": zero_meta["any clamp destination ratio"],
                    "clamp destination pixels": int(np.count_nonzero(clamp_mask)),
                    "object pixels": int(np.count_nonzero(visible)),
                    "object edge pixels": int(np.count_nonzero(object_edge)),
                    "partial-alpha pixels": int(np.count_nonzero(partial)),
                    "object pixels at clamped destinations": int(np.count_nonzero(clamp_visible)),
                    "partial-alpha pixels at clamped destinations": int(np.count_nonzero(clamp_partial)),
                    "alpha max code difference": int(np.max(np.abs(output_alpha.astype(np.int16) - alpha.astype(np.int16)))),
                    "alpha changed pixel ratio": float(np.mean(output_alpha != alpha)),
                }
                for bg_name, background in backgrounds.items():
                    source_comp = composite_code(source, BACKGROUNDS[bg_name])
                    actual_comp = composite_code(clamped_output, BACKGROUNDS[bg_name])
                    zero_comp = composite_code(zero_output, BACKGROUNDS[bg_name])
                    actual_delta = np.abs(actual_comp - source_comp)
                    policy_delta = np.abs(actual_comp - zero_comp)
                    for region_name, mask in (
                        ("whole", np.ones_like(visible)),
                        ("object", visible),
                        ("object-edge", object_edge),
                        ("partial-alpha", partial),
                        ("clamp-destinations", clamp_mask),
                        ("object-at-clamped-destinations", clamp_visible),
                        ("partial-alpha-at-clamped-destinations", clamp_partial),
                    ):
                        metrics[f"{bg_name} actual-vs-source {region_name}"] = summarize_delta(source_comp, actual_comp, mask)
                        metrics[f"{bg_name} clamp-vs-zero-outside {region_name}"] = summarize_delta(zero_comp, actual_comp, mask)
                layout_rows.append(metrics)
        rows.append({
            "layout": layout_name,
            "description": LAYOUTS[layout_name][2],
            "source alpha zero pixels": int(np.count_nonzero(alpha == 0)),
            "source partial-alpha pixels": int(np.count_nonzero(partial)),
            "cases": layout_rows,
        })

    report = {
        "fixture": fixture_manifest,
        "protocol": "Same source sprite translated to centered/edge/corner positions. Apply exact integer shift and clamp equations from ArtifactCore; compare actual clamp with an RGB-zero-outside counterfactual. Report sRGB code-value composites by whole/object/edge/partial-alpha/clamped regions over black/mid-gray/white.",
        "model limits": "Python source-derived CPU model; no C++/GPU execution, arbitrary float/HDR, alpha association matrix, approved visual target, or performance measurement. Zero-outside is diagnostic only.",
        "cases per layout": len(AMOUNTS) * len(ANGLES),
        "layouts": rows,
    }
    (output_dir / "core-chromatic-boundary-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )

    display_amount = 50.0
    display_angles = (0.0, 45.0, 90.0, 135.0)
    tile_w, tile_h, label_h = WIDTH, HEIGHT, 22
    sheet = Image.new("RGB", (5 * tile_w, len(LAYOUTS) * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for row, (layout_name, source) in enumerate(layout_images.items()):
        candidates = [("source", source)]
        candidates.extend((f"A50 {angle:g}deg", apply_core_model(source, display_amount, angle)[0]) for angle in display_angles)
        for col, (label, image) in enumerate(candidates):
            top, left = row * (tile_h + label_h), col * tile_w
            draw.text((left + 3, top + 3), f"{layout_name} | {label}", fill=(245, 245, 245), font=ImageFont.load_default())
            display = np.uint8(np.clip(composite_code(image, (128, 128, 128)) * 255.0 + 0.5, 0, 255))
            sheet.paste(Image.fromarray(display, "RGB"), (left, top + label_h))
    sheet.save(output_dir / "core-chromatic-boundary-sheet.png")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Saved {result['cases per layout']} clamp cases for {len(result['layouts'])} content layouts")
