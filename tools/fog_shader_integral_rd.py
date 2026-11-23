#!/usr/bin/env python3
"""Audit fogHF.hlsli height-fog line integral against independent quadrature.

This is a deterministic CPU equation mirror, not a shader compile or GPU run.
It separates the height-profile integral from the source's start-distance fade.
"""

from __future__ import annotations

import json
import math
from pathlib import Path


OUTPUT = Path("docs/worklog/artifacts/fog-shader-integral-rd-2026-11-12/manifest.json")
HEIGHT_START = 10.0
HEIGHT_END = 40.0
FOG_DENSITY = 0.012
START_DISTANCES = (0.0, 10.0, 30.0, 60.0)
ORIGINS = (-20.0, 0.0, 10.0, 25.0, 60.0)
PITCH_DEGREES = (-89.0, -60.0, -20.0, 0.0, 20.0, 60.0, 89.0)
DISTANCES = (1.0, 10.0, 60.0, 200.0, 1000.0)
REFERENCE_STEPS = 65536


def density_profile(height: float) -> float:
    """Mirror fogHF's constant-below-start, exponential-above profile."""
    fog_falloff = 6.907755 / max(0.01, HEIGHT_END - HEIGHT_START)
    return math.exp(-max(height - HEIGHT_START, 0.0) * fog_falloff)


def source_line_integral(origin_y: float, direction_y: float, distance: float) -> float:
    """Mirror the source's baseDistance + exponentialHeightLineIntegral."""
    effective_z = max(abs(direction_y), 0.001)
    end_height = origin_y + distance * direction_y
    min_height = min(origin_y, end_height)
    height_line_falloff = max(min_height - HEIGHT_START, 0.0)
    base_distance = min(max((HEIGHT_START - min_height) / effective_z, 0.0), distance)
    exponential_distance = distance - base_distance
    fog_falloff = 6.907755 / max(0.01, HEIGHT_END - HEIGHT_START)
    exponential_integral = (
        math.exp(-height_line_falloff * fog_falloff)
        * (1.0 - math.exp(-exponential_distance * effective_z * fog_falloff))
        / (effective_z * fog_falloff)
    )
    return base_distance + exponential_integral


def stable_limit_line_integral(origin_y: float, direction_y: float, distance: float) -> float:
    """Diagnostic analytic candidate using the true horizontal limit and expm1."""
    fog_falloff = 6.907755 / max(0.01, HEIGHT_END - HEIGHT_START)
    abs_z = abs(direction_y)
    if abs_z < 1e-12:
        return distance * density_profile(origin_y)
    end_height = origin_y + distance * direction_y
    min_height = min(origin_y, end_height)
    height_line_falloff = max(min_height - HEIGHT_START, 0.0)
    base_distance = min(max((HEIGHT_START - min_height) / abs_z, 0.0), distance)
    exponential_distance = distance - base_distance
    exponent = exponential_distance * abs_z * fog_falloff
    exponential_integral = (
        math.exp(-height_line_falloff * fog_falloff)
        * (-math.expm1(-exponent))
        / (abs_z * fog_falloff)
    )
    return base_distance + exponential_integral


def quadrature_line_integral(origin_y: float, direction_y: float, distance: float) -> float:
    """Independent midpoint quadrature of the height profile along the ray."""
    step = distance / REFERENCE_STEPS
    total = 0.0
    for index in range(REFERENCE_STEPS):
        t = (index + 0.5) * step
        total += density_profile(origin_y + direction_y * t) * step
    return total


def start_fade(distance: float, start: float) -> float:
    """Mirror saturate((distance - fog.start) / fog.start), including start=0."""
    if start <= 0.0:
        return 1.0 if distance > 0.0 else 0.0
    return min(max((distance - start) / start, 0.0), 1.0)


def exact_cutoff_integral(origin_y: float, direction_y: float, distance: float, start: float) -> float:
    """Physical hard-start reference: integrate only the segment after start."""
    active_distance = max(distance - start, 0.0)
    return quadrature_line_integral(origin_y + direction_y * min(start, distance), direction_y, active_distance)


