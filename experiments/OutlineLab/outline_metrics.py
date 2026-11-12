#!/usr/bin/env python3
"""Numpy-only metrics for Outline Lab reference outputs.

Phase 1.5 adds measured line width and boundary position error so a requested
`width_px` is not silently assumed. Everything here works on float32 arrays and
uses explicit definitions; no scipy, no product code.

Metric definitions:
- boundary_position_error: distance from detected line pixels to the truth
  boundary ring, and from the truth ring to the nearest detected line pixel.
- line_width_50 / line_width_90: extent along the boundary normal where the
  line matte stays above 50% / 90% of its local peak. Reported as p10/p50/p90
  over sampled boundary points.
- unwanted_line_rate: fraction of detected line pixels farther than
  `width_px + 2` from the truth ring.
"""

from __future__ import annotations

from typing import Any

import numpy as np


Array = np.ndarray


def _edt_1d(f: Array) -> Array:
    """Exact 1D squared distance transform (Felzenszwalb-Huttenlocher)."""
    n = f.shape[0]
    d = np.empty(n, dtype=np.float64)
    v = np.zeros(n, dtype=np.int64)
    z = np.empty(n + 1, dtype=np.float64)
    k = 0
    v[0] = 0
    z[0] = -np.inf
    z[1] = np.inf
    for q in range(1, n):
        while True:
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0 * q - 2.0 * v[k])
            if s <= z[k]:
                k -= 1
            else:
                break
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = np.inf
    k = 0
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        d[q] = (q - v[k]) * (q - v[k]) + f[v[k]]
    return d


def distance_transform(mask: Array) -> Array:
    """Euclidean distance (pixels) from each pixel to the nearest True pixel."""
    # A finite sentinel keeps the 1D transform free of inf-inf NaNs.
    f = np.where(np.asarray(mask, dtype=bool), 0.0, 1.0e12)
    rows = np.empty_like(f, dtype=np.float64)
    for y in range(f.shape[0]):
        rows[y] = _edt_1d(f[y])
    out = np.empty_like(rows, dtype=np.float64)
    for x in range(f.shape[1]):
        out[:, x] = _edt_1d(rows[:, x])
    return np.sqrt(out).astype(np.float32)


def boundary_ring(mask: Array) -> Array:
    """Pixels inside `mask` that touch a 4-neighbour outside `mask`."""
    m = np.asarray(mask, dtype=bool)
    eroded = m.copy()
    eroded[1:, :] &= m[:-1, :]
    eroded[:-1, :] &= m[1:, :]
    eroded[:, 1:] &= m[:, :-1]
    eroded[:, :-1] &= m[:, 1:]
    return m & ~eroded


def sample_bilinear(image: Array, ys: Array, xs: Array) -> Array:
    h, w = image.shape
    ys = np.clip(ys, 0.0, h - 1.0)
    xs = np.clip(xs, 0.0, w - 1.0)
    y0 = np.floor(ys).astype(np.int64)
    x0 = np.floor(xs).astype(np.int64)
    y1 = np.minimum(y0 + 1, h - 1)
    x1 = np.minimum(x0 + 1, w - 1)
    wy = ys - y0
    wx = xs - x0
    v00 = image[y0, x0]
    v01 = image[y0, x1]
    v10 = image[y1, x0]
    v11 = image[y1, x1]
    return (
        v00 * (1.0 - wy) * (1.0 - wx)
        + v01 * (1.0 - wy) * wx
        + v10 * wy * (1.0 - wx)
        + v11 * wy * wx
    )


def _percentiles(values: Array) -> dict[str, float]:
    if values.size == 0:
        return {"p10": 0.0, "p50": 0.0, "p90": 0.0, "mean": 0.0, "count": 0}
    return {
        "p10": float(np.percentile(values, 10.0)),
        "p50": float(np.percentile(values, 50.0)),
        "p90": float(np.percentile(values, 90.0)),
        "mean": float(np.mean(values)),
        "count": int(values.size),
    }


def _ring_normals(
    boundary: Array,
    ys: Array,
    xs: Array,
    radius: float = 2.0,
) -> tuple[Array, Array, Array]:
    """Local principal-axis normals from ring points.

    The gradient of the distance-to-ring field is zero at ring pixels of a thin
    ring, so it cannot supply a normal there. A small local covariance instead
    gives the ring tangent, and its perpendicular is the normal.
    """
    py, px = np.nonzero(np.asarray(boundary, dtype=bool))
    ny = np.zeros(ys.size, dtype=np.float64)
    nx = np.zeros(ys.size, dtype=np.float64)
    ok = np.zeros(ys.size, dtype=bool)
    r2 = radius * radius
    for i in range(ys.size):
        dy = py - ys[i]
        dx = px - xs[i]
        keep = (dy * dy + dx * dx) <= r2
        if keep.sum() < 3:
            continue
        dy = dy[keep].astype(np.float64)
        dx = dx[keep].astype(np.float64)
        cxx = float(np.mean(dx * dx))
        cyy = float(np.mean(dy * dy))
        cxy = float(np.mean(dx * dy))
        half_tr = 0.5 * (cxx + cyy)
        disc = max(0.0, half_tr * half_tr - (cxx * cyy - cxy * cxy))
        l1 = half_tr + np.sqrt(disc)
        # Eigenvector for the largest eigenvalue is the ring tangent.
        tx, ty = (l1 - cyy, cxy) if abs(l1 - cyy) > 1e-12 else (cxy, l1 - cxx)
        norm = np.hypot(tx, ty)
        if norm < 1e-9:
            continue
        # Tangent is (tx, ty) in (x, y); the normal is (-ty, tx).
        ny[i] = tx / norm
        nx[i] = -ty / norm
        ok[i] = True
    return ny, nx, ok


