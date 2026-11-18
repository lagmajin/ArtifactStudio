"""Offline reference models for seamless (tileable) texture synthesis (RD1).

Three candidate families, all operating on premultiplied linear float:

  A  mirror tiling   - reflect the tile across its borders. Seamless by
                       construction, but the motif is duplicated.
  B  quilt / offset  - shift the tile by half a period and re-join the two
                       halves along a minimum-cost seam found by dynamic
                       programming, then feather the seam. Keeps more of the
                       original content than mirroring at the cost of a visible
                       blend band.
  C  periodic filter - define the tile on a torus (circular convolution), so
                       the field is continuous across the border by
                       construction. Removes the seam but low-passes the
                       texture.

Contract enforced by every model:
  * RGB and alpha are processed together
  * the unpremultiply happens exactly once, at the very end
  * alpha <= ALPHA_EPS is not divided by; RGB is written as 0 there
  * the straight and premultiplied input descriptors must produce the same
    premultiplied buffer, so the recovered straight RGB must match

No learning model, no product code, no build.
"""

from __future__ import annotations

import cv2
import numpy as np

ALPHA_EPS = 1e-3
MAX_STRAIGHT = 16.0
SEAM_FEATHER = 6  # pixels blended around the quilt seam


def unpremultiply(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Unpremultiply once; alpha <= ALPHA_EPS yields RGB = 0 (no NaN / inf)."""
    a = alpha[..., 0:1] if alpha.shape[-1] == 1 else alpha
    if a.ndim == 2:
        a = a[..., None]
    safe = np.where(a > ALPHA_EPS, a, np.float32(1.0))
    out = np.where(a > ALPHA_EPS, premul / safe, np.float32(0.0))
    return np.clip(out, 0.0, MAX_STRAIGHT).astype(np.float32)


def to_premultiplied(rgba: np.ndarray, descriptor: str) -> tuple[np.ndarray, np.ndarray]:
    src = np.asarray(rgba, dtype=np.float32)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = (src[..., :3] * alpha) if descriptor == "straight" else np.maximum(src[..., :3], 0.0)
    return premul.astype(np.float32), alpha


def _mirror_candidate(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Reflect the tile across both borders (mirror tiling)."""
    mirrored = cv2.flip(premul, 1)
    out_premul = np.concatenate([premul, mirrored], axis=1)
    mirrored_a = cv2.flip(alpha, 1)
    out_alpha = np.concatenate([alpha, mirrored_a], axis=1)
    out_premul = cv2.flip(out_premul, 0)
    out_alpha = cv2.flip(out_alpha, 0)
    return out_premul, out_alpha

def _seam_path(cost: np.ndarray) -> np.ndarray:
    """Classic O(h*w) dynamic-programming minimum-cost path (vertical seam)."""
    h, w = cost.shape
    dp = np.zeros((h, w), dtype=np.float64)
    dp[0] = cost[0]
    for y in range(1, h):
        prev = dp[y - 1]
        left = np.concatenate([np.full(1, np.inf), prev[:-1]])
        right = np.concatenate([prev[1:], np.full(1, np.inf)])
        best = np.minimum(left, right)
        ties = (left == best) | (right == best)
        idx = np.where(left <= right, np.concatenate([[0], np.arange(w - 1)]),
                       np.concatenate([[1], np.arange(1, w)]))
        dp[y] = prev[idx] + cost[y]
        del ties
    path = np.zeros(h, dtype=np.int32)
    path[h - 1] = int(np.argmin(dp[h - 1]))
    for y in range(h - 1, 0, -1):
        x = path[y]
        prev = dp[y - 1]
        left = prev[x - 1] if x > 0 else np.inf
        right = prev[x + 1] if x + 1 < w else np.inf
        if left <= right:
            path[y - 1] = max(0, x - 1)
        else:
            path[y - 1] = min(w - 1, x + 1)
    return path


def _quilt_candidate(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Offset the tile by half a period and re-join along a minimum-cost seam."""
    h, w = premul.shape[:2]
    shift_x, shift_y = w // 2, h // 2
    shifted_premul = np.roll(np.roll(premul, shift_y, axis=0), shift_x, axis=1)
    shifted_alpha = np.roll(np.roll(alpha, shift_y, axis=0), shift_x, axis=1)

    luma_a = premul.mean(axis=2)
    luma_b = shifted_premul.mean(axis=2)
    cost = np.zeros((h, w), dtype=np.float64)
    for dy, dx in ((0, 1), (1, 0), (1, 1)):
        cost += np.abs(np.roll(luma_a, (dy, dx), axis=(0, 1)) - luma_a)
        cost += np.abs(np.roll(luma_b, (dy, dx), axis=(0, 1)) - luma_b)
    cost /= 2.0

    path = _seam_path(cost)
    blend = np.zeros((h, w), dtype=np.float32)
    feather = max(2, SEAM_FEATHER)
    for y, x in enumerate(path):
        lo = max(0, x - feather)
        hi = min(w, x + feather + 1)
        blend[y, lo:hi] = np.linspace(0.0, 1.0, hi - lo, dtype=np.float32)
    blend_2d = np.clip(blend, 0.0, 1.0)
    blend_rgb = blend_2d[..., None]
    out_premul = premul * (1.0 - blend_rgb) + shifted_premul * blend_rgb
    out_alpha = alpha * (1.0 - blend_2d) + shifted_alpha * blend_2d
    return out_premul, out_alpha


def _periodic_candidate(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Define the field on a torus with a circular (FFT) low-pass."""
    h, w = premul.shape[:2]
    stack = np.concatenate([premul, alpha[..., None]], axis=2).astype(np.float64)
    spectrum = np.fft.fft2(stack, axes=(0, 1))
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    radius = np.sqrt(fy ** 2 + fx ** 2)
    cutoff = 0.18
    weight = np.clip(1.0 - (radius / cutoff), 0.0, 1.0)
    filtered = np.real(np.fft.ifft2(spectrum * weight[..., None], axes=(0, 1)))
    out_premul = filtered[..., :3].astype(np.float32)
    out_alpha = np.clip(filtered[..., 3:4], 0.0, 1.0).astype(np.float32)
    return out_premul, out_alpha


def synthesize(rgba: np.ndarray, spec: dict, descriptor: str = "straight") -> np.ndarray:
    """Return a straight RGBA tile produced by the requested candidate."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    premul, alpha = to_premultiplied(rgba, descriptor)
    alpha_2d = alpha[..., 0]
    model = spec.get("model", "A")
    if model == "A":
        out_premul, out_alpha = _mirror_candidate(premul, alpha_2d)
    elif model == "B":
        out_premul, out_alpha = _quilt_candidate(premul, alpha_2d)
    elif model == "C":
        out_premul, out_alpha = _periodic_candidate(premul, alpha_2d)
    else:
        raise ValueError(f"unknown model: {model}")
    out = np.empty((out_premul.shape[0], out_premul.shape[1], 4), dtype=np.float32)
    out[..., :3] = unpremultiply(out_premul, out_alpha)
    out[..., 3:4] = out_alpha[..., None] if out_alpha.ndim == 2 else out_alpha
    return out


def tile(rgba: np.ndarray, repeats: int) -> np.ndarray:
    """Repeat the tile in both directions."""
    return np.tile(rgba, (repeats, repeats, 1))


def _luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def seam_metrics(rgba: np.ndarray, repeats: int = 3) -> dict:
    """Measure how visible the tile borders are when the tile is repeated.

    seam_energy_ratio   gradient energy on the tile border lines divided by
                        the gradient energy inside the tiles. 1.0 means the
                        border is indistinguishable from the interior.
    luminance_step      mean |luma| difference across the border.
    rgb_step            mean absolute RGB difference across the border.
    boundary_mode_energy  share of the 2D spectrum that sits on the tile
                        border frequencies; a perfectly periodic field has
                        almost none of its energy there.
    """
    rgb = rgba[..., :3].astype(np.float64)
    h, w = rgb.shape[:2]
    tiled = np.tile(rgb, (repeats, repeats, 1))
    luma = _luma(tiled)

    gy, gx = np.gradient(luma)
    energy = np.hypot(gx, gy)

    seam_mask = np.zeros(energy.shape, dtype=bool)
    for k in range(1, repeats):
        seam_mask[:, k * w - 1] = True
        seam_mask[k * h - 1, :] = True
    interior = ~seam_mask
    seam_energy = float(energy[seam_mask].mean()) if seam_mask.any() else 0.0
    interior_energy = float(energy[interior].mean()) if interior.any() else 0.0
    ratio = seam_energy / interior_energy if interior_energy > 0 else float("nan")

    steps_luma = []
    steps_rgb = []
    for k in range(1, repeats):
        steps_luma.append(float(np.abs(luma[:, k * w] - luma[:, k * w - 1]).mean()))
        steps_luma.append(float(np.abs(luma[k * h, :] - luma[k * h - 1, :]).mean()))
        steps_rgb.append(float(np.abs(tiled[:, k * w] - tiled[:, k * w - 1]).mean()))
        steps_rgb.append(float(np.abs(tiled[k * h, :] - tiled[k * h - 1, :]).mean()))

    spectrum = np.abs(np.fft.fft2(rgb.mean(axis=2))) ** 2
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    boundary = (np.abs(np.abs(fx) - 1.0 / w) < (0.5 / w)) | (np.abs(np.abs(fy) - 1.0 / h) < (0.5 / h))
    total = float(spectrum.sum())
    boundary_share = float(spectrum[boundary].sum() / total) if total > 0 else float("nan")

    return {
        "seam_energy_ratio": ratio,
        "seam_energy": seam_energy,
        "interior_energy": interior_energy,
        "luminance_step": float(np.mean(steps_luma)) if steps_luma else 0.0,
        "rgb_step": float(np.mean(steps_rgb)) if steps_rgb else 0.0,
        "boundary_mode_energy": boundary_share,
        "repeats": repeats,
    }


def repetition_peak(rgba: np.ndarray, max_lag_fraction: float = 0.5) -> dict:
    """How strongly the texture repeats itself inside one tile.

    A high peak means the motif is duplicated (mirror tiling typically) and the
    tiled result will read as a repeated pattern rather than a continuous
    surface.
    """
    luma = _luma(rgba[..., :3].astype(np.float64))
    luma = luma - luma.mean()
    h, w = luma.shape
    spectrum = np.fft.rfft2(luma)
    autocorr = np.fft.irfft2(np.abs(spectrum) ** 2, s=(h, w))
    autocorr = np.fft.fftshift(autocorr)
    center_y, center_x = h // 2, w // 2
    cy, cx = np.ogrid[:h, :w]
    dist = np.sqrt(((cy - center_y) / h) ** 2 + ((cx - center_x) / w) ** 2)
    band = (dist >= 0.08) & (dist <= max_lag_fraction)
    peak = float(autocorr[band].max()) if band.any() else 0.0
    zero = float(autocorr[center_y, center_x])
    return {
        "repetition_peak_ratio": peak / zero if zero > 0 else float("nan"),
        "repetition_peak_abs": peak,
    }


def absolute_probe(rgba: np.ndarray) -> dict:
    rgb = rgba[..., :3].astype(np.float64)
    alpha = rgba[..., 3].astype(np.float64)
    visible = alpha > 0.5
    near = (alpha > 0.001) & (alpha <= 0.5)
    return {
        "nonfinite_count": int(np.sum(~np.isfinite(rgb))),
        "visible_max": float(rgb[visible].max()) if visible.any() else None,
        "near_transparent_max": float(rgb[near].max()) if near.any() else None,
        "near_transparent_channels_gt_1": int(np.sum(rgb[near] > 1.0)) if near.any() else 0,
        "alpha_min": float(alpha.min()),
        "alpha_max": float(alpha.max()),
    }


def descriptor_delta(straight_out: np.ndarray, premul_out: np.ndarray) -> float:
    """Max difference of the recovered straight RGB between both descriptors."""
    a = straight_out[..., 3:4]
    valid = a[..., 0] > 0.01
    rec = premul_out[..., :3] / np.maximum(premul_out[..., 3:4], 1e-6)
    if not valid.any():
        return 0.0
    return float(np.max(np.abs(rec - straight_out[..., :3])[valid]))