def main() -> None:
    cases = []
    worst_profile = {"relative_error": 0.0}
    worst_stable_candidate = {"relative_error": 0.0}
    worst_start = {"absolute_optical_depth_difference": 0.0}
    for origin_y in ORIGINS:
        for pitch in PITCH_DEGREES:
            direction_y = math.sin(math.radians(pitch))
            for distance in DISTANCES:
                source_integral = source_line_integral(origin_y, direction_y, distance)
                stable_integral = stable_limit_line_integral(origin_y, direction_y, distance)
                reference_integral = quadrature_line_integral(origin_y, direction_y, distance)
                profile_rel = abs(source_integral - reference_integral) / max(reference_integral, 1e-12)
                stable_rel = abs(stable_integral - reference_integral) / max(reference_integral, 1e-12)
                if profile_rel > worst_profile["relative_error"]:
                    worst_profile = {
                        "relative_error": profile_rel,
                        "origin_y": origin_y,
                        "pitch_degrees": pitch,
                        "distance": distance,
                        "source_integral": source_integral,
                        "quadrature_integral": reference_integral,
                    }
                if stable_rel > worst_stable_candidate["relative_error"]:
                    worst_stable_candidate = {
                        "relative_error": stable_rel,
                        "origin_y": origin_y,
                        "pitch_degrees": pitch,
                        "distance": distance,
                        "candidate_integral": stable_integral,
                        "quadrature_integral": reference_integral,
                    }
                start_cases = []
                for start in START_DISTANCES:
                    shader_tau = FOG_DENSITY * start_fade(distance, start) * source_integral
                    exact_faded_tau = FOG_DENSITY * start_fade(distance, start) * reference_integral
                    hard_tau = FOG_DENSITY * exact_cutoff_integral(origin_y, direction_y, distance, start)
                    discrepancy = abs(exact_faded_tau - hard_tau)
                    case = {
                        "start_distance": start,
                        "source_start_fade": start_fade(distance, start),
                        "source_optical_depth": shader_tau,
                        "exact_height_integral_with_source_fade_optical_depth": exact_faded_tau,
                        "hard_cutoff_optical_depth": hard_tau,
                        "exact_fade_vs_hard_cutoff_absolute_optical_depth_difference": discrepancy,
                    }
                    start_cases.append(case)
                    if discrepancy > worst_start["absolute_optical_depth_difference"]:
                        worst_start = {
                            "absolute_optical_depth_difference": discrepancy,
                            "origin_y": origin_y,
                            "pitch_degrees": pitch,
                            "distance": distance,
                            **case,
                        }
                cases.append({
                    "origin_y": origin_y,
                    "pitch_degrees": pitch,
                    "direction_y": direction_y,
                    "distance": distance,
                    "source_height_integral": source_integral,
                    "stable_limit_candidate_height_integral": stable_integral,
                    "quadrature_height_integral": reference_integral,
                    "relative_height_integral_error": profile_rel,
                    "stable_candidate_relative_height_integral_error": stable_rel,
                    "start_distance_comparisons": start_cases,
                })

    manifest = {
        "schema": "fog-shader-integral-rd-v1",
        "source": "Artifact/shaders/fogHF.hlsli::GetFogAmount height-fog branch",
        "medium": {
            "height_start": HEIGHT_START,
            "height_end": HEIGHT_END,
            "density": FOG_DENSITY,
            "profile": "1 below height_start; exp(-6.907755*(height-height_start)/(height_end-height_start)) above",
        },
        "sampling": {
            "origin_heights": list(ORIGINS),
            "ray_pitches_degrees": list(PITCH_DEGREES),
            "distances": list(DISTANCES),
            "start_distances": list(START_DISTANCES),
            "quadrature_steps": REFERENCE_STEPS,
            "case_count": len(cases),
        },
        "summary": {
            "worst_source_height_integral_relative_error_vs_quadrature": worst_profile,
            "worst_stable_limit_candidate_relative_error_vs_quadrature": worst_stable_candidate,
            "largest_exact_source_fade_vs_hard_cutoff_optical_depth_difference": worst_start,
        },
        "limitations": [
            "CPU equation mirror only; HLSL was not compiled or dispatched.",
            "Hard cutoff is an explicit comparison policy; the exact-fade comparison isolates policy from height-integral approximation and does not claim the source fade is a bug.",
            "No shader color selection, atmospheric LUT, host binding, or GPU performance is represented.",
        ],
        "cases": cases,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT} ({len(cases)} height-profile cases)")
    print("worst profile integral relative error:", worst_profile)
    print("worst stable-limit candidate relative error:", worst_stable_candidate)
    print("largest start-fade vs hard-cutoff optical-depth difference:", worst_start)


if __name__ == "__main__":
    main()
