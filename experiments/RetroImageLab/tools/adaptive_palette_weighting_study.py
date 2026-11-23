"""Compare deterministic adaptive palette weighting on licensed RGBA assets."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "captures" / "adaptive-palette-weighting"
sys_path = str(ROOT)
import sys
sys.path.insert(0, sys_path)
from retro_lab import PALETTES, linear_to_srgb, nearest_palette, srgb_to_linear

ASSETS = (
    ("eval-corpus/cc0-white-clover/2e325727cf8369e7.png", "eval-corpus/cc0-white-clover/manifest.json", "2e325727cf8369e7.png", "clover_full"),
    ("eval-inputs/cc0-feathers/feather_silver.png", "eval-inputs/cc0-feathers/provenance.json", "feather_silver.png", "silver_feather"),
    ("eval-inputs/cc0-hair-cutouts/summer_hair_female.png", "eval-inputs/cc0-hair-cutouts/provenance.json", "summer_hair_female.png", "pixel_hair_atlas"),
)
LEVELS = (4, 8, 16)
DOMAINS = ("srgb", "linear")
WEIGHTINGS = ("visible-pixel frequency", "alpha coverage")
TILE = 240


def load_asset(spec: tuple[str, str, str, str]) -> tuple[np.ndarray, dict[str, object], str]:
    rel_path, rel_manifest, filename, _ = spec
    path, manifest_path = ROOT / rel_path, ROOT / rel_manifest
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if isinstance(manifest, list):
        entry = next(row for row in manifest if row["file"] == filename)
        expected = entry["sha256"]
    else:
        entry = manifest[filename]
        expected = entry["source_sha256"]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != expected:
        raise ValueError(f"provenance SHA-256 mismatch: {path}")
    with Image.open(path) as image:
        rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    return rgba, entry, digest


def median_cut_palette(rgb8: np.ndarray, alpha8: np.ndarray, count: int,
                       weighting: str, domain: str) -> np.ndarray:
    visible = alpha8 > 0
    rgb = rgb8[visible].astype(np.float32) / 255.0
    alpha = alpha8[visible].astype(np.float32) / 255.0
    unique8, inverse = np.unique(rgb8[visible].reshape(-1, 3), axis=0, return_inverse=True)
    unique = unique8.astype(np.float32) / 255.0
    points = srgb_to_linear(unique) if domain == "linear" else unique
    pixel_weights = alpha if weighting == "alpha coverage" else np.ones_like(alpha)
    weights = np.bincount(inverse, weights=pixel_weights, minlength=len(unique)).astype(np.float64)
    boxes = [np.arange(len(unique), dtype=np.int32)]

    while len(boxes) < count:
        candidates = []
        for box_index, indices in enumerate(boxes):
            if len(indices) < 2:
                continue
            values = points[indices]
            ranges = values.max(axis=0) - values.min(axis=0)
            axis = int(np.argmax(ranges))
            score = float(ranges[axis] * np.sum(weights[indices]))
            if score > 0:
                candidates.append((score, box_index, axis))
        if not candidates:
            break
        _, box_index, axis = max(candidates, key=lambda item: (item[0], -item[1], -item[2]))
        indices = boxes.pop(box_index)
        order = np.argsort(points[indices, axis], kind="stable")
        sorted_indices = indices[order]
        cumulative = np.cumsum(weights[sorted_indices])
        split = int(np.searchsorted(cumulative, cumulative[-1] * 0.5, side="left") + 1)
        split = min(max(1, split), len(sorted_indices) - 1)
        boxes.extend((sorted_indices[:split], sorted_indices[split:]))

    palette = []
    for indices in boxes:
        local_weights = weights[indices]
        centroid = np.average(points[indices], axis=0, weights=local_weights)
        palette.append(linear_to_srgb(centroid) if domain == "linear" else centroid)
    return np.asarray(palette, dtype=np.float32)


def assign_palette(rgb8: np.ndarray, palette: np.ndarray, domain: str) -> tuple[np.ndarray, np.ndarray]:
    rgb = rgb8.astype(np.float32) / 255.0
    source_domain = srgb_to_linear(rgb) if domain == "linear" else rgb
    palette_domain = srgb_to_linear(palette) if domain == "linear" else palette
    indices = np.empty(rgb.shape[:2], dtype=np.uint8)
    output = np.empty_like(rgb)
    for y0 in range(0, rgb.shape[0], 64):
        block = source_domain[y0:y0 + 64]
        distances = np.sum((block[:, :, None, :] - palette_domain[None, None, :, :]) ** 2, axis=3)
        selected = np.argmin(distances, axis=2)
        indices[y0:y0 + 64] = selected
        output[y0:y0 + 64] = palette[selected]
    return output, indices


def masks(alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    h, w = alpha.shape
    padded = np.pad(visible, 1, constant_values=True)
    edge = partial.copy()
    for dy in range(3):
        for dx in range(3):
            if (dy, dx) != (1, 1):
                edge |= visible != padded[dy:dy + h, dx:dx + w]
    return {"visible": visible, "partial alpha": partial, "alpha edge": edge}


def error_stats(source: np.ndarray, output: np.ndarray, alpha: np.ndarray,
                regions: dict[str, np.ndarray]) -> dict[str, object]:
    source_linear, output_linear = srgb_to_linear(source), srgb_to_linear(output)
    a = alpha.astype(np.float32)[:, :, None] / 255.0
    result: dict[str, object] = {}
    backgrounds = {"black": 0.0, "mid-gray": 0.5, "white": 1.0}
    for bg_name, bg in backgrounds.items():
        bg_linear = np.full((1, 1, 3), bg, dtype=np.float32)
        src_comp = linear_to_srgb(source_linear * a + srgb_to_linear(bg_linear) * (1.0 - a))
        out_comp = linear_to_srgb(output_linear * a + srgb_to_linear(bg_linear) * (1.0 - a))
        per_region = {}
        for region, mask in regions.items():
            delta = (out_comp - src_comp)[mask]
            per_region[region] = {
                "pixel count": int(np.count_nonzero(mask)),
                "display sRGB RMSE": float(np.mean(delta ** 2) ** 0.5) if delta.size else 0.0,
                "display sRGB MAE": float(np.mean(np.abs(delta))) if delta.size else 0.0,
            }
        result[bg_name] = per_region
    return result


def crop_for(alpha: np.ndarray) -> tuple[int, int, int, int]:
    ys, xs = np.nonzero(alpha > 0)
    cx, cy = int((xs.min() + xs.max()) // 2), int((ys.min() + ys.max()) // 2)
    h, w = alpha.shape
    size = min(TILE, w, h)
    x0, y0 = max(0, min(w - size, cx - size // 2)), max(0, min(h - size, cy - size // 2))
    return x0, y0, x0 + size, y0 + size


def make_sheet(source_rgba: np.ndarray, outputs: dict[str, np.ndarray], crop: tuple[int, int, int, int], path: Path) -> None:
    columns, rows, label_h = 4, 4, 24
    sheet = Image.new("RGB", (columns * TILE, rows * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    source = Image.fromarray(source_rgba, "RGBA")
    tiles = [("source", source)] + [(key, Image.fromarray(value, "RGBA")) for key, value in outputs.items()]
    for index, (label, image) in enumerate(tiles):
        col, row = index % columns, index // columns
        x, y = col * TILE, row * (TILE + label_h)
        draw.text((x + 4, y + 4), label, fill=(245, 245, 245), font=font)
        panel = image.crop(crop)
        panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
        sheet.paste(panel.convert("RGB").resize((TILE, TILE), Image.Resampling.NEAREST), (x, y + label_h))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba, provenance, digest = load_asset(spec)
    label = spec[3]
    rgb8, alpha = rgba[:, :, :3], rgba[:, :, 3]
    source = rgb8.astype(np.float32) / 255.0
    region_masks = masks(alpha)
    crop = crop_for(alpha)
    outputs: dict[str, np.ndarray] = {}
    conditions: dict[str, object] = {}
    for weighting in WEIGHTINGS:
        for domain in DOMAINS:
            for count in LEVELS:
                palette = median_cut_palette(rgb8, alpha, count, weighting, domain)
                quantized, indices = assign_palette(rgb8, palette, domain)
                output_rgba = np.concatenate((quantized, alpha[:, :, None].astype(np.float32) / 255.0), axis=2)
                output_u8 = np.uint8(np.clip(output_rgba * 255.0 + 0.5, 0, 255))
                if not np.array_equal(output_u8[:, :, 3], alpha):
                    raise ValueError(f"alpha changed: {label}/{weighting}/{domain}/{count}")
                key = f"adaptive {count} / {weighting} / {domain}"
                outputs[key] = output_u8
                visible_indices = indices[region_masks["visible"]]
                counts = np.bincount(visible_indices, minlength=len(palette))
                conditions[key] = {
                    "palette RGB8": [list(map(int, np.uint8(np.clip(color * 255 + .5, 0, 255)))) for color in palette],
                    "palette entries used": int(np.count_nonzero(counts)),
                    "dominant visible share": float(counts.max() / max(1, counts.sum())),
                    "display error by background": error_stats(source, quantized, alpha, region_masks),
                }
    fixed = PALETTES["pico8"]
    fixed_rgb, fixed_indices = assign_palette(rgb8, fixed, "srgb")
    fixed_rgba = np.concatenate((fixed_rgb, alpha[:, :, None].astype(np.float32) / 255.0), axis=2)
    fixed_u8 = np.uint8(np.clip(fixed_rgba * 255.0 + 0.5, 0, 255))
    if not np.array_equal(fixed_u8[:, :, 3], alpha):
        raise ValueError(f"fixed PICO-8 alpha changed: {label}")
    outputs["fixed PICO-8 / sRGB / no dither"] = fixed_u8
    conditions["fixed PICO-8 / sRGB / no dither"] = {
        "palette RGB8": [list(map(int, np.uint8(np.clip(color * 255 + .5, 0, 255)))) for color in fixed],
        "palette entries used": int(np.unique(fixed_indices[region_masks["visible"]]).size),
        "dominant visible share": float(np.max(np.bincount(fixed_indices[region_masks["visible"]], minlength=len(fixed))) / np.count_nonzero(region_masks["visible"])),
        "display error by background": error_stats(source, fixed_rgb, alpha, region_masks),
    }
    sheet_path = CAPTURE / f"{label}-adaptive-palette.png"
    make_sheet(rgba, outputs, crop, sheet_path)
    return {
        "asset": label,
        "source file": spec[0],
        "source SHA-256": digest,
        "provenance": provenance,
        "dimensions": list(rgba.shape[1::-1]),
        "alpha ratios": {
            "transparent": float(np.mean(alpha == 0)),
            "partial": float(np.mean((alpha > 0) & (alpha < 255))),
            "visible": float(np.mean(alpha > 0)),
            "edge": float(np.mean(region_masks["alpha edge"])),
        },
        "crop xyxy": list(crop),
        "contact sheet": str(sheet_path.relative_to(ROOT)),
        "conditions": conditions,
    }


def main() -> dict[str, object]:
    report = {
        "question": "How do visible-pixel versus alpha-coverage weighting and sRGB versus linear median cut affect adaptive palette mapping on diverse RGBA assets?",
        "method": "Deterministic weighted median cut over unique visible RGB8 colors. Each split chooses the color axis with the largest range and divides at the weighted median; each box becomes a weighted centroid. Palette assignment uses nearest Euclidean distance in the same domain. Alpha is passed through unchanged. Fixed PICO-8 is a comparator.",
        "levels": list(LEVELS),
        "limits": "Three provenance-verified CC0 families: one natural clover cutout, one silver feather illustration, and one pixel-art hair atlas. Source RGB is interpreted as sRGB code values; embedded profiles are not applied. Display errors are numerical deltas, not perceptual quality scores. This median-cut implementation is an explicit study algorithm, not a claim about Pillow, Core, or Artifact quantization. No product/runtime/performance evaluation.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-weighting-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured {len(report['assets'])} CC0 assets across {len(LEVELS)} palette sizes, {len(WEIGHTINGS)} weighting modes, and {len(DOMAINS)} color domains.")
