"""RD6 reference model: fast shared-weight bilateral approximation.

v3 used a hand-rolled 4-channel joint bilateral: exact shared weights for
premul RGB and alpha, but 19-25x slower than v2. v4 keeps the property that
matters for alpha consistency -- ONE normalized weight map applied to all four
channels -- while replacing the per-pixel pairwise color-distance kernel with
a local-energy gate plus separable Gaussian normalization. Cost becomes
O(h*w) with OpenCV separable blurs instead of 49 shifted full-frame passes.

Everything else (paper, edge, quantize bleed, final unpremultiply) is v3.
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
from watercolor_reference_v3 import _quantize_gauss_pair


def _fast_shared_filter(joint: np.ndarray, k: int = 7, sigma_space: float = 2.0,
                        sigma_color: float = 0.22) -> np.ndarray:
    """Approximate joint bilateral with a shared local-energy gate.

    The gate is computed once from the 4-channel joint image. The same gate,
    after Gaussian smoothing, weights premul RGB and alpha through identical
    separable convolutions and an identical denominator.
    """
    mean = cv2.blur(joint, (k, k), borderType=cv2.BORDER_REFLECT)
    sq = cv2.blur(joint * joint, (k, k), borderType=cv2.BORDER_REFLECT)
    variance = np.maximum(sq - mean * mean, 0.0)
    energy = np.sqrt(variance.sum(axis=2) / joint.shape[2])
    gate = np.exp(-0.5 * (energy / sigma_color) ** 2).astype(np.float32)

    spatial = cv2.GaussianBlur(gate, (0, 0), sigmaX=sigma_space, sigmaY=sigma_space,
                               borderType=cv2.BORDER_REFLECT)
    numerator = cv2.GaussianBlur(joint * spatial[..., None], (0, 0),
                                 sigmaX=sigma_space, sigmaY=sigma_space, borderType=cv2.BORDER_REFLECT)
    denominator = cv2.GaussianBlur(spatial, (0, 0), sigmaX=sigma_space, sigmaY=sigma_space,
                                   borderType=cv2.BORDER_REFLECT)
    return numerator / np.maximum(denominator[..., None], 1e-8)


def _fast_bilateral_pair(premul: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    joint = np.concatenate([premul, alpha], axis=2)
    filtered = _fast_shared_filter(joint)
    return filtered[..., :3], _as_alpha(filtered[..., 3])


def apply_candidate_v4(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    """Apply a candidate spec with the fast shared-weight bleed filter."""
    if descriptor not in ("straight", "premultiplied"):
        raise ValueError(f"unknown descriptor: {descriptor}")

    src = np.array(rgba, dtype=np.float32, copy=True)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = src[..., :3] * alpha if descriptor == "straight" else src[..., :3].copy()

    original_p = premul.copy()
    original_a = alpha.copy()

    bleed = spec.get("bleed")
    if bleed == "bilateral":
        premul, alpha = _fast_bilateral_pair(premul, alpha)
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
