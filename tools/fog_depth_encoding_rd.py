#!/usr/bin/env python3
"""Quantify how fog distance changes under common depth encodings.

This is a CPU contract diagnostic built from the deterministic fog scene. It
does not compile or execute a product shader and does not claim a particular
Artifact/Diligent projection convention.
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from fog_height_image_fixture_rd import HEIGHT, WIDTH, ray_for_pixel, scene_sample


OUTPUT = Path("docs/worklog/artifacts/fog-depth-encoding-rd-2026-11-12/manifest.json")
DENSITIES = (0.002, 0.008, 0.02)
ABSORPTION = 0.7
CAMERA_Y = 4.0
FALLOFF = 0.08
GROUND = 0.0
NEAR_FAR = ((0.1, 120.0), (0.1, 1000.0))


def conventional_perspective_depth(z: float, near: float, far: float) -> float:
    return far / (far - near) - (far * near) / ((far - near) * z)


def summarize(pairs: list[tuple[float, float]]) -> dict[str, float | int]:
    if not pairs:
        return {"samples": 0}
    errors = [decoded - truth for decoded, truth in pairs]
    abs_errors = [abs(value) for value in errors]
    sorted_abs = sorted(abs_errors)
    p95 = sorted_abs[math.ceil(0.95 * len(sorted_abs)) - 1]
    return {
        "samples": len(pairs),
        "decoded_distance_mean": sum(decoded for decoded, _ in pairs) / len(pairs),
        "true_distance_mean": sum(truth for _, truth in pairs) / len(pairs),
        "absolute_error_mean": sum(abs_errors) / len(abs_errors),
        "absolute_error_p95": p95,
        "absolute_error_max": max(abs_errors),
        "signed_error_mean": sum(errors) / len(errors),
    }


def main() -> None:
    samples: list[dict[str, float]] = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = ray_for_pixel(x, y)
            _, distance = scene_sample(ray)
            z = distance * ray[2]
            if z <= 0.0:
                continue
            samples.append({"x": x, "y": y, "ray_y": ray[1], "distance": distance, "view_z": z})

    result: dict[str, object] = {
        "schema": "fog-depth-encoding-rd-v1",
        "scene": {
            "generator": "tools/fog_height_image_fixture_rd.py",
            "size": [WIDTH, HEIGHT],
            "camera_position": [0.0, CAMERA_Y, 0.0],
            "projection": "synthetic perspective rays; view forward is +Z",
            "sample_count": len(samples),
        },
        "scope": "CPU encoding diagnostic only; no product depth texture, shader, or GPU execution",
        "interpretation": "distance decode error and Beer-Lambert transmittance error are isolated from heightFogCS.hlsl's separate optical-depth equation",
        "depth_encodings": {
            "linear_euclidean": "depth=(ray_distance-near)/(far-near); tests an explicit linear range input",
            "linear_view_z": "depth=(view_z-near)/(far-near); conventional linear eye-depth input",
            "perspective_device_z": "far/(far-near) - far*near/((far-near)*view_z); hypothetical forward-Z 0..1 perspective depth",
        },
        "absorption": ABSORPTION,
        "height_medium": {"camera_y": CAMERA_Y, "ground": GROUND, "falloff": FALLOFF},
        "regions": {
            "near_surface_distance_lt_20": "true ray distance < 20 scene units",
            "far_surface_distance_ge_20": "true ray distance >= 20 scene units",
            "near_horizon_abs_ray_y_le_0_15": "abs(ray direction y) <= 0.15",
            "steep_ray_abs_ray_y_gt_0_35": "abs(ray direction y) > 0.35",
        },
        "near_far_sweeps": {},
    }

    sweeps: dict[str, object] = result["near_far_sweeps"]  # type: ignore[assignment]
    for near, far in NEAR_FAR:
        methods: dict[str, dict[str, object]] = {}
        for method in ("linear_euclidean", "linear_view_z", "perspective_device_z"):
            decoded_truth: list[tuple[float, float]] = []
            transmittance_errors: dict[str, list[float]] = {str(density): [] for density in DENSITIES}
            region_pairs: dict[str, list[tuple[float, float]]] = {
                "all": [], "near_surface_distance_lt_20": [], "far_surface_distance_ge_20": [],
                "near_horizon_abs_ray_y_le_0_15": [], "steep_ray_abs_ray_y_gt_0_35": [],
            }
            for sample in samples:
                truth = sample["distance"]
                z = sample["view_z"]
                if method == "linear_euclidean":
                    depth = (truth - near) / (far - near)
                elif method == "linear_view_z":
                    depth = (z - near) / (far - near)
                else:
                    depth = conventional_perspective_depth(z, near, far)
                decoded = near + min(1.0, max(0.0, depth)) * (far - near)
                decoded_truth.append((decoded, truth))
                region_pairs["all"].append((decoded, truth))
                if truth < 20.0:
                    region_pairs["near_surface_distance_lt_20"].append((decoded, truth))
                else:
                    region_pairs["far_surface_distance_ge_20"].append((decoded, truth))
                if abs(sample["ray_y"]) <= 0.15:
                    region_pairs["near_horizon_abs_ray_y_le_0_15"].append((decoded, truth))
                if abs(sample["ray_y"]) > 0.35:
                    region_pairs["steep_ray_abs_ray_y_gt_0_35"].append((decoded, truth))
                for density in DENSITIES:
                    rho = density * math.exp(-max(CAMERA_Y - GROUND, 0.0) * FALLOFF)
                    true_t = math.exp(-rho * ABSORPTION * truth)
                    decoded_t = math.exp(-rho * ABSORPTION * decoded)
                    transmittance_errors[str(density)].append(abs(decoded_t - true_t))
            methods[method] = {
                "distance_error": {name: summarize(values) for name, values in region_pairs.items()},
                "transmittance_absolute_error": {
                    "medium_density": DENSITIES,
                    "mean_by_density": {
                        density: sum(values) / len(values) for density, values in transmittance_errors.items()
                    },
                    "p95_by_density": {
                        density: sorted(values)[math.ceil(0.95 * len(values)) - 1]
                        for density, values in transmittance_errors.items()
                    },
                    "max_by_density": {density: max(values) for density, values in transmittance_errors.items()},
                },
            }
        sweeps[f"near_{near:g}_far_{far:g}"] = {"near": near, "far": far, "methods": methods}

    fixture_manifest = Path("docs/worklog/artifacts/fog-atmosphere-rd-2026-11-12/manifest.json")
    result["source_fixture_manifest"] = {
        "path": fixture_manifest.as_posix(),
        "sha256": hashlib.sha256(fixture_manifest.read_bytes()).hexdigest(),
    }
    result["script_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(OUTPUT.as_posix())
    for sweep_name, sweep in sweeps.items():
        methods = sweep["methods"]
        for method_name, method in methods.items():
            all_error = method["distance_error"]["all"]
            p95_t = method["transmittance_absolute_error"]["p95_by_density"][str(DENSITIES[1])]
            print(f"{sweep_name} {method_name}: distance_abs_p95={all_error['absolute_error_p95']:.6f} "
                  f"distance_abs_max={all_error['absolute_error_max']:.6f} "
                  f"T_abs_p95_rho0.008={p95_t:.6f}")


if __name__ == "__main__":
    main()
