#!/usr/bin/env python3
"""Compare width-error temporal sensitivity across volume-preserving AABB shapes."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


ARTIFACTS = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
RAW = ARTIFACTS / "box_aspect_width_temporal.csv"
MANIFEST = ARTIFACTS / "box_aspect_width_manifest.json"
SHAPES = {
    "baseline": (2.2, 2.5, 0.75),
    "x_long_z_thin": (3.3, 2.5, 0.5),
    "x_short_z_thick": (2.2 / 1.5, 2.5, 1.125),
}
AMPLITUDES = (0.5, 1.0, 2.0)
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
WIDTH_ERROR = 0.02
VISIBILITY_DISTANCE_TOLERANCE = 0.05
PENUMBRA = sequence.PENUMBRA_WIDTH
SUN_ELEVATION = math.radians(45.0)
SUN_AZIMUTH = math.radians(scattering.SUN_AZIMUTH_DEGREES)
SUN = scattering.normalize((math.cos(SUN_ELEVATION) * math.sin(SUN_AZIMUTH),
                            math.sin(SUN_ELEVATION),
                            math.cos(SUN_ELEVATION) * math.cos(SUN_AZIMUTH)))


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def norm(a):
    return math.sqrt(dot(a, a))


def normalize(a):
    length = norm(a)
    return tuple(value / length for value in a)


def box_interval(origin, direction, distance, center, half_extents):
    box_min = tuple(center[i] - half_extents[i] for i in range(3))
    box_max = tuple(center[i] + half_extents[i] for i in range(3))
    near_t, far_t = 0.0, distance
    near_face, far_face = "camera_origin", "surface"
    for axis, name in enumerate("xyz"):
        if abs(direction[axis]) < 1.0e-10:
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
    if far_t <= 0.0 or near_t >= distance:
        return None
    start, end = max(0.0, near_t), min(distance, far_t)
    if end <= start:
        return None
    return {
        "intervals": ((start / distance, end / distance),),
        "entry_face": near_face,
        "exit_face": far_face,
        "entry": start / distance,
        "exit": end / distance,
        "width": (end - start) / distance,
    }


def project(point, camera_x):
    origin, right, up, forward = sequence.camera_frame(camera_x)
    relative = sub(point, origin)
    z = dot(relative, forward)
    if z <= 1.0e-6:
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


def correlation(x, y):
    if len(x) < 2:
        return None

    def ranks(values):
        order = sorted(range(len(values)), key=lambda i: (values[i], i))
        result = [0.0] * len(values)
        first = 0
        while first < len(order):
            last = first + 1
            while last < len(order) and values[order[last]] == values[order[first]]:
                last += 1
            rank = ((first + 1) + last) / 2.0
            for j in range(first, last):
                result[order[j]] = rank
            first = last
        return result

    rx, ry = ranks(x), ranks(y)
    mx, my = sum(rx) / len(rx), sum(ry) / len(ry)
    dx, dy = [v - mx for v in rx], [v - my for v in ry]
    denominator = math.sqrt(sum(v * v for v in dx) * sum(v * v for v in dy))
    return sum(a * b for a, b in zip(dx, dy)) / denominator if denominator else None


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return {"count": 0, "p50": None, "p95": None, "max": None}
    return {
        "count": len(ordered),
        "p50": ordered[(len(ordered) - 1) // 2],
        "p95": ordered[math.ceil(0.95 * len(ordered)) - 1],
        "max": ordered[-1],
    }


def summarize(rows):
    residuals = [row["temporal_residual_max_channel"] for row in rows]
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["source_point_index"]].append(row)
    point_rows = [max(items, key=lambda row: row["temporal_residual_max_channel"])
                  for items in grouped.values()]
    return {
        "pair_residual": distribution(residuals),
        "unique_point_max_residual": distribution(
            [row["temporal_residual_max_channel"] for row in point_rows]),
        "pair_abs_exit_delta_spearman": correlation(
            [abs(row["signed_exit_delta"]) for row in rows], residuals),
        "pair_abs_width_delta_spearman": correlation(
            [abs(row["signed_width_delta"]) for row in rows], residuals),
        "unique_point_max_abs_exit_delta_spearman": correlation(
            [abs(row["signed_exit_delta"]) for row in point_rows],
            [row["temporal_residual_max_channel"] for row in point_rows]),
        "unique_point_max_abs_width_delta_spearman": correlation(
            [abs(row["signed_width_delta"]) for row in point_rows],
            [row["temporal_residual_max_channel"] for row in point_rows]),
    }


def main():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    base_xs = sequence.CAMERA_X
    center_x = 0.0
    center_origin, right, up, forward = sequence.camera_frame(center_x)
    source_points = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            _color, distance, surface = scene.scene_sample(center_origin, ray)
            if sequence.ray_box_interval(center_origin, ray, distance):
                point = tuple(center_origin[i] + ray[i] * distance for i in range(3))
                source_points.append({"source_point_index": len(source_points),
                                      "source_pixel": (x, y),
                                      "world_point": point,
                                      "surface": surface})

    center = tuple((sequence.SHADOW_VOLUME_MIN[i] + sequence.SHADOW_VOLUME_MAX[i]) * 0.5
                   for i in range(3))
    all_rows = []
    summaries = {}
    support = {}
    validation_max = 0.0
    for shape_name, half_extents in SHAPES.items():
        summaries[shape_name] = {}
        support[shape_name] = {}
        for amplitude in AMPLITUDES:
            camera_xs = tuple(value * amplitude for value in base_xs)
            frame_cache = {}
            for point in source_points:
                index = point["source_point_index"]
                for frame, camera_x in enumerate(camera_xs):
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
                    geometry = box_interval(origin, direction, distance, center, half_extents)
                    if geometry is None:
                        continue
                    intervals = geometry["intervals"]
                    case = {"camera_y": origin[1], "view_direction": direction,
                            "distance": distance, "sun_direction": SUN}
                    reference = soft.integrate_cells(
                        case, soft.reference_cells(case, intervals, PENUMBRA, REFERENCE_STEPS),
                        intervals, PENUMBRA)
                    validation = soft.integrate_cells(
                        case, soft.reference_cells(case, intervals, PENUMBRA, VALIDATION_STEPS),
                        intervals, PENUMBRA)
                    reference_norm = math.sqrt(sum(value * value for value in reference))
                    validation_error = math.sqrt(sum((validation[c] - reference[c]) ** 2
                                                     for c in range(3))
                                                  / max(reference_norm * reference_norm, 1e-30))
                    validation_max = max(validation_max, validation_error)
                    estimated = combined.estimate_intervals(intervals, 0.0, WIDTH_ERROR)
                    cells = soft.weighted_cells(origin[1], direction[1], distance,
                                                CANDIDATE_STEPS, estimated, PENUMBRA)
                    candidate = soft.integrate_cells(case, cells, intervals, PENUMBRA)
                    frame_cache[(index, frame)] = {
                        "projected_xy": (px, py), "distance": distance,
                        "geometry": geometry, "reference": reference,
                        "candidate": candidate, "validation_error": validation_error,
                    }

            scenario_rows = []
            for pair_index in range(len(camera_xs) - 1):
                for point in source_points:
                    index = point["source_point_index"]
                    previous = frame_cache.get((index, pair_index))
                    current = frame_cache.get((index, pair_index + 1))
                    if previous is None or current is None:
                        continue
                    prev_residual = tuple(previous["candidate"][c] - previous["reference"][c]
                                          for c in range(3))
                    curr_residual = tuple(current["candidate"][c] - current["reference"][c]
                                          for c in range(3))
                    delta = tuple(curr_residual[c] - prev_residual[c] for c in range(3))
                    prev_geo, curr_geo = previous["geometry"], current["geometry"]
                    row = {
                        "shape": shape_name, "half_extent_x": half_extents[0],
                        "half_extent_y": half_extents[1], "half_extent_z": half_extents[2],
                        "motion_amplitude": amplitude, "pair_index": pair_index,
                        "source_point_index": index, "source_pixel": point["source_pixel"],
                        "world_point": point["world_point"], "surface": point["surface"],
                        "previous_camera_x": camera_xs[pair_index],
                        "current_camera_x": camera_xs[pair_index + 1],
                        "previous_entry_face": prev_geo["entry_face"],
                        "current_entry_face": curr_geo["entry_face"],
                        "previous_exit_face": prev_geo["exit_face"],
                        "current_exit_face": curr_geo["exit_face"],
                        "previous_entry": prev_geo["entry"], "current_entry": curr_geo["entry"],
                        "previous_exit": prev_geo["exit"], "current_exit": curr_geo["exit"],
                        "previous_width": prev_geo["width"], "current_width": curr_geo["width"],
                        "signed_entry_delta": curr_geo["entry"] - prev_geo["entry"],
                        "signed_exit_delta": curr_geo["exit"] - prev_geo["exit"],
                        "signed_width_delta": curr_geo["width"] - prev_geo["width"],
                        "entry_face_changed": prev_geo["entry_face"] != curr_geo["entry_face"],
                        "exit_face_changed": prev_geo["exit_face"] != curr_geo["exit_face"],
                        "temporal_residual_rgb": delta,
                        "temporal_residual_max_channel": max(abs(value) for value in delta),
                        "previous_reference_validation_relative_rgb_l2": previous["validation_error"],
                        "current_reference_validation_relative_rgb_l2": current["validation_error"],
                    }
                    scenario_rows.append(row)

            all_rows.extend(scenario_rows)
            key = f"{amplitude:.2f}"
            summaries[shape_name][key] = {
                "valid_pair_rows": len(scenario_rows),
                "unique_points": len({row["source_point_index"] for row in scenario_rows}),
                "face_switch_pair_rows": sum(row["entry_face_changed"] or row["exit_face_changed"]
                                              for row in scenario_rows),
                "metrics": summarize(scenario_rows),
            }
            support[shape_name][key] = sorted({
                (row["pair_index"], row["source_point_index"]) for row in scenario_rows
            })
            print("finished", shape_name, "amplitude", amplitude,
                  "pairs", len(scenario_rows), "unique points",
                  summaries[shape_name][key]["unique_points"])

    common_support = {}
    for amplitude in AMPLITUDES:
        key = f"{amplitude:.2f}"
        common = set.intersection(*(set(support[shape][key]) for shape in SHAPES))
        common_support[key] = {
            "pair_point_rows": len(common),
            "unique_points": len({point for _pair, point in common}),
            "fraction_of_baseline_pairs": len(common) / max(len(support["baseline"][key]), 1),
        }
        for shape_name in SHAPES:
            shape_rows = [row for row in all_rows if row["shape"] == shape_name
                          and row["motion_amplitude"] == amplitude
                          and (row["pair_index"], row["source_point_index"]) in common]
            summaries[shape_name][key]["common_support_pair_rows"] = len(shape_rows)
            summaries[shape_name][key]["common_support_metrics"] = summarize(shape_rows)

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(all_rows[0]))
        writer.writeheader()
        writer.writerows(all_rows)
    source_paths = [Path(__file__), Path(scene.__file__), Path(sequence.__file__),
                    Path(combined.__file__), Path(soft.__file__), Path(scattering.__file__)]
    manifest = {
        "schema": "fog-shadow-volume-preserving-box-aspect-rd-v1",
        "purpose": "Compare temporal sensitivity to a +2% estimated core width over volume-preserving AABB aspect-ratio scenarios.",
        "source_point_count": len(source_points),
        "source_selection": "Baseline-shape center-frame pixel-center rays intersecting the original AABB, world points held fixed across shape scenarios.",
        "box_center": center,
        "shapes_half_extents": SHAPES,
        "box_volume_each": {name: 8.0 * math.prod(extents) for name, extents in SHAPES.items()},
        "camera_amplitudes": AMPLITUDES,
        "camera_x_world_units": {f"{amp:.2f}": [x * amp for x in base_xs]
                                 for amp in AMPLITUDES},
        "candidate": {"center_offset": 0.0, "core_width_error": WIDTH_ERROR,
                      "steps": CANDIDATE_STEPS},
        "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS,
        "maximum_reference_validation_relative_rgb_l2": validation_max,
        "penumbra_ray_length_fraction": PENUMBRA,
        "common_support": common_support,
        "scenario_summaries": summaries,
        "raw_csv": {"path": RAW.as_posix(), "rows": len(all_rows),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "input_scripts": [{"path": path.as_posix(),
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in source_paths],
        "limitations": [
            "All shapes share a source-point cohort selected using the baseline AABB; changed silhouettes and lost/gained support affect inclusion.",
            "The common support is an intersection of visible source point/pair keys and may exclude shape-specific difficult regions.",
            "This is a single synthetic scene, single sun configuration, direct-ray visibility AABB, CPU single-scattering fixture.",
            "No visual/perceptual judgment, real shadow filtering, alpha/color integration, GPU/backend parity, or runtime result is implied.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("maximum reference validation", validation_max)
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
