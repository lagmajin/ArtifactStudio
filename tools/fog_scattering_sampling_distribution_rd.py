#!/usr/bin/env python3
"""Test density-curvature sample placement on a single-scattering integral."""

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


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-scattering-sampling-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
STEPS = (16, 32, 64, 128, 256, 512)
REFERENCE_STEPS = 32768
REFERENCE_VALIDATION_STEPS = 16384
MAX_RELATIVE_RGB_L2_ERROR = 1.0e-3
METHODS = ("uniform_midpoint", "rho_one_third_weighted")


def weighted_segment_steps(
    y0: float, dy: float, length: float, falloff: float, ground: float,
) -> float:
    y1 = y0 + dy * length
    if max(y0, y1) <= ground:
        return length
    return exact_density_integral(y0, dy, length, falloff / 3.0, ground)


def nonuniform_cells(
    camera_y: float, view_y: float, distance: float, steps: int,
) -> list[tuple[float, float]]:
    segments = convergence.ground_split_segments(
        camera_y, view_y, distance, scattering.GROUND,
    )
    weights = [
        weighted_segment_steps(
            camera_y + view_y * start, view_y, end - start,
            scattering.HEIGHT_FALLOFF, scattering.GROUND,
        )
        for start, end in segments
    ]
    counts = convergence.allocate_steps(weights, steps)
    cells: list[tuple[float, float]] = []
    for (start, end), count in zip(segments, counts):
        length = end - start
        segment_y0 = camera_y + view_y * start
        segment_y1 = camera_y + view_y * end
        if max(segment_y0, segment_y1) <= scattering.GROUND or abs(view_y) < 1.0e-12:
            for index in range(count):
                cell_start = start + length * index / count
                cell_end = start + length * (index + 1) / count
                cells.append((cell_start, cell_end))
            continue
        a = scattering.HEIGHT_FALLOFF * view_y / 3.0
        previous = 0.0
        for index in range(1, count + 1):
            current = sampling.invert_rho_weight_cumulative(a, length, index / count)
            cells.append((start + previous, start + current))
            previous = current
    return cells


def integrate_scattering(
    camera_y: float, view_direction: scattering.Vector, distance: float,
    sun_direction: scattering.Vector, steps: int, method: str,
) -> list[float]:
    cosine = max(-1.0, min(1.0, scattering.dot(tuple(-v for v in view_direction), sun_direction)))
    phase = scattering.rayleigh_phase(cosine)
    if method == "uniform_midpoint":
        cell_width = distance / steps
        cells = [(i * cell_width, (i + 1) * cell_width) for i in range(steps)]
    else:
        cells = nonuniform_cells(camera_y, view_direction[1], distance, steps)
    radiance = [0.0, 0.0, 0.0]
    for start, end in cells:
        width = end - start
        t = 0.5 * (start + end)
        sample_y = camera_y + view_direction[1] * t
        density_ratio = math.exp(-scattering.HEIGHT_FALLOFF * max(sample_y - scattering.GROUND, 0.0))
        view_density_length = exact_density_integral(
            camera_y, view_direction[1], t,
            scattering.HEIGHT_FALLOFF, scattering.GROUND,
        )
        view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_density_length)
        sun_t = scattering.sun_transmittance(sample_y, sun_direction[1])
        source = (
            scattering.DENSITY_AT_GROUND * density_ratio * scattering.SCATTERING
            * phase * scattering.SUN_INTENSITY * view_t * sun_t * width
        )
        for channel in range(3):
            radiance[channel] += source * scattering.FOG_TINT[channel]
    return radiance


def relative_rgb_l2(candidate: list[float], reference: list[float]) -> float:
    error_sq = sum((candidate[i] - reference[i]) ** 2 for i in range(3))
    reference_sq = sum(value * value for value in reference)
    return math.sqrt(error_sq / max(reference_sq, 1.0e-30))


def make_sun_cases() -> list[dict[str, object]]:
    cases: list[dict[str, object]] = []
    for camera_y in (2.0, 8.0, 20.0):
        for view_name, (view_direction, distance) in scattering.make_view_cases(camera_y).items():
            for elevation in scattering.SUN_ELEVATIONS_DEGREES:
                el = math.radians(elevation)
                az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
                sun_direction = scattering.normalize((
                    math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az),
                ))
                cases.append({
                    "camera_y": camera_y,
                    "view_name": view_name,
                    "view_direction": view_direction,
                    "distance": distance,
                    "sun_elevation": elevation,
                    "sun_direction": sun_direction,
                })
    return cases


