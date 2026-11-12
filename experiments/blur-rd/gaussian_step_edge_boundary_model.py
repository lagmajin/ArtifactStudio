"""Measure finite-image boundary effects on opaque Gaussian step edges.

This reuses the source-equation model in gaussian_boundary_energy_model.py.
It is not an Artifact capture or a GPU measurement.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

from gaussian_boundary_energy_model import (
    POSITION_SWEEP_SIGMAS,
    SIZE,
    cpu_kernel_size,
    shader_axis,
    shader_kernel,
)

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
MODEL = ROOT / "experiments/blur-rd/gaussian_boundary_energy_model.py"
OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_step_edge_boundary_model_2026-11-12.json"
PREVIEW = ROOT / "experiments/blur-rd/results/previews/gaussian_step_edge_boundary_model_2026-11-12.png"
EDGE_POSITIONS = tuple(range(0, 53)) + (64, 96, 128, 192, 256)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crossing(profile: np.ndarray, threshold: float) -> float | None:
    indices = np.flatnonzero(profile >= threshold)
    if not len(indices):
        return None
    idx = int(indices[0])
    if idx == 0:
        return 0.0 if profile[0] == threshold else None
    lo, hi = float(profile[idx - 1]), float(profile[idx])
    if lo == hi:
        return float(idx)
    return (idx - 1) + (threshold - lo) / (hi - lo)


def metrics(profile: np.ndarray) -> dict[str, object]:
    p = profile.astype(np.float64)
    x10, x50, x90 = crossing(p, 0.1), crossing(p, 0.5), crossing(p, 0.9)
    has_full_transition = p.min() <= 0.1 and p.max() >= 0.9 and x10 is not None and x90 is not None
    return {
        "min": float(p.min()),
        "max": float(p.max()),
        "mean": float(p.mean()),
        "x0": float(p[0]),
        "x1": float(p[1]),
        "edge_crossing_10_px": x10 if has_full_transition else None,
        "edge_crossing_50_px": x50 if has_full_transition else None,
        "edge_crossing_90_px": x90 if has_full_transition else None,
        "edge_spread_10_90_px": x90 - x10 if has_full_transition else None,
        "monotonicity_violations_gt_1e-6": int(np.count_nonzero(np.diff(p) < -1.0e-6)),
    }


def render_profiles(profile: np.ndarray, sigma: float) -> tuple[np.ndarray, np.ndarray]:
    ksize = cpu_kernel_size(sigma)
    applied = float(np.float32(sigma))
    cpu = cv2.GaussianBlur(
        profile.reshape(1, -1),
        (ksize, ksize),
        sigmaX=applied,
        borderType=cv2.BORDER_REFLECT_101,
    ).reshape(-1)
    kernel = shader_kernel(sigma)
    row = profile.reshape(1, -1).copy()
    gpu = shader_axis(row, kernel, 1, "clamp")
    gpu = shader_axis(gpu, kernel, 0, "clamp").reshape(-1)
    return cpu, gpu


def measure_all() -> list[dict[str, object]]:
    records = []
    for sigma in POSITION_SWEEP_SIGMAS:
        for edge_x in EDGE_POSITIONS:
            source = np.zeros((SIZE,), dtype=np.float32)
            source[edge_x:] = 1.0
            cpu, gpu = render_profiles(source, sigma)
            difference = gpu.astype(np.float64) - cpu.astype(np.float64)
            abs_difference = np.abs(difference)
            radius = int(math.ceil(3.0 * sigma))
            boundary_end = min(SIZE, radius + 1)
            edge_start = max(0, edge_x - radius)
            edge_end = min(SIZE, edge_x + radius + 1)
            records.append({
                "sigma_px": sigma,
                "input_step_x_px": edge_x,
                "both_source_values_present_in_frame": bool(edge_x > 0 and edge_x < SIZE),
                "cpu_reflect101": metrics(cpu),
                "gpu_clamp_equation_mirror": metrics(gpu),
                "difference": {
                    "whole_profile_rmse": float(np.sqrt(np.mean(difference * difference))),
                    "whole_profile_mae": float(np.mean(abs_difference)),
                    "whole_profile_max_abs": float(np.max(abs_difference)),
                    "whole_profile_changed_samples_gt_1e-3": int(np.count_nonzero(abs_difference > 1.0e-3)),
                    "whole_profile_l1_error_share_in_boundary_band": float(abs_difference[:boundary_end].sum() / abs_difference.sum()) if abs_difference.sum() > 0.0 else 0.0,
                    "boundary_band_x_px": [0, boundary_end],
                    "boundary_band_rmse": float(np.sqrt(np.mean(difference[:boundary_end] ** 2))),
                    "boundary_band_changed_samples_gt_1e-3": int(np.count_nonzero(abs_difference[:boundary_end] > 1.0e-3)),
                    "feature_band_x_px": [edge_start, edge_end],
                    "feature_band_rmse": float(np.sqrt(np.mean(difference[edge_start:edge_end] ** 2))),
                },
            })
    return records


def support_convergence_summary(records: list[dict[str, object]]) -> list[dict[str, object]]:
    summaries = []
    for sigma in POSITION_SWEEP_SIGMAS:
        cpu_radius = (cpu_kernel_size(sigma) - 1) // 2
        gpu_radius = int(math.ceil(3.0 * sigma))
        cases = [r for r in records if r["sigma_px"] == sigma]
        neutral = [
            int(r["input_step_x_px"])
            for r in cases
            if int(r["input_step_x_px"]) > max(cpu_radius, gpu_radius)
            and r["difference"]["whole_profile_max_abs"] <= 1.0e-6
        ]
        material = [
            int(r["input_step_x_px"])
            for r in cases
            if r["difference"]["whole_profile_max_abs"] > 1.0e-3
        ]
        summaries.append({
            "sigma_px": sigma,
            "cpu_kernel_radius_px": cpu_radius,
            "gpu_shader_radius_px": gpu_radius,
            "first_sampled_neutral_distance_after_support_px": min(neutral) if neutral else None,
            "largest_sampled_edge_distance_with_error_gt_1e-3_px": max(material) if material else None,
            "neutrality_rule": "whole-profile max absolute difference <= 1e-6, after both finite supports no longer intersect the boundary",
            "interpretation": "Equation-model convergence threshold for this opaque step family, not a perceptual tolerance or live backend parity claim.",
        })
    return summaries


def draw_profile_panel(
    canvas: Image.Image,
    draw: ImageDraw.ImageDraw,
    record: dict[str, object],
    origin: tuple[int, int],
) -> None:
    x0, y0 = origin
    width, height = 450, 230
    left, right = x0 + 42, x0 + width - 12
    top, bottom = y0 + 27, y0 + height - 28
    sigma = float(record["sigma_px"])
    edge_x = int(record["input_step_x_px"])
    profile_end = min(SIZE - 1, max(edge_x + int(math.ceil(3.5 * sigma)), 12))
    source = np.zeros((SIZE,), dtype=np.float32)
    source[edge_x:] = 1.0
    cpu, gpu = render_profiles(source, sigma)
    font = ImageFont.load_default()
    draw.text((x0, y0), f"sigma={sigma:g}px, input step x={edge_x}px", fill=(235, 238, 245), font=font)
    for value in (0.0, 0.5, 1.0):
        py = bottom - int(value * (bottom - top))
        draw.line((left, py, right, py), fill=(58, 63, 72), width=1)
        draw.text((x0 + 4, py - 5), f"{value:g}", fill=(170, 176, 187), font=font)
    draw.line((left, bottom, right, bottom), fill=(150, 155, 165), width=1)
    draw.line((left, top, left, bottom), fill=(150, 155, 165), width=1)
    for xpos in (0, edge_x, profile_end):
        px = left + int((xpos / max(1, profile_end)) * (right - left))
        draw.line((px, bottom, px, bottom + 3), fill=(150, 155, 165), width=1)
        draw.text((px - 5, bottom + 6), str(xpos), fill=(170, 176, 187), font=font)
    colors = ((255, 155, 70), (100, 190, 255))
    for curve, color in ((cpu, colors[0]), (gpu, colors[1])):
        points = []
        for x in range(profile_end + 1):
            px = left + int((x / max(1, profile_end)) * (right - left))
            py = bottom - int(np.clip(curve[x], 0.0, 1.0) * (bottom - top))
            points.append((px, py))
        draw.line(points, fill=color, width=2)
    draw.line((x0 + 240, y0 + 11, x0 + 256, y0 + 11), fill=colors[0], width=2)
    draw.text((x0 + 261, y0 + 6), "CPU reflect101", fill=(205, 210, 220), font=font)
    draw.line((x0 + 355, y0 + 11, x0 + 371, y0 + 11), fill=colors[1], width=2)
    draw.text((x0 + 376, y0 + 6), "GPU clamp", fill=(205, 210, 220), font=font)


def write_preview(records: list[dict[str, object]]) -> None:
    canvas = Image.new("RGB", (960, 560), (25, 28, 34))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    draw.text((24, 12), "Gaussian step-edge boundary response — source equation model", fill="white", font=font)
    draw.text((24, 29), "Same input and linear 0–1 scale; no display transform or app capture", fill=(195, 200, 210), font=font)
    cases = [(3.0, 1), (3.0, 8), (16.0, 1), (16.0, 32)]
    lookup = {(float(r["sigma_px"]), int(r["input_step_x_px"])): r for r in records}
    for i, case in enumerate(cases):
        col, row = i % 2, i // 2
        draw_profile_panel(canvas, draw, lookup[case], (24 + col * 468, 54 + row * 246))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(PREVIEW)


def main() -> None:
    records = measure_all()
    convergence = support_convergence_summary(records)
    write_preview(records)
    output = {
        "study": "Gaussian opaque step-edge response near finite image boundary",
        "run_date": "2026-11-12",
        "evidence_class": "CPU/GPU source-equation model only; not Artifact output or GPU dispatch",
        "source": str(SOURCE.relative_to(ROOT)).replace("\\", "/"),
        "source_sha256": sha256(SOURCE),
        "base_model": str(MODEL.relative_to(ROOT)).replace("\\", "/"),
        "base_model_sha256": sha256(MODEL),
        "runner": str(Path(__file__).resolve().relative_to(ROOT)).replace("\\", "/"),
        "runner_sha256": sha256(Path(__file__).resolve()),
        "opencv_version": cv2.__version__,
        "numpy_version": np.__version__,
        "conditions": {
            "image_profile_width_px": SIZE,
            "input": "opaque horizontal black-to-white step; source[x >= edge_x] = 1, source[x < edge_x] = 0",
            "alpha": "constant opaque, not modeled",
            "cpu": "OpenCV GaussianBlur, source kernel-width formula, default REFLECT_101 border",
            "gpu_equation_mirror": "float32 separable weights, radius ceil(3*sigma) capped at 64, clamp sampling",
            "sigma_values_px": list(POSITION_SWEEP_SIGMAS),
            "edge_positions_px": list(EDGE_POSITIONS),
        },
        "support_convergence": convergence,
        "metrics": "whole-profile RMSE/MAE/max error, boundary-band and feature-band RMSE, absolute 10-90 spread and 50% crossing when the full transition remains in frame, monotonicity violations",
        "measurements": records,
        "visualization": {
            "path": str(PREVIEW.relative_to(ROOT)).replace("\\", "/"),
            "sha256": sha256(PREVIEW),
            "description": "Four representative CPU/GPU line profiles near the left boundary at sigma 3 and 16, including a 1 px and a farther-inset edge.",
        },
        "scope_limit": "One-dimensional opaque step profiles isolate border response. No RGB hue, alpha association, 2D corner, composition, display transform, runtime route, GPU performance, or visual acceptance is established.",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(OUTPUT)
    for sigma in POSITION_SWEEP_SIGMAS:
        edge = next(r for r in records if r["sigma_px"] == sigma and r["input_step_x_px"] == 1)
        print(f"sigma={sigma:g}, edge=1px: RMSE={edge['difference']['whole_profile_rmse']:.6g}, max={edge['difference']['whole_profile_max_abs']:.6g}, CPU width={edge['cpu_reflect101']['edge_spread_10_90_px']}, GPU width={edge['gpu_clamp_equation_mirror']['edge_spread_10_90_px']}")


if __name__ == "__main__":
    main()
