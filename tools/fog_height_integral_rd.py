#!/usr/bin/env python3
"""Deterministic CPU R&D comparison for exponential height-fog integration.

This is an offline numeric study, not a product renderer or test target. The
medium has rho(y) = rho0 * exp(-k * max(y - ground, 0)); the output compares
midpoint ray marching with a piecewise closed-form optical-depth reference.
"""

from __future__ import annotations

import argparse
import math


GROUND = 0.0
RHO0 = 0.002
FALLOFF = 0.01
ABSORPTION = 1.0
SCATTERING = 0.65
SUN_INTENSITY = 2.0


CASES = (
    ("horizontal_at_ground", 0.0, 0.0, 1000.0),
    ("horizontal_above", 300.0, 0.0, 1000.0),
    ("descending_crosses_ground", 100.0, -0.2, 1000.0),
    ("ascending_from_ground", 0.0, 0.5, 2000.0),
    ("ascending_from_below", -100.0, 0.5, 2000.0),
)


def density_integral_above_ground(
    y0: float, dy_ds: float, start: float, end: float,
    falloff: float = FALLOFF, ground: float = GROUND,
) -> float:
    """Integrate exp(-falloff * (y(s) - ground)) over an above-ground interval."""
    length = end - start
    if length <= 0.0:
        return 0.0
    y_start = y0 + dy_ds * start
    if abs(dy_ds) < 1e-12 or abs(falloff) < 1e-12:
        return length * math.exp(-falloff * max(y_start - ground, 0.0))
    return (
        math.exp(-falloff * (y_start - ground))
        * -math.expm1(-falloff * dy_ds * length)
        / (falloff * dy_ds)
    )


def exact_density_integral(
    y0: float, dy_ds: float, distance: float,
    falloff: float = FALLOFF, ground: float = GROUND,
) -> float:
    """Integrate rho(y(s))/RHO0, splitting where the ray crosses ground."""
    if distance <= 0.0:
        return 0.0
    y_end = y0 + dy_ds * distance
    if abs(dy_ds) < 1e-12 or (y0 >= ground and y_end >= ground):
        return density_integral_above_ground(y0, dy_ds, 0.0, distance, falloff, ground)
    if y0 < ground and y_end < ground:
        return distance

    crossing = (ground - y0) / dy_ds
    crossing = min(max(crossing, 0.0), distance)
    if y0 < ground:
        return crossing + density_integral_above_ground(y0, dy_ds, crossing, distance, falloff, ground)
    return density_integral_above_ground(y0, dy_ds, 0.0, crossing, falloff, ground) + (distance - crossing)


def marched_density_integral(
    y0: float, dy_ds: float, distance: float, steps: int,
    falloff: float = FALLOFF, ground: float = GROUND,
) -> float:
    ds = distance / steps
    total = 0.0
    for i in range(steps):
        s = (i + 0.5) * ds
        height = y0 + dy_ds * s
        total += math.exp(-falloff * max(height - ground, 0.0)) * ds
    return total


def rayleigh_phase(cos_theta: float) -> float:
    return 3.0 * (1.0 + cos_theta * cos_theta) / (16.0 * math.pi)


def cpu_atmosphere_fog_source_phase(cos_theta: float) -> float:
    """Mirror AtmosphereFogRenderer::phaseRayleigh from the current C++ source."""
    return 0.75 * (1.0 + cos_theta * cos_theta) * (3.0 / (16.0 * math.pi))


def integrate_phase_sphere(phase_function, samples: int = 262144) -> float:
    step = 2.0 / samples
    total = 0.0
    for index in range(samples):
        cosine = -1.0 + (index + 0.5) * step
        total += phase_function(cosine) * step
    return 2.0 * math.pi * total


def hg_phase(g: float, cos_theta: float) -> float:
    denominator = max(1e-12, 1.0 + g * g - 2.0 * g * cos_theta)
    return (1.0 - g * g) / (4.0 * math.pi * denominator ** 1.5)


def metrics(density_integral: float, cos_theta: float) -> tuple[float, float, float]:
    optical_depth = RHO0 * ABSORPTION * density_integral
    transmittance = math.exp(-optical_depth)
    phase = rayleigh_phase(cos_theta)
    # Constant incident sun along the view ray; sun-path extinction is omitted.
    if ABSORPTION > 1e-12:
        scattered = (
            SCATTERING * phase * SUN_INTENSITY
            * (1.0 - transmittance) / ABSORPTION
        )
    else:
        scattered = SCATTERING * phase * SUN_INTENSITY * RHO0 * density_integral
    return optical_depth, transmittance, scattered


def render_markdown(cos_theta: float, step_counts: list[int]) -> str:
    lines = [
        "# Exponential Height-Fog Integral R&D",
        "",
        f"Model: ground={GROUND:g}, rho0={RHO0:g}, falloff={FALLOFF:g}, "
        f"absorption={ABSORPTION:g}, scattering={SCATTERING:g}, cosTheta={cos_theta:g}.",
        "Scattering uses constant incident sun and omits sun-path extinction; it is a controlled integral check, not full atmospheric lighting.",
        "",
        "| Ray | Steps | Exact optical depth | March optical depth | Max relative transmittance error | Scattering absolute error |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for name, y0, dy_ds, distance in CASES:
        exact_integral = exact_density_integral(y0, dy_ds, distance)
        exact_tau, exact_t, exact_scatter = metrics(exact_integral, cos_theta)
        for steps in step_counts:
            marched_integral = marched_density_integral(y0, dy_ds, distance, steps)
            marched_tau, marched_t, marched_scatter = metrics(marched_integral, cos_theta)
            relative_t_error = abs(marched_t - exact_t) / max(exact_t, 1e-12)
            scatter_error = abs(marched_scatter - exact_scatter)
            lines.append(
                f"| {name} | {steps} | {exact_tau:.9g} | {marched_tau:.9g} | "
                f"{relative_t_error:.6g} | {scatter_error:.6g} |"
            )
    lines.extend(
        [
            "",
            f"Phase samples: Rayleigh={rayleigh_phase(cos_theta):.9g}, "
            f"HG(g=0.6)={hg_phase(0.6, cos_theta):.9g} sr^-1.",
            "",
            "AtmosphereFogRenderer source phase audit (equation mirror; not a C++ runtime call):",
            f"- Standard Rayleigh sphere integral: {integrate_phase_sphere(rayleigh_phase):.12g}.",
            f"- C++ phaseRayleigh sphere integral: {integrate_phase_sphere(cpu_atmosphere_fog_source_phase):.12g}.",
            f"- Source/normalized phase ratio at cosTheta={cos_theta:g}: "
            f"{cpu_atmosphere_fog_source_phase(cos_theta) / rayleigh_phase(cos_theta):.9g}.",
            "- evaluateFog source uses constant sunIntensity and contains no sun-path transmittance evaluation.",
            "",
        ]
    )
    return "\n".join(lines)


def parse_steps(value: str) -> list[int]:
    steps = [int(part.strip()) for part in value.split(",") if part.strip()]
    if not steps or any(step <= 0 for step in steps):
        raise argparse.ArgumentTypeError("steps must be comma-separated positive integers")
    return steps


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cos-theta", type=float, default=0.5)
    parser.add_argument("--steps", type=parse_steps, default=parse_steps("8,16,32,64,128"))
    args = parser.parse_args()
    if not math.isfinite(args.cos_theta) or not -1.0 <= args.cos_theta <= 1.0:
        parser.error("--cos-theta must be finite and in [-1, 1]")
    print(render_markdown(args.cos_theta, args.steps))


if __name__ == "__main__":
    main()
