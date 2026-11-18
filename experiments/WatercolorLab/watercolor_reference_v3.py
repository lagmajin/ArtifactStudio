"""RD4 reference model: bleed components also transform alpha with the same kernel.

RD3 found that v2 still diverged near transparent pixels when a bleed
component was active: bilateral/quantize_gauss moved premultiplied RGB across
alpha boundaries while alpha itself was left untouched, so the final
unpremultiply divided a moved premul value by ~0.

v3 keeps the v2 spatial policy for paper/edge, and changes both bleed paths:
  * bilateral -> a joint 4-channel bilateral filter over premul RGB + alpha,
    so every output channel receives exactly the same normalized weights;
  * quantize_gauss -> quantize premul and alpha on the same 12-level grid and
    blur both with the same Gaussian kernel.

The final unpremultiply is unchanged. Absolute recovered-RGB values (visible
and near-transparent), not just straight-vs-premultiplied deltas, must be
inspected to judge whether this actually removes the divergence.
"""

from __future__ import annotations

import cv2
import numpy as np

from watercolor_reference_v2 import (
    _as_alpha,
    _edge_gate_from_premul,
    _edge_weighted_premul,
    _paper_fibers,
    _paper_value,
    _softstep_premul,
    _wobble_premul,
)


def _joint_bilateral(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Bilateral filter over the 4-channel joint image, with one shared weight map.

    OpenCV's bilateralFilter does not accept 4-channel float32, so the kernel
    is accumulated one shift at a time. All four channels use exactly the same
    normalized weights, which is the property RD3 showed was missing in v2.
    """
    joint = np.concatenate([premul, alpha], axis=2)
    h, w = joint.shape[:2]
    r = 3
    padded = np.pad(joint, ((r, r), (r, r), (0, 0)), mode="reflect")
    sigma_color = 0.08
    sigma_space = 3.0

    acc = np.zeros_like(joint)
    wsum = np.zeros((h, w, 1), dtype=np.float32)
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            neighbor = padded[r + dy : r + dy + h, r + dx : r + dx + w]
            diff = neighbor - joint
            color_w = np.exp(-0.5 * np.sum(diff * diff, axis=2) / (sigma_color * sigma_color))
            space_w = float(np.exp(-0.5 * (dy * dy + dx * dx) / (sigma_space * sigma_space)))
            weight = (color_w * space_w)[..., None]
            acc += neighbor * weight
            wsum += weight
    filtered = acc / np.maximum(wsum, 1e-12)
    return filtered[..., :3], _as_alpha(filtered[..., 3])


def _quantize_gauss_pair(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    levels = 12
    qp = np.round(premul * (levels - 1)) / (levels - 1)
    qa = np.round(alpha * (levels - 1)) / (levels - 1)
    sigma = 2.5
    blurred_p = cv2.GaussianBlur(qp, (0, 0), sigmaX=sigma, sigmaY=sigma, borderType=cv2.BORDER_REFLECT)
    blurred_a = cv2.GaussianBlur(qa, (0, 0), sigmaX=sigma, sigmaY=sigma, borderType=cv2.BORDER_REFLECT)
    return blurred_p, _as_alpha(blurred_a)


def apply_candidate_v3(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a candidate spec with alpha-consistent bleed components."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")

    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = src[..., :3] * alpha if descriptor == "straight" else src[..., :3].copy()

    original_p = premul.copy()
    original_a = alpha.copy()

    bleed = spec.get("bleed")
    if bleed == "bilateral":
        premul, alpha = _joint_bilateral(premul, alpha)
    elif bleed == "quantize_gauss":
        premul, alpha = _quantize_gauss_pair(premul, alpha)
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
    out[..., :3] = premul if descriptor == "premultiplied" else straight
    out[..., 3:4] = alpha
    return out
