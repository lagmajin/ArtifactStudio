#!/usr/bin/env python3
"""Compare logarithmic and linear froxel depth slices in the local-fog CPU proxy."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-depth-distribution-rd-2026-11-13")
TILE = 4
LIGHT_COUNT = 3
SLICE_COUNTS = (16, 32, 64)
DISTRIBUTIONS = ("log", "linear")
REFERENCE_STEPS = 512
VALIDATION_STEPS = 256


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    proxy.scene.WIDTH, proxy.scene.HEIGHT = proxy.WIDTH, proxy.HEIGHT
    rays, distances, sources = [], [], []
    for y in range(proxy.HEIGHT):
        for x in range(proxy.WIDTH):
            ray = proxy.scene.ray_for_pixel(x, y)
            source_rgb, distance = proxy.scene.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            sources.append(source_rgb)

    print("Building direct references...", flush=True)
    reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                        LIGHT_COUNT)
                 for i in range(len(rays))]
    validation = [proxy.integrate_direct(rays[i], distances[i], sources[i], VALIDATION_STEPS,
                                         LIGHT_COUNT)
                  for i in range(len(rays))]

    results = {}
    for slices in SLICE_COUNTS:
        for distribution in DISTRIBUTIONS:
            name = f"{distribution}_z{slices}"
            print(f"Evaluating {name}...", flush=True)
            grid = proxy.build_froxel_grid(TILE, slices, LIGHT_COUNT, distribution)
            pixels = [
                proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                       i // proxy.WIDTH, TILE, slices, grid, distribution)
                for i in range(len(rays))
            ]
            view_depths = [distances[i] * rays[i][2] for i in range(len(rays))]
            depth_bands = {
                "view_z_lt_20": [i for i, value in enumerate(view_depths) if value < 20.0],
                "view_z_20_to_50": [i for i, value in enumerate(view_depths)
                                    if 20.0 <= value < 50.0],
                "view_z_ge_50": [i for i, value in enumerate(view_depths) if value >= 50.0],
            }
            edges = grid[2]
            widths = [edges[i + 1] - edges[i] for i in range(slices)]
            results[name] = {
                "tile_size": TILE,
                "depth_slices": slices,
                "distribution": distribution,
                "grid_dimensions": [grid[0], grid[1], slices],
                "froxel_cell_count": grid[0] * grid[1] * slices,
                "light_evaluations": grid[0] * grid[1] * slices * LIGHT_COUNT,
                "estimated_bytes_R16F_density_RGBA16F_source": grid[0] * grid[1] * slices * 10,
                "depth_bin_width_view_z": {
                    "first": widths[0],
                    "median": sorted(widths)[len(widths) // 2],
                    "last": widths[-1],
                    "min": min(widths),
                    "max": max(widths),
                },
                "metrics_vs_direct_512": proxy.image_metrics(reference, pixels),
                "view_depth_band_metrics_vs_direct_512": {
                    band: {
                        "pixel_count": len(indices),
                        "metrics": proxy.image_metrics(
                            [reference[i] for i in indices], [pixels[i] for i in indices]
                        ) if indices else None,
                    }
                    for band, indices in depth_bands.items()
                },
            }

    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "fog-local-froxel-depth-distribution-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "tile_size": TILE,
            "light_count": LIGHT_COUNT,
            "near_view_z": proxy.NEAR_VIEW_Z,
            "far_view_z": proxy.FAR_VIEW_Z,
            "depth_distributions": list(DISTRIBUTIONS),
            "slice_counts": list(SLICE_COUNTS),
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "reference_validation_direct_256_vs_512": proxy.image_metrics(reference, validation),
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Linear and logarithmic bins use the same nominal view-Z interval and sample counts.",
            "The analytic AABB shadow and Gaussian volumes do not model production depth discontinuities or shadow filtering.",
            "Memory estimates are arithmetic scenarios, not a production resource layout.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    print("reference validation", manifest["scene"]["reference_validation_direct_256_vs_512"])
    for name, row in results.items():
        print(name, "relative_rgb_l2", row["metrics_vs_direct_512"]["relative_rgb_l2"],
              "depth widths", row["depth_bin_width_view_z"])


if __name__ == "__main__":
    main()
