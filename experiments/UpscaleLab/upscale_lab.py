#!/usr/bin/env python3
"""Upscale Lab rd0 — deterministic baseline harness for still-image super-resolution R&D.

Offline, CPU-only study. Does not launch Artifact, does not prove product parity, and does
not approve a product feature. See EVALUATION_PLAN.md (pre-registered) and
docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md.

rd0 produces:
  captures/<rd>/manifest.json   fixtures, hashes, environment, seed, conditions
  captures/<rd>/metrics.json    every candidate x case x factor measurement
  captures/<rd>/gates.json      pre-registered thresholds + measured baseline
  captures/<rd>/review/*.png    review crops (display only)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import statistics
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

SEED = 20261115
ROOT = Path(__file__).resolve().parent
PHOTO_ASSETS = ROOT.parent / "GlowLab" / "assets"

CHART_W, CHART_H = 1920, 1080
TILE_W, TILE_H = 640, 400

REGION_ORDER = (
    "step_edge",
    "fine_lines",
    "text_strokes",
    "color_edge",
    "ramp",
    "highlight_step",
    "flat_dark",
    "flat_mid",
    "flat_light",
    "noise_fine",
    "noise_coarse",
    "zone_plate",
)
REGIONS: dict[str, tuple[int, int, int, int]] = {
    "step_edge": (80, 80, 240, 160),
    "fine_lines": (360, 80, 240, 160),
    "text_strokes": (640, 80, 320, 160),
    "color_edge": (1000, 80, 240, 160),
    "ramp": (1280, 80, 240, 160),
    "highlight_step": (1560, 80, 280, 160),
    "flat_dark": (80, 300, 240, 160),
    "flat_mid": (360, 300, 240, 160),
    "flat_light": (640, 300, 240, 160),
    "noise_fine": (1000, 300, 240, 160),
    "noise_coarse": (1280, 300, 240, 160),
    "zone_plate": (80, 520, 640, 400),
}
REVIEW_REGIONS = ("step_edge", "fine_lines", "text_strokes", "zone_plate")
TILE_FLAT_RECT = (0, 0, 128, 128)
TILE_ZONE_RECT = (200, 0, 200, 200)

PHOTO_SOURCES = (
    ("photo_neon_streets", "cc0-neon-streets-1536.jpg"),
    ("photo_street_light", "cc0-street-light-night.jpg"),
)

# Pre-registered gates (EVALUATION_PLAN.md section 5). Fixed before rd1.
# Revised 2026-11-15 after rd0 (user decision): the absolute 2/255 alpha composite gate is exceeded by
# the bicubic baseline itself, so it is replaced by relative gates against baseline_bicubic.
GATES = {
    "fidelity_psnr_gain_db": 0.5,
    "fidelity_ssim_gain": 0.005,
    "edge_rise_reduction": 0.10,
    "ringing_out_of_range_frac": 0.02,
    "ringing_mean_excess_codes": 1.5,
    "flat_noise_sigma_ratio": 1.25,
    "delta_e_mean_worse_than_bicubic": 0.004,
    "alpha_composite_mean_delta_vs_bicubic_codes": 1.0,
    "alpha_composite_max_ratio_vs_bicubic": 1.25,
    "temporal_std_ratio": 1.5,
}


# --------------------------------------------------------------------------- fixtures


def build_chart(seed: int = SEED) -> np.ndarray:
    """Synthetic HR plate with fixed ROIs. 8-bit sRGB, channel order RGB."""
    rng = np.random.default_rng(seed)
    img = np.full((CHART_H, CHART_W, 3), 128, np.uint8)

    def view(name: str) -> np.ndarray:
        x, y, w, h = REGIONS[name]
        return img[y : y + h, x : x + w]

    roi = view("step_edge")
    roi[:, :120] = 0
    roi[:, 120:] = 255

    roi = view("fine_lines")
    for band_index, (period, thick) in enumerate(((3, 1), (4, 1), (6, 2))):
        band = roi[band_index * 53 : (band_index + 1) * 53]
        xs = np.arange(band.shape[1])
        band[:, (xs % period) < thick] = 220

    roi = view("text_strokes")
    roi[:] = 200
    for _ in range(120):
        x = int(rng.integers(0, roi.shape[1] - 6))
        y = int(rng.integers(0, roi.shape[0] - 16))
        w = int(rng.integers(2, 4))
        h = int(rng.integers(8, 15))
        roi[y : y + h, x : x + w] = 20
    for _ in range(24):
        x0 = int(rng.integers(0, roi.shape[1] - 12))
        y0 = int(rng.integers(0, roi.shape[0] - 12))
        for step in range(int(rng.integers(6, 12))):
            xx, yy = x0 + step, y0 + step
            if 0 <= xx < roi.shape[1] and 0 <= yy < roi.shape[0]:
                roi[yy, xx] = 20

    roi = view("color_edge")
    roi[:, :120] = (255, 0, 0)
    roi[:, 120:] = (0, 255, 255)

    roi = view("ramp")
    roi[:] = np.linspace(0, 255, roi.shape[1], dtype=np.uint8)[None, :, None]

    roi = view("highlight_step")
    roi[:, :140] = 250
    roi[:, 140:] = 255

    for name, value in (("flat_dark", 24), ("flat_mid", 128), ("flat_light", 232)):
        view(name)[:] = value

    fine_noise = 128.0 + rng.normal(0.0, 3.0, (160, 240, 1))
    view("noise_fine")[:] = np.clip(fine_noise, 0, 255).astype(np.uint8)
    coarse_noise = 128.0 + rng.normal(0.0, 8.0, (160, 240, 1))
    view("noise_coarse")[:] = np.clip(coarse_noise, 0, 255).astype(np.uint8)

    roi = view("zone_plate")
    h, w = roi.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    rmax = math.hypot(cx, cy)
    r = np.hypot(xx - cx, yy - cy)
    f0, f1 = 0.02, 0.45
    phase = 2.0 * math.pi * (f0 * r + (f1 - f0) * r * r / (2.0 * rmax))
    roi[:] = np.clip(128.0 + 40.0 * np.cos(phase), 0, 255).astype(np.uint8)[..., None]

    return img


def build_sequence_tile() -> np.ndarray:
    """640x400 tile: flat mid patch, zone plate, and fine lines for temporal stability."""
    tile = np.full((TILE_H, TILE_W, 3), 128, np.uint8)

    yy, xx = np.mgrid[0:TILE_H, 0:TILE_W].astype(np.float64)
    r = np.hypot(xx - 320.0, yy - 200.0)
    rmax = math.hypot(320.0, 200.0)
    f0, f1 = 0.02, 0.45
    phase = 2.0 * math.pi * (f0 * r + (f1 - f0) * r * r / (2.0 * rmax))
    tile[0:300, 200:640] = np.clip(128.0 + 40.0 * np.cos(phase), 0, 255).astype(np.uint8)[0:300, 200:640, None]

    band = tile[300:400, 200:640]
    xs = np.arange(band.shape[1])
    band[:, (xs % 3) < 1] = 220

    return tile


def build_sequence(tile: np.ndarray) -> list[np.ndarray]:
    """5 HR frames shifted 0.5 px per frame (subpixel motion)."""
    frames: list[np.ndarray] = []
    src = tile.astype(np.float32)
    for k in range(5):
        m = np.float32([[1.0, 0.0, 0.5 * k], [0.0, 1.0, 0.0]])
        shifted = cv2.warpAffine(
            src, m, (tile.shape[1], tile.shape[0]), flags=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT
        )
        frames.append(np.clip(np.rint(shifted), 0, 255).astype(np.uint8))
    return frames


def build_alpha_fixtures(size: int = 512) -> tuple[np.ndarray, np.ndarray]:
    """(straight-alpha RGBA, hidden-RGB RGBA). Straight alpha, 8-bit."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    c = (size - 1) / 2.0
    r = np.hypot(xx - c, yy - c)

    disc = np.clip((170.0 - r) / 1.0, 0.0, 1.0)
    ring = np.clip((r - 200.0) / 1.0, 0.0, 1.0) * np.clip((215.0 - r) / 1.0, 0.0, 1.0)
    alpha = np.maximum(disc, 0.6 * ring)

    pattern = np.clip(128.0 + 60.0 * np.cos(2.0 * math.pi * r / 12.0), 0, 255).astype(np.uint8)
    rgb = np.stack([pattern, np.roll(pattern, 3, axis=1), np.roll(pattern, 7, axis=0)], axis=-1)

    a8 = np.clip(np.rint(alpha * 255.0), 0, 255).astype(np.uint8)
    straight = np.concatenate([np.where(a8[..., None] > 0, rgb, 0), a8[..., None]], axis=-1)
    hidden = np.concatenate([rgb, a8[..., None]], axis=-1)
    return straight.astype(np.uint8), hidden.astype(np.uint8)


