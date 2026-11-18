"""Offline reference models for material (texture / detail) separation (RD1).

Three candidate families, all operating in premultiplied linear float:

  A  self two-layer decomposition (guided filter base / detail)
  B  frequency separation (gaussian blur difference)
  C  emphasis model (independent base / coarse-texture / micro-texture gains)

Contract enforced by every model:
  * RGB and alpha are processed together with the same neighbourhood operator
  * the unpremultiply happens exactly once, at the very end
  * alpha <= ALPHA_EPS is not divided by; RGB is written as 0 there
  * the straight and premultiplied input descriptors must produce a bitwise
    identical premultiplied buffer, so the recovered straight RGB must match

No learning model, no product code, no build.
"""

from __future__ import annotations

import cv2
import numpy as np

ALPHA_EPS = 1e-3
GUIDED_EPS = 1e-4
MAX_STRAIGHT = 16.0


def _box(img: np.ndarray, radius: int) -> np.ndarray:
    k = (2 * radius + 1, 2 * radius + 1)
    out = cv2.boxFilter(img, -1, k, normalize=True, borderType=cv2.BORDER_REFLECT)
    if out.ndim == 2 and img.ndim == 3:
        out = out[..., None]
    return out


def guided_filter(guide: np.ndarray, src: np.ndarray, radius: int, eps: float) -> np.ndarray:
    """Standard guided filter (He et al.). guide and src may be multi-channel."""
    mean_i = _box(guide, radius)
    mean_p = _box(src, radius)
    corr_i = _box(guide * guide, radius)
    corr_ip = _box(guide * src, radius)
    var_i = corr_i - mean_i * mean_i
    cov_ip = corr_ip - mean_i * mean_p
    a = cov_ip / (var_i + eps)
    b = mean_p - a * mean_i
    return _box(a, radius) * guide + _box(b, radius)


