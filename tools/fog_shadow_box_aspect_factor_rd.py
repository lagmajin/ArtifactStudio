#!/usr/bin/env python3
"""Factor width-error segmentation and weight allocation over AABB shapes."""

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
import fog_shadow_box_aspect_width_rd as aspect
import fog_shadow_quadrature_width_factor_rd as factor
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
RAW = ROOT / "box_aspect_factor_temporal.csv"
MANIFEST = ROOT / "box_aspect_factor_manifest.json"
GEOMETRY_CSV = ROOT / "box_aspect_geometry_analysis.csv"
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
WIDTH_ERROR = 0.02
FACTOR_NAMES = ("true_true", "estimated_true", "true_estimated", "estimated_estimated")
CONTRAST_NAMES = ("segmentation_effect", "weight_effect", "interaction_effect", "full_effect")


def rgb_sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def rgb_add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def rgb_scale(a, scale):
    return tuple(value * scale for value in a)


def quantile(values, p):
    ordered = sorted(values)
    if not ordered:
        return None
    return ordered[min(len(ordered) - 1, max(0, math.ceil(p * len(ordered)) - 1))]


def vector_stats(vectors):
    peaks = [max(abs(value) for value in vector) for vector in vectors]
    squared = sum(value * value for vector in vectors for value in vector)
    return {
        "count": len(vectors),
        "max_channel_p50": quantile(peaks, 0.50),
        "max_channel_p95": quantile(peaks, 0.95),
        "max_channel_max": max(peaks) if peaks else None,
        "rgb_rmse": math.sqrt(squared / (3 * len(vectors))) if vectors else None,
    }


def summarize(rows):
    result = {name: vector_stats([row[name] for row in rows])
              for name in (*FACTOR_NAMES, *CONTRAST_NAMES)}
    result["pair_rows"] = len(rows)
    result["unique_points"] = len({row["source_point_index"] for row in rows})
    result["face_switch_pair_rows"] = sum(row["any_face_changed"] for row in rows)
    return result


def points_from_baseline():
    scene.WIDTH, scene.HEIGHT = aspect.sequence.WIDTH, aspect.sequence.HEIGHT
    origin, right, up, forward = aspect.sequence.camera_frame(0.0)
    points = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            _color, distance, surface = scene.scene_sample(origin, ray)
            if aspect.sequence.ray_box_interval(origin, ray, distance):
                point = tuple(origin[i] + ray[i] * distance for i in range(3))
                points.append({"source_point_index": len(points), "source_pixel": (x, y),
                               "world_point": point, "surface": surface})
    return points


