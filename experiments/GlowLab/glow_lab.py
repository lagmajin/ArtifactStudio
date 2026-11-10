"""Deterministic still-image R&D harness for glow looks.

This deliberately lives outside the application build. It produces PNG captures
and checks image-space invariants so candidate looks can be compared before
being ported into ArtifactCore and Artifact.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np


def make_fixture(width: int = 960, height: int = 540) -> np.ndarray:
    """Return an opaque, deterministic neon still in float32 sRGB RGBA."""
    image = np.zeros((height, width, 4), dtype=np.float32)
    y, x = np.mgrid[0:height, 0:width]
    vignette = np.clip(1.0 - 0.22 * (((x - width * 0.5) / (width * 0.8)) ** 2 +
                                     ((y - height * 0.5) / (height * 0.8)) ** 2), 0.0, 1.0)
    image[..., 0] = 0.012 * vignette
    image[..., 1] = 0.009 * vignette
    image[..., 2] = 0.025 * vignette
    image[..., 3] = 1.0

    # Colored luminous shapes: saturated cyan tube, magenta ring, warm point lights.
    cv2.line(image, (115, 340), (835, 340), (0.92, 0.48, 0.10, 1.0), 10, cv2.LINE_AA)
    cv2.circle(image, (260, 205), 74, (0.95, 0.16, 0.68, 1.0), 9, cv2.LINE_AA)
    cv2.circle(image, (260, 205), 8, (1.0, 0.95, 0.84, 1.0), -1, cv2.LINE_AA)
    cv2.circle(image, (700, 190), 13, (0.28, 0.92, 1.0, 1.0), -1, cv2.LINE_AA)
    cv2.circle(image, (760, 265), 5, (1.0, 0.55, 0.18, 1.0), -1, cv2.LINE_AA)
    cv2.putText(image, "GLOW", (365, 235), cv2.FONT_HERSHEY_DUPLEX, 1.35,
                (0.64, 0.95, 1.0, 1.0), 3, cv2.LINE_AA)
    cv2.putText(image, "IMAGE LAB  /  01", (365, 275), cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                (0.48, 0.56, 0.70, 1.0), 1, cv2.LINE_AA)
    return np.clip(image, 0.0, 1.0)


def srgb_to_linear(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.04045, value / 12.92, ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value: np.ndarray) -> np.ndarray:
    value = np.maximum(value, 0.0)
    return np.where(value <= 0.0031308, value * 12.92, 1.055 * np.power(value, 1.0 / 2.4) - 0.055)


def current_look(rgba: np.ndarray, gain: float = 1.0, radius: float = 7.0) -> np.ndarray:
    """Approximate current fixed-threshold, stacked-Gaussian Glow behavior."""
    rgb = rgba[..., :3]
    luminance = rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722
    mask = np.clip((luminance - 0.60) * 2.5, 0.0, 1.0)
    seed = rgb * mask[..., None]
    accum = np.zeros_like(rgb)
    weights = (0.50, 0.30, 0.20)
    for index, weight in enumerate(weights):
        sigma = radius * (1.0 + index * 0.9)
        blurred = cv2.GaussianBlur(seed, (0, 0), sigmaX=sigma, sigmaY=sigma,
                                  borderType=cv2.BORDER_REPLICATE)
        accum += blurred * weight
    return np.dstack((np.clip(rgb + accum * gain, 0.0, 1.0), rgba[..., 3]))


def soft_knee_glow(rgba: np.ndarray, threshold: float = 0.45, knee: float = 0.30,
                   amount: float = 1.45, radius: float = 8.0, tint: tuple[float, float, float] =
                   (1.0, 0.92, 0.82)) -> np.ndarray:
    """Linear-light, soft-knee, wide multi-scale glow with source alpha preserved."""
    source = srgb_to_linear(np.maximum(rgba[..., :3], 0.0))
    luminance = source[..., 0] * 0.2126 + source[..., 1] * 0.7152 + source[..., 2] * 0.0722
    knee = max(knee, 1.0e-5)
    soft = np.clip((luminance - threshold + knee) / (2.0 * knee), 0.0, 1.0)
    soft = soft * soft * (3.0 - 2.0 * soft)
    contribution = source * soft[..., None]

    # Distinct spatial scales retain a crisp near halo while adding a broad bloom.
    scales = ((0.48, 0.72), (0.30, 1.8), (0.15, 4.2), (0.07, 9.0))
    bloom = np.zeros_like(source)
    for weight, radius_scale in scales:
        sigma = max(0.35, radius * radius_scale)
        bloom += cv2.GaussianBlur(contribution, (0, 0), sigmaX=sigma, sigmaY=sigma,
                                  borderType=cv2.BORDER_REPLICATE) * weight
    bloom *= np.asarray(tint, dtype=np.float32)[None, None, :]
    result = linear_to_srgb(source + bloom * amount)
    return np.dstack((np.clip(result, 0.0, 1.0), rgba[..., 3]))


def to_u8(rgba: np.ndarray) -> np.ndarray:
    return np.round(np.clip(rgba, 0.0, 1.0) * 255.0).astype(np.uint8)


def add_label(image: np.ndarray, label: str) -> np.ndarray:
    output = image.copy()
    cv2.rectangle(output, (0, 0), (output.shape[1], 42), (7, 9, 15, 255), -1)
    cv2.putText(output, label, (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                (230, 235, 245, 255), 1, cv2.LINE_AA)
    return output


def run(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture = make_fixture()
    current = current_look(fixture)
    candidate = soft_knee_glow(fixture)
    assert np.array_equal(current[..., 3], fixture[..., 3]), "current alpha changed"
    assert np.array_equal(candidate[..., 3], fixture[..., 3]), "candidate alpha changed"
    assert np.isfinite(candidate).all(), "candidate produced non-finite pixels"
    assert np.max(np.abs(soft_knee_glow(fixture, amount=0.0)[..., :3] - fixture[..., :3])) < 2e-6
    assert float(candidate[..., :3].mean()) > float(fixture[..., :3].mean())

    cv2.imwrite(str(output_dir / "fixture.png"), cv2.cvtColor(to_u8(fixture), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(output_dir / "current-stacked-gaussian.png"),
                cv2.cvtColor(to_u8(current), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(output_dir / "candidate-soft-knee-multiscale.png"),
                cv2.cvtColor(to_u8(candidate), cv2.COLOR_RGBA2BGRA))
    scale = 0.5
    panels = [add_label(to_u8(fixture), "SOURCE"),
              add_label(to_u8(current), "CURRENT: FIXED THRESHOLD / STACKED GAUSSIANS"),
              add_label(to_u8(candidate), "R&D: LINEAR / SOFT KNEE / MULTI-SCALE")]
    panels = [cv2.resize(panel, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA) for panel in panels]
    sheet = np.vstack(panels)
    cv2.imwrite(str(output_dir / "comparison.png"), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))
    print(f"Saved deterministic glow captures to {output_dir.resolve()}")
    print(f"mean RGB: source={fixture[..., :3].mean():.5f}, current={current[..., :3].mean():.5f}, "
          f"candidate={candidate[..., :3].mean():.5f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures")
    run(parser.parse_args().output)
