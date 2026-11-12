#!/usr/bin/env python3
"""Re-evaluate direct-ray temporal residual while varying only AABB X extent."""

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
import fog_sun_transmittance_rd as scattering


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
RAW = ROOT / "box_extent_temporal_sweep.csv"
MANIFEST = ROOT / "box_extent_temporal_manifest.json"
X_HALF_EXTENT_SCALES = (0.75, 1.0, 1.25)
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
VISIBILITY_DISTANCE_TOLERANCE = 0.05
CANDIDATES = {
    "exact_interval_profile_128": (0.0, 0.0),
    "center_minus_2pct_width_exact": (-0.02, 0.0),
    "center_plus_2pct_width_exact": (0.02, 0.0),
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
    return px, py, norm(relative)


def box_interval(origin, direction, max_distance, box_min, box_max):
    near_t, far_t = 0.0, max_distance
    near_face, far_face = "camera_origin", "surface"
    for axis, name in enumerate("xyz"):
        if abs(direction[axis]) < 1e-10:
            if origin[axis] < box_min[axis] or origin[axis] > box_max[axis]:
                return None
            continue
        low_t = (box_min[axis] - origin[axis]) / direction[axis]
        high_t = (box_max[axis] - origin[axis]) / direction[axis]
        if low_t <= high_t:
            axis_near, axis_far = low_t, high_t
            face_near, face_far = f"{name}:min", f"{name}:max"
        else:
            axis_near, axis_far = high_t, low_t
            face_near, face_far = f"{name}:max", f"{name}:min"
        if axis_near > near_t:
            near_t, near_face = axis_near, face_near
        if axis_far < far_t:
            far_t, far_face = axis_far, face_far
        if far_t <= near_t:
            return None
    if far_t <= 0.0 or near_t >= max_distance:
        return None
    start, end = max(0.0, near_t), min(max_distance, far_t)
    if end <= start:
        return None
    return {"intervals": ((start / max_distance, end / max_distance),),
            "entry_face": near_face, "exit_face": far_face,
            "entry_t": start, "exit_t": end}


def temporal_metrics(rows):
    if not rows:
        return {"pair_count": 0}
    peak = sorted(row["temporal_residual_max_channel"] for row in rows)
    squared = sum(row["temporal_residual_l2_squared"] for row in rows)
    return {"pair_count": len(rows),
            "temporal_residual_rmse_rgb": math.sqrt(squared / (3 * len(rows))),
            "mean_temporal_residual_max_channel": sum(peak) / len(peak),
            "p95_temporal_residual_max_channel": peak[math.ceil(0.95 * len(peak)) - 1],
            "max_temporal_residual_max_channel": peak[-1]}


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    center_frame = len(sequence.CAMERA_X) // 2
    center_x = sequence.CAMERA_X[center_frame]
    center_origin, right, up, forward = sequence.camera_frame(center_x)
    points = []
    intervals_by_pixel = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            _color, distance, surface = scene.scene_sample(center_origin, ray)
            truth = sequence.ray_box_interval(center_origin, ray, distance) or ()
            intervals_by_pixel.append(truth)
            if truth:
                point = tuple(center_origin[i] + ray[i] * distance for i in range(3))
                points.append({"source_pixel": [x, y], "world_point": point,
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
    all_rows = []
    scenario_info = {}
    validation_max = 0.0
    half_x = (sequence.SHADOW_VOLUME_MAX[0] - sequence.SHADOW_VOLUME_MIN[0]) * 0.5
    center_box_x = (sequence.SHADOW_VOLUME_MAX[0] + sequence.SHADOW_VOLUME_MIN[0]) * 0.5

    for scale in X_HALF_EXTENT_SCALES:
        box_min = list(sequence.SHADOW_VOLUME_MIN)
        box_max = list(sequence.SHADOW_VOLUME_MAX)
        box_min[0] = center_box_x - half_x * scale
        box_max[0] = center_box_x + half_x * scale
        box_min, box_max = tuple(box_min), tuple(box_max)
        cache = {}
        hit_ray_count = 0
        for point_index, point in enumerate(points):
            for frame_index, camera_x in enumerate(sequence.CAMERA_X):
                projected = project(point["world_point"], camera_x)
                if projected is None:
                    continue
                px, py, distance = projected
                origin, _r, _u, _f = sequence.camera_frame(camera_x)
                direction = normalize(sub(point["world_point"], origin))
                _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                if (hit_surface != point["surface"]
                        or abs(hit_distance - distance) > VISIBILITY_DISTANCE_TOLERANCE):
                    continue
                geometry = box_interval(origin, direction, distance, box_min, box_max)
                if geometry is None:
                    continue
                hit_ray_count += 1
                case = {"camera_y": origin[1], "view_direction": direction,
                        "distance": distance, "sun_direction": sun}
                reference = soft.integrate_cells(
                    case, soft.reference_cells(case, geometry["intervals"],
                                               sequence.PENUMBRA_WIDTH, REFERENCE_STEPS),
                    geometry["intervals"], sequence.PENUMBRA_WIDTH)
                validation = soft.integrate_cells(
                    case, soft.reference_cells(case, geometry["intervals"],
                                               sequence.PENUMBRA_WIDTH, VALIDATION_STEPS),
                    geometry["intervals"], sequence.PENUMBRA_WIDTH)
                validation_error = math.sqrt(sum((validation[c] - reference[c]) ** 2
                                                 for c in range(3))
                                             / max(sum(reference[c] ** 2 for c in range(3)), 1e-30))
                validation_max = max(validation_max, validation_error)
                candidate_values = {}
                for method, (center_offset, width_error) in CANDIDATES.items():
                    estimated = combined.estimate_intervals(geometry["intervals"],
                                                            center_offset, width_error)
                    cells = soft.weighted_cells(origin[1], direction[1], distance,
                                                CANDIDATE_STEPS, estimated,
                                                sequence.PENUMBRA_WIDTH)
                    candidate_values[method] = soft.integrate_cells(
                        case, cells, geometry["intervals"], sequence.PENUMBRA_WIDTH)
                cache[(point_index, frame_index)] = {
                    "projected_xy": (px, py), "distance": distance,
                    "geometry": geometry, "reference": reference,
                    "validation_error": validation_error, "candidate_values": candidate_values}

        rows = []
        for pair_index in range(len(sequence.CAMERA_X) - 1):
            for point_index, point in enumerate(points):
                previous = cache.get((point_index, pair_index))
                current = cache.get((point_index, pair_index + 1))
                if previous is None or current is None:
                    continue
                for method in CANDIDATES:
                    prev_residual = tuple(previous["candidate_values"][method][c]
                                          - previous["reference"][c] for c in range(3))
                    curr_residual = tuple(current["candidate_values"][method][c]
                                          - current["reference"][c] for c in range(3))
                    delta = tuple(curr_residual[c] - prev_residual[c] for c in range(3))
                    rows.append({
                        "x_half_extent_scale": scale, "pair_index": pair_index,
                        "source_pixel": point["source_pixel"], "world_point": point["world_point"],
                        "source_region": point["source_region"], "surface": point["surface"],
                        "method": method,
                        "previous_projected_xy": previous["projected_xy"],
                        "current_projected_xy": current["projected_xy"],
                        "previous_entry_face": previous["geometry"]["entry_face"],
                        "current_entry_face": current["geometry"]["entry_face"],
                        "previous_exit_face": previous["geometry"]["exit_face"],
                        "current_exit_face": current["geometry"]["exit_face"],
                        "previous_interval": previous["geometry"]["intervals"],
                        "current_interval": current["geometry"]["intervals"],
                        "previous_reference_validation_relative_rgb_l2": previous["validation_error"],
                        "current_reference_validation_relative_rgb_l2": current["validation_error"],
                        "temporal_residual_delta_rgb": delta,
                        "temporal_residual_max_channel": max(abs(value) for value in delta),
                        "temporal_residual_l2_squared": sum(value * value for value in delta),
                    })
        scenario_rows = {name: [row for row in rows if row["method"] == name]
                         for name in CANDIDATES}
        scenario_info[f"{scale:.2f}"] = {
            "box_min": box_min, "box_max": box_max,
            "visible_shadowed_point_frame_rays": hit_ray_count,
            "pair_count": len(rows),
            "methods": {name: {
                "all": temporal_metrics(method_rows),
                "by_region": {region: temporal_metrics([row for row in method_rows
                                                         if row["source_region"] == region])
                              for region in ("silhouette_edge", "shadow_interior")},
            } for name, method_rows in scenario_rows.items()},
        }
        all_rows.extend(rows)

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(all_rows[0]))
        writer.writeheader()
        writer.writerows(all_rows)

    manifest = {
        "schema": "fog-shadow-box-x-extent-temporal-rd-v1",
        "purpose": "Sensitivity analysis of direct-ray temporal residual under X-only shadow-volume extent changes.",
        "execution_scope": "Same baseline center-frame shadowed world points, scene, camera sequence, fog parameters, and visibility test; only AABB X half-extent varies.",
        "source_point_count": len(points), "source_center_camera_x": center_x,
        "x_half_extent_scales": X_HALF_EXTENT_SCALES,
        "reference_steps": REFERENCE_STEPS, "validation_steps": VALIDATION_STEPS,
        "candidate_steps": CANDIDATE_STEPS, "visibility_distance_tolerance": VISIBILITY_DISTANCE_TOLERANCE,
        "candidate_parameters": CANDIDATES,
        "maximum_reference_validation_relative_rgb_l2": validation_max,
        "scenarios": scenario_info,
        "raw_result_count": len(all_rows),
        "raw_csv": {"path": RAW.as_posix(), "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "Source points are selected from the baseline AABB shadowed population; changed box scenarios can remove or add interval hits for these same points.",
            "The AABB is a synthetic occlusion proxy; changing its X extent is not equivalent to moving a geometric occluder or changing filtered shadow penumbra.",
            "No perceptual temporal threshold, GPU/runtime, alpha, or performance is evaluated.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("source points", len(points), "max reference validation", validation_max,
          "raw rows", len(all_rows))
    for scale, data in scenario_info.items():
        print("scale", scale, "visible rays", data["visible_shadowed_point_frame_rays"],
              "exact", data["methods"]["exact_interval_profile_128"]["all"],
              "center+", data["methods"]["center_plus_2pct_width_exact"]["all"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
