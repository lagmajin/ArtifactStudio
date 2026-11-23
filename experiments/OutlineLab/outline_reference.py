"""Deterministic reference model for outline extraction experiments.

The model is intentionally small and explicit. It is a research reference, not
an ArtifactStudio or ArtifactCore implementation. Every stage returns float32
arrays and preserves metadata so comparisons can be reproduced.

Phase 1 stages:
- alpha silhouette (coverage boundary)
- alpha detail (soft alpha change away from the silhouette)
- depth discontinuity
- combined confidence
- soft / binary line matte
- composite over selected backgrounds

The line matte uses the silhouette by default (`alpha_detail_weight=0`); the
detail channel is available for comparison and is never mixed in silently.

Normal, color, temporal stabilization, and GPU paths are intentionally not
implemented in Phase 1.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np


Array = np.ndarray


@dataclass(frozen=True)
class OutlineSettings:
    alpha_threshold: float = 0.5
    depth_threshold: float = 0.08
    width_px: float = 2.0
    softness: float = 0.75
    depth_weight: float = 1.0
    alpha_weight: float = 1.0
    alpha_detail_weight: float = 0.0
    detail_gain: float = 4.0
    detail_suppression_px: int = 1
    include_depth: bool = False


def _shift(a: Array, dy: int, dx: int) -> Array:
    """Shift an array with edge replication.

    Positive dy/dx moves content down/right. Negative dy/dx moves content
    up/left. The shifted-in border replicates the nearest source edge.
    """
    out = np.empty_like(a, dtype=np.float32)
    h, w = a.shape
    if dy == 0 and dx == 0:
        out[:] = a
        return out
    src_y0, src_y1 = max(0, -dy), min(h, h - dy)
    src_x0, src_x1 = max(0, -dx), min(w, w - dx)
    dst_y0, dst_y1 = max(0, dy), min(h, h + dy)
    dst_x0, dst_x1 = max(0, dx), min(w, w + dx)
    if dst_y0 >= dst_y1 or dst_x0 >= dst_x1:
        out[:] = a
        return out
    out[...] = 0.0
    out[dst_y0:dst_y1, dst_x0:dst_x1] = a[src_y0:src_y1, src_x0:src_x1]
    if dy > 0:
        out[0:dy, :] = out[dy:dy + 1, :]
    elif dy < 0:
        out[dy:, :] = out[dy - 1:dy, :]
    if dx > 0:
        out[:, 0:dx] = out[:, dx:dx + 1]
    elif dx < 0:
        out[:, dx:] = out[:, dx - 1:dx]
    return out


def _shift_max(a: Array, dy: int, dx: int) -> Array:
    return _shift(a, dy, dx)


def _max_filter(a: Array, radius: int) -> Array:
    """Square max filter using a separable two-pass implementation."""
    if radius <= 0:
        return a.copy()
    horizontal = _max_filter_axis(a, radius, axis=1)
    vertical = _max_filter_axis(horizontal, radius, axis=0)
    return vertical.astype(np.float32)


def _max_filter_axis(a: Array, radius: int, axis: int) -> Array:
    """1D max filter along one axis."""
    if radius <= 0:
        return a.copy()
    out = a.copy()
    for offset in range(1, radius + 1):
        if axis == 1:
            out = np.maximum(out, _shift_max(a, 0, offset))
            out = np.maximum(out, _shift_max(a, 0, -offset))
        else:
            out = np.maximum(out, _shift_max(a, offset, 0))
            out = np.maximum(out, _shift_max(a, -offset, 0))
    return out


def _gradient_magnitude(field: Array) -> Array:
    # Edge-replicated shifts avoid a false gradient at the image border.
    gx = (_shift(field, 0, -1) - _shift(field, 0, 1)) * 0.5
    gy = (_shift(field, -1, 0) - _shift(field, 1, 0)) * 0.5
    return np.sqrt(gx * gx + gy * gy).astype(np.float32)


def silhouette_confidence(rgba: Array, settings: OutlineSettings) -> Array:
    """Coverage boundary only: where alpha crosses the threshold.

    This is the "silhouette" channel. It deliberately ignores alpha variation
    that never crosses the threshold, so a semi-transparent overlay inside an
    opaque shape does not produce an outline.
    """
    alpha = np.clip(rgba[..., 3].astype(np.float32), 0.0, 1.0)
    binary = (alpha >= settings.alpha_threshold).astype(np.float32)
    crossing = _gradient_magnitude(binary)
    return np.clip(crossing * settings.alpha_weight, 0.0, 1.0).astype(np.float32)


def alpha_detail_confidence(rgba: Array, settings: OutlineSettings) -> Array:
    """Alpha gradient away from the silhouette.

    This is the "detail" channel: soft or semi-transparent alpha variation that
    does not change coverage. The silhouette neighbourhood is suppressed so the
    same edge is not counted twice.
    """
    alpha = np.clip(rgba[..., 3].astype(np.float32), 0.0, 1.0)
    grad = _gradient_magnitude(alpha)
    binary = (alpha >= settings.alpha_threshold).astype(np.float32)
    crossing = _gradient_magnitude(binary)
    near_silhouette = _max_filter(crossing, max(int(settings.detail_suppression_px), 0)) > 0.0
    detail = np.clip(grad * settings.detail_gain, 0.0, 1.0)
    detail[near_silhouette] = 0.0
    return detail.astype(np.float32)


def alpha_boundary(rgba: Array, settings: OutlineSettings) -> Array:
    """Combined alpha term: silhouette plus optional detail channel."""
    silhouette = silhouette_confidence(rgba, settings)
    if settings.alpha_detail_weight <= 0.0:
        return silhouette
    detail = alpha_detail_confidence(rgba, settings)
    return np.clip(
        np.maximum(silhouette, detail * settings.alpha_detail_weight), 0.0, 1.0
    ).astype(np.float32)


def depth_discontinuity(depth: Array, settings: OutlineSettings) -> Array:
    """Return a soft depth-edge confidence.

    `depth_threshold` is a gradient threshold in depth units per pixel, using
    the unit documented for the fixture. It is deliberately absolute instead of
    a per-image percentile: percentile normalization would amplify a gentle
    ramp that covers most of the frame into a full-frame outline.
    """
    d = depth.astype(np.float32)
    grad = _gradient_magnitude(d)
    if not np.isfinite(grad).all():
        raise ValueError("depth contains non-finite values")
    threshold = max(float(settings.depth_threshold), 1e-6)
    response = np.clip(grad / threshold, 0.0, 1.0)
    return (response * settings.depth_weight).astype(np.float32)


def combine(alpha_conf: Array, depth_conf: Array | None, settings: OutlineSettings) -> Array:
    total = alpha_conf.copy()
    if settings.include_depth and depth_conf is not None:
        total = np.maximum(total, depth_conf * settings.depth_weight)
    return np.clip(total, 0.0, 1.0).astype(np.float32)


def soft_line(confidence: Array, width_px: float, softness: float) -> Array:
    """Expand confidence into a soft line matte.

    The core radius follows `width_px / 2`. `softness` adds an outer fringe at
    reduced opacity; it must not raise the whole frame to a non-zero matte, so
    background pixels stay at 0 even at maximum softness.
    """
    width = max(float(width_px), 0.0)
    s = float(np.clip(softness, 0.0, 1.0))
    core_radius = max(0, int(np.ceil(width / 2.0)))
    if core_radius == 0:
        return confidence.copy()
    core = _max_filter(confidence, core_radius)
    if s <= 0.0:
        return np.clip(core, 0.0, 1.0).astype(np.float32)
    fringe_radius = core_radius + max(1, int(np.ceil(core_radius * s)))
    fringe = _max_filter(confidence, fringe_radius) * s
    return np.clip(np.maximum(core, fringe), 0.0, 1.0).astype(np.float32)


def composite(rgb: Array, alpha: Array, background: str, line: Array | None = None, line_color: Array | None = None, premultiplied: bool = False) -> Array:
    """Composite RGB over black/gray/white in linear light.

    Straight RGB is multiplied by alpha here. Premultiplied RGB is already
    multiplied and must not be multiplied again.
    """
    bg_values = {"black": 0.0, "gray": 0.18, "white": 1.0}
    if background not in bg_values:
        raise ValueError(f"unsupported background: {background}")
    a = np.clip(alpha.astype(np.float32), 0.0, 1.0)[..., None]
    rgb = rgb.astype(np.float32)
    source = rgb if premultiplied else rgb * a
    out = source + bg_values[background] * (1.0 - a)
    if line is not None:
        if line_color is None:
            line_color = np.array([0.0, 0.0, 0.0], dtype=np.float32)
        la = np.clip(line.astype(np.float32), 0.0, 1.0)[..., None]
        out = np.clip(line_color, 0.0, 1.0) * la + out * (1.0 - la)
    return np.clip(out, 0.0, 1.0).astype(np.float32)


def extract(rgba: Array, depth: Array | None, settings: OutlineSettings) -> dict[str, Any]:
    if rgba.ndim != 3 or rgba.shape[2] != 4:
        raise ValueError("rgba must have shape (height, width, 4)")
    if depth is not None and depth.shape != rgba.shape[:2]:
        raise ValueError("depth must have shape (height, width)")
    silhouette = silhouette_confidence(rgba, settings)
    detail = alpha_detail_confidence(rgba, settings)
    depth_conf = depth_discontinuity(depth, settings) if depth is not None else None
    combined = combine(alpha_boundary(rgba, settings), depth_conf, settings)
    line = soft_line(combined, settings.width_px, settings.softness)
    return {
        "silhouette": silhouette,
        "alpha_detail": detail,
        "depth_discontinuity": depth_conf,
        "confidence": combined,
        "line": line,
    }