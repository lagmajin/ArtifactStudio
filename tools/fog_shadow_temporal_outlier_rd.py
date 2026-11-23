#!/usr/bin/env python3
"""Re-evaluate camera-reprojection temporal outliers on continuous world-point rays."""

from __future__ import annotations

import ast
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


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
PAIR_CSV = OUTPUT_DIR / "world_reprojection_pairs.csv"
OUTLIER_CSV = OUTPUT_DIR / "temporal_outlier_continuous_ray.csv"
OUTLIER_MANIFEST = OUTPUT_DIR / "temporal_outlier_manifest.json"
TOP_ROWS = 16
REFERENCE_STEPS = 8192
VALIDATION_STEPS = 4096
CANDIDATE_STEPS = 128


def parse_triplet(text: str):
    return tuple(float(v) for v in ast.literal_eval(text))


def normalize(v):
    return tuple(value / math.sqrt(sum(c * c for c in v)) for value in v)


def subtract(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def sun_direction():
    elevation = math.radians(45.0)
    azimuth = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    return scattering.normalize((math.cos(elevation) * math.sin(azimuth), math.sin(elevation),
                                 math.cos(elevation) * math.cos(azimuth)))


def evaluate_ray(origin, world_point, sun):
    delta = subtract(world_point, origin)
    distance = math.sqrt(dot(delta, delta))
    direction = tuple(value / distance for value in delta)
    source_color, surface_distance, surface = scene.scene_sample(origin, direction)
    intervals = sequence.ray_box_interval(origin, direction, distance) or ()
    case = {"camera_y": origin[1], "view_direction": direction,
            "distance": distance, "sun_direction": sun}
    reference_cells = soft.reference_cells(case, intervals, sequence.PENUMBRA_WIDTH, REFERENCE_STEPS)
    validation_cells = soft.reference_cells(case, intervals, sequence.PENUMBRA_WIDTH, VALIDATION_STEPS)
    exact_cells = soft.weighted_cells(origin[1], direction[1], distance,
                                       CANDIDATE_STEPS, intervals, sequence.PENUMBRA_WIDTH)
    estimated_intervals = combined.estimate_intervals(
        intervals, sequence.CENTER_ERROR, sequence.WIDTH_ERROR,
    )
    estimated_cells = soft.weighted_cells(origin[1], direction[1], distance,
                                           CANDIDATE_STEPS, estimated_intervals,
                                           sequence.PENUMBRA_WIDTH)
    values = {}
    for label, cells in (("reference", reference_cells), ("validation", validation_cells),
                         ("exact_128", exact_cells), ("estimated_128", estimated_cells)):
        inscatter = soft.integrate_cells(case, cells, intervals, sequence.PENUMBRA_WIDTH)
        view_depth = scattering.exact_density_integral(
            origin[1], direction[1], distance, scattering.HEIGHT_FALLOFF, scattering.GROUND,
        )
        view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_depth)
        values[label] = tuple(source_color[c] * view_t + inscatter[c] for c in range(3))
    ref = values["reference"]
    values["reference_validation_relative_rgb_l2"] = math.sqrt(
        sum((values["validation"][c] - ref[c]) ** 2 for c in range(3))
        / max(sum(value * value for value in ref), 1e-30))
    values["true_intervals"] = intervals
    values["estimated_intervals"] = estimated_intervals
    values["distance"] = distance
    values["surface_distance"] = surface_distance
    values["surface"] = surface
    values["direction"] = direction
    return values


def residual(candidate, reference):
    return subtract(candidate, reference)


