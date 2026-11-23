#!/usr/bin/env python3
"""Validate selected planetary-scattering sampling outliers at higher view resolution."""

from __future__ import annotations

import json
import math
from pathlib import Path

import planetary_atmosphere_scattering_rd as atmosphere


OUTPUT = Path("docs/worklog/artifacts/planetary-atmosphere-sampling-robustness-rd-2026-11-13/convergence_manifest.json")
SUN_STEPS = 192
LEVELS = ((128, 48), (256, 96), (512, 192), (1024, 384))
CASES = (
    ("sunward_high_sun_low_camera", 75.0, 500.0, 75.0),
    ("sunward_mid_sun_low_camera", 18.0, 500.0, 18.0),
    ("horizon_high_sun_low_camera", 75.0, 500.0, 0.0),
)
CANDIDATES = {"uniform_32": (32, "uniform"), "altitude_biased_32": (32, "altitude_biased"),
              "uniform_64": (64, "uniform")}


def direction(pitch_degrees: float) -> tuple[float, float, float]:
    pitch = math.radians(pitch_degrees)
    azimuth = math.radians(atmosphere.SUN_AZIMUTH_DEGREES)
    return atmosphere.normalize((math.cos(pitch) * math.sin(azimuth), math.sin(pitch),
                                 math.cos(pitch) * math.cos(azimuth)))


def relative_error(value: tuple[float, float, float], reference: tuple[float, float, float]) -> float:
    numerator = math.sqrt(sum((value[c] - reference[c]) ** 2 for c in range(3)))
    denominator = math.sqrt(sum(reference[c] ** 2 for c in range(3)))
    return numerator / max(denominator, 1e-30)


def main() -> None:
    output_cases = []
    for name, sun_elevation, camera_altitude, ray_pitch in CASES:
        ray = direction(ray_pitch)
        levels = []
        for view_steps, sun_steps in LEVELS:
            value = atmosphere.integrate_pixel(
                ray, view_steps, sun_steps,
                sun_elevation_degrees=sun_elevation,
                camera_altitude_m=camera_altitude,
                view_sampling="uniform",
            )
            levels.append({"view_steps": view_steps, "sun_steps": sun_steps,
                           "radiance_rgb": list(value)})
        converged_reference = tuple(levels[-1]["radiance_rgb"])
        candidate_values = {}
        for candidate_name, (view_steps, sampling) in CANDIDATES.items():
            value = atmosphere.integrate_pixel(
                ray, view_steps, SUN_STEPS,
                sun_elevation_degrees=sun_elevation,
                camera_altitude_m=camera_altitude,
                view_sampling=sampling,
            )
            candidate_values[candidate_name] = {
                "view_steps": view_steps,
                "view_sampling": sampling,
                "radiance_rgb": list(value),
                "relative_rgb_l2_to_1024x384": relative_error(value, converged_reference),
            }
        output_cases.append({
            "name": name,
            "sun_elevation_degrees": sun_elevation,
            "camera_altitude_m": camera_altitude,
            "ray_pitch_degrees": ray_pitch,
            "ray_azimuth_degrees": atmosphere.SUN_AZIMUTH_DEGREES,
            "view_direction": list(ray),
            "convergence_levels_uniform_view": levels,
            "candidate_errors_to_1024x384": candidate_values,
            "reference_step_deltas_relative_rgb_l2": [
                relative_error(tuple(levels[i]["radiance_rgb"]), converged_reference)
                for i in range(len(levels) - 1)
            ],
        })
    manifest = {
        "schema": "planetary-atmosphere-sampling-convergence-rd-v1",
        "fixed_sun_steps_for_view_convergence": SUN_STEPS,
        "levels_view_x_sun": [list(level) for level in LEVELS],
        "reference": "uniform 1024x384 view/sun midpoint; high-resolution numeric reference, not exact solution",
        "cases": output_cases,
        "limitations": [
            "Three selected CPU rays only, chosen from the preceding 54-case robustness sweep.",
            "Sun integration is held at 192 steps to isolate view-integration convergence.",
            "No HLSL/GPU execution, full image, camera sequence, or runtime cost measurement.",
        ],
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT} ({len(output_cases)} selected-ray convergence cases)")
    for case in output_cases:
        errs = {name: item["relative_rgb_l2_to_1024x384"]
                for name, item in case["candidate_errors_to_1024x384"].items()}
        print(case["name"], "reference deltas", case["reference_step_deltas_relative_rgb_l2"],
              "candidate errors", errs)


if __name__ == "__main__":
    main()
