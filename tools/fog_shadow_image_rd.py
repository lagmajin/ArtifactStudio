#!/usr/bin/env python3
"""Render a deterministic CPU image fixture for estimated fog-shadow boundaries."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_height_image_fixture_rd as scene
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_shadow_sampling_rd as shadow
import fog_sun_transmittance_rd as scattering


WIDTH = 96
HEIGHT = 54
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
PENUMBRA_WIDTH = 0.01
ESTIMATED_CENTER_OFFSET = 0.005
ESTIMATED_WIDTH_ERROR = 0.005
DIFF_GAIN = 4096.0
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-image-rd-2026-11-12")


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_relative_l2(candidate, reference) -> float:
    error = sum((candidate[i][c] - reference[i][c]) ** 2
                for i in range(len(reference)) for c in range(3))
    norm = sum(value * value for pixel in reference for value in pixel)
    return math.sqrt(error / max(norm, 1.0e-30))


def projected_shadow_shape(x: int, y: int) -> tuple[bool, float]:
    # A fixed screen-space ellipse provides a deterministic projected shadow silhouette.
    cx, cy, rx, ry = 49.0, 27.0, 27.0, 14.0
    radial = math.sqrt(((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2)
    return radial <= 1.0, radial


def region_masks(rays, source_pixels):
    masks = {
        "all": list(range(WIDTH * HEIGHT)),
        "projected_shadow_interior": [],
        "projected_shadow_edge_band": [],
        "shadow_exterior": [],
        "near_horizon_abs_ray_y_le_0_15": [],
        "hdr_source_any_channel_gt_1": [],
        "high_contrast_edge_proxy": [],
    }
    for index, (ray, color) in enumerate(zip(rays, source_pixels)):
        x, y = index % WIDTH, index // WIDTH
        inside, radial = projected_shadow_shape(x, y)
        if radial <= 0.78:
            masks["projected_shadow_interior"].append(index)
        if abs(radial - 1.0) <= 0.18:
            masks["projected_shadow_edge_band"].append(index)
        if radial >= 1.22:
            masks["shadow_exterior"].append(index)
        if abs(ray[1]) <= 0.15:
            masks["near_horizon_abs_ray_y_le_0_15"].append(index)
        if max(color) > 1.0:
            masks["hdr_source_any_channel_gt_1"].append(index)
        neighbors = []
        if x > 0:
            neighbors.append(index - 1)
        if x + 1 < WIDTH:
            neighbors.append(index + 1)
        if y > 0:
            neighbors.append(index - WIDTH)
        if y + 1 < HEIGHT:
            neighbors.append(index + WIDTH)
        if any(max(abs(color[c] - source_pixels[n][c]) for c in range(3)) >= 0.35
               for n in neighbors):
            masks["high_contrast_edge_proxy"].append(index)
    return masks


def select_pixels(pixels, indices):
    return [pixels[i] for i in indices]


def compose(case, source, cells, intervals):
    ray = case["view_direction"]
    view_depth = scattering.exact_density_integral(
        case["camera_y"], ray[1], case["distance"],
        scattering.HEIGHT_FALLOFF, scattering.GROUND,
    )
    view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_depth)
    inscatter = soft.integrate_cells(case, cells, intervals, PENUMBRA_WIDTH)
    return tuple(source[c] * view_t + inscatter[c] for c in range(3))


def save_capture(name: str, pixels) -> dict[str, str]:
    pfm = OUTPUT_DIR / f"{name}.pfm"
    png = OUTPUT_DIR / f"{name}_preview.png"
    scene.pack_pfm(pfm, pixels)
    scene.write_preview_png(png, pixels)
    return {"pfm": pfm.name, "pfm_sha256": file_hash(pfm),
            "preview_png": png.name, "preview_sha256": file_hash(png)}


def write_difference(name: str, pixels, reference) -> dict[str, object]:
    raw = [tuple(abs(pixels[i][c] - reference[i][c]) for c in range(3))
           for i in range(len(reference))]
    display = [tuple(value * DIFF_GAIN for value in pixel) for pixel in raw]
    raw_path = OUTPUT_DIR / f"{name}_diff_raw.pfm"
    png_path = OUTPUT_DIR / f"{name}_diff_gain{int(DIFF_GAIN)}_preview.png"
    scene.pack_pfm(raw_path, raw)
    scene.write_preview_png(png_path, display)
    return {
        "raw_linear_absolute_rgb_pfm": raw_path.name,
        "raw_sha256": file_hash(raw_path),
        "display_gain_preview_png": png_path.name,
        "preview_sha256": file_hash(png_path),
        "display_gain": DIFF_GAIN,
    }


def save_crop(name: str, pixels, box: tuple[int, int, int, int]) -> dict[str, str]:
    x0, y0, width, height = box
    crop = [pixels[(y0 + y) * WIDTH + x0 + x]
            for y in range(height) for x in range(width)]
    old_width, old_height = scene.WIDTH, scene.HEIGHT
    try:
        scene.WIDTH, scene.HEIGHT = width, height
        return save_capture(name, crop)
    finally:
        scene.WIDTH, scene.HEIGHT = old_width, old_height


def main() -> None:
    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    camera_y = scene.CAMERA[1]
    sun_el = math.radians(45.0)
    sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                                math.cos(sun_el) * math.cos(sun_az)))

    rays, distances, sources, cases, intervals_by_pixel = [], [], [], [], []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = scene.ray_for_pixel(x, y)
            source_color, distance = scene.scene_sample(ray)
            shadowed, radial = projected_shadow_shape(x, y)
            start = 0.31 + 0.07 * y / HEIGHT
            end = 0.46 + 0.05 * x / WIDTH
            intervals = ((start, end),) if shadowed else ()
            rays.append(ray)
            distances.append(distance)
            sources.append(source_color)
            cases.append({"camera_y": camera_y, "view_direction": ray,
                          "distance": distance, "sun_direction": sun})
            intervals_by_pixel.append(intervals)

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    reference, validation = [], []
    candidates = {
        "uniform_midpoint_32": [],
        "rho_density_only_32": [],
        "exact_profile_weighted_32": [],
        "exact_profile_weighted_128": [],
        "estimated_center_plus_0_5_width_plus_0_5_128": [],
    }
    for i, case in enumerate(cases):
        truth = intervals_by_pixel[i]
        ref_cells = soft.reference_cells(case, truth, PENUMBRA_WIDTH, REFERENCE_STEPS)
        val_cells = soft.reference_cells(case, truth, PENUMBRA_WIDTH, VALIDATION_STEPS)
        reference.append(compose(case, sources[i], ref_cells, truth))
        validation.append(compose(case, sources[i], val_cells, truth))

        candidate_specs = [
            ("uniform_midpoint_32", "uniform", 32, truth),
            ("rho_density_only_32", "density", 32, truth),
            ("exact_profile_weighted_32", "visibility", 32, truth),
            ("exact_profile_weighted_128", "visibility", 128, truth),
        ]
        estimated = combined.estimate_intervals(
            truth, ESTIMATED_CENTER_OFFSET, ESTIMATED_WIDTH_ERROR,
        )
        candidate_specs.append(("estimated_center_plus_0_5_width_plus_0_5_128",
                                "visibility", 128, estimated))
        for name, method, steps, estimate in candidate_specs:
            if method == "uniform":
                width = case["distance"] / steps
                cells = [(j * width, (j + 1) * width) for j in range(steps)]
            elif method == "density":
                cells = shadow.make_weighted_cells(
                    camera_y, case["view_direction"][1], case["distance"], steps, (), False,
                )
            else:
                cells = soft.weighted_cells(
                    camera_y, case["view_direction"][1], case["distance"],
                    steps, estimate, PENUMBRA_WIDTH,
                )
            candidates[name].append(compose(case, sources[i], cells, truth))

    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    captures = {"source": sources, "reference_profile_aligned_4096": reference, **candidates}
    capture_files = {name: save_capture(name, pixels) for name, pixels in captures.items()}
    masks = region_masks(rays, sources)

    metrics = {}
    for name, pixels in candidates.items():
        metrics[name] = {"whole_image_relative_rgb_l2": image_relative_l2(pixels, reference),
                         "regions": {}}
        for region, indices in masks.items():
            ref_region = select_pixels(reference, indices)
            candidate_region = select_pixels(pixels, indices)
            comparison = scene.compare_pixels(ref_region, candidate_region) if indices else None
            if comparison is not None:
                absolute_pixel_max = sorted(
                    max(abs(candidate_region[i][c] - ref_region[i][c]) for c in range(3))
                    for i in range(len(indices))
                )
                comparison["p95_absolute_rgb"] = absolute_pixel_max[math.ceil(0.95 * len(absolute_pixel_max)) - 1]
            metrics[name]["regions"][region] = {
                "pixel_count": len(indices),
                "relative_rgb_l2": image_relative_l2(candidate_region, ref_region) if indices else None,
                "pixel_comparison": comparison,
            }

    differences = {
        name: write_difference(name, pixels, reference)
        for name, pixels in candidates.items()
    }
    crop_box = (14, 6, 70, 42)
    crops = {
        "reference": save_crop("reference_profile_aligned_center_crop", reference, crop_box),
        "estimated_candidate": save_crop(
            "estimated_center_plus_0_5_width_plus_0_5_128_center_crop",
            candidates["estimated_center_plus_0_5_width_plus_0_5_128"], crop_box,
        ),
    }
    estimated_diff = [
        tuple(abs(candidates["estimated_center_plus_0_5_width_plus_0_5_128"][i][c]
                  - reference[i][c]) * DIFF_GAIN for c in range(3))
        for i in range(len(reference))
    ]
    crops["estimated_candidate_gain_4096_difference"] = save_crop(
        "estimated_center_plus_0_5_width_plus_0_5_128_diff_gain4096_center_crop",
        estimated_diff, crop_box,
    )
    input_digest = hashlib.sha256(json.dumps({
        "size": [WIDTH, HEIGHT], "camera": scene.CAMERA, "fov": scene.FOV_DEGREES,
        "rays": rays, "distances": distances, "source": sources,
        "shadow_intervals": intervals_by_pixel,
    }, separators=(",", ":")).encode("utf-8")).hexdigest()
    validation_error = image_relative_l2(validation, reference)
    manifest = {
        "schema": "fog-shadow-image-rd-v1",
        "purpose": "Map synthetic shadow-boundary estimation errors to deterministic image-space regions.",
        "execution_scope": "CPU image fixture only; no product renderer, geometric shadow map, GPU, or runtime timing.",
        "pre_registered_evaluation_plan": {
            "input": "Existing opaque scene-linear chart from fog_height_image_fixture_rd.py, rendered at 96x54; same pixels, depth/rays and fixed source colors for all candidates.",
            "shadow_fixture": "Screen-space ellipse centered at (49,27) with radii (27,14); within it, a ray-normalized soft shadow core varies deterministically with pixel coordinates.",
            "sun_elevation_degrees": 45.0,
            "penumbra_width_fraction_of_ray": PENUMBRA_WIDTH,
            "candidate_boundary_error": {"center_offset_fraction_of_ray": ESTIMATED_CENTER_OFFSET,
                                          "core_relative_width_error": ESTIMATED_WIDTH_ERROR},
            "reference": f"Profile-knot and ground-aligned uniform midpoint with {REFERENCE_STEPS} samples per nonzero interval; validate against {VALIDATION_STEPS}.",
            "gate": f"Numerical quadrature relative RGB L2 <= {soft.GATE}; not a visual quality approval gate.",
            "regions": "whole image, projected shadow interior, projected edge band, exterior, near horizon, HDR source, high-contrast source edges.",
            "image_statistics": "Linear RGB relative L2, channel absolute max, RGB RMSE, mean absolute RGB, p95 and maximum absolute RGB per region; raw PFM differences plus gain-4096 preview.",
        },
        "size": [WIDTH, HEIGHT],
        "alpha": "Opaque fixture only; alpha not represented.",
        "color": "Scene-linear float32 RGB; HDR source retained; PNG preview uses shared Reinhard plus gamma 1/2.2 transform.",
        "medium": {"ground": scattering.GROUND, "density_at_ground": scattering.DENSITY_AT_GROUND,
                   "falloff": scattering.HEIGHT_FALLOFF, "absorption": scattering.ABSORPTION,
                   "scattering": scattering.SCATTERING, "sun_intensity": scattering.SUN_INTENSITY,
                   "fog_tint": scattering.FOG_TINT},
        "reference_validation_relative_rgb_l2": validation_error,
        "input_fixture_sha256": input_digest,
        "source_captures": capture_files,
        "metrics_vs_reference": metrics,
        "difference_captures": differences,
        "center_crop_box_xywh": crop_box,
        "center_crops": crops,
        "source_scripts": {
            name: {"path": f"tools/{name}.py", "sha256": file_hash(Path("tools") / f"{name}.py")}
            for name in ("fog_height_image_fixture_rd", "fog_shadow_penumbra_rd",
                         "fog_shadow_combined_error_rd", "fog_shadow_sampling_rd",
                         "fog_sun_transmittance_rd", "fog_shadow_image_rd")
        },
        "unverified": ["visual review by a human", "transparent/alpha edges", "temporal stability",
                       "real shadow-map correspondence", "GPU/backend parity", "processing time and memory"],
        "limitations": [
            "The screen-space ellipse is a deterministic projected silhouette proxy, not a geometric occluder or light-space shadow.",
            "One penumbra width and one combined center/width error are shown; this image does not cover the full parameter sweep.",
            "No temporal sequence, alpha-bearing source, production color-management path, or GPU capture is included.",
        ],
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(manifest_path.as_posix())
    print("reference validation image relative RGB L2", validation_error)
    for name, result in metrics.items():
        edge = result["regions"]["projected_shadow_edge_band"]
        inside = result["regions"]["projected_shadow_interior"]
        print(name, "image", result["whole_image_relative_rgb_l2"],
              "edge", edge["relative_rgb_l2"], "interior", inside["relative_rgb_l2"])


if __name__ == "__main__":
    main()
