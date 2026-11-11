"""Evaluate resize-only alpha policies on a controlled synthetic RGBA chart."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from alpha_resize_association_isolation_study import ALPHA_MODES, ASSOCIATIONS, BACKGROUNDS, CAPTURE, metrics, resize  # noqa: E402
from adaptive_palette_temporal_stability_study import get_masks  # noqa: E402
from adaptive_palette_alpha_resize_study import composite  # noqa: E402

FIXTURE = ROOT / "fixtures" / "alpha-resize-edge-chart.png"
SCALES = (0.25, 0.375, 0.5, 0.75, 1.0)
REGIONS = {
    "hard rectangles": (20, 20, 750, 115),
    "curve diagonal": (35, 120, 725, 325),
    "thin strands partial alpha": (15, 315, 490, 495),
    "narrow gap unlike colors": (555, 338, 720, 495),
}


def crop_metrics(output_rgb: np.ndarray, output_alpha: np.ndarray, source: np.ndarray, region: tuple[int, int, int, int]) -> dict[str, object]:
    x0, y0, x1, y1 = region
    rgb = output_rgb[y0:y1, x0:x1]
    alpha = output_alpha[y0:y1, x0:x1]
    src = source[y0:y1, x0:x1]
    result = metrics(rgb, alpha, src)
    masks = get_masks(src[:, :, 3])
    for bg, color in BACKGROUNDS.items():
        rendered = composite(rgb, alpha, color)
        reference = composite(src[:, :, :3], src[:, :, 3], color)
        result[bg + " RMSE by ROI"] = {
            name: float(np.mean((rendered[mask] - reference[mask]) ** 2) ** .5) if np.any(mask) else 0.0
            for name, mask in masks.items()
        }
    return result


def main() -> dict[str, object]:
    with Image.open(FIXTURE) as image:
        source = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    h, w = source.shape[:2]
    scales = {}
    for scale in SCALES:
        conditions = {}
        images = {}
        for association in ASSOCIATIONS:
            for alpha_mode in ALPHA_MODES:
                rgb, alpha, low_alpha = resize(source, scale, association, alpha_mode)
                if rgb.shape != source[:, :, :3].shape or alpha.shape != source[:, :, 3].shape:
                    raise ValueError("resize output dimensions changed")
                key = f"{association}/{alpha_mode}"
                images[key] = np.dstack((rgb, alpha))
                conditions[key] = {
                    "whole image": metrics(rgb, alpha, source),
                    "regions": {name: crop_metrics(rgb, alpha, source, bounds) for name, bounds in REGIONS.items()},
                    "low-resolution alpha changed pixel ratio": float(np.mean(low_alpha != 255)) if scale < 1 else 0.0,
                }
        scale_text = f"{scale:.3f}"
        scales[scale_text] = {
            "conditions": conditions,
            "contact sheet": str(make_sheet(source, images, scale_text).relative_to(ROOT)),
        }
    report = {
        "question": "On a controlled chart, which resize-only changes come from RGB alpha association versus preserving or resampling output alpha?",
        "fixture": "fixtures/alpha-resize-edge-chart.png",
        "method": "Deterministic 768x512 chart with hard colored blocks, antialiased curve/diagonal, 1-8px strands/gaps and four partial-alpha levels. Alpha-zero RGB is forced to (9,240,17). BOX downscale then nearest upscale at five scales; straight/premultiplied RGB and source/resized alpha are factorially compared. All source-over error is computed after linear-light composition on black/mid-gray/white, reported by visible/partial/edge ROI and chart region.",
        "regions": {key: list(value) for key, value in REGIONS.items()},
        "limits": "Synthetic control only, no moving sequence or real-world material. Pillow uint8 model; premultiply before BOX is quantized. No product runtime, Core/GPU parity, performance or perceptual approval claim.",
        "scales": scales,
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "alpha-resize-synthetic-metrics.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


def make_sheet(source: np.ndarray, variants: dict[str, np.ndarray], scale: str) -> Path:
    keys = ["source", *variants.keys()]
    tile = 192
    sheet = Image.new("RGB", (tile * 4, (tile + 20) * len(keys)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, key in enumerate(keys):
        image = source if key == "source" else variants[key]
        for col, bg in enumerate(((0, 0, 0, 255), (128, 128, 128, 255), (255, 255, 255, 255))):
            x, y = col * tile, row * (tile + 20)
            if col == 0:
                draw.text((x + 2, y + 3), key, fill=(245, 245, 245), font=font)
            panel = Image.fromarray(image, "RGBA").resize((tile, tile), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, bg), panel)
            sheet.paste(panel.convert("RGB"), (x, y + 20))
    path = CAPTURE / f"alpha-resize-synthetic-{scale}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)
    return path


if __name__ == "__main__":
    result = main()
    print(f"Evaluated synthetic alpha resize chart at {len(result['scales'])} scales.")
