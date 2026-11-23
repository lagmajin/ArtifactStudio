#!/usr/bin/env python3
"""Sweep combined shadow-center and core-width estimation errors."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_shadow_penumbra_rd as penumbra
import fog_shadow_sampling_rd as shadow


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-combined-error-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
CENTER_OFFSETS = (-0.02, -0.005, 0.0, 0.005, 0.02)
WIDTH_ERRORS = (-0.02, -0.005, 0.0, 0.005, 0.02)
STEPS = 128


def estimate_intervals(truth, center_offset: float, width_error: float):
    intervals = []
    for left, right in truth:
        center = 0.5 * (left + right) + center_offset
        half_width = 0.5 * (right - left) * (1.0 + width_error)
        a = max(0.0, center - half_width)
        b = min(1.0, center + half_width)
        if b > a:
            intervals.append((a, b))
    return tuple(intervals)


def main() -> None:
    cases = shadow.make_cases()
    rows = []
    references = {}
    validation_max = 0.0
    for case_index, case in enumerate(cases):
        truth = case["shadow_intervals_normalized"]
        for soft_width in penumbra.PENUMBRA_WIDTHS:
            ref = penumbra.integrate_cells(
                case,
                penumbra.reference_cells(case, truth, soft_width, penumbra.REFERENCE_STEPS),
                truth, soft_width,
            )
            val = penumbra.integrate_cells(
                case,
                penumbra.reference_cells(case, truth, soft_width, penumbra.VALIDATION_STEPS),
                truth, soft_width,
            )
            references[(case_index, soft_width)] = ref
            validation_max = max(validation_max, shadow.relative_rgb_l2(val, ref))
            for center_offset in CENTER_OFFSETS:
                for width_error in WIDTH_ERRORS:
                    estimated = estimate_intervals(truth, center_offset, width_error)
                    cells = penumbra.weighted_cells(
                        case["camera_y"], case["view_direction"][1], case["distance"],
                        STEPS, estimated, soft_width,
                    )
                    candidate = penumbra.integrate_cells(case, cells, truth, soft_width)
                    rows.append({
                        "case_index": case_index,
                        "steps": STEPS,
                        "true_penumbra_width_fraction": soft_width,
                        "estimated_center_offset_fraction_of_ray": center_offset,
                        "estimated_core_relative_width_error": width_error,
                        "camera_y": case["camera_y"],
                        "view_name": case["view_name"],
                        "sun_elevation": case["sun_elevation"],
                        "shadow_pattern": case["shadow_pattern"],
                        "true_intervals": truth,
                        "estimated_intervals": estimated,
                        "radiance_reference_rgb": references[(case_index, soft_width)],
                        "radiance_candidate_rgb": candidate,
                        "relative_rgb_l2_error": shadow.relative_rgb_l2(
                            candidate, references[(case_index, soft_width)],
                        ),
                    })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    min_pass = {}
    for soft_width in penumbra.PENUMBRA_WIDTHS:
        summary[str(soft_width)] = {}
        min_pass[str(soft_width)] = {}
        for center_offset in CENTER_OFFSETS:
            summary[str(soft_width)][f"{center_offset:+.3%}"] = {}
            min_pass[str(soft_width)][f"{center_offset:+.3%}"] = {}
            for width_error in WIDTH_ERRORS:
                selected = sorted(
                    row["relative_rgb_l2_error"] for row in rows
                    if row["true_penumbra_width_fraction"] == soft_width
                    and math.isclose(row["estimated_center_offset_fraction_of_ray"], center_offset)
                    and math.isclose(row["estimated_core_relative_width_error"], width_error)
                )
                key = f"{width_error:+.3%}"
                passed = selected[-1] <= penumbra.GATE
                summary[str(soft_width)][f"{center_offset:+.3%}"][key] = {
                    "case_count": len(selected),
                    "p95_relative_rgb_l2_error": selected[math.ceil(0.95 * len(selected)) - 1],
                    "max_relative_rgb_l2_error": selected[-1],
                    "passes_gate": passed,
                }
                min_pass[str(soft_width)][f"{center_offset:+.3%}"][key] = STEPS if passed else None

    manifest = {
        "schema": "fog-shadow-combined-error-rd-v1",
        "purpose": "Measure interaction between shadow-center and core-width errors in penumbra-aware scattering quadrature.",
        "execution_scope": "Deterministic CPU synthetic visibility fixture; no geometric shadow map, GPU, rendered image, or performance claim.",
        "case_grid": "189 cases: same 3 camera heights, 3 view types, 3 sun elevations and 7 shadow patterns as prior shadow studies.",
        "true_penumbra_widths_fraction_of_ray": penumbra.PENUMBRA_WIDTHS,
        "estimated_center_offsets_fraction_of_ray": CENTER_OFFSETS,
        "estimated_core_relative_width_errors": WIDTH_ERRORS,
        "candidate": "128-step rho^(1/3)*visibility^(1/3) quadrature over estimated visibility knots; evaluate the true smoothstep visibility at sample midpoints.",
        "visibility": "True core has zero visibility. Cubic smoothstep transitions to full visibility over the configured width outside each core edge; the estimator uses the same transition width.",
        "pre_registered_evaluation_plan": {
            "gate": f"Maximum relative RGB L2 error <= {penumbra.GATE} across all 189 cases per parameter combination.",
            "reference": f"Piecewise-aligned {penumbra.REFERENCE_STEPS}-step midpoint, checked against {penumbra.VALIDATION_STEPS} steps.",
            "statistics": "Record p95, maximum and pass/fail for every center-offset × core-width-error × penumbra-width combination.",
            "candidate_budget": STEPS,
        },
        "reference_validation_max_relative_rgb_l2_error": validation_max,
        "raw_result_count": len(rows),
        "raw_results_csv": {"path": RAW_CSV.as_posix(), "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(), "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "summary": summary,
        "passes_gate": min_pass,
        "limitations": [
            "Center offset and width error are independently controlled synthetic perturbations; their real joint distribution is unknown.",
            "Penumbra uses a cubic smoothstep proxy with known transition width, not a geometric soft-shadow model.",
            "The visibility-knot candidate receives an estimated profile and is not a production shadow-query algorithm.",
            "Only 128 samples are measured here; no cost, temporal stability, image-level defect, or backend behavior is assessed.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print("cases", len(cases), "raw rows", len(rows), "reference validation max", validation_max)
    for soft_width in penumbra.PENUMBRA_WIDTHS:
        pass_count = sum(
            1 for center in CENTER_OFFSETS for width in WIDTH_ERRORS
            if min_pass[str(soft_width)][f"{center:+.3%}"][f"{width:+.3%}"] is not None
        )
        print("penumbra", soft_width, "passing parameter combinations", pass_count,
              "of", len(CENTER_OFFSETS) * len(WIDTH_ERRORS))


if __name__ == "__main__":
    main()
