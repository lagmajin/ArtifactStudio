"""Measure fixed-palette occupancy and spatial transitions on licensed assets."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from retro_lab import BAYER_4, PALETTES, linear_to_srgb, nearest_palette, srgb_to_linear

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "captures" / "palette-occupancy-transitions"
STRENGTHS = (0.0, 0.5, 1.0)
DOMAINS = ("srgb", "linear")
TILE = 220
ASSETS = (
    {
        "path": "eval-corpus/cc0-white-clover/2e325727cf8369e7.png",
        "manifest": "eval-corpus/cc0-white-clover/manifest.json",
        "label": "clover_full",
        "manifest_file": "2e325727cf8369e7.png",
    },
    {
        "path": "eval-inputs/cc0-feathers/feather_silver.png",
        "manifest": "eval-inputs/cc0-feathers/provenance.json",
        "label": "silver_feather",
        "manifest_file": "feather_silver.png",
    },
    {
        "path": "eval-inputs/cc0-hair-cutouts/summer_hair_female.png",
        "manifest": "eval-inputs/cc0-hair-cutouts/provenance.json",
        "label": "pixel_hair_atlas",
        "manifest_file": "summer_hair_female.png",
    },
    {
        "path": "eval-corpus/voa/132819fcfd733c00.jpg",
        "manifest": "eval-corpus/voa/manifest.json",
        "label": "voa_photo_internal_only",
        "manifest_file": "132819fcfd733c00.jpg",
    },
)


def load_asset(spec: dict[str, str]) -> tuple[Image.Image, dict[str, object]]:
    path = ROOT / spec["path"]
    manifest_path = ROOT / spec["manifest"]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if isinstance(manifest, list):
        entry = next(item for item in manifest if item["file"] == spec["manifest_file"])
        expected = entry["sha256"]
    else:
        entry = manifest[spec["manifest_file"]]
        expected = entry["source_sha256"]
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"provenance SHA-256 mismatch: {path}")
    source_image = Image.open(path)
    color_metadata = {
        key: source_image.info[key]
        for key in ("gamma", "chromaticity", "srgb")
        if key in source_image.info
    }
    if "icc_profile" in source_image.info:
        color_metadata["embedded ICC profile bytes"] = len(source_image.info["icc_profile"])
    image = source_image.convert("RGBA")
    return image, {**entry, "source file": str(path.relative_to(ROOT)),
                   "source color metadata": color_metadata,
                   "sha256": actual, "sha256 verified": True}


def indices_for_palette(rgb: np.ndarray, palette: np.ndarray, domain: str, strength: float) -> np.ndarray:
    mapped = nearest_palette(rgb, palette, BAYER_4 if strength > 0.0 else None, strength, domain)
    indices = np.full(mapped.shape[:2], -1, dtype=np.int16)
    for index, color in enumerate(palette):
        indices[np.all(mapped == color, axis=2)] = index
    if np.any(indices < 0):
        raise ValueError("palette output could not be mapped back to a palette index")
    return indices


def masks_for_alpha(alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    height, width = alpha.shape
    # Treat the outside of the image as continuation of the frame, so a fully
    # opaque photograph does not acquire a synthetic alpha edge at its border.
    padded = np.pad(visible, 1, constant_values=True)
    edge = partial.copy()
    for dy in range(3):
        for dx in range(3):
            if (dy, dx) != (1, 1):
                edge |= visible != padded[dy:dy + height, dx:dx + width]
    return {"visible": visible, "partial alpha": partial, "edge": edge}


def occupancy_metrics(indices: np.ndarray, palette: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    values = indices[mask]
    counts = np.bincount(values, minlength=len(palette)) if values.size else np.zeros(len(palette), dtype=np.int64)
    probabilities = counts[counts > 0].astype(np.float64) / max(1, values.size)
    entropy = float(-np.sum(probabilities * np.log2(probabilities))) if probabilities.size else 0.0
    return {
        "pixel count": int(values.size),
        "palette colors used": int(np.count_nonzero(counts)),
        "palette utilization": float(np.count_nonzero(counts) / len(palette)),
        "effective colors by entropy": float(2.0 ** entropy),
        "dominant color share": float(np.max(counts) / max(1, values.size)),
        "counts by palette index": [int(count) for count in counts],
        "counts by RGB8": {"#%02X%02X%02X" % tuple(np.uint8(np.clip(color * 255.0 + 0.5, 0, 255))): int(count)
                           for color, count in zip(palette, counts) if count > 0},
    }


def transition_metrics(indices: np.ndarray, alpha: np.ndarray, masks: dict[str, np.ndarray]) -> dict[str, object]:
    left, right = indices[:, :-1], indices[:, 1:]
    left_visible, right_visible = masks["visible"][:, :-1], masks["visible"][:, 1:]
    valid_visible = left_visible & right_visible
    valid_opaque = (alpha[:, :-1] == 255) & (alpha[:, 1:] == 255)
    valid_edge = valid_visible & (masks["edge"][:, :-1] | masks["edge"][:, 1:])
    changed = left != right

    def summarize(valid: np.ndarray, transition_mask: np.ndarray) -> dict[str, float | int]:
        count = int(np.count_nonzero(valid))
        transitions = int(np.count_nonzero(transition_mask & valid))
        return {"neighbor pairs": count, "color changes": transitions,
                "transition rate": float(transitions / count) if count else 0.0}

    return {
        "horizontal visible": summarize(valid_visible, changed),
        "horizontal opaque interior": summarize(valid_opaque, changed),
        "horizontal alpha edge": summarize(valid_edge, changed),
        "vertical visible": summarize(
            masks["visible"][:-1, :] & masks["visible"][1:, :],
            indices[:-1, :] != indices[1:, :],
        ),
    }


def source_error_metrics(source_rgb: np.ndarray, output_rgb: np.ndarray,
                         alpha: np.ndarray, masks: dict[str, np.ndarray]) -> dict[str, object]:
    source_linear = srgb_to_linear(source_rgb)
    output_linear = srgb_to_linear(output_rgb)
    rgb_delta = output_linear - source_linear
    alpha_float = alpha.astype(np.float32)[:, :, None] / 255.0
    composite_delta = rgb_delta * alpha_float

    def summarize(delta: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
        values = delta[mask]
        if not values.size:
            return {"pixel count": 0, "linear RGB MAE": 0.0,
                    "linear RGB RMSE": 0.0, "P95 absolute channel error": 0.0,
                    "changed pixel ratio >1/255": 0.0}
        return {
            "pixel count": int(np.count_nonzero(mask)),
            "linear RGB MAE": float(np.mean(np.abs(values))),
            "linear RGB RMSE": float(np.mean(values ** 2) ** 0.5),
            "P95 absolute channel error": float(np.percentile(np.abs(values), 95)),
            "changed pixel ratio >1/255": float(
                np.mean(np.max(np.abs(values), axis=1) > (1.0 / 255.0))
            ),
        }

    def summarize_display_code(delta: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
        values = summarize(delta, mask)
        return {
            "pixel count": values["pixel count"],
            "sRGB code MAE": values["linear RGB MAE"],
            "sRGB code RMSE": values["linear RGB RMSE"],
            "P95 absolute sRGB code error": values["P95 absolute channel error"],
            "changed pixel ratio >1/255": values["changed pixel ratio >1/255"],
        }

    background_display_errors: dict[str, object] = {}
    backgrounds = {
        "black": (0.0, 0.0, 0.0),
        "mid-gray": (0.5, 0.5, 0.5),
        "white": (1.0, 1.0, 1.0),
    }
    for background_name, background_code in backgrounds.items():
        bg_code = np.asarray(background_code, dtype=np.float32).reshape(1, 1, 3)
        bg_linear = srgb_to_linear(bg_code)
        source_linear_composite = source_linear * alpha_float + bg_linear * (1.0 - alpha_float)
        output_linear_composite = output_linear * alpha_float + bg_linear * (1.0 - alpha_float)
        linear_blend_display_delta = (
            linear_to_srgb(output_linear_composite)
            - linear_to_srgb(source_linear_composite)
        )
        source_code_composite = source_rgb * alpha_float + bg_code * (1.0 - alpha_float)
        output_code_composite = output_rgb * alpha_float + bg_code * (1.0 - alpha_float)
        code_blend_display_delta = output_code_composite - source_code_composite
        background_display_errors[background_name] = {
            "linear-light composite, displayed sRGB code": {
                region: summarize_display_code(linear_blend_display_delta, mask)
                for region, mask in masks.items()
            },
            "sRGB-code-value composite": {
                region: summarize_display_code(code_blend_display_delta, mask)
                for region, mask in masks.items()
            },
        }

    return {
        "visible source RGB": summarize(rgb_delta, masks["visible"]),
        "alpha-composited error": {
            region: summarize(composite_delta, mask)
            for region, mask in masks.items()
        },
        "displayed sRGB delta by background and blend domain": background_display_errors,
        "background dependence": "In linear-light values, identical-alpha source-over error is background-invariant. After encoding the composite to displayed sRGB codes, the numeric delta can depend on background. Direct sRGB-code-value compositing is background-invariant when alpha matches.",
    }


def choose_crop(alpha: np.ndarray, size: int = TILE) -> tuple[int, int, int, int]:
    visible = alpha > 0
    ys, xs = np.nonzero(visible)
    if not xs.size:
        return (0, 0, min(size, alpha.shape[1]), min(size, alpha.shape[0]))
    center_x = int((xs.min() + xs.max()) // 2)
    center_y = int((ys.min() + ys.max()) // 2)
    width, height = alpha.shape[1], alpha.shape[0]
    x0 = max(0, min(width - size, center_x - size // 2))
    y0 = max(0, min(height - size, center_y - size // 2))
    return x0, y0, min(width, x0 + size), min(height, y0 + size)


def build_sheet(source: Image.Image, outputs: dict[str, Image.Image], crop: tuple[int, int, int, int], path: Path) -> None:
    columns = len(PALETTES) * len(DOMAINS)
    rows = len(STRENGTHS)
    label_h = 23
    sheet = Image.new("RGB", (columns * TILE, rows * (TILE + label_h)), (30, 31, 35))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for row, strength in enumerate(STRENGTHS):
        column = 0
        for palette_name in PALETTES:
            for domain in DOMAINS:
                key = f"{palette_name}/{domain}/{strength:.1f}"
                panel = outputs[key].crop(crop)
                panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
                panel = panel.convert("RGB").resize((TILE, TILE), Image.Resampling.NEAREST)
                x0, y0 = column * TILE, row * (TILE + label_h)
                draw.text((x0 + 4, y0 + 4), f"{palette_name} · {domain} · d={strength:.1f}", fill=(245, 245, 245), font=font)
                sheet.paste(panel, (x0, y0 + label_h))
                column += 1
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def study_asset(spec: dict[str, str]) -> dict[str, object]:
    source, provenance = load_asset(spec)
    source_rgba = np.asarray(source, dtype=np.uint8)
    rgb = source_rgba[:, :, :3].astype(np.float32) / 255.0
    alpha = source_rgba[:, :, 3]
    masks = masks_for_alpha(alpha)
    crop = choose_crop(alpha)
    rows: dict[str, object] = {}
    outputs: dict[str, Image.Image] = {}
    for palette_name, palette in PALETTES.items():
        for domain in DOMAINS:
            for strength in STRENGTHS:
                indices = indices_for_palette(rgb, palette, domain, strength)
                quantized = palette[indices]
                output_rgba = np.concatenate((quantized, alpha[:, :, None].astype(np.float32) / 255.0), axis=2)
                output_image = Image.fromarray(np.uint8(np.clip(output_rgba * 255.0 + 0.5, 0, 255)), "RGBA")
                if not np.array_equal(np.asarray(output_image)[:, :, 3], alpha):
                    raise ValueError(f"alpha changed during palette model: {spec['label']}/{palette_name}/{domain}/{strength}")
                key = f"{palette_name}/{domain}/{strength:.1f}"
                visible_occupancy = occupancy_metrics(indices, palette, masks["visible"])
                visible_pixels = max(1, int(np.count_nonzero(masks["visible"])))
                coverage_weights = np.bincount(
                    indices[masks["visible"]],
                    weights=alpha[masks["visible"]].astype(np.float64) / 255.0,
                    minlength=len(palette),
                )
                rows[key] = {
                    "domain": domain,
                    "dither strength": strength,
                    "source and alpha-composited error": source_error_metrics(rgb, quantized, alpha, masks),
                    "visible occupancy": visible_occupancy,
                    "coverage-weighted palette share": [float(value / max(1e-12, coverage_weights.sum())) for value in coverage_weights],
                    "partial-alpha occupancy": occupancy_metrics(indices, palette, masks["partial alpha"]),
                    "transition metrics": transition_metrics(indices, alpha, masks),
                }
                outputs[key] = output_image
    sheet_path = CAPTURE / f"{spec['label']}-palette-transition-crops.png"
    build_sheet(source, outputs, crop, sheet_path)
    return {
        "asset": spec["label"],
        "provenance": provenance,
        "dimensions": list(source.size),
        "alpha coverage": {
            "alpha zero ratio": float(np.mean(alpha == 0)),
            "partial alpha ratio": float(np.mean((alpha > 0) & (alpha < 255))),
            "visible ratio": float(np.mean(alpha > 0)),
            "alpha edge ratio": float(np.mean(masks["edge"])),
        },
        "transition crop xyxy": list(crop),
        "contact sheet": str(sheet_path.relative_to(ROOT)),
        "conditions": rows,
    }


def main() -> dict[str, object]:
    results = [study_asset(spec) for spec in ASSETS]
    report = {
        "question": "How do fixed-palette distance domain and Bayer strength change visible palette occupancy and neighboring color transitions on natural and pixel-art RGBA assets?",
        "method": "Current retro_lab.nearest_palette and its three existing fixed palettes; sRGB or linear RGB distance; Bayer 4x4 strength 0/0.5/1.0. Alpha is preserved and metrics are evaluated over visible and partial-alpha samples.",
        "metrics": "Palette utilization, entropy-effective colors, dominant share, per-color occupancy, coverage-weighted occupancy, and horizontal/vertical assignment transition rates. These describe patterns and are not quality scores.",
        "source error domain": "The independent lab treats PNG channel values as sRGB code values, so source-error metrics decode both source and palette output with the sRGB transfer curve. Embedded PNG color metadata is recorded but not applied as a color-managed transform.",
        "limits": "Three SHA-verified CC0 assets from three source families: one clover cutout, one silver feather illustration, and one pixel-art hair atlas. One existing VOA Learning English photo/frame is also included for internal exploratory use only; its Wikimedia Commons entry retains a license-review caveat and the derived contact sheet is not for redistribution. The feather PNG has gAMA/chromaticity metadata without ICC, and its sRGB-error model is an explicit suite assumption. The clover embeds an sRGB ICC profile; no color-management transform is applied. No product/runtime/performance validation. Dither transition frequency alone does not establish visible banding or preferred appearance.",
        "assets": results,
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "palette-occupancy-transition-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured {len(report['assets'])} provenance-checked assets across {len(PALETTES)} palettes, {len(DOMAINS)} domains, and {len(STRENGTHS)} dither strengths.")
