#!/usr/bin/env python3
"""Project signed core-width estimation errors into a deterministic fog image."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_height_image_fixture_rd as scene
import fog_shadow_combined_error_rd as combined
import fog_shadow_image_rd as base
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


WIDTH = base.WIDTH
HEIGHT = base.HEIGHT
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-width-signed-image-rd-2026-11-12")
ERRORS = (-0.02, 0.02)
CANDIDATE_STEPS = 128
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
DIFF_GAIN = 4096.0
CROP_BOX = (14, 6, 70, 42)


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def metrics_for_region(reference, candidate, indices):
    ref = [reference[i] for i in indices]
    cand = [candidate[i] for i in indices]
    comparison = scene.compare_pixels(ref, cand) if indices else None
    if comparison is not None:
        ordered = sorted(max(abs(cand[i][c] - ref[i][c]) for c in range(3))
                         for i in range(len(indices)))
        comparison["p95_absolute_rgb"] = ordered[math.ceil(0.95 * len(ordered)) - 1]
        comparison["relative_rgb_l2"] = base.image_relative_l2(cand, ref)
    return {"pixel_count": len(indices), "comparison": comparison}


def vector_symmetry(reference, minus, plus, indices):
    minus_error = [(minus[i][c] - reference[i][c]) for i in indices for c in range(3)]
    plus_error = [(plus[i][c] - reference[i][c]) for i in indices for c in range(3)]
    odd = [(p - m) * 0.5 for m, p in zip(minus_error, plus_error)]
    even = [(p + m) * 0.5 for m, p in zip(minus_error, plus_error)]
    count = max(1, len(odd))
    odd_norm_sq = sum(v * v for v in odd)
    even_norm_sq = sum(v * v for v in even)
    dot = sum(m * p for m, p in zip(minus_error, plus_error))
    denom = math.sqrt(sum(v * v for v in minus_error) * sum(v * v for v in plus_error))
    return {
        "pixel_count": len(indices),
        "odd_rgb_rmse": math.sqrt(odd_norm_sq / count),
        "even_rgb_rmse": math.sqrt(even_norm_sq / count),
        "odd_to_even_rmse_ratio": (math.sqrt(odd_norm_sq / count)
                                   / max(math.sqrt(even_norm_sq / count), 1.0e-30)),
        "minus_plus_error_vector_cosine": dot / denom if denom > 0.0 else None,
    }


def main() -> None:
    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    base.OUTPUT_DIR = OUTPUT_DIR
    camera_y = scene.CAMERA[1]
    sun_el = math.radians(45.0)
    sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                                math.cos(sun_el) * math.cos(sun_az)))

    rays, distances, sources, cases, intervals_by_pixel = [], [], [], [], []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = scene.ray_for_pixel(x, y)
            source, distance = scene.scene_sample(ray)
            inside, _ = base.projected_shadow_shape(x, y)
            start = 0.31 + 0.07 * y / HEIGHT
            end = 0.46 + 0.05 * x / WIDTH
            truth = ((start, end),) if inside else ()
            rays.append(ray)
            distances.append(distance)
            sources.append(source)
            cases.append({"camera_y": camera_y, "view_direction": ray,
                          "distance": distance, "sun_direction": sun})
            intervals_by_pixel.append(truth)

    reference, validation = [], []
    candidates = {"width_minus_2pct_128": [], "width_plus_2pct_128": []}
    for case, source, truth in zip(cases, sources, intervals_by_pixel):
        ref_cells = soft.reference_cells(case, truth, base.PENUMBRA_WIDTH, REFERENCE_STEPS)
        val_cells = soft.reference_cells(case, truth, base.PENUMBRA_WIDTH, VALIDATION_STEPS)
        reference.append(base.compose(case, source, ref_cells, truth))
        validation.append(base.compose(case, source, val_cells, truth))
        for error in ERRORS:
            estimate = combined.estimate_intervals(truth, 0.0, error)
            cells = soft.weighted_cells(camera_y, case["view_direction"][1], case["distance"],
                                        CANDIDATE_STEPS, estimate, base.PENUMBRA_WIDTH)
            name = "width_minus_2pct_128" if error < 0 else "width_plus_2pct_128"
            candidates[name].append(base.compose(case, source, cells, truth))

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    captures = {"reference_profile_aligned_4096": reference, **candidates}
    capture_files = {name: base.save_capture(name, pixels)
                     for name, pixels in captures.items()}
    masks = base.region_masks(rays, sources)
    metrics = {}
    difference_files = {}
    for name, pixels in candidates.items():
        metrics[name] = {"whole_image_relative_rgb_l2": base.image_relative_l2(pixels, reference),
                         "regions": {region: metrics_for_region(reference, pixels, indices)
                                     for region, indices in masks.items()}}
        raw = [tuple(abs(pixels[i][c] - reference[i][c]) for c in range(3))
               for i in range(len(reference))]
        display = [tuple(value * DIFF_GAIN for value in pixel) for pixel in raw]
        raw_path = OUTPUT_DIR / f"{name}_diff_raw.pfm"
        png_path = OUTPUT_DIR / f"{name}_diff_gain4096_preview.png"
        scene.pack_pfm(raw_path, raw)
        scene.write_preview_png(png_path, display)
        difference_files[name] = {
            "raw_linear_absolute_rgb_pfm": raw_path.name,
            "raw_sha256": file_hash(raw_path),
            "display_gain_preview_png": png_path.name,
            "preview_sha256": file_hash(png_path),
            "display_gain": DIFF_GAIN,
        }

    crop_files = {}
    for name, pixels in {"reference": reference, **candidates}.items():
        crop_files[name] = base.save_crop(f"{name}_center_crop", pixels, CROP_BOX)
    for name, pixels in candidates.items():
        diff = [tuple(abs(pixels[i][c] - reference[i][c]) * DIFF_GAIN
                      for c in range(3)) for i in range(len(reference))]
        crop_files[f"{name}_gain4096_difference"] = base.save_crop(
            f"{name}_diff_gain4096_center_crop", diff, CROP_BOX)

    input_digest = hashlib.sha256(json.dumps({
        "size": [WIDTH, HEIGHT], "camera": scene.CAMERA, "fov": scene.FOV_DEGREES,
        "rays": rays, "distances": distances, "source": sources,
        "shadow_intervals": intervals_by_pixel,
    }, separators=(",", ":")).encode("utf-8")).hexdigest()
    region_counts = {name: len(indices) for name, indices in masks.items()}
    minus = candidates["width_minus_2pct_128"]
    plus = candidates["width_plus_2pct_128"]
    sign_symmetry = {region: vector_symmetry(reference, minus, plus, indices)
                     for region, indices in masks.items()}
    validation_error = base.image_relative_l2(validation, reference)
    source_names = ("fog_height_image_fixture_rd", "fog_shadow_penumbra_rd",
                    "fog_shadow_combined_error_rd", "fog_shadow_sampling_rd",
                    "fog_sun_transmittance_rd", "fog_shadow_image_rd",
                    "fog_shadow_width_signed_image_rd")
    manifest = {
        "schema": "fog-shadow-width-signed-image-rd-v1",
        "execution_scope": "CPU opaque image fixture; no product renderer, geometric shadow map, GPU runtime, or timing.",
        "pre_registered_evaluation_plan": {
            "input": "Same 96x54 opaque scene-linear chart, pixel rays and source colors as fog_shadow_image_rd.py.",
            "shadow_fixture": "Screen-space ellipse centered at (49,27), radii (27,14), ray-normalized interval core.",
            "sun_elevation_degrees": 45.0,
            "penumbra_width_fraction_of_ray": base.PENUMBRA_WIDTH,
            "candidate_errors": {"center_offset_fraction_of_ray": 0.0,
                                 "core_relative_width_errors": list(ERRORS)},
            "candidate_steps": CANDIDATE_STEPS,
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "gate": "No perceptual pass/fail threshold; this is a descriptive image-space sensitivity comparison.",
            "regions": "whole image, projected shadow interior/edge/exterior, near horizon, HDR source, high-contrast edge proxy.",
            "statistics": "Scene-linear RGB relative L2, MAE/RMSE, channel maximum, max-channel p95, count above 1e-3; raw PFM and gain-4096 PNG difference.",
            "crop_xywh": list(CROP_BOX),
        },
        "size": [WIDTH, HEIGHT],
        "alpha": "Opaque fixture only; alpha is not represented, so alpha edge behavior is not evaluated.",
        "color": "Scene-linear float32 RGB; HDR retained. PNG preview uses Reinhard plus gamma 1/2.2.",
        "input_fixture_sha256": input_digest,
        "reference_validation_relative_rgb_l2": validation_error,
        "region_pixel_counts": region_counts,
        "source_captures": capture_files,
        "metrics_vs_reference": metrics,
        "signed_error_vector_symmetry": sign_symmetry,
        "difference_captures": difference_files,
        "center_crops": crop_files,
        "source_scripts": {name: {"path": f"tools/{name}.py",
                                  "sha256": file_hash(Path("tools") / f"{name}.py")}
                           for name in source_names},
        "unverified": ["human visual quality approval", "alpha-bearing edges", "temporal stability",
                       "real shadow-map correspondence", "GPU/backend parity", "preview/final parity",
                       "product host integration", "processing time and memory"],
        "limitations": [
            "The ellipse is a screen-space silhouette proxy, not a geometric occluder or light-space shadow.",
            "This comparison holds the scene and intervals fixed and measures signed width errors only; it does not render the three aspect-ratio AABBs.",
            "The 96x54 opaque chart omits production color management, temporal motion, and alpha-bearing sources.",
        ],
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(manifest_path.as_posix())
    print("reference validation image relative RGB L2", validation_error)
    for name, result in metrics.items():
        edge = result["regions"]["projected_shadow_edge_band"]["comparison"]
        print(name, "whole", result["whole_image_relative_rgb_l2"],
              "edge", edge["relative_rgb_l2"], "edge p95", edge["p95_absolute_rgb"])


if __name__ == "__main__":
    main()
