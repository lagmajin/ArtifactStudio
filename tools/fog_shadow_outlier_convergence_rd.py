#!/usr/bin/env python3
"""Sweep step count and boundary-estimation error on continuous-ray outliers."""

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
INPUT_MANIFEST = OUTPUT_DIR / "temporal_outlier_manifest.json"
RAW_CSV = OUTPUT_DIR / "outlier_convergence_sweep.csv"
MANIFEST = OUTPUT_DIR / "outlier_convergence_manifest.json"
CENTER_OFFSETS = (-0.02, -0.005, 0.0, 0.005, 0.02)
WIDTH_ERRORS = (-0.02, -0.005, 0.0, 0.005, 0.02)
STEP_COUNTS = (32, 64, 128, 256, 512, 1024, 2048, 4096)
REFERENCE_STEPS = 16384
VALIDATION_STEPS = 8192
PENUMBRA_WIDTH = sequence.PENUMBRA_WIDTH
GATE = soft.GATE


def rgb_relative_l2(candidate, reference):
    error = sum((candidate[c] - reference[c]) ** 2 for c in range(3))
    norm = sum(value * value for value in reference)
    return math.sqrt(error / max(norm, 1e-30))


def direct_case(world_point, camera_x, sun):
    origin, _right, _up, _forward = sequence.camera_frame(camera_x)
    delta = tuple(world_point[i] - origin[i] for i in range(3))
    distance = math.sqrt(sum(value * value for value in delta))
    direction = tuple(value / distance for value in delta)
    true_intervals = sequence.ray_box_interval(origin, direction, distance) or ()
    case = {"camera_y": origin[1], "view_direction": direction,
            "distance": distance, "sun_direction": sun}
    return case, true_intervals


def integrate(case, cells, intervals):
    return soft.integrate_cells(case, cells, intervals, PENUMBRA_WIDTH)