def downscale(rgb: np.ndarray, factor: int) -> np.ndarray:
    h, w = rgb.shape[:2]
    return cv2.resize(rgb, (w // factor, h // factor), interpolation=cv2.INTER_AREA)


def downscale_rgba(rgba: np.ndarray, factor: int) -> np.ndarray:
    h, w = rgba.shape[:2]
    size = (w // factor, h // factor)
    f = rgba.astype(np.float32) / 255.0
    a = f[..., 3:4]
    premultiplied = cv2.resize(f[..., :3] * a, size, interpolation=cv2.INTER_AREA)
    alpha_small = cv2.resize(a, size, interpolation=cv2.INTER_AREA)
    if alpha_small.ndim == 2:
        alpha_small = alpha_small[..., None]
    rgb_small = np.where(alpha_small > 1e-6, premultiplied / np.maximum(alpha_small, 1e-6), 0.0)
    out = np.concatenate([np.clip(rgb_small, 0.0, 1.0), np.clip(alpha_small, 0.0, 1.0)], axis=-1)
    return np.clip(np.rint(out * 255.0), 0, 255).astype(np.uint8)


# --------------------------------------------------------------------------- candidates


def m_bilinear(rgb: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    return cv2.resize(rgb, size, interpolation=cv2.INTER_LINEAR)


def m_bicubic(rgb: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    return cv2.resize(rgb, size, interpolation=cv2.INTER_CUBIC)


def m_lanczos(rgb: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    return cv2.resize(rgb, size, interpolation=cv2.INTER_LANCZOS4)


def m_bicubic_unsharp(rgb: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    up = cv2.resize(rgb, size, interpolation=cv2.INTER_CUBIC).astype(np.float32)
    blur = cv2.GaussianBlur(up, (0, 0), 1.0)
    return np.clip(up + 1.0 * (up - blur), 0, 255).astype(np.uint8)


CLASSICAL = {
    "baseline_bilinear": m_bilinear,
    "baseline_bicubic": m_bicubic,
    "baseline_lanczos4": m_lanczos,
    "bicubic_then_unsharp": m_bicubic_unsharp,
}


class OnnxSrModel:
    """Minimal ONNX super-resolution runner (CPU provider, single thread for determinism)."""

    def __init__(self, name: str, path: Path) -> None:
        import onnxruntime as ort

        options = ort.SessionOptions()
        options.intra_op_num_threads = 1
        options.inter_op_num_threads = 1
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        self.name = name
        self.path = str(path)
        self.session = ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"])
        self.input = self.session.get_inputs()[0]
        self.output = self.session.get_outputs()[0]
        shape = self.input.shape
        self.nhwc = len(shape) == 4 and shape[-1] in (1, 3, 4) and shape[1] not in (1, 3, 4)
        self.scale = 0

    def _to_input(self, rgb: np.ndarray) -> np.ndarray:
        x = rgb.astype(np.float32) / 255.0
        if self.nhwc:
            return np.ascontiguousarray(x[None])
        return np.ascontiguousarray(np.transpose(x, (2, 0, 1))[None])

    @staticmethod
    def _to_hwc(y: np.ndarray) -> np.ndarray:
        y = np.squeeze(np.asarray(y))
        if y.ndim == 3 and y.shape[0] in (1, 3, 4) and y.shape[-1] not in (1, 3, 4):
            y = np.transpose(y, (1, 2, 0))
        if y.ndim == 2:
            y = np.repeat(y[..., None], 3, axis=-1)
        return y

    def __call__(self, rgb: np.ndarray, size: tuple[int, int]) -> np.ndarray:
        src_h = rgb.shape[0]
        raw = self.session.run([self.output.name], {self.input.name: self._to_input(rgb)})[0]
        y = self._to_hwc(raw)
        self.scale = int(round(y.shape[0] / src_h))
        out = np.clip(np.rint(np.clip(y, 0.0, 1.0) * 255.0), 0, 255).astype(np.uint8)
        if out.shape[:2] != size:
            interp = cv2.INTER_AREA if out.shape[0] > size[1] else cv2.INTER_LINEAR
            out = cv2.resize(out, size, interpolation=interp)
        return out


# --------------------------------------------------------------------------- metrics


def luma(rgb: np.ndarray) -> np.ndarray:
    f = rgb.astype(np.float32)
    return f[..., 0] * 0.2126 + f[..., 1] * 0.7152 + f[..., 2] * 0.0722


def psnr(a: np.ndarray, b: np.ndarray) -> float:
    mse = float(np.mean((luma(a) - luma(b)) ** 2))
    if mse <= 1e-12:
        return 99.0
    return 10.0 * math.log10(255.0 * 255.0 / mse)


def ssim(a: np.ndarray, b: np.ndarray) -> float:
    x, y = luma(a), luma(b)
    kernel = cv2.getGaussianKernel(11, 1.5)
    win = (kernel @ kernel.T).astype(np.float32)
    mu_x = cv2.filter2D(x, -1, win)[5:-5, 5:-5]
    mu_y = cv2.filter2D(y, -1, win)[5:-5, 5:-5]
    xx = cv2.filter2D(x * x, -1, win)[5:-5, 5:-5]
    yy = cv2.filter2D(y * y, -1, win)[5:-5, 5:-5]
    xy = cv2.filter2D(x * y, -1, win)[5:-5, 5:-5]
    c1, c2 = (0.01 * 255.0) ** 2, (0.03 * 255.0) ** 2
    num = (2.0 * mu_x * mu_y + c1) * (2.0 * xy + c2)
    den = (mu_x**2 + mu_y**2 + c1) * (xx + yy + c2)
    return float(np.mean(num / den))


def oklab(rgb: np.ndarray) -> np.ndarray:
    f = rgb.astype(np.float32) / 255.0
    lin = np.where(f <= 0.04045, f / 12.92, ((f + 0.055) / 1.055) ** 2.4).astype(np.float32)
    l = 0.4122214708 * lin[..., 0] + 0.5363325363 * lin[..., 1] + 0.0514459929 * lin[..., 2]
    m = 0.2119034982 * lin[..., 0] + 0.6806995451 * lin[..., 1] + 0.1073969566 * lin[..., 2]
    s = 0.0883024619 * lin[..., 0] + 0.2817188376 * lin[..., 1] + 0.6299787005 * lin[..., 2]
    l_, m_, s_ = np.cbrt(l), np.cbrt(m), np.cbrt(s)
    return np.stack(
        [
            0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
            1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
            0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
        ],
        axis=-1,
    )


def delta_e(a: np.ndarray, b: np.ndarray) -> dict[str, float]:
    d = np.linalg.norm(oklab(a) - oklab(b), axis=-1)
    return {"mean": float(np.mean(d)), "p95": float(np.percentile(d, 95)), "max": float(np.max(d))}


def edge_rise(profile: np.ndarray) -> float:
    n = len(profile)
    mid = max(2, n // 2)
    left, right = float(np.median(profile[:mid])), float(np.median(profile[mid:]))
    lo, hi = (left, right) if right >= left else (right, left)
    if hi - lo < 1e-6:
        return float("nan")

    def cross(t: float) -> float:
        for i in range(1, n):
            a, b = float(profile[i - 1]), float(profile[i])
            if (a - t) * (b - t) <= 0.0 and a != b:
                return (i - 1) + (t - a) / (b - a)
        return float("nan")

    return abs(cross(lo + 0.8 * (hi - lo)) - cross(lo + 0.2 * (hi - lo)))


def ringing(y: np.ndarray, gt: np.ndarray, tol: float = 2.0, margin: int = 3) -> dict[str, float]:
    """Fraction and mean of output outside the source's local (5x5) extrema.

    `margin` excludes the ROI border: an edge just outside a crop leaks into the crop and
    would otherwise be reported as ringing that is not there.
    """
    if margin > 0 and y.shape[0] > 2 * margin and y.shape[1] > 2 * margin:
        y = y[margin:-margin, margin:-margin]
        gt = gt[margin:-margin, margin:-margin]
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5))
    # BORDER_REPLICATE keeps the crop border from being treated as unknown extrema.
    gmin = cv2.erode(gt, kernel, borderType=cv2.BORDER_REPLICATE)
    gmax = cv2.dilate(gt, kernel, borderType=cv2.BORDER_REPLICATE)
    excess = np.maximum(0.0, y - gmax) + np.maximum(0.0, gmin - y)
    return {
        "out_of_range_frac": float(np.mean(excess > tol)),
        "mean_excess": float(np.mean(excess)),
        "max_excess": float(np.max(excess)),
    }


def noise_sigma(y: np.ndarray) -> float:
    hp = y - cv2.GaussianBlur(y, (0, 0), 2.0)
    return float(np.std(hp))


def hf_energy(y: np.ndarray) -> float:
    return float(np.mean(cv2.Laplacian(y, cv2.CV_32F, ksize=3).astype(np.float64) ** 2))


def clip_stats(out: np.ndarray, gt: np.ndarray) -> dict[str, float]:
    out_y, gt_y = luma(out), luma(gt)
    return {
        "newly_clipped_frac": float(np.mean((gt_y < 255.0) & (out_y >= 255.0))),
        "non_finite": int(np.sum(~np.isfinite(out_y))),
    }


def crop(img: np.ndarray, rect: tuple[int, int, int, int]) -> np.ndarray:
    x, y, w, h = rect
    return img[y : y + h, x : x + w]


def profile_rise(out: np.ndarray, gt: np.ndarray, rect: tuple[int, int, int, int]) -> dict:
    """20->80% rise distance of the row-averaged horizontal profile, model vs source."""
    source = edge_rise(luma(crop(gt, rect)).mean(axis=0))
    model = edge_rise(luma(crop(out, rect)).mean(axis=0))
    reduction = (source - model) / source if source and source == source and source > 1e-6 else float("nan")
    return {"source": source, "model": model, "reduction_vs_source": reduction}


def evaluate_region(out: np.ndarray, gt: np.ndarray, rect: tuple[int, int, int, int]) -> dict:
    o, g = crop(out, rect), crop(gt, rect)
    oy, gy = luma(o), luma(g)
    hf_gt = hf_energy(gy)
    sigma_gt = noise_sigma(gy)
    row = {
        "psnr": psnr(o, g),
        "ssim": ssim(o, g),
        "delta_e": delta_e(o, g),
        "mean_abs_err": float(np.mean(np.abs(o.astype(np.float32) - g.astype(np.float32)))),
        "hf_energy_ratio": (hf_energy(oy) / hf_gt) if hf_gt > 1e-9 else float("nan"),
        "noise_sigma_ratio": (noise_sigma(oy) / sigma_gt) if sigma_gt > 1e-9 else float("nan"),
        "edge_rise": profile_rise(out, gt, rect),
        "ringing": ringing(oy, gy),
    }
    row.update(clip_stats(o, g))
    return row


def alpha_composite_error(out_rgb: np.ndarray, alpha: np.ndarray, gt_rgb: np.ndarray, gt_a: np.ndarray) -> dict:
    """Composite error as displayed: candidate RGB with the alpha actually used, vs source RGB with source alpha."""
    model_a = alpha.astype(np.float32) / 255.0
    gt_alpha = gt_a.astype(np.float32) / 255.0
    band = ((gt_a >= 5) & (gt_a <= 250)).astype(np.uint8)
    band = cv2.dilate(band, cv2.getStructuringElement(cv2.MORPH_RECT, (7, 7))).astype(bool)
    result = {"alpha_max_diff_vs_gt": int(np.max(np.abs(alpha.astype(np.int16) - gt_a.astype(np.int16))))}
    for name, bg in (("black", 0.0), ("mid", 128.0), ("white", 255.0)):
        comp_out = out_rgb.astype(np.float32) * model_a[..., None] + bg * (1.0 - model_a[..., None])
        comp_gt = gt_rgb.astype(np.float32) * gt_alpha[..., None] + bg * (1.0 - gt_alpha[..., None])
        err = np.abs(comp_out - comp_gt).max(axis=-1)[band]
        result[f"{name}_max"] = float(np.max(err)) if err.size else 0.0
        result[f"{name}_mean"] = float(np.mean(err)) if err.size else 0.0
    return result


# --------------------------------------------------------------------------- runner


@dataclass
class Case:
    key: str
    kind: str
    hr: np.ndarray
    regions: dict[str, tuple[int, int, int, int]]
    factor: int
    rgba: bool = False
    lrs: dict[int, np.ndarray] = field(default_factory=dict)


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def save_png(path: Path, rgb: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(path), cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))


def build_cases(work: Path, factors: list[int], include: set[str]) -> list[Case]:
    cases: list[Case] = []
    chart = build_chart()
    save_png(work / "chart_hr.png", chart)
    for factor in factors:
        cases.append(Case("chart", "chart", chart, REGIONS, factor, lrs={factor: downscale(chart, factor)}))

    for key, filename in PHOTO_SOURCES:
        path = PHOTO_ASSETS / filename
        if not path.is_file():
            print(f"[skip] missing photo asset: {path}", file=sys.stderr)
            continue
        photo = cv2.cvtColor(cv2.imread(str(path), cv2.IMREAD_COLOR), cv2.COLOR_BGR2RGB)
        save_png(work / f"{key}_hr.png", photo)
        h, w = photo.shape[:2]
        regions = {
            "full": (0, 0, w, h),
            "center": (max(0, w // 2 - 240), max(0, h // 2 - 135), min(480, w), min(270, h)),
            "detail": (max(0, w // 4), max(0, h // 4), min(320, w), min(180, h)),
        }
        for factor in factors:
            cases.append(Case(key, "photo", photo, regions, factor, lrs={factor: downscale(photo, factor)}))

    if "alpha" in include:
        straight, hidden = build_alpha_fixtures()
        for name, rgba in (("alpha_straight", straight), ("alpha_hidden_rgb", hidden)):
            save_png(work / f"{name}_hr.png", rgba)
            for factor in factors:
                cases.append(
                    Case(
                        name,
                        "alpha",
                        rgba,
                        {"disc": (156, 156, 200, 200)},
                        factor,
                        rgba=True,
                        lrs={factor: downscale_rgba(rgba, factor)},
                    )
                )

    if "sequence" in include:
        tile = build_sequence_tile()
        frames = build_sequence(tile)
        seq_dir = work / "sequence_hr"
        seq_dir.mkdir(parents=True, exist_ok=True)
        for index, frame in enumerate(frames):
            save_png(seq_dir / f"frame_{index:02d}.png", frame)
        cases.append(Case("sequence", "sequence", frames[0], {}, factors[-1]))

    return cases


def run_model(model, lr: np.ndarray, size: tuple[int, int], rgba: bool) -> np.ndarray:
    if rgba:
        rgb = model(lr[..., :3], size)
        alpha = cv2.resize(lr[..., 3], size, interpolation=cv2.INTER_CUBIC)
        return np.concatenate([rgb, alpha[..., None]], axis=-1)
    return model(lr, size)


def save_review(out_dir: Path, case: Case, factor: int, name: str, out: np.ndarray) -> None:
    if case.kind == "photo":
        rect = (case.hr.shape[1] // 2 - 240, case.hr.shape[0] // 2 - 135, 480, 270)
    else:
        rect = next((case.regions[r] for r in REVIEW_REGIONS if r in case.regions), None)
    if rect is None:
        return

    if case.kind == "alpha":
        def flatten(img: np.ndarray) -> np.ndarray:
            rgb = crop(img[..., :3], rect).astype(np.float32)
            alpha = crop(img[..., 3], rect).astype(np.float32)[..., None] / 255.0
            return np.clip(rgb * alpha + 128.0 * (1.0 - alpha), 0, 255).astype(np.uint8)

        out_crop, gt_crop = flatten(out), flatten(case.hr)
    else:
        out_crop, gt_crop = crop(out, rect), crop(case.hr, rect)

    gap = np.full((4, out_crop.shape[1], 3), 255, np.uint8)
    sheet = np.concatenate([gt_crop, gap, out_crop], axis=0)
    sheet = cv2.resize(sheet, (sheet.shape[1] * 2, sheet.shape[0] * 2), interpolation=cv2.INTER_NEAREST)
    save_png(out_dir / "review" / f"{case.key}_x{factor}" / f"{name}.png", sheet)


def run_sequence(candidates: dict, factor: int) -> dict:
    tile = build_sequence_tile()
    frames = build_sequence(tile)
    size = (tile.shape[1], tile.shape[0])
    result: dict[str, dict] = {}
    for name, model in candidates.items():
        outputs = [run_model(model, downscale(frame, factor), size, False) for frame in frames]
        flat_means, flat_hf, zone_means, zone_hf, diffs = [], [], [], [], []
        for index, out in enumerate(outputs):
            flat = luma(crop(out, TILE_FLAT_RECT))
            zone = luma(crop(out, TILE_ZONE_RECT))
            flat_means.append(float(np.mean(flat)))
            flat_hf.append(hf_energy(flat))
            zone_means.append(float(np.mean(zone)))
            zone_hf.append(hf_energy(zone))
            if index:
                previous = luma(crop(outputs[index - 1], TILE_ZONE_RECT))
                diffs.append(float(np.mean(np.abs(zone - previous))))
        result[name] = {
            "flat_mean_std": float(np.std(flat_means)),
            "flat_hf_std": float(np.std(flat_hf)),
            "zone_mean_std": float(np.std(zone_means)),
            "zone_hf_std": float(np.std(zone_hf)),
            "zone_frame_diff_mean": float(np.mean(diffs)) if diffs else 0.0,
            "frames": len(outputs),
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Upscale Lab rd0 baseline harness")
    parser.add_argument("--rd", default="rd0", help="run label, e.g. rd0 / rd1")
    parser.add_argument("--factors", type=int, nargs="+", default=[2, 4])
    parser.add_argument(
        "--cases",
        nargs="+",
        default=["chart", "photo", "alpha", "sequence"],
        choices=["chart", "photo", "alpha", "sequence"],
    )
    parser.add_argument(
        "--onnx",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="optional ONNX candidate, repeatable",
    )
    args = parser.parse_args()

    out_dir = ROOT / "captures" / args.rd
    work = out_dir / "fixtures"
    work.mkdir(parents=True, exist_ok=True)

    started = time.time()
    cases = build_cases(work, args.factors, set(args.cases))

    candidates: dict[str, object] = dict(CLASSICAL)
    onnx_meta: dict[str, dict] = {}
    for spec in args.onnx:
        name, _, path = spec.partition("=")
        model_path = Path(path)
        if not model_path.is_file():
            print(f"[skip] onnx model not found: {model_path}", file=sys.stderr)
            continue
        candidates[name] = OnnxSrModel(name, model_path)
        onnx_meta[name] = {"path": str(model_path), "sha256": sha256_of(model_path)}

    metrics: dict[str, dict] = {}
    for case in cases:
        if case.kind == "sequence":
            continue
        for factor, lr in case.lrs.items():
            target = (case.hr.shape[1], case.hr.shape[0])
            metrics.setdefault(case.key, {}).setdefault(str(factor), {})
            for name, model in candidates.items():
                timings = []
                for _ in range(3):
                    begin = time.perf_counter()
                    out = run_model(model, lr, target, case.rgba)
                    timings.append(time.perf_counter() - begin)

                entry = {
                    "kind": "classical" if name in CLASSICAL else "learned",
                    "ms_median": statistics.median(timings) * 1000.0,
                    "model_scale": getattr(model, "scale", None),
                    "regions": {},
                    "global": {},
                }
                if case.kind == "alpha":
                    entry["global"] = alpha_composite_error(
                        out[..., :3], out[..., 3], case.hr[..., :3], case.hr[..., 3]
                    )
                    for region, rect in case.regions.items():
                        entry["regions"][region] = evaluate_region(out[..., :3], case.hr[..., :3], rect)
                else:
                    for region, rect in case.regions.items():
                        entry["regions"][region] = evaluate_region(out, case.hr, rect)
                    entry["global"] = {
                        "psnr": psnr(out, case.hr),
                        "ssim": ssim(out, case.hr),
                        "delta_e": delta_e(out, case.hr),
                    }
                metrics[case.key][str(factor)][name] = entry
                save_review(out_dir, case, factor, name, out)

    if "sequence" in args.cases:
        metrics["sequence"] = run_sequence(candidates, args.factors[-1])

    reference = metrics.get("chart", {}).get("2", {}).get("baseline_bicubic", {}).get("global", {})
    baseline: dict[str, dict] = {}
    for factor in (str(f) for f in args.factors):
        entry = metrics.get("chart", {}).get(factor, {}).get("baseline_bicubic")
        if not entry:
            continue
        baseline[factor] = {
            "psnr": entry["global"]["psnr"],
            "ssim": entry["global"]["ssim"],
            "edge_rise_model": entry["regions"]["step_edge"]["edge_rise"]["model"],
            "edge_rise_source": entry["regions"]["step_edge"]["edge_rise"]["source"],
            "ringing_out_of_range_frac": entry["regions"]["step_edge"]["ringing"]["out_of_range_frac"],
            "noise_sigma_ratio_noise_fine": entry["regions"]["noise_fine"]["noise_sigma_ratio"],
            "hf_energy_ratio_step_edge": entry["regions"]["step_edge"]["hf_energy_ratio"],
        }
    alpha_baseline: dict[str, dict] = {}
    for factor in (str(f) for f in args.factors):
        entry = metrics.get("alpha_straight", {}).get(factor, {}).get("baseline_bicubic", {}).get("global")
        if entry:
            alpha_baseline[factor] = entry
    if alpha_baseline:
        baseline["alpha_composite"] = alpha_baseline

    sequence_entry = metrics.get("sequence", {}).get("baseline_bicubic")
    if sequence_entry:
        baseline["sequence"] = sequence_entry

    gates = {
        "thresholds": GATES,
        "reference_case": "chart",
        "reference_factor": 2,
        "reference_candidate": "baseline_bicubic",
        "reference_values": reference,
        "rd0_baseline": baseline,
        "gate_revision": [
            {
                "gate": "alpha_composite",
                "previous": {"name": "alpha_composite_error_codes", "threshold": 2.0},
                "replaced_by": {
                    "name": "alpha_composite_mean_delta_vs_bicubic_codes",
                    "threshold": GATES["alpha_composite_mean_delta_vs_bicubic_codes"],
                    "secondary": {
                        "name": "alpha_composite_max_ratio_vs_bicubic",
                        "threshold": GATES["alpha_composite_max_ratio_vs_bicubic"],
                    },
                    "definition": "band composite error per background (black/mid/white). Candidate band "
                    "mean may be at most +1.0 code vs baseline_bicubic at the same factor, and band max at "
                    "most 1.25x. Alpha is resized identically for every candidate, so this compares RGB "
                    "accuracy under the same alpha.",
                },
                "measured_baseline": alpha_baseline,
                "reason": "a 1 px source alpha ramp cannot be reproduced by any upscaler; the absolute "
                "2/255 gate is exceeded by baseline_bicubic itself (x2 black max 60.5, mean 4.5), so it "
                "cannot discriminate candidates.",
                "decided": "2026-11-15",
                "decided_by": "user approval (relative/absolute gate revision)",
            }
        ],
        "note": "thresholds fixed before rd1; baseline recorded for comparison only",
    }

    manifest = {
        "rd": args.rd,
        "seed": SEED,
        "created": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "factors": args.factors,
        "cases": [case.key for case in cases],
        "candidates": sorted(candidates),
        "onnx_models": onnx_meta,
        "fixtures": {path.name: sha256_of(path) for path in sorted(work.glob("*.png"))},
        "sequence_frames": [path.name for path in sorted((work / "sequence_hr").glob("*.png"))],
        "environment": {
            "python": sys.version.split()[0],
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "pillow": Image.__version__,
            "platform": platform.platform(),
        },
        "reviewer": "AI agent; independent human visual review not performed",
        "elapsed_s": time.time() - started,
    }

    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    (out_dir / "gates.json").write_text(json.dumps(gates, indent=2), encoding="utf-8")
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"rd={args.rd} cases={len(cases)} candidates={len(candidates)} out={out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())