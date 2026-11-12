#!/usr/bin/env python3
"""Measure single-scattering quadrature sensitivity to estimated shadow edges."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_shadow_sampling_rd as shadow


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-boundary-sensitivity-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "raw_cases.csv"
MANIFEST = OUTPUT_DIR / "manifest.json"
EDGE_OFFSETS = (0.001, 0.005, 0.01, 0.02)
STEP_COUNTS = (32, 64, 128, 256, 512, 1024, 2048)
GATE = shadow.MAX_RELATIVE_RGB_L2_ERROR


def shift_intervals(intervals: tuple[tuple[float, float], ...], offset: float):
    shifted = []
    for start, end in intervals:
        a = max(0.0, min(1.0, start + offset))
        b = max(0.0, min(1.0, end + offset))
        if b > a:
            shifted.append((a, b))
    return tuple(shifted)


def main() -> None:
    cases = shadow.make_cases()
    rows = []
    reference_max = 0.0
    for case in cases:
        truth = case["shadow_intervals_normalized"]
        reference_cells = shadow.uniform_visible_cells(
            case["camera_y"], case["view_direction"][1], case["distance"],
            shadow.REFERENCE_STEPS, truth,
        )
        validation_cells = shadow.uniform_visible_cells(
            case["camera_y"], case["view_direction"][1], case["distance"],
            shadow.REFERENCE_VALIDATION_STEPS, truth,
        )
        reference = shadow.integrate_cells(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], reference_cells, truth,
        )
        validation = shadow.integrate_cells(
            case["camera_y"], case["view_direction"], case["distance"],
            case["sun_direction"], validation_cells, truth,
        )
        reference_max = max(reference_max, shadow.relative_rgb_l2(validation, reference))
        hypotheses = [("exact", 0.0, truth)]
        for amount in EDGE_OFFSETS:
            for sign in (-1, 1):
                hypotheses.append(("underestimate" if sign < 0 else "overestimate", sign * amount,
                                   shift_intervals(truth, sign * amount)))
        for error_type, offset, estimated in hypotheses:
            for steps in STEP_COUNTS:
                cells = shadow.make_weighted_cells(
                    case["camera_y"], case["view_direction"][1], case["distance"],
                    steps, estimated, True,
                )
                candidate = shadow.integrate_cells(
                    case["camera_y"], case["view_direction"], case["distance"],
                    case["sun_direction"], cells, truth,
                )
                rows.append({
                    "edge_error_type": error_type,
                    "edge_offset_fraction_of_ray": offset,
                    "edge_offset_percent_of_ray": offset * 100.0,
                    "steps": steps,
                    "camera_y": case["camera_y"],
                    "view_name": case["view_name"],
                    "distance": case["distance"],
                    "sun_elevation": case["sun_elevation"],
                    "shadow_pattern": case["shadow_pattern"],
                    "true_shadow_intervals": truth,
                    "estimated_shadow_intervals": estimated,
                    "radiance_reference_rgb": reference,
                    "radiance_candidate_rgb": candidate,
                    "relative_rgb_l2_error": shadow.relative_rgb_l2(candidate, reference),
                })

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    offset_keys = [("exact", 0.0)] + [
        (kind, sign * amount)
        for amount in EDGE_OFFSETS for sign, kind in ((-1, "underestimate"), (1, "overestimate"))
    ]
    for kind, offset in offset_keys:
        key = f"{kind}:{offset:+.3%}"
        summary[key] = {}
        for steps in STEP_COUNTS:
            selected = [r for r in rows if r["edge_error_type"] == kind
                        and math.isclose(r["edge_offset_fraction_of_ray"], offset)
                        and r["steps"] == steps]
            errors = sorted(r["relative_rgb_l2_error"] for r in selected)
            summary[key][str(steps)] = {
                "case_count": len(selected),
                "p95_relative_rgb_l2_error": errors[math.ceil(0.95 * len(errors)) - 1],
                "max_relative_rgb_l2_error": max(errors),
                "passes_gate": max(errors) <= GATE,
            }
    minimum_steps = {
        key: next((n for n in STEP_COUNTS if data[str(n)]["passes_gate"]), None)
        for key, data in summary.items()
    }
    manifest = {
        "schema": "fog-shadow-boundary-sensitivity-rd-v1",
        "purpose": "Quantify how translating estimated binary shadow intervals changes single-scattering quadrature error.",
        "scope": "Standalone CPU stress fixture; synthetic intervals are not geometric shadow maps or GPU measurements.",
        "case_count": len(cases),
        "case_grid": "Same 189 cases as fog-shadow-sampling-rd: 3 camera heights, 3 view types, 3 sun elevations, 7 shadow patterns.",
        "edge_offsets_fraction_of_ray_length": list(EDGE_OFFSETS),
        "offset_semantics": "Translate both interval edges together and clamp to [0,1]. Overestimate expands the predicted shadow toward later ray distances; underestimate moves it earlier. Empty/collapsed predicted intervals are allowed.",
        "candidate": "Allocate rho^(1/3)-weighted samples only to predicted-lit intervals; evaluate true binary visibility at each sample. Thus overestimated shadows can omit truly lit contributions, while underestimated shadows waste samples inside true shadows.",
        "reference": "16384 midpoint samples per true lit interval with exact shadow and ground boundaries; validated against 8192 samples.",
        "reference_validation_max_relative_rgb_l2_error": reference_max,
        "preregistered_gate_max_relative_rgb_l2_error": GATE,
        "step_counts": STEP_COUNTS,
        "raw_result_count": len(rows),
        "raw_results_csv": {"path": RAW_CSV.as_posix(), "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(), "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "summary": summary,
        "minimum_steps_passing_gate": minimum_steps,
        "limitations": [
            "Boundary error is a rigid translation of a synthetic binary interval, not a projected geometric shadow-edge error distribution.",
            "No penumbra, filtering, temporal motion, GPU precision, or performance is measured.",
            "Sample omission/waste models the stated predicted-lit interval policy only; it does not establish a production shadow-aware algorithm.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(MANIFEST.as_posix())
    print("case count", len(cases), "raw rows", len(rows))
    print("reference validation max", reference_max)
    for key, steps in minimum_steps.items():
        print(key, "minimum passing steps", steps)


if __name__ == "__main__":
    main()
