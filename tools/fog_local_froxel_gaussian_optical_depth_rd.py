#!/usr/bin/env python3
"""Compare froxel point density with analytic Gaussian optical-depth integration."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-gaussian-optical-depth-rd-2026-11-13")
SCENARIOS = {
    "single_baseline": (((0.0, 3.0, 18.0), (2.5, 2.5, 2.0), 0.035),),
    "thin_depth": (((0.0, 3.0, 18.0), (2.5, 2.5, 0.35), 0.035),),
    "two_overlapping": (
        ((-1.0, 3.0, 19.0), (3.0, 2.5, 2.5), 0.025),
        ((1.0, 4.0, 21.0), (2.5, 2.0, 2.0), 0.02),
    ),
}
TILE = 4
SLICES = (32, 64)
XY_RAYS_PER_AXIS = (1, 2, 4)
LIGHT_COUNT = 3
FAR_DISTANCE = 120.0


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def gaussian_line_integral(origin, direction, t0, t1, volume):
    center, sigma, amplitude = volume
    a = sum((direction[i] / sigma[i]) ** 2 for i in range(3))
    if a <= 1e-20:
        value = proxy.density_at(origin, (volume,))
        return value * (t1 - t0)
    b = sum((origin[i] - center[i]) * direction[i] / (sigma[i] ** 2)
            for i in range(3))
    c = sum(((origin[i] - center[i]) / sigma[i]) ** 2 for i in range(3))
    midpoint = -b / a
    minimum_exponent = max(0.0, c - b * b / a)
    scale = math.sqrt(a * 0.5)
    prefactor = amplitude * math.exp(-0.5 * minimum_exponent) * math.sqrt(math.pi / (2.0 * a))
    return prefactor * (math.erf(scale * (t1 - midpoint)) -
                        math.erf(scale * (t0 - midpoint)))


def exact_optical_depth(ray, camera, volumes, max_distance):
    return proxy.EXTINCTION * sum(
        gaussian_line_integral(camera, ray, 0.0, max_distance, volume) for volume in volumes
    )


def build_integrated_grid(volumes, slices, rays_per_axis):
    grid_width = math.ceil(proxy.WIDTH / TILE)
    grid_height = math.ceil(proxy.HEIGHT / TILE)
    edges = proxy.depth_edges(slices, "log")
    values = []
    ray_integrals = 0
    for z in range(slices):
        z0, z1 = edges[z], edges[z + 1]
        for y in range(grid_height):
            for x in range(grid_width):
                density_sum = 0.0
                weighted_light = [0.0, 0.0, 0.0]
                sample_count = rays_per_axis * rays_per_axis
                for sy in range(rays_per_axis):
                    py = y * TILE + ((sy + 0.5) / rays_per_axis) * TILE - 0.5
                    for sx in range(rays_per_axis):
                        px = x * TILE + ((sx + 0.5) / rays_per_axis) * TILE - 0.5
                        ray = proxy.scene.ray_for_pixel(px, py)
                        t0, t1 = z0 / ray[2], z1 / ray[2]
                        path_length = t1 - t0
                        integral = sum(gaussian_line_integral(proxy.CAMERA, ray, t0, t1, volume)
                                       for volume in volumes)
                        density = integral / path_length
                        density_sum += density
                        if integral > 1e-15:
                            # Midpoint source sample weighted by local analytic density.
                            t_mid = 0.5 * (t0 + t1)
                            position = proxy.add(proxy.CAMERA, proxy.mul(ray, t_mid))
                            light = proxy.light_source_at(position, LIGHT_COUNT)
                            for channel in range(3):
                                weighted_light[channel] += density * light[channel]
                        ray_integrals += len(volumes)
                avg_density = density_sum / sample_count
                if density_sum > 0.0:
                    light = tuple(value / density_sum for value in weighted_light)
                else:
                    px = min(proxy.WIDTH - 1, int((x + 0.5) * TILE))
                    py = min(proxy.HEIGHT - 1, int((y + 0.5) * TILE))
                    ray = proxy.scene.ray_for_pixel(px, py)
                    position = proxy.add(proxy.CAMERA,
                                         proxy.mul(ray, math.sqrt(z0 * z1) / ray[2]))
                    light = proxy.light_source_at(position, LIGHT_COUNT)
                values.append((avg_density, light))
    return (grid_width, grid_height, edges, values), ray_integrals


def grid_optical_depth(ray, x, y, grid, slices):
    grid_width, grid_height, edges, _ = grid
    tau = 0.0
    for z in range(slices):
        z0, z1 = edges[z], edges[z + 1]
        sample_z = math.sqrt(z0 * z1)
        density, _ = proxy.sample_froxel(grid, grid_width, grid_height, slices, TILE,
                                         x, y, sample_z, "log")
        tau += proxy.EXTINCTION * density * (z1 - z0) / ray[2]
    return tau


def optical_metrics(reference_tau, candidate_tau, indices):
    errors = [candidate_tau[i] - reference_tau[i] for i in indices]
    reference_t = [math.exp(-reference_tau[i]) for i in indices]
    candidate_t = [math.exp(-candidate_tau[i]) for i in indices]
    t_errors = [candidate_t[j] - reference_t[j] for j in range(len(indices))]
    return {
        "pixel_count": len(indices),
        "optical_depth_rmse": math.sqrt(sum(e * e for e in errors) / max(1, len(errors))),
        "max_absolute_optical_depth_error": max((abs(e) for e in errors), default=0.0),
        "transmittance_rmse": math.sqrt(sum(e * e for e in t_errors) / max(1, len(t_errors))),
        "max_absolute_transmittance_error": max((abs(e) for e in t_errors), default=0.0),
    }


def main() -> None:
    proxy.scene.WIDTH, proxy.scene.HEIGHT = proxy.WIDTH, proxy.HEIGHT
    rays = [proxy.scene.ray_for_pixel(x, y)
            for y in range(proxy.HEIGHT) for x in range(proxy.WIDTH)]
    results = {}
    for scenario, volumes in SCENARIOS.items():
        print(f"Scenario {scenario}...", flush=True)
        exact_tau = [exact_optical_depth(ray, proxy.CAMERA, volumes, FAR_DISTANCE)
                     for ray in rays]
        active_indices = [i for i, tau in enumerate(exact_tau) if tau > 1e-8]
        configs = {}
        for slices in SLICES:
            # Standard center-density grid baseline.
            standard_grid = proxy.build_froxel_grid(TILE, slices, LIGHT_COUNT, "log",
                                                     proxy.CAMERA, volumes)
            standard_tau = [grid_optical_depth(rays[i], i % proxy.WIDTH, i // proxy.WIDTH,
                                               standard_grid, slices)
                            for i in range(len(rays))]
            configs[f"z{slices}_center_sample"] = {
                "froxel_cell_count": standard_grid[0] * standard_grid[1] * slices,
                "all_pixels": optical_metrics(exact_tau, standard_tau, list(range(len(rays)))),
                "volume_affected_pixels": optical_metrics(exact_tau, standard_tau, active_indices),
            }
            for ray_count in XY_RAYS_PER_AXIS:
                name = f"z{slices}_analytic_line_integral_xy{ray_count}x{ray_count}"
                print(" ", name, flush=True)
                grid, integral_evaluations = build_integrated_grid(volumes, slices, ray_count)
                candidate_tau = [grid_optical_depth(rays[i], i % proxy.WIDTH, i // proxy.WIDTH,
                                                    grid, slices)
                                 for i in range(len(rays))]
                configs[name] = {
                    "froxel_cell_count": grid[0] * grid[1] * slices,
                    "xy_rays_per_axis": ray_count,
                    "analytic_gaussian_line_integrals": integral_evaluations,
                    "all_pixels": optical_metrics(exact_tau, candidate_tau, list(range(len(rays)))),
                    "volume_affected_pixels": optical_metrics(exact_tau, candidate_tau, active_indices),
                }
        results[scenario] = {
            "volume_count": len(volumes),
            "volumes": [{"center": list(c), "sigma": list(s), "amplitude": a}
                        for c, s, a in volumes],
            "affected_pixel_count": len(active_indices),
            "configs": configs,
        }

    manifest = {
        "schema": "fog-local-froxel-gaussian-optical-depth-rd-v1",
        "method": "Closed-form Gaussian density line integral along each sampled froxel ray segment; convert integral to average density per froxel depth slice, then compare accumulated optical depth and Beer-Lambert transmittance against closed-form full-ray integral.",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "tile_width": TILE,
            "depth_slices": list(SLICES),
            "depth_distribution": "logarithmic view-Z",
            "xy_rays_per_axis": list(XY_RAYS_PER_AXIS),
            "far_distance": FAR_DISTANCE,
            "scenarios": list(SCENARIOS),
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU runtime or performance measurement.",
            "Optical depth is assessed independently; source lighting/scattering integration is not validated here.",
            "Gaussian line integral is exact in ray parameter, but XY froxel averages use finite stratified rays.",
            "This tests smooth analytic density, not hard volume boundaries or production density textures.",
            "No temporal reprojection, shadows, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {path}")
    for scenario, result in results.items():
        print(scenario)
        for name, row in result["configs"].items():
            print(" ", name, "affected tau RMSE", row["volume_affected_pixels"]["optical_depth_rmse"],
                  "T RMSE", row["volume_affected_pixels"]["transmittance_rmse"])


if __name__ == "__main__":
    main()
