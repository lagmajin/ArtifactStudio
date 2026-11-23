#!/usr/bin/env python3
"""Compare local height-fog single scattering with and without sun-path extinction."""

from __future__ import annotations

import json
import math
from pathlib import Path

from fog_height_integral_rd import exact_density_integral


GROUND = 0.0
ATMOSPHERE_TOP = 100.0
DENSITY_AT_GROUND = 0.008
HEIGHT_FALLOFF = 0.08
ABSORPTION = 0.7
SCATTERING = 0.65
SUN_INTENSITY = 10.0
FOG_TINT = (0.46, 0.59, 0.72)
SUN_ELEVATIONS_DEGREES = (2.0, 5.0, 15.0, 45.0, 80.0)
SUN_AZIMUTH_DEGREES = 25.0
VIEW_STEPS = 256
CONVERGENCE_STEPS = (16, 32, 64, 128, 256, 512, 1024)
CONVERGENCE_REFERENCE_STEPS = 4096

Vector = tuple[float, float, float]


def dot(a: Vector, b: Vector) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def normalize(v: Vector) -> Vector:
    return tuple(x / math.sqrt(max(dot(v, v), 1e-20)) for x in v)


def rayleigh_phase(cos_theta: float) -> float:
    return 3.0 * (1.0 + cos_theta * cos_theta) / (16.0 * math.pi)


def source_rayleigh_phase(cos_theta: float) -> float:
    # Equation mirror of AtmosphereFogRenderer::phaseRayleigh.
    return 0.75 * rayleigh_phase(cos_theta)


def sun_transmittance(sample_y: float, sun_y: float) -> float:
    if sample_y >= ATMOSPHERE_TOP:
        return 1.0
    if sun_y < -1e-9 and sample_y >= GROUND:
        return 0.0  # The flat ground blocks the sun ray.
    if abs(sun_y) <= 1e-9:
        return 0.0  # Infinite horizontal path through a flat, nonzero medium.
    distance = ((ATMOSPHERE_TOP - sample_y) / sun_y if sun_y > 0.0
                else (GROUND - sample_y) / sun_y)
    if distance <= 0.0:
        return 0.0
    optical_density_length = exact_density_integral(
        sample_y, sun_y, distance, HEIGHT_FALLOFF, GROUND)
    return math.exp(-ABSORPTION * DENSITY_AT_GROUND * optical_density_length)


def integrate_scattering(
    camera_y: float, view_direction: Vector, distance: float, sun_direction: Vector,
    steps: int = VIEW_STEPS, solar_transmission: bool = True,
    source_phase: bool = False,
) -> dict[str, object]:
    cosine = max(-1.0, min(1.0, dot(tuple(-v for v in view_direction), sun_direction)))
    phase = source_rayleigh_phase(cosine) if source_phase else rayleigh_phase(cosine)
    ds = distance / steps
    radiance = [0.0, 0.0, 0.0]
    for index in range(steps):
        t = (index + 0.5) * ds
        sample_y = camera_y + view_direction[1] * t
        density_ratio = math.exp(-HEIGHT_FALLOFF * max(sample_y - GROUND, 0.0))
        view_density_length = exact_density_integral(
            camera_y, view_direction[1], t, HEIGHT_FALLOFF, GROUND)
        view_t = math.exp(-ABSORPTION * DENSITY_AT_GROUND * view_density_length)
        sun_t = sun_transmittance(sample_y, sun_direction[1]) if solar_transmission else 1.0
        source = (DENSITY_AT_GROUND * density_ratio * SCATTERING * phase
                  * SUN_INTENSITY * view_t * sun_t * ds)
        for channel in range(3):
            radiance[channel] += source * FOG_TINT[channel]
    return {"radiance_rgb": radiance, "phase": phase, "cos_theta": cosine}


