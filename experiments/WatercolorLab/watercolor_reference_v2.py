"""RD2 reference model: descriptor-consistent watercolor conversion.

RD1 found that "unpremultiply -> neighborhood operation -> re-premultiply"
breaks straight color at alpha boundaries: premultiplied inputs lose hidden
RGB in transparent neighbors, so a subsequent blur/remap mixes in 0 or
diverging values.

This v2 model instead performs every component in PREMULTIPLIED space and
unpremultiplies only once at the end. For spatial filters it filters the
premultiplied RGB and the alpha with the same kernel, then divides. That is
the standard way to keep compositing linear and avoids depending on hidden
RGB that a premultiplied descriptor does not carry.

The public entry point returns exactly the requested descriptor, so a
straight input and an equivalent premultiplied input must produce the same
recovered straight RGB (up to float32 rounding).
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8


def _value_noise(h: int, w: int, rng: np.random.Generator, cells_x: int, cells_y: int) -> np.ndarray:
    grid = rng.random((cells_y, cells_x)).astype(np.float32)
    up = cv2.resize(grid, (w, h), interpolation=cv2.INTER_CUBIC)
    return (up - up.mean()) / (up.std() + 1e-6)


def _paper_value(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    octaves = []
    for cx, cy, weight in ((8, 6, 0.55), (24, 16, 0.30), (64, 48, 0.15)):
        octaves.append(_value_noise(h, w, rng, cx, cy) * weight)
    return np.sum(octaves, axis=0).astype(np.float32)


def _paper_fibers(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed + 11)
    fine = rng.normal(0.0, 1.0, size=(h, w)).astype(np.float32)
    fibers = cv2.GaussianBlur(fine, (0, 0), sigmaX=8.0, sigmaY=1.0, borderType=cv2.BORDER_REFLECT)
    fibers = (fibers - fibers.mean()) / (fibers.std() + 1e-6)
    grain = _value_noise(h, w, rng, 96, 64)
    return (0.75 * fibers + 0.25 * grain).astype(np.float32)


def _as_alpha(x: np.ndarray) -> np.ndarray:
    """OpenCV drops a trailing singleton channel; keep alpha as (h, w, 1)."""
    return x[..., None] if x.ndim == 2 else x


def _edge_gate_from_premul(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Compute a normalized edge gate from premultiplied RGB (alpha-aware)."""
    straight_vis = premul / np.maximum(alpha, 1e-3)
    gray = cv2.cvtColor(linear_to_srgb_u8(straight_vis), cv2.COLOR_RGB2GRAY).astype(np.float32) / 255.0
    gx = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    mag = np.hypot(gx, gy)
    gate = np.clip(mag * 2.5, 0.0, 1.0)
    return gate * np.clip(alpha[..., 0], 0.0, 1.0)


def _bilateral_premul(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    # Color weighting uses premultiplied channels; transparent neighbors
    # (near zero) are naturally down-weighted by sigmaColor.
    return cv2.bilateralFilter(premul, d=7, sigmaColor=0.08, sigmaSpace=3.0)


def _quantize_gauss_premul(premul: np.ndarray) -> np.ndarray:
    levels = 12
    quantized = np.round(premul * (levels - 1)) / (levels - 1)
    return cv2.GaussianBlur(quantized, (0, 0), sigmaX=2.5, sigmaY=2.5, borderType=cv2.BORDER_REFLECT)


def _edge_weighted_premul(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    gate = _edge_gate_from_premul(premul, alpha)
    sigma = 3.0
    blurred_p = cv2.GaussianBlur(premul, (0, 0), sigmaX=sigma, sigmaY=sigma, borderType=cv2.BORDER_REFLECT)
    blurred_a = cv2.GaussianBlur(alpha, (0, 0), sigmaX=sigma, sigmaY=sigma, borderType=cv2.BORDER_REFLECT)
    weight = (1.0 - gate)[..., None] * 0.75
    return premul * (1.0 - weight) + blurred_p * weight, alpha * (1.0 - weight) + _as_alpha(blurred_a) * weight


def _softstep_premul(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    k = 9
    mean_p = cv2.blur(premul, (k, k), borderType=cv2.BORDER_REFLECT)
    mean_a = cv2.blur(alpha, (k, k), borderType=cv2.BORDER_REFLECT)
    sq = cv2.blur(premul * premul, (k, k), borderType=cv2.BORDER_REFLECT)
    std = np.sqrt(np.maximum(sq - mean_p * mean_p, 0.0)).mean(axis=2, keepdims=True)
    weight = np.clip(std * 6.0, 0.0, 1.0) * 0.45
    return premul * (1.0 - weight) + mean_p * weight, alpha * (1.0 - weight) + _as_alpha(mean_a) * weight


def _wobble_premul(premul: np.ndarray, alpha: np.ndarray, seed: int) -> tuple[np.ndarray, np.ndarray]:
    h, w = premul.shape[:2]
    rng = np.random.default_rng(seed + 7)
    n1 = _value_noise(h, w, rng, 12, 10)
    n2 = _value_noise(h, w, rng, 12, 10)
    gate = _edge_gate_from_premul(premul, alpha)
    amp = 1.4 * (0.35 + 0.65 * gate)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    map_x = (xx + n1 * amp).astype(np.float32)
    map_y = (yy + n2 * amp).astype(np.float32)
    warped_p = cv2.remap(premul, map_x, map_y, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT)
    warped_a = cv2.remap(alpha, map_x, map_y, interpolation=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT)
    return warped_p, _as_alpha(warped_a)


def apply_candidate_v2(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a candidate spec in premultiplied space.

    The alpha channel is carried through the same spatial transforms as the
    premultiplied RGB and used for the single final unpremultiply.
    """
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")

    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    if descriptor == "straight":
        premul = src[..., :3] * alpha
    else:
        premul = src[..., :3].copy()

    original_p = premul.copy()
    original_a = alpha.copy()

    bleed = spec.get("bleed")
    if bleed == "bilateral":
        premul = _bilateral_premul(premul, alpha)
    elif bleed == "quantize_gauss":
        premul = _quantize_gauss_premul(premul)
    elif bleed == "edge_weighted":
        premul, alpha = _edge_weighted_premul(premul, alpha)
    elif bleed is not None:
        raise ValueError(f"unknown bleed component: {bleed}")

    paper = spec.get("paper")
    if paper == "value_noise":
        n = _paper_value(premul.shape[0], premul.shape[1], seed)
        premul = premul * (1.0 + 0.06 * n)[..., None]
    elif paper == "fibers":
        n = _paper_fibers(premul.shape[0], premul.shape[1], seed)
        premul = premul * (1.0 + 0.04 * n)[..., None]
    elif paper is not None:
        raise ValueError(f"unknown paper component: {paper}")

    edge = spec.get("edge")
    if edge == "softstep":
        premul, alpha = _softstep_premul(premul, alpha)
    elif edge == "wobble":
        premul, alpha = _wobble_premul(premul, alpha, seed)
    elif edge is not None:
        raise ValueError(f"unknown edge component: {edge}")

    strength = float(spec.get("strength", 1.0))
    premul = original_p + strength * (premul - original_p)
    alpha = original_a + strength * (alpha - original_a)
    premul = np.maximum(premul, 0.0)
    alpha = np.clip(alpha, 0.0, 1.0)

    straight = premul / np.maximum(alpha, 1e-6)

    out = np.empty_like(src)
    if descriptor == "premultiplied":
        out[..., :3] = premul
    else:
        out[..., :3] = straight
    out[..., 3:4] = alpha
    return out
