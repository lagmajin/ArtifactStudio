#!/usr/bin/env python3
"""Compare local-light height fog raymarching with a CPU froxel-grid proxy."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_height_image_fixture_rd as scene


WIDTH = 96
HEIGHT = 54
OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-proxy-rd-2026-11-13")
CAMERA = scene.CAMERA
NEAR_VIEW_Z = 0.5
FAR_VIEW_Z = 120.0
REFERENCE_STEPS = 512
VALIDATION_STEPS = 256
EXTINCTION = 1.0
SCATTERING_ALBEDO = 0.9
ISOTROPIC_PHASE = 1.0 / (4.0 * math.pi)

VOLUMES = (
    ((-2.5, 3.0, 16.0), (3.5, 2.5, 5.0), 0.035),
    ((4.0, 3.0, 32.0), (5.0, 4.0, 8.0), 0.025),
    ((0.0, 8.0, 52.0), (7.0, 6.0, 12.0), 0.015),
)
LIGHTS = (
    ((-5.0, 9.0, 6.0), (1.0, 0.68, 0.42), 520.0),
    ((9.0, 5.5, 30.0), (0.34, 0.56, 1.0), 780.0),
    ((0.0, 15.0, 55.0), (0.52, 1.0, 0.66), 1_050.0),
)
OCCLUDERS = (
    ((-1.2, 0.0, 18.0), (1.2, 7.5, 20.0)),
    ((5.0, 0.0, 34.0), (7.5, 9.0, 36.5)),
)
FROXEL_CONFIGS = ((2, 64), (4, 64), (8, 64), (4, 32))
DIFF_DISPLAY_GAIN = 24.0


def add(a, b):
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def sub(a, b):
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def mul(a, scale):
    return a[0] * scale, a[1] * scale, a[2] * scale


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def length_sq(a):
    return dot(a, a)


def segment_hits_box(origin, endpoint, box) -> bool:
    direction = sub(endpoint, origin)
    t_min, t_max = 0.0, 1.0
    for axis in range(3):
        lo, hi = box[0][axis], box[1][axis]
        if abs(direction[axis]) < 1e-12:
            if origin[axis] < lo or origin[axis] > hi:
                return False
            continue
        inverse = 1.0 / direction[axis]
        a = (lo - origin[axis]) * inverse
        b = (hi - origin[axis]) * inverse
        if a > b:
            a, b = b, a
        t_min = max(t_min, a)
        t_max = min(t_max, b)
        if t_min > t_max:
            return False
    return t_max > 1e-6 and t_min < 1.0 - 1e-6


def density_at(position, volumes=VOLUMES) -> float:
    total = 0.0
    for center, sigma, amplitude in volumes:
        exponent = sum(((position[axis] - center[axis]) / sigma[axis]) ** 2
                       for axis in range(3))
        total += amplitude * math.exp(-0.5 * exponent)
    return total


def light_source_at(position, light_count=len(LIGHTS)):
    total = [0.0, 0.0, 0.0]
    for light_position, color, intensity in LIGHTS[:light_count]:
        delta = sub(light_position, position)
        distance_sq = max(length_sq(delta), 1.0)
        visibility = 1.0
        for box in OCCLUDERS:
            if segment_hits_box(position, light_position, box):
                visibility = 0.0
                break
        scale = visibility * intensity / (distance_sq + 16.0) * ISOTROPIC_PHASE
        for channel in range(3):
            total[channel] += color[channel] * scale
    return tuple(total)


def field_at(position, light_count=len(LIGHTS), volumes=VOLUMES, density_fn=None):
    density = density_fn(position) if density_fn is not None else density_at(position, volumes)
    return density, light_source_at(position, light_count)


def integrate_direct(ray, distance, source_rgb, steps, light_count=len(LIGHTS), camera=CAMERA,
                     volumes=VOLUMES, density_fn=None):
    if ray[2] <= 1e-8 or distance <= 0.0:
        return source_rgb
    ds = distance / steps
    transmittance = 1.0
    scattered = [0.0, 0.0, 0.0]
    for index in range(steps):
        position = add(camera, mul(ray, (index + 0.5) * ds))
        density, light_source = field_at(position, light_count, volumes, density_fn)
        segment_t = math.exp(-EXTINCTION * density * ds)
        weight = transmittance * (1.0 - segment_t) * SCATTERING_ALBEDO
        for channel in range(3):
            scattered[channel] += weight * light_source[channel]
        transmittance *= segment_t
    return tuple(source_rgb[channel] * transmittance + scattered[channel] for channel in range(3))


def depth_edges(slices: int, distribution: str = "log") -> list[float]:
    if distribution == "linear":
        return [NEAR_VIEW_Z + (FAR_VIEW_Z - NEAR_VIEW_Z) * index / slices
                for index in range(slices + 1)]
    if distribution != "log":
        raise ValueError(f"Unknown depth distribution: {distribution}")
    ratio = FAR_VIEW_Z / NEAR_VIEW_Z
    return [NEAR_VIEW_Z * ratio ** (index / slices) for index in range(slices + 1)]


def build_froxel_grid(tile: int, slices: int, light_count=len(LIGHTS),
                      distribution: str = "log", camera=CAMERA, volumes=VOLUMES,
                      density_fn=None, spatial_samples: int = 1):
    grid_width = math.ceil(WIDTH / tile)
    grid_height = math.ceil(HEIGHT / tile)
    edges = depth_edges(slices, distribution)
    values = []
    for z in range(slices):
        view_z = math.sqrt(edges[z] * edges[z + 1])
        for y in range(grid_height):
            py = min(HEIGHT - 1, int((y + 0.5) * tile))
            for x in range(grid_width):
                px = min(WIDTH - 1, int((x + 0.5) * tile))
                if spatial_samples <= 1:
                    ray = scene.ray_for_pixel(px, py)
                    if ray[2] <= 1e-8:
                        values.append((0.0, (0.0, 0.0, 0.0)))
                        continue
                    position = add(camera, mul(ray, view_z / ray[2]))
                    values.append(field_at(position, light_count, volumes, density_fn))
                    continue
                density_sum = 0.0
                light_sum = [0.0, 0.0, 0.0]
                sample_count = spatial_samples * spatial_samples
                for sy in range(spatial_samples):
                    sub_y = y * tile + ((sy + 0.5) / spatial_samples) * tile - 0.5
                    for sx in range(spatial_samples):
                        sub_x = x * tile + ((sx + 0.5) / spatial_samples) * tile - 0.5
                        ray = scene.ray_for_pixel(sub_x, sub_y)
                        if ray[2] <= 1e-8:
                            continue
                        position = add(camera, mul(ray, view_z / ray[2]))
                        density, light = field_at(position, light_count, volumes, density_fn)
                        density_sum += density
                        for channel in range(3):
                            light_sum[channel] += light[channel]
                values.append((density_sum / sample_count,
                               tuple(value / sample_count for value in light_sum)))
    return grid_width, grid_height, edges, values


def mix(a, b, t):
    return a + (b - a) * t


def axis_weights(position, count):
    position = min(max(position, 0.0), count - 1.0)
    lower = int(math.floor(position))
    upper = min(lower + 1, count - 1)
    return lower, upper, position - lower


def sample_froxel(grid, grid_width, grid_height, slices, tile, x, y, view_z,
                  distribution: str = "log"):
    # Froxel values are stored at tile-center and selected depth-center locations.
    gx = (x + 0.5) / tile - 0.5
    gy = (y + 0.5) / tile - 0.5
    edges = grid[2]
    clamped_view_z = min(max(view_z, NEAR_VIEW_Z), FAR_VIEW_Z)
    if distribution == "linear":
        depth_coord = (clamped_view_z - NEAR_VIEW_Z) / (FAR_VIEW_Z - NEAR_VIEW_Z)
    elif distribution == "log":
        depth_coord = math.log(clamped_view_z / NEAR_VIEW_Z) / math.log(FAR_VIEW_Z / NEAR_VIEW_Z)
    else:
        raise ValueError(f"Unknown depth distribution: {distribution}")
    gz = depth_coord * slices - 0.5
    x0, x1, tx = axis_weights(gx, grid_width)
    y0, y1, ty = axis_weights(gy, grid_height)
    z0, z1, tz = axis_weights(gz, slices)
    density = 0.0
    light = [0.0, 0.0, 0.0]
    for iz, wz in ((z0, 1.0 - tz), (z1, tz)):
        for iy, wy in ((y0, 1.0 - ty), (y1, ty)):
            for ix, wx in ((x0, 1.0 - tx), (x1, tx)):
                weight = wx * wy * wz
                value = grid[3][(iz * grid_height + iy) * grid_width + ix]
                density += value[0] * weight
                for channel in range(3):
                    light[channel] += value[1][channel] * weight
    return density, tuple(light)


def integrate_froxel(ray, distance, source_rgb, x, y, tile, slices, grid,
                     distribution: str = "log", substeps_per_slice: int = 1):
    if ray[2] <= 1e-8 or distance <= 0.0:
        return source_rgb
    grid_width, grid_height, edges, _ = grid
    maximum_view_z = min(FAR_VIEW_Z, distance * ray[2])
    transmittance = 1.0
    scattered = [0.0, 0.0, 0.0]
    for index in range(slices):
        slice_z0 = edges[index]
        slice_z1 = min(edges[index + 1], maximum_view_z)
        if slice_z1 <= slice_z0:
            continue
        dz = (slice_z1 - slice_z0) / substeps_per_slice
        for substep in range(substeps_per_slice):
            z0 = slice_z0 + substep * dz
            z1 = slice_z0 + (substep + 1) * dz
            sample_z = (math.sqrt(z0 * z1) if substeps_per_slice == 1
                        else 0.5 * (z0 + z1))
            ds = (z1 - z0) / ray[2]
            density, light = sample_froxel(grid, grid_width, grid_height, slices, tile, x, y,
                                           sample_z, distribution)
            segment_t = math.exp(-EXTINCTION * density * ds)
            weight = transmittance * (1.0 - segment_t) * SCATTERING_ALBEDO
            for channel in range(3):
                scattered[channel] += weight * light[channel]
            transmittance *= segment_t
        if slice_z1 >= maximum_view_z:
            break
    return tuple(source_rgb[channel] * transmittance + scattered[channel] for channel in range(3))


def image_metrics(reference, candidate):
    deltas = [candidate[i][c] - reference[i][c]
              for i in range(len(reference)) for c in range(3)]
    reference_values = [value for pixel in reference for value in pixel]
    return {
        "rgb_rmse": math.sqrt(sum(value * value for value in deltas) / max(1, len(deltas))),
        "max_absolute_channel_error": max((abs(value) for value in deltas), default=0.0),
        "relative_rgb_l2": math.sqrt(sum(value * value for value in deltas) /
                                      max(sum(value * value for value in reference_values), 1e-30)),
        "pixel_count": len(reference),
    }


def shadow_influence_flags(rays, distances, light_count=len(LIGHTS)):
    flags = []
    probe_steps = 64
    for ray, distance in zip(rays, distances):
        affected = False
        if ray[2] > 1e-8 and distance > 0.0:
            ds = distance / probe_steps
            for index in range(probe_steps):
                position = add(CAMERA, mul(ray, (index + 0.5) * ds))
                if density_at(position) < 0.003:
                    continue
                if any(segment_hits_box(position, light[0], box)
                       for light in LIGHTS[:light_count] for box in OCCLUDERS):
                    affected = True
                    break
        flags.append(affected)
    return flags


def region_masks(rays, distances, sources, shadow_flags):
    masks = {
        "near_surface_distance_lt_20": [],
        "far_surface_distance_ge_30": [],
        "near_horizon_abs_ray_y_le_0_15": [],
        "steep_ray_abs_ray_y_gt_0_35": [],
        "hdr_source_any_channel_gt_1": [],
        "ray_intersects_shadowed_dense_fog_probe": [],
    }
    for index, (ray, distance, source, shadowed) in enumerate(zip(rays, distances, sources, shadow_flags)):
        if distance < 20.0:
            masks["near_surface_distance_lt_20"].append(index)
        if distance >= 30.0:
            masks["far_surface_distance_ge_30"].append(index)
        if abs(ray[1]) <= 0.15:
            masks["near_horizon_abs_ray_y_le_0_15"].append(index)
        if abs(ray[1]) > 0.35:
            masks["steep_ray_abs_ray_y_gt_0_35"].append(index)
        if max(source) > 1.0:
            masks["hdr_source_any_channel_gt_1"].append(index)
        if shadowed:
            masks["ray_intersects_shadowed_dense_fog_probe"].append(index)
    return masks


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    rays, distances, sources = [], [], []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = scene.ray_for_pixel(x, y)
            source_rgb, distance = scene.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            sources.append(source_rgb)

    reference = [integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS)
                 for i in range(WIDTH * HEIGHT)]
    validation = [integrate_direct(rays[i], distances[i], sources[i], VALIDATION_STEPS)
                  for i in range(WIDTH * HEIGHT)]
    captures = {"direct_32": [integrate_direct(rays[i], distances[i], sources[i], 32)
                              for i in range(WIDTH * HEIGHT)],
                "direct_64": [integrate_direct(rays[i], distances[i], sources[i], 64)
                              for i in range(WIDTH * HEIGHT)]}

    grids = {}
    config_rows = []
    for tile, slices in FROXEL_CONFIGS:
        key = f"tile{tile}_z{slices}"
        grid = build_froxel_grid(tile, slices)
        grids[key] = grid
        captures[f"froxel_{key}"] = [
            integrate_froxel(rays[i], distances[i], sources[i], i % WIDTH, i // WIDTH,
                             tile, slices, grid)
            for i in range(WIDTH * HEIGHT)
        ]
        cells = grid[0] * grid[1] * slices
        config_rows.append({
            "name": key,
            "tile_size": tile,
            "depth_slices": slices,
            "grid_dimensions": [grid[0], grid[1], slices],
            "froxel_cell_count": cells,
            "illustrative_bytes_per_cell": {
                "one_RGBA16F": cells * 8,
                "density_R16F_plus_source_RGBA16F": cells * 10,
                "previous_plus_history_RGBA16F": cells * 18,
                "three_RGBA16F_intermediates": cells * 24,
            },
            "injection_light_evaluations_for_three_lights": cells * len(LIGHTS),
            "pixel_slice_integrations": WIDTH * HEIGHT * slices,
            "trilinear_froxel_cell_reads_upper_bound": WIDTH * HEIGHT * slices * 8,
        })

    OUTPUT.mkdir(parents=True, exist_ok=True)
    captures = {"reference_direct_512": reference,
                "validation_direct_256": validation, **captures}
    capture_files = {}
    for name, pixels in captures.items():
        pfm = OUTPUT / f"{name}.pfm"
        png = OUTPUT / f"{name}_preview.png"
        scene.pack_pfm(pfm, pixels)
        scene.write_preview_png(png, pixels)
        capture_files[name] = {
            "pfm": pfm.name,
            "pfm_sha256": sha256(pfm),
            "preview_png": png.name,
            "preview_sha256": sha256(png),
        }

    metrics = {name: image_metrics(reference, pixels) for name, pixels in captures.items()
               if name != "reference_direct_512"}
    shadow_flags = shadow_influence_flags(rays, distances)
    masks = region_masks(rays, distances, sources, shadow_flags)
    region_metrics = {
        name: {
            region: image_metrics([reference[i] for i in indices],
                                  [pixels[i] for i in indices]) if indices else None
            for region, indices in masks.items()
        }
        for name, pixels in captures.items() if name != "reference_direct_512"
    }
    difference_files = {}
    for name, pixels in captures.items():
        if name == "reference_direct_512":
            continue
        difference = [tuple(abs(pixels[i][c] - reference[i][c]) for c in range(3))
                      for i in range(len(reference))]
        display_difference = [tuple(value * DIFF_DISPLAY_GAIN for value in pixel)
                              for pixel in difference]
        pfm = OUTPUT / f"{name}_diff_raw.pfm"
        png = OUTPUT / f"{name}_diff_gain{int(DIFF_DISPLAY_GAIN)}_preview.png"
        scene.pack_pfm(pfm, difference)
        scene.write_preview_png(png, display_difference)
        difference_files[name] = {
            "pfm": pfm.name,
            "pfm_sha256": sha256(pfm),
            "preview_png": png.name,
            "preview_sha256": sha256(png),
            "display_gain": DIFF_DISPLAY_GAIN,
        }
    reference_validation = image_metrics(reference, validation)
    reference_script = Path("tools/fog_height_image_fixture_rd.py")
    manifest = {
        "schema": "fog-local-froxel-proxy-rd-v1",
        "scene": {
            "size": [WIDTH, HEIGHT],
            "camera": list(CAMERA),
            "depth": "synthetic Euclidean ray distance; not a product depth buffer",
            "density_volumes": [{"center": list(c), "sigma": list(s), "amplitude": a}
                                for c, s, a in VOLUMES],
            "point_lights": [{"position": list(p), "color": list(c), "intensity": i}
                             for p, c, i in LIGHTS],
            "occluder_aabbs": [{"minimum": list(a), "maximum": list(b)} for a, b in OCCLUDERS],
        },
        "model": {
            "extinction": EXTINCTION,
            "scattering_albedo": SCATTERING_ALBEDO,
            "phase": "isotropic 1/(4*pi)",
            "reference_direct_steps": REFERENCE_STEPS,
            "validation_direct_steps": VALIDATION_STEPS,
            "reference_validation": reference_validation,
            "froxel_configurations": config_rows,
            "shadow_region_mask": {
                "definition": "64 midpoint probes along pixel ray; density >= 0.003 and at least one point-light segment intersects an analytic AABB",
                "pixel_count": sum(shadow_flags),
            },
            "direct_light_evaluations_three_lights": {
                "steps_32": WIDTH * HEIGHT * 32 * len(LIGHTS),
                "steps_64": WIDTH * HEIGHT * 64 * len(LIGHTS),
                "steps_512_reference": WIDTH * HEIGHT * REFERENCE_STEPS * len(LIGHTS),
            },
        },
        "metrics_vs_reference_direct_512": metrics,
        "region_metrics_vs_reference_direct_512": {
            name: {region: {"pixel_count": len(masks[region]), "metrics": value}
                   for region, value in regions.items()}
            for name, regions in region_metrics.items()
        },
        "captures": capture_files,
        "difference_captures": difference_files,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Hard AABB visibility, isotropic phase, analytic gaussian density, and fixed three point lights only.",
            "Froxel storage estimates are arithmetic scenarios, not a proposed production resource layout.",
            "The 512-step direct raymarch is a high-step reference; validation compares 256 to 512.",
            "No temporal reprojection, anisotropic scattering, shadow-map filtering, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "scene_fixture_sha256": sha256(reference_script),
    }
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                            encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT / 'manifest.json'}")
    print("reference validation:", reference_validation)
    for name, result in metrics.items():
        print(name, result)
    for row in config_rows:
        print("cost proxy", row["name"], "cells", row["froxel_cell_count"],
              "three-light injection evaluations", row["injection_light_evaluations_for_three_lights"])


if __name__ == "__main__":
    main()
