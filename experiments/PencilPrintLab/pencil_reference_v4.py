"""RD4 reference model: multi-level ordered dithering instead of binary Bayer.

RD3 showed pp_b (Bayer 2-level) kept a v1/v3 max RGB delta of 1.0 because the
ordered dither could only output 0 or upper. v4 adds `bayer_multi`: quantize
straight RGB to N ordered levels using the Bayer matrix as the rounding
threshold, then apply the same per-pixel cap as v3 (a source <= 1 never
becomes > 1, HDR pixels keep their range).

Alpha is unchanged, all color operations are pointwise after the premul line
gate, so straight/premultiplied inputs remain consistent.
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


def _bayer_threshold(h: int, w: int) -> np.ndarray:
    return np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]


def _bounded_tone(straight: np.ndarray, tone: str | None, levels: int, upper: float,
                  dither_levels: int = 4) -> np.ndarray:
    before = straight.copy()
    if tone == "posterize":
        q = np.round(np.clip(straight, 0.0, upper) * (levels - 1) / upper) * upper / (levels - 1)
    elif tone == "bayer":
        threshold = _bayer_threshold(straight.shape[0], straight.shape[1])
        q = (straight >= threshold[..., None]).astype(np.float32) * upper
    elif tone == "bayer_multi":
        threshold = _bayer_threshold(straight.shape[0], straight.shape[1])
        steps = max(2, int(dither_levels)) - 1
        scaled = np.clip(straight, 0.0, upper) * steps / upper
        base = np.floor(scaled)
        q = (base + (scaled - base >= threshold[..., None]).astype(np.float32)) * upper / steps
        q = np.clip(q, 0.0, upper)
    else:
        return straight
    cap = np.maximum(1.0, before)
    return np.minimum(q, cap)


def apply_candidate_v4(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a pencil/print candidate with optional multi-level ordered dither."""
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
    straight = _bounded_tone(straight, spec.get("tone"), int(spec.get("tone_levels", 6)), upper,
                             int(spec.get("dither_levels", 4)))
    straight = np.clip(straight, 0.0, upper)

    strength = float(spec.get("strength", 1.0))
    straight = original_straight + strength * (straight - original_straight)
    straight = np.clip(straight, 0.0, upper)
    premul = straight * np.clip(alpha, 0.0, 1.0)

    out = np.empty_like(src)
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = np.clip(alpha, 0.0, 1.0)
    return out
