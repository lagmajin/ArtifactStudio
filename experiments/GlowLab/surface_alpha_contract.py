"""Small SDR reference helpers for the Artifact surface alpha contract."""

from __future__ import annotations

import numpy as np

from glow_lab import linear_to_srgb, srgb_to_linear


def premultiply_linear(rgba_straight_srgb: np.ndarray) -> np.ndarray:
    """Convert straight sRGB RGBA to a bounded linear-premultiplied surface."""
    alpha = np.clip(rgba_straight_srgb[..., 3:4], 0.0, 1.0)
    rgb_linear = srgb_to_linear(np.maximum(rgba_straight_srgb[..., :3], 0.0))
    return np.concatenate((rgb_linear * alpha, alpha), axis=-1)


def composite_linear_premultiplied(
    premultiplied_linear: np.ndarray, background_srgb: np.ndarray
) -> np.ndarray:
    """Composite a linear-premultiplied SDR surface over an sRGB background."""
    alpha = np.clip(premultiplied_linear[..., 3:4], 0.0, 1.0)
    background_linear = srgb_to_linear(np.maximum(background_srgb, 0.0))
    output_linear = premultiplied_linear[..., :3] + background_linear * (1.0 - alpha)
    output = np.clip(linear_to_srgb(np.maximum(output_linear, 0.0)), 0.0, 1.0)
    return np.dstack((output, np.ones_like(alpha[..., 0])))


def composite_straight_misread_as_premultiplied(
    rgba_straight_srgb: np.ndarray, background_srgb: np.ndarray
) -> np.ndarray:
    """Model the bug where straight RGB is tagged/bound as premultiplied."""
    alpha = np.clip(rgba_straight_srgb[..., 3:4], 0.0, 1.0)
    background_linear = srgb_to_linear(np.maximum(background_srgb, 0.0))
    # Deliberately omit multiplication by alpha to visualize the contract error.
    output_linear = srgb_to_linear(np.maximum(rgba_straight_srgb[..., :3], 0.0))
    output_linear += background_linear * (1.0 - alpha)
    output = np.clip(linear_to_srgb(np.maximum(output_linear, 0.0)), 0.0, 1.0)
    return np.dstack((output, np.ones_like(alpha[..., 0])))


def premultiplied_contract_error(rgba_straight_srgb: np.ndarray) -> float:
    """Return the max RGB <= alpha violation after correct SDR premultiplication."""
    surface = premultiply_linear(rgba_straight_srgb)
    violation = np.maximum(surface[..., :3] - surface[..., 3:4], 0.0)
    return float(violation.max(initial=0.0))
