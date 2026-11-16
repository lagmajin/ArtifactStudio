#!/usr/bin/env python3
"""Denoise Lab rd0 — deterministic spatial-denoise baseline harness.

Offline, CPU-only study for spatial (single-frame) denoising of still and sequence image
layers. Does not launch Artifact, does not prove product parity, and does not approve a
product feature. See EVALUATION_PLAN.md (pre-registered) and
docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md.

rd0 produces:
  captures/<rd>/manifest.json   fixtures, hashes, environment, seed, noise models, ROI
  captures/<rd>/metrics.json    every candidate x case x noise model x region measurement
  captures/<rd>/gates.json      pre-registered thresholds, measured raw baseline, verdicts
  captures/<rd>/review/*.png    review sheets (display only)
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
from pathlib import Path

import cv2
import numpy as np

SEED = 20261115
ROOT = Path(__file__).resolve().parent
PHOTO_ASSETS = ROOT.parent / "GlowLab" / "assets"

CHART_W, CHART_H = 1536, 1024
TILE_W, TILE_H = 640, 400
TILE_FLAT_RECT = (16, 16, 160, 120)
TILE_ZONE_RECT = (200, 0, 220, 200)

# ROIs inside chart_hr.png (x, y, w, h). Non-overlapping on purpose.
REGIONS: dict[str, tuple[int, int, int, int]] = {
    "step_edge": (64, 64, 200, 120),
    "fine_lines": (288, 64, 240, 120),
    "text_strokes": (552, 64, 280, 120),
    "color_edge": (856, 64, 200, 120),
    "flat_dark": (1080, 64, 200, 120),
    "flat_mid": (1304, 64, 200, 120),
    "flat_light": (64, 208, 200, 120),
    "skin_tone": (288, 208, 240, 120),
    "ramp": (552, 208, 280, 120),
    "fine_detail": (856, 208, 200, 120),
    "zone_plate": (64, 352, 600, 360),
    "flat_colored": (700, 352, 240, 200),
    "hair_strands": (964, 352, 300, 200),
}
REGION_ORDER = tuple(REGIONS)
FLAT_REGIONS = ("flat_dark", "flat_mid", "flat_light")
REVIEW_REGIONS = ("fine_lines", "text_strokes", "hair_strands", "flat_colored")

# Noise is added in float before clipping to 8-bit:
#   channel = image + luma_noise + chroma_noise[c], luma_noise ~ N(0, luma_sigma^2),
#   chroma_noise[c] ~ N(0, chroma_sigma^2) independent per channel.
NOISE_MODELS: dict[str, dict[str, float]] = {
    "fine": {"luma_sigma": 5.0, "chroma_sigma": 3.0},
    "strong": {"luma_sigma": 12.0, "chroma_sigma": 12.0},
    "lowlight": {"luma_sigma": 20.0, "chroma_sigma": 30.0},
}

PHOTO_SOURCES = (("photo_neon_streets", "cc0-neon-streets-1536.jpg"),)

# Pre-registered gates (EVALUATION_PLAN.md section 5). Relative to the noisy input of the
# same case, so a candidate is judged on what it removed and what it destroyed.
GATES = {
    "flat_noise_sigma_ratio_max": 0.40,
    "psnr_gain_min_db": 2.0,
    "line_contrast_retention_min": 0.85,
    "edge_shift_max_px": 0.25,
    "delta_e_improvement_min": 0.5,
    "ramp_level_retention_min": 0.85,
    "ramp_max_adjacent_step_codes": 2.0,
    "chroma_noise_ratio_max": 0.60,
    "temporal_flat_std_ratio_max": 1.25,
}

# Gate defects found by rd0. Gates are left unchanged so the recorded verdicts stay reproducible;
# revisions require explicit approval (IMAGE_EFFECT_RND_EVALUATION_RULES.md, "比較と判定").
GATE_REVIEW = {
    "status": "proposed, not approved",
    "date": "2026-11-15",
    "items": [
        {
            "gate": "ramp_max_adjacent_step_codes",
            "registered": GATES["ramp_max_adjacent_step_codes"],
            "measured_control": "3.6 / 5.8 / 11.3 code (fine / strong / lowlight baseline_raw)",
            "problem": "The control itself violates the gate: the metric is the max over ~279 adjacent column-mean differences, so it is dominated by residual noise rather than by posterization.",
            "proposed_change": "Replace the absolute max with a control-relative bound (<= control value) or a robust statistic (median adjacent step, or a contiguous-flat run length).",
        },
        {
            "gate": "delta_e_improvement_min",
            "registered": GATES["delta_e_improvement_min"],
            "measured_control": "OKLab dE mean vs clean is 0.017 / 0.052 / 0.109 for the noisy control (fine / strong / lowlight)",
            "problem": "The absolute 0.5 threshold was written in a 0-100 dE scale; in this OKLab metric every candidate and control sits between 0.005 and 0.11, so no candidate can pass.",
            "proposed_change": "Scale the gate to this metric (e.g. >= 40% relative reduction of dE mean against baseline_raw) and record the before/after.",
        },
    ],
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
    roi[:, :100] = 32
    roi[:, 100:] = 224

    roi = view("fine_lines")
    roi[:] = 96
    for band_index, (period, thick, value) in enumerate(((3, 1, 230), (4, 1, 200), (6, 2, 180))):
        band = roi[band_index * 40 : (band_index + 1) * 40]
        xs = np.arange(band.shape[1])
        band[:, (xs % period) < thick] = value

    roi = view("text_strokes")
    roi[:] = 210
    for _ in range(90):
        x = int(rng.integers(0, roi.shape[1] - 6))
        y = int(rng.integers(0, roi.shape[0] - 16))
        w = int(rng.integers(2, 4))
        h = int(rng.integers(8, 15))
        roi[y : y + h, x : x + w] = 25
    for _ in range(18):
        x0 = int(rng.integers(0, roi.shape[1] - 12))
        y0 = int(rng.integers(0, roi.shape[0] - 12))
        for step in range(int(rng.integers(6, 12))):
            xx, yy = x0 + step, y0 + step
            if 0 <= xx < roi.shape[1] and 0 <= yy < roi.shape[0]:
                roi[yy, xx] = 25

    roi = view("color_edge")
    roi[:, :100] = (210, 40, 40)
    roi[:, 100:] = (40, 150, 200)

    for name, value in (("flat_dark", 26), ("flat_mid", 128), ("flat_light", 226)):
        view(name)[:] = value

    roi = view("skin_tone")
    xs = np.linspace(0.0, 1.0, roi.shape[1])[None, :, None]
    ys = np.linspace(0.0, 1.0, roi.shape[0])[:, None, None]
    base = np.array([214.0, 168.0, 140.0])[None, None, :]
    shade = 1.0 - 0.35 * ys - 0.10 * xs
    roi[:] = np.clip(base * shade, 0, 255).astype(np.uint8)

    roi = view("ramp")
    roi[:] = np.linspace(0, 255, roi.shape[1], dtype=np.uint8)[None, :, None]

    roi = view("fine_detail")
    h, w = roi.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    detail = 128.0 + 46.0 * np.sin(2.0 * math.pi * xx / 5.0) * np.cos(2.0 * math.pi * yy / 7.0)
    roi[:] = np.clip(detail, 0, 255).astype(np.uint8)[..., None]

    roi = view("zone_plate")
    h, w = roi.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    rmax = math.hypot(cx, cy)
    r = np.hypot(xx - cx, yy - cy)
    f0, f1 = 0.02, 0.42
    phase = 2.0 * math.pi * (f0 * r + (f1 - f0) * r * r / (2.0 * rmax))
    roi[:] = np.clip(128.0 + 42.0 * np.cos(phase), 0, 255).astype(np.uint8)[..., None]

    roi = view("flat_colored")
    roi[:] = np.array([190.0, 92.0, 70.0], np.uint8)[None, None, :]

    roi = view("hair_strands")
    roi[:] = 198
    h, w = roi.shape[:2]
    xs = np.arange(w)
    for k in range(14):
        amp = 18.0 + 3.0 * k
        y0 = 12.0 + 13.0 * k
        center = y0 + amp * np.sin(2.0 * math.pi * xs / (70.0 + 5.0 * k) + 0.4 * k)
        for x in xs:
            y = int(round(center[x]))
            if 0 <= y < h:
                roi[max(0, y - 1) : y + 1, x] = (58, 44, 38)

    return img


def build_sequence_tile(seed: int = SEED) -> np.ndarray:
    """640x400 tile: flat patch, zone plate, and fine lines for temporal stability."""
    del seed
    tile = np.full((TILE_H, TILE_W, 3), 128, np.uint8)

    yy, xx = np.mgrid[0:TILE_H, 0:TILE_W].astype(np.float64)
    r = np.hypot(xx - 320.0, yy - 200.0)
    rmax = math.hypot(320.0, 200.0)
    f0, f1 = 0.02, 0.42
    phase = 2.0 * math.pi * (f0 * r + (f1 - f0) * r * r / (2.0 * rmax))
    tile[0:300, 200:640] = np.clip(128.0 + 40.0 * np.cos(phase), 0, 255).astype(np.uint8)[0:300, 200:640, None]

    band = tile[300:400, 200:640]
    xs = np.arange(band.shape[1])
    band[:, (xs % 3) < 1] = 220

    return tile


def build_sequence(tile: np.ndarray) -> list[np.ndarray]:
    """5 clean frames shifted 0.5 px per frame (subpixel motion)."""
    frames: list[np.ndarray] = []
    src = tile.astype(np.float32)
    for k in range(5):
        m = np.float32([[1.0, 0.0, 0.5 * k], [0.0, 1.0, 0.0]])
        shifted = cv2.warpAffine(
            src, m, (tile.shape[1], tile.shape[0]), flags=cv2.INTER_CUBIC, borderMode=cv2.BORDER_REFLECT
        )
        frames.append(np.clip(np.rint(shifted), 0, 255).astype(np.uint8))
    return frames


def add_noise(clean: np.ndarray, model: dict[str, float], seed: int) -> np.ndarray:
    """Additive luma+per-channel chroma Gaussian noise, clipped and quantized to 8-bit."""
    rng = np.random.default_rng(seed)
    shape = clean.shape[:2]
    luma_noise = rng.normal(0.0, model["luma_sigma"], (shape[0], shape[1], 1))
    chroma_noise = rng.normal(0.0, model["chroma_sigma"], (shape[0], shape[1], 3))
    noisy = clean.astype(np.float32) + luma_noise + chroma_noise
    return np.clip(np.rint(noisy), 0, 255).astype(np.uint8)


# --------------------------------------------------------------------------- candidates


def c_raw(rgb: np.ndarray) -> np.ndarray:
    return rgb.copy()


def c_median3(rgb: np.ndarray) -> np.ndarray:
    return cv2.medianBlur(rgb, 3)


def c_gaussian1(rgb: np.ndarray) -> np.ndarray:
    return cv2.GaussianBlur(rgb, (0, 0), 1.0)


def c_bilateral(rgb: np.ndarray) -> np.ndarray:
    return cv2.bilateralFilter(rgb, 7, 25.0, 5.0)


def c_nlm_light(rgb: np.ndarray) -> np.ndarray:
    return cv2.fastNlMeansDenoisingColored(rgb, None, 4.0, 4.0, 7, 15)


def c_nlm_strong(rgb: np.ndarray) -> np.ndarray:
    return cv2.fastNlMeansDenoisingColored(rgb, None, 10.0, 10.0, 7, 21)


CANDIDATES: dict[str, dict] = {
    "baseline_raw": {"fn": c_raw, "params": {}},
    "median3": {"fn": c_median3, "params": {"ksize": 3}},
    "gaussian_s1": {"fn": c_gaussian1, "params": {"sigma": 1.0}},
    "bilateral": {"fn": c_bilateral, "params": {"d": 7, "sigma_color": 25.0, "sigma_space": 5.0}},
    "nlm_light": {
        "fn": c_nlm_light,
        "params": {"h": 4.0, "h_color": 4.0, "template": 7, "search": 15},
    },
    "nlm_strong": {
        "fn": c_nlm_strong,
        "params": {"h": 10.0, "h_color": 10.0, "template": 7, "search": 21},
    },
}


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


def noise_sigma(y: np.ndarray) -> float:
    hp = y - cv2.GaussianBlur(y, (0, 0), 2.0)
    return float(np.std(hp))


def hf_energy(y: np.ndarray) -> float:
    return float(np.mean(cv2.Laplacian(y, cv2.CV_32F, ksize=3).astype(np.float64) ** 2))


def chroma_noise(rgb: np.ndarray) -> float:
    lab = oklab(rgb)
    chroma = lab[..., 1:]
    hp = chroma - cv2.GaussianBlur(chroma, (0, 0), 2.0)
    return float(np.std(hp))


def crop(img: np.ndarray, rect: tuple[int, int, int, int]) -> np.ndarray:
    x, y, w, h = rect
    return img[y : y + h, x : x + w]


def line_contrast(rgb: np.ndarray) -> float:
    """Contrast of the vertical 1px stripe patterns, measured on the column-averaged profile."""
    profile = luma(rgb).mean(axis=0)
    return float(np.percentile(profile, 95) - np.percentile(profile, 5))


def edge_position(rgb: np.ndarray) -> float:
    """Sub-pixel 50% crossing position of the row-averaged horizontal step profile."""
    profile = luma(rgb).mean(axis=0)
    n = len(profile)
    lo = float(np.median(profile[: max(2, n // 4)]))
    hi = float(np.median(profile[-max(2, n // 4) :]))
    if abs(hi - lo) < 1e-6:
        return float("nan")
    level = 0.5 * (lo + hi)
    for i in range(1, n):
        a, b = float(profile[i - 1]), float(profile[i])
        if (a - level) * (b - level) <= 0.0 and a != b:
            return (i - 1) + (level - a) / (b - a)
    return float("nan")


def ramp_stats(rgb: np.ndarray) -> dict[str, float]:
    """Distinct level count and max adjacent step on the ramp ROI (banding / posterization)."""
    profile = luma(rgb).mean(axis=0)
    steps = np.abs(np.diff(profile))
    return {
        "distinct_levels": int(np.unique(np.rint(profile).astype(np.int32)).size),
        "max_adjacent_step": float(np.max(steps)) if steps.size else 0.0,
    }


def region_metrics(out: np.ndarray, clean: np.ndarray, noisy: np.ndarray, name: str, rect) -> dict:
    o, c, n = crop(out, rect), crop(clean, rect), crop(noisy, rect)
    oy, cy, ny = luma(o), luma(c), luma(n)
    row: dict[str, float] = {
        "psnr_vs_clean": psnr(o, c),
        "psnr_vs_input": psnr(o, n),
        "ssim_vs_clean": ssim(o, c),
        "delta_e_vs_clean": delta_e(o, c),
        "hf_energy_ratio_vs_clean": (hf_energy(oy) / hf_energy(cy))
        if hf_energy(cy) > 1e-9
        else float("nan"),
        "noise_sigma": noise_sigma(oy),
        "noise_sigma_input": noise_sigma(ny),
        "noise_sigma_clean": noise_sigma(cy),
        "chroma_noise": chroma_noise(o),
        "chroma_noise_input": chroma_noise(n),
    }
    if name in FLAT_REGIONS:
        row["noise_sigma_ratio"] = (
            row["noise_sigma"] / row["noise_sigma_input"] if row["noise_sigma_input"] > 1e-9 else float("nan")
        )
    if name == "fine_lines":
        row["line_contrast"] = line_contrast(o)
        row["line_contrast_clean"] = line_contrast(c)
        row["line_contrast_input"] = line_contrast(n)
        row["line_contrast_retention"] = (
            row["line_contrast"] / row["line_contrast_clean"] if row["line_contrast_clean"] > 1e-9 else float("nan")
        )
    if name == "step_edge":
        row["edge_position"] = edge_position(o)
        row["edge_position_clean"] = edge_position(c)
        shift = row["edge_position"] - row["edge_position_clean"]
        row["edge_shift_px"] = abs(shift) if shift == shift else float("nan")
    if name == "ramp":
        out_stats, clean_stats = ramp_stats(o), ramp_stats(c)
        row["distinct_levels"] = out_stats["distinct_levels"]
        row["distinct_levels_clean"] = clean_stats["distinct_levels"]
        row["level_retention"] = out_stats["distinct_levels"] / max(1, clean_stats["distinct_levels"])
        row["max_adjacent_step"] = out_stats["max_adjacent_step"]
        row["max_adjacent_step_clean"] = clean_stats["max_adjacent_step"]
    return row


def frame_metrics(out: np.ndarray, clean: np.ndarray, noisy: np.ndarray) -> dict:
    return {
        "psnr_vs_clean": psnr(out, clean),
        "psnr_vs_input": psnr(out, noisy),
        "ssim_vs_clean": ssim(out, clean),
        "delta_e_vs_clean": delta_e(out, clean),
        "mean_abs_err": float(np.mean(np.abs(out.astype(np.float32) - clean.astype(np.float32)))),
        "noise_sigma": noise_sigma(luma(out)),
        "noise_sigma_input": noise_sigma(luma(noisy)),
        "hf_energy_ratio_vs_clean": (hf_energy(luma(out)) / hf_energy(luma(clean)))
        if hf_energy(luma(clean)) > 1e-9
        else float("nan"),
        "non_finite": int(np.sum(~np.isfinite(luma(out)))),
    }


# --------------------------------------------------------------------------- verdicts


def judge(candidate: str, case_metrics: dict, noise_name: str) -> dict:
    """Evaluate one candidate against the pre-registered gates for one case."""
    if candidate == "baseline_raw":
        return {"status": "control", "failed": [], "unmeasured": []}

    results: dict[str, dict] = {}
    frame = case_metrics["frame"]
    results["psnr_gain"] = {
        "value": frame["psnr_vs_clean"] - frame["psnr_vs_input"],
        "gate": f">= {GATES['psnr_gain_min_db']} dB",
        "pass": (frame["psnr_vs_clean"] - frame["psnr_vs_input"]) >= GATES["psnr_gain_min_db"],
    }

    regions = case_metrics["regions"]
    flat_ratios = [regions[name]["noise_sigma_ratio"] for name in FLAT_REGIONS if name in regions]
    if flat_ratios:
        worst = max(flat_ratios)
        results["flat_noise_sigma_ratio"] = {
            "value": worst,
            "gate": f"<= {GATES['flat_noise_sigma_ratio_max']}",
            "pass": worst <= GATES["flat_noise_sigma_ratio_max"],
        }

    if "fine_lines" in regions:
        retention = regions["fine_lines"]["line_contrast_retention"]
        results["line_contrast_retention"] = {
            "value": retention,
            "gate": f">= {GATES['line_contrast_retention_min']}",
            "pass": retention >= GATES["line_contrast_retention_min"],
        }
    if "step_edge" in regions:
        shift = regions["step_edge"]["edge_shift_px"]
        results["edge_shift_px"] = {
            "value": shift,
            "gate": f"<= {GATES['edge_shift_max_px']}",
            "pass": shift == shift and shift <= GATES["edge_shift_max_px"],
        }
    if "ramp" in regions:
        level_retention = regions["ramp"]["level_retention"]
        max_step = regions["ramp"]["max_adjacent_step"]
        results["ramp_level_retention"] = {
            "value": level_retention,
            "gate": f">= {GATES['ramp_level_retention_min']}",
            "pass": level_retention >= GATES["ramp_level_retention_min"],
        }
        results["ramp_max_adjacent_step"] = {
            "value": max_step,
            "gate": f"<= {GATES['ramp_max_adjacent_step_codes']}",
            "pass": max_step <= GATES["ramp_max_adjacent_step_codes"],
        }
    if "flat_colored" in regions:
        region = regions["flat_colored"]
        ratio = (
            region["chroma_noise"] / region["chroma_noise_input"]
            if region["chroma_noise_input"] > 1e-9
            else float("nan")
        )
        results["chroma_noise_ratio"] = {
            "value": ratio,
            "gate": f"<= {GATES['chroma_noise_ratio_max']}",
            "pass": ratio == ratio and ratio <= GATES["chroma_noise_ratio_max"],
        }

    delta_improvement = case_metrics["frame"]["delta_e_vs_input"]["mean"] - frame["delta_e_vs_clean"]["mean"]
    results["delta_e_improvement"] = {
        "value": delta_improvement,
        "gate": f">= {GATES['delta_e_improvement_min']}",
        "pass": delta_improvement >= GATES["delta_e_improvement_min"],
    }

    failed = [key for key, item in results.items() if not item["pass"]]
    return {
        "status": "pass" if not failed else "fail",
        "noise_model": noise_name,
        "checks": results,
        "failed": failed,
        "unmeasured": [],
    }


# --------------------------------------------------------------------------- runner


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def save_png(path: Path, rgb: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(path), cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))


def timed(fn, rgb: np.ndarray, runs: int = 3) -> tuple[np.ndarray, float]:
    out = fn(rgb)
    durations = []
    for _ in range(runs):
        start = time.perf_counter()
        out = fn(rgb)
        durations.append((time.perf_counter() - start) * 1000.0)
    return out, statistics.median(durations)


def save_review(out_dir: Path, case: str, noise_name: str, name: str, clean, noisy, out) -> None:
    rect = next((REGIONS[r] for r in REVIEW_REGIONS if r in REGIONS), None)
    if rect is None:
        return
    panels = [crop(clean, rect), crop(noisy, rect), crop(out, rect)]
    gap = np.full((panels[0].shape[0], 3, 3), 255, np.uint8)
    sheet = np.concatenate([panels[0], gap, panels[1], gap, panels[2]], axis=1)
    sheet = cv2.resize(sheet, (sheet.shape[1] * 3, sheet.shape[0] * 3), interpolation=cv2.INTER_NEAREST)
    save_png(out_dir / "review" / case / noise_name / f"{name}.png", sheet)


def run_sequence(denoisers: dict, noise_name: str) -> dict:
    tile = build_sequence_tile()
    clean_frames = build_sequence(tile)
    model = NOISE_MODELS[noise_name]
    noisy_frames = [add_noise(frame, model, SEED + 900 + index) for index, frame in enumerate(clean_frames)]
    result: dict[str, dict] = {}
    for name, fn in denoisers.items():
        outputs = [fn(frame) for frame in noisy_frames]
        flat_means, flat_hf, zone_hf, diffs = [], [], [], []
        for index, out in enumerate(outputs):
            flat = luma(crop(out, TILE_FLAT_RECT))
            zone = luma(crop(out, TILE_ZONE_RECT))
            flat_means.append(float(np.mean(flat)))
            flat_hf.append(hf_energy(flat))
            zone_hf.append(hf_energy(zone))
            if index:
                previous = luma(crop(outputs[index - 1], TILE_ZONE_RECT))
                diffs.append(float(np.mean(np.abs(zone - previous))))
        result[name] = {
            "flat_mean_std": float(np.std(flat_means)),
            "flat_hf_std": float(np.std(flat_hf)),
            "zone_hf_std": float(np.std(zone_hf)),
            "zone_frame_diff_mean": float(np.mean(diffs)) if diffs else 0.0,
            "frames": len(outputs),
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Denoise Lab rd0 baseline harness")
    parser.add_argument("--rd", default="rd0", help="run label, e.g. rd0 / rd1")
    parser.add_argument("--noise", nargs="+", default=list(NOISE_MODELS), help="noise models to run")
    parser.add_argument("--no-photo", action="store_true", help="skip the CC0 photo case")
    parser.add_argument("--no-sequence", action="store_true", help="skip the sequence case")
    args = parser.parse_args()

    out_dir = ROOT / "captures" / args.rd
    work = out_dir / "fixtures"
    work.mkdir(parents=True, exist_ok=True)

    fixtures: dict[str, np.ndarray] = {"chart": build_chart()}
    for key, filename in PHOTO_SOURCES:
        if args.no_photo:
            continue
        path = PHOTO_ASSETS / filename
        if not path.is_file():
            print(f"[skip] missing photo asset: {path}", file=sys.stderr)
            continue
        fixtures[key] = cv2.cvtColor(cv2.imread(str(path), cv2.IMREAD_COLOR), cv2.COLOR_BGR2RGB)

    for key, image in fixtures.items():
        save_png(work / f"{key}.png", image)

    metrics: dict[str, dict] = {}
    timings: dict[str, dict] = {}
    for noise_name in args.noise:
        model = NOISE_MODELS[noise_name]
        metrics[noise_name] = {}
        timings[noise_name] = {}
        for case_key, clean in fixtures.items():
            noisy = add_noise(clean, model, SEED + 100 + list(NOISE_MODELS).index(noise_name))
            save_png(work / f"{case_key}_{noise_name}.png", noisy)
            case_metrics: dict[str, dict] = {"candidates": {}, "frame": {}, "regions": {}}
            for candidate, spec in CANDIDATES.items():
                out, ms = timed(spec["fn"], noisy)
                timings[noise_name].setdefault(candidate, {})[case_key] = round(ms, 2)
                case_metrics["candidates"][candidate] = frame_metrics(out, clean, noisy)
                case_metrics["candidates"][candidate]["delta_e_vs_input"] = delta_e(out, noisy)
                case_metrics["frame"][candidate] = case_metrics["candidates"][candidate]
                if case_key in ("chart", "photo_neon_streets"):
                    case_metrics["regions"][candidate] = {
                        name: region_metrics(out, clean, noisy, name, rect)
                        for name, rect in REGIONS.items()
                        if rect[0] + rect[2] <= clean.shape[1] and rect[1] + rect[3] <= clean.shape[0]
                    }
                save_review(out_dir, case_key, noise_name, candidate, clean, noisy, out)
                print(f"[{noise_name}/{case_key}] {candidate}: {ms:.1f} ms", file=sys.stderr)
            metrics[noise_name][case_key] = case_metrics

    control_measured: dict[str, dict] = {}
    for noise_name, cases in metrics.items():
        control_measured[noise_name] = {}
        for case_key, case_metrics in cases.items():
            raw_regions = case_metrics["regions"].get("baseline_raw", {})
            control_measured[noise_name][case_key] = {
                "ramp_max_adjacent_step": raw_regions.get("ramp", {}).get("max_adjacent_step"),
                "ramp_max_adjacent_step_clean": raw_regions.get("ramp", {}).get("max_adjacent_step_clean"),
                "delta_e_vs_clean_mean": case_metrics["frame"]["baseline_raw"]["delta_e_vs_clean"]["mean"],
                "psnr_vs_clean": case_metrics["frame"]["baseline_raw"]["psnr_vs_clean"],
            }

    verdicts: dict[str, dict] = {}
    for noise_name, cases in metrics.items():
        verdicts[noise_name] = {}
        for case_key, case_metrics in cases.items():
            if not case_metrics["regions"]:
                verdicts[noise_name][case_key] = {
                    candidate: {"status": "not-applicable", "failed": [], "unmeasured": ["region gates"]}
                    for candidate in CANDIDATES
                }
                continue
            verdicts[noise_name][case_key] = {
                candidate: judge(
                    candidate,
                    {
                        "frame": case_metrics["frame"][candidate],
                        "regions": case_metrics["regions"][candidate],
                    },
                    noise_name,
                )
                for candidate in CANDIDATES
            }

    sequence: dict[str, dict] = {}
    if not args.no_sequence:
        for noise_name in args.noise:
            sequence[noise_name] = run_sequence(
                {name: spec["fn"] for name, spec in CANDIDATES.items()}, noise_name
            )

    gates = {
        "gates": GATES,
        "determined": "2026-11-15 (pre-registered in EVALUATION_PLAN.md before rd0)",
        "gate_revision": None,
        "gate_review": GATE_REVIEW,
        "control_measured": control_measured,
        "baseline_reference": "candidate baseline_raw (no denoising) of the same case and noise model",
        "notes": [
            "Gates are relative to the noisy input so they measure removal and destruction together.",
            "Sequence gate uses baseline_raw of the same noise model as the reference.",
        ],
        "verdicts": verdicts,
        "sequence": sequence,
    }

    manifest = {
        "lab": "DenoiseLab",
        "rd": args.rd,
        "date": "2026-11-15",
        "seed": SEED,
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "platform": platform.platform(),
        },
        "noise_models": {name: NOISE_MODELS[name] for name in args.noise},
        "noise_contract": "channel = clean + N(0, luma_sigma^2) + N(0, chroma_sigma^2) per channel, clipped to 0..255 and rounded to 8-bit",
        "candidates": {name: spec["params"] for name, spec in CANDIDATES.items()},
        "regions": {name: {"rect": list(rect), "pixels": rect[2] * rect[3]} for name, rect in REGIONS.items()},
        "fixtures": {key: {"file": f"fixtures/{key}.png", "sha256": sha256_of(work / f"{key}.png")} for key in fixtures},
        "noisy_fixtures": {
            f"{key}_{noise_name}": {
                "file": f"fixtures/{key}_{noise_name}.png",
                "sha256": sha256_of(work / f"{key}_{noise_name}.png"),
            }
            for noise_name in args.noise
            for key in fixtures
        },
        "timings_ms": timings,
        "human_visual_review": "not performed (AI agent run only)",
        "review_sheets": "review/<case>/<noise>/<candidate>.png — clean | noisy | candidate, nearest x3 (display only)",
    }

    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=1), encoding="utf-8")
    (out_dir / "gates.json").write_text(json.dumps(gates, indent=1), encoding="utf-8")
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    print(f"wrote {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