def integrate_cpu_source_equation_mirror(
    camera_y: float, view_direction: Vector, distance: float, sun_direction: Vector,
    steps: int = VIEW_STEPS,
) -> list[float]:
    """Mirror evaluateFog's discrete scattering term; intentionally omits sun-path extinction."""
    cosine = max(-1.0, min(1.0, dot(tuple(-v for v in view_direction), sun_direction)))
    phase = source_rayleigh_phase(cosine)
    dt = distance / steps
    optical_depth_so_far = 0.0
    scattered = [0.0, 0.0, 0.0]
    for index in range(steps):
        t = (index + 0.5) * dt
        sample_y = camera_y + view_direction[1] * t
        density = DENSITY_AT_GROUND * math.exp(-HEIGHT_FALLOFF * max(sample_y - GROUND, 0.0))
        sample_density = density * dt
        sample_view_t = math.exp(-optical_depth_so_far - sample_density * ABSORPTION * 0.5)
        contribution = sample_density * SCATTERING * phase * sample_view_t * SUN_INTENSITY
        for channel in range(3):
            scattered[channel] += contribution * FOG_TINT[channel]
        optical_depth_so_far += sample_density * ABSORPTION
    return scattered


def relative_rgb_l2(candidate: list[float], reference: list[float]) -> float:
    error_sq = sum((candidate[channel] - reference[channel]) ** 2 for channel in range(3))
    reference_sq = sum(value * value for value in reference)
    return math.sqrt(error_sq / max(reference_sq, 1e-30))


def make_view_cases(camera_y: float) -> dict[str, tuple[Vector, float]]:
    down = normalize((0.0, -0.25, math.sqrt(1.0 - 0.25 * 0.25)))
    cases: dict[str, tuple[Vector, float]] = {
        "horizontal_120m": ((0.0, 0.0, 1.0), 120.0),
        "upward_to_atmosphere_top": ((0.0, 1.0, 0.0), max(0.0, ATMOSPHERE_TOP - camera_y)),
        "groundward_to_ground": (down, camera_y / -down[1]),
    }
    return cases


