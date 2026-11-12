#!/usr/bin/env python3
"""Render height-fog camera-height comparisons on a deterministic world scene."""

from __future__ import annotations

import hashlib
import json
import math
import struct
import zlib
from pathlib import Path

from fog_height_integral_rd import exact_density_integral


WIDTH = 160
HEIGHT = 90
FOV_DEGREES = 58.0
RAY_LIMIT = 120.0
GROUND = 0.0
DENSITY = 0.008
FALLOFF = 0.08
FOG_COLOR = (0.46, 0.59, 0.72)
CAMERA_HEIGHTS = (2.0, 8.0, 20.0)
DENSITIES = (0.002, 0.008, 0.02)
FALLOFFS = (0.02, 0.08, 0.2)
TARGET = (0.0, 4.0, 18.0)

Vector = tuple[float, float, float]
RGB = tuple[float, float, float]


def add(a: Vector, b: Vector) -> Vector:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def sub(a: Vector, b: Vector) -> Vector:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def mul(v: Vector, s: float) -> Vector:
    return v[0] * s, v[1] * s, v[2] * s


def dot(a: Vector, b: Vector) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vector, b: Vector) -> Vector:
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(v: Vector) -> Vector:
    return mul(v, 1.0 / math.sqrt(max(dot(v, v), 1e-20)))


def camera_basis(height: float) -> tuple[Vector, Vector, Vector, Vector]:
    origin = (0.0, height, 0.0)
    forward = normalize(sub(TARGET, origin))
    right = normalize(cross((0.0, 1.0, 0.0), forward))
    up = normalize(cross(forward, right))
    return origin, right, up, forward


def ray_for_pixel(x: int, y: int, right: Vector, up: Vector, forward: Vector) -> Vector:
    aspect = WIDTH / HEIGHT
    half_height = math.tan(math.radians(FOV_DEGREES) * 0.5)
    nx = 2.0 * (x + 0.5) / WIDTH - 1.0
    ny = 1.0 - 2.0 * (y + 0.5) / HEIGHT
    return normalize(add(forward, add(mul(right, nx * aspect * half_height), mul(up, ny * half_height))))


def chart_color(x: float, y: float, far: bool) -> RGB:
    palette = ((0.88, 0.08, 0.035), (0.025, 0.35, 0.95), (0.95, 0.52, 0.025),
               (0.08, 0.72, 0.22), (0.6, 0.12, 0.8), (0.72, 0.76, 0.8))
    tile_x = math.floor((x + (10.0 if far else 4.0)) * (0.72 if far else 1.15))
    tile_y = math.floor(y * 0.9)
    color = palette[(tile_x + 2 * tile_y + (3 if far else 0)) % len(palette)]
    if abs(((x + 0.13) * 7.0) % 1.0 - 0.5) < 0.025 or abs((y * 5.0) % 1.0 - 0.5) < 0.025:
        return (0.018, 0.022, 0.028)
    return color


def scene_sample(origin: Vector, direction: Vector) -> tuple[RGB, float, str]:
    hits: list[tuple[float, RGB, str]] = []
    ox, oy, oz = origin
    dx, dy, dz = direction

    # Slanted near and far vertical charts expose height/depth interaction.
    near_denominator = dz - 0.10 * dx
    if abs(near_denominator) > 1e-9:
        t = (12.0 + 0.10 * ox - oz) / near_denominator
        px, py, pz = (ox + t * dx, oy + t * dy, oz + t * dz)
        if t > 0.0 and -6.0 <= px <= 6.0 and 0.0 <= py <= 26.0 and abs(pz - (12.0 + 0.10 * px)) < 1e-4:
            hits.append((t, chart_color(px, py, False), "near_chart"))

    far_denominator = dz + 0.16 * dx
    if abs(far_denominator) > 1e-9:
        t = (32.0 - 0.16 * ox - oz) / far_denominator
        px, py, pz = (ox + t * dx, oy + t * dy, oz + t * dz)
        if t > 0.0 and -15.0 <= px <= 15.0 and 0.0 <= py <= 42.0 and abs(pz - (32.0 - 0.16 * px)) < 1e-4:
            hits.append((t, chart_color(px, py, True), "far_chart"))

    if dy < -1e-9:
        t = (GROUND - oy) / dy
        if 0.0 < t <= RAY_LIMIT:
            px, pz = ox + t * dx, oz + t * dz
            checker = (math.floor(px * 0.5) + math.floor(pz * 0.5)) & 1
            color = (0.20, 0.23, 0.26) if checker else (0.055, 0.075, 0.095)
            if abs((pz * 0.16) % 1.0 - 0.5) < 0.018:
                color = (1.5, 1.2, 0.7)
            hits.append((t, color, "ground"))

    if hits:
        distance, color, label = min(hits, key=lambda hit: hit[0])
        return color, min(distance, RAY_LIMIT), label

    if dy > 1e-9:
        distance = min(max(0.0, (80.0 - oy) / dy), RAY_LIMIT)
    else:
        distance = RAY_LIMIT
    horizon = max(0.0, min(1.0, (dy + 0.16) / 0.72))
    sky = (0.16 + 0.25 * horizon, 0.24 + 0.34 * horizon, 0.36 + 0.48 * horizon)
    return sky, distance, "sky"


