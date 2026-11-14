"""Offline detail-sharpening R&D lab (rd1).

Deterministic, scene-linear CPU models. This script does not call Artifact
binaries and does not establish product parity, visual acceptance, or runtime
performance. Review PNGs use a Reinhard+sRGB preview and are not measurement
data; measurements use the raw float32 arrays stored in .npz files.

Evaluation plan: EVALUATION_PLAN.md (pre-registered before this run).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


LAB = Path(__file__).resolve().parent
OUT_DEFAULT = LAB / "captures" / "rd1"
SEED = 20261114
LUMA = np.array((0.2126, 0.7152, 0.0722), np.float32)
BACKGROUNDS = {"black": 0.0, "gray": 0.18, "white": 1.0}


# --------------------------------------------------------------------------
# basic helpers
# --------------------------------------------------------------------------
def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def linear_to_srgb(value: np.ndarray) -> np.ndarray:
    value = np.maximum(value, 0.0)
    return np.where(value <= 0.0031308, 12.92 * value,
                    1.055 * np.power(value, 1.0 / 2.4) - 0.055)


def preview_rgb(rgba: np.ndarray, background: float | None = None) -> np.ndarray:
    rgb = rgba[..., :3].astype(np.float32)
    alpha = np.clip(rgba[..., 3:4], 0.0, 1.0)
    if background is not None:
        rgb = rgb * alpha + background * (1.0 - alpha)
    tone = rgb / (1.0 + np.maximum(rgb, 0.0))
    encoded = np.clip(linear_to_srgb(tone), 0.0, 1.0)
    return np.rint(encoded * 255.0).astype(np.uint8)


def luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * LUMA[0] + rgb[..., 1] * LUMA[1] + rgb[..., 2] * LUMA[2]


def gaussian(image: np.ndarray, sigma: float) -> np.ndarray:
    return cv2.GaussianBlur(image, (0, 0), sigmaX=float(sigma), sigmaY=float(sigma),
                            borderType=cv2.BORDER_REFLECT)


def distance_to_rect(xx: np.ndarray, yy: np.ndarray, x0: float, y0: float,
                     x1: float, y1: float) -> np.ndarray:
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    hx, hy = (x1 - x0) * 0.5, (y1 - y0) * 0.5
    dx = np.abs(xx - cx) - hx
    dy = np.abs(yy - cy) - hy
    outside = np.sqrt(np.maximum(dx, 0.0) ** 2 + np.maximum(dy, 0.0) ** 2)
    inside = np.minimum(np.maximum(dx, dy), 0.0)
    return outside + inside


def smoothstep(edge0: np.ndarray | float, edge1: float, value: np.ndarray) -> np.ndarray:
    t = np.clip((value - edge0) / max(float(edge1) - float(edge0), 1e-8), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# --------------------------------------------------------------------------
# fixtures
# --------------------------------------------------------------------------
def chart_rois(width: int, height: int) -> dict[str, list[int]]:
    def box(x0: float, y0: float, x1: float, y1: float) -> list[int]:
        return [int(round(x0 * width)), int(round(y0 * height)),
                int(round(x1 * width)), int(round(y1 * height))]

    return {
        "step_edge": box(0.06, 0.04, 0.26, 0.30),
        "fine_lines": box(0.30, 0.04, 0.46, 0.30),
        "color_edge": box(0.50, 0.04, 0.68, 0.30),
        "text_strokes": box(0.72, 0.04, 0.94, 0.30),
        "noise_low": box(0.06, 0.36, 0.24, 0.52),
        "noise_high": box(0.06, 0.58, 0.24, 0.74),
        "ramp": box(0.30, 0.36, 0.52, 0.74),
        "highlight_step": box(0.58, 0.36, 0.92, 0.52),
        "flat_gray": box(0.58, 0.58, 0.92, 0.74),
    }


def make_chart(width: int, height: int, rng: np.random.Generator) -> tuple[np.ndarray, dict]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    px, py = xx + 0.5, yy + 0.5
    nx, ny = px / width, py / height
    rgb = np.full((height, width, 3), 0.02, np.float32)

    # Hard vertical step: plateau 0.03 -> 0.22.
    step = (nx >= 0.06) & (nx < 0.26) & (ny >= 0.04) & (ny < 0.30)
    rgb[step] = np.where((nx[step] >= 0.16)[:, None], 0.22, 0.03)

    # 1 px fine lines, low contrast.
    lines = (nx >= 0.30) & (nx < 0.46) & (ny >= 0.04) & (ny < 0.30)
    rgb[lines] = 0.10
    line_mask = lines & ((xx.astype(np.int32) % 4) == 0)
    rgb[line_mask] = 0.145

    # Colored step edge: warm -> cool.
    color_region = (nx >= 0.50) & (nx < 0.68) & (ny >= 0.04) & (ny < 0.30)
    warm = nx < 0.59
    rgb[color_region] = np.where(warm[color_region][:, None],
                                 np.array((0.30, 0.08, 0.03), np.float32),
                                 np.array((0.03, 0.10, 0.30), np.float32))

    # Text-like bars: 5 rows, 2 px bars with 3 px gaps.
    text_region = (nx >= 0.72) & (nx < 0.94) & (ny >= 0.04) & (ny < 0.30)
    rgb[text_region] = 0.06
    row = (yy.astype(np.int32) // 6) % 6
    bar = (xx.astype(np.int32) % 5) < 2
    rgb[text_region & (row < 3) & bar] = 0.35

    # Noise patches (deterministic seed).
    low = (nx >= 0.06) & (nx < 0.24) & (ny >= 0.36) & (ny < 0.52)
    high = (nx >= 0.06) & (nx < 0.24) & (ny >= 0.58) & (ny < 0.74)
    rgb[low] = 0.10 + rng.normal(0.0, 0.004, (int(low.sum()), 3)).astype(np.float32)
    rgb[high] = 0.10 + rng.normal(0.0, 0.020, (int(high.sum()), 3)).astype(np.float32)

    # Linear ramp 0.0 -> 0.90.
    ramp = (nx >= 0.30) & (nx < 0.52) & (ny >= 0.36) & (ny < 0.74)
    ramp_value = np.clip((nx - 0.30) / 0.22, 0.0, 1.0) * 0.90
    rgb[ramp] = ramp_value[ramp][:, None]

    # Near-clipping highlight step: 0.60 -> 0.985.
    highlight = (nx >= 0.58) & (nx < 0.92) & (ny >= 0.36) & (ny < 0.52)
    rgb[highlight] = np.where((nx[highlight] >= 0.68)[:, None], 0.985, 0.60)

    # Flat mid gray.
    flat = (nx >= 0.58) & (nx < 0.92) & (ny >= 0.58) & (ny < 0.74)
    rgb[flat] = 0.18

    alpha = np.ones((height, width, 1), np.float32)
    rgba = np.concatenate((np.maximum(rgb, 0.0), alpha), axis=2).astype(np.float32)
    meta = {"edge_x_px": round(0.16 * width, 3), "color_edge_x_px": round(0.59 * width, 3),
            "highlight_edge_x_px": round(0.68 * width, 3), "rois": chart_rois(width, height)}
    return rgba, meta


def make_alpha_edge(width: int = 960, height: int = 540) -> tuple[np.ndarray, dict]:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    px, py = xx + 0.5, yy + 0.5
    rgb = np.zeros((height, width, 3), np.float32)
    alpha = np.zeros((height, width), np.float32)

    rect = distance_to_rect(px, py, 0.12 * width, 0.16 * height, 0.62 * width, 0.84 * height)
    rect_alpha = 1.0 - smoothstep(-1.5, 1.5, rect)
    rgb[:] = np.array((0.85, 0.22, 0.04), np.float32)
    alpha = np.maximum(alpha, rect_alpha)

    d = np.sqrt((px - 0.76 * width) ** 2 + (py - 0.42 * height) ** 2)
    spot = 1.0 - smoothstep((0.10 * width) - 1.5, (0.10 * width) + 1.5, d)
    spot_alpha = spot * 0.40
    rgb = np.where((spot_alpha > alpha)[..., None], np.array((0.05, 0.35, 0.80), np.float32), rgb)
    alpha = np.maximum(alpha, spot_alpha)

    alpha = np.clip(alpha, 0.0, 1.0).astype(np.float32)
    rgb[alpha <= 0.0] = 0.0
    rgba = np.concatenate((rgb, alpha[..., None]), axis=2)
    boxes = {"opaque_edge": [int(0.12 * width) - 8, int(0.16 * height) - 8,
                             int(0.40 * width), int(0.84 * height) + 8],
             "translucent_edge": [int(0.66 * width), int(0.30 * height),
                                  int(0.86 * width), int(0.54 * height)]}
    return rgba, boxes


def make_grating_sequence(width: int = 480, height: int = 270, frames: int = 8):
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    outputs = []
    for index in range(frames):
        phase = (xx + 0.5 - 0.6 * index) / 8.0
        value = 0.10 + 0.020 * np.sin(2.0 * np.pi * phase)
        rgb = np.repeat(value[..., None], 3, axis=2)
        alpha = np.ones((height, width, 1), np.float32)
        outputs.append(np.concatenate((rgb, alpha), axis=2).astype(np.float32))
    roi = [int(0.20 * width), int(0.20 * height), int(0.80 * width), int(0.80 * height)]
    return outputs, roi


# --------------------------------------------------------------------------
# models
# --------------------------------------------------------------------------
def _luma_detail(rgb: np.ndarray, sigma: float, amount: float,
                 threshold: float, knee: float) -> np.ndarray:
    detail = luma(rgb) - gaussian(luma(rgb), sigma)
    if threshold > 0.0:
        magnitude = np.abs(detail)
        if knee > 0.0:
            gain = smoothstep(threshold, threshold + knee, magnitude)
        else:
            gain = (magnitude > threshold).astype(np.float32)
        detail = detail * gain
    return detail * amount


def _local_bounds(rgb: np.ndarray, radius: int):
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    low = cv2.erode(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    return low, high


def product_usm_mirror(rgba: np.ndarray, p: dict) -> np.ndarray:
    rgb = rgba[..., :3]
    blurred = gaussian(rgb, p["sigma"])
    diff = (rgb - blurred) * p["amount"]
    result = rgb + diff
    if p["threshold"] > 0.0:
        result = np.where(np.abs(diff) > p["threshold"], result, rgb)
    result = np.maximum(result, 0.0)
    return np.concatenate((result.astype(np.float32), rgba[..., 3:4]), axis=2)


def luma_usm_soft(rgba: np.ndarray, p: dict) -> np.ndarray:
    rgb = rgba[..., :3]
    detail = _luma_detail(rgb, p["sigma"], p["amount"], p["threshold"], p["knee"])
    result = np.maximum(rgb + detail[..., None], 0.0)
    return np.concatenate((result.astype(np.float32), rgba[..., 3:4]), axis=2)


def halo_limited(rgba: np.ndarray, p: dict) -> np.ndarray:
    rgb = rgba[..., :3]
    detail = _luma_detail(rgb, p["sigma"], p["amount"], p["threshold"], p["knee"])
    low, high = _local_bounds(rgb, max(1, int(np.ceil(3.0 * p["sigma"]))))
    result = np.clip(rgb + detail[..., None], low, high)
    result = np.maximum(result, 0.0)
    return np.concatenate((result.astype(np.float32), rgba[..., 3:4]), axis=2)


def multiscale_limited(rgba: np.ndarray, p: dict) -> np.ndarray:
    rgb = rgba[..., :3]
    lum = luma(rgb)
    fine = lum - gaussian(lum, p["sigma"])
    coarse = gaussian(lum, p["sigma"]) - gaussian(lum, 3.0 * p["sigma"])
    magnitude = np.abs(fine + p["coarse_weight"] * coarse)
    gain = smoothstep(p["threshold"], p["threshold"] + p["knee"], magnitude) \
        if p["threshold"] > 0.0 else np.float32(1.0)
    detail = (fine + p["coarse_weight"] * coarse) * gain * p["amount"]
    radius = max(1, int(np.ceil(9.0 * p["sigma"])))
    low, high = _local_bounds(rgb, radius)
    result = np.clip(rgb + detail[..., None], low, high)
    result = np.maximum(result, 0.0)
    return np.concatenate((result.astype(np.float32), rgba[..., 3:4]), axis=2)


MODELS = {
    "product_usm_mirror": product_usm_mirror,
    "luma_usm_soft": luma_usm_soft,
    "halo_limited": halo_limited,
    "multiscale_limited": multiscale_limited,
}

PRESETS = {
    "product_usm_mirror": {"amount": 1.0, "sigma": 1.0, "threshold": 0.0, "knee": 0.0},
    "luma_usm_soft": {"amount": 1.0, "sigma": 1.0, "threshold": 0.02, "knee": 0.01},
    "halo_limited": {"amount": 1.0, "sigma": 1.0, "threshold": 0.02, "knee": 0.01},
    "multiscale_limited": {"amount": 1.0, "sigma": 1.0, "threshold": 0.02,
                           "knee": 0.01, "coarse_weight": 0.35},
}


# --------------------------------------------------------------------------
# metrics
# --------------------------------------------------------------------------
def _distribution(values: np.ndarray) -> dict:
    values = np.asarray(values, np.float64).ravel()
    finite = values[np.isfinite(values)]
    if finite.size == 0:
        return {"count": int(values.size), "nonfinite": int(values.size - finite.size)}
    return {
        "count": int(values.size),
        "nonfinite": int(values.size - finite.size),
        "min": float(finite.min()), "mean": float(finite.mean()),
        "p01": float(np.percentile(finite, 1)), "p50": float(np.percentile(finite, 50)),
        "p99": float(np.percentile(finite, 99)), "max": float(finite.max()),
    }


def _clip_fraction(rgb: np.ndarray, reference: np.ndarray | None = None) -> float:
    clipped = np.any(rgb > 1.0, axis=2)
    if reference is not None:
        allowed = np.any(reference > 1.0, axis=2)
        clipped = clipped & ~allowed
    return float(clipped.mean())


def _roi(rgb: np.ndarray, box: list[int]) -> np.ndarray:
    x0, y0, x1, y1 = box
    return rgb[y0:y1, x0:x1]


def _rise_distance(profile: np.ndarray) -> float:
    lo = float(np.median(profile[:4]))
    hi = float(np.median(profile[-4:]))
    span = max(hi - lo, 1e-9)

    def crossing(target: float) -> float:
        scaled = (profile - lo) / span
        for index in range(1, scaled.size):
            if scaled[index] >= target:
                previous = scaled[index - 1]
                fraction = (target - previous) / max(scaled[index] - previous, 1e-9)
                return index - 1 + fraction
        return float(scaled.size - 1)

    return float(crossing(0.8) - crossing(0.2))


def step_edge_metrics(src: np.ndarray, out: np.ndarray, meta: dict, sigma: float) -> dict:
    x0, y0, x1, y1 = meta["rois"]["step_edge"]
    edge = meta["edge_x_px"] - x0
    radius = max(1, int(np.ceil(3.0 * sigma)))

    src_roi = src[y0:y1, x0:x1]
    out_roi = out[y0:y1, x0:x1]
    src_l = luma(src_roi)
    out_l = luma(out_roi)

    src_profile = src_l.mean(axis=0)
    out_profile = out_l.mean(axis=0)
    low_plateau = float(np.median(src_profile[:4]))
    high_plateau = float(np.median(src_profile[-4:]))

    k = 2 * radius + 1
    local_low = cv2.erode(src_l, np.ones((k, k), np.uint8), borderType=cv2.BORDER_REFLECT)
    local_high = cv2.dilate(src_l, np.ones((k, k), np.uint8), borderType=cv2.BORDER_REFLECT)
    beyond = (out_l > local_high + 1e-3) | (out_l < local_low - 1e-3)
    xs = np.arange(src_l.shape[1])
    outside_transition = np.abs(xs - edge) > 2.0
    band = beyond[:, outside_transition]
    return {
        "plateau_low": low_plateau,
        "plateau_high": high_plateau,
        "overshoot_linear": float(out_profile.max() - high_plateau),
        "overshoot_codes": float((out_profile.max() - high_plateau) * 255.0),
        "undershoot_linear": float(low_plateau - out_profile.min()),
        "halo_fraction_outside_transition": float(band.mean()),
        "rise_20_80_px": _rise_distance(out_profile),
        "source_rise_20_80_px": _rise_distance(src_profile),
    }


def _highpass_energy(rgb: np.ndarray) -> float:
    lum = luma(rgb)
    return float(np.mean((lum - gaussian(lum, 1.0)) ** 2))


def chart_metrics(src: np.ndarray, out: np.ndarray, meta: dict, model: dict) -> dict:
    rois = meta["rois"]
    sigma = float(model.get("sigma", 1.0))
    src_rgb, out_rgb = src[..., :3], out[..., :3]

    fine_box = rois["fine_lines"]
    fine_src = _roi(src_rgb, fine_box)
    fine_out = _roi(out_rgb, fine_box)

    noise = {}
    for key in ("noise_low", "noise_high"):
        a = luma(_roi(src_rgb, rois[key]))
        b = luma(_roi(out_rgb, rois[key]))
        noise[key] = {"std_src": float(a.std()), "std_out": float(b.std()),
                      "std_ratio": float(b.std() / max(a.std(), 1e-9))}

    color_box = rois["color_edge"]
    cx0, cy0, cx1, cy1 = color_box
    xs = np.arange(cx0, cx1)[None, :]
    band = np.broadcast_to(np.abs(xs - meta["color_edge_x_px"]) <= 3.0,
                           (cy1 - cy0, cx1 - cx0))
    src_band = _roi(src_rgb, color_box)
    out_band = _roi(out_rgb, color_box)
    fringe_src = np.stack((src_band[..., 0] - src_band[..., 1],
                           src_band[..., 2] - src_band[..., 1]), axis=2)
    fringe_out = np.stack((out_band[..., 0] - out_band[..., 1],
                           out_band[..., 2] - out_band[..., 1]), axis=2)
    fringe_delta = np.abs(fringe_out - fringe_src)
    chroma_band = fringe_delta[band]
    color = {
        "band_pixels": int(chroma_band.shape[0]),
        "chroma_delta_mean_codes": float(chroma_band.mean() * 255.0),
        "chroma_delta_max_codes": float(chroma_band.max() * 255.0),
    }

    return {
        "frame_luma": _distribution(luma(out_rgb)),
        "frame_new_clip_fraction": _clip_fraction(out_rgb, src_rgb),
        "fine_lines_energy_ratio": _highpass_energy(fine_out) / max(_highpass_energy(fine_src), 1e-12),
        "fine_lines_contrast_p99_p01": float(np.percentile(luma(fine_out), 99) -
                                             np.percentile(luma(fine_out), 1)),
        "fine_lines_source_contrast": float(np.percentile(luma(fine_src), 99) -
                                            np.percentile(luma(fine_src), 1)),
        "noise": noise,
        "color": color,
        "highlight_new_clip_fraction": _clip_fraction(_roi(out_rgb, rois["highlight_step"]),
                                                      _roi(src_rgb, rois["highlight_step"])),
        "step_edge": step_edge_metrics(src_rgb, out_rgb, meta, sigma),
    }


def alpha_metrics(src: np.ndarray, out: np.ndarray) -> dict:
    alpha_diff = np.abs(src[..., 3] - out[..., 3])
    composites = {}
    for name, background in BACKGROUNDS.items():
        src_rgb = src[..., :3] * src[..., 3:4] + background * (1.0 - src[..., 3:4])
        out_rgb = out[..., :3] * out[..., 3:4] + background * (1.0 - out[..., 3:4])
        src_codes = np.clip(linear_to_srgb(src_rgb), 0.0, 1.0) * 255.0
        out_codes = np.clip(linear_to_srgb(out_rgb), 0.0, 1.0) * 255.0
        composites[name] = float(np.abs(out_codes - src_codes).max())
    return {"alpha_max_abs_diff": float(alpha_diff.max()),
            "alpha_changed_pixels": float((alpha_diff > 0.0).mean()),
            "composite_max_error_codes": composites}


# --------------------------------------------------------------------------
# run helpers
# --------------------------------------------------------------------------
def save_preview(path: Path, rgba: np.ndarray, background: float | None = None) -> None:
    Image.fromarray(preview_rgb(rgba, background)).save(path)


def save_array(path: Path, rgba: np.ndarray) -> None:
    np.savez_compressed(path, rgba=rgba.astype(np.float32))


def contact_sheet(path: Path, panels: list[tuple[str, np.ndarray]], scale: float) -> None:
    prepared = []
    for label, image in panels:
        image = cv2.resize(image, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
        canvas = image.copy()
        cv2.rectangle(canvas, (0, 0), (canvas.shape[1], 22), (24, 24, 24), -1)
        cv2.putText(canvas, label, (6, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (235, 235, 235), 1,
                    cv2.LINE_AA)
        prepared.append(canvas)
    width = max(panel.shape[1] for panel in prepared)
    height = max(panel.shape[0] for panel in prepared)
    padded = []
    for panel in prepared:
        canvas = np.zeros((height, width, 3), np.uint8)
        canvas[:panel.shape[0], :panel.shape[1]] = panel
        padded.append(canvas)
    rows = []
    for start in range(0, len(padded), 2):
        chunk = padded[start:start + 2]
        if len(chunk) == 1:
            chunk.append(np.zeros_like(chunk[0]))
        rows.append(np.concatenate(chunk, axis=1))
    Image.fromarray(np.concatenate(rows, axis=0)).save(path)


def crop_zoom(rgba: np.ndarray, box: list[int], zoom: int = 4) -> np.ndarray:
    x0, y0, x1, y1 = box
    patch = preview_rgb(rgba)[y0:y1, x0:x1]
    return cv2.resize(patch, None, fx=zoom, fy=zoom, interpolation=cv2.INTER_NEAREST)


def run_model(name: str, rgba: np.ndarray, settings: dict) -> np.ndarray:
    return MODELS[name](rgba, settings)


def timed(name: str, rgba: np.ndarray, settings: dict) -> float:
    samples = []
    for _ in range(3):
        start = time.perf_counter()
        run_model(name, rgba, settings)
        samples.append((time.perf_counter() - start) * 1000.0)
    return float(np.median(samples))


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUT_DEFAULT)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()

    out_dir: Path = args.output
    out_dir.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)

    chart, chart_meta = make_chart(960, 540, rng)
    chart_hd, chart_hd_meta = make_chart(1920, 1080, np.random.default_rng(args.seed))
    alpha_edge, alpha_boxes = make_alpha_edge()
    frames, grating_roi = make_grating_sequence()

    fixtures = {
        "chart_960": chart,
        "chart_1920": chart_hd,
        "alpha_edge": alpha_edge,
    }
    sources: dict[str, dict] = {}
    for name, image in fixtures.items():
        npz_path = out_dir / f"source__{name}.npz"
        save_array(npz_path, image)
        png_path = out_dir / f"source__{name}.preview.png"
        save_preview(png_path, image, 0.18 if name == "alpha_edge" else None)
        sources[name] = {"npz": npz_path.name, "sha256": sha256(npz_path),
                         "preview": png_path.name}

    cases: list[dict] = []
    outputs: dict[tuple[str, str], np.ndarray] = {}
    for fixture_name in ("chart_960", "chart_1920"):
        source = fixtures[fixture_name]
        meta = chart_meta if fixture_name == "chart_960" else chart_hd_meta
        for model_name, preset in PRESETS.items():
            settings = dict(preset)
            if fixture_name == "chart_1920":
                settings["sigma"] = settings["sigma"] * 2.0
            result = run_model(model_name, source, settings)
            outputs[(fixture_name, model_name)] = result
            npz_path = out_dir / f"{fixture_name}__{model_name}.npz"
            save_array(npz_path, result)
            png_path = out_dir / f"{fixture_name}__{model_name}.preview.png"
            save_preview(png_path, result)
            metrics = chart_metrics(source, result, meta, settings)
            cases.append({
                "fixture": fixture_name,
                "model": model_name,
                "settings": settings,
                "output_npz": npz_path.name,
                "output_sha256": sha256(npz_path),
                "preview_png": png_path.name,
                "metrics": metrics,
            })

    # Neutral settings must be an exact identity.
    neutral = {}
    for model_name, preset in PRESETS.items():
        settings = dict(preset)
        settings["amount"] = 0.0
        result = run_model(model_name, chart, settings)
        neutral[model_name] = {
            "settings": settings,
            "max_abs_diff": float(np.abs(result[..., :3] - chart[..., :3]).max()),
            "alpha_max_abs_diff": float(np.abs(result[..., 3] - chart[..., 3]).max()),
        }

    # Alpha fixture: every model, plus composite evidence.
    alpha_cases = []
    for model_name, preset in PRESETS.items():
        settings = dict(preset)
        result = run_model(model_name, alpha_edge, settings)
        npz_path = out_dir / f"alpha_edge__{model_name}.npz"
        save_array(npz_path, result)
        png_path = out_dir / f"alpha_edge__{model_name}.preview.png"
        save_preview(png_path, result, 0.18)
        alpha_cases.append({"model": model_name, "settings": settings,
                            "output_npz": npz_path.name,
                            "output_sha256": sha256(npz_path),
                            "metrics": alpha_metrics(alpha_edge, result)})

    # Parameter sweep for the product mirror (response shape only).
    sweep = []
    for amount in (0.5, 1.0, 2.0):
        for sigma in (1.0, 2.0):
            for threshold in (0.0, 0.02):
                settings = {"amount": amount, "sigma": sigma, "threshold": threshold, "knee": 0.0}
                result = run_model("product_usm_mirror", chart, settings)
                metrics = chart_metrics(chart, result, chart_meta, settings)
                sweep.append({"settings": settings,
                              "step_edge": metrics["step_edge"],
                              "frame_new_clip_fraction": metrics["frame_new_clip_fraction"],
                              "noise_low_ratio": metrics["noise"]["noise_low"]["std_ratio"],
                              "fine_lines_energy_ratio": metrics["fine_lines_energy_ratio"]})

    # Temporal: threshold flicker on a moving low-amplitude grating.
    temporal = {}
    for model_name in ("product_usm_mirror", "halo_limited"):
        settings = dict(PRESETS[model_name])
        processed = [run_model(model_name, frame, settings) for frame in frames]
        src_hp = np.stack([luma(f[..., :3]) - gaussian(luma(f[..., :3]), 1.0) for f in frames])
        out_hp = np.stack([luma(f[..., :3]) - gaussian(luma(f[..., :3]), 1.0) for f in processed])
        x0, y0, x1, y1 = grating_roi
        src_roi = src_hp[:, y0:y1, x0:x1]
        out_roi = out_hp[:, y0:y1, x0:x1]
        src_temporal = float(src_roi.std(axis=0).mean())
        out_temporal = float(out_roi.std(axis=0).mean())
        src_step = float(np.abs(np.diff(src_roi, axis=0)).mean())
        out_step = float(np.abs(np.diff(out_roi, axis=0)).mean())
        temporal[model_name] = {
            "settings": settings,
            "hp_temporal_std_source": src_temporal,
            "hp_temporal_std_output": out_temporal,
            "temporal_std_ratio": out_temporal / max(src_temporal, 1e-12),
            "frame_diff_source": src_step,
            "frame_diff_output": out_step,
            "frame_diff_ratio": out_step / max(src_step, 1e-12),
            "roi": grating_roi,
            "frames": len(frames),
        }

    # Review sheets.
    contact_sheet(out_dir / "chart_960.contact.png",
                  [("source", preview_rgb(chart))] +
                  [(name, preview_rgb(outputs[("chart_960", name)])) for name in PRESETS],
                  0.5)
    crop_panels = []
    for label, box in (("step x4", chart_meta["rois"]["step_edge"]),
                       ("lines x4", chart_meta["rois"]["fine_lines"]),
                       ("color x4", chart_meta["rois"]["color_edge"])):
        crop_panels.append((f"source {label}", crop_zoom(chart, box)))
        for name in PRESETS:
            crop_panels.append((f"{name} {label}", crop_zoom(outputs[("chart_960", name)], box)))
    contact_sheet(out_dir / "chart_960.roi-crops.png", crop_panels, 1.0)

    diff_panels = []
    for name in PRESETS:
        diff = np.abs(outputs[("chart_960", name)][..., :3] - chart[..., :3])
        diff = np.clip(diff * 8.0, 0.0, 1.0)
        diff_panels.append((f"{name} |diff| x8",
                            np.rint(linear_to_srgb(diff) * 255.0).astype(np.uint8)))
    contact_sheet(out_dir / "chart_960.diff-x8.png", diff_panels, 0.5)

    timings = {name: timed(name, chart, dict(PRESETS[name])) for name in PRESETS}

    # Pre-registered gates, evaluated mechanically.
    gates: dict[str, dict] = {}
    for model_name in PRESETS:
        primary = next(c for c in cases
                       if c["fixture"] == "chart_960" and c["model"] == model_name)["metrics"]
        hd = next(c for c in cases
                  if c["fixture"] == "chart_1920" and c["model"] == model_name)["metrics"]
        step = primary["step_edge"]
        mirror_color = next(c for c in cases
                            if c["fixture"] == "chart_960"
                            and c["model"] == "product_usm_mirror")["metrics"]["color"]
        candidate = model_name != "product_usm_mirror"
        rise_gain = 1.0 - step["rise_20_80_px"] / step["source_rise_20_80_px"]
        hd_gain = 1.0 - hd["step_edge"]["rise_20_80_px"] / hd["step_edge"]["source_rise_20_80_px"]
        checks = {
            "halo_fraction_le_1pct": step["halo_fraction_outside_transition"] <= 0.01,
            "overshoot_le_0.004_linear": step["overshoot_linear"] <= 0.004,
            "edge_rise_reduction_ge_15pct": rise_gain >= 0.15,
            "fine_energy_ratio_ge_1.10": primary["fine_lines_energy_ratio"] >= 1.10,
            "frame_new_clip_le_0.1pct": primary["frame_new_clip_fraction"] <= 0.001,
            "highlight_new_clip_le_1pct": primary["highlight_new_clip_fraction"] <= 0.01,
            "chroma_le_50pct_of_mirror": primary["color"]["chroma_delta_mean_codes"]
            <= 0.5 * mirror_color["chroma_delta_mean_codes"],
            "noise_low_std_ratio_le_1.25": primary["noise"]["noise_low"]["std_ratio"] <= 1.25,
            "temporal_std_ratio_le_1.50": temporal[model_name]["temporal_std_ratio"] <= 1.50
            if model_name in temporal else None,
            "resolution_gain_within_25pct": abs(hd_gain - rise_gain) <= 0.25 * max(abs(rise_gain), 1e-6),
        }
        if model_name == "product_usm_mirror":
            controls = ["chroma_le_50pct_of_mirror"]
            for key in controls:
                checks[key] = None  # control, not a candidate gate
        gates[model_name] = {
            "candidate": candidate,
            "edge_rise_reduction": rise_gain,
            "edge_rise_reduction_1920": hd_gain,
            "checks": checks,
            "gated_checks_passed": all(v for v in checks.values() if v is not None),
        }

    manifest = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "lab": "DetailSharpeningLab",
        "round": "rd1",
        "evaluation_plan": "EVALUATION_PLAN.md",
        "seed": args.seed,
        "environment": {
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "pillow": Image.__version__,
        },
        "domain": "scene-linear float32 RGB, Rec.709 luma detail; straight alpha unchanged",
        "presets": PRESETS,
        "model_notes": {
            "product_usm_mirror": "Source-equation mirror of SharpenEffectCPUImpl::applyCPU "
                                  "(per-channel Gaussian USM, hard per-channel threshold, clamp >=0). "
                                  "Not an installed-effect capture.",
            "luma_usm_soft": "Luma USM with soft-knee threshold (knee 0.01).",
            "halo_limited": "Luma USM, output clamped to source local extrema (radius 3*sigma).",
            "multiscale_limited": "Fine USM + 0.35 coarse clarity, clamped to local extrema "
                                  "(radius 9*sigma).",
        },
        "rois": {"chart_960": chart_meta["rois"], "chart_1920": chart_hd_meta["rois"],
                 "alpha_edge": alpha_boxes, "grating": grating_roi},
        "sources": sources,
        "cases": cases,
        "neutral_check": neutral,
        "alpha_cases": alpha_cases,
        "sweep_product_mirror": sweep,
        "temporal": temporal,
        "timings_ms_960x540_median_of_3": timings,
        "gates": gates,
        "evaluation_status": {
            "effect_correctness_neutral": "measured: amount=0 max abs difference recorded in "
                                          "neutral_check for every model",
            "halo_overshoot": "measured on synthetic step edge",
            "detail_edge_slope": "measured on synthetic step edge",
            "color_alpha": "measured on synthetic color edge and straight-alpha fixture",
            "temporal_stability": "measured on synthetic 8-frame grating only",
            "resolution": "measured at 960 and 1920 with sigma scaled x2",
            "gpu_backend_parity": "unverified",
            "final_viewer_and_render_queue": "unverified",
            "photographic_material": "unverified (no external asset in rd1)",
            "allocation_profile": "unverified",
        },
        "limitations": [
            "Offline NumPy/OpenCV models; the product mirror is not a capture.",
            "Synthetic fixtures only; no real photographic texture, skin, foliage, or codec.",
            "Temporal fixture isolates threshold flicker; no motion compensation or cut test.",
            "Review PNGs are preview-only (Reinhard+sRGB); measurements use raw .npz floats.",
            "No GPU, preview/final route, Render Queue, or allocation evidence.",
            "No independent human reviewer; gates are mechanical on the recorded fixtures.",
        ],
    }
    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8",
                             newline="\n")
    (out_dir / "manifest.sha256").write_text(sha256(manifest_path) + "  manifest.json\n",
                                             encoding="utf-8", newline="\n")

    print(f"wrote {manifest_path}")
    for model_name, gate in gates.items():
        status = "PASS" if gate["gated_checks_passed"] else "FAIL"
        print(f"  {model_name:22s} gates={status}  rise_gain={gate['edge_rise_reduction']:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
