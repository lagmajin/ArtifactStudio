"""Offline reference model for pencil/print-style conversion (RD1).

Three separable components operate on premultiplied RGB:
  line:    Sobel-based line darkening (shared alpha policy: alpha unchanged)
  tone:    posterize or ordered-dither on premul and alpha with one grid
  texture: paper grain or hatch modulation

All components are pointwise after any neighborhood-derived gate is computed,
and premul is scaled with the same factor that straight color should receive.
Alpha is either unchanged or quantized with the same grid, so the final
unpremultiply cannot expose hidden RGB from transparent neighbors (the failure
mode found in Watercolor RD3/RD4).

No learning model, no product code, no build.
"""

from __future__ import annotations

import cv2
import numpy as np

from generate_fixtures import linear_to_srgb_u8

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0


def _luma_visual(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    straight = premul / np.maximum(alpha, 1e-3)
    return straight[..., 0] * 0.2126 + straight[..., 1] * 0.7152 + straight[..., 2] * 0.0722


def _line_sobel(premul: np.ndarray, alpha: np.ndarray, strength: float) -> np.ndarray:
    gray = cv2.cvtColor(linear_to_srgb_u8(_luma_visual(premul, alpha)[..., None].repeat(3, axis=2)),
                        cv2.COLOR_RGB2GRAY).astype(np.float32) / 255.0
    gx = cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3)
    gate = np.clip(np.hypot(gx, gy) * 2.0, 0.0, 1.0)[..., None]
    return premul * (1.0 - strength * gate)


def _line_alpha_edge(premul: np.ndarray, alpha: np.ndarray, strength: float) -> np.ndarray:
    gx = cv2.Sobel(alpha[..., 0], cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(alpha[..., 0], cv2.CV_32F, 0, 1, ksize=3)
    gate = np.clip(np.hypot(gx, gy) * 2.0, 0.0, 1.0)[..., None]
    return premul * (1.0 - strength * gate)


def _tone_posterize(premul: np.ndarray, alpha: np.ndarray, levels: int) -> tuple[np.ndarray, np.ndarray]:
    q = np.round(premul * (levels - 1)) / (levels - 1)
    qa = np.round(alpha * (levels - 1)) / (levels - 1)
    return q, qa


def _tone_bayer(premul: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    h, w = premul.shape[:2]
    threshold = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
    bit = (_luma_visual(premul, alpha) >= threshold)[..., None].astype(np.float32)
    return premul * bit


def _grain(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    noise = cv2.resize(rng.random((max(2, h // 16), max(2, w // 16))).astype(np.float32), (w, h),
                       interpolation=cv2.INTER_CUBIC)
    if noise.ndim == 2:
        noise = noise[..., None]
    n = noise[..., 0] - noise[..., 0].mean()
    return n / (n.std() + 1e-6)


def _texture_grain(premul: np.ndarray, seed: int) -> np.ndarray:
    n = _grain(premul.shape[0], premul.shape[1], seed)
    return premul * (1.0 + 0.08 * n)[..., None]


def _texture_hatch(premul: np.ndarray, seed: int) -> np.ndarray:
    h, w = premul.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    angle = 0.6 + 0.2 * (seed % 3)
    phase = xx * np.cos(angle) + yy * np.sin(angle)
    pattern = 0.5 + 0.5 * np.cos(phase * (2.0 * np.pi / max(4.0, w / 48.0)))
    return premul * (0.78 + 0.22 * pattern)[..., None]


def apply_candidate(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a pencil/print candidate spec to a float32 RGBA buffer."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")
    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = src[..., :3] * alpha if descriptor == "straight" else src[..., :3].copy()
    original_p = premul.copy()
    original_a = alpha.copy()

    line = spec.get("line")
    if line == "sobel":
        premul = _line_sobel(premul, alpha, float(spec.get("line_strength", 0.8)))
    elif line == "alpha_edge":
        premul = _line_alpha_edge(premul, alpha, float(spec.get("line_strength", 0.8)))
    elif line is not None:
        raise ValueError(f"unknown line component: {line}")

    tone = spec.get("tone")
    if tone == "posterize":
        premul, alpha = _tone_posterize(premul, alpha, int(spec.get("tone_levels", 6)))
    elif tone == "bayer":
        premul = _tone_bayer(premul, alpha)
    elif tone is not None:
        raise ValueError(f"unknown tone component: {tone}")

    texture = spec.get("texture")
    if texture == "grain":
        premul = _texture_grain(premul, seed)
    elif texture == "hatch":
        premul = _texture_hatch(premul, seed)
    elif texture is not None:
        raise ValueError(f"unknown texture component: {texture}")

    strength = float(spec.get("strength", 1.0))
    premul = original_p + strength * (premul - original_p)
    alpha = original_a + strength * (alpha - original_a)
    premul = np.maximum(premul, 0.0)
    alpha = np.clip(alpha, 0.0, 1.0)
    straight = premul / np.maximum(alpha, 1e-6)

    out = np.empty_like(src)
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = alpha
    return out