def blend(source: RGB, transmittance: float) -> RGB:
    return tuple(source[c] * transmittance + FOG_COLOR[c] * (1.0 - transmittance) for c in range(3))


def fog_reference(source: RGB, origin_y: float, ray_y: float, distance: float,
                  density: float = DENSITY, falloff: float = FALLOFF) -> RGB:
    density_integral = exact_density_integral(origin_y, ray_y, distance, falloff, GROUND)
    return blend(source, math.exp(-density * density_integral))


def fog_camera_height_linear(source: RGB, camera_y: float, distance: float,
                             density_at_ground: float = DENSITY,
                             falloff: float = FALLOFF) -> RGB:
    density = density_at_ground * math.exp(-max(camera_y - GROUND, 0.0) * falloff)
    return blend(source, math.exp(-density * distance))


def fog_shader_source_mirror(source: RGB, camera_y: float, distance: float,
                             density_at_ground: float = DENSITY,
                             falloff: float = FALLOFF) -> RGB:
    density = density_at_ground * math.exp(-max(camera_y - GROUND, 0.0) * falloff)
    distance_factor = max(0.0, min(1.0, distance / RAY_LIMIT))
    transmittance = math.exp(-density * distance_factor * max(distance, 0.0))
    return blend(source, transmittance)


def compare(reference: list[RGB], candidate: list[RGB], indices: list[int] | None = None) -> dict[str, object]:
    if indices is None:
        indices = list(range(len(reference)))
    if not indices:
        return {"pixel_count": 0, "rgb_rmse": 0.0, "mean_absolute_rgb": [0.0, 0.0, 0.0],
                "max_absolute_rgb": [0.0, 0.0, 0.0]}
    abs_sum = [0.0, 0.0, 0.0]
    max_delta = [0.0, 0.0, 0.0]
    sum_sq = 0.0
    for i in indices:
        for c in range(3):
            delta = abs(reference[i][c] - candidate[i][c])
            abs_sum[c] += delta
            max_delta[c] = max(max_delta[c], delta)
            sum_sq += delta * delta
    return {"pixel_count": len(indices), "rgb_rmse": math.sqrt(sum_sq / (3 * len(indices))),
            "mean_absolute_rgb": [v / len(indices) for v in abs_sum], "max_absolute_rgb": max_delta}


def write_pfm(path: Path, pixels: list[RGB]) -> None:
    body = b"".join(struct.pack("<3f", *pixels[y * WIDTH + x])
                    for y in reversed(range(HEIGHT)) for x in range(WIDTH))
    path.write_bytes(f"PF\n{WIDTH} {HEIGHT}\n-1.0\n".encode("ascii") + body)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    data = kind + payload
    return struct.pack(">I", len(payload)) + data + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)


