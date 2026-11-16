#!/usr/bin/env python3
"""Matte Lab rd0 — deterministic classical-baseline harness for AI matte extraction R&D.

Offline, CPU-only study. Does not launch Artifact, does not prove product parity, and does
not approve a product feature. See EVALUATION_PLAN.md (pre-registered) and
docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md.

rd0 produces:
  captures/<rd>/manifest.json   fixtures, hashes, environment, seed, fixed settings
  captures/<rd>/metrics.json    every candidate x fixture measurement
  captures/<rd>/gates.json      pre-registered thresholds + measured baseline
  captures/<rd>/review/*.png    input / GT alpha / candidate alpha / composite tiles
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
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

SEED = 20261115
ROOT = Path(__file__).resolve().parent
PHOTO_ASSETS = ROOT.parent / "GlowLab" / "assets"
PHOTO_FG = ("cc0-neon-streets-1536.jpg", "cc0-street-light-night.jpg")

SYNTH_SIZE = 512
PHOTO_SIZE = 640
GRABCUT_ITERATIONS = 5
REVIEW_BGS = (("gray", (128, 128, 128)), ("green", (0, 177, 64)), ("white", (255, 255, 255)))

# Pre-registered gates (EVALUATION_PLAN.md section 5). Fixed before rd1.
# Revised 2026-11-15 after rd0 (user decision): the softness ratio gate is undefined because the
# binary baseline has 0 partial alpha, and composite error was absolute with no measured basis.
GATES = {
    "sad_improvement_vs_grabcut_rect": 0.40,
    "boundary_band_mae": 0.05,
    "translucent_mae": 0.08,
    "partial_recall_0p1": 0.50,
    "composite_mean_delta_vs_grabcut_rect_codes": 1.0,
    "composite_max_ratio_vs_grabcut_rect": 1.25,
    "gradient_must_improve": True,
    "connectivity_must_not_worsen": True,
}

FIXTURE_ORDER = (
    "disc_aa",
    "hair_strands",
    "glass_panel",
    "thin_wire",
    "fur_edge",
    "soft_shadow",
    "green_screen",
    "photo_composite",
)
SOFT_FIXTURES = ("hair_strands", "fur_edge")
TRANSLUCENT_FIXTURES = ("glass_panel", "soft_shadow")
ORACLE_CANDIDATES = ("grabcut_gt_trimap",)


# --------------------------------------------------------------------------- fixtures


def make_background(size: int, rng: np.random.Generator, base: float) -> np.ndarray:
    """Smooth textured background so colour-only keys cannot trivially win or lose."""
    low = rng.normal(0.0, 1.0, (6, 6, 3))
    low = cv2.resize(low, (size, size), interpolation=cv2.INTER_CUBIC)
    low = cv2.GaussianBlur(low, (0, 0), size / 12.0)
    low = low / (np.max(np.abs(low)) + 1e-6)

    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    ramp = (xx / max(1, size - 1) * 0.35 + yy / max(1, size - 1) * 0.2).astype(np.float32)
    field = base + 0.22 * low + ramp[..., None]
    field += rng.normal(0.0, 0.012, (size, size, 1)).astype(np.float32)
    return np.clip(np.rint(np.clip(field, 0.0, 1.0) * 255.0), 0, 255).astype(np.uint8)


def smooth_noise(size: int, rng: np.random.Generator, cells: int, sigma: float) -> np.ndarray:
    field = cv2.resize(
        rng.normal(0.0, 1.0, (cells, cells)).astype(np.float32), (size, size), interpolation=cv2.INTER_CUBIC
    )
    field = cv2.GaussianBlur(field, (0, 0), sigma)
    peak = float(np.max(np.abs(field))) + 1e-6
    return field / peak


def make_foreground_texture(size: int, rng: np.random.Generator) -> np.ndarray:
    """Distinct, structured foreground colour field (not a flat colour)."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    stripes = 0.5 + 0.5 * np.cos(2.0 * math.pi * xx / 19.0)
    rings = 0.5 + 0.5 * np.cos(2.0 * math.pi * np.hypot(xx - size * 0.42, yy - size * 0.55) / 27.0)
    r = 0.18 + 0.35 * stripes
    g = 0.35 + 0.30 * rings
    b = 0.22 + 0.25 * (stripes * rings)
    img = np.stack([r, g, b], axis=-1)
    img += rng.normal(0.0, 0.02, (size, size, 3)).astype(np.float32)
    return np.clip(np.rint(np.clip(img, 0.0, 1.0) * 255.0), 0, 255).astype(np.uint8)


