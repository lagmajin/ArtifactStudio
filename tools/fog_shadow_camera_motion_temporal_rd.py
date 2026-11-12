#!/usr/bin/env python3
"""Measure direct-ray temporal residual under camera-translation amplitude changes."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_box_extent_temporal_rd as geometry
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
RAW = ROOT / "camera_motion_temporal_sweep.csv"
MANIFEST = ROOT / "camera_motion_temporal_manifest.json"
MOTION_AMPLITUDES = (0.5, 1.0, 2.0)
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
VISIBILITY_DISTANCE_TOLERANCE = 0.05
CANDIDATES = {
    "exact_interval_profile_128": (0.0, 0.0),
    "center_minus_2pct_width_exact": (-0.02, 0.0),
    "center_plus_2pct_width_exact": (0.02, 0.0),
}


def aggregate(rows):
    if not rows:
        return {"pair_count": 0}
    peaks = sorted(row["temporal_residual_max_channel"] for row in rows)
    squared = sum(row["temporal_residual_l2_squared"] for row in rows)
    return {
        "pair_count": len(rows),
        "temporal_residual_rmse_rgb": math.sqrt(squared / (3 * len(rows))),
        "mean_temporal_residual_max_channel": sum(peaks) / len(peaks),
        "p95_temporal_residual_max_channel": peaks[math.ceil(0.95 * len(peaks)) - 1],
        "max_temporal_residual_max_channel": peaks[-1],
    }


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    center_frame = len(sequence.CAMERA_X) // 2
    center_origin, right, up, forward = sequence.camera_frame(0.0)
    points = []
    intervals_by_pixel = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            _color, distance, surface = scene.scene_sample(center_origin, ray)
            intervals = sequence.ray_box_interval(center_origin, ray, distance) or ()
            intervals_by_pixel.append(intervals)
            if intervals:
                world_point = tuple(center_origin[i] + ray[i] * distance for i in range(3))
                points.append({"source_pixel": [x, y], "world_point": world_point,
                               "surface": surface})
    _inside, edge_indices = sequence.boundary_mask(intervals_by_pixel)
    edge_set = set(edge_indices)
    for point in points:
        x, y = point["source_pixel"]
        point["source_region"] = ("silhouette_edge" if y * scene.WIDTH + x in edge_set
                                  else "shadow_interior")

    sun_elevation = math.radians(45.0)
    sun_azimuth = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_elevation) * math.sin(sun_azimuth),
                                math.sin(sun_elevation),
                                math.cos(sun_elevation) * math.cos(sun_azimuth)))
    scenario_rows = {}
    scenario_info = {}
    max_reference_validation = 0.0
    for amplitude in MOTION_AMPLITUDES:
        camera_xs = tuple(x * amplitude for x in sequence.CAMERA_X)
        cache = {}
        visible_shadow_rays = 0
        for point_index, point in enumerate(points):
            for frame_index, camera_x in enumerate(camera_xs):
                projected = geometry.project(point["world_point"], camera_x)
                if projected is None:
                    continue
                px, py, distance = projected
                origin, _r, _u, _f = sequence.camera_frame(camera_x)
                direction = geometry.normalize(geometry.sub(point["world_point"], origin))
                _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                if (hit_surface != point["surface"]
                        or abs(hit_distance - distance) > VISIBILITY_DISTANCE_TOLERANCE):
                    continue
                geometry_hit = geometry.box_interval(
                    origin, direction, distance,
                    sequence.SHADOW_VOLUME_MIN, sequence.SHADOW_VOLUME_MAX)
                if geometry_hit is None:
                    continue
                visible_shadow_rays += 1
                true_intervals = geometry_hit["intervals"]
                case = {"camera_y": origin[1], "view_direction": direction,
                        "distance": distance, "sun_direction": sun}
                reference = soft.integrate_cells(
                    case, soft.reference_cells(case, true_intervals,
                                               sequence.PENUMBRA_WIDTH, REFERENCE_STEPS),
                    true_intervals, sequence.PENUMBRA_WIDTH)
                validation = soft.integrate_cells(
                    case, soft.reference_cells(case, true_intervals,
                                               sequence.PENUMBRA_WIDTH, VALIDATION_STEPS),
                    true_intervals, sequence.PENUMBRA_WIDTH)
                validation_error = math.sqrt(sum((validation[c] - reference[c]) ** 2
                                                 for c in range(3))
                                             / max(sum(reference[c] ** 2 for c in range(3)), 1e-30))
                max_reference_validation = max(max_reference_validation, validation_error)
                candidates = {}
                for name, (center_offset, width_error) in CANDIDATES.items():
                    estimated = combined.estimate_intervals(true_intervals,
                                                            center_offset, width_error)
                    cells = soft.weighted_cells(origin[1], direction[1], distance,
                                                CANDIDATE_STEPS, estimated,
                                                sequence.PENUMBRA_WIDTH)
                    candidates[name] = soft.integrate_cells(
                        case, cells, true_intervals, sequence.PENUMBRA_WIDTH)
                cache[(point_index, frame_index)] = {
                    "projected_xy": (px, py), "distance": distance,
                    "geometry": geometry_hit, "reference": reference,
                    "validation_error": validation_error, "candidates": candidates,
                }

        rows = []
        pair_counts = []
        for pair_index in range(len(camera_xs) - 1):
            count = 0
            for point_index, point in enumerate(points):
                previous = cache.get((point_index, pair_index))
                current = cache.get((point_index, pair_index + 1))
                if previous is None or current is None:
                    continue
                count += 1
                for method in CANDIDATES:
                    prev_residual = tuple(previous["candidates"][method][c] - previous["reference"][c]
                                          for c in range(3))
                    curr_residual = tuple(current["candidates"][method][c] - current["reference"][c]
                                          for c in range(3))
                    delta = tuple(curr_residual[c] - prev_residual[c] for c in range(3))
                    rows.append({
                        "motion_amplitude": amplitude, "pair_index": pair_index,
                        "previous_camera_x": camera_xs[pair_index],
                        "current_camera_x": camera_xs[pair_index + 1],
                        "source_point_index": point_index, "source_pixel": point["source_pixel"],
                        "world_point": point["world_point"], "surface": point["surface"],
                        "source_region": point["source_region"], "method": method,
                        "previous_projected_xy": previous["projected_xy"],
                        "current_projected_xy": current["projected_xy"],
                        "previous_interval": previous["geometry"]["intervals"],
                        "current_interval": current["geometry"]["intervals"],
                        "previous_entry_face": previous["geometry"]["entry_face"],
                        "current_entry_face": current["geometry"]["entry_face"],
                        "previous_exit_face": previous["geometry"]["exit_face"],
                        "current_exit_face": current["geometry"]["exit_face"],
                        "previous_reference_validation_relative_rgb_l2": previous["validation_error"],
                        "current_reference_validation_relative_rgb_l2": current["validation_error"],
                        "temporal_residual_delta_rgb": delta,
                        "temporal_residual_max_channel": max(abs(value) for value in delta),
                        "temporal_residual_l2_squared": sum(value * value for value in delta),
                    })
            pair_counts.append({"pair_index": pair_index,
                                "visible_correspondence_count": count})
        scenario_rows[amplitude] = rows
        scenario_info[f"{amplitude:.2f}"] = {
            "camera_x_positions": camera_xs,
            "visible_shadowed_point_frame_rays": visible_shadow_rays,
            "pair_visible_correspondence_counts": pair_counts,
            "pair_count": sum(item["visible_correspondence_count"] for item in pair_counts),
            "methods": {name: {
                "all": aggregate([row for row in rows if row["method"] == name]),
                "by_region": {region: aggregate([row for row in rows
                                                  if row["method"] == name
                                                  and row["source_region"] == region])
                              for region in ("silhouette_edge", "shadow_interior")},
            } for name in CANDIDATES},
        }

    # Common-support comparison uses the intersection of point/pair keys present
    # in every trajectory, so sample-count changes do not redefine the cohort.
    keys_by_amplitude = {
        amplitude: {(row["pair_index"], row["source_point_index"])
                    for row in rows if row["method"] == "exact_interval_profile_128"}
        for amplitude, rows in scenario_rows.items()
    }
    common_keys = set.intersection(*(keys_by_amplitude[a] for a in MOTION_AMPLITUDES))
    common_summary = {}
    for amplitude, rows in scenario_rows.items():
        common_summary[f"{amplitude:.2f}"] = {
            method: aggregate([row for row in rows
                               if row["method"] == method
                               and (row["pair_index"], row["source_point_index"]) in common_keys])
            for method in CANDIDATES
        }
    baseline_rows = [row for row in scenario_rows[1.0]
                     if row["method"] == "center_plus_2pct_width_exact"]
    baseline_worst = max(baseline_rows, key=lambda row: row["temporal_residual_max_channel"])
    worst_key = (baseline_worst["pair_index"], baseline_worst["source_point_index"])
    outlier_presence = {}
    for amplitude, rows in scenario_rows.items():
        match = next((row for row in rows if row["method"] == "center_plus_2pct_width_exact"
                      and (row["pair_index"], row["source_point_index"]) == worst_key), None)
        outlier_presence[f"{amplitude:.2f}"] = None if match is None else {
            "temporal_residual_max_channel": match["temporal_residual_max_channel"],
            "world_point": match["world_point"], "source_pixel": match["source_pixel"],
            "previous_interval": match["previous_interval"],
            "current_interval": match["current_interval"],
            "previous_camera_x": match["previous_camera_x"],
            "current_camera_x": match["current_camera_x"],
        }

    all_rows = [row for rows in scenario_rows.values() for row in rows]
    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(all_rows[0]))
        writer.writeheader()
        writer.writerows(all_rows)

    manifest = {
        "schema": "fog-shadow-camera-motion-temporal-rd-v1",
        "purpose": "Measure direct-ray temporal residual sensitivity to camera translation amplitude with fixed AABB and world points.",
        "execution_scope": "Same fixed world-space AABB and 440 center-frame shadowed world points; only horizontal camera translation amplitude changes.",
        "source_point_count": len(points), "center_frame": center_frame,
        "base_camera_x_positions": sequence.CAMERA_X,
        "motion_amplitudes": MOTION_AMPLITUDES,
        "reference_steps": REFERENCE_STEPS, "validation_steps": VALIDATION_STEPS,
        "candidate_steps": CANDIDATE_STEPS, "visibility_distance_tolerance": VISIBILITY_DISTANCE_TOLERANCE,
        "candidate_parameters": CANDIDATES,
        "maximum_reference_validation_relative_rgb_l2": max_reference_validation,
        "scenarios": scenario_info,
        "common_support_pair_count": len(common_keys),
        "common_support_summary": common_summary,
        "baseline_max_center_plus_world_point": {
            "pair_index": baseline_worst["pair_index"],
            "source_point_index": baseline_worst["source_point_index"],
            "source_pixel": baseline_worst["source_pixel"],
            "world_point": baseline_worst["world_point"],
            "baseline_residual": baseline_worst["temporal_residual_max_channel"],
            "presence_by_amplitude": outlier_presence,
        },
        "temporal_acceptance_threshold": None,
        "raw_result_count": len(all_rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "Camera positions are equally spaced by frame index; no physical velocity, acceleration, shutter, or motion blur is modeled.",
            "Correspondences are analytic synthetic-scene world points and do not evaluate optical flow or production motion vectors.",
            "AABB and cubic smoothstep are synthetic shadow proxies; no geometric shadow map, GPU, production runtime, alpha, or performance is covered.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("source points", len(points), "max ref validation", max_reference_validation,
          "raw rows", len(all_rows), "common pairs", len(common_keys))
    for amplitude, info in scenario_info.items():
        print("amplitude", amplitude, "pairs", info["pair_count"],
              "exact", info["methods"]["exact_interval_profile_128"]["all"],
              "center+", info["methods"]["center_plus_2pct_width_exact"]["all"])
    print("baseline max point", manifest["baseline_max_center_plus_world_point"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
