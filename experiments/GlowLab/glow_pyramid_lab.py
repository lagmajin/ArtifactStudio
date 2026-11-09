"""Compare full-resolution glow blur with a downsample/blur/upsample pyramid."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np

from glow_lab import (add_label, linear_to_srgb, make_fixture,
                      srgb_to_linear, to_u8)


WEIGHTS = (0.48, 0.30, 0.15, 0.07)
SCALES = (0.72, 1.8, 4.2, 9.0)


def bright_seed(rgba: np.ndarray, threshold: float = 0.45,
                knee: float = 0.30) -> tuple[np.ndarray, np.ndarray]:
    source = srgb_to_linear(np.maximum(rgba[..., :3], 0.0))
    luma = source[..., 0] * 0.2126 + source[..., 1] * 0.7152 + source[..., 2] * 0.0722
    soft = np.clip((luma - threshold + max(knee, 1.0e-5)) /
                   (2.0 * max(knee, 1.0e-5)), 0.0, 1.0)
    soft = soft * soft * (3.0 - 2.0 * soft)
    return source * soft[..., None], source


def full_resolution_reference(seed: np.ndarray, radius: float) -> np.ndarray:
    bloom = np.zeros_like(seed)
    for weight, scale in zip(WEIGHTS, SCALES):
        sigma = max(0.35, radius * scale)
        blurred = cv2.GaussianBlur(seed, (0, 0), sigmaX=sigma, sigmaY=sigma,
                                  borderType=cv2.BORDER_REPLICATE)
        bloom += blurred * weight
    return bloom


def pyramid_bloom(seed: np.ndarray, radius: float) -> tuple[np.ndarray, list[np.ndarray]]:
    """Keep the tight scale full-res; use successively smaller blur planes."""
    h, w = seed.shape[:2]
    bloom = np.zeros_like(seed)
    levels: list[np.ndarray] = []
    source_level = seed
    for level, (weight, scale) in enumerate(zip(WEIGHTS, SCALES)):
        if level == 0:
            sigma = max(0.35, radius * scale)
            blurred = cv2.GaussianBlur(source_level, (0, 0), sigmaX=sigma, sigmaY=sigma,
                                       borderType=cv2.BORDER_REPLICATE)
        else:
            source_level = cv2.pyrDown(source_level)
            level_scale = 2 ** level
            sigma = max(0.35, radius * scale / level_scale)
            blurred_small = cv2.GaussianBlur(
                source_level, (0, 0), sigmaX=sigma, sigmaY=sigma,
                borderType=cv2.BORDER_REPLICATE)
            blurred = cv2.resize(blurred_small, (w, h), interpolation=cv2.INTER_LINEAR)
        levels.append(blurred)
        bloom += blurred * weight
    return bloom, levels


def render_pair(rgba: np.ndarray, radius: float = 8.0, amount: float = 1.45):
    seed, source = bright_seed(rgba)
    reference = full_resolution_reference(seed, radius)
    pyramid, _ = pyramid_bloom(seed, radius)
    tint = np.asarray((1.0, 0.92, 0.82), dtype=np.float32)[None, None, :]
    ref_output = np.clip(linear_to_srgb(source + reference * amount * tint), 0.0, 1.0)
    pyr_output = np.clip(linear_to_srgb(source + pyramid * amount * tint), 0.0, 1.0)
    return reference, pyramid, ref_output, pyr_output


def make_difference(reference: np.ndarray, candidate: np.ndarray, gain: float = 5.0) -> np.ndarray:
    error = np.abs(reference - candidate) * gain
    # Show a red/cyan split where the two bloom fields disagree.
    red = np.clip(error.max(axis=2), 0.0, 1.0)
    signed = np.clip((candidate - reference) * gain, -1.0, 1.0)
    green = np.clip(-signed.min(axis=2), 0.0, 1.0)
    blue = np.clip(signed.max(axis=2), 0.0, 1.0)
    return np.dstack((red, green, blue, np.ones_like(red)))


def run(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture = make_fixture()
    fields_ref, fields_pyr, ref, pyr = render_pair(fixture)
    rmse = float(np.sqrt(np.mean((fields_ref - fields_pyr) ** 2)))
    relative_energy_error = float(
        abs(float(fields_ref.sum()) - float(fields_pyr.sum())) /
        max(float(fields_ref.sum()), 1.0e-8)
    )
    max_field_delta = float(np.max(np.abs(fields_ref - fields_pyr)))
    assert np.isfinite(fields_pyr).all() and np.isfinite(pyr).all()
    assert rmse < 0.012, f"pyramid/full-resolution field RMSE too high: {rmse:.6f}"
    assert relative_energy_error < 0.025, f"pyramid energy drift too high: {relative_energy_error:.4%}"

    cv2.imwrite(str(output_dir / "glow-pyramid-source.png"), cv2.cvtColor(
        to_u8(fixture), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(output_dir / "glow-pyramid-fullres-reference.png"), cv2.cvtColor(
        to_u8(np.dstack((ref, fixture[..., 3]))), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(output_dir / "glow-pyramid-down-up.png"), cv2.cvtColor(
        to_u8(np.dstack((pyr, fixture[..., 3]))), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(output_dir / "glow-pyramid-difference.png"), cv2.cvtColor(
        to_u8(make_difference(fields_ref, fields_pyr)), cv2.COLOR_RGBA2BGRA))
    panels = [
        add_label(to_u8(fixture), "SOURCE"),
        add_label(to_u8(np.dstack((ref, fixture[..., 3]))), "FULL-RESOLUTION BLUR REFERENCE"),
        add_label(to_u8(np.dstack((pyr, fixture[..., 3]))), "DOWNSAMPLE / BLUR / UPSAMPLE"),
        add_label(to_u8(make_difference(fields_ref, fields_pyr)), "ABSOLUTE FIELD DIFFERENCE x5"),
    ]
    panels = [cv2.resize(p, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA) for p in panels]
    cv2.imwrite(str(output_dir / "glow-pyramid-comparison.png"), cv2.cvtColor(
        np.vstack(panels), cv2.COLOR_RGBA2BGRA))

    print(f"Saved pyramid captures to {output_dir.resolve()}")
    print(f"field RMSE={rmse:.7f}; relative energy error={relative_energy_error:.4%}; "
          f"max field delta={max_field_delta:.6f}")
    for height, width in ((540, 960), (359, 641), (180, 320)):
        scaled = cv2.resize(fixture, (width, height), interpolation=cv2.INTER_AREA)
        scale = height / 540.0
        ref_small, pyr_small, _, _ = render_pair(scaled, radius=8.0 * scale)
        small_rmse = float(np.sqrt(np.mean((ref_small - pyr_small) ** 2)))
        assert np.isfinite(pyr_small).all()
        print(f"{width}x{height}: field RMSE={small_rmse:.7f}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "glow-pyramid")
    run(parser.parse_args().output)
