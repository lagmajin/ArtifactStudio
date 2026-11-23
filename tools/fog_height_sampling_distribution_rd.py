#!/usr/bin/env python3
"""Compare curvature-weighted sample placement for exponential height fog.

The nonuniform rule equidistributes rho^(1/3), following the local composite
midpoint error model |f''| h^3. It is an offline CPU numerical study only.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_convergence_sweep_rd as baseline
from fog_height_integral_rd import exact_density_integral, marched_density_integral


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-height-sampling-distribution-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
STEP_COUNTS = (16, 32, 64, 128, 256)
METHODS = ("uniform_midpoint", "ground_split_midpoint", "rho_one_third_weighted")

# The convergence gate is carried forward unchanged from the previous sweep.
MAX_ABSOLUTE_TRANSMITTANCE_ERROR = 1.0e-3
MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1 = 1.0e-2


def invert_rho_weight_cumulative(a: float, total_length: float, fraction: float) -> float:
    """Invert integral_0^s exp(-a*t)dt at a normalized cumulative fraction."""
    if abs(a * total_length) < 1.0e-8:
        return total_length * fraction
    total = -math.expm1(-a * total_length) / a
    target = total * fraction
    argument = -a * target
    if argument <= -1.0:
        return total_length * fraction
    return -math.log1p(argument) / a


def rho_one_third_weighted_integral(
    y0: float, dy_ds: float, distance: float, steps: int,
    falloff: float, ground: float,
) -> tuple[float, int]:
    """Integrate below-ground constant density exactly and sample above it.

    In the above-ground interval, sample-cell boundaries are equally spaced in
    cumulative rho^(1/3), which clusters cells near the high-density boundary.
    The returned count is the number of density samples, not transcendental ops.
    """
    segments = baseline.ground_split_segments(y0, dy_ds, distance, ground)
    total = 0.0
    density_samples = 0
    for start, end in segments:
        length = end - start
        segment_y0 = y0 + dy_ds * start
        segment_y1 = y0 + dy_ds * end
        midpoint_height = 0.5 * (segment_y0 + segment_y1)
        if midpoint_height < ground:
            total += length
            continue

        if falloff == 0.0 or abs(dy_ds) < 1.0e-12:
            total += length * math.exp(-falloff * max(midpoint_height - ground, 0.0))
            density_samples += 1
            continue

        a = falloff * dy_ds / 3.0
        previous = 0.0
        for index in range(1, steps + 1):
            current = invert_rho_weight_cumulative(a, length, index / steps)
            cell_length = current - previous
            sample_s = 0.5 * (previous + current)
            height = segment_y0 + dy_ds * sample_s
            total += math.exp(-falloff * max(height - ground, 0.0)) * cell_length
            previous = current
            density_samples += 1
    return total, density_samples


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[math.ceil(fraction * len(ordered)) - 1]


def main() -> None:
    cases = baseline.make_cases()
    raw_rows: list[dict[str, object]] = []
    for case in cases:
        exact_integral = exact_density_integral(
            case["y0"], case["dy_ds"], case["distance"], case["falloff"], baseline.GROUND,
        )
        exact_t = math.exp(-case["density"] * baseline.ABSORPTION * exact_integral)
        for method in METHODS:
            for steps in STEP_COUNTS:
                if method == "uniform_midpoint":
                    approx_integral = marched_density_integral(
                        case["y0"], case["dy_ds"], case["distance"], steps,
                        case["falloff"], baseline.GROUND,
                    )
                    density_samples = steps
                elif method == "ground_split_midpoint":
                    approx_integral = baseline.split_midpoint_integral(
                        case["y0"], case["dy_ds"], case["distance"], steps,
                        case["falloff"], baseline.GROUND,
                    )
                    density_samples = steps
                else:
                    approx_integral, density_samples = rho_one_third_weighted_integral(
                        case["y0"], case["dy_ds"], case["distance"], steps,
                        case["falloff"], baseline.GROUND,
                    )
                approx_t = math.exp(-case["density"] * baseline.ABSORPTION * approx_integral)
                absolute_error = abs(approx_t - exact_t)
                relative_error = absolute_error / exact_t if exact_t > 1.0e-12 else None
                raw_rows.append({
                    "method": method,
                    "steps": steps,
                    "density_samples": density_samples,
                    **case,
                    "exact_density_integral": exact_integral,
                    "approx_density_integral": approx_integral,
                    "exact_transmittance": exact_t,
                    "approx_transmittance": approx_t,
                    "absolute_transmittance_error": absolute_error,
                    "relative_transmittance_error": relative_error,
                })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(raw_rows[0].keys()))
        writer.writeheader()
        writer.writerows(raw_rows)

    summaries: dict[str, dict[str, object]] = {}
    for method in METHODS:
        method_summary: dict[str, object] = {}
        for steps in STEP_COUNTS:
            rows = [r for r in raw_rows if r["method"] == method and r["steps"] == steps]
            abs_errors = [float(r["absolute_transmittance_error"]) for r in rows]
            rel_rows = [r for r in rows if float(r["exact_transmittance"]) >= 0.1]
            rel_errors = [float(r["relative_transmittance_error"]) for r in rel_rows]
            worst_abs = max(rows, key=lambda r: float(r["absolute_transmittance_error"]))
            worst_rel = max(rel_rows, key=lambda r: float(r["relative_transmittance_error"]))
            max_abs = float(worst_abs["absolute_transmittance_error"])
            max_rel = float(worst_rel["relative_transmittance_error"])
            method_summary[str(steps)] = {
                "case_count": len(rows),
                "absolute_error_p95": percentile(abs_errors, 0.95),
                "absolute_error_max": max_abs,
                "absolute_error_worst_case": worst_abs,
                "relative_error_p95_T_ge_0_1": percentile(rel_errors, 0.95),
                "relative_error_max_T_ge_0_1": max_rel,
                "relative_error_worst_case_T_ge_0_1": worst_rel,
                "density_sample_count_min": min(int(r["density_samples"]) for r in rows),
                "density_sample_count_max": max(int(r["density_samples"]) for r in rows),
                "passes_preregistered_gate": (
                    max_abs <= MAX_ABSOLUTE_TRANSMITTANCE_ERROR
                    and max_rel <= MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1
                ),
            }
        summaries[method] = method_summary

    minimum_pass = {
        method: next(
            (steps for steps in STEP_COUNTS
             if summaries[method][str(steps)]["passes_preregistered_gate"]),
            None,
        )
        for method in METHODS
    }
    manifest = {
        "schema": "fog-height-sampling-distribution-rd-v1",
        "purpose": "Compare sample placement strategies for a known exponential height-density integral.",
        "execution_scope": "Standalone Python CPU numerical analysis; no product renderer, shader compilation, GPU capture, or performance measurement.",
        "case_grid": {
            "camera_heights": baseline.CAMERA_HEIGHTS,
            "ray_elevations_degrees": baseline.RAY_ANGLES_DEGREES,
            "distances": baseline.DISTANCES,
            "ground_densities": baseline.DENSITIES,
            "falloffs": baseline.FALLOFFS,
            "case_count": len(cases),
        },
        "preregistered_gate": {
            "max_absolute_transmittance_error": MAX_ABSOLUTE_TRANSMITTANCE_ERROR,
            "max_relative_transmittance_error_when_reference_T_at_least_0_1": MAX_RELATIVE_TRANSMITTANCE_ERROR_AT_T_GE_0_1,
            "rule": "Both limits must pass over every case; inherited unchanged from the preceding convergence sweep.",
        },
        "sampling_rule": {
            "method": "rho_one_third_weighted",
            "derivation": "For composite midpoint, local error scales approximately as |f''| h^3. For exponential density, |rho''| is proportional to rho, so equalizing local error suggests interval density proportional to rho^(1/3).",
            "mapping": "Cell boundaries equally spaced in cumulative integral of rho^(1/3); invert analytically using log1p/expm1.",
            "ground_handling": "Split at analytic ground crossing; integrate constant below-ground density exactly and spend the sample budget in the above-ground interval.",
            "cost_caveat": "Same maximum number of density samples as comparison methods, but nonuniform boundaries add transcendental mapping work; no runtime cost was measured.",
        },
        "quadrature": {
            "steps": STEP_COUNTS,
            "reference": "Piecewise closed-form height-density integral in Python double precision.",
            "baseline_methods": {
                "uniform_midpoint": "Uniform midpoint over the whole ray.",
                "ground_split_midpoint": "Ground split with total midpoint sample budget allocated proportional to segment length.",
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
        "minimum_step_count_passing_gate": minimum_pass,
        "limitations": [
            "Known analytic exponential density only; the sampling rule is specialized and may not generalize to local volumes, noisy fields, or multiple scattering integrands.",
            "The error model motivates the sample mapping but does not prove global optimality for the transmittance output.",
            "Accuracy does not prove visual quality, shader precision, GPU speed, resource behavior, or backend parity.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(MANIFEST.as_posix())
    for method in METHODS:
        for steps in STEP_COUNTS:
            summary = summaries[method][str(steps)]
            print(
                f"{method} steps={steps}: abs_p95={summary['absolute_error_p95']:.8g} "
                f"abs_max={summary['absolute_error_max']:.8g} "
                f"rel_max_T>=0.1={summary['relative_error_max_T_ge_0_1']:.8g} "
                f"pass={summary['passes_preregistered_gate']}"
            )
        print(f"{method} first passing steps: {minimum_pass[method]}")


if __name__ == "__main__":
    main()
