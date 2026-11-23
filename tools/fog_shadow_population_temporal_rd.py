#!/usr/bin/env python3
"""Measure direct-ray temporal error at reprojected shadowed world points."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_shadow_temporal_outlier_rd as outlier
import fog_sun_transmittance_rd as scattering


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "population_temporal_direct_ray.csv"
MANIFEST = OUTPUT_DIR / "population_temporal_manifest.json"
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
VISIBILITY_DISTANCE_TOLERANCE = 0.05
CANDIDATES = {
    "exact_interval_profile_128": (0.0, 0.0),
    "center_minus_2pct_width_exact": (-0.02, 0.0),
    "center_plus_2pct_width_exact": (0.02, 0.0),
    "center_exact_width_minus_2pct": (0.0, -0.02),
    "center_exact_width_plus_2pct": (0.0, 0.02),
}


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def norm(v):
    return math.sqrt(dot(v, v))


def normalize(v):
    length = norm(v)
    return tuple(component / length for component in v)


def project(point, camera_x):
    origin, right, up, forward = sequence.camera_frame(camera_x)
    relative = sub(point, origin)
    z = dot(relative, forward)
    if z <= 1e-6:
        return None
    aspect = scene.WIDTH / scene.HEIGHT
    tangent = math.tan(math.radians(scene.FOV_DEGREES) * 0.5)
    nx = dot(relative, right) / (z * tangent * aspect)
    ny = dot(relative, up) / (z * tangent)
    px = (nx + 1.0) * scene.WIDTH * 0.5 - 0.5
    py = (1.0 - ny) * scene.HEIGHT * 0.5 - 0.5
    if px < 0.0 or py < 0.0 or px >= scene.WIDTH or py >= scene.HEIGHT:
        return None
    distance = norm(relative)
    return px, py, distance


def relative_rgb_l2(a, b):
    error = sum((a[c] - b[c]) ** 2 for c in range(3))
    norm_squared = sum(value * value for value in b)
    return math.sqrt(error / max(norm_squared, 1e-30))


def aggregate(rows):
    if not rows:
        return {"pair_count": 0}
    max_channels = sorted(row["temporal_residual_max_channel"] for row in rows)
    squared = sum(row["temporal_residual_l2_squared"] for row in rows)
    return {
        "pair_count": len(rows),
        "temporal_residual_rmse_rgb": math.sqrt(squared / (3 * len(rows))),
        "mean_temporal_residual_max_channel": sum(max_channels) / len(max_channels),
        "p95_temporal_residual_max_channel": max_channels[math.ceil(0.95 * len(rows)) - 1],
        "max_temporal_residual_max_channel": max(max_channels),
    }


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    center_frame = len(sequence.CAMERA_X) // 2
    center_x = sequence.CAMERA_X[center_frame]
    center_origin, center_right, center_up, center_forward = sequence.camera_frame(center_x)

    intervals_by_pixel = []
    source_points = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, center_right, center_up, center_forward)
            _color, distance, surface = scene.scene_sample(center_origin, ray)
            intervals = sequence.ray_box_interval(center_origin, ray, distance) or ()
            intervals_by_pixel.append(intervals)
            if not intervals:
                continue
            point = tuple(center_origin[i] + ray[i] * distance for i in range(3))
            source_points.append({"pixel": [x, y], "world_point": point,
                                  "surface": surface, "intervals": intervals})
    _inside, edge_indices = sequence.boundary_mask(intervals_by_pixel)
    edge_set = set(edge_indices)
    for point in source_points:
        x, y = point["pixel"]
        point["region"] = "silhouette_edge" if y * scene.WIDTH + x in edge_set else "shadow_interior"

    sun_elevation = math.radians(45.0)
    sun_azimuth = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_elevation) * math.sin(sun_azimuth),
                                math.sin(sun_elevation),
                                math.cos(sun_elevation) * math.cos(sun_azimuth)))
    cache = {}
    cache_validation_max = 0.0
    for point_index, point in enumerate(source_points):
        world_point = point["world_point"]
        for frame_index, camera_x in enumerate(sequence.CAMERA_X):
            projected = project(world_point, camera_x)
            if projected is None:
                continue
            px, py, distance = projected
            origin, _right, _up, _forward = sequence.camera_frame(camera_x)
            direction = normalize(sub(world_point, origin))
            _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
            if (hit_surface != point["surface"]
                    or abs(hit_distance - distance) > VISIBILITY_DISTANCE_TOLERANCE):
                continue
            true_intervals = sequence.ray_box_interval(origin, direction, distance) or ()
            if not true_intervals:
                continue
            case = {"camera_y": origin[1], "view_direction": direction,
                    "distance": distance, "sun_direction": sun}
            reference = soft.integrate_cells(
                case, soft.reference_cells(case, true_intervals, sequence.PENUMBRA_WIDTH,
                                           REFERENCE_STEPS),
                true_intervals, sequence.PENUMBRA_WIDTH)
            validation = soft.integrate_cells(
                case, soft.reference_cells(case, true_intervals, sequence.PENUMBRA_WIDTH,
                                           VALIDATION_STEPS),
                true_intervals, sequence.PENUMBRA_WIDTH)
            validation_error = relative_rgb_l2(validation, reference)
            cache_validation_max = max(cache_validation_max, validation_error)
            candidate_values = {}
            for name, (center_offset, width_error) in CANDIDATES.items():
                estimated = combined.estimate_intervals(true_intervals, center_offset, width_error)
                cells = soft.weighted_cells(origin[1], direction[1], distance,
                                            CANDIDATE_STEPS, estimated,
                                            sequence.PENUMBRA_WIDTH)
                candidate_values[name] = soft.integrate_cells(
                    case, cells, true_intervals, sequence.PENUMBRA_WIDTH)
            cache[(point_index, frame_index)] = {
                "projected_xy": (px, py), "distance": distance,
                "true_intervals": true_intervals, "reference_rgb": reference,
                "validation_error": validation_error, "candidates": candidate_values,
            }

    pair_rows = []
    pair_counts = []
    for frame_index in range(len(sequence.CAMERA_X) - 1):
        visible_points = 0
        for point_index, point in enumerate(source_points):
            previous = cache.get((point_index, frame_index))
            current = cache.get((point_index, frame_index + 1))
            if previous is None or current is None:
                continue
            visible_points += 1
            for method in CANDIDATES:
                prev_residual = tuple(previous["candidates"][method][c] - previous["reference_rgb"][c]
                                      for c in range(3))
                curr_residual = tuple(current["candidates"][method][c] - current["reference_rgb"][c]
                                      for c in range(3))
                delta = tuple(curr_residual[c] - prev_residual[c] for c in range(3))
                pair_rows.append({
                    "pair_index": frame_index,
                    "previous_camera_x": sequence.CAMERA_X[frame_index],
                    "current_camera_x": sequence.CAMERA_X[frame_index + 1],
                    "method": method, "source_pixel": point["pixel"],
                    "world_point": point["world_point"], "surface": point["surface"],
                    "source_region": point["region"],
                    "previous_projected_xy": previous["projected_xy"],
                    "current_projected_xy": current["projected_xy"],
                    "previous_true_intervals": previous["true_intervals"],
                    "current_true_intervals": current["true_intervals"],
                    "previous_reference_validation_relative_rgb_l2": previous["validation_error"],
                    "current_reference_validation_relative_rgb_l2": current["validation_error"],
                    "temporal_residual_delta_rgb": delta,
                    "temporal_residual_max_channel": max(abs(value) for value in delta),
                    "temporal_residual_l2_squared": sum(value * value for value in delta),
                })
        pair_counts.append({"pair_index": frame_index,
                            "visible_correspondence_count": visible_points})

    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pair_rows[0]))
        writer.writeheader()
        writer.writerows(pair_rows)

    summary = {}
    for method in CANDIDATES:
        method_rows = [row for row in pair_rows if row["method"] == method]
        summary[method] = {"all": aggregate(method_rows),
                           "by_region": {region: aggregate([row for row in method_rows
                                                            if row["source_region"] == region])
                                         for region in ("silhouette_edge", "shadow_interior")},
                           "by_surface": {surface: aggregate([row for row in method_rows
                                                              if row["surface"] == surface])
                                          for surface in sorted({row["surface"] for row in method_rows})},
                           "by_pair": {str(pair): aggregate([row for row in method_rows
                                                            if row["pair_index"] == pair])
                                       for pair in range(len(sequence.CAMERA_X) - 1)}}

    manifest = {
        "schema": "fog-shadow-population-temporal-direct-ray-rd-v1",
        "purpose": "Measure camera-motion temporal residual at known shadowed world points using direct continuous rays.",
        "execution_scope": "Center-frame shadowed pixel-center world points, projected into adjacent camera frames; same world point re-integrated on each continuous ray, no PFM/image interpolation.",
        "sequence_script_sha256": hashlib.sha256(Path("tools/fog_shadow_camera_sequence_rd.py").read_bytes()).hexdigest(),
        "source_shadowed_world_point_count": len(source_points),
        "center_source_camera_x": center_x,
        "pair_visible_correspondence_counts": pair_counts,
        "visibility_distance_tolerance": VISIBILITY_DISTANCE_TOLERANCE,
        "reference_steps": REFERENCE_STEPS, "validation_steps": VALIDATION_STEPS,
        "candidate_steps": CANDIDATE_STEPS, "reference_validation_max_relative_rgb_l2": cache_validation_max,
        "candidates": CANDIDATES,
        "temporal_acceptance_gate": None,
        "raw_result_count": len(pair_rows),
        "raw_csv": {"path": RAW_CSV.as_posix(),
                    "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "summary": summary,
        "limitations": [
            "Known analytic scene correspondences are not optical-flow performance or production motion vectors.",
            "AABB interval and cubic 1% smoothstep are synthetic visibility proxies, not geometric shadow maps.",
            "This measures direct-ray numerical residual only; no perceptual temporal acceptance threshold, GPU runtime, alpha, or performance is covered.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("source points", len(source_points), "visible pairs", pair_counts,
          "raw rows", len(pair_rows), "reference validation max", cache_validation_max)
    for method, values in summary.items():
        print(method, values["all"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