def main():
    points = points_from_baseline()
    center = tuple((aspect.sequence.SHADOW_VOLUME_MIN[i] + aspect.sequence.SHADOW_VOLUME_MAX[i]) * 0.5
                   for i in range(3))
    elevation = math.radians(45.0)
    azimuth = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(elevation) * math.sin(azimuth),
                                math.sin(elevation), math.cos(elevation) * math.cos(azimuth)))
    geometry_lookup = {}
    with GEOMETRY_CSV.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            key = (row["shape"], float(row["motion_amplitude"]),
                   int(row["pair_index"]), int(row["source_point_index"]))
            geometry_lookup[key] = {
                "width_quartile": row["width_delta_quartile"],
                "mean_width_quartile": row["mean_width_quartile"],
                "common_width_quartile": row["common_width_delta_quartile"],
                "face_switch": (row["entry_face_changed"].lower() == "true"
                                or row["exit_face_changed"].lower() == "true"),
            }

    all_rows = []
    summary = {shape: {} for shape in aspect.SHAPES}
    supports = defaultdict(set)
    validation_max = 0.0
    source_scripts = (Path(__file__), Path(aspect.__file__), Path(factor.__file__),
                      Path(aspect.scene.__file__), Path(aspect.sequence.__file__),
                      Path(aspect.combined.__file__), Path(aspect.soft.__file__),
                      Path(aspect.scattering.__file__))

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
                    _px, _py, projected_distance = projected
                    origin, _right, _up, _forward = aspect.sequence.camera_frame(camera_x)
                    direction = aspect.normalize(aspect.sub(point["world_point"], origin))
                    _color, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                    if (hit_surface != point["surface"]
                            or abs(hit_distance - projected_distance) > aspect.VISIBILITY_DISTANCE_TOLERANCE):
                        continue
                    geometry = aspect.box_interval(origin, direction, projected_distance,
                                                   center, half_extents)
                    if geometry is None:
                        continue
                    truth = geometry["intervals"]
                    estimated = aspect.combined.estimate_intervals(truth, 0.0, WIDTH_ERROR)
                    case = {"camera_y": origin[1], "view_direction": direction,
                            "distance": projected_distance, "sun_direction": sun}
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
                    intervals = {
                        "true": truth,
                        "estimated": estimated,
                    }
                    candidates = {}
                    for segmentation, weights, name in (
                        ("true", "true", "true_true"),
                        ("estimated", "true", "estimated_true"),
                        ("true", "estimated", "true_estimated"),
                        ("estimated", "estimated", "estimated_estimated"),
                    ):
                        cells = factor.weighted_cells_split(
                            case, truth, intervals[segmentation], intervals[weights],
                            aspect.PENUMBRA, CANDIDATE_STEPS)
                        candidates[name] = soft.integrate_cells(case, cells, truth, aspect.PENUMBRA)
                    frame_cache[(point_index, frame_index)] = {
                        "geometry": geometry, "reference": reference,
                        "candidates": candidates, "validation_error": validation_error,
                    }

            scenario_rows = []
            for pair_index in range(len(camera_xs) - 1):
                for point in points:
                    point_index = point["source_point_index"]
                    previous = frame_cache.get((point_index, pair_index))
                    current = frame_cache.get((point_index, pair_index + 1))
                    if previous is None or current is None:
                        continue
                    temporal = {}
                    for name in FACTOR_NAMES:
                        prev_error = rgb_sub(previous["candidates"][name], previous["reference"])
                        curr_error = rgb_sub(current["candidates"][name], current["reference"])
                        temporal[name] = rgb_sub(curr_error, prev_error)
                    seg_effect = rgb_sub(temporal["estimated_true"], temporal["true_true"])
                    weight_effect = rgb_sub(temporal["true_estimated"], temporal["true_true"])
                    interaction = rgb_add(
                        rgb_sub(temporal["estimated_estimated"], temporal["estimated_true"]),
                        rgb_sub(temporal["true_true"], temporal["true_estimated"]))
                    full_effect = rgb_sub(temporal["estimated_estimated"], temporal["true_true"])
                    geo = geometry_lookup[(shape_name, amplitude, pair_index, point_index)]
                    row = {
                        "shape": shape_name, "motion_amplitude": amplitude,
                        "pair_index": pair_index, "source_point_index": point_index,
                        "source_pixel": point["source_pixel"], "world_point": point["world_point"],
                        "surface": point["surface"], "width_delta_quartile": geo["width_quartile"],
                        "mean_width_quartile": geo["mean_width_quartile"],
                        "common_width_delta_quartile": geo["common_width_quartile"],
                        "any_face_changed": geo["face_switch"],
                        **temporal,
                        "segmentation_effect": seg_effect,
                        "weight_effect": weight_effect,
                        "interaction_effect": interaction,
                        "full_effect": full_effect,
                        "reference_validation_max": max(previous["validation_error"],
                                                         current["validation_error"]),
                    }
                    scenario_rows.append(row)
                    supports[(shape_name, amplitude)].add((pair_index, point_index))

            key = f"{amplitude:.2f}"
            summary[shape_name][key] = {"all_support": summarize(scenario_rows)}
            summary[shape_name][key]["by_width_delta_quartile"] = {
                label: summarize([row for row in scenario_rows
                                  if row["width_delta_quartile"] == label])
                for label in ("Q1", "Q2", "Q3", "Q4")
            }
            summary[shape_name][key]["by_face_switch"] = {
                str(switch).lower(): summarize([row for row in scenario_rows
                                                if row["any_face_changed"] == switch])
                for switch in (False, True)
            }
            all_rows.extend(scenario_rows)
            print("finished", shape_name, amplitude, "pairs", len(scenario_rows))

    common_support = {}
    for amplitude in aspect.AMPLITUDES:
        key = f"{amplitude:.2f}"
        common = set.intersection(*(supports[(shape, amplitude)] for shape in aspect.SHAPES))
        common_support[key] = {"pair_point_rows": len(common),
                               "unique_points": len({point for _pair, point in common})}
        for shape_name in aspect.SHAPES:
            cohort = [row for row in all_rows if row["shape"] == shape_name
                      and row["motion_amplitude"] == amplitude
                      and (row["pair_index"], row["source_point_index"]) in common]
            summary[shape_name][key]["common_support"] = summarize(cohort)
            summary[shape_name][key]["common_support_by_width_delta_quartile"] = {
                label: summarize([row for row in cohort
                                  if row["common_width_delta_quartile"] == label])
                for label in ("Q1", "Q2", "Q3", "Q4")
            }
            summary[shape_name][key]["common_support_by_face_switch"] = {
                str(switch).lower(): summarize([row for row in cohort
                                                if row["any_face_changed"] == switch])
                for switch in (False, True)
            }

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(all_rows[0]))
        writer.writeheader()
        writer.writerows(all_rows)
    manifest = {
        "schema": "fog-shadow-box-aspect-factor-rd-v1",
        "purpose": "2x2 factor decomposition of width-estimation effects into profile segmentation and importance-weight allocation across volume-preserving box shapes.",
        "source_point_count": len(points), "source_selection": "Same 440 baseline center-frame AABB-hit world points as aspect study.",
        "shapes_half_extents": aspect.SHAPES, "box_center": center,
        "box_volume_each": {name: 8.0 * math.prod(extents)
                            for name, extents in aspect.SHAPES.items()},
        "camera_amplitudes": aspect.AMPLITUDES,
        "width_error": WIDTH_ERROR, "center_offset": 0.0,
        "candidate_steps": CANDIDATE_STEPS, "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS,
        "maximum_reference_validation_relative_rgb_l2": validation_max,
        "factor_corners": {
            "true_true": "true interval segmentation and true importance weights",
            "estimated_true": "estimated +2% width segmentation and true importance weights",
            "true_estimated": "true interval segmentation and estimated +2% width importance weights",
            "estimated_estimated": "estimated +2% width segmentation and estimated +2% width importance weights",
        },
        "factor_contrasts": {
            "segmentation_effect": "estimated_true - true_true",
            "weight_effect": "true_estimated - true_true",
            "interaction_effect": "estimated_estimated - estimated_true - true_estimated + true_true",
            "full_effect": "estimated_estimated - true_true",
            "note": "Contrasts are RGB vector differences; nonlinear interaction is retained and terms are not independent errors to sum as magnitudes.",
        },
        "common_support": common_support,
        "scenario_summaries": summary,
        "maximum_reconciliation_error": max(
            (max(abs(row["full_effect"][c] - row["segmentation_effect"][c]
                     - row["weight_effect"][c] - row["interaction_effect"][c])
                 for c in range(3)) for row in all_rows), default=0.0),
        "raw_csv": {"path": RAW.as_posix(), "rows": len(all_rows),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "input_scripts": [{"path": path.as_posix(),
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in source_scripts],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "One synthetic scene and one sun configuration; not a general geometry distribution.",
            "Shape-specific and common-support rows are both reported because changed AABB silhouettes alter valid ray support.",
            "Factor contrasts describe numerical approximation effects and do not establish perceptual flicker or causality in product rendering.",
            "Visual review, alpha/background integration, GPU/backend parity, host integration and runtime are unverified.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("rows", len(all_rows), "max reference validation", validation_max,
          "max factor reconciliation", manifest["maximum_reconciliation_error"])
    print("common support", common_support)
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
