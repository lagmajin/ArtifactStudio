#!/usr/bin/env python3
"""Stress CPU single-scattering view-sample placement across sun/altitude/ray."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import planetary_atmosphere_scattering_rd as atmosphere


OUTPUT_DIR = Path("docs/worklog/artifacts/planetary-atmosphere-sampling-robustness-rd-2026-11-13")
SUN_ELEVATIONS = (2.0, 18.0, 75.0)
CAMERA_ALTITUDES_M = (500.0, 2_000.0, 10_000.0)
RAY_PITCHES_DEGREES = (-8.0, 0.0, 2.0, 10.0, 90.0)
REFERENCE = (128, 48)
CANDIDATES = {"uniform_32": (32, "uniform"), "altitude_biased_32": (32, "altitude_biased"),
              "uniform_64": (64, "uniform")}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def ray_at_pitch(pitch_degrees: float) -> tuple[float, float, float]:
    pitch = math.radians(pitch_degrees)
    azimuth = math.radians(atmosphere.SUN_AZIMUTH_DEGREES)
    cosine = math.cos(pitch)
    return atmosphere.normalize((cosine * math.sin(azimuth), math.sin(pitch),
                                 cosine * math.cos(azimuth)))


def error(candidate: tuple[float, float, float], reference: tuple[float, float, float]) -> dict[str, object]:
    deltas = [candidate[channel] - reference[channel] for channel in range(3)]
    absolute_l2 = math.sqrt(sum(value * value for value in deltas))
    reference_l2 = math.sqrt(sum(value * value for value in reference))
    return {
        "reference_rgb": list(reference),
        "candidate_rgb": list(candidate),
        "absolute_rgb_l2": absolute_l2,
        "relative_rgb_l2": absolute_l2 / max(reference_l2, 1.0e-30),
        "max_absolute_channel_error": max(abs(value) for value in deltas),
    }


def main() -> None:
    cases: list[dict[str, object]] = []
    for sun_elevation in SUN_ELEVATIONS:
        sunward_pitch = sun_elevation
        pitches = list(RAY_PITCHES_DEGREES[:-1])
        if not any(abs(pitch - sunward_pitch) < 1e-9 for pitch in pitches):
            pitches.append(sunward_pitch)
        pitches.append(RAY_PITCHES_DEGREES[-1])
        for camera_altitude in CAMERA_ALTITUDES_M:
            for pitch in pitches:
                direction = ray_at_pitch(pitch)
                reference = atmosphere.integrate_pixel(
                    direction, REFERENCE[0], REFERENCE[1],
                    sun_elevation_degrees=sun_elevation,
                    camera_altitude_m=camera_altitude,
                    view_sampling="uniform",
                )
                candidate_results = {}
                for name, (view_steps, sampling) in CANDIDATES.items():
                    candidate = atmosphere.integrate_pixel(
                        direction, view_steps, REFERENCE[1],
                        sun_elevation_degrees=sun_elevation,
                        camera_altitude_m=camera_altitude,
                        view_sampling=("uniform" if sampling == "uniform" else "altitude_biased"),
                    )
                    candidate_results[name] = error(candidate, reference)
                cases.append({
                    "sun_elevation_degrees": sun_elevation,
                    "camera_altitude_m": camera_altitude,
                    "ray_pitch_degrees": pitch,
                    "ray_azimuth_degrees": atmosphere.SUN_AZIMUTH_DEGREES,
                    "view_direction": list(direction),
                    "reference_view_steps": REFERENCE[0],
                    "sun_steps_all_candidates_and_reference": REFERENCE[1],
                    "candidates": candidate_results,
                })

    biased_wins = sum(
        case["candidates"]["altitude_biased_32"]["relative_rgb_l2"]
        < case["candidates"]["uniform_32"]["relative_rgb_l2"]
        for case in cases
    )
    summaries = {}
    for name in CANDIDATES:
        values = [case["candidates"][name]["relative_rgb_l2"] for case in cases]
        absolute = [case["candidates"][name]["absolute_rgb_l2"] for case in cases]
        summaries[name] = {
            "case_count": len(values),
            "median_relative_rgb_l2": sorted(values)[len(values) // 2],
            "p95_relative_rgb_l2": sorted(values)[math.ceil(len(values) * 0.95) - 1],
            "max_relative_rgb_l2": max(values),
            "median_absolute_rgb_l2": sorted(absolute)[len(absolute) // 2],
            "max_absolute_rgb_l2": max(absolute),
        }

    source_path = Path("tools/planetary_atmosphere_scattering_rd.py")
    manifest = {
        "schema": "planetary-atmosphere-sampling-robustness-rd-v1",
        "source_script": source_path.as_posix(),
        "source_script_sha256": sha256(source_path),
        "sweep": {
            "sun_elevations_degrees": list(SUN_ELEVATIONS),
            "camera_altitudes_m": list(CAMERA_ALTITUDES_M),
            "ray_pitches_degrees": "-8, 0, 2, 10, sun-aligned, 90; azimuth follows configured sun azimuth",
            "case_count": len(cases),
            "reference": {"view_steps": REFERENCE[0], "sun_steps": REFERENCE[1],
                          "view_sampling": "uniform"},
            "candidate_sun_steps": REFERENCE[1],
            "candidates": {name: {"view_steps": steps, "view_sampling": sampling}
                           for name, (steps, sampling) in CANDIDATES.items()},
        },
        "summary": {
            "metrics_by_candidate": summaries,
            "altitude_biased_32_cases_better_than_uniform_32": biased_wins,
            "altitude_biased_32_case_fraction_better": biased_wins / len(cases),
        },
        "limitations": [
            "Selected CPU rays only; not a full-image metric or GPU implementation.",
            "Sun-path step count is fixed across view-sampling candidates to isolate view quadrature.",
            "128x48 is a numerical comparison reference, not proven converged for every geometry.",
            "No temporal camera sequence, LUT, terrain, ground reflection, or performance timing is included.",
        ],
        "cases": cases,
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output = OUTPUT_DIR / "manifest.json"
    output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {output} ({len(cases)} selected-ray cases)")
    print("summary:", json.dumps(summaries, sort_keys=True))
    print("altitude-biased wins:", biased_wins, "/", len(cases))


if __name__ == "__main__":
    main()