def line_widths(
    line: Array,
    boundary: Array,
    max_width_px: float = 12.0,
    step: float = 0.25,
    max_samples: int = 512,
    min_peak: float = 0.05,
) -> dict[str, Any]:
    """Measure line width along boundary normals.

    The normal comes from the gradient of the distance-to-boundary field. Both
    directions are sampled, so the normal sign does not matter.
    """
    ys, xs = np.nonzero(np.asarray(boundary, dtype=bool))
    if ys.size == 0:
        return {
            "width_50": _percentiles(np.empty(0, dtype=np.float32)),
            "width_90": _percentiles(np.empty(0, dtype=np.float32)),
            "peak": _percentiles(np.empty(0, dtype=np.float32)),
        }
    stride = max(1, int(np.ceil(ys.size / float(max_samples))))
    ys = ys[::stride]
    xs = xs[::stride]

    ny, nx, usable = _ring_normals(boundary, ys, xs)
    # Boundary points with no reliable local ring direction are dropped instead
    # of sampling along an arbitrary axis.
    if not usable.any():
        return {
            "width_50": _percentiles(np.empty(0, dtype=np.float32)),
            "width_90": _percentiles(np.empty(0, dtype=np.float32)),
            "peak": _percentiles(np.empty(0, dtype=np.float32)),
        }
    ys = ys[usable]
    xs = xs[usable]
    ny = ny[usable]
    nx = nx[usable]

    offsets = np.arange(-max_width_px, max_width_px + step * 0.5, step, dtype=np.float64)
    sample_y = ys[:, None] + ny[:, None] * offsets[None, :]
    sample_x = xs[:, None] + nx[:, None] * offsets[None, :]
    profile = sample_bilinear(line.astype(np.float64), sample_y, sample_x)

    peak = profile.max(axis=1)
    valid = peak > min_peak
    if not valid.any():
        return {
            "width_50": _percentiles(np.empty(0, dtype=np.float32)),
            "width_90": _percentiles(np.empty(0, dtype=np.float32)),
            "peak": _percentiles(np.empty(0, dtype=np.float32)),
        }
    profile = profile[valid]
    peak = peak[valid]
    # Measure the contiguous run around offset 0, so a normal that crosses two
    # nearby edges does not sum two separate line runs into one width.
    center = offsets.size // 2
    index = np.arange(offsets.size)
    above_50 = profile >= 0.5 * peak[:, None]
    above_90 = profile >= 0.9 * peak[:, None]

    def _run(a: Array) -> Array:
        left = np.where(~a[:, : center + 1], index[: center + 1], -1).max(axis=1)
        right = np.where(~a[:, center:], index[center:], offsets.size).min(axis=1)
        return np.clip(right - left - 1, 0, None).astype(np.float64) * step

    width_50 = _run(above_50)
    width_90 = _run(above_90)
    return {
        "width_50": _percentiles(width_50.astype(np.float32)),
        "width_90": _percentiles(width_90.astype(np.float32)),
        "peak": _percentiles(peak.astype(np.float32)),
    }


def position_error(
    line: Array,
    boundary: Array,
    threshold: float = 0.5,
    width_px: float = 2.0,
) -> dict[str, Any]:
    """Distance statistics between the detected line and the truth ring."""
    detected = line >= threshold
    truth = np.asarray(boundary, dtype=bool)
    if not detected.any() or not truth.any():
        empty = _percentiles(np.empty(0, dtype=np.float32))
        return {
            "line_to_truth": empty,
            "truth_to_line": empty,
            "unwanted_line_rate": 0.0,
        }
    d_truth = distance_transform(truth)
    d_line = distance_transform(detected)
    line_to_truth = d_truth[detected].astype(np.float32)
    truth_to_line = d_line[truth].astype(np.float32)
    tolerance = float(width_px) + 2.0
    return {
        "line_to_truth": _percentiles(line_to_truth),
        "truth_to_line": _percentiles(truth_to_line),
        "unwanted_line_rate": float(np.mean(line_to_truth > tolerance)),
    }