def main() -> None:
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    input_manifest = json.loads(INPUT_MANIFEST.read_text(encoding="utf-8"))
    selected = input_manifest["selected_outliers"]
    sun = outlier.sun_direction()
    rays = []
    reference_validation_max = 0.0
    rows = []
    for outlier_index, item in enumerate(selected):
        pair = int(item["pair_index"])
        world_point = tuple(item["world_point"])
        for frame_side, camera_x in (("previous", sequence.CAMERA_X[pair]),
                                     ("current", sequence.CAMERA_X[pair + 1])):
            case, true_intervals = direct_case(world_point, camera_x, sun)
            ref = integrate(case, soft.reference_cells(
                case, true_intervals, PENUMBRA_WIDTH, REFERENCE_STEPS), true_intervals)
            validation = integrate(case, soft.reference_cells(
                case, true_intervals, PENUMBRA_WIDTH, VALIDATION_STEPS), true_intervals)
            validation_error = rgb_relative_l2(validation, ref)
            reference_validation_max = max(reference_validation_max, validation_error)
            ray_id = f"outlier_{outlier_index:02d}_{frame_side}"
            rays.append({"ray_id": ray_id, "outlier_index": outlier_index,
                         "pair_index": pair, "frame_side": frame_side,
                         "camera_x": camera_x, "world_point": world_point,
                         "region": item["region"], "surface": item["surface"],
                         "true_intervals": true_intervals,
                         "reference_rgb": ref,
                         "reference_validation_relative_rgb_l2": validation_error})
            for center_offset in CENTER_OFFSETS:
                for width_error in WIDTH_ERRORS:
                    estimated = combined.estimate_intervals(
                        true_intervals, center_offset, width_error,
                    )
                    for steps in STEP_COUNTS:
                        cells = soft.weighted_cells(
                            case["camera_y"], case["view_direction"][1], case["distance"],
                            steps, estimated, PENUMBRA_WIDTH,
                        )
                        candidate = integrate(case, cells, true_intervals)
                        rows.append({
                            "ray_id": ray_id,
                            "outlier_index": outlier_index,
                            "pair_index": pair,
                            "frame_side": frame_side,
                            "camera_x": camera_x,
                            "surface": item["surface"],
                            "region": item["region"],
                            "world_point": world_point,
                            "steps": steps,
                            "center_offset_fraction_of_ray": center_offset,
                            "core_relative_width_error": width_error,
                            "true_intervals": true_intervals,
                            "estimated_intervals": estimated,
                            "reference_rgb": ref,
                            "candidate_rgb": candidate,
                            "relative_rgb_l2_error": rgb_relative_l2(candidate, ref),
                            "reference_validation_relative_rgb_l2": validation_error,
                        })

    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    first_pass_steps = {}
    stable_pass_steps = {}
    for center_offset in CENTER_OFFSETS:
        center_key = f"{center_offset:+.3%}"
        summary[center_key] = {}
        first_pass_steps[center_key] = {}
        stable_pass_steps[center_key] = {}
        for width_error in WIDTH_ERRORS:
            width_key = f"{width_error:+.3%}"
            summary[center_key][width_key] = {}
            maximum_by_step = []
            for steps in STEP_COUNTS:
                selected_errors = sorted(
                    row["relative_rgb_l2_error"] for row in rows
                    if row["steps"] == steps
                    and math.isclose(row["center_offset_fraction_of_ray"], center_offset)
                    and math.isclose(row["core_relative_width_error"], width_error)
                )
                maximum = selected_errors[-1]
                summary[center_key][width_key][str(steps)] = {
                    "ray_count": len(selected_errors),
                    "p95_relative_rgb_l2_error": selected_errors[math.ceil(0.95 * len(selected_errors)) - 1],
                    "max_relative_rgb_l2_error": maximum,
                    "passes_all_outlier_rays_gate": maximum <= GATE,
                }
                maximum_by_step.append(maximum)
            first_pass_steps[center_key][width_key] = next(
                (steps for steps, maximum in zip(STEP_COUNTS, maximum_by_step) if maximum <= GATE), None,
            )
            stable_pass_steps[center_key][width_key] = next(
                (steps for index, steps in enumerate(STEP_COUNTS)
                 if all(maximum <= GATE for maximum in maximum_by_step[index:])), None,
            )

    manifest = {
        "schema": "fog-shadow-outlier-convergence-rd-v1",
        "purpose": "Distinguish quadrature convergence from persistent boundary-estimation bias on selected temporal outlier rays.",
        "execution_scope": "Direct continuous-ray CPU scattering integral on 32 selected rays; no screen image sampling is used in candidate/reference evaluation.",
        "input_outlier_manifest_sha256": hashlib.sha256(INPUT_MANIFEST.read_bytes()).hexdigest(),
        "selected_world_points": len(selected),
        "continuous_ray_count": len(rays),
        "center_offsets_fraction_of_ray": CENTER_OFFSETS,
        "core_relative_width_errors": WIDTH_ERRORS,
        "step_counts": STEP_COUNTS,
        "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS,
        "reference_validation_max_relative_rgb_l2": reference_validation_max,
        "pre_registered_gate_max_relative_rgb_l2": GATE,
        "raw_result_count": len(rows),
        "raw_csv": {"path": RAW_CSV.as_posix(),
                    "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(),
                           "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "ray_inventory": rays,
        "summary": summary,
        "first_step_count_passing_gate": first_pass_steps,
        "minimum_step_count_passing_at_this_and_all_higher_swept_steps": stable_pass_steps,
        "limitations": [
            "The ray set is selected from the top camera-sequence residual outliers and is not representative of all pixels or scenes.",
            "The interval boundary is an analytic AABB intersection, not a shadow-map sample or filtered geometric shadow.",
            "The relative RGB L2 gate is inherited from prior quadrature stress studies and does not express perceptual or temporal acceptability.",
            "No GPU timing, actual renderer integration, alpha, or multi-backend behavior is measured.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print("outlier points", len(selected), "rays", len(rays), "rows", len(rows))
    print("reference validation max", reference_validation_max)
    print("first passing and stable passing samples by center/width error")
    for center, widths in first_pass_steps.items():
        print(center, widths)
    print("stable pass")
    for center, widths in stable_pass_steps.items():
        print(center, widths)


if __name__ == "__main__":
    main()
