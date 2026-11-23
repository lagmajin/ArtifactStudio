#!/usr/bin/env python3
"""Sweep froxel XY tile width and local-volume spatial scale/phase."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-spatial-resolution-rd-2026-11-13")
TILE_WIDTHS = (1, 2, 4, 8, 16)
SLICES = 32
LIGHT_COUNT = 3
REFERENCE_STEPS = 512
VALIDATION_STEPS = 256
VOLUME_SCENARIOS = {
    "baseline": (0.0, 1.0),
    "fine_half_sigma": (0.0, 0.5),
    "x_shift_half_unit": (0.5, 1.0),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def scenario_volumes(x_shift, sigma_scale):
    result = []
    for center, sigma, amplitude in proxy.VOLUMES:
        shifted_center = (center[0] + x_shift, center[1], center[2])
        scaled_sigma = tuple(component * sigma_scale for component in sigma)
        result.append((shifted_center, scaled_sigma, amplitude))
    return tuple(result)


def main() -> None:
    proxy.scene.WIDTH, proxy.scene.HEIGHT = proxy.WIDTH, proxy.HEIGHT
    rays, distances, sources = [], [], []
    for y in range(proxy.HEIGHT):
        for x in range(proxy.WIDTH):
            ray = proxy.scene.ray_for_pixel(x, y)
            source, distance = proxy.scene.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            sources.append(source)

    results = {}
    for scenario, (x_shift, sigma_scale) in VOLUME_SCENARIOS.items():
        volumes = scenario_volumes(x_shift, sigma_scale)
        print(f"Reference for {scenario}...", flush=True)
        reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                            LIGHT_COUNT, proxy.CAMERA, volumes)
                     for i in range(len(rays))]
        fog_reference = [proxy.integrate_direct(rays[i], distances[i], (0.0, 0.0, 0.0),
                                                REFERENCE_STEPS, LIGHT_COUNT, proxy.CAMERA, volumes)
                         for i in range(len(rays))]
        validation = [proxy.integrate_direct(rays[i], distances[i], sources[i], VALIDATION_STEPS,
                                             LIGHT_COUNT, proxy.CAMERA, volumes)
                      for i in range(len(rays))]
        tile_rows = {}
        for tile in TILE_WIDTHS:
            print(f"  {scenario}: tile {tile}", flush=True)
            grid = proxy.build_froxel_grid(tile, SLICES, LIGHT_COUNT, "log", proxy.CAMERA, volumes)
            pixels = [
                proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                       i // proxy.WIDTH, tile, SLICES, grid, "log")
                for i in range(len(rays))
            ]
            fog_pixels = [
                proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                       i // proxy.WIDTH, tile, SLICES, grid, "log")
                for i in range(len(rays))
            ]
            cells = grid[0] * grid[1] * SLICES
            tile_rows[str(tile)] = {
                "tile_width_pixels": tile,
                "grid_dimensions": [grid[0], grid[1], SLICES],
                "froxel_cell_count": cells,
                "light_evaluations": cells * LIGHT_COUNT,
                "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
                "metrics_vs_direct_512": proxy.image_metrics(reference, pixels),
                "fog_scattering_only_metrics_vs_direct_512": proxy.image_metrics(fog_reference,
                                                                                 fog_pixels),
            }
        results[scenario] = {
            "volume_x_shift": x_shift,
            "volume_sigma_scale": sigma_scale,
            "reference_validation_direct_256_vs_512": proxy.image_metrics(reference, validation),
            "fog_scattering_reference_energy_rgb_l2": (
                sum(value * value for pixel in fog_reference for value in pixel) ** 0.5
            ),
            "tile_width_results": tile_rows,
        }

    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "fog-local-froxel-spatial-resolution-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "slice_distribution": "logarithmic view-Z",
            "depth_slices": SLICES,
            "light_count": LIGHT_COUNT,
            "tile_widths_pixels": list(TILE_WIDTHS),
            "volume_scenarios": {
                name: {"x_shift": settings[0], "sigma_scale": settings[1]}
                for name, settings in VOLUME_SCENARIOS.items()
            },
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Volume scenarios change Gaussian widths/positions but not production density field or volume boundary representation.",
            "All volumes are smooth Gaussians; hard-edged volume boundaries and depth discontinuities are not modeled.",
            "The X shift is a world-space stress perturbation, not an exact screen-space half-pixel phase shift.",
            "Memory estimates are arithmetic scenarios, not a production resource layout.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for scenario, row in results.items():
        print(scenario, "ref validation", row["reference_validation_direct_256_vs_512"]["relative_rgb_l2"])
        for tile, tile_row in row["tile_width_results"].items():
            print(" ", tile, tile_row["metrics_vs_direct_512"]["relative_rgb_l2"],
                  "fog-only", tile_row["fog_scattering_only_metrics_vs_direct_512"]["relative_rgb_l2"],
                  "cells", tile_row["froxel_cell_count"])


if __name__ == "__main__":
    main()