def main() -> None:
    cases = make_sun_cases()
    raw_rows: list[dict[str, object]] = []
    reference_validation_max = 0.0
    for case in cases:
        reference = integrate_scattering(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], REFERENCE_STEPS, "uniform_midpoint",
        )
        validation = integrate_scattering(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], REFERENCE_VALIDATION_STEPS, "uniform_midpoint",
        )
        reference_validation_max = max(
            reference_validation_max, relative_rgb_l2(validation, reference),
        )
        for method in METHODS:
            for steps in STEPS:
                candidate = integrate_scattering(
                    case["camera_y"], case["view_direction"], case["distance"],
                    case["sun_direction"], steps, method,
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
    for method in METHODS:
        summaries[method] = {}
        for steps in STEPS:
            rows = [r for r in raw_rows if r["method"] == method and r["steps"] == steps]
            relative = sorted(float(r["relative_rgb_l2_error"]) for r in rows)
            worst = max(rows, key=lambda r: float(r["relative_rgb_l2_error"]))
            max_error = float(worst["relative_rgb_l2_error"])
            summaries[method][str(steps)] = {
                "case_count": len(rows),
                "relative_rgb_l2_error_p95": relative[math.ceil(0.95 * len(relative)) - 1],
                "relative_rgb_l2_error_max": max_error,
                "max_absolute_channel_error_max": max(float(r["max_absolute_channel_error"]) for r in rows),
                "worst_case": worst,
                "passes_preregistered_gate": max_error <= MAX_RELATIVE_RGB_L2_ERROR,
            }
    minimum_pass = {
        method: next(
            (steps for steps in STEPS if summaries[method][str(steps)]["passes_preregistered_gate"]),
            None,
        )
        for method in METHODS
    }

    manifest = {
        "schema": "fog-scattering-sampling-rd-v1",
        "purpose": "Check whether density-curvature sample placement improves view-ray single-scattering integration.",
        "execution_scope": "Standalone Python CPU diagnostic; not C++ runtime parity, GPU execution, visual image evaluation, or performance measurement.",
        "medium": {
            "ground_y": scattering.GROUND,
            "atmosphere_top_y": scattering.ATMOSPHERE_TOP,
            "density_at_ground": scattering.DENSITY_AT_GROUND,
            "falloff": scattering.HEIGHT_FALLOFF,
            "absorption": scattering.ABSORPTION,
            "scattering": scattering.SCATTERING,
            "sun_intensity": scattering.SUN_INTENSITY,
            "fog_tint": scattering.FOG_TINT,
        },
        "case_grid": {
            "camera_heights": [2.0, 8.0, 20.0],
            "view_types": ["horizontal_120m", "upward_to_atmosphere_top", "groundward_to_ground"],
            "sun_elevations": scattering.SUN_ELEVATIONS_DEGREES,
            "sun_azimuth": scattering.SUN_AZIMUTH_DEGREES,
            "case_count": len(cases),
        },
        "preregistered_gate": {
            "maximum_relative_rgb_l2_error": MAX_RELATIVE_RGB_L2_ERROR,
            "rule": "Each candidate must remain below the threshold for every case against the uniform midpoint reference.",
        },
        "quadrature": {
            "candidate_steps": STEPS,
            "reference_steps": REFERENCE_STEPS,
            "reference_validation_steps": REFERENCE_VALIDATION_STEPS,
            "reference_validation_max_relative_rgb_l2_error": reference_validation_max,
            "reference": "High-step uniform midpoint over analytic view transmittance and analytic sun-path transmittance.",
            "density_weighted_mapping": "Equal intervals in cumulative rho^(1/3), split at ground; segment sample budget proportional to integral rho^(1/3).",
            "same_ray_sample_budget": True,
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
        "minimum_step_count_passing_gate": minimum_pass,
        "limitations": [
            "Flat local medium; no terrain shadow, local volumes, noise, multiple scattering, or temporal accumulation.",
            "Density-only weighting may not be optimal for the full product scattering integrand, which also depends on view and sun transmittance.",
            "The reference is high-resolution CPU quadrature, not an exact closed form for the complete scattering integral.",
            "No output images, GPU cost, transcendental instruction cost, or backend parity were measured.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print(f"reference 16384-vs-32768 max relative RGB L2: {reference_validation_max:.8g}")
    for method in METHODS:
        for steps in STEPS:
            summary = summaries[method][str(steps)]
            print(
                f"{method} steps={steps}: rel_p95={summary['relative_rgb_l2_error_p95']:.8g} "
                f"rel_max={summary['relative_rgb_l2_error_max']:.8g} "
                f"pass={summary['passes_preregistered_gate']}"
            )
        print(f"{method} first passing steps: {minimum_pass[method]}")


if __name__ == "__main__":
    main()
