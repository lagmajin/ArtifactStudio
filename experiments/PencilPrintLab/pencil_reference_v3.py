"""RD3 reference model: HDR-preserving bounded quantization.

RD2 removed near-transparent >1 recovered RGB by clamping straight color to
[0, 1], which destroyed HDR highlights (opaque max v1/v2 delta up to 1.47).

v3 keeps highlights by:
  * deriving the quantization upper bound from the data: upper = max(1, max(straight));
  * quantizing straight RGB over [0, upper] instead of [0, 1];
  * capping each quantized channel at max(1, its pre-quantization value), so a
    pixel whose source straight color is <= 1 can never be pushed above 1,
    while pixels that were already > 1 keep their HDR range;
  * leaving alpha unchanged, so the final unpremultiply is an exact inverse of
    the premultiply and cannot expose hidden RGB from transparent neighbors.

The line gate is still computed from premul+alpha; every color change after
that is pointwise, so straight and premultiplied inputs agree.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8
from pencil_reference import BAYER4
from pencil_reference_v2 import _grain, _line_gate


def _bounded_texture(straight: np.ndarray, texture: str | None, seed: int, upper: float) -> np.ndarray:
    if texture == "grain":
        n = _grain(straight.shape[0], straight.shape[1], seed)
        return np.clip(straight * (1.0 + 0.08 * n)[..., None], 0.0, upper)
    if texture == "hatch":
        h, w = straight.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        angle = 0.6 + 0.2 * (seed % 3)
        phase = xx * np.cos(angle) + yy * np.sin(angle)
        pattern = 0.5 + 0.5 * np.cos(phase * (2.0 * np.pi / max(4.0, w / 48.0)))
        return np.clip(straight * (0.78 + 0.22 * pattern)[..., None], 0.0, upper)
    return straight


def _bounded_tone(straight: np.ndarray, tone: str | None, levels: int, upper: float) -> np.ndarray:
    before = straight.copy()
    if tone == "posterize":
        q = np.round(np.clip(straight, 0.0, upper) * (levels - 1) / upper) * upper / (levels - 1)
    elif tone == "bayer":
        h, w = straight.shape[:2]
        threshold = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
        q = (straight >= threshold[..., None]).astype(np.float32) * upper
    else:
        return straight
    # A source straight channel <= 1 stays <= 1; HDR channels may keep > 1.
    cap = np.maximum(1.0, before)
    return np.minimum(q, cap)


def apply_candidate_v3(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a pencil/print candidate with HDR-preserving bounded quantization."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = src[..., :3] * alpha if descriptor == "straight" else np.maximum(src[..., :3], 0.0)
    original_straight = premul / np.maximum(alpha, 1e-6)

    line = spec.get("line")
    gate = np.zeros_like(premul)
    if line is not None:
        if line not in ("sobel", "alpha_edge"):
            raise ValueError(f"unknown line component: {line}")
        gate = _line_gate(premul, alpha, line)

    upper = float(max(1.0, float(original_straight.max())))
    straight = _bounded_texture(original_straight, spec.get("texture"), seed, upper)
    straight = straight * (1.0 - float(spec.get("line_strength", 0.8)) * gate)
    straight = _bounded_tone(straight, spec.get("tone"), int(spec.get("tone_levels", 6)), upper)
    straight = np.clip(straight, 0.0, upper)

    strength = float(spec.get("strength", 1.0))
    straight = original_straight + strength * (straight - original_straight)
    straight = np.clip(straight, 0.0, upper)
    alpha = np.clip(alpha, 0.0, 1.0)
    premul = straight * alpha

    out = np.empty_like(src)
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = alpha
    return out
