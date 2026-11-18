"""Offline reference model for watercolor-style conversion (RD1).

Three separable components operate on straight linear RGB:
  bleed: color bleeding near boundaries
  paper: deterministic paper texture
  edge:  contour dissolve / wobble

The caller declares the input alpha descriptor. Premultiplied input is
unpremultiplied before processing and re-premultiplied afterwards, so the
recovered straight color must not depend on alpha.

No learning model, no product code, no build.
"""

from __future__ import annotations

import cv2
import numpy as np


def _value_noise(h: int, w: int, rng: np.random.Generator, cells_x: int, cells_y: int) -> np.ndarray:
    grid = rng.random((cells_y, cells_x)).astype(np.float32)
    up = cv2.resize(grid, (w, h), interpolation=cv2.INTER_CUBIC)
    return (up - up.mean()) / (up.std() + 1e-6)


def _paper_value(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    octaves = []
    for cx, cy, weight in ((8, 6, 0.55), (24, 16, 0.30), (64, 48, 0.15)):
        octaves.append(_value_noise(h, w, rng, cx, cy) * weight)
    return np.sum(octaves, axis=0).astype(np.float32)


def _paper_fibers(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed + 11)
    fine = rng.normal(0.0, 1.0, size=(h, w)).astype(np.float32)
    fibers = cv2.GaussianBlur(fine, (0, 0), sigmaX=8.0, sigmaY=1.0, borderType=cv2.BORDER_REFLECT)
    fibers = (fibers - fibers.mean()) / (fibers.std() + 1e-6)
    grain = _value_noise(h, w, rng, 96, 64)
    return (0.75 * fibers + 0.25 * grain).astype(np.float32)


def _edge_gate(rgb: np.ndarray) -> np.ndarray:
    from generate_fixtures import linear_to_srgb_u8

    gray = cv2.cvtColor(linear_to_srgb_u8(rgb), cv2.COLOR_RGB2GRAY).astype(np.float32) / 255.0
    gx = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    mag = np.hypot(gx, gy)
    return np.clip(mag * 2.5, 0.0, 1.0)


def _bleed_bilateral(rgb: np.ndarray) -> np.ndarray:
    return cv2.bilateralFilter(rgb, d=7, sigmaColor=0.08, sigmaSpace=3.0)


def _bleed_quantize_gauss(rgb: np.ndarray) -> np.ndarray:
    levels = 12
    quantized = np.round(rgb * (levels - 1)) / (levels - 1)
    return cv2.GaussianBlur(quantized, (0, 0), sigmaX=2.5, sigmaY=2.5, borderType=cv2.BORDER_REFLECT)


def _bleed_edge_weighted(rgb: np.ndarray) -> np.ndarray:
    gate = _edge_gate(rgb)
    blurred = cv2.GaussianBlur(rgb, (0, 0), sigmaX=3.0, sigmaY=3.0, borderType=cv2.BORDER_REFLECT)
    weight = (1.0 - gate)[..., None] * 0.75
    return rgb * (1.0 - weight) + blurred * weight


def _edge_softstep(rgb: np.ndarray) -> np.ndarray:
    k = 9
    mean = cv2.blur(rgb, (k, k), borderType=cv2.BORDER_REFLECT)
    sq = cv2.blur(rgb * rgb, (k, k), borderType=cv2.BORDER_REFLECT)
    std = np.sqrt(np.maximum(sq - mean * mean, 0.0)).mean(axis=2, keepdims=True)
    weight = np.clip(std * 6.0, 0.0, 1.0) * 0.45
    return rgb * (1.0 - weight) + mean * weight


def _edge_wobble(rgb: np.ndarray, seed: int) -> np.ndarray:
    h, w = rgb.shape[:2]
    rng = np.random.default_rng(seed + 7)
    n1 = _value_noise(h, w, rng, 12, 10)
    n2 = _value_noise(h, w, rng, 12, 10)
    gate = _edge_gate(rgb)
    amp = 1.4 * (0.35 + 0.65 * gate)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    map_x = (xx + n1 * amp).astype(np.float32)
    map_y = (yy + n2 * amp).astype(np.float32)
    return cv2.remap(rgb, map_x, map_y, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT)


def apply_candidate(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a candidate spec to a float32 RGBA buffer.

    spec keys: bleed, paper, edge, strength. Unknown component names are rejected.
    """
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    out = np.array(rgba, dtype=np.float32, copy=True)
    alpha = out[..., 3:4]
    if descriptor == "premultiplied":
        straight = out[..., :3] / np.maximum(alpha, 1e-6)
    else:
        straight = out[..., :3].copy()

    original = straight.copy()
    bleed = spec.get("bleed")
    if bleed == "bilateral":
        straight = _bleed_bilateral(straight)
    elif bleed == "quantize_gauss":
        straight = _bleed_quantize_gauss(straight)
    elif bleed == "edge_weighted":
        straight = _bleed_edge_weighted(straight)
    elif bleed is not None:
        raise ValueError(f"unknown bleed component: {bleed}")

    paper = spec.get("paper")
    if paper == "value_noise":
        n = _paper_value(straight.shape[0], straight.shape[1], seed)
        straight = straight * (1.0 + 0.06 * n)[..., None]
    elif paper == "fibers":
        n = _paper_fibers(straight.shape[0], straight.shape[1], seed)
        straight = straight * (1.0 + 0.04 * n)[..., None]
    elif paper is not None:
        raise ValueError(f"unknown paper component: {paper}")

    edge = spec.get("edge")
    if edge == "softstep":
        straight = _edge_softstep(straight)
    elif edge == "wobble":
        straight = _edge_wobble(straight, seed)
    elif edge is not None:
        raise ValueError(f"unknown edge component: {edge}")

    strength = float(spec.get("strength", 1.0))
    straight = original + strength * (straight - original)
    straight = np.maximum(straight, 0.0)  # keep HDR > 1; do not clamp to 1

    if descriptor == "premultiplied":
        out[..., :3] = straight * alpha
    else:
        out[..., :3] = straight
    return out
