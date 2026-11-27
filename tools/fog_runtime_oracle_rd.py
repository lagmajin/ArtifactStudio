#!/usr/bin/env python3
"""Independent double-precision oracle for ArtifactCore::RayTrace::AtmosphereFogRenderer.

This script mirrors the runtime equations of
``ArtifactCore/src/Render/AtmosphereFog.cpp`` in Python double precision and
emits a golden fixture that ``tests/ArtifactCore/AtmosphereFogContractTest.cpp``
compares against the shipped C++ runtime.

Scope and limits:

* The oracle is a *behavioural* mirror of the current runtime, not a physical
  fog model. It reproduces the runtime's known approximations on purpose:
  the 0.75x scaled Rayleigh phase term, the per-sample ``exp(-total)``
  attenuation used inside the scattering accumulation, the ``fogBlend * 0.5``
  ambient term, the ``density * heightFalloff`` optical depth used by
  ``applyToPixel()``, and the 8-bit normalized depth handling of
  ``applyToImage()``.
* Because it mirrors the runtime, agreement with this oracle proves the C++
  runtime is stable and matches the recorded contract. It does *not* prove
  physical correctness, GPU parity, or visual quality.
* The oracle intentionally reproduces the source's truncated pi constant
  (``3.14159265``) so the golden values are not biased by a different pi.

Usage::

    python tools/fog_runtime_oracle_rd.py

Outputs:

* ``tests/fixtures/fog-rd/fog_runtime_golden.json`` — golden fixture consumed
  by the C++ contract test.
* ``docs/worklog/artifacts/fog-runtime-oracle-rd-2026-11-27/manifest.json`` —
  capture manifest with input/output hashes and case summary.
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any

PI_SOURCE_CONSTANT = 3.14159265  # Mirrors the float literal used in the C++ source.

REPO_ROOT = Path(__file__).resolve().parents[1]
GOLDEN_PATH = REPO_ROOT / "tests" / "fixtures" / "fog-rd" / "fog_runtime_golden.json"
MANIFEST_PATH = (
    REPO_ROOT
    / "docs"
    / "worklog"
    / "artifacts"
    / "fog-runtime-oracle-rd-2026-11-27"
    / "manifest.json"
)

TOLERANCE_EVALUATE_ABS = 1e-4
TOLERANCE_APPLY_PIXEL_ABS = 1e-5


# ---------------------------------------------------------------------------
# Runtime mirror
# ---------------------------------------------------------------------------


def sanitize_settings(settings: dict[str, Any]) -> dict[str, Any]:
    """Mirror AtmosphereFogRenderer::setSettings() clamping."""

    def clamp_non_negative(value: float) -> float:
        if not math.isfinite(value):
            return 0.0
        return max(0.0, value)

    sanitized = dict(settings)
    sanitized["density"] = clamp_non_negative(settings["density"])
    sanitized["height_falloff"] = clamp_non_negative(settings["height_falloff"])
    sanitized["absorption"] = clamp_non_negative(settings["absorption"])
    sanitized["scattering"] = clamp_non_negative(settings["scattering"])
    sanitized["sun_intensity"] = clamp_non_negative(settings["sun_intensity"])
    sanitized["max_distance"] = clamp_non_negative(settings["max_distance"])
    sanitized["samples"] = min(max(int(settings["samples"]), 1), 4096)
    return sanitized


def height_density(settings: dict[str, Any], height: float) -> float:
    above = max(0.0, height - settings["ground_level"])
    return settings["density"] * math.exp(-above * settings["height_falloff"])


def phase_rayleigh(cos_theta: float) -> float:
    # Source formula: 0.75 * (1 + cos^2) * 3 / (16 * pi). The 0.75 factor is
    # the known deviation from the standard Rayleigh phase function.
    return 0.75 * (1.0 + cos_theta * cos_theta) * (3.0 / (16.0 * PI_SOURCE_CONSTANT))


def lerp_color(a: list[float], b: list[float], t: float) -> list[float]:
    return [a[i] + (b[i] - a[i]) * t for i in range(3)]


def evaluate_fog(
    settings: dict[str, Any],
    origin: list[float],
    direction: list[float],
    t_max: float,
    background: list[float],
) -> list[float]:
    """Mirror AtmosphereFogRenderer::evaluateFog()."""

    settings = sanitize_settings(settings)
    if not settings["enabled"]:
        return list(background)

    clamped_t_max = min(t_max, settings["max_distance"])
    if clamped_t_max <= 0.0:
        return list(background)

    dt = clamped_t_max / float(settings["samples"])

    sun_rad = settings["sun_angle"] * PI_SOURCE_CONSTANT / 180.0
    sun_dir = (math.cos(sun_rad), -math.sin(sun_rad), 0.0)

    total_transmittance = 0.0
    scattered = [0.0, 0.0, 0.0]

    for index in range(settings["samples"]):
        t = (index + 0.5) * dt
        position = [origin[i] + t * direction[i] for i in range(3)]
        density = height_density(settings, position[1])
        if density <= 0.0:
            continue

        cos_theta = -sum(sun_dir[i] * direction[i] for i in range(3))
        phase = phase_rayleigh(cos_theta)

        sample_density = density * dt
        sample_absorption = math.exp(
            -total_transmittance - sample_density * settings["absorption"] * 0.5
        )

        for channel in range(3):
            scattered[channel] += (
                sample_density
                * settings["scattering"]
                * phase
                * settings["fog_color"][channel]
                * sample_absorption
                * settings["sun_intensity"]
            )

        total_transmittance += sample_density * settings["absorption"]

    fog_transmittance = math.exp(-total_transmittance)
    height_ratio = min(max(direction[1] * 0.5 + 0.5, 0.0), 1.0)
    fog_blend = lerp_color(settings["horizon_color"], settings["fog_color"], height_ratio)

    result = []
    for channel in range(3):
        result.append(
            background[channel] * fog_transmittance
            + scattered[channel]
            + fog_blend[channel] * (1.0 - fog_transmittance) * 0.5
        )
    return result


def apply_to_pixel(
    settings: dict[str, Any],
    pixel_color: list[float],
    depth: float,
    near_plane: float,
    far_plane: float,
) -> list[float]:
    """Mirror AtmosphereFogRenderer::applyToPixel()."""

    settings = sanitize_settings(settings)
    if (
        not settings["enabled"]
        or not math.isfinite(depth)
        or not math.isfinite(near_plane)
        or not math.isfinite(far_plane)
        or far_plane <= near_plane
        or depth < near_plane
    ):
        return list(pixel_color)

    clamped_depth = min(max(depth, near_plane), far_plane)
    max_dist = min(settings["max_distance"], far_plane - near_plane)
    t_max = (clamped_depth - near_plane) / (far_plane - near_plane) * max_dist

    # Known runtime approximation: linear optical depth without the
    # exponential height falloff and without a fixed sample count.
    optical_depth = settings["density"] * settings["height_falloff"] * t_max
    transmittance = math.exp(-optical_depth * settings["absorption"])

    fog_blend = lerp_color(settings["horizon_color"], settings["fog_color"], 0.5)

    result = []
    for channel in range(3):
        result.append(pixel_color[channel] * transmittance + fog_blend[channel] * (1.0 - transmittance))
    return result


# ---------------------------------------------------------------------------
# Case definitions
# ---------------------------------------------------------------------------


def settings_variant(
    name: str,
    *,
    density: float = 0.1,
    height_falloff: float = 0.5,
    ground_level: float = 0.0,
    fog_color: tuple[float, float, float] = (0.5, 0.6, 0.7),
    horizon_color: tuple[float, float, float] = (0.67, 0.53, 0.36),
    sun_angle: float = 45.0,
    sun_intensity: float = 1.0,
    scattering: float = 0.3,
    absorption: float = 0.1,
    samples: int = 64,
    max_distance: float = 100.0,
    enabled: bool = True,
) -> dict[str, Any]:
    return {
        "name": name,
        "density": density,
        "height_falloff": height_falloff,
        "ground_level": ground_level,
        "fog_color": list(fog_color),
        "horizon_color": list(horizon_color),
        "sun_angle": sun_angle,
        "sun_intensity": sun_intensity,
        "scattering": scattering,
        "absorption": absorption,
        "samples": samples,
        "max_distance": max_distance,
        "enabled": enabled,
    }


def normalize(vector: tuple[float, float, float]) -> list[float]:
    length = math.sqrt(sum(component * component for component in vector))
    if length <= 0.0:
        return [0.0, 0.0, 0.0]
    return [component / length for component in vector]


DEFAULT_SETTINGS = settings_variant("default")
DENSE_LOW_SUN = settings_variant(
    "dense_low_sun",
    density=0.05,
    height_falloff=0.02,
    sun_angle=10.0,
    scattering=0.8,
    absorption=0.5,
    samples=128,
    max_distance=200.0,
)
HIGH_ALTITUDE = settings_variant(
    "high_altitude",
    density=0.02,
    height_falloff=0.2,
    ground_level=5.0,
    samples=256,
    max_distance=500.0,
)
NO_SCATTERING = settings_variant(
    "no_scattering",
    density=0.08,
    height_falloff=0.1,
    scattering=0.0,
    absorption=0.2,
)
HIGH_SUN = settings_variant(
    "high_sun",
    density=0.06,
    height_falloff=0.05,
    sun_angle=80.0,
    samples=96,
)
SHORT_MAX_DISTANCE = settings_variant(
    "short_max_distance",
    density=0.1,
    max_distance=25.0,
)
NEGATIVE_DENSITY = settings_variant(
    "negative_density_clamped_to_zero",
    density=-0.5,
    height_falloff=0.5,
)
DISABLED = settings_variant("disabled", enabled=False)
SINGLE_SAMPLE = settings_variant("single_sample", samples=1, density=0.09)

RAYS: dict[str, dict[str, list[float]]] = {
    "horizontal": {"origin": [0.0, 2.0, 0.0], "direction": [1.0, 0.0, 0.0]},
    "upward": {"origin": [0.0, 2.0, 0.0], "direction": [0.0, 1.0, 0.0]},
    "downward": {"origin": [0.0, 10.0, 0.0], "direction": [0.0, -1.0, 0.0]},
    "tilted": {"origin": [0.0, 1.0, 0.0], "direction": normalize((1.0, 0.3, -0.4))},
    "below_ground": {"origin": [0.0, -2.0, 0.0], "direction": normalize((0.5, 0.2, 0.5))},
}

BACKGROUNDS: dict[str, list[float]] = {
    "dark": [0.1, 0.1, 0.12],
    "white": [1.0, 1.0, 1.0],
    "warm": [0.8, 0.2, 0.1],
}

EVALUATE_PLAN: list[tuple[str, dict[str, Any], str, float, str]] = [
    ("default_horizontal_short", DEFAULT_SETTINGS, "horizontal", 5.0, "dark"),
    ("default_horizontal_long", DEFAULT_SETTINGS, "horizontal", 30.0, "dark"),
    ("default_upward", DEFAULT_SETTINGS, "upward", 30.0, "white"),
    ("default_downward", DEFAULT_SETTINGS, "downward", 30.0, "white"),
    ("default_tilted", DEFAULT_SETTINGS, "tilted", 20.0, "warm"),
    ("default_below_ground", DEFAULT_SETTINGS, "below_ground", 12.0, "dark"),
    ("dense_low_sun_horizontal", DENSE_LOW_SUN, "horizontal", 150.0, "dark"),
    ("dense_low_sun_upward", DENSE_LOW_SUN, "upward", 80.0, "warm"),
    ("dense_low_sun_below_ground", DENSE_LOW_SUN, "below_ground", 40.0, "dark"),
    ("high_altitude_horizontal", HIGH_ALTITUDE, "horizontal", 120.0, "white"),
    ("high_altitude_upward", HIGH_ALTITUDE, "upward", 200.0, "dark"),
    ("high_altitude_downward", HIGH_ALTITUDE, "downward", 60.0, "dark"),
    ("no_scattering_horizontal", NO_SCATTERING, "horizontal", 40.0, "dark"),
    ("no_scattering_downward", NO_SCATTERING, "downward", 25.0, "warm"),
    ("high_sun_tilted", HIGH_SUN, "tilted", 35.0, "white"),
    ("high_sun_horizontal", HIGH_SUN, "horizontal", 90.0, "dark"),
    ("short_max_distance_clamped", SHORT_MAX_DISTANCE, "horizontal", 120.0, "dark"),
    ("short_max_distance_exact", SHORT_MAX_DISTANCE, "horizontal", 25.0, "dark"),
    ("negative_density_is_transparent", NEGATIVE_DENSITY, "horizontal", 40.0, "dark"),
    ("disabled_returns_background", DISABLED, "horizontal", 40.0, "warm"),
    ("single_sample", SINGLE_SAMPLE, "horizontal", 10.0, "dark"),
]

APPLY_PLAN: list[tuple[str, dict[str, Any], str, float, float, float]] = [
    ("default_near", DEFAULT_SETTINGS, "dark", 0.25, 0.1, 100.0),
    ("default_mid", DEFAULT_SETTINGS, "white", 25.0, 0.1, 100.0),
    ("default_far", DEFAULT_SETTINGS, "warm", 75.0, 0.1, 100.0),
    ("default_beyond_far_clamped", DEFAULT_SETTINGS, "dark", 250.0, 0.1, 100.0),
    ("default_before_near_passthrough", DEFAULT_SETTINGS, "white", 0.05, 0.1, 100.0),
    ("dense_low_sun_mid", DENSE_LOW_SUN, "dark", 50.0, 0.1, 200.0),
    ("no_scattering_mid", NO_SCATTERING, "warm", 30.0, 0.1, 100.0),
    ("negative_density_is_transparent", NEGATIVE_DENSITY, "dark", 40.0, 0.1, 100.0),
    ("disabled_returns_pixel", DISABLED, "warm", 40.0, 0.1, 100.0),
    ("short_max_distance", SHORT_MAX_DISTANCE, "white", 20.0, 0.1, 100.0),
    ("high_altitude_mid", HIGH_ALTITUDE, "dark", 60.0, 0.1, 500.0),
    ("degenerate_far_plane_passthrough", DEFAULT_SETTINGS, "white", 5.0, 10.0, 10.0),
]

PIXEL_COLORS: dict[str, list[float]] = {
    "dark": [0.1, 0.1, 0.12],
    "white": [1.0, 1.0, 1.0],
    "warm": [0.8, 0.2, 0.1],
}


def build_fixture() -> dict[str, Any]:
    evaluate_cases: list[dict[str, Any]] = []
    for name, settings, ray_key, t_max, background_key in EVALUATE_PLAN:
        ray = RAYS[ray_key]
        expected = evaluate_fog(
            settings, ray["origin"], ray["direction"], t_max, BACKGROUNDS[background_key]
        )
        evaluate_cases.append(
            {
                "name": name,
                "settings": settings,
                "ray": {"origin": ray["origin"], "direction": ray["direction"]},
                "t_max": t_max,
                "background": BACKGROUNDS[background_key],
                "expected_rgb": expected,
                "tolerance_abs": TOLERANCE_EVALUATE_ABS,
            }
        )

    apply_cases: list[dict[str, Any]] = []
    for name, settings, pixel_key, depth, near_plane, far_plane in APPLY_PLAN:
        expected = apply_to_pixel(
            settings, PIXEL_COLORS[pixel_key], depth, near_plane, far_plane
        )
        apply_cases.append(
            {
                "name": name,
                "settings": settings,
                "pixel_color": PIXEL_COLORS[pixel_key],
                "depth": depth,
                "near_plane": near_plane,
                "far_plane": far_plane,
                "expected_rgb": expected,
                "tolerance_abs": TOLERANCE_APPLY_PIXEL_ABS,
            }
        )

    return {
        "schema": "artifact.fog.runtime.golden/1",
        "generator": "tools/fog_runtime_oracle_rd.py",
        "source_reference": "ArtifactCore/src/Render/AtmosphereFog.cpp",
        "description": (
            "Behavioural mirror of the shipped AtmosphereFogRenderer runtime in "
            "double precision. Agreement proves runtime stability against the "
            "recorded contract, not physical correctness or GPU parity."
        ),
        "evaluate_fog_cases": evaluate_cases,
        "apply_to_pixel_cases": apply_cases,
    }


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    fixture = build_fixture()

    GOLDEN_PATH.parent.mkdir(parents=True, exist_ok=True)
    GOLDEN_PATH.write_text(
        json.dumps(fixture, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    script_path = Path(__file__).resolve()
    manifest = {
        "artifact": "fog-runtime-oracle-rd-2026-11-27",
        "generated_by": "tools/fog_runtime_oracle_rd.py",
        "script_hash_sha256": sha256_of(script_path),
        "golden_path": str(GOLDEN_PATH.relative_to(REPO_ROOT)).replace("\\", "/"),
        "golden_hash_sha256": sha256_of(GOLDEN_PATH),
        "source_reference": "ArtifactCore/src/Render/AtmosphereFog.cpp",
        "evaluate_fog_case_count": len(fixture["evaluate_fog_cases"]),
        "apply_to_pixel_case_count": len(fixture["apply_to_pixel_cases"]),
        "tolerance_abs": {
            "evaluate_fog": TOLERANCE_EVALUATE_ABS,
            "apply_to_pixel": TOLERANCE_APPLY_PIXEL_ABS,
        },
        "scope": (
            "CPU double-precision behavioural mirror of the runtime equations. "
            "No GPU capture, no physical validation, no visual acceptance."
        ),
    }

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )

    print(f"golden  -> {GOLDEN_PATH}")
    print(f"manifest-> {MANIFEST_PATH}")
    print(
        f"cases   -> evaluate_fog={len(fixture['evaluate_fog_cases'])}, "
        f"apply_to_pixel={len(fixture['apply_to_pixel_cases'])}"
    )


if __name__ == "__main__":
    main()
