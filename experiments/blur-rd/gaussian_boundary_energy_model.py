"""Compare Gaussian source equations at finite image edges.

The CPU branch mirrors OpenCV GaussianBlur's default REFLECT_101 border. The
GPU branch mirrors GauusianBlur.cpp's separable clamp-sampling equation. This
is a numerical source model only; it does not execute Artifact or a GPU.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_boundary_energy_model_2026-11-12.json"
PREVIEW = ROOT / "experiments/blur-rd/results/previews/gaussian_boundary_energy_model_2026-11-12.png"
POSITION_PREVIEW = ROOT / "experiments/blur-rd/results/previews/gaussian_boundary_position_sweep_2026-11-12.png"
SIZE = 513
SIGMAS = (0.5, 1.0, 3.0, 8.0, 16.0, 64.0)
POSITION_SWEEP_SIGMAS = (3.0, 16.0)
DISTANCES_FROM_LEFT_EDGE = (0, 1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cpu_kernel_size(sigma: float) -> int:
    # GauusianBlur.cpp uses float literals and a float appliedSigma, so preserve
    # the source's float32 multiply/add before the integer conversion.
    applied = np.float32(sigma)
    scaled = np.float32(np.float32(6.0) * applied + np.float32(1.0))
    size = max(1, int(scaled))
    return size + (size % 2 == 0)


def shader_kernel(sigma: float) -> np.ndarray:
    applied = max(np.float32(0.1), np.float32(sigma))
    radius = min(64, int(math.ceil(float(np.float32(applied * np.float32(3.0))))))
    weights = np.empty((radius * 2 + 1,), dtype=np.float32)
    total = np.float32(0.0)
    for i, offset in enumerate(range(-radius, radius + 1)):
        x = np.float32(offset)
        denom = max(np.float32(0.0001), np.float32(applied * applied))
        exponent = np.float32(np.float32(-0.5) * np.float32(np.float32(x * x) / denom))
        weight = np.float32(np.exp(exponent))
        weights[i] = weight
        total = np.float32(total + weight)
    return weights / total


def shader_axis(image: np.ndarray, kernel: np.ndarray, axis: int, border: str = "clamp") -> np.ndarray:
    radius = len(kernel) // 2
    positions = np.arange(image.shape[axis], dtype=np.int64)
    result = np.zeros_like(image)
    for index, offset in enumerate(range(-radius, radius + 1)):
        coordinates = positions + offset
        if border == "clamp":
            source_positions = np.clip(coordinates, 0, image.shape[axis] - 1)
        elif border == "reflect101":
            period = max(1, 2 * image.shape[axis] - 2)
            folded = np.mod(coordinates, period)
            source_positions = np.where(folded < image.shape[axis], folded, period - folded)
        else:
            raise ValueError(f"unsupported border policy: {border}")
        result += np.take(image, source_positions, axis=axis) * kernel[index]
    return result


def shader_blur(image: np.ndarray, sigma: float, border: str = "clamp") -> np.ndarray:
    kernel = shader_kernel(sigma)
    return shader_axis(shader_axis(image, kernel, 1, border), kernel, 0, border)


def image_difference(a: np.ndarray, b: np.ndarray) -> dict[str, float]:
    difference = a.astype(np.float64) - b.astype(np.float64)
    return {
        "rmse": float(np.sqrt(np.mean(difference * difference))),
        "max_abs": float(np.max(np.abs(difference))),
        "in_frame_energy_a": float(a.sum(dtype=np.float64)),
        "in_frame_energy_b": float(b.sum(dtype=np.float64)),
    }


def measure(image: np.ndarray) -> dict[str, float]:
    values = image.astype(np.float64)
    energy = float(values.sum())
    yy, xx = np.indices(values.shape, dtype=np.float64)
    if energy <= 0.0:
        return {"energy_in_image": energy, "peak": float(values.max())}
    cx = float((values * xx).sum() / energy)
    cy = float((values * yy).sum() / energy)
    vx = float((values * (xx - cx) ** 2).sum() / energy)
    vy = float((values * (yy - cy) ** 2).sum() / energy)
    return {
        "energy_in_image": energy,
        "centroid_xy_px": [cx, cy],
        "spread_sigma_xy_px": [math.sqrt(max(0.0, vx)), math.sqrt(max(0.0, vy))],
        "peak": float(values.max()),
    }


def write_preview() -> None:
    panel_size = 260
    margin = 20
    header = 72
    row_gap = 46
    width = margin * 2 + panel_size * 3 + 32 * 2
    height = header + margin * 2 + (panel_size + row_gap) * 2
    canvas = Image.new("RGB", (width, height), (25, 28, 34))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    draw.text((margin, 16), "Gaussian boundary impulse response — source equation model", fill="white", font=font)
    draw.text((margin, 36), "Shared linear scale within each sigma row; display color map only", fill=(195, 200, 210), font=font)
    columns = ("CPU: REFLECT_101", "GPU: clamp equation", "Absolute difference")
    for column, title in enumerate(columns):
        draw.text((margin + column * (panel_size + 32), 58), title, fill=(225, 228, 235), font=font)

    for row, sigma in enumerate((3.0, 16.0)):
        impulse = np.zeros((SIZE, SIZE), dtype=np.float32)
        impulse[0, 0] = 1.0
        ksize = cpu_kernel_size(sigma)
        cpu = cv2.GaussianBlur(impulse, (ksize, ksize), sigmaX=float(np.float32(sigma)))
        gpu = shader_blur(impulse, sigma)
        crop_cpu = cpu[:65, :65]
        crop_gpu = gpu[:65, :65]
        crop_diff = np.abs(crop_gpu - crop_cpu)
        vmax = float(crop_gpu.max())
        y = header + margin + row * (panel_size + row_gap)
        draw.text((margin, y - 19), f"sigma = {sigma:g} px   shared range [0, {vmax:.5g}]", fill=(255, 210, 120), font=font)
        for col, crop in enumerate((crop_cpu, crop_gpu, crop_diff)):
            normalized = np.clip(crop / vmax, 0.0, 1.0)
            gray = np.rint(normalized * 255.0).astype(np.uint8)
            heat = cv2.applyColorMap(gray, cv2.COLORMAP_INFERNO)
            rgb = cv2.cvtColor(heat, cv2.COLOR_BGR2RGB)
            panel = Image.fromarray(rgb).resize((panel_size, panel_size), Image.Resampling.NEAREST)
            x = margin + col * (panel_size + 32)
            canvas.paste(panel, (x, y))
            draw.rectangle((x, y, x + panel_size - 1, y + panel_size - 1), outline=(125, 130, 140))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(PREVIEW)


def position_sweep() -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    y = SIZE // 2
    for sigma in POSITION_SWEEP_SIGMAS:
        ksize = cpu_kernel_size(sigma)
        for distance in DISTANCES_FROM_LEFT_EDGE:
            impulse = np.zeros((SIZE, SIZE), dtype=np.float32)
            impulse[y, distance] = 1.0
            cpu = cv2.GaussianBlur(impulse, (ksize, ksize), sigmaX=float(np.float32(sigma)))
            gpu = shader_blur(impulse, sigma)
            cpu_metrics = measure(cpu)
            gpu_metrics = measure(gpu)
            difference = gpu.astype(np.float64) - cpu.astype(np.float64)
            records.append({
                "sigma_px": sigma,
                "distance_from_left_edge_px": distance,
                "cpu_in_frame_energy": cpu_metrics["energy_in_image"],
                "gpu_equation_in_frame_energy": gpu_metrics["energy_in_image"],
                "gpu_to_cpu_energy_ratio": gpu_metrics["energy_in_image"] / cpu_metrics["energy_in_image"],
                "cpu_centroid_xy_px": cpu_metrics["centroid_xy_px"],
                "gpu_equation_centroid_xy_px": gpu_metrics["centroid_xy_px"],
                "gpu_minus_cpu_centroid_x_px": gpu_metrics["centroid_xy_px"][0] - cpu_metrics["centroid_xy_px"][0],
                "full_frame_rgb_scalar_rmse": float(np.sqrt(np.mean(difference * difference))),
                "full_frame_max_abs_difference": float(np.max(np.abs(difference))),
            })
    return records


def boundary_policy_crossover() -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    y = SIZE // 2
    distances = (0, 1, 2, 4, 8, 16, 32, 48, 64)
    for sigma in POSITION_SWEEP_SIGMAS:
        ksize = cpu_kernel_size(sigma)
        for distance in distances:
            impulse = np.zeros((SIZE, SIZE), dtype=np.float32)
            impulse[y, distance] = 1.0
            cpu_reflect = cv2.GaussianBlur(
                impulse, (ksize, ksize), sigmaX=float(np.float32(sigma)), borderType=cv2.BORDER_REFLECT_101
            )
            cpu_clamp = cv2.GaussianBlur(
                impulse, (ksize, ksize), sigmaX=float(np.float32(sigma)), borderType=cv2.BORDER_REPLICATE
            )
            gpu_clamp = shader_blur(impulse, sigma, "clamp")
            gpu_reflect = shader_blur(impulse, sigma, "reflect101")
            records.append({
                "sigma_px": sigma,
                "distance_from_left_edge_px": distance,
                "original_route_cpu_reflect_vs_gpu_clamp": image_difference(cpu_reflect, gpu_clamp),
                "matched_reflect101_cpu_vs_gpu_equation": image_difference(cpu_reflect, gpu_reflect),
                "matched_clamp_cpu_vs_gpu_equation": image_difference(cpu_clamp, gpu_clamp),
                "crossed_cpu_clamp_vs_gpu_reflect": image_difference(cpu_clamp, gpu_reflect),
            })
    return records


def write_position_preview(records: list[dict[str, object]]) -> None:
    width, height = 1040, 690
    canvas = Image.new("RGB", (width, height), (25, 28, 34))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    draw.text((28, 15), "Gaussian edge response by impulse distance — source equation model", fill="white", font=font)
    draw.text((28, 34), "log2 spacing: x = distance + 1, y = GPU clamp / CPU REFLECT_101 energy ratio", fill=(195, 200, 210), font=font)
    left, right, panel_h = 100, 1000, 260
    x_max = math.log2(max(DISTANCES_FROM_LEFT_EDGE) + 1)
    colors = {3.0: (255, 155, 70), 16.0: (100, 190, 255)}
    for panel, sigma in enumerate(POSITION_SWEEP_SIGMAS):
        top = 84 + panel * 292
        bottom = top + panel_h
        y_min = 0.25
        y_max = 4.0 if sigma == 3.0 else 16.0
        draw.text((28, top + 4), f"sigma = {sigma:g} px", fill=(255, 220, 145), font=font)
        draw.line((left, top, left, bottom), fill=(150, 155, 165), width=1)
        draw.line((left, bottom, right, bottom), fill=(150, 155, 165), width=1)
        ticks = (0.25, 0.5, 1.0, 2.0, 4.0) if sigma == 3.0 else (0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0)
        log_min, log_max = math.log2(y_min), math.log2(y_max)
        for tick in ticks:
            ty = bottom - int(((math.log2(tick) - log_min) / (log_max - log_min)) * panel_h)
            draw.line((left, ty, right, ty), fill=(57, 61, 70), width=1)
            draw.text((44, ty - 5), f"{tick:g}", fill=(185, 190, 200), font=font)
        ref_y = bottom - int(((0.0 - log_min) / (log_max - log_min)) * panel_h)
        draw.line((left, ref_y, right, ref_y), fill=(220, 220, 220), width=1)
        subset = [row for row in records if row["sigma_px"] == sigma]
        points = []
        for item in subset:
            distance = int(item["distance_from_left_edge_px"])
            ratio = float(item["gpu_to_cpu_energy_ratio"])
            px = left + int((math.log2(distance + 1) / x_max) * (right - left))
            ratio = max(y_min, min(ratio, y_max))
            py = bottom - int(((math.log2(ratio) - log_min) / (log_max - log_min)) * panel_h)
            points.append((px, py))
        draw.line(points, fill=colors[sigma], width=3)
        for px, py in points:
            draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=colors[sigma], outline=(20, 22, 26))
        for distance in (0, 1, 2, 4, 8, 16, 32, 64, 128, 256):
            px = left + int((math.log2(distance + 1) / x_max) * (right - left))
            draw.line((px, bottom, px, bottom + 4), fill=(150, 155, 165), width=1)
            draw.text((px - 8, bottom + 8), str(distance), fill=(185, 190, 200), font=font)
    POSITION_PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(POSITION_PREVIEW)


def main() -> None:
    centers = {
        "center": (SIZE // 2, SIZE // 2),
        "edge_midpoint": (SIZE // 2, 0),
        "corner": (0, 0),
    }
    results = []
    for sigma in SIGMAS:
        size = cpu_kernel_size(sigma)
        cases = []
        for name, (y, x) in centers.items():
            impulse = np.zeros((SIZE, SIZE), dtype=np.float32)
            impulse[y, x] = 1.0
            cpu = cv2.GaussianBlur(impulse, (size, size), sigmaX=float(np.float32(sigma)))
            gpu_equation = shader_blur(impulse, sigma)
            cpu_metrics = measure(cpu)
            gpu_metrics = measure(gpu_equation)
            cases.append({
                "location": name,
                "coordinate_xy_px": [x, y],
                "cpu_opencv_reflect101": cpu_metrics,
                "gpu_shader_clamp_equation_mirror": gpu_metrics,
                "clamp_to_cpu_energy_ratio": (
                    gpu_metrics["energy_in_image"] / cpu_metrics["energy_in_image"]
                    if cpu_metrics["energy_in_image"] else None
                ),
            })
        results.append({
            "sigma_px": sigma,
            "cpu_kernel_width_px": size,
            "gpu_shader_radius_px": min(64, int(math.ceil(max(0.1, sigma) * 3.0))),
            "cases": cases,
        })

    write_preview()
    position_records = position_sweep()
    write_position_preview(position_records)
    crossover_records = boundary_policy_crossover()
    record = {
        "study": "Gaussian finite-image boundary energy: CPU REFLECT_101 vs GPU clamp equation",
        "run_date": "2026-11-12",
        "evidence_class": "source-equation numerical model; not an Artifact capture or GPU dispatch",
        "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(SOURCE),
        "runner": str(Path(__file__).resolve().relative_to(ROOT)).replace("\\", "/"),
        "runner_sha256": sha256(Path(__file__).resolve()),
        "opencv_version": cv2.__version__,
        "numpy_version": np.__version__,
        "fixture": {
            "dimensions_px": [SIZE, SIZE],
            "content": "opaque scalar unit impulse at center, edge midpoint, or corner",
            "alpha": "not modeled; constant opaque alpha makes the shader premultiply/unpremultiply neutral",
            "sigma_values_px": list(SIGMAS),
        },
        "method": {
            "cpu": "cv2.GaussianBlur with the source kernel-size formula and default BORDER_REFLECT_101",
            "gpu_equation_mirror": "float32 separable shader weights, radius ceil(3*sigma) capped at 64, coordinate clamp, source loop order",
            "metrics": "in-frame scalar energy, centroid, second-moment spread, peak",
        },
        "measurements": results,
        "position_sweep": {
            "fixture": "opaque unit impulse on the horizontal center row, translated from the left image boundary",
            "sigma_values_px": list(POSITION_SWEEP_SIGMAS),
            "distances_from_left_edge_px": list(DISTANCES_FROM_LEFT_EDGE),
            "measurements": position_records,
            "visualization": {
                "path": str(POSITION_PREVIEW.relative_to(ROOT)).replace("\\", "/"),
                "sha256": sha256(POSITION_PREVIEW),
                "description": "GPU clamp to CPU REFLECT_101 in-frame energy ratio by distance; log2(distance+1) horizontal spacing, log2 energy-ratio vertical axis, and a reference line at 1.",
            },
        },
        "boundary_policy_crossover": {
            "purpose": "Separate finite-image boundary-policy differences from kernel-weight/equation differences by crossing clamp and REFLECT_101 policies on both equation models.",
            "method": "OpenCV GaussianBlur with BORDER_REFLECT_101 or BORDER_REPLICATE; the float32 shader-equation mirror with clamp or reflect101 index mapping.",
            "sigma_values_px": list(POSITION_SWEEP_SIGMAS),
            "distances_from_left_edge_px": [0, 1, 2, 4, 8, 16, 32, 48, 64],
            "measurements": crossover_records,
            "scope_limit": "Counterfactual policy variants only. Artifact does not currently execute both equation models with both border policies, and this does not recommend which boundary is visually correct.",
        },
        "visualization": {
            "path": str(PREVIEW.relative_to(ROOT)).replace("\\", "/"),
            "sha256": sha256(PREVIEW),
            "description": "Corner impulse responses at sigma 3 and 16; CPU, GPU equation mirror, and absolute difference use one shared linear scale per sigma row.",
        },
        "scope_limit": "Does not execute Artifact, Diligent, a GPU, effect wrappers, color descriptors, interactive resolution scaling, or user-visible composition. The result identifies equation-level edge-response differences only.",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(OUTPUT)
    for row in results:
        named = {case["location"]: case["clamp_to_cpu_energy_ratio"] for case in row["cases"]}
        print(f"sigma={row['sigma_px']:>4}: edge={named['edge_midpoint']:.4g}x corner={named['corner']:.4g}x")


if __name__ == "__main__":
    main()