def render() -> dict[str, object]:
    results: dict[str, object] = {}
    for camera_y in (2.0, 8.0, 20.0):
        camera_key = f"camera_y_{camera_y:g}m"
        results[camera_key] = {}
        for ray_name, (view_direction, distance) in make_view_cases(camera_y).items():
            results[camera_key][ray_name] = {"view_direction": view_direction, "distance_m": distance,
                                             "sun_elevation_sweep": {}}
            for elevation in SUN_ELEVATIONS_DEGREES:
                el = math.radians(elevation)
                az = math.radians(SUN_AZIMUTH_DEGREES)
                sun_direction = normalize((math.cos(el) * math.sin(az), math.sin(el),
                                           math.cos(el) * math.cos(az)))
                exact = integrate_scattering(camera_y, view_direction, distance, sun_direction)
                no_sun_extinction = integrate_scattering(
                    camera_y, view_direction, distance, sun_direction, solar_transmission=False)
                source_model = integrate_scattering(
                    camera_y, view_direction, distance, sun_direction,
                    solar_transmission=False, source_phase=True)
                source_phase_with_sun = integrate_scattering(
                    camera_y, view_direction, distance, sun_direction, source_phase=True)
                source_discrete = integrate_cpu_source_equation_mirror(
                    camera_y, view_direction, distance, sun_direction)
                convergence_reference = integrate_scattering(
                    camera_y, view_direction, distance, sun_direction,
                    steps=CONVERGENCE_REFERENCE_STEPS)["radiance_rgb"]
                convergence = {}
                for steps in CONVERGENCE_STEPS:
                    value = integrate_scattering(camera_y, view_direction, distance,
                                                 sun_direction, steps=steps)["radiance_rgb"]
                    convergence[f"{steps}"] = {
                        "relative_rgb_l2_error_to_4096": relative_rgb_l2(value, convergence_reference),
                        "max_absolute_rgb_error_to_4096": max(
                            abs(value[channel] - convergence_reference[channel]) for channel in range(3)),
                    }
                base_luminance = sum(0.2126 * x + 0.7152 * y + 0.0722 * z
                                     for x, y, z in [tuple(exact["radiance_rgb"])])
                no_sun_luminance = sum(0.2126 * x + 0.7152 * y + 0.0722 * z
                                       for x, y, z in [tuple(no_sun_extinction["radiance_rgb"])])
                reference_luminance = base_luminance
                candidate_radiance = {
                    "normalized_phase_no_sun_transmittance": no_sun_extinction["radiance_rgb"],
                    "source_phase_with_sun_transmittance": source_phase_with_sun["radiance_rgb"],
                    "source_phase_no_sun_transmittance_analytic_view": source_model["radiance_rgb"],
                    "cpu_evaluateFog_discrete_scattering_equation_mirror": source_discrete,
                }
                candidate_ratios = {}
                for candidate_name, candidate_rgb in candidate_radiance.items():
                    candidate_luminance = (0.2126 * candidate_rgb[0] + 0.7152 * candidate_rgb[1]
                                           + 0.0722 * candidate_rgb[2])
                    candidate_ratios[candidate_name] = candidate_luminance / max(reference_luminance, 1e-20)
                results[camera_key][ray_name]["sun_elevation_sweep"][f"{elevation:g}"] = {
                    "sun_direction": sun_direction,
                    "phase_cos_theta": exact["cos_theta"],
                    "single_scattering_with_analytic_sun_transmittance": exact["radiance_rgb"],
                    "single_scattering_without_sun_transmittance": no_sun_extinction["radiance_rgb"],
                    "source_phase_no_sun_transmittance_equation_mirror": source_model["radiance_rgb"],
                    "source_phase_with_sun_transmittance": source_phase_with_sun["radiance_rgb"],
                    "cpu_evaluateFog_discrete_scattering_equation_mirror": source_discrete,
                    "sun_transmittance_radiance_ratio": base_luminance / max(no_sun_luminance, 1e-20),
                    "candidate_luminance_ratio_to_normalized_phase_with_sun_reference": candidate_ratios,
                    "view_step_convergence_to_4096": convergence,
                }

    return {
        "schema": "fog-sun-transmittance-rd-v1",
        "medium": {"ground_y_m": GROUND, "atmosphere_top_y_m": ATMOSPHERE_TOP,
                   "density_at_ground_per_m": DENSITY_AT_GROUND,
                   "height_falloff_per_m": HEIGHT_FALLOFF, "absorption": ABSORPTION,
                   "scattering": SCATTERING, "sun_intensity": SUN_INTENSITY,
                   "fog_tint_rgb": FOG_TINT},
        "integration": {"view_steps": VIEW_STEPS, "convergence_steps": CONVERGENCE_STEPS,
                        "convergence_reference_steps": CONVERGENCE_REFERENCE_STEPS,
                        "sun_path": "Piecewise analytic exponential-height optical depth to top boundary; flat ground blocks downward sun rays.",
                        "view_transmittance": "Piecewise analytic exponential-height optical depth to each view sample.",
                        "phase": "Normalized Rayleigh; separate equation mirror uses 0.75 * normalized phase.",
                        "note": "CPU numeric study only; view scattering still uses midpoint quadrature."},
        "candidate_definitions": {
            "normalized_phase_with_sun_reference": "Normalized Rayleigh phase, analytic view transmittance, analytic sun-path transmittance.",
            "cpu_evaluateFog_discrete_scattering_equation_mirror": "C++ evaluateFog discrete view loop, source 0.75 Rayleigh phase and midpoint view attenuation; no sun-path transmittance. Omits final empirical horizon/fog blend and is not runtime parity.",
        },
        "camera_results": results,
        "limitations": ["Synthetic flat local fog medium; no planetary curvature or terrain shadow map.",
                        "Arbitrary world scale and sun radiance; no measured lighting calibration.",
                        "Scattering term only; omits the legacy renderer's additional empirical horizon/fog blend.",
                        "No C++ runtime parity, GPU capture, temporal sampling, or performance measurement."],
    }


def main() -> None:
    output_dir = Path("docs/worklog/artifacts/fog-sun-transmittance-rd-2026-11-12")
    output_dir.mkdir(parents=True, exist_ok=True)
    result = render()
    path = output_dir / "manifest.json"
    path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote sun-path scattering sweep: {path}")
    for camera, rays in result["camera_results"].items():
        horizontal = rays["horizontal_120m"]["sun_elevation_sweep"]
        print(camera, {angle: round(values["sun_transmittance_radiance_ratio"], 4)
                       for angle, values in horizontal.items()})


if __name__ == "__main__":
    main()
