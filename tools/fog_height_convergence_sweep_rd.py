#!/usr/bin/env python3
"""CPU convergence sweep for fixed-step exponential height-fog integration.

This is an offline numerical experiment, not a product implementation or test.
It compares uniform midpoint integration with the same quadrature split at an
analytic ground crossing, using the same total sample budget.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

from fog_height_integral_rd import exact_density_integral, marched_density_integral


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-height-convergence-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
GROUND = 0.0
ABSORPTION = 0.7
CAMERA_HEIGHTS = (-10.0, 0.0, 2.0, 8.0, 20.0, 100.0)
RAY_ANGLES_DEGREES = (-75.0, -45.0, -15.0, 0.0, 15.0, 45.0, 75.0)
DISTANCES = (30.0, 120.0, 500.0)
DENSITIES = (0.002, 0.008, 0.02)
FALLOFFS = (0.005, 0.02, 0.08, 0.2)
STEP_COUNTS = (8, 16, 32, 64, 128, 256, 512)

# Prospective numerical gate declared before evaluating this sweep:
# transmission error <= 0.001 absolute, and <= 1% relative where T >= 0.1.
MAX_ABSOLUTE_TRANSMITTANCE_ERROR = 1.0e-3
MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1 = 1.0e-2


def ground_split_segments(
    y0: float, dy_ds: float, distance: float, ground: float,
) -> list[tuple[float, float]]:
    y1 = y0 + dy_ds * distance
    crosses = (y0 < ground <= y1) or (y1 < ground <= y0)
    if not crosses or abs(dy_ds) < 1.0e-12:
        return [(0.0, distance)]
    crossing = min(distance, max(0.0, (ground - y0) / dy_ds))
    if crossing <= 0.0 or crossing >= distance:
        return [(0.0, distance)]
    return [(0.0, crossing), (crossing, distance)]


def allocate_steps(lengths: list[float], budget: int) -> list[int]:
    if len(lengths) == 1:
        return [budget]
    if budget < len(lengths):
        raise ValueError("sample budget must cover every split segment")
    total_length = sum(lengths)
    exact = [budget * length / total_length for length in lengths]
    allocation = [max(1, math.floor(value)) for value in exact]
    while sum(allocation) < budget:
        index = max(range(len(lengths)), key=lambda i: (exact[i] - math.floor(exact[i]), lengths[i] / allocation[i]))
        allocation[index] += 1
    while sum(allocation) > budget:
        candidates = [i for i, count in enumerate(allocation) if count > 1]
        if not candidates:
            raise ValueError("cannot honor bounded sample budget")
        index = min(candidates, key=lambda i: (exact[i] - math.floor(exact[i]), lengths[i] / allocation[i]))
        allocation[index] -= 1
    return allocation


def split_midpoint_integral(
    y0: float, dy_ds: float, distance: float, steps: int, falloff: float, ground: float,
) -> float:
    segments = ground_split_segments(y0, dy_ds, distance, ground)
    counts = allocate_steps([end - start for start, end in segments], steps)
    total = 0.0
    for (start, end), count in zip(segments, counts):
        total += marched_density_integral(
            y0 + dy_ds * start, dy_ds, end - start, count, falloff, ground,
        )
    return total


def make_cases() -> list[dict[str, float]]:
    cases = []
    for y0 in CAMERA_HEIGHTS:
        for angle in RAY_ANGLES_DEGREES:
            dy_ds = math.sin(math.radians(angle))
            for distance in DISTANCES:
                for density in DENSITIES:
                    for falloff in FALLOFFS:
                        cases.append({
                            "y0": y0,
                            "angle_degrees": angle,
                            "dy_ds": dy_ds,
                            "distance": distance,
                            "density": density,
                            "falloff": falloff,
                        })
    return cases


def main() -> None:
    cases = make_cases()
    methods = ("uniform_midpoint", "ground_split_midpoint")
    raw_rows: list[dict[str, object]] = []
    for case in cases:
        exact_integral = exact_density_integral(
            case["y0"], case["dy_ds"], case["distance"], case["falloff"], GROUND,
        )
        exact_t = math.exp(-case["density"] * ABSORPTION * exact_integral)
        for method in methods:
            for steps in STEP_COUNTS:
                if method == "uniform_midpoint":
                    approx_integral = marched_density_integral(
                        case["y0"], case["dy_ds"], case["distance"], steps,
                        case["falloff"], GROUND,
                    )
                else:
                    approx_integral = split_midpoint_integral(
                        case["y0"], case["dy_ds"], case["distance"], steps,
                        case["falloff"], GROUND,
                    )
                approx_t = math.exp(-case["density"] * ABSORPTION * approx_integral)
                absolute_error = abs(approx_t - exact_t)
                relative_error = absolute_error / exact_t if exact_t > 1.0e-12 else None
                raw_rows.append({
                    "method": method,
                    "steps": steps,
                    **case,
                    "exact_density_integral": exact_integral,
                    "approx_density_integral": approx_integral,
                    "exact_transmittance": exact_t,
                    "approx_transmittance": approx_t,
                    "absolute_transmittance_error": absolute_error,
                    "relative_transmittance_error": relative_error,
                })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    fields = list(raw_rows[0].keys())
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(raw_rows)

    summaries: dict[str, dict[str, object]] = {}
    for method in methods:
        summaries[method] = {}
        for steps in STEP_COUNTS:
            rows = [row for row in raw_rows if row["method"] == method and row["steps"] == steps]
            absolute = sorted(float(row["absolute_transmittance_error"]) for row in rows)
            relative_eligible = [
                row for row in rows if float(row["exact_transmittance"]) >= 0.1
            ]
            relative = sorted(float(row["relative_transmittance_error"]) for row in relative_eligible)
            worst_abs = max(rows, key=lambda row: float(row["absolute_transmittance_error"]))
            worst_rel = max(relative_eligible, key=lambda row: float(row["relative_transmittance_error"]))
            passed = (
                float(worst_abs["absolute_transmittance_error"]) <= MAX_ABSOLUTE_TRANSMITTANCE_ERROR
                and float(worst_rel["relative_transmittance_error"])
                <= MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1
            )
            summaries[method][str(steps)] = {
                "case_count": len(rows),
                "absolute_error_p95": absolute[math.ceil(0.95 * len(absolute)) - 1],
                "absolute_error_max": float(worst_abs["absolute_transmittance_error"]),
                "absolute_error_worst_case": worst_abs,
                "relative_error_case_count_T_ge_0_1": len(relative_eligible),
                "relative_error_p95_T_ge_0_1": relative[math.ceil(0.95 * len(relative)) - 1],
                "relative_error_max_T_ge_0_1": float(worst_rel["relative_transmittance_error"]),
                "relative_error_worst_case_T_ge_0_1": worst_rel,
                "passes_preregistered_gate": passed,
            }

    minimum_pass_steps: dict[str, int | None] = {}
    for method in methods:
        minimum_pass_steps[method] = next(
            (steps for steps in STEP_COUNTS if summaries[method][str(steps)]["passes_preregistered_gate"]),
            None,
        )

    manifest = {
        "schema": "fog-height-convergence-rd-v1",
        "purpose": "Numerical convergence diagnostic for fixed-step exponential height-fog extinction.",
        "execution_scope": "Standalone Python CPU analysis; no product renderer, test target, shader compilation, GPU capture, or performance measurement.",
        "medium": {
            "density_at_ground_values": DENSITIES,
            "falloff_values_per_scene_unit": FALLOFFS,
            "ground": GROUND,
            "absorption": ABSORPTION,
            "distance_values_scene_units": DISTANCES,
            "camera_height_values_scene_units": CAMERA_HEIGHTS,
            "ray_elevation_values_degrees": RAY_ANGLES_DEGREES,
        },
        "quadrature": {
            "steps": STEP_COUNTS,
            "methods": {
                "uniform_midpoint": "Uniform midpoint over the entire ray interval.",
                "ground_split_midpoint": "Split at the exact analytic ground intersection, then allocate the same total midpoint sample budget proportional to segment length, with at least one sample per segment.",
            },
            "reference": "Piecewise closed-form exponential-height integral split at ground crossing using Python double precision.",
        },
        "preregistered_gate": {
            "max_absolute_transmittance_error": MAX_ABSOLUTE_TRANSMITTANCE_ERROR,
            "max_relative_transmittance_error_when_reference_T_at_least_0_1": MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1,
            "rule": "Both bounds must pass across the full 1512-case sweep; this is a numerical integration gate, not a visual quality or GPU performance claim.",
        },
        "case_count": len(cases),
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
        "minimum_step_count_passing_gate": minimum_pass_steps,
        "limitations": [
            "Homogeneous horizontal geometry with analytic height profile; no local media, shadow boundaries, texture noise, or temporal variation.",
            "The split method assumes the ground height and ray direction are known and that density is piecewise smooth except at ground.",
            "A numerical transmittance gate does not establish acceptable image quality, shader precision, GPU time, or backend parity.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(MANIFEST.as_posix())
    for method in methods:
        for steps in STEP_COUNTS:
            summary = summaries[method][str(steps)]
            print(
                f"{method} steps={steps}: abs_p95={summary['absolute_error_p95']:.8g} "
                f"abs_max={summary['absolute_error_max']:.8g} "
                f"rel_p95_T>=0.1={summary['relative_error_p95_T_ge_0_1']:.8g} "
                f"rel_max_T>=0.1={summary['relative_error_max_T_ge_0_1']:.8g} "
                f"pass={summary['passes_preregistered_gate']}"
            )
        print(f"{method} first passing step count: {minimum_pass_steps[method]}")


if __name__ == "__main__":
    main()
