#!/usr/bin/env python3
"""Test density-weighted fog sampling around binary shadow boundaries.

The shadow is a deterministic visibility interval along the view ray. This is
an integration stress fixture, not a shadow-map or product-renderer model.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_convergence_sweep_rd as convergence
import fog_height_sampling_distribution_rd as sampling
import fog_sun_transmittance_rd as scattering
from fog_height_integral_rd import exact_density_integral


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-sampling-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
SUN_ELEVATIONS = (5.0, 45.0, 80.0)
STEP_COUNTS = (16, 32, 64, 128, 256, 512, 1024, 2048, 4096)
REFERENCE_STEPS = 16384
REFERENCE_VALIDATION_STEPS = 8192
MAX_RELATIVE_RGB_L2_ERROR = 1.0e-3
METHODS = ("uniform_midpoint", "rho_one_third_weighted", "shadow_boundary_aware")
SHADOW_PATTERNS = {
    "unshadowed": (),
    "narrow_early": ((0.25, 0.29),),
    "narrow_center": ((0.48, 0.52),),
    "narrow_late": ((0.75, 0.79),),
    "wide_early": ((0.0, 0.25),),
    "wide_center": ((0.35, 0.65),),
    "wide_late": ((0.75, 1.0),),
}


def is_occluded(t: float, distance: float, intervals: tuple[tuple[float, float], ...]) -> bool:
    u = t / distance if distance > 0.0 else 0.0
    return any(start <= u < end for start, end in intervals)


def split_at_ground(camera_y: float, view_y: float, distance: float) -> list[tuple[float, float]]:
    return convergence.ground_split_segments(camera_y, view_y, distance, scattering.GROUND)


def visible_pieces(
    camera_y: float, view_y: float, distance: float,
    shadow_intervals: tuple[tuple[float, float], ...],
) -> list[tuple[float, float]]:
    boundaries = {0.0, distance}
    for start, end in shadow_intervals:
        boundaries.add(max(0.0, min(distance, start * distance)))
        boundaries.add(max(0.0, min(distance, end * distance)))
    ordered = sorted(boundaries)
    pieces: list[tuple[float, float]] = []
    for start, end in zip(ordered, ordered[1:]):
        if end <= start or is_occluded(0.5 * (start + end), distance, shadow_intervals):
            continue
        y0 = camera_y + view_y * start
        pieces.extend((start + a, start + b) for a, b in split_at_ground(y0, view_y, end - start))
    return pieces


def curvature_weight(y0: float, dy: float, length: float, falloff: float) -> float:
    y1 = y0 + dy * length
    if max(y0, y1) <= scattering.GROUND:
        return length
    return exact_density_integral(y0, dy, length, falloff / 3.0, scattering.GROUND)


def make_weighted_cells(
    camera_y: float, view_y: float, distance: float, steps: int,
    shadow_intervals: tuple[tuple[float, float], ...],
    shadow_aware: bool,
) -> list[tuple[float, float]]:
    if shadow_aware:
        pieces = visible_pieces(camera_y, view_y, distance, shadow_intervals)
    else:
        pieces = split_at_ground(camera_y, view_y, distance)
    if not pieces:
        return []
    weights = [
        curvature_weight(camera_y + view_y * start, view_y, end - start,
                         scattering.HEIGHT_FALLOFF)
        for start, end in pieces
    ]
    counts = convergence.allocate_steps(weights, steps)
    cells: list[tuple[float, float]] = []
    for (start, end), count in zip(pieces, counts):
        length = end - start
        y0 = camera_y + view_y * start
        y1 = camera_y + view_y * end
        if max(y0, y1) <= scattering.GROUND or abs(view_y) < 1.0e-12:
            cells.extend(
                (start + length * index / count, start + length * (index + 1) / count)
                for index in range(count)
            )
            continue
        a = scattering.HEIGHT_FALLOFF * view_y / 3.0
        previous = 0.0
        for index in range(1, count + 1):
            current = sampling.invert_rho_weight_cumulative(a, length, index / count)
            cells.append((start + previous, start + current))
            previous = current
    return cells


def uniform_visible_cells(
    camera_y: float, view_y: float, distance: float, steps: int,
    shadow_intervals: tuple[tuple[float, float], ...],
) -> list[tuple[float, float]]:
    visible = visible_pieces(camera_y, view_y, distance, shadow_intervals)
    if not visible:
        return []
    counts = convergence.allocate_steps([end - start for start, end in visible], steps)
    cells: list[tuple[float, float]] = []
    for (start, end), count in zip(visible, counts):
        width = (end - start) / count
        cells.extend((start + i * width, start + (i + 1) * width) for i in range(count))
    return cells


def integrate_cells(
    camera_y: float, view_direction: scattering.Vector, distance: float,
    sun_direction: scattering.Vector, cells: list[tuple[float, float]],
    shadow_intervals: tuple[tuple[float, float], ...],
) -> list[float]:
    cosine = max(-1.0, min(1.0, scattering.dot(tuple(-v for v in view_direction), sun_direction)))
    phase = scattering.rayleigh_phase(cosine)
    radiance = [0.0, 0.0, 0.0]
    for start, end in cells:
        width = end - start
        t = 0.5 * (start + end)
        if is_occluded(t, distance, shadow_intervals):
            continue
        sample_y = camera_y + view_direction[1] * t
        density_ratio = math.exp(-scattering.HEIGHT_FALLOFF * max(sample_y - scattering.GROUND, 0.0))
        view_integral = exact_density_integral(
            camera_y, view_direction[1], t, scattering.HEIGHT_FALLOFF, scattering.GROUND,
        )
        view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_integral)
        sun_t = scattering.sun_transmittance(sample_y, sun_direction[1])
        contribution = (
            scattering.DENSITY_AT_GROUND * density_ratio * scattering.SCATTERING
            * phase * scattering.SUN_INTENSITY * view_t * sun_t * width
        )
        for channel in range(3):
            radiance[channel] += contribution * scattering.FOG_TINT[channel]
    return radiance


def integrate(
    method: str, camera_y: float, view_direction: scattering.Vector,
    distance: float, sun_direction: scattering.Vector, steps: int,
    shadow_intervals: tuple[tuple[float, float], ...],
) -> list[float]:
    view_y = view_direction[1]
    if method == "uniform_midpoint":
        width = distance / steps
        cells = [(i * width, (i + 1) * width) for i in range(steps)]
    elif method == "rho_one_third_weighted":
        cells = make_weighted_cells(camera_y, view_y, distance, steps, shadow_intervals, False)
    else:
        cells = make_weighted_cells(camera_y, view_y, distance, steps, shadow_intervals, True)
    return integrate_cells(camera_y, view_direction, distance, sun_direction, cells, shadow_intervals)


def relative_rgb_l2(candidate: list[float], reference: list[float]) -> float:
    error_sq = sum((candidate[i] - reference[i]) ** 2 for i in range(3))
    reference_sq = sum(value * value for value in reference)
    return math.sqrt(error_sq / max(reference_sq, 1.0e-30))


def make_cases() -> list[dict[str, object]]:
    cases: list[dict[str, object]] = []
    for camera_y in (2.0, 8.0, 20.0):
        for view_name, (view_direction, distance) in scattering.make_view_cases(camera_y).items():
            for elevation in SUN_ELEVATIONS:
                sun_el = math.radians(elevation)
                sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
                sun_direction = scattering.normalize((
                    math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                    math.cos(sun_el) * math.cos(sun_az),
                ))
                for shadow_name, intervals in SHADOW_PATTERNS.items():
                    cases.append({
                        "camera_y": camera_y,
                        "view_name": view_name,
                        "view_direction": view_direction,
                        "distance": distance,
                        "sun_elevation": elevation,
                        "sun_direction": sun_direction,
                        "shadow_pattern": shadow_name,
                        "shadow_intervals_normalized": intervals,
                    })
    return cases


def main() -> None:
    cases = make_cases()
    raw_rows: list[dict[str, object]] = []
    reference_validation_max = 0.0
    for case in cases:
        intervals = case["shadow_intervals_normalized"]
        reference_cells = uniform_visible_cells(
            case["camera_y"], case["view_direction"][1], case["distance"],
            REFERENCE_STEPS, intervals,
        )
        validation_cells = uniform_visible_cells(
            case["camera_y"], case["view_direction"][1], case["distance"],
            REFERENCE_VALIDATION_STEPS, intervals,
        )
        reference = integrate_cells(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], reference_cells, intervals,
        )
        validation = integrate_cells(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], validation_cells, intervals,
        )
        reference_validation_max = max(
            reference_validation_max, relative_rgb_l2(validation, reference),
        )
        for method in METHODS:
            for steps in STEP_COUNTS:
                candidate = integrate(
                    method, case["camera_y"], case["view_direction"], case["distance"],
                    case["sun_direction"], steps, intervals,
                )
                raw_rows.append({
                    "method": method,
                    "steps": steps,
                    "camera_y": case["camera_y"],
                    "view_name": case["view_name"],
                    "view_direction": case["view_direction"],
                    "distance": case["distance"],
                    "sun_elevation": case["sun_elevation"],
                    "sun_direction": case["sun_direction"],
                    "shadow_pattern": case["shadow_pattern"],
                    "shadow_intervals_normalized": intervals,
                    "radiance_reference_rgb": reference,
                    "radiance_candidate_rgb": candidate,
                    "relative_rgb_l2_error": relative_rgb_l2(candidate, reference),
                    "max_absolute_channel_error": max(abs(candidate[i] - reference[i]) for i in range(3)),
                })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(raw_rows[0].keys()))
        writer.writeheader()
        writer.writerows(raw_rows)

    summaries: dict[str, dict[str, object]] = {}
    pattern_summaries: dict[str, dict[str, object]] = {}
    for method in METHODS:
        summaries[method] = {}
        pattern_summaries[method] = {}
        for steps in STEP_COUNTS:
            rows = [row for row in raw_rows if row["method"] == method and row["steps"] == steps]
            errors = sorted(float(row["relative_rgb_l2_error"]) for row in rows)
            worst = max(rows, key=lambda row: float(row["relative_rgb_l2_error"]))
            max_error = float(worst["relative_rgb_l2_error"])
            summaries[method][str(steps)] = {
                "case_count": len(rows),
                "relative_rgb_l2_error_p95": errors[math.ceil(0.95 * len(errors)) - 1],
                "relative_rgb_l2_error_max": max_error,
                "max_absolute_channel_error_max": max(float(row["max_absolute_channel_error"]) for row in rows),
                "worst_case": worst,
                "passes_preregistered_gate": max_error <= MAX_RELATIVE_RGB_L2_ERROR,
            }
            pattern_summaries[method][str(steps)] = {}
            for pattern in SHADOW_PATTERNS:
                pattern_rows = [row for row in rows if row["shadow_pattern"] == pattern]
                pattern_summaries[method][str(steps)][pattern] = {
                    "case_count": len(pattern_rows),
                    "relative_rgb_l2_error_p95": sorted(
                        float(row["relative_rgb_l2_error"]) for row in pattern_rows
                    )[math.ceil(0.95 * len(pattern_rows)) - 1],
                    "relative_rgb_l2_error_max": max(
                        float(row["relative_rgb_l2_error"]) for row in pattern_rows
                    ),
                }

    minimum_pass = {
        method: next(
            (steps for steps in STEP_COUNTS if summaries[method][str(steps)]["passes_preregistered_gate"]),
            None,
        )
        for method in METHODS
    }
    manifest = {
        "schema": "fog-shadow-sampling-rd-v1",
        "purpose": "Stress-test density-only nonuniform quadrature when single-scattering visibility has a sharp boundary.",
        "execution_scope": "Standalone CPU synthetic visibility-mask diagnostic; not shadow-map parity, GPU execution, image-quality approval, or performance measurement.",
        "medium": {
            "ground": scattering.GROUND,
            "atmosphere_top": scattering.ATMOSPHERE_TOP,
            "density_at_ground": scattering.DENSITY_AT_GROUND,
            "falloff": scattering.HEIGHT_FALLOFF,
            "absorption": scattering.ABSORPTION,
            "scattering": scattering.SCATTERING,
            "sun_intensity": scattering.SUN_INTENSITY,
        },
        "case_grid": {
            "camera_heights": [2.0, 8.0, 20.0],
            "view_types": ["horizontal_120m", "upward_to_atmosphere_top", "groundward_to_ground"],
            "sun_elevations": SUN_ELEVATIONS,
            "shadow_patterns_normalized_to_ray_length": SHADOW_PATTERNS,
            "case_count": len(cases),
        },
        "visibility_model": "Binary directional shadow slab along the view ray; lit contribution is zero inside each normalized interval. It does not model geometric blockers, penumbra, or a shadow map.",
        "preregistered_gate": {
            "maximum_relative_rgb_l2_error": MAX_RELATIVE_RGB_L2_ERROR,
            "rule": "Every case must remain below the threshold against a boundary-aligned high-resolution reference.",
        },
        "quadrature": {
            "candidate_steps": STEP_COUNTS,
            "reference_steps_per_lit_path": REFERENCE_STEPS,
            "reference_validation_steps_per_lit_path": REFERENCE_VALIDATION_STEPS,
            "reference_validation_max_relative_rgb_l2_error": reference_validation_max,
            "reference": "Uniform midpoint on each lit interval, with visibility and ground boundaries aligned exactly.",
            "methods": {
                "uniform_midpoint": "Spend all samples uniformly on the full ray; reject samples whose midpoint lies in shadow.",
                "rho_one_third_weighted": "Density-curvature weighting split at ground only; samples inside shadow intervals still consume budget.",
                "shadow_boundary_aware": "Split at shadow and ground boundaries, allocate the sample budget only to lit subintervals, then use rho^(1/3) weighting. Assumes exact boundary locations are already known.",
            },
        },
        "raw_result_count": len(raw_rows),
        "raw_results_csv": {
            "path": RAW_CSV.as_posix(),
            "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest(),
        },
        "source_script": {
            "path": Path(__file__).as_posix(),
            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        },
        "summaries": summaries,
        "summaries_by_shadow_pattern": pattern_summaries,
        "minimum_step_count_passing_gate": minimum_pass,
        "limitations": [
            "Shadow is a fixed binary interval along a ray rather than a geometric occluder and projected shadow map.",
            "The shadow-aware candidate receives exact interval boundaries and is an optimistic bound, not a practical product sampling method as implemented.",
            "No penumbra, contact-hardening, shadow filtering, temporal stability, GPU cost, or float precision is measured.",
            "The result applies only to the chosen flat exponential medium and single-scattering source model.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print("case count", len(cases), "raw rows", len(raw_rows))
    print("reference 8192-vs-16384 max relative RGB L2", reference_validation_max)
    for method in METHODS:
        for steps in STEP_COUNTS:
            summary = summaries[method][str(steps)]
            print(method, steps, "p95", summary["relative_rgb_l2_error_p95"],
                  "max", summary["relative_rgb_l2_error_max"],
                  "gate", summary["passes_preregistered_gate"])
        print("first pass", method, minimum_pass[method])


if __name__ == "__main__":
    main()
