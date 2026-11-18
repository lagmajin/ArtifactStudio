"""RD8 reference model: sub-pixel phase interpolation for content-locked dither.

RD7 locked the Bayer threshold to content coordinates but truncated the
offset to int32, so 0.5px shifts (odd frames) lost their fractional part and
left an integer-alignment MAE around 0.13.

v6 samples the threshold at the two neighboring integer phases and blends
them by the fractional part of the offset:

    threshold(x, y) = lerp(B[p0], B[p1], frac)

where p0 = floor(offset), p1 = p0 + 1 and frac = offset - p0. For an offset of
exactly 0.5 this yields the average of the two adjacent Bayer phases, which
moves continuously with sub-pixel content motion.

A `nearest` mode reproduces RD7's truncation as a control. Everything else
(line gate, HDR cap, alpha policy) matches v5.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8
from pencil_reference import BAYER4
from pencil_reference_v2 import _grain, _line_gate
from pencil_reference_v4 import _bounded_texture


def _offsets(h: int, w: int, mode: str, manual: tuple[float, float] | None) -> np.ndarray:
    out = np.zeros((h, w, 2), dtype=np.float32)
    if mode == "screen":
        return out
    if manual is None:
        ox, oy = 0.0, 0.0
    else:
        ox, oy = float(manual[0]), float(manual[1])
    out[..., 0] = ox
    out[..., 1] = oy
    return out


def _phase(xx: np.ndarray, yy: np.ndarray, offset_x: np.ndarray, offset_y: np.ndarray, mode: str):
    if mode == "nearest":
        sx = ((xx - offset_x.astype(np.int32)) % 4).astype(np.int32)
        sy = ((yy - offset_y.astype(np.int32)) % 4).astype(np.int32)
        return BAYER4[sy, sx], 1.0
    fx = xx - offset_x
    fy = yy - offset_y
    ix, iy = np.floor(fx).astype(np.int32), np.floor(fy).astype(np.int32)
    tx, ty = fx - ix, fy - iy
    bx, by = (ix + 1) % 4, (iy + 1) % 4
    ix, iy = ix % 4, iy % 4
    b00 = BAYER4[iy, ix]
    b10 = BAYER4[iy, bx]
    b01 = BAYER4[by, ix]
    b11 = BAYER4[by, bx]
    # Bilinear blend of four neighboring phases; for fractional y = 0 and
    # fractional x = 0.5 this is exactly the mean of adjacent x phases.
    lower = b00 * (1.0 - tx) + b10 * tx
    upper = b01 * (1.0 - tx) + b11 * tx
    return lower * (1.0 - ty) + upper * ty, 1.0


def _bounded_tone(straight: np.ndarray, tone: str | None, levels: int, upper: float,
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


def apply_candidate_v6(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Pencil/print candidate with sub-pixel content-locked dither."""
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

    h, w = straight.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    off = _offsets(h, w, spec.get("dither_lock", "screen"), spec.get("dither_offset"))
    threshold, _ = _phase(xx, yy, off[..., 0], off[..., 1], spec.get("phase_mode", "interp"))
    straight = _bounded_tone(straight, spec.get("tone"), int(spec.get("tone_levels", 6)), upper,
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