def unpremultiply(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Unpremultiply once; alpha <= ALPHA_EPS yields RGB = 0 (no NaN / inf).

    The result is clamped to MAX_STRAIGHT so that a near-zero divisor cannot
    produce unbounded values (the WatercolorLab RD3/RD4 failure mode). The
    clamp is a safety bound, not a quality gate: values above 1.0 are still
    reported by the measurements so the reviewer can judge them.
    """
    a = alpha[..., 0:1] if alpha.shape[-1] == 1 else alpha
    safe = np.where(a > ALPHA_EPS, a, np.float32(1.0))
    out = np.where(a > ALPHA_EPS, premul / safe, np.float32(0.0))
    return np.clip(out, 0.0, MAX_STRAIGHT).astype(np.float32)


def _guided_base(premul: np.ndarray, alpha: np.ndarray, radius: int, eps: float) -> tuple[np.ndarray, np.ndarray]:
    """Self-guided base for RGB (per channel) and alpha (scalar)."""
    base_rgb = guided_filter(premul, premul, radius, eps)
    base_a = guided_filter(alpha, alpha, radius, eps)
    return base_rgb, base_a


def _gaussian_base(premul: np.ndarray, alpha: np.ndarray, sigma: float) -> tuple[np.ndarray, np.ndarray]:
    """Frequency separation base: one 4-channel gaussian so RGB and alpha share the kernel."""
    stack = np.concatenate([premul, alpha], axis=2).astype(np.float32)
    blurred = cv2.GaussianBlur(stack, (0, 0), sigmaX=sigma, sigmaY=sigma,
                               borderType=cv2.BORDER_REFLECT)
    return blurred[..., :3], blurred[..., 3:4]

def decompose(rgba: np.ndarray, spec: dict, descriptor: str) -> dict:
    """Return premultiplied base/detail layers for the requested model.

    The alpha channel is decomposed with the same neighbourhood operator as
    RGB, so every recomposition has a matching alpha divisor. Mixing a
    filtered premul with an unfiltered alpha is what produces unpremultiply
    blow-up (hundreds of times 1.0) near transparent pixels.
    """
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    src = np.asarray(rgba, dtype=np.float32)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    # straight path multiplies in float32 exactly like the fixture writer does,
    # so both descriptors feed bitwise identical premultiplied buffers.
    premul = (src[..., :3] * alpha) if descriptor == "straight" else src[..., :3].copy()
    premul = premul.astype(np.float32)

    model = spec.get("model", "A")
    out: dict = {"premul": premul, "alpha": alpha}
    if model == "A":
        radius = int(spec.get("radius", 4))
        eps = float(spec.get("eps", GUIDED_EPS))
        base_rgb, base_a = _guided_base(premul, alpha, radius, eps)
        out.update({
            "base": base_rgb, "detail": premul - base_rgb,
            "base_alpha": base_a, "detail_alpha": alpha - base_a,
        })
    elif model == "B":
        sigma = float(spec.get("sigma", 2.0))
        base_rgb, base_a = _gaussian_base(premul, alpha, sigma)
        out.update({
            "base": base_rgb, "detail": premul - base_rgb,
            "base_alpha": base_a, "detail_alpha": alpha - base_a,
        })
    elif model == "C":
        radius_fine = int(spec.get("radius_fine", 2))
        radius_coarse = int(spec.get("radius_coarse", 8))
        eps = float(spec.get("eps", GUIDED_EPS))
        base_fine, base_a_fine = _guided_base(premul, alpha, radius_fine, eps)
        base_coarse, base_a_coarse = _guided_base(premul, alpha, radius_coarse, eps)
        detail_coarse = base_coarse - base_fine
        detail_fine = premul - base_coarse
        out.update({
            "base": base_fine, "detail": premul - base_fine,
            "detail_coarse": detail_coarse, "detail_fine": detail_fine,
            "base_alpha": base_a_fine,
            "detail_alpha": alpha - base_a_fine,
            "detail_coarse_alpha": base_a_coarse - base_a_fine,
            "detail_fine_alpha": alpha - base_a_coarse,
        })
    else:
        raise ValueError(f"unknown model: {model}")
    return out


def recompose(layers: dict, spec: dict) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Apply the gains.

    Returns (premultiplied RGB, alpha channel of the output, alpha divisor).
    The divisor is the alpha that belongs to the recomposed layers, so the
    single final unpremultiply stays consistent. Neutral gains (all 1.0)
    reproduce the input premultiplied buffer and the input alpha exactly.
    """
    model = spec.get("model", "A")
    base_gain = float(spec.get("base_gain", 1.0))
    detail_gain = float(spec.get("detail_gain", 1.0))
    if model == "C":
        coarse_gain = float(spec.get("coarse_gain", 1.0))
        fine_gain = float(spec.get("fine_gain", 1.0))
        premul = (layers["base"] * base_gain
                  + layers["detail_coarse"] * coarse_gain
                  + layers["detail_fine"] * fine_gain)
        divisor = (layers["base_alpha"] * base_gain
                   + layers["detail_coarse_alpha"] * coarse_gain
                   + layers["detail_fine_alpha"] * fine_gain)
    else:
        premul = layers["base"] * base_gain + layers["detail"] * detail_gain
        divisor = layers["base_alpha"] * base_gain + layers["detail_alpha"] * detail_gain
    # The output alpha is the layer-consistent alpha (the same one used as the
    # unpremultiply divisor), so straight RGB * alpha always reconstructs the
    # recomposed premultiplied buffer exactly. With neutral gains it equals
    # the input alpha.
    divisor = np.clip(divisor, 0.0, 1.0).astype(np.float32)
    alpha = divisor
    return premul.astype(np.float32), alpha, divisor


def apply_candidate(rgba: np.ndarray, spec: dict, seed: int = 0, descriptor: str = "straight") -> np.ndarray:
    """Apply a candidate spec to a float32 RGBA buffer and return straight RGBA."""
    layers = decompose(rgba, spec, descriptor)
    premul, alpha, divisor = recompose(layers, spec)
    out = np.empty_like(np.asarray(rgba, dtype=np.float32))
    out[..., :3] = unpremultiply(premul, divisor)
    out[..., 3:4] = alpha
    return out


def apply_base_only(rgba: np.ndarray, spec: dict, seed: int = 0, descriptor: str = "straight") -> np.ndarray:
    """detail_gain = 0 variant: only the low frequency base survives."""
    spec = dict(spec)
    spec["detail_gain"] = 0.0
    spec["coarse_gain"] = 0.0
    spec["fine_gain"] = 0.0
    return apply_candidate(rgba, spec, seed, descriptor)


def detail_statistics(layers: dict) -> dict:
    """Descriptive amplitude statistics of the detail layer (premultiplied space)."""
    detail = layers["detail"].astype(np.float64)
    mag = np.abs(detail).mean(axis=2)
    base = np.abs(layers["base"]).mean(axis=2)
    rel = detail.mean(axis=2) / np.maximum(base, 1e-4)
    finite = mag[np.isfinite(mag)]
    return {
        "detail_abs_mean": float(finite.mean()) if finite.size else None,
        "detail_abs_std": float(finite.std()) if finite.size else None,
        "detail_abs_p50": float(np.percentile(finite, 50)) if finite.size else None,
        "detail_abs_p95": float(np.percentile(finite, 95)) if finite.size else None,
        "detail_abs_p99": float(np.percentile(finite, 99)) if finite.size else None,
        "detail_abs_max": float(finite.max()) if finite.size else None,
        "detail_signed_mean": float(detail.mean()),
        "detail_relative_mean": float(rel.mean()),
        "detail_relative_p95": float(np.percentile(rel, 95)),
        "nonfinite_count": int(np.sum(~np.isfinite(detail))),
    }
