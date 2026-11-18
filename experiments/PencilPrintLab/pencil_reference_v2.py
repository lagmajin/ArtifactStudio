"""RD2 reference model: descriptor-safe quantization for pencil/print.

RD1 found pp_a (Sobel + posterize + grain) produced recovered straight RGB up
to 1.137 near transparent pixels. Cause: texture modulation was applied AFTER
quantizing premultiplied RGB/alpha, so a coarse alpha step divided a modulated
premul value and exposed >1 straight color.

v2 orders the pipeline as:
  1. line gate from premul+alpha (premul is only scaled down, never moved)
  2. unpremultiply once into straight RGB
  3. texture modulation in straight space, bounded to [0, 1] for SDR fixtures
  4. line darkening in straight space
  5. tone quantization on straight RGB, alpha quantized on the same grid
  6. re-premultiply once

All spatial gates are computed on premul+alpha; all color changes are
pointwise after that, so straight and premultiplied inputs must agree.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8
from pencil_reference import BAYER4


def _line_gate(premul: np.ndarray, alpha: np.ndarray, mode: str) -> np.ndarray:
    if mode == "alpha_edge":
        gx = cv2.Sobel(alpha[..., 0], cv2.CV_32F, 1, 0, ksize=3)
        gy = cv2.Sobel(alpha[..., 0], cv2.CV_32F, 0, 1, ksize=3)
        return np.clip(np.hypot(gx, gy) * 2.0, 0.0, 1.0)[..., None]
    straight = premul / np.maximum(alpha, 1e-3)
    lm = straight[..., 0] * 0.2126 + straight[..., 1] * 0.7152 + straight[..., 2] * 0.0722
    gray = linear_to_srgb_u8(lm).astype(np.float32) / 255.0
    gx = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    return np.clip(np.hypot(gx, gy) * 2.0, 0.0, 1.0)[..., None]


def _grain(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    noise = cv2.resize(rng.random((max(2, h // 16), max(2, w // 16))).astype(np.float32), (w, h),
                       interpolation=cv2.INTER_CUBIC)
    if noise.ndim == 2:
        noise = noise[..., None]
    n = noise[..., 0] - noise[..., 0].mean()
    return n / (n.std() + 1e-6)


def _texture_straight(straight: np.ndarray, texture: str | None, seed: int) -> np.ndarray:
    if texture == "grain":
        n = _grain(straight.shape[0], straight.shape[1], seed)
        return np.clip(straight * (1.0 + 0.08 * n)[..., None], 0.0, 1.0)
    if texture == "hatch":
        h, w = straight.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        angle = 0.6 + 0.2 * (seed % 3)
        phase = xx * np.cos(angle) + yy * np.sin(angle)
        pattern = 0.5 + 0.5 * np.cos(phase * (2.0 * np.pi / max(4.0, w / 48.0)))
        return np.clip(straight * (0.78 + 0.22 * pattern)[..., None], 0.0, 1.0)
    return straight


def _tone_straight(straight: np.ndarray, alpha: np.ndarray, tone: str | None, levels: int) -> tuple[np.ndarray, np.ndarray]:
    if tone == "posterize":
        q = np.round(np.clip(straight, 0.0, 1.0) * (levels - 1)) / (levels - 1)
        qa = np.round(np.clip(alpha, 0.0, 1.0) * (levels - 1)) / (levels - 1)
        return q, qa
    if tone == "bayer":
        h, w = straight.shape[:2]
        threshold = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
        bit = (straight >= threshold[..., None]).astype(np.float32)
        return bit, alpha
    return straight, alpha


def apply_candidate_v2(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a pencil/print candidate with bounded, descriptor-safe quantization."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = src[..., :3] * alpha if descriptor == "straight" else np.maximum(src[..., :3], 0.0)

    original_p = premul.copy()
    original_a = alpha.copy()

    gate = np.zeros_like(premul)
    line = spec.get("line")
    if line is not None:
        gate = _line_gate(premul, alpha, line)
        if line not in ("sobel", "alpha_edge"):
            raise ValueError(f"unknown line component: {line}")

    straight = premul / np.maximum(alpha, 1e-6)
    straight = _texture_straight(straight, spec.get("texture"), seed)
    line_strength = float(spec.get("line_strength", 0.8))
    straight = straight * (1.0 - line_strength * gate)
    straight, alpha = _tone_straight(straight, alpha, spec.get("tone"), int(spec.get("tone_levels", 6)))
    straight = np.clip(straight, 0.0, 1.0)

    strength = float(spec.get("strength", 1.0))
    src_straight = original_p / np.maximum(original_a, 1e-6)
    straight = src_straight + strength * (straight - src_straight)
    alpha = original_a + strength * (alpha - original_a)
    straight = np.clip(straight, 0.0, 1.0)
    alpha = np.clip(alpha, 0.0, 1.0)
    premul = straight * alpha

    out = np.empty_like(src)
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = alpha
    return out
