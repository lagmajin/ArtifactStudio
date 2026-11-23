#!/usr/bin/env python3
"""Extend AABB width-factor sweep to negative error and sign parity analysis."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
import fog_shadow_box_aspect_factor_rd as base_factor
import fog_shadow_box_aspect_width_rd as aspect
import fog_shadow_quadrature_width_factor_rd as factor
import fog_shadow_penumbra_rd as soft
import fog_height_camera_scene_rd as scene
import fog_sun_transmittance_rd as scattering


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
PLUS_INPUT = ROOT / "box_aspect_factor_temporal.csv"
PLUS_MANIFEST = ROOT / "box_aspect_factor_manifest.json"
GEOMETRY_CSV = ROOT / "box_aspect_geometry_analysis.csv"
RAW = ROOT / "box_aspect_factor_sign_temporal.csv"
MANIFEST = ROOT / "box_aspect_factor_sign_manifest.json"
NEGATIVE_WIDTH_ERROR = -0.02
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
FACTOR_NAMES = base_factor.FACTOR_NAMES
CONTRAST_NAMES = base_factor.CONTRAST_NAMES


def vadd(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def vsub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def vscale(a, value):
    return tuple(component * value for component in a)


def vector_stats(vectors):
    peaks = [max(abs(value) for value in vector) for vector in vectors]
    squared = sum(value * value for vector in vectors for value in vector)
    return {
        "count": len(vectors),
        "max_channel_p50": base_factor.quantile(peaks, 0.50),
        "max_channel_p95": base_factor.quantile(peaks, 0.95),
        "max_channel_max": max(peaks) if peaks else None,
        "rgb_rmse": math.sqrt(squared / (3 * len(vectors))) if vectors else None,
    }


def summary(rows):
    result = {name: vector_stats([row[name] for row in rows])
              for name in (*FACTOR_NAMES, *CONTRAST_NAMES)}
    result["pair_rows"] = len(rows)
    result["unique_points"] = len({row["source_point_index"] for row in rows})
    result["face_switch_pair_rows"] = sum(row["any_face_changed"] for row in rows)
    return result


def parse_plus_rows():
    result = {}
    with PLUS_INPUT.open(newline="", encoding="utf-8") as stream:
        for raw in csv.DictReader(stream):
            key = (raw["shape"], float(raw["motion_amplitude"]), int(raw["pair_index"]),
                   int(raw["source_point_index"]))
            row = {name: ast.literal_eval(raw[name])
                   for name in (*FACTOR_NAMES, *CONTRAST_NAMES)}
            row.update({
                "shape": key[0], "motion_amplitude": key[1], "pair_index": key[2],
                "source_point_index": key[3], "source_pixel": raw["source_pixel"],
                "world_point": raw["world_point"], "surface": raw["surface"],
                "width_delta_quartile": raw["width_delta_quartile"],
                "common_width_delta_quartile": raw["common_width_delta_quartile"],
                "any_face_changed": raw["any_face_changed"].lower() == "true",
                "width_sign": "plus_2pct",
            })
            result[key] = row
    return result


def main():
    plus_rows = parse_plus_rows()
    points = base_factor.points_from_baseline()
    center = tuple((aspect.sequence.SHADOW_VOLUME_MIN[i] + aspect.sequence.SHADOW_VOLUME_MAX[i]) * 0.5
                   for i in range(3))
    elevation, azimuth = math.radians(45.0), math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(elevation) * math.sin(azimuth),
                                math.sin(elevation), math.cos(elevation) * math.cos(azimuth)))
    geometry_lookup = {}
    with GEOMETRY_CSV.open(newline="", encoding="utf-8") as stream:
        for raw in csv.DictReader(stream):
            key = (raw["shape"], float(raw["motion_amplitude"]), int(raw["pair_index"]),
                   int(raw["source_point_index"]))
            geometry_lookup[key] = {
                "width_delta_quartile": raw["width_delta_quartile"],
                "common_width_delta_quartile": raw["common_width_delta_quartile"],
                "any_face_changed": (raw["entry_face_changed"].lower() == "true"
                                     or raw["exit_face_changed"].lower() == "true"),
            }

    minus_rows = {}
    validation_max = 0.0
    for shape_name, half_extents in aspect.SHAPES.items():
        for amplitude in aspect.AMPLITUDES:
            camera_xs = tuple(x * amplitude for x in aspect.sequence.CAMERA_X)
            frame_cache = {}
            for point in points:
                point_index = point["source_point_index"]
                for frame_index, camera_x in enumerate(camera_xs):
                    projected = aspect.project(point["world_point"], camera_x)
                    if projected is None:
                        continue
                    _px, _py, distance = projected
                    origin, _r, _u, _f = aspect.sequence.camera_frame(camera_x)
                    direction = aspect.normalize(aspect.sub(point["world_point"], origin))
                    _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                    if (hit_surface != point["surface"]
                            or abs(hit_distance - distance) > aspect.VISIBILITY_DISTANCE_TOLERANCE):
                        continue
                    geometry = aspect.box_interval(origin, direction, distance, center, half_extents)
                    if geometry is None:
                        continue
                    truth = geometry["intervals"]
                    estimated = aspect.combined.estimate_intervals(
                        truth, 0.0, NEGATIVE_WIDTH_ERROR)
                    case = {"camera_y": origin[1], "view_direction": direction,
                            "distance": distance, "sun_direction": sun}
                    reference = soft.integrate_cells(
                        case, soft.reference_cells(case, truth, aspect.PENUMBRA, REFERENCE_STEPS),
                        truth, aspect.PENUMBRA)
                    validation = soft.integrate_cells(
                        case, soft.reference_cells(case, truth, aspect.PENUMBRA, VALIDATION_STEPS),
                        truth, aspect.PENUMBRA)
                    validation_error = math.sqrt(
                        sum((validation[c] - reference[c]) ** 2 for c in range(3))
                        / max(sum(reference[c] ** 2 for c in range(3)), 1e-30))
                    validation_max = max(validation_max, validation_error)
                    candidates = {}
                    for segmentation, weights, name in (
                        ("true", "true", "true_true"),
                        ("estimated", "true", "estimated_true"),
                        ("true", "estimated", "true_estimated"),
                        ("estimated", "estimated", "estimated_estimated"),
                    ):
                        seg_intervals = truth if segmentation == "true" else estimated
                        weight_intervals = truth if weights == "true" else estimated
                        cells = factor.weighted_cells_split(
                            case, truth, seg_intervals, weight_intervals,
                            aspect.PENUMBRA, CANDIDATE_STEPS)
                        candidates[name] = soft.integrate_cells(case, cells, truth, aspect.PENUMBRA)
                    frame_cache[(point_index, frame_index)] = {
                        "reference": reference, "candidates": candidates,
                        "validation_error": validation_error,
                    }

            for pair_index in range(len(camera_xs) - 1):
                for point in points:
                    point_index = point["source_point_index"]
                    previous = frame_cache.get((point_index, pair_index))
                    current = frame_cache.get((point_index, pair_index + 1))
                    if previous is None or current is None:
                        continue
                    temporal = {}
                    for name in FACTOR_NAMES:
                        prev_error = vsub(previous["candidates"][name], previous["reference"])
                        curr_error = vsub(current["candidates"][name], current["reference"])
                        temporal[name] = vsub(curr_error, prev_error)
                    segmentation = vsub(temporal["estimated_true"], temporal["true_true"])
                    weight = vsub(temporal["true_estimated"], temporal["true_true"])
                    interaction = vadd(
                        vsub(temporal["estimated_estimated"], temporal["estimated_true"]),
                        vsub(temporal["true_true"], temporal["true_estimated"]))
                    full = vsub(temporal["estimated_estimated"], temporal["true_true"])
                    extra = geometry_lookup[(shape_name, amplitude, pair_index, point_index)]
                    key = (shape_name, amplitude, pair_index, point_index)
                    minus_rows[key] = {
                        "shape": shape_name, "motion_amplitude": amplitude,
                        "pair_index": pair_index, "source_point_index": point_index,
                        "source_pixel": point["source_pixel"], "world_point": point["world_point"],
                        "surface": point["surface"], "width_sign": "minus_2pct",
                        "width_delta_quartile": extra["width_delta_quartile"],
                        "common_width_delta_quartile": extra["common_width_delta_quartile"],
                        "any_face_changed": extra["any_face_changed"],
                        **temporal,
                        "segmentation_effect": segmentation, "weight_effect": weight,
                        "interaction_effect": interaction, "full_effect": full,
                    }
            print("finished negative width", shape_name, amplitude,
                  "pairs", sum(key[0] == shape_name and key[1] == amplitude
                               for key in minus_rows))

    if set(minus_rows) != set(plus_rows):
        raise ValueError(f"Signed support mismatch: minus-only={len(set(minus_rows)-set(plus_rows))}, "
                         f"plus-only={len(set(plus_rows)-set(minus_rows))}")

    combined_rows = []
    paired = defaultdict(dict)
    for key in sorted(plus_rows):
        for row in (plus_rows[key], minus_rows[key]):
            combined_rows.append(row)
            paired[key][row["width_sign"]] = row

    plus_minus_true_true = [abs(paired[key]["plus_2pct"]["true_true"][c]
                                 - paired[key]["minus_2pct"]["true_true"][c])
                            for key in paired for c in range(3)]
    identity_error = max((abs(row["full_effect"][c] - row["segmentation_effect"][c]
                              - row["weight_effect"][c] - row["interaction_effect"][c])
                         for row in combined_rows for c in range(3)), default=0.0)

    scenario_summary = {}
    sign_summary = {}
    common_support = {}
    common_support_keys = {}
    for amplitude in aspect.AMPLITUDES:
        amp_key = f"{amplitude:.2f}"
        common_pairs_points = set.intersection(*(
            {(key[2], key[3]) for key in paired if key[0] == shape and key[1] == amplitude}
            for shape in aspect.SHAPES))
        common_support_keys[amplitude] = common_pairs_points
        common_support[amp_key] = {
            "pair_point_rows": len(common_pairs_points),
            "unique_points": len({point for _pair, point in common_pairs_points}),
        }
    for shape in aspect.SHAPES:
        scenario_summary[shape] = {}
        sign_summary[shape] = {}
        for amplitude in aspect.AMPLITUDES:
            amp_key = f"{amplitude:.2f}"
            shape_keys = [key for key in paired if key[0] == shape and key[1] == amplitude]
            scenario_summary[shape][amp_key] = {}
            for sign in ("minus_2pct", "plus_2pct"):
                rows = [paired[key][sign] for key in shape_keys]
                scenario_summary[shape][amp_key][sign] = {
                    "all_support": summary(rows),
                    "by_width_delta_quartile": {
                        label: summary([row for row in rows
                                        if row["width_delta_quartile"] == label])
                        for label in ("Q1", "Q2", "Q3", "Q4")
                    },
                    "common_support": summary([row for row in rows
                                                if (row["pair_index"], row["source_point_index"])
                                                in common_support_keys[amplitude]]),
                    "common_support_by_width_delta_quartile": {
                        label: summary([row for row in rows
                                        if (row["pair_index"], row["source_point_index"])
                                        in common_support_keys[amplitude]
                                        and row["common_width_delta_quartile"] == label])
                        for label in ("Q1", "Q2", "Q3", "Q4")
                    },
                }
            sign_summary[shape][amp_key] = {}
            for group_name, selected in (
                ("all_support", shape_keys),
                ("common_support", []),
            ):
                if group_name == "common_support":
                    common_pairs_points = common_support_keys[amplitude]
                    selected = [key for key in shape_keys if (key[2], key[3]) in common_pairs_points]
                terms = (*FACTOR_NAMES, *CONTRAST_NAMES)
                sign_summary[shape][amp_key][group_name] = {}
                for term in terms:
                    plus = [paired[key]["plus_2pct"][term] for key in selected]
                    minus = [paired[key]["minus_2pct"][term] for key in selected]
                    even = [vscale(vadd(a, b), 0.5) for a, b in zip(plus, minus)]
                    odd = [vscale(vsub(a, b), 0.5) for a, b in zip(plus, minus)]
                    delta = [vsub(a, b) for a, b in zip(plus, minus)]
                    sign_summary[shape][amp_key][group_name][term] = {
                        "plus_minus_delta": vector_stats(delta),
                        "even_component": vector_stats(even),
                        "odd_component": vector_stats(odd),
                    }
            common_pairs_points = common_support_keys[amplitude]
            sign_summary[shape][amp_key]["common_support_by_width_delta_quartile"] = {}
            for label in ("Q1", "Q2", "Q3", "Q4"):
                selected = [key for key in shape_keys if (key[2], key[3]) in common_pairs_points
                            and paired[key]["plus_2pct"]["common_width_delta_quartile"] == label]
                sign_summary[shape][amp_key]["common_support_by_width_delta_quartile"][label] = {}
                for term in (*FACTOR_NAMES, *CONTRAST_NAMES):
                    plus = [paired[key]["plus_2pct"][term] for key in selected]
                    minus = [paired[key]["minus_2pct"][term] for key in selected]
                    sign_summary[shape][amp_key]["common_support_by_width_delta_quartile"][label][term] = {
                        "plus_minus_delta": vector_stats([vsub(a, b) for a, b in zip(plus, minus)]),
                        "even_component": vector_stats([vscale(vadd(a, b), 0.5)
                                                         for a, b in zip(plus, minus)]),
                        "odd_component": vector_stats([vscale(vsub(a, b), 0.5)
                                                        for a, b in zip(plus, minus)]),
                    }

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(combined_rows[0]))
        writer.writeheader()
        writer.writerows(combined_rows)
    source_scripts = (Path(__file__), Path(base_factor.__file__), Path(aspect.__file__),
                      Path(factor.__file__), Path(soft.__file__), Path(scene.__file__),
                      Path(scattering.__file__))
    manifest = {
        "schema": "fog-shadow-box-aspect-factor-sign-rd-v1",
        "purpose": "Compare signed +/-2% width-error 2x2 factor decompositions and RGB vector symmetry across three AABB shapes.",
        "width_errors": {"minus_2pct": NEGATIVE_WIDTH_ERROR, "plus_2pct": 0.02},
        "source_point_count": len(points),
        "shapes_half_extents": aspect.SHAPES,
        "box_center": center,
        "box_volume_each": {name: 8.0 * math.prod(extents)
                            for name, extents in aspect.SHAPES.items()},
        "camera_amplitudes": aspect.AMPLITUDES,
        "positive_sign_source_csv": {"path": PLUS_INPUT.as_posix(),
                                     "sha256": hashlib.sha256(PLUS_INPUT.read_bytes()).hexdigest()},
        "positive_sign_source_manifest_sha256": hashlib.sha256(PLUS_MANIFEST.read_bytes()).hexdigest(),
        "negative_sign_candidate_steps": CANDIDATE_STEPS,
        "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS,
        "maximum_negative_reference_validation_relative_rgb_l2": validation_max,
        "pair_rows_per_sign": {shape: {f"{amp:.2f}": sum(
            key[0] == shape and key[1] == amp for key in minus_rows)
            for amp in aspect.AMPLITUDES} for shape in aspect.SHAPES},
        "minus_plus_common_support": common_support,
        "plus_minus_true_true_max_rgb_component_difference": max(plus_minus_true_true, default=0.0),
        "factor_identity_max_rgb_component_error": identity_error,
        "scenario_summary_by_sign": scenario_summary,
        "sign_symmetry_summary": sign_summary,
        "raw_csv": {"path": RAW.as_posix(), "rows": len(combined_rows),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "input_scripts": [{"path": path.as_posix(),
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in source_scripts],
        "limitations": [
            "Positive sign is reused from a prior complete run; negative sign is recomputed under the same factor helper and settings.",
            "One synthetic scene and fixed AABB shapes; no scene-level generalization or causal geometry isolation.",
            "Sign even/odd vectors summarize numerical symmetry and do not imply perceptual preference or stability.",
            "Visual review, alpha/background, real shadow filtering, GPU/backend parity, host integration and runtime remain unverified.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("negative rows", len(minus_rows), "combined rows", len(combined_rows),
          "max reference validation", validation_max,
          "true/true sign delta", manifest["plus_minus_true_true_max_rgb_component_difference"],
          "factor identity", identity_error)
    print("common support", common_support)
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