def write_preview(path: Path, pixels: list[RGB]) -> None:
    raw = bytearray()
    for y in range(HEIGHT):
        raw.append(0)
        for x in range(WIDTH):
            for channel in pixels[y * WIDTH + x]:
                mapped = max(0.0, channel) / (1.0 + max(0.0, channel))
                raw.append(int(max(0.0, min(1.0, mapped ** (1.0 / 2.2))) * 255.0 + 0.5))
    header = struct.pack(">2I5B", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    output = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
    output += png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + png_chunk(b"IEND", b"")
    path.write_bytes(output)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def render(output_dir: Path) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    camera_results: dict[str, object] = {}
    file_manifest: dict[str, dict[str, str]] = {}
    for camera_height in CAMERA_HEIGHTS:
        origin, right, up, forward = camera_basis(camera_height)
        sources: list[RGB] = []
        ray_data: list[tuple[Vector, float, str]] = []
        for y in range(HEIGHT):
            for x in range(WIDTH):
                ray = ray_for_pixel(x, y, right, up, forward)
                source, distance, label = scene_sample(origin, ray)
                sources.append(source)
                ray_data.append((ray, distance, label))

        reference: list[RGB] = []
        camera_linear: list[RGB] = []
        shader_mirror: list[RGB] = []
        groups: dict[str, list[int]] = {}
        for index, (source, (ray, distance, label)) in enumerate(zip(sources, ray_data)):
            reference.append(fog_reference(source, origin[1], ray[1], distance))
            camera_linear.append(fog_camera_height_linear(source, origin[1], distance))
            shader_mirror.append(fog_shader_source_mirror(source, origin[1], distance))
            groups.setdefault(label, []).append(index)

        parameter_sweep: dict[str, object] = {}
        for density in DENSITIES:
            for falloff in FALLOFFS:
                parameter_reference = [fog_reference(source, origin[1], ray[1], distance, density, falloff)
                                       for source, (ray, distance, _label) in zip(sources, ray_data)]
                linear_candidate = [fog_camera_height_linear(source, origin[1], distance, density, falloff)
                                    for source, (_ray, distance, _label) in zip(sources, ray_data)]
                shader_candidate = [fog_shader_source_mirror(source, origin[1], distance, density, falloff)
                                    for source, (_ray, distance, _label) in zip(sources, ray_data)]
                key = f"density={density:g},falloff={falloff:g}"
                parameter_sweep[key] = {
                    "camera_height_linear_distance": {
                        "all_pixels": compare(parameter_reference, linear_candidate),
                        "by_surface": {surface: compare(parameter_reference, linear_candidate, indices)
                                       for surface, indices in groups.items()},
                    },
                    "heightFogCS_source_equation": {
                        "all_pixels": compare(parameter_reference, shader_candidate),
                        "by_surface": {surface: compare(parameter_reference, shader_candidate, indices)
                                       for surface, indices in groups.items()},
                    },
                }

        camera_key = f"camera_y_{camera_height:g}m"
        camera_metrics = {}
        for name, candidate in (("camera_height_linear_distance", camera_linear),
                                 ("heightFogCS_source_equation", shader_mirror)):
            camera_metrics[name] = {
                "all_pixels": compare(reference, candidate),
                "by_surface": {surface: compare(reference, candidate, indices)
                               for surface, indices in groups.items()},
            }
        camera_results[camera_key] = {
            "camera_position": list(origin),
            "target": list(TARGET),
            "visible_surface_pixel_counts": {surface: len(indices) for surface, indices in groups.items()},
            "camera_height_sweep_vs_world_height_analytic": camera_metrics,
            "density_falloff_sweep_vs_world_height_analytic": parameter_sweep,
        }

        for candidate_name, pixels in (("world_height_analytic", reference),
                                       ("camera_height_linear_distance", camera_linear),
                                       ("heightFogCS_source_equation_mirror", shader_mirror)):
            stem = f"{camera_key}_{candidate_name}"
            pfm = output_dir / f"{stem}.pfm"
            preview = output_dir / f"{stem}_preview.png"
            write_pfm(pfm, pixels)
            write_preview(preview, pixels)
            file_manifest[stem] = {"pfm": pfm.name, "pfm_sha256": sha256(pfm),
                                   "preview_png": preview.name, "preview_sha256": sha256(preview)}

    manifest = {
        "schema": "fog-height-camera-scene-rd-v1",
        "image_size": [WIDTH, HEIGHT],
        "projection": {"type": "perspective", "vertical_fov_degrees": FOV_DEGREES,
                       "ray_limit_m": RAY_LIMIT},
        "medium": {"ground_y_m": GROUND, "density_at_ground_per_m": DENSITY,
                   "height_falloff_per_m": FALLOFF, "fog_rgb": FOG_COLOR,
                   "density_sweep_per_m": DENSITIES, "falloff_sweep_per_m": FALLOFFS,
                   "shader_absorption": 1.0},
        "scene": {"geometry": "two slanted height charts, checker ground, finite gradient sky",
                  "camera_target": TARGET, "camera_heights_m": CAMERA_HEIGHTS},
        "reference": "Piecewise analytic Beer-Lambert optical depth over world-space ray height; no in-scattering.",
        "candidates": {
            "camera_height_linear_distance": "Linear ray distance with density evaluated once at camera height.",
            "heightFogCS_source_equation_mirror": "Python mirror of heightFogCS.hlsl, including distanceFactor*distance; shader was not compiled or dispatched.",
        },
        "camera_results": camera_results,
        "captures": file_manifest,
        "limitations": ["Synthetic chart scene; arbitrary world units and fixed target.",
                        "Camera is aimed at the same world target for each height; scene and pixel rays are rerendered.",
                        "CPU equation mirror only; no application host, GPU runtime, depth buffer, alpha, or performance path.",
                        "Reference includes extinction only and does not evaluate directional in-scattering."],
    }
    (output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def main() -> None:
    output = Path("docs/worklog/artifacts/fog-camera-height-scene-rd-2026-11-12")
    manifest = render(output)
    print(f"Generated {len(manifest['captures'])} camera/candidate capture sets in {output}")
    for camera, result in manifest["camera_results"].items():
        metrics = result["camera_height_sweep_vs_world_height_analytic"]
        print(camera, {name: round(values["all_pixels"]["rgb_rmse"], 6)
                       for name, values in metrics.items()})


if __name__ == "__main__":
    main()
