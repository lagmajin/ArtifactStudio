"""Offline, scene-linear CPU models for Atmospheric Glow Blur R&D.

This script does not call Artifact product code and does not establish product
parity or perceptual acceptance. PNGs are review-only; NPY files are raw
float32 BGRA values used by the metrics.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
LAB = Path(__file__).resolve().parent
ASSETS = ROOT / "experiments" / "GlowLab" / "assets"
OUT_DEFAULT = LAB / "captures" / "rd1"
SIZES = ((480, 270), (960, 540), (1920, 1080))
BACKGROUNDS = {
    "black": np.array((0.0, 0.0, 0.0), np.float32),
    "gray": np.array((0.18, 0.18, 0.18), np.float32),
    "white": np.array((1.0, 1.0, 1.0), np.float32),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def srgb_to_linear(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4).astype(np.float32)


def linear_to_srgb(value: np.ndarray) -> np.ndarray:
    value = np.maximum(value, 0.0)
    return np.where(value <= 0.0031308, 12.92 * value,
                    1.055 * np.power(value, 1.0 / 2.4) - 0.055)


def preview_bgra(linear_rgba: np.ndarray) -> np.ndarray:
    rgb = linear_to_srgb(linear_rgba[..., :3] / (1.0 + linear_rgba[..., :3]))
    rgba = np.concatenate((rgb, np.clip(linear_rgba[..., 3:4], 0, 1)), axis=2)
    bgra8 = np.rint(np.clip(rgba[..., (2, 1, 0, 3)], 0, 1) * 255).astype(np.uint8)
    return bgra8


def resize_scene_linear(image: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    return cv2.resize(image, (width, height), interpolation=cv2.INTER_AREA).astype(np.float32)


def synthetic_fixture(width: int, height: int, source_shift: int = 0) -> tuple[np.ndarray, dict[str, list[int]]]:
    """Deterministic HDR chart: two colored sources, fine detail and gray ramps."""
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    x = (xx + 0.5) / width
    y = (yy + 0.5) / height
    rgb = np.zeros((height, width, 3), np.float32)
    vignette = np.clip(1.0 - 0.45 * ((x - 0.5) ** 2 + (y - 0.5) ** 2), 0, 1)
    rgb[:] = np.array((0.012, 0.016, 0.028), np.float32) * vignette[..., None]

    # Multiscale sinusoidal panels reveal broad veil and local contrast loss.
    panel = (x > 0.08) & (x < 0.40) & (y > 0.60) & (y < 0.91)
    checker = (((xx.astype(np.int32) // max(1, width // 96)) +
                (yy.astype(np.int32) // max(1, height // 64))) & 1).astype(np.float32)
    rgb[panel] = (0.035 + checker[panel, None] * 0.12)
    ramp = (x > 0.56) & (x < 0.92) & (y > 0.62) & (y < 0.88)
    ramp_value = np.clip((x - 0.56) / 0.36, 0, 1)
    rgb[ramp] = (0.025 + ramp_value[ramp, None] * 0.20)

    # Narrow lines and an angled edge stress detail preservation.
    line_y = int(height * 0.54)
    rgb[max(0, line_y - 1):line_y + 2, int(width * 0.10):int(width * 0.90)] = (0.32, 0.36, 0.42)
    diagonal = np.abs(y - (0.43 + (x - 0.5) * 0.28)) < 0.0015
    rgb[diagonal] = (0.42, 0.40, 0.36)

    source_specs = (
        (int(width * 0.27), int(height * 0.28), max(2.0, width * 0.006),
         np.array((12.0, 5.0, 1.1), np.float32)),
        (int(width * 0.73), int(height * 0.34), max(1.5, width * 0.0035),
         np.array((1.0, 5.5, 14.0), np.float32)),
    )
    masks: dict[str, list[int]] = {}
    for index, (cx, cy, radius, color) in enumerate(source_specs):
        if index == 0:
            cx += int(source_shift)
        d2 = ((xx - cx) ** 2 + (yy - cy) ** 2) / (radius * radius)
        core = d2 <= 1.0
        rgb += color[None, None, :] * np.exp(-0.5 * d2)[..., None] * 0.38
        rgb[core] += color * 0.55
        pad = int(round(radius * 6))
        masks[f"source_{index}"] = [max(0, cx - pad), max(0, cy - pad),
                                     min(width, cx + pad + 1), min(height, cy + pad + 1)]

    alpha = np.ones((height, width, 1), np.float32)
    return np.concatenate((rgb, alpha), axis=2), masks


def alpha_edge_fixture(width: int, height: int) -> tuple[np.ndarray, dict[str, list[int]]]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    rgb = np.zeros((height, width, 3), np.float32)
    alpha = np.zeros((height, width), np.float32)
    x0, x1 = int(width * 0.20), int(width * 0.82)
    y0, y1 = int(height * 0.18), int(height * 0.82)
    ramp = np.clip((xx - x0) / max(1.0, width * 0.04), 0, 1)
    alpha_region = (xx >= x0) & (xx < x1) & (yy >= y0) & (yy < y1)
    alpha[alpha_region] = ramp[alpha_region]
    color = np.array((2.8, 0.42, 0.08), np.float32)
    rgb[alpha_region] = color
    # Opaque thin edge and semitransparent emissive point stress the halo boundary.
    rgb[int(height * 0.50):int(height * 0.505), x0:x1] = (0.04, 0.08, 0.12)
    alpha[int(height * 0.50):int(height * 0.505), x0:x1] = 1.0
    d2 = ((xx - width * 0.70) ** 2 + (yy - height * 0.34) ** 2) / max(1.0, (width * 0.012) ** 2)
    spot = d2 < 1.0
    rgb[spot] = (4.0, 3.2, 1.8)
    alpha[spot] = 0.35
    rgba = np.concatenate((rgb, alpha[..., None]), axis=2)
    rgba[alpha == 0, :3] = 0.0
    boxes = {"emissive_edge": [x0, y0, x1, y1],
             "translucent_source": [int(width * .66), int(height * .30),
                                    int(width * .74), int(height * .38)]}
    return rgba, boxes


def smooth_threshold(luma: np.ndarray, threshold: float, knee: float) -> np.ndarray:
    knee = max(float(knee), 1.0e-6)
    t = np.clip((luma - (threshold - knee)) / (2.0 * knee), 0.0, 1.0)
    return (t * t * (3.0 - 2.0 * t)).astype(np.float32)


def blur(image: np.ndarray, sigma: float) -> np.ndarray:
    if sigma <= 0:
        return image.copy()
    return cv2.GaussianBlur(image, (0, 0), sigmaX=sigma, sigmaY=sigma,
                            borderType=cv2.BORDER_REFLECT_101)


def baseline_glow(image: np.ndarray, *, threshold: float = 0.62,
                  knee: float = 0.10, amount: float = 0.70,
                  radius: float = 5.0) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    rgb = image[..., :3]
    luma = rgb @ np.array((0.2126, 0.7152, 0.0722), np.float32)
    seed = rgb * smooth_threshold(luma, threshold, knee)[..., None]
    halo = (blur(seed, radius) * 0.50 + blur(seed, radius * 1.9) * 0.30 +
            blur(seed, radius * 2.8) * 0.20)
    result = np.concatenate((rgb + halo * amount, image[..., 3:4]), axis=2)
    return result.astype(np.float32), {"seed": seed, "local": halo, "broad": np.zeros_like(halo)}


def atmospheric_glow_blur(image: np.ndarray, *, threshold: float = 0.75,
                          knee: float = 0.15, amount: float = 0.70,
                          local_sigma: float = 5.0, broad_sigma: float = 24.0,
                          broad_fraction: float = 0.35,
                          veil_fraction: float = 0.08,
                          veil_sigma: float | None = None) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    rgb = image[..., :3]
    luma = rgb @ np.array((0.2126, 0.7152, 0.0722), np.float32)
    seed = rgb * smooth_threshold(luma, threshold, knee)[..., None]
    local = blur(seed, local_sigma)
    broad = blur(seed, broad_sigma)
    selected = local * (1.0 - broad_fraction) + broad * broad_fraction
    # A separately observable low-frequency veil term, sourced only by highlights.
    veil_sigma = broad_sigma * 2.5 if veil_sigma is None else veil_sigma
    veil = blur(seed, veil_sigma) * veil_fraction
    contribution = selected * amount + veil
    result = np.concatenate((rgb + contribution, image[..., 3:4]), axis=2)
    return result.astype(np.float32), {"seed": seed, "local": local,
                                      "broad": broad, "veil": veil}


def region_masks(width: int, height: int, source_boxes: dict[str, list[int]]) -> dict[str, np.ndarray]:
    yy, xx = np.mgrid[0:height, 0:width]
    masks: dict[str, np.ndarray] = {"whole": np.ones((height, width), bool)}
    for name, (x0, y0, x1, y1) in source_boxes.items():
        d2 = (xx - (x0 + x1 - 1) / 2.0) ** 2 + (yy - (y0 + y1 - 1) / 2.0) ** 2
        inner = d2 <= ((min(x1 - x0, y1 - y0) / 6.0) ** 2)
        outer = d2 <= ((min(x1 - x0, y1 - y0) / 2.0) ** 2)
        masks[f"{name}_core"] = inner
        masks[f"{name}_ring"] = outer & ~inner
    masks["fine_detail"] = ((xx >= width * 0.08) & (xx < width * 0.42) &
                            (yy >= height * 0.58) & (yy < height * 0.94))
    masks["ramp"] = ((xx >= width * 0.54) & (xx < width * 0.94) &
                     (yy >= height * 0.60) & (yy < height * 0.90))
    masks["edge_band"] = (xx < 4) | (yy < 4) | (xx >= width - 4) | (yy >= height - 4)
    return masks


def summarize(array: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    values = array[mask].reshape(-1, array.shape[-1]).astype(np.float64)
    luma = values[:, :3] @ np.array((0.2126, 0.7152, 0.0722), np.float64)
    return {
        "pixels": int(mask.sum()),
        "channel_min": np.min(values, axis=0).tolist(),
        "channel_max": np.max(values, axis=0).tolist(),
        "channel_mean": np.mean(values, axis=0).tolist(),
        "luma_p50_p95_p99": np.percentile(luma, (50, 95, 99)).tolist(),
        "non_finite_values": int(np.size(values) - np.isfinite(values).sum()),
        "channels_above_1": int(np.count_nonzero(values[:, :3] > 1.0)),
    }


def analyze_case(out_dir: Path, name: str, source: np.ndarray,
                 boxes: dict[str, list[int]], *, source_kind: str,
                 source_hash: str | None, width: int, height: int) -> dict[str, object]:
    input_path = out_dir / f"{name}.input.rgba.npy"
    np.save(input_path, source.astype(np.float32), allow_pickle=False)
    resolution_scale = width / 960.0
    baseline, baseline_components = baseline_glow(source, radius=5.0 * resolution_scale)
    candidate, components = atmospheric_glow_blur(source,
                                                    local_sigma=5.0 * resolution_scale,
                                                    broad_sigma=24.0 * resolution_scale,
                                                    veil_sigma=60.0 * resolution_scale)
    regions = region_masks(width, height, boxes)
    outputs: dict[str, np.ndarray] = {"input": source, "baseline_model": baseline,
                                     "candidate_model": candidate,
                                     "baseline_halo": baseline_components["local"],
                                     "candidate_seed": components["seed"],
                                     "candidate_local": components["local"],
                                     "candidate_broad": components["broad"],
                                     "candidate_veil": components["veil"]}
    for key, image in outputs.items():
        np.save(out_dir / f"{name}.{key}.npy", image.astype(np.float32), allow_pickle=False)
    for key, image in (("input", source), ("baseline_model", baseline),
                       ("candidate_model", candidate)):
        cv2.imwrite(str(out_dir / f"{name}.{key}.preview.png"), preview_bgra(image))
    if width == 960:
        delta_rgb = np.abs(candidate[..., :3] - baseline[..., :3]) * 8.0
        delta_rgba = np.concatenate((delta_rgb, np.ones((height, width, 1), np.float32)), axis=2)
        comparison = cv2.hconcat((preview_bgra(source), preview_bgra(baseline),
                                 preview_bgra(candidate), preview_bgra(delta_rgba)))
        cv2.imwrite(str(out_dir / f"{name}.comparison.review.png"), comparison)

    metric_regions: dict[str, object] = {}
    for region_name, mask in regions.items():
        b = baseline[mask, :3].astype(np.float64)
        c = candidate[mask, :3].astype(np.float64)
        delta = c - b
        metric_regions[region_name] = {
            "input": summarize(source, mask), "baseline_model": summarize(baseline, mask),
            "candidate_model": summarize(candidate, mask),
            "candidate_vs_baseline_model_rgb_mae": float(np.mean(np.abs(delta))),
            "candidate_vs_baseline_model_rgb_rmse": float(np.sqrt(np.mean(delta * delta))),
            "candidate_vs_baseline_model_abs_p95_max": [
                float(np.percentile(np.abs(delta), 95)), float(np.max(np.abs(delta)))],
        }

    alpha_records: dict[str, object] = {}
    for bg_name, bg in BACKGROUNDS.items():
        source_alpha = source[..., 3:4]
        src_comp = source[..., :3] * source_alpha + bg[None, None, :] * (1.0 - source_alpha)
        can_comp = candidate[..., :3] * source_alpha + bg[None, None, :] * (1.0 - source_alpha)
        alpha_records[bg_name] = {
            "composited_rgb_mae_candidate_vs_input": float(np.mean(np.abs(can_comp - src_comp))),
            "source_alpha_changed": bool(np.any(candidate[..., 3] != source[..., 3])),
        }

    seed_energy = float(np.sum(components["seed"] @ np.array((0.2126, 0.7152, 0.0722), np.float32)))
    veil_energy = float(np.sum(components["veil"] @ np.array((0.2126, 0.7152, 0.0722), np.float32)))
    record = {
        "source_kind": source_kind, "source_sha256": source_hash,
        "dimensions_width_height": [width, height],
        "storage": "float32 RGBA; scene-linear for synthetic and sRGB-decoded assumption for CC0 JPEG",
        "alpha_association": "straight; opaque synthetic and source alpha preserved for real plates",
        "source_boxes_xyxy": boxes,
        "regions": metric_regions,
        "alpha_composites": alpha_records,
        "effect_specific": {
            "source_luma_energy": seed_energy,
            "candidate_veil_luma_energy": veil_energy,
            "veil_to_selected_source_energy_ratio": veil_energy / max(seed_energy, 1.0e-12),
            "amount_zero_identity_checked_separately": True,
        },
        "resolution_scale_from_960_width": resolution_scale,
        "baseline_parameters": {"threshold": 0.62, "knee": 0.10, "amount": 0.70,
                                "gaussian_sigmas_native_pixels": [5.0, 9.5, 14.0],
                                "applied_gaussian_sigmas_pixels": [
                                    5.0 * resolution_scale, 9.5 * resolution_scale,
                                    14.0 * resolution_scale],
                                "weights": [0.50, 0.30, 0.20]},
        "candidate_parameters": {"threshold": 0.75, "knee": 0.15, "amount": 0.70,
                                 "local_sigma_at_960": 5.0,
                                 "broad_sigma_at_960": 24.0,
                                 "applied_local_sigma_pixels": 5.0 * resolution_scale,
                                 "applied_broad_sigma_pixels": 24.0 * resolution_scale,
                                 "broad_fraction": 0.35, "veil_sigma_at_960": 60.0,
                                 "applied_veil_sigma_pixels": 60.0 * resolution_scale,
                                 "veil_fraction": 0.08},
        "interpretation_limit": "Independent CPU equations only; not Artifact product output, HDR display pipeline, perceptual acceptance, or runtime performance evidence.",
    }
    return record


def label_tile(image: np.ndarray, label: str) -> np.ndarray:
    tile = image.copy()
    cv2.rectangle(tile, (0, 0), (tile.shape[1], 30), (12, 15, 22, 255), -1)
    cv2.putText(tile, label, (9, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                (240, 244, 250, 255), 1, cv2.LINE_AA)
    return tile


def parameter_sweeps(output: Path) -> dict[str, object]:
    """One-factor exploratory response sweeps; no acceptance gates are applied."""
    width, height = 960, 540
    synthetic, synthetic_boxes = synthetic_fixture(width, height)
    photo_inputs: dict[str, tuple[np.ndarray, dict[str, list[int]], str]] = {
        "synthetic-hdr": (synthetic, synthetic_boxes, "analytic deterministic float32 scene-linear fixture")}
    for key, asset_name, original_size, native_rois in (
        ("cc0-neon", "cc0-neon-streets-1536.jpg", (1536, 1025), {
            "central_intersection": [690, 360, 980, 700], "right_practicals": [1130, 330, 1500, 720],
            "lower_road_detail": [450, 640, 850, 980]}),
        ("cc0-streetlight", "cc0-street-light-night.jpg", (576, 1280), {
            "left_lamp": [80, 390, 205, 525], "right_lamp": [310, 390, 435, 525],
            "sky_field": [120, 120, 450, 360]}),
    ):
        image, _digest, _meta = load_cc0_plate(ASSETS / asset_name, (width, height))
        photo_inputs[key] = (image, scale_boxes(native_rois, original_size, (width, height)),
                             f"CC0 asset {asset_name}; sRGB decode assumption")

    sweep_values: dict[str, list[tuple[str, float]]] = {
        "threshold": [("0p45", .45), ("0p60", .60), ("0p75", .75), ("0p90", .90), ("1p10", 1.10)],
        "broad_fraction": [("0p00", 0.0), ("0p20", .20), ("0p35", .35), ("0p65", .65), ("1p00", 1.0)],
        "broad_sigma": [("12", 12.0), ("24", 24.0), ("48", 48.0), ("72", 72.0)],
        "veil_fraction": [("0p00", 0.0), ("0p02", .02), ("0p08", .08), ("0p20", .20)],
        "amount": [("0p00", 0.0), ("0p35", .35), ("0p70", .70), ("1p40", 1.40)],
    }
    raw: dict[str, object] = {"design": "one factor at a time around rd1 base; no acceptance thresholds",
                              "base": {"threshold": .75, "knee": .15, "amount": .70,
                                       "local_sigma_px": 5.0, "broad_sigma_px": 24.0,
                                       "broad_fraction": .35, "veil_sigma_px": 60.0,
                                       "veil_fraction": .08},
                              "fixed_resolution": [width, height], "sources": {}}
    for source_name, (image, boxes, provenance) in photo_inputs.items():
        baseline, _ = baseline_glow(image)
        masks = region_masks(width, height, boxes)
        src: dict[str, object] = {"provenance": provenance, "source_sha256": (
            sha256(ASSETS / "cc0-neon-streets-1536.jpg") if source_name == "cc0-neon" else
            sha256(ASSETS / "cc0-street-light-night.jpg") if source_name == "cc0-streetlight" else None),
                                  "factors": {}}
        for factor, values in sweep_values.items():
            factor_results = []
            tiles = [label_tile(preview_bgra(image), "INPUT"),
                     label_tile(preview_bgra(baseline), "BASELINE MODEL")]
            for value_name, value in values:
                parameters: dict[str, float] = {}
                parameters[factor] = value
                candidate, components = atmospheric_glow_blur(image, **parameters)
                diff = candidate[..., :3] - baseline[..., :3]
                regions: dict[str, object] = {}
                for region_name, mask in masks.items():
                    region = diff[mask]
                    candidate_rgb = candidate[mask, :3]
                    regions[region_name] = {
                        "candidate_vs_baseline_rgb_mae": float(np.mean(np.abs(region))),
                        "candidate_vs_baseline_rgb_rmse": float(np.sqrt(np.mean(region * region))),
                        "candidate_rgb_luma_p95": float(np.percentile(
                            candidate_rgb @ np.array((.2126, .7152, .0722), np.float32), 95)),
                    }
                factor_results.append({"value_label": value_name, "value": value,
                                       "regions": regions,
                                       "whole_candidate_luma_mean": float(np.mean(
                                           candidate[..., :3] @ np.array((.2126, .7152, .0722), np.float32))),
                                       "whole_candidate_channels_above_1": int(np.count_nonzero(candidate[..., :3] > 1)),
                                       "whole_candidate_non_finite": int(candidate.size - np.isfinite(candidate).sum()),
                                       "veil_luma_energy": float(np.sum(components["veil"] @
                                           np.array((.2126, .7152, .0722), np.float32)))})
                tiles.append(label_tile(preview_bgra(candidate), f"{factor}={value_name}"))
            src["factors"][factor] = factor_results
            cv2.imwrite(str(output / f"sweep-{source_name}-{factor}.review.png"), cv2.hconcat(tiles))
        raw["sources"][source_name] = src
    path = output / "rd1_parameter_sweeps.json"
    path.write_text(json.dumps(raw, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return {"path": path.name, "sha256": sha256(path), "factor_count": len(sweep_values),
            "source_count": len(photo_inputs),
            "measurements": sum(len(v) for source in raw["sources"].values()
                                for v in source["factors"].values())}


def load_cc0_plate(path: Path, size: tuple[int, int]) -> tuple[np.ndarray, str, dict[str, object]]:
    with Image.open(path) as image:
        original_size = image.size
        icc_profile = image.info.get("icc_profile")
        rgb_srgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    target_w, target_h = size
    scale = min(target_w / original_size[0], target_h / original_size[1])
    content_w = max(1, int(round(original_size[0] * scale)))
    content_h = max(1, int(round(original_size[1] * scale)))
    offset_x, offset_y = (target_w - content_w) // 2, (target_h - content_h) // 2
    content = cv2.resize(rgb_srgb, (content_w, content_h), interpolation=cv2.INTER_AREA)
    canvas = np.zeros((target_h, target_w, 3), np.float32)
    canvas[offset_y:offset_y + content_h, offset_x:offset_x + content_w] = content
    rgb_linear = srgb_to_linear(canvas)
    alpha = np.ones((*rgb_linear.shape[:2], 1), np.float32)
    metadata = {"original_dimensions_width_height": list(original_size),
                "target_dimensions_width_height": [target_w, target_h],
                "resize_policy": "aspect-preserving contain; centered black letterbox",
                "content_rect_xyxy": [offset_x, offset_y, offset_x + content_w, offset_y + content_h],
                "embedded_icc_profile_present": bool(icc_profile),
                "transfer_assumption": "sRGB OETF; source profile absent or not applied by this study"}
    return np.concatenate((rgb_linear, alpha), axis=2), sha256(path), metadata


def scale_boxes(boxes: dict[str, list[int]], original_size: tuple[int, int],
                size: tuple[int, int]) -> dict[str, list[int]]:
    target_w, target_h = size
    scale = min(target_w / original_size[0], target_h / original_size[1])
    content_w = max(1, int(round(original_size[0] * scale)))
    content_h = max(1, int(round(original_size[1] * scale)))
    offset_x, offset_y = (target_w - content_w) // 2, (target_h - content_h) // 2
    result = {}
    for name, (x0, y0, x1, y1) in boxes.items():
        result[name] = [offset_x + int(round(x0 * content_w / original_size[0])),
                        offset_y + int(round(y0 * content_h / original_size[1])),
                        offset_x + int(round(x1 * content_w / original_size[0])),
                        offset_y + int(round(y1 * content_h / original_size[1]))]
    return result


def run(output: Path, only_960: bool) -> None:
    output.mkdir(parents=True, exist_ok=True)
    sizes = (SIZES[1],) if only_960 else SIZES
    manifest: dict[str, object] = {
        "title": "Atmospheric Glow Blur R&D rd1 offline model study",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source_revision": subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                          capture_output=True, text=True, check=True).stdout.strip(),
        "runtime": {"python": platform.python_version(), "numpy": np.__version__,
                    "opencv": cv2.__version__, "pillow": Image.__version__},
        "source_provenance": {
            "synthetic_chart": {"generator": str(Path(__file__).name), "seed": "none; deterministic analytic fixture"},
            "cc0_neon": {"path": "experiments/GlowLab/assets/cc0-neon-streets-1536.jpg",
                         "license": "CC0 1.0 per experiments/GlowLab/assets/SOURCES.md",
                         "record": "Wikimedia Commons: Neon streets (Unsplash).jpg"},
            "cc0_streetlight": {"path": "experiments/GlowLab/assets/cc0-street-light-night.jpg",
                                "license": "CC0 1.0 per experiments/GlowLab/assets/SOURCES.md",
                                "record": "Wikimedia Commons: Street light night.jpg"},
            "portrait_plate": "Excluded: no per-asset license record was located.",
        },
        "display_transform": "review PNG only: Reinhard x/(1+x), then sRGB OETF; metric arrays remain float32 linear.",
        "cases": {},
        "execution_class": "offline CPU equation study; no product code/build/app execution",
    }
    cases: dict[str, object] = {}
    for width, height in sizes:
        synthetic, source_boxes = synthetic_fixture(width, height)
        name = f"synthetic-hdr-{width}x{height}"
        cases[name] = analyze_case(output, name, synthetic, source_boxes,
                                  source_kind="deterministic synthetic HDR chart; linear float32",
                                  source_hash=None, width=width, height=height)
        photographic_rois = {
            "cc0-neon": ((1536, 1025), {
                "central_intersection": [690, 360, 980, 700],
                "right_practicals": [1130, 330, 1500, 720],
                "lower_road_detail": [450, 640, 850, 980]}),
            "cc0-streetlight": ((576, 1280), {
                "left_lamp": [80, 390, 205, 525],
                "right_lamp": [310, 390, 435, 525],
                "sky_field": [120, 120, 450, 360]}),
        }
        for key, asset_name in (("cc0-neon", "cc0-neon-streets-1536.jpg"),
                                ("cc0-streetlight", "cc0-street-light-night.jpg")):
            path = ASSETS / asset_name
            image, digest, image_meta = load_cc0_plate(path, (width, height))
            native_size, native_rois = photographic_rois[key]
            scaled_rois = scale_boxes(native_rois, native_size, (width, height))
            record = analyze_case(
                output, f"{key}-{width}x{height}", image, scaled_rois,
                source_kind="CC0 JPEG decoded as sRGB then converted to scene-linear; assumed transfer function",
                source_hash=digest, width=width, height=height)
            record["source_image_metadata"] = image_meta
            record["roi_basis"] = "manually selected normalized practical-light/detail ROIs, scaled from source-pixel coordinates"
            cases[f"{key}-{width}x{height}"] = record

    alpha_image, alpha_boxes = alpha_edge_fixture(960, 540)
    alpha_case = analyze_case(output, "alpha-edge-960x540", alpha_image, alpha_boxes,
                              source_kind="deterministic HDR straight-alpha edge; alpha ramp and hidden-RGB zero",
                              source_hash=None, width=960, height=540)
    premultiplied = alpha_image.copy()
    premultiplied[..., :3] *= premultiplied[..., 3:4]
    recovered = np.zeros_like(premultiplied)
    valid = premultiplied[..., 3] > 1.0e-6
    recovered[..., :3][valid] = premultiplied[..., :3][valid] / premultiplied[..., 3:4][valid]
    recovered[..., 3] = premultiplied[..., 3]
    alpha_candidate, _ = atmospheric_glow_blur(alpha_image)
    recovered_candidate, _ = atmospheric_glow_blur(recovered)
    hidden_stress = alpha_image.copy()
    hidden_x, hidden_y = int(960 * .185), int(540 * .36)
    hidden_stress[hidden_y, hidden_x, :3] = (5.0, 4.0, 2.0)
    hidden_stress[hidden_y, hidden_x, 3] = 0.0
    hidden_candidate, _ = atmospheric_glow_blur(hidden_stress)
    yy_roi, xx_roi = np.ogrid[:540, :960]
    valid_object_roi = ((xx_roi >= int(960 * .20)) & (xx_roi < int(960 * .82)) &
                        (yy_roi >= int(540 * .18)) & (yy_roi < int(540 * .82)))
    hidden_rgb_delta = hidden_candidate[..., :3] - alpha_candidate[..., :3]
    visible_hidden_delta = hidden_rgb_delta * alpha_image[..., 3:4]
    hidden_stress_record = {
        "hidden_source_xy": [hidden_x, hidden_y],
        "hidden_rgb": [5.0, 4.0, 2.0],
        "output_alpha_changed": bool(np.any(hidden_candidate[..., 3] != alpha_candidate[..., 3])),
        "visible_object_roi_pixels": int(valid_object_roi.sum()),
        "visible_object_roi_rgb_rmse_after_alpha_composite": float(
            np.sqrt(np.mean(visible_hidden_delta[valid_object_roi] ** 2))),
        "visible_object_roi_rgb_max_after_alpha_composite": float(
            np.max(np.abs(visible_hidden_delta[valid_object_roi]))),
        "whole_image_hidden_rgb_max_before_composite": float(np.max(np.abs(hidden_rgb_delta))),
        "interpretation": "invalid straight-RGB stress exposes whether hidden RGB can influence visible semi-transparent neighbors; this CPU model is not alpha-aware",
    }
    np.save(output / "alpha-edge-hidden-rgb-stress.input.rgba.npy", hidden_stress, allow_pickle=False)
    np.save(output / "alpha-edge-hidden-rgb-stress.candidate.npy", hidden_candidate, allow_pickle=False)
    alpha_case["alpha_representation_pair"] = {
        "input_visible_rgb_max_abs_straight_vs_recovered_premultiplied": float(
            np.max(np.abs(alpha_image[valid, :3] - recovered[valid, :3]))),
        "candidate_visible_rgb_max_abs_difference": float(
            np.max(np.abs(alpha_candidate[valid, :3] - recovered_candidate[valid, :3]))),
        "fully_transparent_hidden_rgb_fixture": "canonical input has zero hidden RGB; invalid hidden-RGB stress is separately measured, not used as a visual case",
        "invalid_hidden_rgb_stress": hidden_stress_record,
    }
    cases["alpha-edge-960x540"] = alpha_case

    temporal_frames: list[dict[str, object]] = []
    temporal_errors: list[np.ndarray] = []
    for frame in range(7):
        shift = (frame - 3) * 10
        frame_image, frame_boxes = synthetic_fixture(480, 270, source_shift=shift)
        frame_name = f"moving-source-frame-{frame:02d}"
        frame_record = analyze_case(output, frame_name, frame_image, frame_boxes,
                                    source_kind="deterministic 7-frame moving HDR source sequence",
                                    source_hash=None, width=480, height=270)
        candidate_image = np.load(output / f"{frame_name}.candidate_model.npy")
        temporal_errors.append(candidate_image[..., :3] - frame_image[..., :3])
        temporal_frames.append({"frame": frame, "source_center_x_pixels": int(480 * .27 + shift),
                                "candidate_veil_energy": frame_record["effect_specific"]["candidate_veil_luma_energy"],
                                "source_luma_energy": frame_record["effect_specific"]["source_luma_energy"]})
    residual_frame_delta = [float(np.sqrt(np.mean((b - a) ** 2)))
                            for a, b in zip(temporal_errors, temporal_errors[1:])]
    temporal = {"sequence": temporal_frames,
                "adjacent_screen_space_candidate_residual_rgb_rmse": residual_frame_delta,
                "interpretation": "screen-space diagnostic without motion compensation; not a flicker acceptance test"}
    cases["moving-source-7-frame-480x270"] = temporal
    manifest["parameter_sweep"] = parameter_sweeps(output)

    # Contract diagnostics: amount zero is exact identity; repeated static frame is deterministic.
    probe, _ = synthetic_fixture(128, 72)
    zero, _ = atmospheric_glow_blur(probe, amount=0.0, veil_fraction=0.0)
    repeated_a, _ = atmospheric_glow_blur(probe)
    repeated_b, _ = atmospheric_glow_blur(probe.copy())
    contract = {
        "amount_zero_max_abs_rgb_error": float(np.max(np.abs(zero[..., :3] - probe[..., :3]))),
        "amount_zero_alpha_changed": bool(np.any(zero[..., 3] != probe[..., 3])),
        "static_repeat_max_abs_error": float(np.max(np.abs(repeated_a - repeated_b))),
        "all_candidate_finite": all(bool(np.isfinite(np.load(output / f"{name}.candidate_model.npy")).all())
                                     for name in cases if name.startswith(("synthetic-", "cc0-", "alpha-edge-"))),
        "temporal_candidates_finite": all(bool(np.isfinite(np.load(output / f"moving-source-frame-{frame:02d}.candidate_model.npy")).all())
                                           for frame in range(7)),
        "alpha_edge_candidate_finite": bool(np.isfinite(alpha_candidate).all()),
    }
    manifest["cases"] = cases
    manifest["contract_diagnostics"] = contract
    manifest["artifact_sha256"] = {
        "generator": sha256(Path(__file__).resolve()),
        "cc0_neon_asset": sha256(ASSETS / "cc0-neon-streets-1536.jpg"),
        "cc0_streetlight_asset": sha256(ASSETS / "cc0-street-light-night.jpg"),
        "sources_record": sha256(ASSETS / "SOURCES.md"),
    }
    output_files = sorted(p for p in output.iterdir()
                          if p.is_file() and p.name not in ("rd1_manifest.json", "rd1_manifest.sha256"))
    manifest["outputs"] = {p.name: {"sha256": sha256(p), "bytes": p.stat().st_size}
                            for p in output_files}
    manifest_path = output / "rd1_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest_hash = sha256(manifest_path)
    (output / "rd1_manifest.sha256").write_text(f"{manifest_hash}  rd1_manifest.json\n", encoding="ascii")
    print(json.dumps({"output": str(output.resolve()), "case_count": len(cases),
                      "contract_diagnostics": contract,
                      "manifest_sha256": manifest_hash,
                      "output_count": len(output_files)}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUT_DEFAULT)
    parser.add_argument("--only-960", action="store_true", help="capture only the primary 960x540 resolution")
    args = parser.parse_args()
    run(args.output, args.only_960)
