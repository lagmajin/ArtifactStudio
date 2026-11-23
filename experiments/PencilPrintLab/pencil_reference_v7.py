"""RD10 reference model: analytic continuous dither threshold fields.

RD7 locked the Bayer threshold to content coordinates but truncated the offset
to int32, so a 0.5 px/frame translation snapped the pattern back to integer
phases. RD8 bilinearly interpolated the 4x4 Bayer table between neighbouring
phases, which keeps the pattern attached to the content but low-passes it, so
the dither contrast (and therefore the dither strength) shrinks with the
fractional part of the offset.

RD10 replaces the lookup table with threshold fields that can be evaluated at
any fractional coordinate:

  * ``bayer``  - the RD8 bilinear-blended 4x4 Bayer table (control),
  * ``ign``    - interleaved gradient noise (Jimenez, SIGGRAPH 2014),
  * ``r2``     - the R2 low-discrepancy sequence (Roberts, 2018).

Both analytic fields are defined for real coordinates, so a content-locked
dither can follow a sub-pixel translation without interpolation or phase
quantization. Line gate, HDR cap, texture and alpha policy match v5/v6.

Offline reference model only; it is not a product implementation.
"""

from __future__ import annotations

import numpy as np

from pencil_reference import BAYER4
from pencil_reference_v2 import _grain, _line_gate
from pencil_reference_v4 import _bounded_texture

IGN_SCALE = 52.9829189
IGN_DOT = (0.06711056, 0.00583715)
R2_DOT = (0.7548776662466927, 0.5698402909980532)


def _bayer_interp(x: np.ndarray, y: np.ndarray) -> np.ndarray:
    """Bilinear blend of the four neighbouring 4x4 Bayer phases (v6 compatible)."""
    fx = np.asarray(x, dtype=np.float64)
    fy = np.asarray(y, dtype=np.float64)
    ix = np.floor(fx).astype(np.int32)
    iy = np.floor(fy).astype(np.int32)
    tx = fx - ix
    ty = fy - iy
    bx = (ix + 1) % 4
    by = (iy + 1) % 4
    ix4 = ix % 4
    iy4 = iy % 4
    b00 = BAYER4[iy4, ix4]
    b10 = BAYER4[iy4, bx]
    b01 = BAYER4[by, ix4]
    b11 = BAYER4[by, bx]
    lower = b00 * (1.0 - tx) + b10 * tx
    upper = b01 * (1.0 - tx) + b11 * tx
    return lower * (1.0 - ty) + upper * ty


def threshold_field(x: np.ndarray, y: np.ndarray, source: str) -> np.ndarray:
    """Evaluate a dither threshold field in [0, 1) at (possibly fractional) coordinates."""
    fx = np.asarray(x, dtype=np.float64)
    fy = np.asarray(y, dtype=np.float64)
    if source == "bayer":
        return _bayer_interp(fx, fy)
    if source == "ign":
        return np.mod(IGN_SCALE * np.mod(IGN_DOT[0] * fx + IGN_DOT[1] * fy, 1.0), 1.0)
    if source == "r2":
        return np.mod(R2_DOT[0] * fx + R2_DOT[1] * fy, 1.0)
    raise ValueError(f"unknown threshold source: {source}")


def _bounded_tone(straight: np.ndarray, tone: str | None, levels: int, upper: float,
                  dither_levels: int, threshold: np.ndarray) -> np.ndarray:
    before = straight.copy()
    if tone == "posterize":
        q = np.round(np.clip(straight, 0.0, upper) * (levels - 1) / upper) * upper / (levels - 1)
    elif tone in ("bayer", "multi"):
        steps = max(2, int(dither_levels)) - 1
        scaled = np.clip(straight, 0.0, upper) * steps / upper
        base = np.floor(scaled)
        q = (base + (scaled - base >= threshold[..., None]).astype(np.float64)) * upper / steps
        q = np.clip(q, 0.0, upper)
    else:
        return straight
    return np.minimum(q, np.maximum(1.0, before))


def apply_candidate_v7(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Pencil/print candidate with an analytic (optionally content-locked) dither."""
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
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    offset = spec.get("dither_offset") or [0.0, 0.0]
    ox, oy = float(offset[0]), float(offset[1])
    if spec.get("dither_lock", "screen") == "content":
        sx, sy = xx - ox, yy - oy
    else:
        sx, sy = xx, yy
    threshold = threshold_field(sx, sy, spec.get("threshold_source", "bayer"))

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
