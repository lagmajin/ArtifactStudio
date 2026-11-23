"""Reproduce the Gaussian CPU kernel call for a narrowly scoped R&D model.

This is not an Artifact capture: it mirrors the OpenCV call and kernel-size
formula from Artifact/src/Effects/Blur/GauusianBlur.cpp using Python OpenCV.
It does not exercise ImageF32x4_RGBA, effect wrappers, interactive scaling,
or Artifact's color descriptor conversion.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_cpu_source_mirror_2026-11-12.json"
SIGMAS = (0.5, 1.0, 1.5, 3.0, 8.0, 16.0)
SIZE = 513


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def kernel_size(sigma: float) -> int:
    size = max(1, int(6.0 * sigma + 1.0))
    return size + (size % 2 == 0)


def impulse_fixture(x: int) -> np.ndarray:
    pixels = np.zeros((SIZE, SIZE, 4), dtype=np.float32)
    pixels[:, :, 3] = 1.0
    pixels[SIZE // 2, x, 2] = 1.0  # Logical red in physical BGRA backing.
    return pixels


def describe(sigma: float) -> dict[str, float | int]:
    size = kernel_size(sigma)
    kernel = cv2.getGaussianKernel(size, sigma, cv2.CV_64F)[:, 0]
    kernel_positions = np.arange(size, dtype=np.float64) - size // 2
    kernel_mass = float(kernel.sum())
    sigma_effective = float(
        np.sqrt(np.sum(kernel * kernel_positions**2) / kernel_mass)
    )
    radius_95 = int(
        np.flatnonzero(np.cumsum(kernel) / kernel_mass >= 0.975)[0] - size // 2
    )

    center_output = cv2.GaussianBlur(
        impulse_fixture(SIZE // 2), (size, size), sigmaX=sigma
    )
    border_output = cv2.GaussianBlur(impulse_fixture(0), (size, size), sigmaX=sigma)
    profile = center_output[SIZE // 2, :, 2].astype(np.float64)
    x = np.arange(SIZE, dtype=np.float64)
    profile_mass = float(profile.sum())
    profile_centroid = float(np.sum(profile * x) / profile_mass)
    profile_variance = float(np.sum(profile * (x - profile_centroid) ** 2) / profile_mass)

    center_energy = float(center_output[:, :, 2].sum(dtype=np.float64))
    border_energy = float(border_output[:, :, 2].sum(dtype=np.float64))
    return {
        "requested_sigma_px": sigma,
        "kernel_width_px": size,
        "kernel_mass": kernel_mass,
        "kernel_effective_sigma_px": sigma_effective,
        "kernel_effective_sigma_relative_error": sigma_effective / sigma - 1.0,
        "one_dimensional_r95_px": radius_95,
        "center_impulse_energy": center_energy,
        "center_profile_sigma_px": float(np.sqrt(profile_variance)),
        "center_impulse_peak": float(center_output[SIZE // 2, SIZE // 2, 2]),
        "border_impulse_energy": border_energy,
        "border_impulse_energy_delta": border_energy - 1.0,
        "border_impulse_peak": float(border_output[SIZE // 2, 0, 2]),
    }


def small_sigma_quantization_study() -> dict[str, object]:
    """Measure effective-spread jumps caused by the source kernel-width rule."""
    def source_kernel_width(sigma: float) -> int:
        applied = np.float32(sigma)
        width = max(1, int(np.float32(np.float32(6.0) * applied + np.float32(1.0))))
        return width + (width % 2 == 0)

    def boundary_record(sigma: float) -> dict[str, float | int]:
        width = source_kernel_width(sigma)
        # C++ passes float sigma to OpenCV; the Python binding receives the
        # same binary32 value promoted exactly to double.
        return describe_kernel_only(float(np.float32(sigma)), width)

    def line_contrast_retention(sigma: float, period_px: int = 2) -> float:
        width = source_kernel_width(sigma)
        source = np.zeros((1, 257, 4), dtype=np.float32)
        source[:, :, 3] = 1.0
        source[:, ::period_px, :3] = 1.0
        result = cv2.GaussianBlur(
            source, (width, 1), sigmaX=float(np.float32(sigma)), sigmaY=0.0
        )[0, :, 0]
        interior = result[32:-32]
        return float(interior.max() - interior.min())

    def edge_spread_10_90(sigma: float, low: float = 0.0, high: float = 1.0) -> float:
        width = source_kernel_width(sigma)
        source = np.zeros((1, 513), dtype=np.float32)
        source[:, :] = low
        source[:, 256:] = high
        profile = cv2.GaussianBlur(
            source, (width, 1), sigmaX=float(np.float32(sigma)), sigmaY=0.0
        )[0]
        amplitude = high - low

        def crossing(fraction: float) -> float:
            level = low + amplitude * fraction
            idx = int(np.flatnonzero(profile >= level)[0])
            if idx == 0:
                return 0.0
            profile_low, profile_high = float(profile[idx - 1]), float(profile[idx])
            return (idx - 1) + (level - profile_low) / (profile_high - profile_low) if profile_high != profile_low else float(idx)

        return crossing(0.9) - crossing(0.1)

    samples = []
    for i in range(1, 3001):
        source_sigma = float(np.float32(i / 1000.0))
        record = boundary_record(source_sigma)
        # Assert that the mirror's common formula remains aligned with the
        # source-expression float32 approximation before recording it.
        assert record["kernel_width_px"] == source_kernel_width(source_sigma)
        samples.append(record)

    transitions = []
    for n in range(1, 19):
        nominal = n / 6.0
        target_width = (max(1, n + 1) + ((n + 1) % 2 == 0))
        lo = np.float32(max(0.0, nominal - 0.01))
        hi = np.float32(nominal + 0.01)
        if source_kernel_width(float(hi)) < target_width:
            continue
        # Binary-search ordered positive float32 bit patterns to find the
        # first representable applied sigma yielding the target width.
        lo_bits = int(np.asarray(lo).view(np.uint32))
        hi_bits = int(np.asarray(hi).view(np.uint32))
        while lo_bits + 1 < hi_bits:
            mid_bits = (lo_bits + hi_bits) // 2
            mid = np.asarray(mid_bits, dtype=np.uint32).view(np.float32).item()
            if source_kernel_width(float(mid)) >= target_width:
                hi_bits = mid_bits
            else:
                lo_bits = mid_bits
        after_value = float(np.asarray(hi_bits, dtype=np.uint32).view(np.float32).item())
        previous_value = float(np.nextafter(np.float32(after_value), np.float32(-np.inf)))
        if source_kernel_width(previous_value) == source_kernel_width(after_value):
            continue
        prev_record = boundary_record(previous_value)
        next_record = boundary_record(after_value)
        transitions.append({
            "nominal_sigma_px": nominal,
            "nearest_float32_to_nominal_sigma_px": float(np.float32(nominal)),
            "nearest_float32_kernel_width_px": source_kernel_width(float(np.float32(nominal))),
            "previous_requested_sigma_px": previous_value,
            "previous_kernel_width_px": prev_record["kernel_width_px"],
            "previous_kernel_effective_sigma_px": prev_record["kernel_effective_sigma_px"],
            "requested_sigma_px": after_value,
            "kernel_width_px": next_record["kernel_width_px"],
            "kernel_effective_sigma_px": next_record["kernel_effective_sigma_px"],
            "previous_two_pixel_line_contrast_retention": line_contrast_retention(previous_value),
            "two_pixel_line_contrast_retention": line_contrast_retention(after_value),
        })
    max_relative_error = max(
        (item for item in samples if item["requested_sigma_px"] >= 0.5),
        key=lambda item: abs(item["kernel_effective_sigma_relative_error"]),
    )
    property_step_samples = []
    for requested in np.arange(0.0, 0.51, 0.1):
        value = float(np.float32(requested))
        width = source_kernel_width(value)
        record = boundary_record(value)
        record["two_pixel_line_contrast_retention"] = line_contrast_retention(value)
        record["step_edge_spread_10_90_px"] = edge_spread_10_90(value)
        record["contrast_edge_spread_10_90_px"] = {
            "full_range_0_to_1": edge_spread_10_90(value, 0.0, 1.0),
            "medium_range_0_25_to_0_75": edge_spread_10_90(value, 0.25, 0.75),
            "low_range_0_02_to_0_04": edge_spread_10_90(value, 0.02, 0.04),
        }
        property_step_samples.append(record)
    interactive_scale_samples = []
    for requested in np.arange(0.0, 0.51, 0.1):
        requested_sigma = np.float32(requested)
        for resolution_scale in (1.0, 0.5, 0.125):
            applied_sigma = np.float32(
                requested_sigma * np.float32(resolution_scale)
            )
            record = boundary_record(float(applied_sigma))
            record.update({
                "requested_sigma_px": float(requested_sigma),
                "resolution_scale": resolution_scale,
                "applied_sigma_px": float(applied_sigma),
                "two_pixel_line_contrast_retention": line_contrast_retention(float(applied_sigma)),
                "step_edge_spread_10_90_px": edge_spread_10_90(float(applied_sigma)),
            })
            interactive_scale_samples.append(record)
    return {
        "sigma_range_px": [samples[0]["requested_sigma_px"], samples[-1]["requested_sigma_px"]],
        "sample_step_px": 0.001,
        "transition_rule": "source float32 expression max(1,int(6*sigma+1)); increment even support to odd; transition probes use adjacent float32 inputs and binary32 multiply/add",
        "transitions": transitions,
        "property_step_samples_0_to_0_5": property_step_samples,
        "interactive_resolution_scale_samples": interactive_scale_samples,
        "maximum_absolute_relative_spread_error_sample": max_relative_error,
        "sub_first_transition_behavior": {
            "kernel_width_px": 1,
            "effective_sigma_px": 0.0,
            "range_px": [0.001, 1.0 / 6.0],
            "note": "The odd-width rule keeps a 1-tap identity kernel below the first transition; this is source-formula behavior, not a UX recommendation.",
        },
        "scope_limit": "Kernel statistics from OpenCV getGaussianKernel; no Artifact output or perceived-quality threshold.",
    }


def describe_kernel_only(sigma: float, size: int | None = None) -> dict[str, float | int | None]:
    if size is None:
        size = kernel_size(sigma)
    kernel = cv2.getGaussianKernel(size, sigma, cv2.CV_64F)[:, 0]
    positions = np.arange(size, dtype=np.float64) - size // 2
    effective = float(np.sqrt(np.sum(kernel * positions**2) / kernel.sum()))
    return {
        "requested_sigma_px": sigma,
        "kernel_width_px": size,
        "kernel_effective_sigma_px": effective,
        "kernel_effective_sigma_relative_error": effective / sigma - 1.0 if sigma > 0.0 else None,
    }


def apply_source_mirror(source: np.ndarray, sigma: float) -> np.ndarray:
    size = kernel_size(sigma)
    working = source.copy()
    working[:, :, :3] *= working[:, :, 3:4]
    output = cv2.GaussianBlur(working, (size, size), sigmaX=sigma)
    visible_alpha = output[:, :, 3] > 1.0e-5
    output[:, :, :3][visible_alpha] /= output[:, :, 3:4][visible_alpha]
    output[:, :, :3][~visible_alpha] = 0.0
    return output


def alpha_association_study(sigma: float = 3.0) -> dict[str, object]:
    size = kernel_size(sigma)
    measurements = {}
    for alpha_mode in ("Straight", "Premultiplied"):
        source = np.zeros((SIZE, SIZE, 4), dtype=np.float32)
        source[SIZE // 2, SIZE // 2, 3] = 0.5
        source_red = 0.8 if alpha_mode == "Straight" else 0.4
        source[SIZE // 2, SIZE // 2, 2] = source_red

        actual = apply_source_mirror(source, sigma)
        reference_input = source.copy()
        if alpha_mode == "Straight":
            reference_input[:, :, :3] *= reference_input[:, :, 3:4]
        expected = cv2.GaussianBlur(reference_input, (size, size), sigmaX=sigma)
        if alpha_mode == "Straight":
            valid = expected[:, :, 3] > 1.0e-5
            expected[:, :, :3][valid] /= expected[:, :, 3:4][valid]
            expected[:, :, :3][~valid] = 0.0

        actual_visible_red = actual[:, :, 2].astype(np.float64)
        expected_visible_red = expected[:, :, 2].astype(np.float64)
        if alpha_mode == "Straight":
            actual_visible_red *= actual[:, :, 3]
            expected_visible_red *= expected[:, :, 3]
        expected_energy = float(expected_visible_red.sum())
        actual_energy = float(actual_visible_red.sum())
        measurements[alpha_mode] = {
            "output_alpha_mode_preserved_by_wrapper": True,
            "actual_center_bgra": actual[SIZE // 2, SIZE // 2].astype(float).tolist(),
            "expected_center_bgra": expected[SIZE // 2, SIZE // 2].astype(float).tolist(),
            "actual_visible_red_energy": actual_energy,
            "expected_visible_red_energy": expected_energy,
            "visible_red_energy_ratio": actual_energy / expected_energy,
            "visible_red_max_abs_error": float(
                np.max(np.abs(actual_visible_red - expected_visible_red))
            ),
        }
    return {
        "sigma_px": sigma,
        "kernel_width_px": size,
        "fixture": "one 0.5-alpha red sample on transparent black; source red=0.8 straight or 0.4 premultiplied",
        "comparison": "descriptor-aware alpha-correct reference blur against source-mirror wrapper output, interpreted using preserved source alpha mode",
        "measurements": measurements,
        "scope_limit": "Potential only: production caller input alpha mode is not yet traced or captured.",
    }


def alpha_composite_study(sigma: float = 3.0) -> dict[str, object]:
    """Compare wrapper equations with descriptor-aware output over RGB backdrops."""
    size = kernel_size(sigma)
    center = SIZE // 2
    backgrounds = {
        "black": np.array([0.0, 0.0, 0.0], dtype=np.float32),
        "white": np.array([1.0, 1.0, 1.0], dtype=np.float32),
        "green": np.array([0.0, 1.0, 0.0], dtype=np.float32),
    }
    records: dict[str, object] = {}
    for alpha_mode in ("Straight", "Premultiplied"):
        source = np.zeros((SIZE, SIZE, 4), dtype=np.float32)
        source[center, center, 3] = 0.5
        source_rgb = np.array([0.8, 0.2, 0.1], dtype=np.float32)
        if alpha_mode == "Premultiplied":
            source_rgb *= 0.5
        source[center, center, :3] = source_rgb

        actual = apply_source_mirror(source, sigma)
        reference = source.copy()
        if alpha_mode == "Straight":
            reference[:, :, :3] *= reference[:, :, 3:4]
        reference[:, :, :3] = cv2.GaussianBlur(
            reference[:, :, :3], (size, size), sigmaX=sigma
        )
        reference[:, :, 3] = cv2.GaussianBlur(
            reference[:, :, 3], (size, size), sigmaX=sigma
        )
        if alpha_mode == "Straight":
            valid = reference[:, :, 3] > 1.0e-5
            reference[:, :, :3][valid] /= reference[:, :, 3:4][valid]
            reference[:, :, :3][~valid] = 0.0

        # The source descriptor is preserved by the wrapper. A compositor
        # therefore interprets output RGB as straight or premultiplied by mode.
        if alpha_mode == "Straight":
            actual_premult = actual[:, :, :3] * actual[:, :, 3:4]
        else:
            actual_premult = actual[:, :, :3]
        reference_premult = reference[:, :, :3] * reference[:, :, 3:4] \
            if alpha_mode == "Straight" else reference[:, :, :3]
        by_background = {}
        for name, background in backgrounds.items():
            actual_composite = actual_premult + background * (1.0 - actual[:, :, 3:4])
            reference_composite = reference_premult + background * (1.0 - reference[:, :, 3:4])
            diff = actual_composite.astype(np.float64) - reference_composite.astype(np.float64)
            by_background[name] = {
                "composite_rmse_rgb": float(np.sqrt(np.mean(diff**2))),
                "composite_max_abs_error_rgb": float(np.max(np.abs(diff))),
                "composite_mae_rgb": float(np.mean(np.abs(diff))),
            }
        records[alpha_mode] = {
            "descriptor_preserved": alpha_mode,
            "source_center_rgb_alpha": [*source_rgb.tolist(), 0.5],
            "output_alpha_max_abs_error_vs_reference": float(
                np.max(np.abs(actual[:, :, 3] - reference[:, :, 3]))
            ),
            "output_composited_rgb_errors": by_background,
        }

    # Hidden RGB invariance: change only RGB under zero alpha and compare visible
    # premultiplied output. This records whether the wrapper leaks hidden color.
    hidden_outputs = []
    for hidden in ((1.0, 0.0, 1.0), (0.0, 1.0, 1.0)):
        source = np.zeros((SIZE, SIZE, 4), dtype=np.float32)
        source[:, :, :3] = np.asarray(hidden, dtype=np.float32)
        source[center, center, :3] = (0.8, 0.2, 0.1)
        source[center, center, 3] = 0.5
        output = apply_source_mirror(source, sigma)
        hidden_outputs.append(output[:, :, :3] * output[:, :, 3:4])
    visible_difference = hidden_outputs[0] - hidden_outputs[1]
    return {
        "sigma_px": sigma,
        "kernel_width_px": size,
        "fixture": "one alpha=0.5 colored point on transparent pixels; hidden RGB variants are magenta and cyan",
        "composition_space": "linear RGB, premultiplied source-over, no tone map or quantization",
        "backgrounds": list(backgrounds),
        "measurements": records,
        "hidden_rgb_variant_visible_premultiplied_max_abs_difference": float(
            np.max(np.abs(visible_difference))
        ),
        "scope_limit": "CPU wrapper equation model only; not an Artifact capture or user-visible defect proof.",
    }


def alpha_edge_composite_study(sigma: float = 3.0) -> dict[str, object]:
    """Model a broad half-alpha edge and report edge ROI plus whole-frame error."""
    width, height = 257, 65
    edge_x = width // 2
    size = kernel_size(sigma)
    backgrounds = {
        "black": np.zeros(3, dtype=np.float32),
        "white": np.ones(3, dtype=np.float32),
        "green": np.array([0.0, 1.0, 0.0], dtype=np.float32),
    }
    records: dict[str, object] = {}
    for alpha_mode in ("Straight", "Premultiplied"):
        source = np.zeros((height, width, 4), dtype=np.float32)
        source[:, edge_x:, 3] = 0.5
        foreground = np.array([0.8, 0.2, 0.1], dtype=np.float32)
        source[:, edge_x:, :3] = foreground
        if alpha_mode == "Premultiplied":
            source[:, edge_x:, :3] *= 0.5

        actual_working = source.copy()
        actual_working[:, :, :3] *= actual_working[:, :, 3:4]
        actual = cv2.GaussianBlur(actual_working, (size, size), sigmaX=sigma)
        actual_alpha = actual[:, :, 3].copy()
        valid = actual_alpha > 1.0e-5
        actual[:, :, :3][valid] /= actual_alpha[valid, None]
        actual[:, :, :3][~valid] = 0.0

        reference = source.copy()
        if alpha_mode == "Straight":
            reference[:, :, :3] *= reference[:, :, 3:4]
        reference = cv2.GaussianBlur(reference, (size, size), sigmaX=sigma)
        if alpha_mode == "Straight":
            ref_valid = reference[:, :, 3] > 1.0e-5
            reference[:, :, :3][ref_valid] /= reference[:, :, 3][ref_valid, None]
            reference[:, :, :3][~ref_valid] = 0.0

        actual_premult = actual[:, :, :3] * actual[:, :, 3:4] \
            if alpha_mode == "Straight" else actual[:, :, :3]
        reference_premult = reference[:, :, :3] * reference[:, :, 3:4] \
            if alpha_mode == "Straight" else reference[:, :, :3]
        roi = slice(max(0, edge_x - int(np.ceil(3 * sigma)) - 2),
                    min(width, edge_x + int(np.ceil(3 * sigma)) + 3))
        by_background = {}
        for name, background in backgrounds.items():
            actual_composite = actual_premult + background * (1.0 - actual[:, :, 3:4])
            reference_composite = reference_premult + background * (1.0 - reference[:, :, 3:4])
            diff = (actual_composite - reference_composite).astype(np.float64)
            edge_diff = diff[:, roi, :]
            actual_display = np.clip(actual_composite, 0.0, 1.0)
            reference_display = np.clip(reference_composite, 0.0, 1.0)
            actual_display = np.where(
                actual_display <= 0.0031308,
                actual_display * 12.92,
                1.055 * np.power(actual_display, 1.0 / 2.4) - 0.055,
            )
            reference_display = np.where(
                reference_display <= 0.0031308,
                reference_display * 12.92,
                1.055 * np.power(reference_display, 1.0 / 2.4) - 0.055,
            )
            display_diff = (actual_display - reference_display).astype(np.float64)
            display_edge_diff = display_diff[:, roi, :]
            by_background[name] = {
                "whole_frame_rmse_rgb": float(np.sqrt(np.mean(diff**2))),
                "whole_frame_max_abs_error_rgb": float(np.max(np.abs(diff))),
                "edge_roi_rmse_rgb": float(np.sqrt(np.mean(edge_diff**2))),
                "edge_roi_max_abs_error_rgb": float(np.max(np.abs(edge_diff))),
                "display_srgb_whole_frame_rmse_code_normalized": float(
                    np.sqrt(np.mean(display_diff**2))
                ),
                "display_srgb_edge_roi_rmse_code_normalized": float(
                    np.sqrt(np.mean(display_edge_diff**2))
                ),
                "display_srgb_edge_roi_max_abs_error_code_normalized": float(
                    np.max(np.abs(display_edge_diff))
                ),
            }
        records[alpha_mode] = {
            "background_composite_errors": by_background,
            "output_alpha_max_abs_error": float(
                np.max(np.abs(actual[:, :, 3] - reference[:, :, 3]))
            ),
            "descriptor_association_diagnostics": {
                "mode": alpha_mode,
                "wrapper_pixels_with_any_rgb_above_alpha_1e-6": int(
                    np.count_nonzero(np.any(
                        actual[:, :, :3] > actual[:, :, 3:4] + 1.0e-6, axis=2
                    ))
                ) if alpha_mode == "Premultiplied" else 0,
                "reference_pixels_with_any_rgb_above_alpha_1e-6": int(
                    np.count_nonzero(np.any(
                        reference[:, :, :3] > reference[:, :, 3:4] + 1.0e-6, axis=2
                    ))
                ) if alpha_mode == "Premultiplied" else 0,
                "wrapper_max_rgb_minus_alpha": float(np.max(
                    actual[:, :, :3] - actual[:, :, 3:4]
                )) if alpha_mode == "Premultiplied" else 0.0,
                "reference_max_rgb_minus_alpha": float(np.max(
                    reference[:, :, :3] - reference[:, :, 3:4]
                )) if alpha_mode == "Premultiplied" else 0.0,
            },
        }
    return {
        "sigma_px": sigma,
        "fixture": {
            "dimensions": [width, height],
            "geometry": "vertical half-alpha foreground occupies x>=128 over transparent pixels",
            "edge_x": edge_x,
            "edge_roi_x": [int(max(0, edge_x - np.ceil(3 * sigma) - 2)),
                           int(min(width, edge_x + np.ceil(3 * sigma) + 2))],
            "foreground_linear_rgb": [0.8, 0.2, 0.1],
            "alpha": 0.5,
        },
        "measurements": records,
        "scope_limit": "CPU wrapper equation model, linear source-over only; not an Artifact capture or appearance acceptance.",
        "display_metric_contract": "Optional display-sRGB values use linear [0,1] clamp followed by the sRGB OETF; no tone mapping, quantization, or perceptual weighting.",
    }


def save_alpha_edge_preview(sigma: float = 3.0) -> dict[str, str]:
    """Save a display-only crop of modeled premultiplied output vs reference."""
    width, height = 257, 65
    edge_x = width // 2
    size = kernel_size(sigma)
    source = np.zeros((height, width, 4), dtype=np.float32)
    source[:, edge_x:, 3] = 0.5
    source[:, edge_x:, :3] = np.array([0.4, 0.1, 0.05], dtype=np.float32)
    working = source.copy()
    working[:, :, :3] *= working[:, :, 3:4]
    actual = cv2.GaussianBlur(working, (size, size), sigmaX=sigma)
    valid = actual[:, :, 3] > 1.0e-5
    actual[:, :, :3][valid] /= actual[:, :, 3][valid, None]
    actual[:, :, :3][~valid] = 0.0
    reference = cv2.GaussianBlur(working, (size, size), sigmaX=sigma)

    backgrounds = {
        "Black": np.zeros(3, dtype=np.float32),
        "White": np.ones(3, dtype=np.float32),
        "Green": np.array([0.0, 1.0, 0.0], dtype=np.float32),
    }
    def to_display(rgb: np.ndarray) -> np.ndarray:
        linear = np.clip(rgb, 0.0, 1.0)
        srgb = np.where(linear <= 0.0031308, linear * 12.92,
                        1.055 * np.power(linear, 1.0 / 2.4) - 0.055)
        return np.uint8(np.rint(np.clip(srgb, 0.0, 1.0) * 255.0))

    x0, x1 = edge_x - 10, edge_x + 11
    crop_scale = 5
    label_h = 30
    row_h = height * crop_scale + label_h
    canvas = Image.new("RGB", (2 * (x1 - x0) * crop_scale, 3 * row_h), (32, 32, 32))
    draw = ImageDraw.Draw(canvas)
    for row, (name, background) in enumerate(backgrounds.items()):
        draw.text((4, row * row_h + 2), name, fill=(240, 240, 240))
        actual_composite = actual[:, :, :3] + background * (1.0 - actual[:, :, 3:4])
        reference_composite = reference[:, :, :3] + background * (1.0 - reference[:, :, 3:4])
        for col, (caption, pixels) in enumerate((
            ("A", actual_composite),
            ("B", reference_composite),
        )):
            crop = to_display(pixels[:, x0:x1, :])
            image = Image.fromarray(crop, mode="RGB").resize(
                ((x1 - x0) * crop_scale, height * crop_scale), Image.Resampling.NEAREST
            )
            left = col * (x1 - x0) * crop_scale
            top = row * row_h + label_h
            canvas.paste(image, (left, top))
            draw.text((left + 4, row * row_h + 15), caption, fill=(240, 240, 240))
    path = ROOT / "experiments/blur-rd/results/previews/gaussian_alpha_edge_sigma3_premultiplied.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)
    return {
        "path": path.relative_to(ROOT).as_posix(),
        "sha256": sha256(path),
        "description": "display preview of CPU source-equation model vs alpha-aware reference; black/white/green backgrounds; 5x nearest magnification; not an Artifact capture",
    }


def alpha_unpremultiply_cutoff_study() -> dict[str, object]:
    """Quantify RGB energy discarded below the CPU wrapper's alpha threshold."""
    threshold = 1.0e-5
    dimensions = (65, 257, 513)
    sigmas = (0.5, 1.0, 3.0, 16.0, 64.0)
    cases = []
    for dimension in dimensions:
        for sigma in sigmas:
            size = kernel_size(sigma)
            source = np.zeros((dimension, dimension, 4), dtype=np.float32)
            source[dimension // 2, dimension // 2, 0] = 0.8
            source[dimension // 2, dimension // 2, 3] = 0.5
            premultiplied = source.copy()
            premultiplied[:, :, :3] *= premultiplied[:, :, 3:4]
            blurred = cv2.GaussianBlur(premultiplied, (size, size), sigmaX=sigma)
            alpha = blurred[:, :, 3]
            tail_mask = (alpha > 0.0) & (alpha <= threshold)
            retained_mask = alpha > threshold
            visible_reference = blurred[:, :, :3].astype(np.float64)
            total_visible_energy = float(visible_reference.sum())
            input_visible_energy = float(
                (source[:, :, :3] * source[:, :, 3:4]).sum(dtype=np.float64)
            )
            discarded_tail_energy = float(visible_reference[tail_mask].sum())
            cases.append({
                "dimensions": [dimension, dimension],
                "sigma_px": sigma,
                "kernel_width_px": size,
                "kernel_radius_px": size // 2,
                "kernel_support_fully_inside_image": size <= dimension,
                "threshold": threshold,
                "nonzero_alpha_pixels": int(np.count_nonzero(alpha > 0.0)),
                "pixels_above_threshold": int(np.count_nonzero(retained_mask)),
                "nonzero_pixels_at_or_below_threshold": int(np.count_nonzero(tail_mask)),
                "reference_visible_rgb_energy_total": total_visible_energy,
                "input_visible_rgb_energy_total": input_visible_energy,
                "output_energy_change_from_input": total_visible_energy - input_visible_energy,
                "visible_rgb_energy_zeroed_by_wrapper": discarded_tail_energy,
                "visible_rgb_energy_fraction_zeroed": (
                    discarded_tail_energy / total_visible_energy
                    if total_visible_energy > 0.0 else 0.0
                ),
                "maximum_alpha_in_zeroed_tail": float(
                    np.max(alpha[tail_mask]) if np.any(tail_mask) else 0.0
                ),
                "minimum_alpha_above_threshold": float(
                    np.min(alpha[retained_mask]) if np.any(retained_mask) else 0.0
                ),
            })
    return {
        "fixture": "single straight-alpha red point (RGB=0.8, alpha=0.5) on transparent black, centered; linear unclamped float32",
        "threshold_source": "GaussianBlurCPUImpl::applyCPU uses output alpha > 1e-5 before unpremultiplying RGB; otherwise it sets RGB=0",
        "measurement": "count nonzero alpha pixels at/below threshold and reference premultiplied RGB energy that the wrapper zeroes there",
        "cases": cases,
        "scope_limit": "Source-equation model; no actual Artifact image, descriptor conversion, or visual detectability claim.",
    }


def alpha_cutoff_position_study() -> dict[str, object]:
    """Separate Reflect101 finite-image energy from alpha-threshold RGB zeroing."""
    cases = []
    for dimension in (65, 257, 513):
        center = dimension // 2
        positions = {
            "center": (center, center),
            "edge_center": (0, center),
            "corner": (0, 0),
        }
        for sigma in (3.0, 16.0, 64.0):
            size = kernel_size(sigma)
            support_fits = size <= dimension
            for placement, (x, y) in positions.items():
                alpha = np.zeros((dimension, dimension), dtype=np.float32)
                premult_red = np.zeros_like(alpha)
                alpha[y, x] = 0.5
                premult_red[y, x] = 0.4
                output_alpha = cv2.GaussianBlur(alpha, (size, size), sigmaX=sigma)
                reference_red = cv2.GaussianBlur(
                    premult_red, (size, size), sigmaX=sigma
                )
                retained = output_alpha > 1.0e-5
                actual_visible_red = np.where(retained, reference_red, 0.0)
                reference_energy = float(reference_red.sum(dtype=np.float64))
                actual_energy = float(actual_visible_red.sum(dtype=np.float64))
                input_energy = 0.4
                threshold_sweep = []
                for threshold in (1.0e-7, 1.0e-6, 1.0e-5, 1.0e-4, 1.0e-3):
                    threshold_retained = output_alpha > threshold
                    threshold_output = np.where(threshold_retained, reference_red, 0.0)
                    threshold_energy = float(threshold_output.sum(dtype=np.float64))
                    zeroed_tail = (output_alpha > 0.0) & ~threshold_retained
                    threshold_sweep.append({
                        "threshold": threshold,
                        "visible_energy_after_threshold": threshold_energy,
                        "lost_energy": reference_energy - threshold_energy,
                        "lost_fraction_of_reference": (
                            (reference_energy - threshold_energy) / reference_energy
                            if reference_energy > 0.0 else 0.0
                        ),
                        "nonzero_alpha_pixels_zeroed": int(np.count_nonzero(zeroed_tail)),
                    })
                cases.append({
                    "dimensions_px": [dimension, dimension],
                    "sigma_px": sigma,
                    "kernel_width_px": size,
                    "kernel_support_fits_image": support_fits,
                    "placement": placement,
                    "position_xy": [x, y],
                    "alpha_threshold": 1.0e-5,
                    "reference_reflect101_energy": reference_energy,
                    "reflect101_energy_delta_from_input": reference_energy - input_energy,
                    "actual_after_cutoff_energy": actual_energy,
                    "cutoff_lost_energy": reference_energy - actual_energy,
                    "cutoff_lost_fraction_of_reference": (
                        (reference_energy - actual_energy) / reference_energy
                        if reference_energy > 0.0 else 0.0
                    ),
                    "nonzero_alpha_pixels_at_or_below_threshold": int(
                        np.count_nonzero((output_alpha > 0.0) & ~retained)
                    ),
                    "max_alpha_in_zeroed_region": float(
                        np.max(output_alpha[(output_alpha > 0.0) & ~retained])
                        if np.any((output_alpha > 0.0) & ~retained) else 0.0
                    ),
                    "threshold_sweep": threshold_sweep,
                })
    return {
        "fixture": "single straight-alpha red point on transparent black; RGB=.8, alpha=.5; linear unclamped float32",
        "placements": ["center", "edge_center", "corner"],
        "dimensions_px": [65, 257, 513],
        "sigma_values_px": [3.0, 16.0, 64.0],
        "measurement": "Reflect101 output energy before unpremultiply/cutoff versus visible premultiplied RGB after output-alpha threshold zeroing",
        "threshold_values": [1.0e-7, 1.0e-6, 1.0e-5, 1.0e-4, 1.0e-3],
        "cases": cases,
        "scope_limit": "CPU source-equation mirror only; no actual Artifact surface, descriptor semantics, display composite, or visual threshold.",
    }


def alpha_cutoff_input_alpha_sweep() -> dict[str, object]:
    """Measure fixed alpha-threshold sensitivity to source alpha association."""
    cases = []
    dimensions_list = (257, 513)
    sigmas = (8.0, 16.0, 32.0, 64.0)
    input_alphas = (0.01, 0.1, 0.5, 1.0)
    thresholds = (1.0e-7, 1.0e-6, 1.0e-5, 1.0e-4, 1.0e-3)
    for dimension in dimensions_list:
        center = dimension // 2
        positions = {
            "center": (center, center),
            "edge_center": (0, center),
            "corner": (0, 0),
        }
        for sigma in sigmas:
            size = kernel_size(sigma)
            for input_alpha in input_alphas:
                for placement, (x, y) in positions.items():
                    alpha = np.zeros((dimension, dimension), dtype=np.float32)
                    premult_red = np.zeros_like(alpha)
                    alpha[y, x] = np.float32(input_alpha)
                    premult_red[y, x] = np.float32(0.8 * input_alpha)
                    output_alpha = cv2.GaussianBlur(alpha, (size, size), sigmaX=sigma)
                    reference_red = cv2.GaussianBlur(
                        premult_red, (size, size), sigmaX=sigma
                    )
                    yy, xx = np.indices((dimension, dimension), dtype=np.float64)
                    radial_distance = np.sqrt((xx - x) ** 2 + (yy - y) ** 2)
                    radial_order = np.argsort(radial_distance.ravel(), kind="stable")
                    sorted_radii = radial_distance.ravel()[radial_order]

                    def energy_r95(profile: np.ndarray) -> float:
                        values = np.maximum(profile.astype(np.float64), 0.0).ravel()
                        total = float(values.sum())
                        if total <= 0.0:
                            return 0.0
                        cumulative = np.cumsum(values[radial_order])
                        index = int(np.searchsorted(cumulative, 0.95 * total, side="left"))
                        return float(sorted_radii[min(index, len(sorted_radii) - 1)])

                    reference_r95 = energy_r95(reference_red)
                    threshold_results = []
                    roi_radius = int(np.ceil(3.0 * sigma))
                    y0, y1 = max(0, y - roi_radius), min(dimension, y + roi_radius + 1)
                    x0, x1 = max(0, x - roi_radius), min(dimension, x + roi_radius + 1)
                    central_roi = np.zeros_like(output_alpha, dtype=bool)
                    central_roi[y0:y1, x0:x1] = True
                    for threshold in thresholds:
                        retained = output_alpha > threshold
                        actual = np.where(retained, reference_red, 0.0)
                        reference_energy = float(reference_red.sum(dtype=np.float64))
                        actual_energy = float(actual.sum(dtype=np.float64))
                        error = reference_red - actual
                        zeroed_tail = (output_alpha > 0.0) & ~retained
                        tail_radii = radial_distance[zeroed_tail]
                        if np.any(zeroed_tail):
                            tail_y, tail_x = np.nonzero(zeroed_tail)
                            tail_bbox = [int(tail_x.min()), int(tail_y.min()),
                                         int(tail_x.max()) + 1, int(tail_y.max()) + 1]
                        else:
                            tail_bbox = None
                        radial_loss_bands = {}
                        for band_name, lower, upper in (
                            ("within_1sigma", 0.0, sigma),
                            ("1_to_3sigma", sigma, 3.0 * sigma),
                            ("outside_3sigma", 3.0 * sigma, float("inf")),
                        ):
                            band_mask = zeroed_tail & (radial_distance > lower) & (radial_distance <= upper)
                            radial_loss_bands[band_name] = {
                                "zeroed_pixel_count": int(np.count_nonzero(band_mask)),
                                "lost_energy": float(np.maximum(reference_red[band_mask].astype(np.float64), 0.0).sum()),
                            }

                        def error_stats(mask: np.ndarray) -> dict[str, float | int]:
                            values = np.abs(error[mask]).astype(np.float64)
                            if values.size == 0:
                                return {"pixel_count": 0, "mae": 0.0, "p95_abs_error": 0.0, "max_abs_error": 0.0}
                            return {
                                "pixel_count": int(values.size),
                                "mae": float(values.mean()),
                                "p95_abs_error": float(np.quantile(values, 0.95)),
                                "max_abs_error": float(values.max()),
                            }

                        threshold_results.append({
                            "threshold": threshold,
                            "lost_fraction_of_reference": (
                                (reference_energy - actual_energy) / reference_energy
                                if reference_energy > 0.0 else 0.0
                            ),
                            "zeroed_nonzero_alpha_pixels": int(
                                np.count_nonzero((output_alpha > 0.0) & ~retained)
                            ),
                            "absolute_error_stats": {
                                "full_image": error_stats(np.ones_like(retained, dtype=bool)),
                                "central_psf_roi_xyxy": {
                                    "bounds": [x0, y0, x1, y1],
                                    **error_stats(central_roi),
                                },
                                "threshold_zeroed_tail": error_stats(zeroed_tail),
                            },
                            "spatial_extent": {
                                "reference_energy_r95_px": reference_r95,
                                "thresholded_output_energy_r95_px": energy_r95(actual),
                                "zeroed_tail_bbox_xyxy": tail_bbox,
                                "zeroed_tail_radius_min_px": float(tail_radii.min()) if tail_radii.size else None,
                                "zeroed_tail_radius_max_px": float(tail_radii.max()) if tail_radii.size else None,
                                "kernel_nominal_radius_px": size // 2,
                                "radial_zeroed_tail_breakdown": radial_loss_bands,
                            },
                        })
                    cases.append({
                        "dimensions_px": [dimension, dimension],
                        "sigma_px": sigma,
                        "kernel_width_px": size,
                        "kernel_support_fits_image": size <= dimension,
                        "input_alpha": input_alpha,
                        "input_visible_red_energy": 0.8 * input_alpha,
                        "placement": placement,
                        "position_xy": [x, y],
                        "reflect101_reference_energy": float(reference_red.sum(dtype=np.float64)),
                        "reference_energy_delta_from_input": float(
                            reference_red.sum(dtype=np.float64) - 0.8 * input_alpha
                        ),
                        "threshold_sweep": threshold_results,
                    })
    return {
        "fixture": "single straight-alpha red point (RGB=.8) on transparent black; linear unclamped float32",
        "dimensions_px": list(dimensions_list),
        "sigma_values_px": list(sigmas),
        "input_alpha_values": list(input_alphas),
        "placements": ["center", "edge_center", "corner"],
        "threshold_values": list(thresholds),
        "measurement": "Reflect101 premultiplied reference energy versus energy retained after candidate alpha cutoffs",
        "error_statistics": {
            "channel": "premultiplied red, unclamped linear float32 values",
            "absolute_error": "reference convolution minus thresholded output, absolute-valued for stats",
            "full_image": "MAE, p95, max over every image pixel, including unchanged zeros",
            "central_psf_roi": "square centered on source point with radius ceil(3*sigma), clipped to image bounds",
            "threshold_zeroed_tail": "nonzero output alpha at or below the tested threshold",
            "spatial_extent": "R95 from positive premultiplied red energy; Euclidean distance to impulse center; radial loss bands are within 1sigma, 1-3sigma, and outside 3sigma; separable kernel support remains square",
        },
        "cases": cases,
        "scope_limit": "CPU source-equation sensitivity model only; threshold alternatives are diagnostic, not recommendations or product output claims.",
    }


def alpha_cutoff_display_composite_study() -> dict[str, object]:
    """Evaluate the fixed CPU alpha cutoff in linear and display composites."""
    dimension = 513
    center = dimension // 2
    threshold = 1.0e-5
    sigmas = (16.0, 64.0)
    alphas = (0.01, 0.1, 0.5, 1.0)
    placements = {
        "center": (center, center),
        "edge_center": (0, center),
    }
    backgrounds = {
        "black": np.zeros(3, dtype=np.float32),
        "gray": np.full(3, 0.18, dtype=np.float32),
        "white": np.ones(3, dtype=np.float32),
        "green": np.array([0.05, 0.35, 0.08], dtype=np.float32),
    }

    def display_srgb(linear_rgb: np.ndarray) -> np.ndarray:
        clamped = np.clip(linear_rgb, 0.0, 1.0)
        return np.where(
            clamped <= 0.0031308,
            clamped * 12.92,
            1.055 * np.power(clamped, 1.0 / 2.4) - 0.055,
        )

    def stats(values: np.ndarray) -> dict[str, float | int]:
        absolute = np.abs(values.astype(np.float64)).reshape(-1)
        return {
            "component_count": int(absolute.size),
            "rmse": float(np.sqrt(np.mean(absolute * absolute))) if absolute.size else 0.0,
            "p95_abs": float(np.quantile(absolute, 0.95)) if absolute.size else 0.0,
            "max_abs": float(np.max(absolute)) if absolute.size else 0.0,
            "components_over_1_code_value_255": int(np.count_nonzero(absolute > 1.0 / 255.0)),
        }

    cases = []
    preview_data = None
    for sigma in sigmas:
        size = kernel_size(sigma)
        radius = size // 2
        for alpha_value in alphas:
            for placement, (x, y) in placements.items():
                alpha = np.zeros((dimension, dimension), dtype=np.float32)
                premult_rgb = np.zeros((dimension, dimension, 3), dtype=np.float32)
                alpha[y, x] = np.float32(alpha_value)
                premult_rgb[y, x, 0] = np.float32(0.8 * alpha_value)  # logical red
                blurred_alpha = cv2.GaussianBlur(alpha, (size, size), sigmaX=sigma)
                reference_premult = cv2.GaussianBlur(
                    premult_rgb, (size, size), sigmaX=sigma
                )
                actual_premult = np.where(
                    blurred_alpha[..., None] > threshold,
                    reference_premult,
                    0.0,
                )
                x0, x1 = max(0, x - radius), min(dimension, x + radius + 1)
                y0, y1 = max(0, y - radius), min(dimension, y + radius + 1)
                roi = np.zeros((dimension, dimension), dtype=bool)
                roi[y0:y1, x0:x1] = True
                background_results = {}
                for background_name, background in backgrounds.items():
                    actual_composite = actual_premult + background * (1.0 - blurred_alpha[..., None])
                    reference_composite = reference_premult + background * (1.0 - blurred_alpha[..., None])
                    linear_error = actual_composite - reference_composite
                    display_error = display_srgb(actual_composite) - display_srgb(reference_composite)
                    background_results[background_name] = {
                        "linear_full_image": stats(linear_error),
                        "linear_kernel_support_roi": stats(linear_error[roi]),
                        "display_srgb_full_image_code_normalized": stats(display_error),
                        "display_srgb_kernel_support_roi_code_normalized": stats(display_error[roi]),
                    }
                cases.append({
                    "dimensions_px": [dimension, dimension],
                    "sigma_px": sigma,
                    "kernel_width_px": size,
                    "kernel_support_fits_image": size <= dimension,
                    "input_alpha": alpha_value,
                    "placement": placement,
                    "position_xy": [x, y],
                    "alpha_cutoff": threshold,
                    "reference_premultiplied_red_energy": float(reference_premult[..., 0].sum(dtype=np.float64)),
                    "retained_premultiplied_red_energy": float(actual_premult[..., 0].sum(dtype=np.float64)),
                    "zeroed_nonzero_alpha_pixels": int(np.count_nonzero((blurred_alpha > 0.0) & (blurred_alpha <= threshold))),
                    "kernel_support_roi_xyxy": [x0, y0, x1, y1],
                    "backgrounds": background_results,
                })
                if sigma == 64.0 and alpha_value == 0.5 and placement == "center":
                    preview_data = (actual_premult.copy(), reference_premult.copy(), blurred_alpha.copy())

    preview_path = None
    if preview_data is not None:
        actual, reference, output_alpha = preview_data
        x0, x1 = center - 192, center + 193
        y0, y1 = center - 192, center + 193
        scale = 2
        tile = (x1 - x0) * scale
        label_h = 26
        canvas = Image.new("RGB", (tile * 2, (tile + label_h) * len(backgrounds)), (30, 33, 38))
        draw = ImageDraw.Draw(canvas)
        for row, (background_name, background) in enumerate(backgrounds.items()):
            for column, (caption, premult) in enumerate((
                ("threshold 1e-5", actual),
                ("no cutoff reference", reference),
            )):
                composite = premult + background * (1.0 - output_alpha[..., None])
                display = np.uint8(np.rint(np.clip(display_srgb(composite), 0.0, 1.0) * 255.0))
                crop = Image.fromarray(display[y0:y1, x0:x1, :], mode="RGB")
                crop = crop.resize((tile, tile), Image.Resampling.NEAREST)
                left = column * tile
                top = row * (tile + label_h) + label_h
                canvas.paste(crop, (left, top))
                draw.text((left + 4, row * (tile + label_h) + 4),
                          f"{background_name}: {caption}", fill=(245, 245, 245))
        preview_file = ROOT / "experiments/blur-rd/results/previews/gaussian_alpha_cutoff_sigma64_alpha05.png"
        preview_file.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(preview_file)
        preview_path = preview_file.relative_to(ROOT).as_posix()

    return {
        "fixture": "single straight-alpha red point RGB=.8 on transparent black, linear float32",
        "dimensions_px": [dimension, dimension],
        "sigma_values_px": list(sigmas),
        "input_alpha_values": list(alphas),
        "placements": list(placements),
        "alpha_cutoff": threshold,
        "backgrounds_linear_rgb": {name: value.tolist() for name, value in backgrounds.items()},
        "measurement": "reference convolution premultiplied RGB versus the same RGB zeroed where output alpha <= 1e-5, composited over backgrounds; linear and clamped sRGB OETF diagnostics",
        "cases": cases,
        "display_preview": preview_path,
        "display_preview_contract": "8-bit sRGB code-value preview after linear clamp/OETF; distinct float32 outputs below 1/255 code difference quantize to identical pixels",
        "scope_limit": "CPU source-equation model only; display preview clamps to SDR and applies sRGB OETF without Artifact view transform, quantization pipeline, or perceptual acceptance threshold.",
    }


def alpha_cutoff_density_hdr_study() -> dict[str, object]:
    """Test whether overlapping HDR source regions make cutoff tails material."""
    dimension = 513
    center = dimension // 2
    threshold = 1.0e-5
    sigmas = (16.0, 64.0)
    alphas = (0.01, 0.5)
    source_red_values = (0.8, 64.0)
    patterns = ("isolated_point", "dense_16_square", "dense_128_square", "half_plane")
    backgrounds = {
        "black": np.zeros(3, dtype=np.float32),
        "gray": np.full(3, 0.18, dtype=np.float32),
        "white": np.ones(3, dtype=np.float32),
        "green": np.array([0.05, 0.35, 0.08], dtype=np.float32),
    }

    def make_mask(pattern: str) -> np.ndarray:
        mask = np.zeros((dimension, dimension), dtype=np.float32)
        if pattern == "isolated_point":
            mask[center, center] = 1.0
        elif pattern in ("dense_16_square", "dense_128_square"):
            side = 16 if pattern == "dense_16_square" else 128
            start = center - side // 2
            mask[start:start + side, start:start + side] = 1.0
        else:
            mask[:, center:] = 1.0
        return mask

    def display_srgb(linear_rgb: np.ndarray) -> np.ndarray:
        clamped = np.clip(linear_rgb, 0.0, 1.0)
        return np.where(
            clamped <= 0.0031308,
            clamped * 12.92,
            1.055 * np.power(clamped, 1.0 / 2.4) - 0.055,
        )

    def abs_stats(values: np.ndarray) -> dict[str, float | int]:
        absolute = np.abs(values.astype(np.float64)).reshape(-1)
        return {
            "component_count": int(absolute.size),
            "rmse": float(np.sqrt(np.mean(absolute * absolute))) if absolute.size else 0.0,
            "p95_abs": float(np.quantile(absolute, 0.95)) if absolute.size else 0.0,
            "max_abs": float(np.max(absolute)) if absolute.size else 0.0,
            "components_over_1_code_value_255": int(np.count_nonzero(absolute > 1.0 / 255.0)),
        }

    cases = []
    preview = None
    for sigma in sigmas:
        size = kernel_size(sigma)
        radius = size // 2
        for pattern in patterns:
            mask = make_mask(pattern)
            for alpha_value in alphas:
                alpha_input = mask * np.float32(alpha_value)
                blurred_alpha = cv2.GaussianBlur(alpha_input, (size, size), sigmaX=sigma)
                zeroed_tail = (blurred_alpha > 0.0) & (blurred_alpha <= threshold)
                tail_y, tail_x = np.nonzero(zeroed_tail)
                if tail_x.size:
                    tail_bounds = [int(tail_x.min()), int(tail_y.min()),
                                   int(tail_x.max()) + 1, int(tail_y.max()) + 1]
                else:
                    tail_bounds = None
                for source_red in source_red_values:
                    premult_source = np.zeros((dimension, dimension, 3), dtype=np.float32)
                    premult_source[..., 0] = mask * np.float32(alpha_value * source_red)
                    reference_premult = cv2.GaussianBlur(
                        premult_source, (size, size), sigmaX=sigma
                    )
                    actual_premult = np.where(
                        blurred_alpha[..., None] > threshold,
                        reference_premult,
                        0.0,
                    )
                    lost = reference_premult - actual_premult
                    roi = np.zeros((dimension, dimension), dtype=bool)
                    if tail_bounds:
                        x0 = max(0, tail_bounds[0] - 2)
                        y0 = max(0, tail_bounds[1] - 2)
                        x1 = min(dimension, tail_bounds[2] + 2)
                        y1 = min(dimension, tail_bounds[3] + 2)
                        roi[y0:y1, x0:x1] = True
                    bg_results = {}
                    for name, background in backgrounds.items():
                        actual_composite = actual_premult + background * (1.0 - blurred_alpha[..., None])
                        reference_composite = reference_premult + background * (1.0 - blurred_alpha[..., None])
                        display_error = display_srgb(actual_composite) - display_srgb(reference_composite)
                        bg_results[name] = {
                            "linear_lost_premult_rgb_full": abs_stats(lost),
                            "linear_lost_premult_rgb_tail_roi": abs_stats(lost[roi]) if np.any(roi) else abs_stats(lost[:0]),
                            "display_srgb_composite_full": abs_stats(display_error),
                            "display_srgb_composite_tail_roi": abs_stats(display_error[roi]) if np.any(roi) else abs_stats(display_error[:0]),
                        }
                    cases.append({
                        "dimensions_px": [dimension, dimension],
                        "sigma_px": sigma,
                        "kernel_width_px": size,
                        "kernel_support_fits_image": size <= dimension,
                        "pattern": pattern,
                        "pattern_input_pixel_count": int(np.count_nonzero(mask)),
                        "source_alpha": alpha_value,
                        "source_straight_red": source_red,
                        "source_premultiplied_red_energy": float(premult_source[..., 0].sum(dtype=np.float64)),
                        "alpha_cutoff": threshold,
                        "reference_premultiplied_red_energy": float(reference_premult[..., 0].sum(dtype=np.float64)),
                        "retained_premultiplied_red_energy": float(actual_premult[..., 0].sum(dtype=np.float64)),
                        "lost_energy_fraction": (
                            float(lost[..., 0].sum(dtype=np.float64) / reference_premult[..., 0].sum(dtype=np.float64))
                            if reference_premult[..., 0].sum(dtype=np.float64) > 0.0 else 0.0
                        ),
                        "zeroed_tail_pixel_count": int(np.count_nonzero(zeroed_tail)),
                        "zeroed_tail_bounds_xyxy": tail_bounds,
                        "tail_error_bounds_xyxy_expanded_2px": (
                            [max(0, tail_bounds[0] - 2), max(0, tail_bounds[1] - 2),
                             min(dimension, tail_bounds[2] + 2), min(dimension, tail_bounds[3] + 2)]
                            if tail_bounds else None
                        ),
                        "backgrounds": bg_results,
                    })
                    if (sigma == 64.0 and pattern == "dense_128_square"
                            and alpha_value == 0.5 and source_red == 64.0):
                        preview = (actual_premult.copy(), reference_premult.copy(), blurred_alpha.copy())

    preview_path = None
    if preview is not None:
        actual, reference, output_alpha = preview
        # Show a support-corner crop where the dense patch's alpha tail falls
        # below the cutoff; the patch interior itself would clip in SDR.
        x0, x1 = 0, 128
        y0, y1 = 0, 128
        exposures = (0, 4)
        names = list(backgrounds)
        tile = (x1 - x0) * 2
        label_height = 24
        canvas = Image.new("RGB", (tile * 2, (tile + label_height) * len(names) * len(exposures)), (28, 31, 36))
        draw = ImageDraw.Draw(canvas)
        for exposure_index, ev in enumerate(exposures):
            gain = float(2.0 ** ev)
            for bg_index, name in enumerate(names):
                background = backgrounds[name]
                row = exposure_index * len(names) + bg_index
                for column, (label, premult) in enumerate((
                    ("cutoff", actual),
                    ("no cutoff", reference),
                )):
                    composite = (premult + background * (1.0 - output_alpha[..., None])) * gain
                    display = np.uint8(np.rint(np.clip(display_srgb(composite), 0.0, 1.0) * 255.0))
                    crop = Image.fromarray(display[y0:y1, x0:x1, :], mode="RGB")
                    crop = crop.resize((tile, tile), Image.Resampling.NEAREST)
                    left = column * tile
                    top = row * (tile + label_height) + label_height
                    canvas.paste(crop, (left, top))
                    draw.text((left + 4, row * (tile + label_height) + 4),
                              f"EV{ev:+d} {name} / {label}", fill=(245, 245, 245))
        path = ROOT / "experiments/blur-rd/results/previews/gaussian_alpha_cutoff_dense_hdr_bracket.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(path)
        preview_path = path.relative_to(ROOT).as_posix()

    return {
        "fixture_patterns": {
            "isolated_point": "one pixel at center",
            "dense_16_square": "16x16 solid block at center",
            "dense_128_square": "128x128 solid block at center",
            "half_plane": "solid half image beginning at x=256",
        },
        "dimensions_px": [dimension, dimension],
        "sigma_values_px": list(sigmas),
        "source_alpha_values": list(alphas),
        "straight_red_values_linear": list(source_red_values),
        "alpha_cutoff": threshold,
        "backgrounds_linear_rgb": {name: value.tolist() for name, value in backgrounds.items()},
        "display_model": "linear composite; SDR diagnostic clamps at zero/one and applies sRGB OETF; bracket applies exposure gain before clamp",
        "cases": cases,
        "display_preview": preview_path,
        "display_preview_crop_xyxy": [0, 0, 128, 128],
        "scope_limit": "Synthetic source-equation sensitivity model; no Artifact capture, active view transform, or aesthetic acceptance. Half-plane and dense patch do not represent a sampled production asset.",
    }


def hdr_linear_response_study() -> dict[str, object]:
    """Measure unclamped linear Gaussian response on opaque HDR/negative taps."""
    dimension = 513
    center = dimension // 2
    sigmas = (0.5, 3.0, 16.0, 64.0)
    amplitudes = {"blue": 1.5, "green": -0.4, "red": 8.0}
    cases = []
    for sigma in sigmas:
        size = kernel_size(sigma)
        kernel = cv2.getGaussianKernel(size, sigma, cv2.CV_64F)[:, 0]
        weights2d = np.outer(kernel, kernel)
        source = np.zeros((dimension, dimension, 4), dtype=np.float32)
        source[:, :, 3] = 1.0
        for channel, amplitude in amplitudes.items():
            logical_index = {"blue": 0, "green": 1, "red": 2}[channel]
            source[center, center, logical_index] = amplitude
        # This source mirror stores logical RGBA here; GaussianBlur is
        # channel-independent, so the source-layout permutation is immaterial.
        output = cv2.GaussianBlur(source, (size, size), sigmaX=sigma)
        half = size // 2
        expected = np.zeros_like(output, dtype=np.float64)
        for channel, amplitude in amplitudes.items():
            logical_index = {"blue": 0, "green": 1, "red": 2}[channel]
            expected[center-half:center+half+1, center-half:center+half+1,
                     logical_index] = amplitude * weights2d
        expected[:, :, 3] = 1.0
        measurements = {}
        for channel, amplitude in amplitudes.items():
            index = {"blue": 0, "green": 1, "red": 2}[channel]
            actual_plane = output[:, :, index].astype(np.float64)
            expected_plane = expected[:, :, index]
            diff = actual_plane - expected_plane
            measurements[channel] = {
                "input_impulse_value": amplitude,
                "output_min": float(np.min(actual_plane)),
                "output_max": float(np.max(actual_plane)),
                "output_sum": float(actual_plane.sum()),
                "energy_delta_from_input": float(actual_plane.sum() - amplitude),
                "negative_pixel_count": int(np.count_nonzero(actual_plane < 0.0)),
                "above_one_pixel_count": int(np.count_nonzero(actual_plane > 1.0)),
                "reference_max_abs_error": float(np.max(np.abs(diff))),
                "reference_rmse": float(np.sqrt(np.mean(diff**2))),
            }
        display_brackets = {}
        rgb = output[:, :, :3].astype(np.float32)
        for exposure_ev in (-2.0, 0.0, 2.0):
            exposed = rgb * np.float32(2.0**exposure_ev)
            negative_mask = exposed < 0.0
            high_clip_mask = exposed > 1.0
            encoded = np.clip(exposed, 0.0, 1.0)
            encoded = np.where(
                encoded <= 0.0031308,
                encoded * 12.92,
                1.055 * np.power(encoded, 1.0 / 2.4) - 0.055,
            )
            display_brackets[str(int(exposure_ev))] = {
                "display_model": "linear exposure EV, clamp to [0,1], then sRGB OETF; illustrative only",
                "exposure_ev": exposure_ev,
                "high_clipped_channel_samples": int(np.count_nonzero(high_clip_mask)),
                "negative_channel_samples_clamped_to_zero": int(np.count_nonzero(negative_mask)),
                "encoded_rgb_max": [float(v) for v in np.max(encoded, axis=(0, 1))],
                "encoded_rgb_mean": [float(v) for v in np.mean(encoded, axis=(0, 1))],
            }
        cases.append({
            "dimensions": [dimension, dimension],
            "sigma_px": sigma,
            "kernel_width_px": size,
            "kernel_support_fully_inside_image": size <= dimension,
            "alpha_min": float(np.min(output[:, :, 3])),
            "alpha_max": float(np.max(output[:, :, 3])),
            "non_finite_component_count": int(np.count_nonzero(~np.isfinite(output))),
            "channels": measurements,
            "illustrative_sdr_display_bracket": display_brackets,
        })
    return {
        "fixture": "opaque alpha=1 black field with centered logical RGB impulses [1.5, -0.4, 8.0]; linear unclamped float32",
        "reference": "outer product of normalized cv2.getGaussianKernel weights placed at center; support is fully inside the image for every sigma",
        "measurement": "per-channel output extrema, sum/energy delta, negative/>1 pixel counts, finite count, and max/RMSE against direct kernel outer product",
        "display_model": "separate illustrative bracket: exposure EV -2/0/+2, clamp linear RGB to [0,1], then sRGB OETF; not the Artifact Viewer transform",
        "cases": cases,
        "scope_limit": "OpenCV source-equation mirror and mathematical impulse reference; not Artifact capture, color-managed display, or perceptual HDR evaluation.",
    }


def shader_axis_profile(position: int, sigma: float) -> np.ndarray:
    source = np.zeros(SIZE, dtype=np.float32)
    source[position] = 1.0
    output = np.zeros_like(source)
    applied_sigma = np.float32(max(0.1, sigma))
    radius = min(64, int(np.ceil(float(applied_sigma) * 3.0)))
    weight_sum = np.float32(0.0)
    offsets_and_weights = []
    for offset in range(-radius, radius + 1):
        sample_offset = np.float32(offset)
        exponent = np.float32(-0.5) * (sample_offset * sample_offset)
        denominator = np.maximum(
            np.float32(0.0001), applied_sigma * applied_sigma
        )
        weight = np.float32(np.exp(exponent / denominator))
        positions = np.clip(np.arange(SIZE) + offset, 0, SIZE - 1)
        output += source[positions] * weight
        weight_sum = np.float32(weight_sum + weight)
        offsets_and_weights.append(float(weight))
    output /= np.maximum(weight_sum, np.float32(0.0001))
    return output


def resident_gaussian_node_axis_profile(position: int, sigma: float) -> np.ndarray:
    """Mirror the shared resident SeparableGaussianBlur shader and CPU weights."""
    source = np.zeros(SIZE, dtype=np.float32)
    source[position] = 1.0
    output = np.zeros_like(source)
    applied_sigma = np.float32(np.clip(sigma, 0.1, 64.0))
    radius = min(64, int(np.ceil(float(applied_sigma) * 3.0)))
    offsets = np.arange(-radius, radius + 1, dtype=np.float32)
    weights = np.exp(
        np.float32(-0.5) * offsets * offsets / (applied_sigma * applied_sigma)
    ).astype(np.float32)
    weight_sum = np.float32(0.0)
    for weight in weights:
        weight_sum = np.float32(weight_sum + weight)
    weights /= weight_sum
    positions = np.arange(SIZE)
    for offset, weight in zip(range(-radius, radius + 1), weights, strict=True):
        output += source[np.clip(positions + offset, 0, SIZE - 1)] * weight
    return output


def backend_equation_mirror(sigma: float) -> dict[str, float | int]:
    size = kernel_size(sigma)
    source_center = np.zeros((1, SIZE), dtype=np.float32)
    source_center[0, SIZE // 2] = 1.0
    source_border = np.zeros_like(source_center)
    source_border[0, 0] = 1.0
    cpu_center = cv2.GaussianBlur(source_center, (size, 1), sigmaX=sigma)[0]
    cpu_border = cv2.GaussianBlur(source_border, (size, 1), sigmaX=sigma)[0]
    gpu_center = shader_axis_profile(SIZE // 2, sigma)
    gpu_border = shader_axis_profile(0, sigma)
    resident_center = resident_gaussian_node_axis_profile(SIZE // 2, sigma)
    resident_border = resident_gaussian_node_axis_profile(0, sigma)

    center_cpu = np.outer(cpu_center, cpu_center).astype(np.float64)
    center_gpu = np.outer(gpu_center, gpu_center).astype(np.float64)
    border_cpu = np.outer(cpu_center, cpu_border).astype(np.float64)
    border_gpu = np.outer(gpu_center, gpu_border).astype(np.float64)
    center_resident = np.outer(resident_center, resident_center).astype(np.float64)
    border_resident = np.outer(resident_center, resident_border).astype(np.float64)

    center_difference = center_cpu - center_gpu
    border_difference = border_cpu - border_gpu
    center_resident_difference = center_cpu - center_resident
    border_resident_difference = border_cpu - border_resident
    border_gpu_resident_difference = border_gpu - border_resident
    return {
        "sigma_px": sigma,
        "kernel_width_px": size,
        "hlsl_radius_px": min(64, int(np.ceil(max(0.1, sigma) * 3.0))),
        "center_rmse": float(np.sqrt(np.mean(center_difference**2))),
        "center_max_abs_error": float(np.max(np.abs(center_difference))),
        "center_cpu_energy": float(center_cpu.sum()),
        "center_hlsl_mirror_energy": float(center_gpu.sum()),
        "border_rmse": float(np.sqrt(np.mean(border_difference**2))),
        "border_max_abs_error": float(np.max(np.abs(border_difference))),
        "border_cpu_energy": float(border_cpu.sum()),
        "border_hlsl_mirror_energy": float(border_gpu.sum()),
        "border_hlsl_minus_cpu_energy": float(border_gpu.sum() - border_cpu.sum()),
        "resident_radius_px": min(64, int(np.ceil(min(64.0, sigma) * 3.0))),
        "resident_center_rmse": float(np.sqrt(np.mean(center_resident_difference**2))),
        "resident_center_max_abs_error": float(np.max(np.abs(center_resident_difference))),
        "resident_center_energy": float(center_resident.sum()),
        "resident_border_rmse": float(np.sqrt(np.mean(border_resident_difference**2))),
        "resident_border_max_abs_error": float(np.max(np.abs(border_resident_difference))),
        "resident_border_energy": float(border_resident.sum()),
        "resident_border_minus_cpu_energy": float(border_resident.sum() - border_cpu.sum()),
        "resident_border_vs_standalone_hlsl_rmse": float(
            np.sqrt(np.mean(border_gpu_resident_difference**2))
        ),
        "resident_border_vs_standalone_hlsl_max_abs_error": float(
            np.max(np.abs(border_gpu_resident_difference))
        ),
    }


def finite_border_size_study() -> dict[str, object]:
    """Compare edge impulse responses across finite image sizes."""
    cases = []
    for dimension in (65, 257, 513, 1025):
        for sigma in (0.5, 3.0, 16.0, 64.0):
            size = kernel_size(sigma)
            cpu_source = np.zeros((1, dimension), dtype=np.float32)
            cpu_source[0, 0] = 1.0
            cpu_profile = cv2.GaussianBlur(cpu_source, (size, 1), sigmaX=sigma)[0]

            gpu_source = np.zeros(dimension, dtype=np.float32)
            gpu_source[0] = 1.0
            gpu_profile = np.zeros_like(gpu_source)
            radius = min(64, int(np.ceil(max(0.1, sigma) * 3.0)))
            offsets = np.arange(-radius, radius + 1, dtype=np.float32)
            weights = np.exp(
                np.float32(-0.5) * offsets * offsets / np.float32(sigma * sigma)
            ).astype(np.float32)
            weights /= np.sum(weights, dtype=np.float32)
            output_positions = np.arange(dimension)
            for offset, weight in zip(range(-radius, radius + 1), weights, strict=True):
                sample_positions = np.clip(output_positions + offset, 0, dimension - 1)
                gpu_profile += weight * gpu_source[sample_positions]

            coordinates = np.arange(dimension, dtype=np.float64)
            cpu = cpu_profile.astype(np.float64)
            gpu = gpu_profile.astype(np.float64)
            roi_end = min(dimension, int(np.ceil(3.0 * sigma)) + 2)
            roi_diff = cpu[:roi_end] - gpu[:roi_end]

            def profile_stats(profile: np.ndarray) -> dict[str, float]:
                energy = float(profile.sum())
                centroid = float(np.sum(profile * coordinates) / energy) if energy else 0.0
                variance = float(np.sum(profile * (coordinates - centroid) ** 2) / energy) if energy else 0.0
                return {
                    "energy_inside_image": energy,
                    "centroid_x_px": centroid,
                    "spread_sigma_px": float(np.sqrt(max(0.0, variance))),
                    "peak": float(profile.max()),
                }

            cases.append({
                "dimensions_px": [dimension, dimension],
                "requested_sigma_px": sigma,
                "cpu_kernel_width_px": size,
                "shader_radius_px": radius,
                "cpu_support_fits_dimension": size <= dimension,
                "shader_support_fits_dimension": 2 * radius + 1 <= dimension,
                "edge_roi_x_px": [0, roi_end],
                "cpu_reflect101": profile_stats(cpu),
                "shader_clamp_equation_mirror": profile_stats(gpu),
                "edge_roi_rmse": float(np.sqrt(np.mean(roi_diff**2))),
                "edge_roi_max_abs_difference": float(np.max(np.abs(roi_diff))),
            })
    return {
        "fixture": "opaque unit impulse at x=0 in a 1D row; float32; image height 1 for separable-axis isolation",
        "dimensions_px": [65, 257, 513, 1025],
        "sigma_values_px": [0.5, 3.0, 16.0, 64.0],
        "comparison": "OpenCV default Reflect101 CPU equation versus standalone Gaussian HLSL clamp-sampling equation mirror",
        "measurements": cases,
        "scope_limit": "No GPU device, Artifact wrapper, host dispatch, descriptor conversion, or production capture; 1D finite-row result only.",
    }


def low_sigma_cpu_shader_profile_study() -> dict[str, object]:
    """Compare CPU and standalone HLSL equation profiles at property-step values."""
    cases = []
    requested_sigmas = (0.0, 0.1, 0.1666666567325592, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 0.8333333730697632, 1.0)
    coordinates = np.arange(SIZE, dtype=np.float64)
    center = SIZE // 2
    for requested in requested_sigmas:
        cpu_sigma = np.float32(requested)
        raw_width = max(
            1,
            int(np.float32(np.float32(6.0) * cpu_sigma + np.float32(1.0))),
        )
        cpu_width = raw_width + (raw_width % 2 == 0)
        cpu_center = np.zeros((1, SIZE), dtype=np.float32)
        cpu_center[0, center] = 1.0
        cpu_border = np.zeros_like(cpu_center)
        cpu_border[0, 0] = 1.0
        cpu_center_out = cv2.GaussianBlur(
            cpu_center, (cpu_width, 1), sigmaX=float(cpu_sigma)
        )[0].astype(np.float64)
        cpu_border_out = cv2.GaussianBlur(
            cpu_border, (cpu_width, 1), sigmaX=float(cpu_sigma)
        )[0].astype(np.float64)
        shader_center = shader_axis_profile(center, float(cpu_sigma)).astype(np.float64)
        shader_border = shader_axis_profile(0, float(cpu_sigma)).astype(np.float64)
        resident_center = resident_gaussian_node_axis_profile(
            center, float(cpu_sigma)
        ).astype(np.float64)
        resident_border = resident_gaussian_node_axis_profile(
            0, float(cpu_sigma)
        ).astype(np.float64)

        def profile_metrics(profile: np.ndarray, origin: int) -> dict[str, float | int]:
            energy = float(profile.sum())
            mass = profile / energy if energy > 0.0 else profile
            centroid = float(np.sum(mass * coordinates))
            variance = float(np.sum(mass * (coordinates - centroid) ** 2))
            radii = np.abs(coordinates - origin)
            order = np.argsort(radii, kind="stable")
            cumulative = np.cumsum(np.maximum(profile[order], 0.0))
            r95_index = int(np.searchsorted(cumulative, 0.95 * energy, side="left")) if energy else 0
            return {
                "energy_in_image": energy,
                "centroid_x_px": centroid,
                "effective_sigma_px": float(np.sqrt(max(0.0, variance))),
                "r95_from_impulse_px": float(radii[order[min(r95_index, SIZE - 1)]]),
                "peak": float(profile.max()),
            }

        center_diff = cpu_center_out - shader_center
        border_diff = cpu_border_out - shader_border
        resident_center_diff = cpu_center_out - resident_center
        resident_border_diff = cpu_border_out - resident_border
        cases.append({
            "requested_sigma_px": float(cpu_sigma),
            "cpu_kernel_width_px": cpu_width,
            "shader_minimum_sigma_px": max(0.1, float(cpu_sigma)),
            "shader_radius_px": min(64, int(np.ceil(max(0.1, float(cpu_sigma)) * 3.0))),
            "center_cpu": profile_metrics(cpu_center_out, center),
            "center_shader_equation_mirror": profile_metrics(shader_center, center),
            "center_profile_rmse": float(np.sqrt(np.mean(center_diff**2))),
            "center_profile_max_abs_error": float(np.max(np.abs(center_diff))),
            "center_profile_l1_error": float(np.sum(np.abs(center_diff))),
            "border_cpu": profile_metrics(cpu_border_out, 0),
            "border_shader_equation_mirror": profile_metrics(shader_border, 0),
            "border_profile_rmse": float(np.sqrt(np.mean(border_diff**2))),
            "border_profile_max_abs_error": float(np.max(np.abs(border_diff))),
            "border_profile_l1_error": float(np.sum(np.abs(border_diff))),
            "center_resident_equation_mirror": profile_metrics(resident_center, center),
            "resident_center_profile_rmse": float(np.sqrt(np.mean(resident_center_diff**2))),
            "resident_center_profile_max_abs_error": float(np.max(np.abs(resident_center_diff))),
            "border_resident_equation_mirror": profile_metrics(resident_border, 0),
            "resident_border_profile_rmse": float(np.sqrt(np.mean(resident_border_diff**2))),
            "resident_border_profile_max_abs_error": float(np.max(np.abs(resident_border_diff))),
        })
    return {
        "fixture": "513px 1D opaque unit impulse, center and x=0; float32 signal",
        "requested_sigma_values_px": list(requested_sigmas),
        "cpu": "GaussianBlurCPUImpl kernel-size formula and OpenCV default border, source-equation mirror",
        "shader": "Standalone GaussianBlurGPUImpl and shared resident GaussianBlurCS equation mirrors; sigma floor .1, radius ceil(3*sigma) capped at64, coordinate clamp",
        "metrics": "center and border finite-image energy, centroid, effective sigma, R95, peak; CPU-vs-standalone and CPU-vs-resident axis profile RMSE, max, and L1 error",
        "cases": cases,
        "scope_limit": "No Artifact wrapper, device execution, runtime dispatch, color descriptor, or visual acceptability judgment.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()

    result = {
        "study": "Artifact Gaussian CPU source mirror",
        "run_date": "2026-11-12",
        "evidence_class": "source-mirrored OpenCV model; not an Artifact capture",
        "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(SOURCE),
        "comparative_sources": {
            "general_blur_spatial_node_interface": {
                "path": "Artifact/include/Effects/Blur/BlurEffect.ixx",
                "sha256": sha256(ROOT / "Artifact/include/Effects/Blur/BlurEffect.ixx"),
                "effect_ids": ["blur"],
            },
            "gaussian_blur_spatial_node_interface": {
                "path": "Artifact/include/Effects/GauusianBlur.ixx",
                "sha256": sha256(ROOT / "Artifact/include/Effects/GauusianBlur.ixx"),
                "effect_ids": ["effect.blur.gaussian"],
            },
            "shared_resident_shader": {
                "path": "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp",
                "sha256": sha256(ROOT / "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp"),
                "entry_point": "GaussianBlurCS",
                "effect_ids": ["blur", "effect.blur.gaussian"],
            },
        },
        "runner": str(Path(__file__).resolve().relative_to(ROOT)).replace("\\", "/"),
        "runner_sha256": sha256(Path(__file__).resolve()),
        "opencv_version": cv2.__version__,
        "numpy_version": np.__version__,
        "fixture": {
            "dimensions": [SIZE, SIZE],
            "storage": "float32 BGRA, matching ImageF32x4_RGBA physical channel backing",
            "logical_color_contract": "opaque linear red impulse on black; alpha=1 everywhere",
            "impulse_positions_xy": {"center": [SIZE // 2, SIZE // 2], "border": [0, SIZE // 2]},
            "interactive": False,
            "iterations": 1,
        },
        "method": {
            "kernel_size": "max(1, int(6*sigma + 1)); increment if even",
            "opencv_call": "GaussianBlur(float32 BGRA, (k,k), sigmaX=sigma) with OpenCV default border type",
            "scope_limit": "Does not run Artifact wrappers, descriptor conversions, GPU code, or runtime capture.",
        },
        "measurements": [describe(sigma) for sigma in SIGMAS],
        "small_sigma_quantization_study": small_sigma_quantization_study(),
        "alpha_association_study": alpha_association_study(),
        "alpha_association_sigma_sweep": [
            alpha_association_study(sigma) for sigma in SIGMAS
        ],
        "alpha_composite_sigma_sweep": [
            alpha_composite_study(sigma) for sigma in (0.5, 3.0, 16.0)
        ],
        "alpha_edge_composite_sigma_sweep": [
            alpha_edge_composite_study(sigma) for sigma in (0.5, 3.0, 16.0)
        ],
        "alpha_edge_display_preview": save_alpha_edge_preview(),
        "alpha_unpremultiply_cutoff_study": alpha_unpremultiply_cutoff_study(),
        "alpha_cutoff_position_study": alpha_cutoff_position_study(),
        "alpha_cutoff_input_alpha_sweep": alpha_cutoff_input_alpha_sweep(),
        "alpha_cutoff_display_composite_study": alpha_cutoff_display_composite_study(),
        "alpha_cutoff_density_hdr_study": alpha_cutoff_density_hdr_study(),
        "hdr_linear_response_study": hdr_linear_response_study(),
        "cpu_vs_standalone_hlsl_equation_mirror": {
            "evidence_class": "float32 equation mirror; no GPU device or dispatch",
            "fixture": "opaque red impulse, 513x513, centered or centered on left border",
            "cpu_border_rule": "OpenCV default border type from cv::GaussianBlur call",
            "hlsl_border_rule": "clamp sample coordinates to image extent",
            "measurements": [backend_equation_mirror(sigma) for sigma in SIGMAS],
        },
        "finite_border_size_study": finite_border_size_study(),
        "low_sigma_cpu_shader_profile_study": low_sigma_cpu_shader_profile_study(),
        "cpu_vs_resident_separable_gaussian_equation_mirror": {
            "evidence_class": "float32 source-equation mirror; no GPU device or dispatch",
            "source": "Shared SeparableGaussianBlur node: Artifact/include/Effects/GauusianBlur.ixx and Artifact/include/Effects/Blur/BlurEffect.ixx append nodes; Artifact/src/Render/ArtifactRenderLayerPipeline.cpp::applySpatialEffect dispatches GaussianBlurCS",
            "fixture": "opaque red impulse, 513x513, centered or centered on left border",
            "cpu_border_rule": "OpenCV default border type from cv::GaussianBlur call",
            "resident_border_rule": "clamp sample coordinates to image extent",
            "resident_weights": "CPU-generated normalized float32 Gaussian weights; sigma clamped to [0.1,64], radius ceil(3*sigma) capped at 64",
            "measurements": [backend_equation_mirror(sigma) for sigma in SIGMAS],
            "low_sigma_measurements": low_sigma_cpu_shader_profile_study(),
            "scope_limit": "This is only a source-equation mirror of the shared SeparableGaussianBlur resident shader used by both general Blur and Gaussian Blur nodes when their GPU render plans are eligible. It does not execute GPU dispatch, establish a particular frame's route, texture values, or runtime parity.",
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(args.output)


if __name__ == "__main__":
    main()
