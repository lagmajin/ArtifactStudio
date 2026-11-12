#!/usr/bin/env python3
"""Factor core-width-driven segmentation from importance-weight allocation."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_height_convergence_sweep_rd as convergence
import fog_height_sampling_distribution_rd as sampling
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_box_extent_temporal_rd as geometry
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
RAW = ROOT / "quadrature_width_factor_temporal.csv"
MANIFEST = ROOT / "quadrature_width_factor_manifest.json"
MOTION_AMPLITUDES = (0.5, 1.0, 2.0)
WIDTH_ERRORS = (-0.02, 0.02)
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
CANDIDATES = {"true_segmentation_true_weights": None}
for sign_name, width_error in (("minus_2pct", -0.02), ("plus_2pct", 0.02)):
    CANDIDATES[f"true_segmentation_estimated_weights_width_{sign_name}"] = ("true", "estimated", width_error)
    CANDIDATES[f"estimated_segmentation_true_weights_width_{sign_name}"] = ("estimated", "true", width_error)
    CANDIDATES[f"estimated_segmentation_estimated_weights_width_{sign_name}"] = ("estimated", "estimated", width_error)


def weighted_cells_split(case, true_intervals, segmentation_intervals, weight_intervals,
                         penumbra, steps):
    camera_y = case["camera_y"]
    view_y = case["view_direction"][1]
    distance = case["distance"]
    knots = soft.profile_knots(segmentation_intervals, penumbra)
    pieces = soft.ground_segments(camera_y, view_y, distance, knots)
    pieces = [piece for piece in pieces
              if soft.visibility(0.5 * (piece[0] + piece[1]) / distance,
                                 segmentation_intervals, penumbra) > 0.0]
    if not pieces:
        return []
    weights = [soft.rho_visibility_weight(camera_y, view_y, start, end, distance,
                                          weight_intervals, penumbra)
               for start, end in pieces]
    counts = convergence.allocate_steps(weights, steps)
    cells = []
    for (start, end), count in zip(pieces, counts):
        length = end - start
        y0 = camera_y + view_y * start
        y1 = camera_y + view_y * end
        if max(y0, y1) <= scattering.GROUND or abs(view_y) < 1.0e-12:
            cells.extend((start + length * i / count, start + length * (i + 1) / count)
                         for i in range(count))
        else:
            exponent = scattering.HEIGHT_FALLOFF * view_y / 3.0
            previous = 0.0
            for i in range(1, count + 1):
                current = sampling.invert_rho_weight_cumulative(exponent, length, i / count)
                cells.append((start + previous, start + current))
                previous = current
    return cells


def aggregate(rows):
    if not rows:
        return {"pair_count": 0}
    peaks = sorted(row["temporal_residual_max_channel"] for row in rows)
    squared = sum(row["temporal_residual_l2_squared"] for row in rows)
    return {"pair_count": len(rows),
            "temporal_residual_rmse_rgb": math.sqrt(squared / (3 * len(rows))),
            "mean_temporal_residual_max_channel": sum(peaks) / len(peaks),
            "p95_temporal_residual_max_channel": peaks[math.ceil(0.95 * len(peaks)) - 1],
            "max_temporal_residual_max_channel": peaks[-1]}


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
    max_reference_validation = 0.0
    for amplitude in MOTION_AMPLITUDES:
        camera_xs = tuple(x * amplitude for x in sequence.CAMERA_X)
        cache = {}
        for point_index, point in enumerate(points):
            for frame_index, camera_x in enumerate(camera_xs):
                projected = geometry.project(point["world_point"], camera_x)
                if projected is None:
                    continue
                px, py, distance = projected
                origin, _r, _u, _f = sequence.camera_frame(camera_x)
                direction = geometry.normalize(geometry.sub(point["world_point"], origin))
                _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                if hit_surface != point["surface"] or abs(hit_distance - distance) > 0.05:
                    continue
                box = geometry.box_interval(origin, direction, distance,
                                            sequence.SHADOW_VOLUME_MIN,
                                            sequence.SHADOW_VOLUME_MAX)
                if box is None:
                    continue
                truth = box["intervals"]
                case = {"camera_y": origin[1], "view_direction": direction,
                        "distance": distance, "sun_direction": sun}
                reference = soft.integrate_cells(
                    case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH,
                                               REFERENCE_STEPS), truth,
                    sequence.PENUMBRA_WIDTH)
                validation = soft.integrate_cells(
                    case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH,
                                               VALIDATION_STEPS), truth,
                    sequence.PENUMBRA_WIDTH)
                validation_error = math.sqrt(sum((validation[c] - reference[c]) ** 2
                                                 for c in range(3))
                                             / max(sum(reference[c] ** 2 for c in range(3)), 1e-30))
                max_reference_validation = max(max_reference_validation, validation_error)
                candidate_values = {}
                for name, factors in CANDIDATES.items():
                    if factors is None:
                        seg_intervals = weight_intervals = truth
                    else:
                        seg_factor, weight_factor, width_error = factors
                        estimated = combined.estimate_intervals(truth, 0.0, width_error)
                        seg_intervals = truth if seg_factor == "true" else estimated
                        weight_intervals = truth if weight_factor == "true" else estimated
                    cells = weighted_cells_split(case, truth, seg_intervals, weight_intervals,
                                                 sequence.PENUMBRA_WIDTH, CANDIDATE_STEPS)
                    candidate_values[name] = soft.integrate_cells(
                        case, cells, truth, sequence.PENUMBRA_WIDTH)
                cache[(point_index, frame_index)] = {
                    "projected_xy": (px, py), "intervals": truth,
                    "reference": reference, "validation_error": validation_error,
                    "candidate_values": candidate_values,
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
                    prev_resid = tuple(previous["candidate_values"][method][c] - previous["reference"][c]
                                       for c in range(3))
                    curr_resid = tuple(current["candidate_values"][method][c] - current["reference"][c]
                                       for c in range(3))
                    delta = tuple(curr_resid[c] - prev_resid[c] for c in range(3))
                    rows.append({
                        "motion_amplitude": amplitude, "pair_index": pair_index,
                        "source_point_index": point_index, "source_pixel": point["source_pixel"],
                        "world_point": point["world_point"], "surface": point["surface"],
                        "source_region": point["source_region"], "method": method,
                        "previous_camera_x": camera_xs[pair_index],
                        "current_camera_x": camera_xs[pair_index + 1],
                        "previous_projected_xy": previous["projected_xy"],
                        "current_projected_xy": current["projected_xy"],
                        "previous_interval": previous["intervals"],
                        "current_interval": current["intervals"],
                        "previous_reference_validation_relative_rgb_l2": previous["validation_error"],
                        "current_reference_validation_relative_rgb_l2": current["validation_error"],
                        "temporal_residual_delta_rgb": delta,
                        "temporal_residual_max_channel": max(abs(v) for v in delta),
                        "temporal_residual_l2_squared": sum(v * v for v in delta),
                    })
            pair_counts.append(count)
        scenario_info[f"{amplitude:.2f}"] = {
            "camera_x_positions": camera_xs, "visible_correspondence_counts_by_pair": pair_counts,
            "pair_count": sum(pair_counts),
            "methods": {method: {
                "all": aggregate([row for row in rows if row["method"] == method]),
                "by_region": {region: aggregate([row for row in rows
                                                  if row["method"] == method
                                                  and row["source_region"] == region])
                              for region in ("silhouette_edge", "shadow_interior")},
            } for method in CANDIDATES},
        }
        all_rows.extend(rows)

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(all_rows[0]))
        writer.writeheader()
        writer.writerows(all_rows)

    prior_factor_path = ROOT / "quadrature_factor_temporal.csv"
    with prior_factor_path.open(newline="", encoding="utf-8") as stream:
        prior_rows = csv.DictReader(stream)
        prior_baseline = {
            (float(row["motion_amplitude"]), int(row["pair_index"]),
             int(row["source_point_index"])): float(row["temporal_residual_max_channel"])
            for row in prior_rows if row["method"] == "true_segmentation_true_weights"
        }
    baseline_differences = [
        abs(float(row["temporal_residual_max_channel"]) - prior_baseline[(
            float(row["motion_amplitude"]), int(row["pair_index"]),
            int(row["source_point_index"]))])
        for row in all_rows if row["method"] == "true_segmentation_true_weights"
    ]

    manifest = {
        "schema": "fog-shadow-quadrature-width-factor-rd-v1",
        "purpose": "Separate core-width-driven profile segmentation from importance-weight allocation in direct-ray temporal residual.",
        "execution_scope": "Same three camera amplitudes, center-frame shadowed points, fixed AABB, references, and true-visibility integrand as prior camera-motion sweep.",
        "source_point_count": len(points), "motion_amplitudes": MOTION_AMPLITUDES,
        "candidate_steps": CANDIDATE_STEPS, "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS, "center_offset": 0.0,
        "core_width_errors": WIDTH_ERRORS,
        "factor_definitions": {
            "segmentation": "Chooses profile knots and filters fully blocked pieces using true or estimated interval.",
            "importance_weights": "Chooses rho×visibility quadrature allocation using true or estimated interval.",
            "integrand": "Always evaluates single-scattering visibility with the true interval.",
        },
        "maximum_reference_validation_relative_rgb_l2": max_reference_validation,
        "scenarios": scenario_info,
        "exact_interval_baseline_max_channel_residual_difference_from_prior_factor_sweep": max(baseline_differences, default=None),
        "exact_interval_baseline_compared_sample_count": len(baseline_differences),
        "raw_result_count": len(all_rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "source_sweep_sha256": hashlib.sha256((ROOT / "camera_motion_temporal_sweep.csv").read_bytes()).hexdigest(),
        "prior_factor_baseline_sha256": hashlib.sha256(prior_factor_path.read_bytes()).hexdigest(),
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "Factorial effects are descriptive and can interact; this is not a perceptual or temporal acceptance test.",
            "Estimated segmentation combines knot positions with fully blocked piece inclusion because both are coupled in weighted_cells().",
            "Only core-width error is factored here; center offset remains exact.",
            "Synthetic AABB, analytic rays, CPU integrals only; no production shadow filter, GPU, runtime, alpha, or performance.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("points", len(points), "rows", len(all_rows), "max reference validation", max_reference_validation)
    print("exact true/true corner provides shared baseline for comparison with prior factor sweep")
    for amplitude, summary in scenario_info.items():
        print("amplitude", amplitude)
        for method, values in summary["methods"].items():
            if method in ("true_segmentation_true_weights",
                          "true_segmentation_estimated_weights_width_plus_2pct",
                          "estimated_segmentation_true_weights_width_plus_2pct",
                          "estimated_segmentation_estimated_weights_width_plus_2pct"):
                print(" ", method, values["all"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