def main() -> None:
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    with PAIR_CSV.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    estimated_rows = sorted(
        (row for row in rows if row["method"] == "estimated"),
        key=lambda row: float(row["delta_error_max_channel"]), reverse=True,
    )
    selected = []
    seen = set()
    for row in estimated_rows:
        key = (row["pair_index"], row["center_pixel_index"])
        if key in seen:
            continue
        seen.add(key)
        exact_row = next(r for r in rows if r["method"] == "exact"
                         and r["pair_index"] == row["pair_index"]
                         and r["center_pixel_index"] == row["center_pixel_index"])
        selected.append((row, exact_row))
        if len(selected) >= TOP_ROWS:
            break

    sun = sun_direction()
    out_rows = []
    convergence_max = 0.0
    for screen_row, exact_screen_row in selected:
        pair = int(screen_row["pair_index"])
        point = parse_triplet(screen_row["world_point"])
        direct = []
        for camera_x in (sequence.CAMERA_X[pair], sequence.CAMERA_X[pair + 1]):
            origin, _right, _up, _forward = sequence.camera_frame(camera_x)
            direct.append(evaluate_ray(origin, point, sun))
        convergence_max = max(convergence_max,
                              direct[0]["reference_validation_relative_rgb_l2"],
                              direct[1]["reference_validation_relative_rgb_l2"])
        reference_residual = subtract(direct[1]["reference"], direct[0]["reference"])
        exact_delta = subtract(residual(direct[1]["exact_128"], direct[1]["reference"]),
                              residual(direct[0]["exact_128"], direct[0]["reference"]))
        estimated_delta = subtract(residual(direct[1]["estimated_128"], direct[1]["reference"]),
                                   residual(direct[0]["estimated_128"], direct[0]["reference"]))
        screen_estimated = parse_triplet(screen_row["temporal_residual_delta_rgb"])
        screen_exact = parse_triplet(exact_screen_row["temporal_residual_delta_rgb"])
        out_rows.append({
            "pair_index": pair,
            "previous_frame": screen_row["previous_frame"],
            "current_frame": screen_row["current_frame"],
            "center_pixel_index": screen_row["center_pixel_index"],
            "surface": screen_row["surface"],
            "region": screen_row["region"],
            "world_point": point,
            "screen_reprojection_estimated_residual_delta_rgb": screen_estimated,
            "screen_reprojection_exact_residual_delta_rgb": screen_exact,
            "continuous_ray_exact_residual_delta_rgb": exact_delta,
            "continuous_ray_estimated_residual_delta_rgb": estimated_delta,
            "screen_minus_continuous_estimated_delta_rgb": subtract(screen_estimated, estimated_delta),
            "screen_minus_continuous_exact_delta_rgb": subtract(screen_exact, exact_delta),
            "screen_estimated_max_channel": max(abs(v) for v in screen_estimated),
            "continuous_estimated_max_channel": max(abs(v) for v in estimated_delta),
            "previous_true_intervals": direct[0]["true_intervals"],
            "current_true_intervals": direct[1]["true_intervals"],
            "previous_estimated_intervals": direct[0]["estimated_intervals"],
            "current_estimated_intervals": direct[1]["estimated_intervals"],
            "previous_distance": direct[0]["distance"],
            "current_distance": direct[1]["distance"],
            "previous_reference_validation_error": direct[0]["reference_validation_relative_rgb_l2"],
            "current_reference_validation_error": direct[1]["reference_validation_relative_rgb_l2"],
            "reference_appearance_motion_delta_rgb": reference_residual,
        })

    OUTLIER_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUTLIER_CSV.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(out_rows[0]))
        writer.writeheader()
        writer.writerows(out_rows)

    def summary(method: str, key: str):
        values = [max(abs(float(v)) for v in row[key]) for row in out_rows]
        return {"count": len(values), "mean_max_channel": sum(values) / len(values),
                "p95_max_channel": sorted(values)[math.ceil(0.95 * len(values)) - 1],
                "max_channel": max(values)}

    screen_dominated = sum(
        max(abs(v) for v in row["screen_reprojection_estimated_residual_delta_rgb"])
        > 10.0 * max(abs(v) for v in row["continuous_ray_estimated_residual_delta_rgb"])
        for row in out_rows
    )

    result = {
        "schema": "fog-shadow-temporal-outlier-rd-v1",
        "purpose": "Separate image-space reprojection/resampling from quadrature error at the largest camera-sequence residual outliers.",
        "execution_scope": "Top residual correspondences are recomputed on continuous known-world-point rays using the same deterministic CPU medium and AABB shadow volume.",
        "pre_registered_plan": {
            "selection": f"Top {TOP_ROWS} estimated candidate rows by max-channel temporal residual, unique by frame pair and center-frame pixel.",
            "reference": f"Profile/ground aligned {REFERENCE_STEPS}-step midpoint; compare with {VALIDATION_STEPS} steps.",
            "candidate": f"Exact and +{sequence.CENTER_ERROR:.1%} center / +{sequence.WIDTH_ERROR:.1%} width profile candidates at {CANDIDATE_STEPS} steps.",
            "separation": "Compare image-PFM bilinear residual delta with direct continuous-ray residual delta; difference is a proxy for image sampling plus reference discretization.",
        },
        "selected_outlier_count": len(out_rows),
        "direct_reference_validation_max_relative_rgb_l2": convergence_max,
        "screen_estimated_residual_max_channel": summary("estimated", "screen_reprojection_estimated_residual_delta_rgb"),
        "continuous_estimated_residual_max_channel": summary("estimated", "continuous_ray_estimated_residual_delta_rgb"),
        "screen_exact_residual_max_channel": summary("exact", "screen_reprojection_exact_residual_delta_rgb"),
        "continuous_exact_residual_max_channel": summary("exact", "continuous_ray_exact_residual_delta_rgb"),
        "screen_minus_continuous_estimated_delta_max_channel": summary(
            "estimated", "screen_minus_continuous_estimated_delta_rgb"),
        "screen_minus_continuous_exact_delta_max_channel": summary(
            "exact", "screen_minus_continuous_exact_delta_rgb"),
        "descriptive_screen_dominated_outliers": {
            "count_screen_residual_over_10x_continuous_ray_residual": screen_dominated,
            "selection": "Post-hoc descriptive ratio among the preregistered top-16 outliers; not an acceptance gate.",
        },
        "raw_result_count": len(out_rows),
        "raw_csv": {"path": OUTLIER_CSV.as_posix(),
                    "sha256": hashlib.sha256(OUTLIER_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(),
                           "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "selected_outliers": out_rows,
        "limitations": [
            "Only the top residual rows are recomputed; statistics are diagnostic and not a population estimate.",
            "The continuous reference is still finite-step quadrature; its 8192-vs-4096 validation is recorded per ray.",
            "Difference from PFM reprojection combines bilinear image sampling, pixel-center ray mismatch, and reference discretization.",
            "The AABB is a synthetic shadowed-volume proxy, not a geometric shadow map.",
        ],
    }
    OUTLIER_MANIFEST.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(OUTLIER_MANIFEST.as_posix())
    print("selected outliers", len(out_rows), "continuous reference validation max", convergence_max)
    print("screen estimated", result["screen_estimated_residual_max_channel"])
    print("continuous estimated", result["continuous_estimated_residual_max_channel"])
    print("screen exact", result["screen_exact_residual_max_channel"])
    print("continuous exact", result["continuous_exact_residual_max_channel"])


if __name__ == "__main__":
    main()
