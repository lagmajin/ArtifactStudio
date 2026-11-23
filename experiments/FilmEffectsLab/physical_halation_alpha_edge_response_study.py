#!/usr/bin/env python3
"""Model Physical Halation diffusion and alpha-edge spill offline."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/physical-halation-alpha-edge-response-r1.json"
HALATION_SOURCE = ROOT / "ArtifactCore/include/ImageProcessing/Halation.ixx"
EFFECT_SOURCE = ROOT / "Artifact/src/Effects/Glow/PhysicalHalationEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/physical-halation-alpha-edge-response-r1"
SIZES = ((256, 128), (512, 256))
POSITIONS = {"interior": .82, "alpha_edge": .89}
INTENSITIES = (.5, 1.5)
PARAMS = {"threshold": .8, "spread": 15.0, "redDiffusion": 2.0, "softness": .5}
BACKGROUNDS = {"black": np.zeros(3, np.float32),
               "gray18": np.full(3, .18, np.float32),
               "white": np.ones(3, np.float32)}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture(width: int, height: int, stripe_x: float) -> tuple[np.ndarray, dict[str, np.ndarray], dict]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / max(1, width - 1), yy / max(1, height - 1)
    radial = np.sqrt(((nx - .5) / .39) ** 2 + ((ny - .5) / .43) ** 2)
    alpha = np.clip((1.04 - radial) * 18.0, 0, 1).astype(np.float32)
    rgb = np.empty((height, width, 3), np.float32)
    rgb[:] = (.10, .09, .08)
    source_stripe = (np.abs(nx - stripe_x) <= 1.5 / max(1, width - 1)) & (ny >= .34) & (ny <= .66)
    rgb[source_stripe & (ny < .44)] = (1.0, 1.0, 1.0)
    rgb[source_stripe & (ny >= .44) & (ny < .55)] = (1.0, .80, .60)
    rgb[source_stripe & (ny >= .55)] = (.90, .86, .78)
    rgb[alpha == 0] = 0
    straight = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)  # BGRA, as the Halation buffer contract states.
    logical_luma = .2126 * rgb[..., 0] + .7152 * rgb[..., 1] + .0722 * rgb[..., 2]
    highlight_roi = source_stripe & (logical_luma > PARAMS["threshold"])
    regions = {
        "all": np.ones(alpha.shape, bool),
        "transparent": alpha == 0,
        "low_alpha": (alpha > 0) & (alpha <= .1),
        "edge": (alpha > .1) & (alpha < .9),
        "near_opaque": (alpha >= .9) & (alpha < 1),
        "opaque": alpha == 1,
        "source_highlight": highlight_roi,
    }
    stripe_pixels = np.argwhere(source_stripe)
    center_y = int(round(.5 * (height - 1)))
    center_x = int(round(stripe_x * (width - 1)))
    roi_size = 64
    x0, y0 = max(0, center_x - roi_size // 2), max(0, center_y - roi_size // 2)
    x1, y1 = min(width, x0 + roi_size), min(height, y0 + roi_size)
    x0, y0 = max(0, x1 - roi_size), max(0, y1 - roi_size)
    roi = np.zeros(alpha.shape, bool)
    roi[y0:y1, x0:x1] = True
    return straight, regions, {"stripe_pixels": int(stripe_pixels.shape[0]),
                               "source_highlight_pixels": int(highlight_roi.sum()),
                               "roi_bounds_xyxy": [x0, y0, x1, y1], "roi_pixels": int(roi.sum()),
                               "roi_mask": roi, "center_xy": [center_x, center_y]}


def diffuse_box(field: np.ndarray, radius: int) -> np.ndarray:
    height, width, channels = field.shape
    if radius < 0:
        return np.zeros_like(field)
    count = np.float32(2 * radius + 1)
    horizontal = np.zeros_like(field)
    x = np.arange(width)
    for dx in range(-radius, radius + 1):
        horizontal += field[:, np.clip(x + dx, 0, width - 1), :]
    horizontal /= count
    output = np.zeros_like(field)
    y = np.arange(height)
    for dy in range(-radius, radius + 1):
        output += horizontal[np.clip(y + dy, 0, height - 1), :, :]
    output /= count
    return output


def halation_model(source: np.ndarray, intensity: float) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    blue, green, red, alpha = (source[..., i] for i in range(4))
    lum = np.float32(blue * np.float32(.114) + green * np.float32(.587) + red * np.float32(.299))
    knee = np.float32(np.clip(PARAMS["softness"], 0, 1))
    soft = np.float32(np.clip(lum - np.float32(PARAMS["threshold"]) + knee, 0, 2 * knee))
    soft_contribution = np.float32((soft * soft) / np.float32(4 * knee + .000001))
    contribution = np.maximum(np.maximum(lum - np.float32(PARAMS["threshold"]), 0), soft_contribution)
    weight = np.clip(contribution / np.float32(1 - PARAMS["threshold"] + .001), 0, 1).astype(np.float32)
    active = contribution > 0
    highlights = np.zeros_like(source)
    highlights[..., 0] = np.where(active, blue * weight * np.float32(.1), 0)
    highlights[..., 1] = np.where(active, green * weight * np.float32(.2), 0)
    highlights[..., 2] = np.where(active, red * weight, 0)
    radius = int(PARAMS["spread"])
    red_radius = int(PARAMS["spread"] * PARAMS["redDiffusion"])
    red_layer = diffuse_box(highlights[..., :3], red_radius)
    gb_layer = diffuse_box(highlights[..., :3], radius)
    output = source.copy()
    output[..., 0] += gb_layer[..., 0] * np.float32(.05 * intensity)
    output[..., 1] += gb_layer[..., 1] * np.float32(.10 * intensity)
    output[..., 2] += red_layer[..., 2] * np.float32(intensity)
    output[..., 3] = alpha
    layers = {"highlight_input": highlights[..., :3],
              "red_diffusion": red_layer[..., 2],
              "green_diffusion": gb_layer[..., 1] * np.float32(.10),
              "blue_diffusion": gb_layer[..., 0] * np.float32(.05),
              "source_contribution": contribution}
    return output, layers


def composite(image: np.ndarray, association: str, bg: np.ndarray) -> np.ndarray:
    rgb = image[..., :3][..., ::-1]
    alpha = image[..., 3:4]
    if association == "straight":
        return rgb * alpha + bg * (1 - alpha)
    return rgb + bg * (1 - alpha)


def stats(values: np.ndarray) -> dict:
    x = np.asarray(values, np.float64).reshape(-1)
    if x.size == 0:
        return {"count": 0, "mae": 0.0, "rmse": 0.0, "p95_abs": 0.0, "max_abs": 0.0}
    absolute = np.abs(x)
    return {"count": int(x.size), "mae": float(absolute.mean()),
            "rmse": float(np.sqrt(np.mean(x * x))),
            "p95_abs": float(np.quantile(absolute, .95)), "max_abs": float(absolute.max())}


def range_stats(image: np.ndarray, alpha_reference: np.ndarray, association: str) -> dict:
    rgb = image[..., :3].astype(np.float64)
    return {"min_bgr": [float(rgb[..., i].min()) for i in range(3)],
            "max_bgr": [float(rgb[..., i].max()) for i in range(3)],
            "channels_lt_0_fraction": float(np.mean(rgb < 0)),
            "channels_gt_1_fraction": float(np.mean(rgb > 1)),
            "premultiplied_bgr_gt_alpha_channel_fraction": (float(np.mean(rgb > image[..., 3:4]))
                                                            if association == "premultiplied" else None),
            "nonfinite_rgb_channels": int(rgb.size - np.isfinite(rgb).sum()),
            "alpha_exact": bool(np.array_equal(image[..., 3], alpha_reference))}


def preview(image: np.ndarray, association: str, bg: np.ndarray) -> Image.Image:
    rgb = composite(image, association, bg)
    return Image.fromarray(np.rint(np.clip(rgb, 0, 1) * 255).astype(np.uint8), "RGB")


def contact_sheet(items: list[tuple[str, Image.Image]], path: Path) -> None:
    tile_w, tile_h, label_h = 256, 128, 25
    columns = 3
    rows = (len(items) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (25, 27, 30))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(items):
        image.thumbnail((tile_w, tile_h), Image.Resampling.NEAREST)
        x, y = (index % columns) * tile_w, (index // columns) * (tile_h + label_h)
        sheet.paste(image, (x, y))
        draw.text((x + 4, y + tile_h + 4), label, fill=(235, 235, 235))
    sheet.save(path)


def detail_sheet(items: list[tuple[str, Image.Image]], path: Path) -> None:
    tile_w = tile_h = 256
    label_h = 24
    columns = len(items)
    sheet = Image.new("RGB", (columns * tile_w, tile_h + label_h), (25, 27, 30))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(items):
        sheet.paste(image, (index * tile_w, 0))
        draw.text((index * tile_w + 4, tile_h + 4), label, fill=(235, 235, 235))
    sheet.save(path)


def crop_preview(image: np.ndarray, association: str, bg: np.ndarray,
                 bounds: list[int]) -> Image.Image:
    x0, y0, x1, y1 = bounds
    return preview(image, association, bg).crop((x0, y0, x1, y1)).resize((256, 256), Image.Resampling.NEAREST)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    halation_text = HALATION_SOURCE.read_text(encoding="utf-8")
    effect_text = EFFECT_SOURCE.read_text(encoding="utf-8")
    markers = {
        "source_is_bgra": "Process a BGRA image buffer (float32x4)" in halation_text,
        "rec601_bgr_luma": "source[i].x * 0.114f + source[i].y * 0.587f + source[i].z * 0.299f" in halation_text,
        "soft_knee_contribution": "soft * soft / (4.0f * knee + 0.000001f)" in halation_text,
        "red_diffusion_wide_layer": "settings.spread * settings.redDiffusion, 1.0f, 0.0f, 0.0f" in halation_text,
        "green_blue_diffusion_narrow_layer": "settings.spread, 0.0f, 0.1f, 0.05f" in halation_text,
        "additive_rgb_composite": "source[i].z +\n                              (highlights[i].z + buffer[i].z) * settings.intensity" in halation_text,
        "alpha_restored_from_source": "buffer[i].w = source[i].w" in halation_text,
        "wrapper_copies_source": "dst = src;" in effect_text,
    }
    all_cases, contact_groups, detail_groups = [], {}, {}
    for width, height in SIZES:
        for association in ("straight", "premultiplied"):
            for position_name, stripe_x in POSITIONS.items():
                straight, base_regions, info = fixture(width, height, stripe_x)
                source = straight.copy()
                if association == "premultiplied":
                    source[..., :3] *= source[..., 3:4]
                alpha = source[..., 3]
                regions = dict(base_regions)
                for intensity in INTENSITIES:
                    output, layers = halation_model(source, intensity)
                    delta_rgb = output[..., :3] - source[..., :3]
                    leak_mask = (alpha == 0) & np.any(np.abs(delta_rgb) > 1e-10, axis=2)
                    regions["diffusion_spill"] = leak_mask
                    off = source
                    row = {
                        "width": width, "height": height, "association": association,
                        "stripe_position": position_name, "stripe_x_normalized": stripe_x,
                        "intensity": intensity, "parameters": {**PARAMS, "intensity": intensity},
                        "source_sha256": sha(source.tobytes()),
                        "fixture_alpha_sha256": sha(alpha.tobytes()),
                        "rois": {"stripe_pixels": info["stripe_pixels"],
                                 "source_highlight_pixels": info["source_highlight_pixels"],
                                 "bounds_xyxy": info["roi_bounds_xyxy"], "pixels": info["roi_pixels"]},
                        "output": range_stats(output, alpha, association),
                        "diffusion": {}, "regions": {},
                    }
                    for channel_name, field in layers.items():
                        support = (np.any(np.abs(field) > 1e-10, axis=2)
                                   if field.ndim == 3 else np.abs(field) > 1e-10)
                        if support.any():
                            ys, xs = np.nonzero(support)
                            bbox = [int(xs.min()), int(ys.min()), int(xs.max() + 1), int(ys.max() + 1)]
                        else:
                            bbox = None
                        row["diffusion"][channel_name] = {
                            "stats": stats(field), "support_pixels": int(support.sum()),
                            "support_fraction": float(support.mean()), "bbox_xyxy": bbox,
                            "alpha_zero_support_pixels": int(np.sum(support & (alpha == 0))),
                        }
                    for region_name, mask in regions.items():
                        row["regions"][region_name] = {"pixels": int(mask.sum()),
                            "stored_rgb_delta": stats(delta_rgb[mask]),
                            "composite_delta": {bg_name: stats((composite(output, association, bg) -
                                                               composite(off, association, bg))[mask])
                                                for bg_name, bg in BACKGROUNDS.items()}}
                    row["roi_composite_delta"] = {
                        bg_name: stats((composite(output, association, bg) -
                                        composite(off, association, bg))[info["roi_mask"]])
                        for bg_name, bg in BACKGROUNDS.items()}
                    all_cases.append(row)
                    key = f"{association}_{width}x{height}"
                    contact_groups.setdefault(key, [("disabled", preview(off, association, BACKGROUNDS["gray18"]))])
                    contact_groups[key].append((f"{position_name} I={intensity:g}",
                                                 preview(output, association, BACKGROUNDS["gray18"])))
                    detail_key = f"{association}_{width}x{height}_{position_name}"
                    detail_groups.setdefault(detail_key, [("disabled", crop_preview(off, association, BACKGROUNDS["gray18"], info["roi_bounds_xyxy"]))])
                    detail_groups[detail_key].append((f"I={intensity:g}",
                                                      crop_preview(output, association, BACKGROUNDS["gray18"], info["roi_bounds_xyxy"])))

    contact_paths, detail_paths = [], []
    for key, items in contact_groups.items():
        path = OUT / f"contact_{key}.png"
        contact_sheet(items, path)
        contact_paths.append(path)
    for key, items in detail_groups.items():
        path = OUT / f"detail_{key}.png"
        detail_sheet(items, path)
        detail_paths.append(path)
    metrics = {
        "study": plan["plan_id"],
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__},
        "input_sha256": {"plan": sha(PLAN.read_bytes()), "halation_source": sha(HALATION_SOURCE.read_bytes()),
                         "effect_wrapper": sha(EFFECT_SOURCE.read_bytes()),
                         "runner": sha(Path(__file__).read_bytes())},
        "source_markers": markers,
        "conditions": {"sizes_wh": SIZES, "associations": ["straight", "premultiplied"],
                       "positions": POSITIONS, "intensities": INTENSITIES,
                       "parameters": PARAMS, "backgrounds": list(BACKGROUNDS)},
        "contact_sheets": [str(p.relative_to(ROOT)).replace("\\", "/") for p in contact_paths],
        "detail_sheets": [str(p.relative_to(ROOT)).replace("\\", "/") for p in detail_paths],
        "cases": all_cases,
        "limitations": plan["scope"]["excluded"],
    }
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {str(p.relative_to(ROOT)).replace("\\", "/"): sha(p.read_bytes())
                 for p in sorted(OUT.rglob("*")) if p.is_file() and p.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(all_cases), "source_markers": markers,
                      "transparent_spill_max": max(c["regions"]["diffusion_spill"]["pixels"] for c in all_cases),
                      "output": str(OUT)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
