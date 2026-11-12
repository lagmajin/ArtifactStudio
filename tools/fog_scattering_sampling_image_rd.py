#!/usr/bin/env python3
"""Render a small deterministic image fixture for scattering sample placement."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_height_image_fixture_rd as scene_fixture
import fog_scattering_sampling_distribution_rd as sampling_study
import fog_sun_transmittance_rd as scattering


WIDTH = 96
HEIGHT = 54
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
SUN_ELEVATION = 45.0
DIFF_DISPLAY_GAIN = 4096.0
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-scattering-sampling-image-rd-2026-11-12")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def make_masks(
    rays: list[tuple[float, float, float]],
    distances: list[float],
    source: list[tuple[float, float, float]],
) -> dict[str, list[int]]:
    masks = {
        "all": list(range(len(rays))),
        "near_surface_distance_lt_20": [],
        "far_surface_distance_ge_20": [],
        "near_horizon_abs_ray_y_le_0_15": [],
        "steep_ray_abs_ray_y_gt_0_35": [],
        "hdr_source_any_channel_gt_1": [],
        "high_contrast_edge_proxy": [],
    }
    for index, (ray, distance, color) in enumerate(zip(rays, distances, source)):
        if distance < 20.0:
            masks["near_surface_distance_lt_20"].append(index)
        else:
            masks["far_surface_distance_ge_20"].append(index)
        if abs(ray[1]) <= 0.15:
            masks["near_horizon_abs_ray_y_le_0_15"].append(index)
        if abs(ray[1]) > 0.35:
            masks["steep_ray_abs_ray_y_gt_0_35"].append(index)
        if max(color) > 1.0:
            masks["hdr_source_any_channel_gt_1"].append(index)
        x = index % WIDTH
        y = index // WIDTH
        neighbors = []
        if x > 0:
            neighbors.append(index - 1)
        if x + 1 < WIDTH:
            neighbors.append(index + 1)
        if y > 0:
            neighbors.append(index - WIDTH)
        if y + 1 < HEIGHT:
            neighbors.append(index + WIDTH)
        if any(max(abs(color[c] - source[n][c]) for c in range(3)) >= 0.35 for n in neighbors):
            masks["high_contrast_edge_proxy"].append(index)
    return masks


def select(pixels: list[tuple[float, float, float]], indices: list[int]) -> list[tuple[float, float, float]]:
    return [pixels[index] for index in indices]


def relative_image_rgb_l2(
    candidate: list[tuple[float, float, float]],
    reference: list[tuple[float, float, float]],
) -> float:
    error_sq = sum(
        (candidate[i][channel] - reference[i][channel]) ** 2
        for i in range(len(reference))
        for channel in range(3)
    )
    reference_sq = sum(value * value for pixel in reference for value in pixel)
    return math.sqrt(error_sq / max(reference_sq, 1.0e-30))


def compare_capture(
    reference: list[tuple[float, float, float]],
    candidate: list[tuple[float, float, float]],
) -> dict[str, object]:
    result = scene_fixture.compare_pixels(reference, candidate)
    relative_error = relative_image_rgb_l2(candidate, reference)
    result["relative_rgb_l2"] = relative_error
    result["passes_preregistered_numerical_gate"] = relative_error <= 1.0e-3
    return result


def write_crop(name: str, pixels: list[tuple[float, float, float]],
               x0: int, y0: int, width: int, height: int) -> dict[str, str]:
    crop = [
        pixels[(y0 + y) * WIDTH + (x0 + x)]
        for y in range(height)
        for x in range(width)
    ]
    old_width, old_height = scene_fixture.WIDTH, scene_fixture.HEIGHT
    try:
        scene_fixture.WIDTH, scene_fixture.HEIGHT = width, height
        pfm_path = OUTPUT_DIR / f"{name}.pfm"
        png_path = OUTPUT_DIR / f"{name}_preview.png"
        scene_fixture.pack_pfm(pfm_path, crop)
        scene_fixture.write_preview_png(png_path, crop)
    finally:
        scene_fixture.WIDTH, scene_fixture.HEIGHT = old_width, old_height
    return {
        "pfm": pfm_path.name,
        "pfm_sha256": sha256(pfm_path),
        "preview_png": png_path.name,
        "preview_sha256": sha256(png_path),
        "source_crop_xywh": [x0, y0, width, height],
    }


def compose(
    source: tuple[float, float, float], camera_y: float,
    view_direction: tuple[float, float, float], distance: float,
    sun_direction: tuple[float, float, float], steps: int, method: str,
) -> tuple[float, float, float]:
    optical_density_length = scattering.exact_density_integral(
        camera_y, view_direction[1], distance,
        scattering.HEIGHT_FALLOFF, scattering.GROUND,
    )
    view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * optical_density_length)
    scattered = sampling_study.integrate_scattering(
        camera_y, view_direction, distance, sun_direction, steps, method,
    )
    return tuple(source[channel] * view_t + scattered[channel] for channel in range(3))


def main() -> None:
    # Reuse the established synthetic scene while reducing its raster resolution.
    scene_fixture.WIDTH = WIDTH
    scene_fixture.HEIGHT = HEIGHT
    camera_y = scene_fixture.CAMERA[1]
    sun_elevation = math.radians(SUN_ELEVATION)
    sun_azimuth = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun_direction = scattering.normalize((
        math.cos(sun_elevation) * math.sin(sun_azimuth),
        math.sin(sun_elevation),
        math.cos(sun_elevation) * math.cos(sun_azimuth),
    ))

    rays: list[tuple[float, float, float]] = []
    distances: list[float] = []
    source_pixels: list[tuple[float, float, float]] = []
    reference: list[tuple[float, float, float]] = []
    validation: list[tuple[float, float, float]] = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = scene_fixture.ray_for_pixel(x, y)
            source, distance = scene_fixture.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            source_pixels.append(source)
            reference.append(compose(
                source, camera_y, ray, distance, sun_direction,
                REFERENCE_STEPS, "uniform_midpoint",
            ))
            validation.append(compose(
                source, camera_y, ray, distance, sun_direction,
                VALIDATION_STEPS, "uniform_midpoint",
            ))

    candidates = {
        "uniform_midpoint_32": [],
        "uniform_midpoint_64": [],
        "rho_one_third_weighted_32": [],
    }
    for index, ray in enumerate(rays):
        source = source_pixels[index]
        distance = distances[index]
        for name, steps, method in (
            ("uniform_midpoint_32", 32, "uniform_midpoint"),
            ("uniform_midpoint_64", 64, "uniform_midpoint"),
            ("rho_one_third_weighted_32", 32, "rho_one_third_weighted"),
        ):
            candidates[name].append(compose(
                source, camera_y, ray, distance, sun_direction, steps, method,
            ))

    masks = make_masks(rays, distances, source_pixels)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    captures = {"source": source_pixels, "reference_uniform_4096": reference, **candidates}
    capture_files: dict[str, dict[str, str]] = {}
    for name, pixels in captures.items():
        pfm_path = OUTPUT_DIR / f"{name}.pfm"
        png_path = OUTPUT_DIR / f"{name}_preview.png"
        scene_fixture.pack_pfm(pfm_path, pixels)
        scene_fixture.write_preview_png(png_path, pixels)
        capture_files[name] = {
            "pfm": pfm_path.name,
            "pfm_sha256": sha256(pfm_path),
            "preview_png": png_path.name,
            "preview_sha256": sha256(png_path),
        }

    crop_box = (16, 9, 64, 36)
    crop_files = {
        name: write_crop(f"{name}_center_crop", pixels, *crop_box)
        for name, pixels in captures.items()
    }

    metrics: dict[str, object] = {}
    for name, pixels in {"reference_validation_uniform_2048": validation, **candidates}.items():
        metrics[name] = {
            "whole_image": compare_capture(reference, pixels),
            "regions": {
                region: {
                    "pixel_count": len(indices),
                    "comparison": compare_capture(
                        select(reference, indices), select(pixels, indices),
                    ) if indices else None,
                }
                for region, indices in masks.items()
            },
        }

    diff_files: dict[str, dict[str, str]] = {}
    for name, pixels in candidates.items():
        raw_diff_pixels = [
            tuple(abs(pixels[i][c] - reference[i][c]) for c in range(3))
            for i in range(len(reference))
        ]
        display_diff_pixels = [
            tuple(value * DIFF_DISPLAY_GAIN for value in pixel)
            for pixel in raw_diff_pixels
        ]
        pfm_path = OUTPUT_DIR / f"{name}_diff_raw.pfm"
        png_path = OUTPUT_DIR / f"{name}_diff_gain{int(DIFF_DISPLAY_GAIN)}_preview.png"
        scene_fixture.pack_pfm(pfm_path, raw_diff_pixels)
        scene_fixture.write_preview_png(png_path, display_diff_pixels)
        diff_files[name] = {
            "pfm": pfm_path.name,
            "pfm_sha256": sha256(pfm_path),
            "preview_png": png_path.name,
            "preview_sha256": sha256(png_path),
            "display_gain": DIFF_DISPLAY_GAIN,
            "display_note": "PFM stores raw linear absolute RGB difference; PNG multiplies by display_gain before the common Reinhard+gamma preview transform.",
        }

    manifest = {
        "schema": "fog-scattering-sampling-image-rd-v1",
        "size": [WIDTH, HEIGHT],
        "input_scene": {
            "generator": "tools/fog_height_image_fixture_rd.py",
            "camera_position": list(scene_fixture.CAMERA),
            "vertical_fov_degrees": scene_fixture.FOV_DEGREES,
            "surface_distance_encoding": "Euclidean ray distance; synthetic, not GPU depth",
            "source_color": "scene-linear RGB float32; HDR preserved; opaque input",
        },
        "medium": {
            "ground": scattering.GROUND,
            "density_at_ground": scattering.DENSITY_AT_GROUND,
            "falloff": scattering.HEIGHT_FALLOFF,
            "absorption": scattering.ABSORPTION,
            "scattering": scattering.SCATTERING,
            "sun_intensity": scattering.SUN_INTENSITY,
            "sun_elevation_degrees": SUN_ELEVATION,
            "sun_azimuth_degrees": scattering.SUN_AZIMUTH_DEGREES,
            "fog_tint": scattering.FOG_TINT,
        },
        "alpha": "opaque only; alpha is not represented",
        "reference": {
            "method": "uniform midpoint with analytic view/sun transmittance",
            "steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "validation_relative_rgb_l2": relative_image_rgb_l2(validation, reference),
            "note": "Image-wide RGB RMSE validation; per-ray convergence has a separate 16,384-vs-32,768 sweep.",
        },
        "candidates": {name: {"steps": 32 if name != "uniform_midpoint_64" else 64,
                               "method": "rho_one_third_weighted" if name.startswith("rho_") else "uniform_midpoint",
                               "files": capture_files[name]}
                       for name in candidates},
        "reference_capture": capture_files["reference_uniform_4096"],
        "source_capture": capture_files["source"],
        "source_scripts": {
            "image_fixture": {"path": "tools/fog_height_image_fixture_rd.py",
                              "sha256": sha256(Path("tools/fog_height_image_fixture_rd.py"))},
            "sampling_integrator": {"path": "tools/fog_scattering_sampling_distribution_rd.py",
                                    "sha256": sha256(Path("tools/fog_scattering_sampling_distribution_rd.py"))},
            "sun_reference": {"path": "tools/fog_sun_transmittance_rd.py",
                              "sha256": sha256(Path("tools/fog_sun_transmittance_rd.py"))},
            "image_sweep": {"path": Path(__file__).as_posix(), "sha256": sha256(Path(__file__))},
        },
        "center_crops": {
            "description": "Fixed crop x=16, y=9, width=64, height=36; includes horizon and near/far chart boundaries.",
            "files": crop_files,
        },
        "regions": {
            "near_surface_distance_lt_20": "true Euclidean ray distance < 20 scene units",
            "far_surface_distance_ge_20": "true Euclidean ray distance >= 20 scene units",
            "near_horizon_abs_ray_y_le_0_15": "abs(view ray y) <= 0.15",
            "steep_ray_abs_ray_y_gt_0_35": "abs(view ray y) > 0.35",
            "hdr_source_any_channel_gt_1": "source RGB max > 1.0",
            "high_contrast_edge_proxy": "at least one 4-neighbor source RGB max-channel difference >= 0.35",
        },
        "metrics_vs_reference": metrics,
        "difference_captures": diff_files,
        "preview_transform": "Reinhard channel mapping then gamma 1/2.2; difference PNGs scale raw linear RGB delta by display_gain first.",
        "source_script": {"path": Path(__file__).as_posix(), "sha256": sha256(Path(__file__))},
        "preregistered_numerical_gate": {
            "relative_rgb_l2_error": 1.0e-3,
            "scope": "numerical quadrature only; not an image-quality approval gate",
        },
        "limitations": [
            "Synthetic chart, arbitrary world scale, 96x54 resolution, no real production footage.",
            "No shadows, local volume boundary, temporal sequence, alpha-bearing input, or color-management path.",
            "CPU quadrature only; no product shader execution, GPU timing, backend parity, or instruction-cost measurement.",
        ],
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(manifest_path.as_posix())
    print("reference 2048 validation RMSE:", metrics["reference_validation_uniform_2048"]["whole_image"]["rgb_rmse"])
    for name in candidates:
        comparison = metrics[name]["whole_image"]
        print(name, "RMSE", comparison["rgb_rmse"], "max", comparison["max_absolute_rgb"])


if __name__ == "__main__":
    main()
