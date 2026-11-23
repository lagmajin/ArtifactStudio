#!/usr/bin/env python3
"""Sweep binary/soft shadow visibility and interval-width estimation for fog quadrature."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_convergence_sweep_rd as convergence
import fog_height_sampling_distribution_rd as sampling
import fog_shadow_sampling_rd as shadow
import fog_sun_transmittance_rd as scattering
from fog_height_integral_rd import exact_density_integral


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-penumbra-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
PENUMBRA_WIDTHS = (0.0, 0.002, 0.01, 0.04)
ESTIMATED_WIDTH_ERRORS = (-0.02, -0.005, 0.0, 0.005, 0.02)
STEP_COUNTS = (32, 128, 512, 2048)
WIDTH_ERROR_STEPS = 128
REFERENCE_STEPS = 16384
VALIDATION_STEPS = 8192
GATE = shadow.MAX_RELATIVE_RGB_L2_ERROR


def profile_knots(intervals, penumbra: float) -> list[float]:
    knots = {0.0, 1.0}
    for a, b in intervals:
        if penumbra <= 0.0:
            knots.update((a, b))
        else:
            knots.update((max(0.0, a - penumbra), a, b, min(1.0, b + penumbra)))
    return sorted(knots)


def visibility(u: float, intervals, penumbra: float) -> float:
    for a, b in intervals:
        if a <= u <= b:
            return 0.0
        if penumbra > 0.0 and a - penumbra < u < a:
            x = (a - u) / penumbra
            return x * x * (3.0 - 2.0 * x)
        if penumbra > 0.0 and b < u < b + penumbra:
            x = (u - b) / penumbra
            return x * x * (3.0 - 2.0 * x)
    return 1.0


def estimated_intervals(truth, relative_width_error: float):
    estimated = []
    for a, b in truth:
        center = 0.5 * (a + b)
        half = 0.5 * (b - a) * (1.0 + relative_width_error)
        left, right = max(0.0, center - half), min(1.0, center + half)
        if right > left:
            estimated.append((left, right))
    return tuple(estimated)


def ground_segments(camera_y: float, view_y: float, distance: float,
                    knots: list[float]) -> list[tuple[float, float]]:
    cuts = {u * distance for u in knots}
    cuts.update((0.0, distance))
    ordered = sorted(cuts)
    pieces = []
    for a, b in zip(ordered, ordered[1:]):
        if b <= a:
            continue
        y0 = camera_y + view_y * a
        for x, y in convergence.ground_split_segments(y0, view_y, b - a, scattering.GROUND):
            pieces.append((a + x, a + y))
    return pieces


def rho_visibility_weight(y0: float, view_y: float, start: float, end: float,
                          distance: float, intervals, penumbra: float) -> float:
    # Midpoint integration is used only to allocate a bounded integer sample budget.
    # The candidate radiance is still evaluated with its actual visibility at each sample.
    subdivisions = 16
    width = (end - start) / subdivisions
    total = 0.0
    for i in range(subdivisions):
        t = start + (i + 0.5) * width
        y = y0 + view_y * t
        density = math.exp(-scattering.HEIGHT_FALLOFF * max(y - scattering.GROUND, 0.0))
        vis = visibility(t / distance, intervals, penumbra)
        total += (density * vis) ** (1.0 / 3.0) * width
    return total


def weighted_cells(camera_y: float, view_y: float, distance: float, steps: int,
                   intervals, penumbra: float) -> list[tuple[float, float]]:
    knots = profile_knots(intervals, penumbra)
    pieces = ground_segments(camera_y, view_y, distance, knots)
    pieces = [p for p in pieces
              if visibility(0.5 * (p[0] + p[1]) / distance, intervals, penumbra) > 0.0]
    if not pieces:
        return []
    weights = [rho_visibility_weight(camera_y, view_y, a, b, distance, intervals, penumbra)
               for a, b in pieces]
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
            a = scattering.HEIGHT_FALLOFF * view_y / 3.0
            previous = 0.0
            for i in range(1, count + 1):
                current = sampling.invert_rho_weight_cumulative(a, length, i / count)
                cells.append((start + previous, start + current))
                previous = current
    return cells


def integrate_cells(case, cells, true_intervals, true_penumbra: float) -> list[float]:
    camera_y = case["camera_y"]
    direction = case["view_direction"]
    distance = case["distance"]
    sun = case["sun_direction"]
    cosine = max(-1.0, min(1.0, scattering.dot(tuple(-v for v in direction), sun)))
    phase = scattering.rayleigh_phase(cosine)
    result = [0.0, 0.0, 0.0]
    for start, end in cells:
        t = 0.5 * (start + end)
        vis = visibility(t / distance, true_intervals, true_penumbra)
        if vis <= 0.0:
            continue
        y = camera_y + direction[1] * t
        density = math.exp(-scattering.HEIGHT_FALLOFF * max(y - scattering.GROUND, 0.0))
        view_depth = exact_density_integral(
            camera_y, direction[1], t, scattering.HEIGHT_FALLOFF, scattering.GROUND,
        )
        view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_depth)
        sun_t = scattering.sun_transmittance(y, sun[1])
        scale = (scattering.DENSITY_AT_GROUND * density * scattering.SCATTERING * phase
                 * scattering.SUN_INTENSITY * view_t * sun_t * vis * (end - start))
        for channel in range(3):
            result[channel] += scale * scattering.FOG_TINT[channel]
    return result


def reference_cells(case, intervals, penumbra: float, steps: int):
    knots = profile_knots(intervals, penumbra)
    pieces = ground_segments(case["camera_y"], case["view_direction"][1],
                             case["distance"], knots)
    pieces = [p for p in pieces
              if visibility(0.5 * (p[0] + p[1]) / case["distance"], intervals, penumbra) > 0.0]
    counts = convergence.allocate_steps([b - a for a, b in pieces], steps)
    cells = []
    for (a, b), count in zip(pieces, counts):
        width = (b - a) / count
        cells.extend((a + i * width, a + (i + 1) * width) for i in range(count))
    return cells


def main() -> None:
    cases = shadow.make_cases()
    references = {}
    validation_max = 0.0
    rows = []
    for case_index, case in enumerate(cases):
        truth = case["shadow_intervals_normalized"]
        for penumbra in PENUMBRA_WIDTHS:
            key = (case_index, penumbra)
            ref = integrate_cells(case, reference_cells(case, truth, penumbra, REFERENCE_STEPS),
                                  truth, penumbra)
            val = integrate_cells(case, reference_cells(case, truth, penumbra, VALIDATION_STEPS),
                                  truth, penumbra)
            references[key] = ref
            validation_max = max(validation_max, shadow.relative_rgb_l2(val, ref))
            for width_error in ESTIMATED_WIDTH_ERRORS:
                estimated = estimated_intervals(truth, width_error)
                steps_to_run = STEP_COUNTS if width_error == 0.0 else (WIDTH_ERROR_STEPS,)
                for steps in steps_to_run:
                    visibility_weighted = weighted_cells(
                        case["camera_y"], case["view_direction"][1], case["distance"],
                        steps, estimated, penumbra,
                    )
                    candidates = [("estimated_visibility_weighted", visibility_weighted)]
                    if width_error == 0.0:
                        # Baselines and estimated candidate use the same true profile without width error.
                        uniform_width = case["distance"] / steps
                        uniform = [(i * uniform_width, (i + 1) * uniform_width) for i in range(steps)]
                        density_only = shadow.make_weighted_cells(
                            case["camera_y"], case["view_direction"][1], case["distance"],
                            steps, (), False,
                        )
                        candidates = [("uniform_midpoint", uniform),
                                      ("rho_one_third_density_only", density_only), *candidates]
                    for method, cells in candidates:
                        candidate = integrate_cells(case, cells, truth, penumbra)
                        rows.append({
                            "case_index": case_index,
                            "method": method,
                            "steps": steps,
                            "true_penumbra_width_fraction": penumbra,
                            "estimated_interval_relative_width_error": width_error,
                            "camera_y": case["camera_y"],
                            "view_name": case["view_name"],
                            "sun_elevation": case["sun_elevation"],
                            "shadow_pattern": case["shadow_pattern"],
                            "true_intervals": truth,
                            "estimated_intervals": estimated,
                            "radiance_reference_rgb": references[key],
                            "radiance_candidate_rgb": candidate,
                            "relative_rgb_l2_error": shadow.relative_rgb_l2(candidate, references[key]),
                        })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summaries = {}
    for method in ("uniform_midpoint", "rho_one_third_density_only", "estimated_visibility_weighted"):
        summaries[method] = {}
        for penumbra in PENUMBRA_WIDTHS:
            summaries[method][str(penumbra)] = {}
            relevant_width_errors = ESTIMATED_WIDTH_ERRORS if method == "estimated_visibility_weighted" else (0.0,)
            for width_error in relevant_width_errors:
                summaries[method][str(penumbra)][f"{width_error:+.3%}"] = {}
                available_steps = STEP_COUNTS if width_error == 0.0 else (WIDTH_ERROR_STEPS,)
                for steps in available_steps:
                    selected = [r["relative_rgb_l2_error"] for r in rows
                                if r["method"] == method and r["steps"] == steps
                                and r["true_penumbra_width_fraction"] == penumbra
                                and math.isclose(r["estimated_interval_relative_width_error"], width_error)]
                    selected.sort()
                    summaries[method][str(penumbra)][f"{width_error:+.3%}"][str(steps)] = {
                        "case_count": len(selected),
                        "p95_relative_rgb_l2_error": selected[math.ceil(0.95 * len(selected)) - 1],
                        "max_relative_rgb_l2_error": selected[-1],
                        "passes_gate": selected[-1] <= GATE,
                    }
    minimum_steps = {}
    for method, by_penumbra in summaries.items():
        minimum_steps[method] = {}
        for penumbra, by_error in by_penumbra.items():
            minimum_steps[method][penumbra] = {}
            for width_error, by_steps in by_error.items():
                minimum_steps[method][penumbra][width_error] = next(
                    (int(n) for n, summary in by_steps.items() if summary["passes_gate"]), None,
                )

    manifest = {
        "schema": "fog-shadow-penumbra-rd-v1",
        "purpose": "Predeclared CPU sweep of quadrature under smooth shadow transitions and estimated shadow-width error.",
        "execution_scope": "Synthetic visibility fixture for numerical sensitivity only; no geometric shadow maps, image output, GPU, or runtime timing.",
        "case_grid": "Same 189 deterministic cases as fog-shadow-sampling-rd: 3 camera heights, 3 view types, 3 sun elevations, 7 shadow patterns.",
        "true_penumbra_widths_fraction_of_ray": PENUMBRA_WIDTHS,
        "estimated_core_width_relative_errors": ESTIMATED_WIDTH_ERRORS,
        "evaluation_matrix": "For zero width error, compare all three methods at 32/128/512/2048 steps. For nonzero width errors, evaluate the visibility-weighted candidate at 128 steps only; this avoids repeating width-independent baselines and gives a fixed-budget sensitivity comparison.",
        "visibility_definition": "Zero visibility inside the true/estimated core interval; outside, cubic smoothstep ramps from 0 to 1 over the penumbra width. Penumbra width is fixed and known to the candidate; only core width is perturbed.",
        "methods": {
            "uniform_midpoint": "Uniform cells over full ray; true visibility evaluated at each midpoint.",
            "rho_one_third_density_only": "Existing density-curvature weighted cells split at ground only; true visibility evaluated at each midpoint.",
            "estimated_visibility_weighted": "Split at estimated visibility knots and ground crossings, omit predicted-zero intervals, and allocate rho^(1/3)*visibility^(1/3) weight; evaluate true visibility at cell midpoints.",
        },
        "pre_registered_evaluation_plan": {
            "reference": f"Piecewise-aligned uniform midpoint with {REFERENCE_STEPS} samples across nonzero-visibility intervals; validate against {VALIDATION_STEPS}.",
            "gate": f"Maximum relative RGB L2 error <= {GATE} across all 189 conditions for a step count.",
            "statistics": "Per-condition raw relative RGB L2, p95 and maximum by method, penumbra width, estimated-width error, and steps; minimum passing step count.",
            "fixed_inputs": "Same fog medium, camera/view/sun grid, and shadow patterns as the prior binary-boundary study.",
        },
        "reference_validation_max_relative_rgb_l2_error": validation_max,
        "step_counts": STEP_COUNTS,
        "raw_result_count": len(rows),
        "raw_results_csv": {"path": RAW_CSV.as_posix(), "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(), "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "summaries": summaries,
        "minimum_steps_passing_gate": minimum_steps,
        "limitations": [
            "The cubic transition is an analytic proxy; it is not a filtered shadow map or penumbra derived from light size/occluder geometry.",
            "Predicted interval-width error is a symmetric width scale around the exact center; center/location error is covered by the prior study, not combined here.",
            "Visibility-knot sampling assumes the estimated profile and penumbra width are available; cost and robustness of obtaining them are unmeasured.",
            "No real image, alpha, temporal stability, GPU backend, instruction count, or performance measurement is included.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print("cases", len(cases), "raw rows", len(rows), "reference validation max", validation_max)
    for method in summaries:
        for penumbra in PENUMBRA_WIDTHS:
            for width_error in ESTIMATED_WIDTH_ERRORS:
                if method != "estimated_visibility_weighted" and width_error != 0.0:
                    continue
                steps = minimum_steps[method][str(penumbra)][f"{width_error:+.3%}"]
                print(method, "penumbra", penumbra, "width error", width_error,
                      "min gate steps", steps)


if __name__ == "__main__":
    main()
