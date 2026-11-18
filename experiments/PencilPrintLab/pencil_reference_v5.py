"""RD7 reference model: content-locked ordered dither.

RD6 showed screen-locked Bayer patterns leave a large MAE after warping frame 0
by the known 0.5*k px translation, i.e. the dither pattern does not move with
the image. v5 locks the dither grid to content coordinates by sampling the
threshold at (x - offset_x, y - offset_y), where the offset is derived from the
content position. For the RD6 synthetic sequence the exact shift is known, so
this is a controlled study of whether content-locking removes the alignment
residual; it is not a motion-estimation implementation.

A `global_offset` mode keeps the same pipeline with a constant offset for a
screen-locked control. All other components match v4, including the RD3
per-pixel HDR cap.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8
from pencil_reference import BAYER4
from pencil_reference_v2 import _grain, _line_gate
from pencil_reference_v4 import _bounded_texture


def _content_offset(straight: np.ndarray, alpha: np.ndarray, mode: str, manual: tuple[float, float] | None) -> np.ndarray:
    h, w = straight.shape[:2]
    if mode == "screen":
        return np.zeros((h, w, 2), dtype=np.float32)
    if mode == "global":
        ox, oy = (0.0, 0.0) if manual is None else (float(manual[0]), float(manual[1]))
        out = np.zeros((h, w, 2), dtype=np.float32)
        out[..., 0] = ox
        out[..., 1] = oy
        return out
    if mode == "content":
        ox, oy = (0.0, 0.0) if manual is None else (float(manual[0]), float(manual[1]))
        out = np.zeros((h, w, 2), dtype=np.float32)
        out[..., 0] = ox
        out[..., 1] = oy
        return out
    raise ValueError(f"unknown dither lock mode: {mode}")


def _locked_threshold(h: int, w: int, offset: np.ndarray) -> np.ndarray:
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    sx = (xx - offset[..., 0]).astype(np.int32) % 4
    sy = (yy - offset[..., 1]).astype(np.int32) % 4
    return BAYER4[sy, sx]


def _bounded_tone_locked(straight: np.ndarray, tone: str | None, levels: int, upper: float,
                         dither_levels: int, threshold: np.ndarray) -> np.ndarray:
    before = straight.copy()
    if tone == "posterize":
        q = np.round(np.clip(straight, 0.0, upper) * (levels - 1) / upper) * upper / (levels - 1)
    elif tone == "bayer":
        q = (straight >= threshold[..., None]).astype(np.float32) * upper
    elif tone == "bayer_multi":
        steps = max(2, int(dither_levels)) - 1
        scaled = np.clip(straight, 0.0, upper) * steps / upper
        base = np.floor(scaled)
        q = (base + (scaled - base >= threshold[..., None]).astype(np.float32)) * upper / steps
        q = np.clip(q, 0.0, upper)
    else:
        return straight
    return np.minimum(q, np.maximum(1.0, before))


def apply_candidate_v5(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Pencil/print candidate with content-locked ordered dither."""
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

    mode = spec.get("dither_lock", "screen")
    manual = spec.get("dither_offset")
    offset = _content_offset(straight, alpha, mode, manual)
    threshold = _locked_threshold(straight.shape[0], straight.shape[1], offset)
    straight = _bounded_tone_locked(straight, spec.get("tone"), int(spec.get("tone_levels", 6)), upper,
                                    int(spec.get("dither_levels", 4)), threshold)
    straight = np.clip(straight, 0.0, upper)

    strength = float(spec.get("strength", 1.0))
    straight = original_straight + strength * (straight - original_straight)
    straight = np.clip(straight, 0.0, upper)
    premul = straight * np.clip(alpha, 0.0, 1.0)

    out = np.empty_like(src)
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = np.clip(alpha, 0.0, 1.0)
    return out