def composite(fg: np.ndarray, bg: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    a = alpha[..., None].astype(np.float32)
    out = fg.astype(np.float32) * a + bg.astype(np.float32) * (1.0 - a)
    return np.clip(np.rint(out), 0, 255).astype(np.uint8)


@dataclass
class Fixture:
    key: str
    image: np.ndarray  # uint8 RGB, I = F*a + B*(1-a)
    alpha: np.ndarray  # float32 0..1 ground truth
    fg: np.ndarray
    bg: np.ndarray


def fixture_disc_aa(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.hypot(xx - size * 0.5, yy - size * 0.52)
    alpha = np.clip(170.0 - r, 0.0, 1.0).astype(np.float32)
    fg = make_foreground_texture(size, rng)
    bg = make_background(size, rng, 0.42)
    return Fixture("disc_aa", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_hair_strands(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    alpha = np.zeros((size, size), np.float32)
    for _ in range(60):
        x0 = float(rng.uniform(0.15, 0.85) * size)
        y0 = float(rng.uniform(0.05, 0.5) * size)
        weight = float(rng.uniform(0.2, 1.0))
        points = [(x0, y0)]
        x, y = x0, y0
        for _ in range(int(rng.integers(10, 22))):
            x += float(rng.uniform(-12.0, 12.0))
            y += float(rng.uniform(8.0, 22.0))
            points.append((x, y))
        canvas = np.zeros((size, size), np.uint8)
        cv2.polylines(
            canvas,
            [np.rint(np.array(points)).astype(np.int32)],
            False,
            255,
            1,
            cv2.LINE_AA,
        )
        strand = (canvas.astype(np.float32) / 255.0) * weight
        alpha = np.maximum(alpha, np.clip(strand, 0.0, 1.0))
    fg = make_foreground_texture(size, rng)
    bg = make_background(size, rng, 0.30)
    return Fixture("hair_strands", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_glass_panel(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    alpha = np.zeros((size, size), np.float32)
    panel = np.zeros((size, size), np.uint8)
    box = cv2.boxPoints(((size * 0.5, size * 0.52), (size * 0.62, size * 0.44), 18.0))
    cv2.fillConvexPoly(panel, np.rint(box).astype(np.int32), 255)
    alpha[panel > 0] = 0.35

    highlight = np.zeros((size, size), np.uint8)
    hbox = cv2.boxPoints(((size * 0.44, size * 0.42), (size * 0.34, size * 0.08), 18.0))
    cv2.fillConvexPoly(highlight, np.rint(hbox).astype(np.int32), 255)
    highlight = cv2.GaussianBlur(highlight.astype(np.float32) / 255.0, (0, 0), 4.0)
    alpha = np.clip(alpha + 0.30 * highlight, 0.0, 1.0).astype(np.float32)

    fg = np.clip(make_foreground_texture(size, rng).astype(np.float32) * 0.6 + 110.0, 0, 255).astype(np.uint8)
    bg = make_background(size, rng, 0.55)
    return Fixture("glass_panel", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_thin_wire(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    alpha = np.zeros((size, size), np.float32)
    for _ in range(9):
        canvas = np.zeros((size, size), np.uint8)
        x = float(rng.uniform(0.0, size))
        points = []
        for _ in range(8):
            points.append((x, float(rng.uniform(-20.0, size + 20.0))))
            x += float(rng.uniform(-26.0, 26.0))
        cv2.polylines(
            canvas,
            [np.rint(np.array(points)).astype(np.int32)],
            False,
            255,
            int(rng.integers(1, 3)),
            cv2.LINE_AA,
        )
        alpha = np.maximum(alpha, canvas.astype(np.float32) / 255.0)
    fg = np.clip(make_foreground_texture(size, rng).astype(np.float32) * 0.4 + 40.0, 0, 255).astype(np.uint8)
    bg = make_background(size, rng, 0.62)
    return Fixture("thin_wire", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_fur_edge(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.hypot(xx - size * 0.5, yy - size * 0.5)
    coarse = smooth_noise(size, rng, 10, size / 40.0) * 9.0
    fine = smooth_noise(size, rng, 96, 1.0) * 5.0
    alpha = np.clip((150.0 + coarse + fine - r) / 4.0, 0.0, 1.0).astype(np.float32)
    fg = make_foreground_texture(size, rng)
    bg = make_background(size, rng, 0.36)
    return Fixture("fur_edge", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_soft_shadow(rng: np.random.Generator) -> Fixture:
    size = SYNTH_SIZE
    alpha = np.zeros((size, size), np.float32)

    body = np.zeros((size, size), np.uint8)
    cv2.rectangle(body, (int(size * 0.30), int(size * 0.22)), (int(size * 0.70), int(size * 0.62)), 255, -1)
    alpha[body > 0] = 1.0

    shadow = np.zeros((size, size), np.float32)
    cv2.ellipse(shadow, (int(size * 0.50), int(size * 0.66)), (int(size * 0.30), int(size * 0.09)), 0, 0, 360, 1.0, -1)
    shadow = cv2.GaussianBlur(shadow, (0, 0), 14.0)
    alpha = np.clip(alpha + 0.55 * shadow, 0.0, 1.0).astype(np.float32)

    fg = make_foreground_texture(size, rng)
    bg = make_background(size, rng, 0.66)
    return Fixture("soft_shadow", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_green_screen(rng: np.random.Generator) -> Fixture:
    """Single-colour screen fixture: the domain where a chroma keyer is supposed to work."""
    size = SYNTH_SIZE
    alpha = np.zeros((size, size), np.float32)

    body = np.zeros((size, size), np.uint8)
    cv2.ellipse(body, (int(size * 0.5), int(size * 0.60)), (int(size * 0.22), int(size * 0.29)), 0, 0, 360, 255, -1)
    cv2.rectangle(body, (int(size * 0.34), int(size * 0.60)), (int(size * 0.66), int(size * 0.88)), 255, -1)
    alpha[body > 0] = 1.0

    # Hair-like fringe along the top of the head: partial alpha the keyer should struggle with.
    for _ in range(70):
        theta = float(rng.uniform(math.pi * 1.05, math.pi * 1.95))
        x = size * 0.5 + math.cos(theta) * size * 0.22
        y = size * 0.60 + math.sin(theta) * size * 0.29
        weight = float(rng.uniform(0.25, 1.0))
        points = [(x, y)]
        for _ in range(int(rng.integers(6, 14))):
            x += float(rng.uniform(-9.0, 9.0))
            y -= float(rng.uniform(3.0, 12.0))
            points.append((x, y))
        canvas = np.zeros((size, size), np.uint8)
        cv2.polylines(canvas, [np.rint(np.array(points)).astype(np.int32)], False, 255, 1, cv2.LINE_AA)
        alpha = np.maximum(alpha, np.clip(canvas.astype(np.float32) / 255.0 * weight, 0.0, 1.0))

    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    bgf = np.zeros((size, size, 3), np.float32)
    bgf[...] = (34.0, 177.0, 76.0)
    bgf += (xx / max(1, size - 1) - 0.5)[..., None] * 8.0
    bgf += (yy / max(1, size - 1) - 0.5)[..., None] * 5.0
    bgf += rng.normal(0.0, 2.0, (size, size, 1)).astype(np.float32)
    bg = np.clip(np.rint(np.clip(bgf, 0.0, 255.0)), 0, 255).astype(np.uint8)

    fg = make_foreground_texture(size, rng)
    return Fixture("green_screen", composite(fg, bg, alpha), alpha, fg, bg)


def fixture_photo_composite(rng: np.random.Generator) -> Fixture:
    size = PHOTO_SIZE
    fg_path = PHOTO_ASSETS / PHOTO_FG[0]
    bg_path = PHOTO_ASSETS / PHOTO_FG[1]
    if not fg_path.is_file() or not bg_path.is_file():
        raise FileNotFoundError(f"missing CC0 photo assets under {PHOTO_ASSETS}")

    fg_src = cv2.cvtColor(cv2.imread(str(fg_path), cv2.IMREAD_COLOR), cv2.COLOR_BGR2RGB)
    bg_src = cv2.cvtColor(cv2.imread(str(bg_path), cv2.IMREAD_COLOR), cv2.COLOR_BGR2RGB)
    h, w = fg_src.shape[:2]
    y0, x0 = (h - size) // 2, (w - size) // 2
    fg = np.ascontiguousarray(fg_src[y0 : y0 + size, x0 : x0 + size])
    bg = cv2.resize(bg_src, (size, size), interpolation=cv2.INTER_AREA)

    coarse = smooth_noise(size, rng, 7, size / 26.0)
    fine = smooth_noise(size, rng, 128, 1.4)
    alpha = np.clip((coarse * 0.9 + fine * 0.35 + 0.10) / 0.55, 0.0, 1.0)
    alpha = cv2.GaussianBlur(alpha.astype(np.float32), (0, 0), 1.2)
    gt = np.clip(alpha, 0.0, 1.0).astype(np.float32)
    return Fixture("photo_composite", composite(fg, bg, gt), gt, fg, bg)


BUILDERS = {
    "disc_aa": fixture_disc_aa,
    "hair_strands": fixture_hair_strands,
    "glass_panel": fixture_glass_panel,
    "thin_wire": fixture_thin_wire,
    "fur_edge": fixture_fur_edge,
    "soft_shadow": fixture_soft_shadow,
    "green_screen": fixture_green_screen,
    "photo_composite": fixture_photo_composite,
}


def build_fixtures(keys: list[str]) -> list[Fixture]:
    rng = np.random.default_rng(SEED)
    fixtures: list[Fixture] = []
    for key in FIXTURE_ORDER:
        if key not in keys:
            continue
        # Deterministic per fixture: reseed so a subset run matches a full run.
        local = np.random.default_rng(SEED + FIXTURE_ORDER.index(key) * 7919)
        fixtures.append(BUILDERS[key](local))
    del rng
    return fixtures


# --------------------------------------------------------------------------- classical candidates


def _trimap_from_gt(alpha: np.ndarray, radius: int = 3) -> np.ndarray:
    core = (alpha > 0.5).astype(np.uint8)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (radius * 2 + 1, radius * 2 + 1))
    inner = cv2.erode(core, kernel)
    outer = cv2.dilate(core, kernel)
    trimap = np.full(alpha.shape, cv2.GC_BGD, np.uint8)
    trimap[outer > 0] = cv2.GC_PR_FGD
    trimap[inner > 0] = cv2.GC_FGD
    return trimap


def grabcut_rect(image: np.ndarray, gt: np.ndarray) -> np.ndarray:
    bgr = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
    h, w = bgr.shape[:2]
    inset = max(2, int(round(0.02 * min(h, w))))
    rect = (inset, inset, w - 2 * inset, h - 2 * inset)
    mask = np.full((h, w), cv2.GC_PR_BGD, np.uint8)
    bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    # GrabCut's GMM init uses OpenCV's RNG; reseed per call so run order cannot change results.
    cv2.setRNGSeed(SEED)
    cv2.grabCut(bgr, mask, rect, bgd, fgd, GRABCUT_ITERATIONS, cv2.GC_INIT_WITH_RECT)
    return np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 1.0, 0.0).astype(np.float32)


def grabcut_gt_trimap(image: np.ndarray, gt: np.ndarray) -> np.ndarray:
    bgr = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
    mask = _trimap_from_gt(gt)
    bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    cv2.setRNGSeed(SEED)
    cv2.grabCut(bgr, mask, None, bgd, fgd, GRABCUT_ITERATIONS, cv2.GC_INIT_WITH_MASK)
    return np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 1.0, 0.0).astype(np.float32)


def chroma_key(image: np.ndarray, gt: np.ndarray) -> np.ndarray:
    """Robust chroma-distance keyer; the key colour is estimated from the image border.

    Uses YCrCb chroma so uneven luma (vignetting, ramps) does not move the key. On the
    textured fixtures there is no single background colour, so this candidate is expected to
    return little or nothing for those; `green_screen` is the fixture where it should work.
    """
    del gt
    chroma = cv2.cvtColor(image, cv2.COLOR_RGB2YCrCb).astype(np.float32)[..., 1:]
    border = np.concatenate(
        [
            chroma[:8].reshape(-1, 2),
            chroma[-8:].reshape(-1, 2),
            chroma[:, :8].reshape(-1, 2),
            chroma[:, -8:].reshape(-1, 2),
        ]
    )
    key = np.median(border, axis=0)
    dev = np.linalg.norm(border - key, axis=-1)
    mad = float(np.median(np.abs(dev - np.median(dev)))) * 1.4826
    spread = max(1.5, mad)
    t0 = float(np.median(dev)) + 3.0 * spread
    t1 = t0 + 3.0 * spread
    dist = np.linalg.norm(chroma - key, axis=-1)
    return np.clip((dist - t0) / (t1 - t0), 0.0, 1.0).astype(np.float32)


def otsu_luma(image: np.ndarray, gt: np.ndarray) -> np.ndarray:
    del gt
    luma = cv2.cvtColor(image, cv2.COLOR_RGB2GRAY)
    _, binary = cv2.threshold(luma, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    border_luma = float(np.median(np.concatenate([luma[:8].ravel(), luma[-8:].ravel(), luma[:, :8].ravel(), luma[:, -8:].ravel()])))
    fg_mean = float(np.mean(luma[binary > 0])) if np.any(binary > 0) else 255.0
    bg_mean = float(np.mean(luma[binary == 0])) if np.any(binary == 0) else 0.0
    take_high = abs(fg_mean - border_luma) > abs(bg_mean - border_luma)
    mask = (binary > 0) if take_high else (binary == 0)
    return mask.astype(np.float32)


CLASSICAL = {
    "grabcut_rect": grabcut_rect,
    "grabcut_gt_trimap": grabcut_gt_trimap,
    "chroma_key": chroma_key,
    "otsu_luma": otsu_luma,
}

# Per-model input contracts. min-max normalisation is the default; these are the published
# pre/post-processing contracts of the candidates (see MODELS.md).
ONNX_PRESETS = {
    "u2net": {
        "name": "u2net",
        "input_size": (320, 320),
        "mean": (0.485, 0.456, 0.406),
        "std": (0.229, 0.224, 0.225),
        "output_index": 0,
        "resize": "cv2.INTER_AREA on 8-bit RGB, then /255 then ImageNet mean/std",
        "output": "d0 (fused side output), sigmoid applied in-model, upsampled bilinear to input size",
    },
    "modnet": {
        "name": "modnet",
        "input_size": (512, 512),
        "mean": (0.5, 0.5, 0.5),
        "std": (0.5, 0.5, 0.5),
        "output_index": 0,
        "resize": "cv2.INTER_AREA on 8-bit RGB, then /255 then (x-0.5)/0.5",
        "output": "single-channel alpha, sigmoid applied in-model, upsampled bilinear to input size",
    },
    "birefnet": {
        "name": "birefnet",
        "input_size": (512, 512),
        "mean": (0.485, 0.456, 0.406),
        "std": (0.229, 0.224, 0.225),
        "output_index": 0,
        "resize": "cv2.INTER_AREA on 8-bit RGB, then /255 then ImageNet mean/std",
        "output": "single-channel sigmoid prediction; the fused output is the largest/highest-resolution one",
    },
}


class OnnxMatteModel:
    """Minimal ONNX alpha-matte runner (CPU provider, single thread for determinism)."""

    def __init__(self, name: str, path: Path, preset: dict | None = None) -> None:
        import onnxruntime as ort

        options = ort.SessionOptions()
        options.intra_op_num_threads = 1
        options.inter_op_num_threads = 1
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        self.name = name
        self.path = str(path)
        self.preset_name = (preset or {}).get("name")
        self.session = ort.InferenceSession(str(path), options, providers=["CPUExecutionProvider"])
        self.input = self.session.get_inputs()[0]
        self.output_index = int((preset or {}).get("output_index", 0))
        self.output = self.session.get_outputs()[self.output_index]
        shape = self.input.shape
        self.channels = shape[1] if len(shape) == 4 else 3
        self.fixed_hw = (
            (int(shape[2]), int(shape[3]))
            if len(shape) == 4 and isinstance(shape[2], int) and isinstance(shape[3], int)
            else None
        )

    def _prepare(self, rgb: np.ndarray, preset: dict) -> np.ndarray:
        x = rgb.astype(np.float32)
        # Resize in 8-bit space first: scaling before a 320x320 downscale loses edge detail.
        size = preset.get("input_size") or self.fixed_hw
        if size:
            x = cv2.resize(x, (size[1], size[0]), interpolation=cv2.INTER_AREA)
        x = x / 255.0
        if self.channels == 1:
            x = cv2.cvtColor(x, cv2.COLOR_RGB2GRAY)[..., None]
        else:
            mean = np.array(preset.get("mean", (0.0, 0.0, 0.0)), np.float32)
            std = np.array(preset.get("std", (1.0, 1.0, 1.0)), np.float32)
            x = (x - mean) / std
        return np.ascontiguousarray(np.transpose(x, (2, 0, 1))[None])

    def __call__(self, image: np.ndarray, gt: np.ndarray | None = None) -> np.ndarray:
        del gt
        h, w = image.shape[:2]
        preset = ONNX_PRESETS.get(self.preset_name or "", {})
        raw = np.asarray(
            self.session.run([self.output.name], {self.input.name: self._prepare(image, preset)})[
                self.output_index
            ]
        )
        a = np.squeeze(raw)
        if a.ndim == 3:
            a = a[1] if a.shape[0] > 1 else a[0]
        # Softmax-style two-channel output => foreground probability is channel 1.
        if float(np.min(a)) < 0.0 or float(np.max(a)) > 1.0:
            a = 1.0 / (1.0 + np.exp(-a.astype(np.float64)))
        a = a.astype(np.float32)
        if a.shape[:2] != (h, w):
            a = cv2.resize(a, (w, h), interpolation=cv2.INTER_LINEAR)
        return np.clip(a, 0.0, 1.0)


GUIDED_RADIUS = 2
GUIDED_EPS = 1e-4


def guided_filter(guide: np.ndarray, src: np.ndarray, radius: int = GUIDED_RADIUS, eps: float = GUIDED_EPS) -> np.ndarray:
    """Edge-aware guided filter (He et al.) on 0..1 values.

    The image is the guide and the model's alpha is the input; the result is a soft alpha that
    follows the image edges instead of the model's coarse mask.
    """
    window = (radius * 2 + 1, radius * 2 + 1)
    g = np.ascontiguousarray(guide.astype(np.float32))
    s = np.ascontiguousarray(src.astype(np.float32))
    mean_g = cv2.boxFilter(g, -1, window)
    mean_s = cv2.boxFilter(s, -1, window)
    cov_gs = cv2.boxFilter(g * s[..., None], -1, window) - mean_g * mean_s[..., None]
    var_g = cv2.boxFilter(g * g, -1, window) - mean_g * mean_g
    a = cov_gs / (var_g + eps)
    b = mean_s[..., None] - np.sum(a * mean_g, axis=-1, keepdims=True)
    mean_a = cv2.boxFilter(a, -1, window)
    mean_b = cv2.boxFilter(b[..., 0], -1, window)
    q = np.sum(mean_a * g, axis=-1) + mean_b
    return np.clip(q, 0.0, 1.0).astype(np.float32)


class RefinedAlpha:
    """Classical edge-aware refinement of a model's alpha (rd5).

    Hypothesis: a coarse saliency mask has the right regions but the wrong edges; an edge-aware
    filter can recover the partial alpha without a new model.
    """

    def __init__(self, name: str, model: OnnxMatteModel, radius: int = GUIDED_RADIUS, eps: float = GUIDED_EPS) -> None:
        self.name = name
        self.model = model
        self.radius = radius
        self.eps = eps
        self.derived_from = model.name

    def __call__(self, image: np.ndarray, gt: np.ndarray | None = None) -> np.ndarray:
        del gt
        alpha = self.model(image)
        return guided_filter(image, alpha, self.radius, self.eps)


SURE_BG_ALPHA = 0.10
SURE_FG_ALPHA = 0.90
TRIMAP_KERNEL_RADIUS = 2


class SaliencyTrimapGrabcut:
    """Use a saliency model as a GrabCut trimap, then optionally keep its soft band alpha.

    Hypothesis (rd2): the model's large-scale segmentation limits GrabCut's rectangle blow-up,
    and GrabCut's colour model corrects the model's over/under-segmentation. The `soft` variant
    keeps the model's alpha only inside the unknown band, so the output is still a matte rather
    than a binary mask.
    """

    def __init__(self, name: str, model: OnnxMatteModel, soft: bool) -> None:
        self.name = name
        self.model = model
        self.soft = soft
        self.derived_from = model.name

    def _trimap(self, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        kernel = cv2.getStructuringElement(
            cv2.MORPH_ELLIPSE, (TRIMAP_KERNEL_RADIUS * 2 + 1, TRIMAP_KERNEL_RADIUS * 2 + 1)
        )
        sure_fg = cv2.erode((alpha >= SURE_FG_ALPHA).astype(np.uint8), kernel)
        sure_bg = cv2.dilate((alpha <= SURE_BG_ALPHA).astype(np.uint8), kernel)
        trimap = np.full(alpha.shape, cv2.GC_PR_FGD, np.uint8)
        trimap[sure_bg > 0] = cv2.GC_BGD
        trimap[sure_fg > 0] = cv2.GC_FGD
        return trimap, sure_fg, sure_bg

    def __call__(self, image: np.ndarray, gt: np.ndarray | None = None) -> np.ndarray:
        del gt
        alpha = self.model(image)
        trimap, sure_fg, sure_bg = self._trimap(alpha)
        mask = trimap.copy()
        bgd, fgd = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
        bgr = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
        cv2.setRNGSeed(SEED)
        cv2.grabCut(bgr, mask, None, bgd, fgd, GRABCUT_ITERATIONS, cv2.GC_INIT_WITH_MASK)
        binary = ((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD)).astype(np.float32)
        band = (sure_fg == 0) & (sure_bg == 0)
        self.stats = {
            "sure_fg_frac": float(np.mean(sure_fg > 0)),
            "sure_bg_frac": float(np.mean(sure_bg > 0)),
            "unknown_frac": float(np.mean(band)),
            "model_partial_frac": float(np.mean((alpha > SURE_BG_ALPHA) & (alpha < SURE_FG_ALPHA))),
            "degenerate_no_sure_fg": bool(not np.any(sure_fg)),
        }
        if not self.soft:
            return binary
        out = binary.copy()
        out[band] = alpha[band]
        return np.clip(out, 0.0, 1.0)


# --------------------------------------------------------------------------- metrics


def _band_mask(gt: np.ndarray, radius: int = 5) -> np.ndarray:
    partial = ((gt > 0.0) & (gt < 1.0)).astype(np.uint8)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (radius * 2 + 1, radius * 2 + 1))
    return cv2.dilate(partial, kernel) > 0


def sad(pred: np.ndarray, gt: np.ndarray) -> float:
    """Sum of absolute alpha error / 1000 (alpha matting benchmark convention)."""
    return float(np.sum(np.abs(pred - gt)) / 1000.0)


def mse(pred: np.ndarray, gt: np.ndarray) -> float:
    """Mean squared alpha error x 1000."""
    return float(np.mean((pred - gt) ** 2) * 1000.0)


def gradient_error(pred: np.ndarray, gt: np.ndarray) -> float:
    """Sum of |grad(pred) - grad(gt)| / 1000 using normalised Scharr derivatives."""

    def grad(a: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        gx = cv2.Scharr(a.astype(np.float32), cv2.CV_32F, 1, 0) / 32.0
        gy = cv2.Scharr(a.astype(np.float32), cv2.CV_32F, 0, 1) / 32.0
        return gx, gy

    px, py = grad(pred)
    gx, gy = grad(gt)
    return float(np.sum(np.hypot(px - gx, py - gy)) / 1000.0)


def connectivity_error(pred: np.ndarray, gt: np.ndarray, image: np.ndarray) -> float:
    """Rhemann connectivity: 4-neighbour difference of alpha error, colour-similarity weighted."""
    diff = pred - gt
    img = image.astype(np.float32)
    total = 0.0
    for axis in (0, 1):
        d_a = np.abs(np.diff(diff, axis=axis))
        d_i = np.linalg.norm(np.diff(img, axis=axis), axis=-1)
        weight = np.minimum(1.0, 1.0 / (d_i + 1e-3))
        total += float(np.sum(d_a * weight))
    return total / 1000.0


def composite_error(
    pred: np.ndarray, gt: np.ndarray, fixture: Fixture, band: np.ndarray
) -> dict[str, float]:
    """Recomposite with candidate alpha and GT foreground; GT composite is the reference."""
    result: dict[str, float] = {}
    for name, bg in REVIEW_BGS:
        bg_arr = np.array(bg, np.float32)[None, None, :]
        comp_pred = fixture.fg.astype(np.float32) * pred[..., None] + bg_arr * (1.0 - pred[..., None])
        comp_gt = fixture.fg.astype(np.float32) * gt[..., None] + bg_arr * (1.0 - gt[..., None])
        err = np.abs(comp_pred - comp_gt).max(axis=-1)
        result[f"{name}_max"] = float(np.max(err[band])) if np.any(band) else 0.0
        result[f"{name}_mean"] = float(np.mean(err[band])) if np.any(band) else 0.0
    return result


def spill_proxy(pred: np.ndarray, gt: np.ndarray, fixture: Fixture, band: np.ndarray) -> float:
    """Signed leakage along the foreground-minus-background colour direction."""
    if not np.any(band):
        return 0.0
    direction = fixture.fg.astype(np.float32) - fixture.bg.astype(np.float32)
    norm = np.linalg.norm(direction, axis=-1, keepdims=True) + 1e-3
    unit = direction / norm
    err = (pred - gt)[..., None] * direction
    projected = np.sum(err * unit, axis=-1)
    return float(np.mean(projected[band]))


def alpha_stats(alpha: np.ndarray) -> dict[str, float]:
    values = alpha.astype(np.float64).ravel()
    return {
        "min": float(np.min(values)),
        "max": float(np.max(values)),
        "mean": float(np.mean(values)),
        "p5": float(np.percentile(values, 5)),
        "p50": float(np.percentile(values, 50)),
        "p95": float(np.percentile(values, 95)),
        "frac_zero": float(np.mean(values <= 1.0 / 255.0)),
        "frac_one": float(np.mean(values >= 254.0 / 255.0)),
        "frac_partial_5_95": float(np.mean((values > 0.05) & (values < 0.95))),
        "non_finite": int(np.sum(~np.isfinite(values))),
    }


def evaluate(pred: np.ndarray, fixture: Fixture) -> dict:
    gt = fixture.alpha
    band = _band_mask(gt)
    translucent = (gt > 0.05) & (gt < 0.95)
    partial = (gt > 1.0 / 255.0) & (gt < 254.0 / 255.0)
    diff = pred - gt
    row = {
        "sad": sad(pred, gt),
        "mse": mse(pred, gt),
        "gradient": gradient_error(pred, gt),
        "connectivity": connectivity_error(pred, gt, fixture.image),
        "band_mae": float(np.mean(np.abs(diff[band]))) if np.any(band) else 0.0,
        "band_max": float(np.max(np.abs(diff[band]))) if np.any(band) else 0.0,
        "band_frac": float(np.mean(band)),
        "translucent_mae": float(np.mean(np.abs(diff[translucent]))) if np.any(translucent) else None,
        "translucent_frac": float(np.mean(translucent)),
        "partial_recall_0p1": float(np.mean(np.abs(diff[partial]) <= 0.1)) if np.any(partial) else None,
        "partial_frac": float(np.mean(partial)),
        "full_mae": float(np.mean(np.abs(diff))),
        "composite": composite_error(pred, gt, fixture, band),
        "spill_proxy": spill_proxy(pred, gt, fixture, band),
        "pred": alpha_stats(pred),
        "gt": alpha_stats(gt),
    }
    row["band_over_255"] = row["band_mae"] * 255.0
    return row


# --------------------------------------------------------------------------- evidence


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def save_png(path: Path, rgb: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(path), cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))


def save_alpha_png(path: Path, alpha: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    a8 = np.clip(np.rint(alpha * 255.0), 0, 255).astype(np.uint8)
    cv2.imwrite(str(path), a8)


def save_fixtures(out_dir: Path, fixtures: list[Fixture]) -> dict[str, dict]:
    records: dict[str, dict] = {}
    work = out_dir / "fixtures"
    for fixture in fixtures:
        save_png(work / f"{fixture.key}_image.png", fixture.image)
        save_alpha_png(work / f"{fixture.key}_alpha.png", fixture.alpha)
        save_png(work / f"{fixture.key}_fg.png", fixture.fg)
        save_png(work / f"{fixture.key}_bg.png", fixture.bg)
        records[fixture.key] = {
            "size": [int(fixture.image.shape[1]), int(fixture.image.shape[0])],
            "files": {
                name: sha256_of(work / f"{fixture.key}_{name}.png")
                for name in ("image", "alpha", "fg", "bg")
            },
        }
    return records


def save_review(out_dir: Path, fixture: Fixture, name: str, pred: np.ndarray) -> None:
    gt = fixture.alpha
    bg = np.array(REVIEW_BGS[1][1], np.float32)[None, None, :]
    comp_pred = fixture.fg.astype(np.float32) * pred[..., None] + bg * (1.0 - pred[..., None])
    comp_gt = fixture.fg.astype(np.float32) * gt[..., None] + bg * (1.0 - gt[..., None])
    tiles = [
        fixture.image,
        np.repeat(np.rint(gt * 255.0).astype(np.uint8)[..., None], 3, axis=-1),
        np.repeat(np.rint(pred * 255.0).astype(np.uint8)[..., None], 3, axis=-1),
        np.clip(np.rint(comp_pred), 0, 255).astype(np.uint8),
        np.clip(np.rint(comp_gt), 0, 255).astype(np.uint8),
        np.clip(np.rint(255.0 * np.minimum(1.0, np.abs(pred - gt) * 4.0)), 0, 255).astype(np.uint8)[..., None].repeat(3, axis=-1),
    ]
    h, w = tiles[0].shape[:2]
    gap = np.full((h, 4, 3), 255, np.uint8)
    top = np.concatenate([tiles[0], gap, tiles[1], gap, tiles[2]], axis=1)
    bottom = np.concatenate([tiles[3], gap, tiles[4], gap, tiles[5]], axis=1)
    sheet = np.concatenate([top, np.full((4, top.shape[1], 3), 255, np.uint8), bottom], axis=0)
    scale = max(1, 1536 // max(1, sheet.shape[1]))
    if scale > 1:
        sheet = cv2.resize(sheet, (sheet.shape[1] * scale, sheet.shape[0] * scale), interpolation=cv2.INTER_NEAREST)
    save_png(out_dir / "review" / fixture.key / f"{name}.png", sheet)


# --------------------------------------------------------------------------- runner


def validate_alpha(name: str, alpha: np.ndarray) -> np.ndarray:
    if alpha.ndim == 3 and alpha.shape[-1] == 1:
        alpha = alpha[..., 0]
    if alpha.ndim != 2:
        raise ValueError(f"{name}: expected a 2-D alpha mask, got shape {alpha.shape}")
    return np.clip(alpha.astype(np.float32), 0.0, 1.0)


def gate_verdicts(metrics: dict, baseline: dict) -> dict:
    """Apply the pre-registered gates to learned candidates and record pass/fail per fixture."""
    out: dict[str, dict] = {}
    for name in sorted({n for entries in metrics.values() for n in entries}):
        kinds = {entries[name]["kind"] for entries in metrics.values() if name in entries}
        if "learned" not in kinds:
            continue
        fixtures: dict[str, dict] = {}
        failing: dict[str, int] = {}
        for fixture_key, entries in metrics.items():
            entry = entries.get(name)
            ref = baseline.get(fixture_key)
            if entry is None or ref is None:
                continue
            checks: dict[str, dict] = {}

            sad_ratio = entry["sad"] / ref["sad"] if ref["sad"] > 0 else None
            checks["sad_improvement_40pct"] = {
                "value": sad_ratio,
                "threshold": 1.0 - GATES["sad_improvement_vs_grabcut_rect"],
                "pass": sad_ratio is not None and sad_ratio <= 1.0 - GATES["sad_improvement_vs_grabcut_rect"],
            }
            checks["band_mae"] = {
                "value": entry["band_mae"],
                "threshold": GATES["boundary_band_mae"],
                "pass": entry["band_mae"] <= GATES["boundary_band_mae"],
            }
            if entry["translucent_mae"] is not None:
                checks["translucent_mae"] = {
                    "value": entry["translucent_mae"],
                    "threshold": GATES["translucent_mae"],
                    "pass": entry["translucent_mae"] <= GATES["translucent_mae"],
                }
            if fixture_key in SOFT_FIXTURES and entry["partial_recall_0p1"] is not None:
                checks["partial_recall_0p1"] = {
                    "value": entry["partial_recall_0p1"],
                    "threshold": GATES["partial_recall_0p1"],
                    "pass": entry["partial_recall_0p1"] >= GATES["partial_recall_0p1"],
                }
            composite: dict[str, dict] = {}
            for bg in ("gray", "green", "white"):
                mean_delta = entry["composite"][f"{bg}_mean"] - ref["composite"][f"{bg}_mean"]
                ref_max = ref["composite"][f"{bg}_max"]
                max_ratio = entry["composite"][f"{bg}_max"] / ref_max if ref_max > 0 else None
                composite[bg] = {
                    "mean_delta_codes": mean_delta,
                    "mean_delta_threshold": GATES["composite_mean_delta_vs_grabcut_rect_codes"],
                    "max_ratio": max_ratio,
                    "max_ratio_threshold": GATES["composite_max_ratio_vs_grabcut_rect"],
                    "pass": mean_delta <= GATES["composite_mean_delta_vs_grabcut_rect_codes"]
                    and (max_ratio is None or max_ratio <= GATES["composite_max_ratio_vs_grabcut_rect"]),
                }
            checks["composite"] = composite
            checks["gradient"] = {
                "value": entry["gradient"],
                "reference": ref["gradient"],
                "pass": entry["gradient"] < ref["gradient"],
            }
            checks["connectivity"] = {
                "value": entry["connectivity"],
                "reference": ref["connectivity"],
                "pass": entry["connectivity"] <= ref["connectivity"],
            }

            def _pass(value: object) -> bool:
                if isinstance(value, dict):
                    if "pass" in value:
                        return bool(value["pass"])
                    return all(_pass(v) for v in value.values())
                return True

            failed = [key for key, value in checks.items() if not _pass(value)]
            for key in failed:
                failing[key] = failing.get(key, 0) + 1
            fixtures[fixture_key] = {"checks": checks, "failed": failed, "pass": not failed}

        total = len(fixtures)
        passed = sum(1 for v in fixtures.values() if v["pass"])
        out[name] = {
            "fixtures": fixtures,
            "summary": {
                "fixtures_passed": passed,
                "fixtures_total": total,
                "failing_gate_counts": failing,
                "overall": "pass" if total and passed == total else "fail",
                "note": "gate verdicts are mechanical; a pass still requires human visual review of "
                "fabricated detail and does not approve a product feature",
            },
        }
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="Matte Lab rd0 classical baseline harness")
    parser.add_argument("--rd", default="rd0", help="run label, e.g. rd0 / rd1")
    parser.add_argument(
        "--fixtures",
        nargs="+",
        default=list(FIXTURE_ORDER),
        choices=list(FIXTURE_ORDER),
        help="subset of fixtures",
    )
    parser.add_argument(
        "--onnx",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="optional ONNX matte model, repeatable",
    )
    parser.add_argument(
        "--onnx-preset",
        action="append",
        default=[],
        metavar="NAME=PRESET",
        help=f"preprocessing preset per candidate, e.g. u2net=u2net. Known: {sorted(ONNX_PRESETS)}",
    )
    parser.add_argument(
        "--refine",
        action="store_true",
        help="also run <name>_refined (guided-filter alpha refinement) for each ONNX model",
    )
    parser.add_argument("--runs", type=int, default=3, help="timing repetitions per candidate")
    parser.add_argument(
        "--derive-trimap",
        action="store_true",
        help="also run <name>_trimap_grabcut and <name>_trimap_soft derived from each ONNX model",
    )
    args = parser.parse_args()
    args.onnx_preset = dict(spec.split("=", 1) for spec in args.onnx_preset)

    cv2.setRNGSeed(SEED)
    out_dir = ROOT / "captures" / args.rd
    out_dir.mkdir(parents=True, exist_ok=True)
    started = time.time()

    fixtures = build_fixtures(args.fixtures)
    fixture_records = save_fixtures(out_dir, fixtures)

    candidates: dict[str, object] = dict(CLASSICAL)
    onnx_meta: dict[str, dict] = {}
    for spec in args.onnx:
        name, _, path = spec.partition("=")
        model_path = Path(path)
        if not model_path.is_file():
            # Accept paths relative to the lab directory as well as to the current directory.
            alternative = ROOT / path
            if alternative.is_file():
                model_path = alternative
        if not model_path.is_file():
            print(f"[skip] onnx model not found: {model_path}", file=sys.stderr)
            continue
        preset_name = args.onnx_preset.get(name) or name
        preset = ONNX_PRESETS.get(preset_name)
        if preset is None:
            print(f"[warn] no preprocessing preset for {name}; using plain 0..1 min-max", file=sys.stderr)
        candidates[name] = OnnxMatteModel(name, model_path, preset)
        onnx_meta[name] = {
            "path": str(model_path.relative_to(ROOT)) if model_path.is_relative_to(ROOT) else str(model_path),
            "bytes": model_path.stat().st_size,
            "sha256": sha256_of(model_path),
            "preset": preset_name if preset else None,
            "input_name": candidates[name].input.name,
            "input_shape": list(candidates[name].input.shape),
            "output_name": candidates[name].output.name,
            "output_shape": list(candidates[name].output.shape),
        }
        if args.derive_trimap:
            for suffix, soft in (("trimap_grabcut", False), ("trimap_soft", True)):
                derived_name = f"{name}_{suffix}"
                candidates[derived_name] = SaliencyTrimapGrabcut(derived_name, candidates[name], soft)
                onnx_meta[derived_name] = {
                    "derived_from": name,
                    "transform": f"trimap(sure_fg alpha>={SURE_FG_ALPHA} eroded {TRIMAP_KERNEL_RADIUS}px, "
                    f"sure_bg alpha<={SURE_BG_ALPHA} dilated) -> grabCut({GRABCUT_ITERATIONS}) "
                    + ("-> keep model alpha inside the unknown band" if soft else "-> binary"),
                }
        if args.refine:
            refined_name = f"{name}_refined"
            candidates[refined_name] = RefinedAlpha(refined_name, candidates[name])
            onnx_meta[refined_name] = {
                "derived_from": name,
                "transform": f"guided filter (guide=image, radius={GUIDED_RADIUS}, eps={GUIDED_EPS})",
            }

    metrics: dict[str, dict] = {}
    for fixture in fixtures:
        metrics[fixture.key] = {}
        for name, candidate in candidates.items():
            timings: list[float] = []
            alpha = np.zeros_like(fixture.alpha)
            for _ in range(max(1, args.runs)):
                begin = time.perf_counter()
                alpha = validate_alpha(name, candidate(fixture.image, fixture.alpha))
                timings.append(time.perf_counter() - begin)
            entry = {
                "kind": "oracle" if name in ORACLE_CANDIDATES else ("classical" if name in CLASSICAL else "learned"),
                "ms_median": statistics.median(timings) * 1000.0,
                "derived_from": getattr(candidate, "derived_from", None),
                "trimap_stats": getattr(candidate, "stats", None),
            }
            entry.update(evaluate(alpha, fixture))
            metrics[fixture.key][name] = entry
            save_review(out_dir, fixture, name, alpha)

    baseline = {}
    for fixture_key, entries in metrics.items():
        reference = entries.get("grabcut_rect")
        if not reference:
            continue
        baseline[fixture_key] = {
            "sad": reference["sad"],
            "band_mae": reference["band_mae"],
            "translucent_mae": reference["translucent_mae"],
            "gradient": reference["gradient"],
            "connectivity": reference["connectivity"],
            "softness_partial_frac": reference["pred"]["frac_partial_5_95"],
            "partial_recall_0p1": reference["partial_recall_0p1"],
            "gt_partial_frac": reference["gt"]["frac_partial_5_95"],
            "composite": reference["composite"],
        }

    soft_baseline = [baseline[k]["softness_partial_frac"] for k in SOFT_FIXTURES if k in baseline]
    gate_revision = [
        {
            "gate": "softness",
            "previous": {"name": "softness_ratio_vs_grabcut_rect", "threshold": 5.0},
            "replaced_by": {
                "name": "partial_recall_0p1",
                "threshold": GATES["partial_recall_0p1"],
                "definition": "fraction of GT pixels with 1/255 < alpha < 254/255 predicted within +/-0.1",
                "fixtures": list(SOFT_FIXTURES),
            },
            "measured_baseline_partial_frac": soft_baseline,
            "measured_baseline_partial_recall": [
                baseline[k]["partial_recall_0p1"] for k in SOFT_FIXTURES if k in baseline
            ],
            "reason": "a binary mask has ~0 partial alpha, so the 5x ratio is undefined (division by "
            "near-zero). Replaced by an absolute recall condition on the soft-edge fixtures.",
            "decided": "2026-11-15",
            "decided_by": "user approval (relative/absolute gate revision)",
        },
        {
            "gate": "composite_error",
            "previous": {"name": "composite_absolute_codes", "threshold": None},
            "replaced_by": {
                "name": "composite_mean_delta_vs_grabcut_rect_codes",
                "threshold": GATES["composite_mean_delta_vs_grabcut_rect_codes"],
                "secondary": {
                    "name": "composite_max_ratio_vs_grabcut_rect",
                    "threshold": GATES["composite_max_ratio_vs_grabcut_rect"],
                },
                "definition": "band composite error per background (gray/green/white); candidate band mean "
                "may be at most +1.0 code vs grabcut_rect and band max at most 1.25x",
            },
            "measured_baseline_composite": {
                k: v["composite"] for k, v in baseline.items()
            },
            "reason": "no absolute composite value had a measured basis; upstream alpha error dominates, so "
            "the realistic contract is 'no worse than the binary baseline' per background.",
            "decided": "2026-11-15",
            "decided_by": "user approval (relative/absolute gate revision)",
        },
        {
            "gate": "sad_improvement_vs_grabcut_rect",
            "registered": GATES["sad_improvement_vs_grabcut_rect"],
            "note": "grabcut_rect is a binary baseline, so part of any SAD gain is the binary/soft "
            "difference itself, not matte accuracy. Report both SAD and band MAE before claiming a win.",
        },
    ]
    gates = {
        "thresholds": GATES,
        "applies_to": "learned candidates; classical baselines are reference only",
        "reference_candidate": "grabcut_rect",
        "rd0_baseline": baseline,
        "gate_revision": gate_revision,
        "verdicts": gate_verdicts(metrics, baseline),
        "note": "thresholds fixed before rd1; baseline recorded for comparison only",
    }

    manifest = {
        "rd": args.rd,
        "seed": SEED,
        "created": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "fixtures": fixture_records,
        "candidates": sorted(candidates),
        "onnx_models": onnx_meta,
        "settings": {
            "grabcut_iterations": GRABCUT_ITERATIONS,
            "grabcut_rect_inset_frac": 0.02,
            "grabcut_trimap_radius_px": 3,
            "chroma_key_border_px": 8,
            "band_radius_px": 5,
            "alpha_space": "straight (unassociated), 0..1 float32, 8-bit sRGB composite",
            "timing_runs": max(1, args.runs),
            "source_photo_assets": list(PHOTO_FG),
        },
        "metric_definitions": {
            "sad": "sum(|pred-gt|)/1000",
            "mse": "mean((pred-gt)^2)*1000",
            "gradient": "sum(|grad(pred)-grad(gt)|)/1000, Scharr/32",
            "connectivity": "sum over 4-neighbour of |d_alpha| * min(1, 1/(|d_colour|+1e-3)) /1000",
            "band_mae": "mean |pred-gt| over dilate(0<gt<1, 5px)",
            "translucent_mae": "mean |pred-gt| over 0.05<gt<0.95",
            "softness": "fraction of pixels with 0.05<pred<0.95",
            "partial_recall_0p1": "fraction of GT partial pixels (1/255<gt<254/255) with |pred-gt|<=0.1",
            "composite": "max/mean per-channel error of fg_pred vs fg_gt over gray/green/white, in 8-bit codes",
        },
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

    print(f"rd={args.rd} fixtures={len(fixtures)} candidates={len(candidates)} out={out_dir}")
    print(f"{'fixture':<16}{'candidate':<20}{'SAD':>9}{'bandMAE':>9}{'trans':>7}{'soft':>7}{'ms':>9}")
    for fixture_key, entries in metrics.items():
        for name, entry in entries.items():
            trans = entry["translucent_mae"]
            print(
                f"{fixture_key:<16}{name:<20}{entry['sad']:>9.2f}{entry['band_mae']:>9.4f}"
                f"{('n/a' if trans is None else f'{trans:.3f}'):>7}"
                f"{entry['pred']['frac_partial_5_95']:>7.3f}{entry['ms_median']:>9.1f}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
