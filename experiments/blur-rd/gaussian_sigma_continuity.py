"""Measure discrete sigma-step responses for opaque impulse and HDR line.

CPU and GPU paths mirror the current source equations. This is not a live
Artifact render or a perceptual acceptance test.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

from gaussian_boundary_energy_model import ROOT, SOURCE, cpu_kernel_size, shader_axis, shader_kernel

OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_sigma_continuity_2026-11-12.json"
PREVIEW = ROOT / "experiments/blur-rd/results/previews/gaussian_sigma_continuity_2026-11-12.png"
SIZE = 513
CENTER = SIZE // 2
SAMPLES = tuple(float(np.float32(i / 10.0)) for i in range(1, 641))
TRANSITIONS = (0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 2.9, 3.0, 3.1, 21.0, 21.1, 21.2, 21.3, 21.4, 21.5, 64.0)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_kernel(sigma: float, route: str) -> np.ndarray:
    if route == "cpu_reflect101":
        radius = (cpu_kernel_size(sigma) - 1) // 2
        return cv2.getGaussianKernel(2 * radius + 1, float(np.float32(sigma)), cv2.CV_32F).reshape(-1)
    return shader_kernel(sigma)


def source_kernel_with_radius(sigma: float, route: str, radius: int) -> np.ndarray:
    """Evaluate the route's weight equation while holding finite support fixed."""
    if route == "cpu_reflect101":
        return cv2.getGaussianKernel(2 * radius + 1, float(np.float32(sigma)), cv2.CV_32F).reshape(-1)
    applied = max(np.float32(0.1), np.float32(sigma))
    denom = max(np.float32(0.0001), np.float32(applied * applied))
    weights = np.empty((2 * radius + 1,), dtype=np.float32)
    total = np.float32(0.0)
    for index, offset in enumerate(range(-radius, radius + 1)):
        x = np.float32(offset)
        exponent = np.float32(np.float32(-0.5) * np.float32(np.float32(x * x) / denom))
        weights[index] = np.float32(np.exp(exponent))
        total = np.float32(total + weights[index])
    return weights / total


def radii(sigma: float) -> tuple[int, int]:
    cpu = (cpu_kernel_size(sigma) - 1) // 2
    gpu = min(64, int(math.ceil(float(np.float32(np.float32(sigma) * np.float32(3.0))))))
    return cpu, gpu


def profile_stats(profile: np.ndarray) -> dict[str, float]:
    p = profile.astype(np.float64)
    mass = float(p.sum())
    xs = np.arange(len(p), dtype=np.float64)
    centroid = float((p * xs).sum() / mass) if mass else float(CENTER)
    variance = float((p * (xs - centroid) ** 2).sum() / mass) if mass else 0.0
    return {
        "in_frame_mass": mass,
        "peak": float(p.max()),
        "centroid_px": centroid,
        "spread_sigma_px": math.sqrt(max(0.0, variance)),
        "r50_px": radius_at_mass(p, 0.50, centroid),
        "r95_px": radius_at_mass(p, 0.95, centroid),
    }


def radius_at_mass(p: np.ndarray, fraction: float, center: float) -> float:
    order = np.argsort(np.abs(np.arange(len(p), dtype=np.float64) - center))
    cumulative = np.cumsum(p[order])
    index = int(np.searchsorted(cumulative, fraction * p.sum(), side="left"))
    return float(abs(float(order[min(index, len(order) - 1)]) - center))


def impulse_profile(kernel: np.ndarray) -> np.ndarray:
    profile = np.zeros((SIZE,), dtype=np.float32)
    radius = len(kernel) // 2
    profile[CENTER - radius:CENTER + radius + 1] = kernel
    return profile


def line_profile(kernel: np.ndarray) -> np.ndarray:
    source = np.zeros((SIZE,), dtype=np.float32)
    source[CENTER - 1:CENTER + 2] = 4.0
    return convolve_profile(source, kernel)


def convolve_profile(source: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    radius = len(kernel) // 2
    result = np.zeros_like(source)
    for offset, weight in enumerate(kernel):
        shift = offset - radius
        start = max(0, CENTER - 1 + shift)
        end = min(SIZE, CENTER + 2 + shift)
        src_start = start - shift
        src_end = end - shift
        result[start:end] += source[src_start:src_end] * weight
    return result


def model_fidelity_controls(fixtures: dict[str, np.ndarray]) -> dict[str, object]:
    """Compare compact kernel-factor models with explicit 1D source operations."""
    summaries = {}
    for fixture_name, image in fixtures.items():
        row = image[CENTER:CENTER + 1, :].copy()
        cpu_errors = []
        gpu_errors = []
        for sigma in SAMPLES:
            ksize = cpu_kernel_size(sigma)
            cpu_reference = cv2.GaussianBlur(
                row, (ksize, ksize), sigmaX=float(np.float32(sigma)), borderType=cv2.BORDER_REFLECT_101
            ).reshape(-1)
            cpu_kernel = cv2.getGaussianKernel(ksize, float(np.float32(sigma)), cv2.CV_32F).reshape(-1)
            cpu_model = convolve_profile(row.reshape(-1), cpu_kernel)
            gpu_kernel = shader_kernel(sigma)
            gpu_reference = shader_axis(row, gpu_kernel, 1, "clamp").reshape(-1)
            gpu_model = convolve_profile(row.reshape(-1), gpu_kernel)
            cpu_errors.append(diff_metrics(cpu_reference, cpu_model))
            gpu_errors.append(diff_metrics(gpu_reference, gpu_model))
        summaries[fixture_name] = {
            "sample_count": len(SAMPLES),
            "cpu_gaussianblur_vs_kernel_factor_max_rmse": max(cpu_errors, key=lambda row: row["rmse"]),
            "cpu_gaussianblur_vs_kernel_factor_p95_rmse": float(np.percentile([row["rmse"] for row in cpu_errors], 95)),
            "gpu_shader_axis_mirror_vs_kernel_factor_max_rmse": max(gpu_errors, key=lambda row: row["rmse"]),
            "gpu_shader_axis_mirror_vs_kernel_factor_p95_rmse": float(np.percentile([row["rmse"] for row in gpu_errors], 95)),
            "interpretation": "Checks the 1D horizontal response used by the separable outer-product derivation against explicit CPU/API and GPU-equation sample accumulation. It does not execute the Artifact compute shader or cover vertical pass resource/descriptor behavior.",
        }
    return summaries


def image_rmse_from_separable_profiles(
    a: np.ndarray,
    b: np.ndarray,
    fixture: str,
    kernel_a: np.ndarray,
    kernel_b: np.ndarray,
) -> float:
    # Centered impulse output is the outer product of the 1D PSF. A 3px
    # horizontal line is separable: vertical profile is the PSF and horizontal
    # profile is the 1D convolution of the line. Keep exact finite float32
    # output for the measured support region and zero elsewhere.
    if fixture == "opaque_center_impulse_linear_1":
        vertical_a = np.zeros((SIZE,), dtype=np.float32)
        vertical_b = np.zeros((SIZE,), dtype=np.float32)
        radius_a, radius_b = len(kernel_a) // 2, len(kernel_b) // 2
        vertical_a[CENTER - radius_a:CENTER + radius_a + 1] = kernel_a
        vertical_b[CENTER - radius_b:CENTER + radius_b + 1] = kernel_b
        outer_delta = np.outer(vertical_a, vertical_a).astype(np.float32) - np.outer(vertical_b, vertical_b).astype(np.float32)
        return float(np.sqrt(np.mean(outer_delta.astype(np.float64) ** 2)))
    # A horizontal line's image is the outer product of the horizontal line
    # response and the vertical impulse response. Both change with sigma.
    vertical_a = np.zeros((SIZE,), dtype=np.float32)
    vertical_b = np.zeros((SIZE,), dtype=np.float32)
    radius_a, radius_b = len(kernel_a) // 2, len(kernel_b) // 2
    vertical_a[CENTER - radius_a:CENTER + radius_a + 1] = kernel_a
    vertical_b[CENTER - radius_b:CENTER + radius_b + 1] = kernel_b
    horizontal_a = line_profile(kernel_a)
    horizontal_b = line_profile(kernel_b)
    outer_a = np.outer(vertical_a, horizontal_a).astype(np.float32)
    outer_b = np.outer(vertical_b, horizontal_b).astype(np.float32)
    return float(np.sqrt(np.mean((outer_a.astype(np.float64) - outer_b.astype(np.float64)) ** 2)))


def route_image_profiles(kernel: np.ndarray, fixture: str) -> tuple[np.ndarray, np.ndarray]:
    vertical = np.zeros((SIZE,), dtype=np.float32)
    radius = len(kernel) // 2
    vertical[CENTER - radius:CENTER + radius + 1] = kernel
    horizontal = impulse_profile(kernel) if fixture == "opaque_center_impulse_linear_1" else line_profile(kernel)
    return vertical, horizontal


def full_image_route_difference(kernel_cpu: np.ndarray, kernel_gpu: np.ndarray, fixture: str) -> dict[str, float]:
    vertical_cpu, horizontal_cpu = route_image_profiles(kernel_cpu, fixture)
    vertical_gpu, horizontal_gpu = route_image_profiles(kernel_gpu, fixture)
    active_radius = max(len(kernel_cpu), len(kernel_gpu)) // 2
    start, end = CENTER - active_radius, CENTER + active_radius + 1
    image_cpu = np.outer(vertical_cpu[start:end], horizontal_cpu).astype(np.float32)
    image_gpu = np.outer(vertical_gpu[start:end], horizontal_gpu).astype(np.float32)
    d = image_gpu.astype(np.float64) - image_cpu.astype(np.float64)
    mse = float(np.sum(d * d) / (SIZE * SIZE))
    ad = np.abs(d)
    return {
        "rmse": math.sqrt(mse),
        "mae": float(ad.sum() / (SIZE * SIZE)),
        "max_abs": float(ad.max()),
        "l1": float(ad.sum()),
    }


def output_factors(kernel: np.ndarray, fixture: str) -> tuple[np.ndarray, np.ndarray]:
    vertical = np.zeros((SIZE,), dtype=np.float64)
    radius = len(kernel) // 2
    vertical[CENTER - radius:CENTER + radius + 1] = kernel.astype(np.float64)
    horizontal = (
        impulse_profile(kernel).astype(np.float64)
        if fixture == "opaque_center_impulse_linear_1"
        else line_profile(kernel).astype(np.float64)
    )
    return vertical, horizontal


def signed_separable_combination_rmse(terms: list[tuple[float, tuple[np.ndarray, np.ndarray]]]) -> float:
    squared_sum = 0.0
    for coeff_a, (vertical_a, horizontal_a) in terms:
        for coeff_b, (vertical_b, horizontal_b) in terms:
            squared_sum += (
                coeff_a * coeff_b
                * float(np.dot(vertical_a, vertical_b))
                * float(np.dot(horizontal_a, horizontal_b))
            )
    return math.sqrt(max(0.0, squared_sum / (SIZE * SIZE)))


def support_transition_extra_rmse(
    sigma_from: float,
    sigma_to: float,
    route: str,
    fixture: str,
) -> tuple[float, int, int]:
    radius_from = radii(sigma_from)[0 if route == "cpu_reflect101" else 1]
    radius_to = radii(sigma_to)[0 if route == "cpu_reflect101" else 1]
    held_radius = max(radius_from, radius_to)
    actual_from = output_factors(source_kernel(sigma_from, route), fixture)
    actual_to = output_factors(source_kernel(sigma_to, route), fixture)
    fixed_from = output_factors(source_kernel_with_radius(sigma_from, route, held_radius), fixture)
    fixed_to = output_factors(source_kernel_with_radius(sigma_to, route, held_radius), fixture)
    # (actual output step) - (same sigma step with support held constant).
    extra = signed_separable_combination_rmse([
        (1.0, actual_to),
        (-1.0, actual_from),
        (-1.0, fixed_to),
        (1.0, fixed_from),
    ])
    return extra, radius_from, radius_to


def write_preview(outputs: dict[str, dict[str, dict[str, np.ndarray]]]) -> None:
    width, height = 1280, 760
    margin, gap_x, gap_y = 56, 38, 36
    panel_w, panel_h = 565, 300
    canvas = Image.new("RGB", (width, height), (25, 28, 34))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    draw.text((margin, 18), "Gaussian sigma response — source equation model", fill=(242, 244, 248), font=font)
    draw.text((margin, 36), "Linear float32 central-row profiles; synthetic opaque fixtures; not an Artifact capture", fill=(190, 197, 207), font=font)
    fixtures = list(outputs)
    for row, sigma in enumerate((0.8, 64.0)):
        for col, fixture in enumerate(fixtures):
            x0 = margin + col * (panel_w + gap_x)
            y0 = 76 + row * (panel_h + gap_y)
            x1, y1 = x0 + panel_w, y0 + panel_h
            draw.rectangle((x0, y0, x1, y1), outline=(89, 96, 108), width=1)
            draw.text((x0 + 8, y0 + 7), f"{fixture}; sigma={sigma:g}; CPU/GPU radii={radii(sigma)}", fill=(232, 220, 177), font=font)
            key = f"{sigma:.1f}"
            profiles = outputs[fixture][key]
            half = max(12, int(math.ceil(3.5 * sigma)))
            left = max(0, CENTER - half)
            right = min(SIZE, CENTER + half + 1)
            plot_l, plot_r = x0 + 42, x1 - 14
            plot_t, plot_b = y0 + 35, y1 - 30
            draw.line((plot_l, plot_b, plot_r, plot_b), fill=(120, 126, 138), width=1)
            vmax = max(float(profiles["cpu_reflect101"][left:right].max()), float(profiles["gpu_clamp_equation"][left:right].max()), 1e-20)
            for name, color in (("cpu_reflect101", (67, 175, 255)), ("gpu_clamp_equation", (255, 153, 72))):
                values = profiles[name][left:right].astype(np.float64)
                points = []
                for index, value in enumerate(values):
                    px = plot_l + index * (plot_r - plot_l) / max(1, len(values) - 1)
                    py = plot_b - float(value / vmax) * (plot_b - plot_t)
                    points.append((px, py))
                draw.line(points, fill=color, width=2)
            draw.text((plot_l, plot_b + 8), f"x = {left-CENTER}..{right-CENTER-1} px from center; shared linear peak scale 0..{vmax:.4g}", fill=(181, 187, 198), font=font)
            draw.line((x1 - 170, y0 + 21, x1 - 151, y0 + 21), fill=(67, 175, 255), width=3)
            draw.text((x1 - 147, y0 + 15), "CPU", fill=(211, 216, 223), font=font)
            draw.line((x1 - 92, y0 + 21, x1 - 73, y0 + 21), fill=(255, 153, 72), width=3)
            draw.text((x1 - 69, y0 + 15), "GPU", fill=(211, 216, 223), font=font)
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(PREVIEW)


def diff_metrics(a: np.ndarray, b: np.ndarray) -> dict[str, float]:
    d = a.astype(np.float64) - b.astype(np.float64)
    ad = np.abs(d)
    return {
        "rmse": float(np.sqrt(np.mean(d * d))),
        "mae": float(np.mean(ad)),
        "max_abs": float(ad.max()),
        "l1": float(ad.sum()),
    }


def make_fixtures() -> dict[str, np.ndarray]:
    impulse = np.zeros((SIZE, SIZE), dtype=np.float32)
    impulse[CENTER, CENTER] = 1.0
    hdr_line = np.zeros_like(impulse)
    hdr_line[CENTER, CENTER - 1:CENTER + 2] = 4.0
    return {"opaque_center_impulse_linear_1": impulse, "opaque_center_3px_hdr_line_linear_4": hdr_line}


def main() -> None:
    fixtures = make_fixtures()
    fixture_hashes = {}
    for name, image in fixtures.items():
        fixture_hashes[name] = hashlib.sha256(image.tobytes()).hexdigest()
    by_fixture: dict[str, object] = {}
    outputs_by_fixture: dict[str, dict[str, dict[str, np.ndarray]]] = {}
    for fixture_name, image in fixtures.items():
        outputs_by_sigma: dict[str, dict[str, np.ndarray]] = {}
        kernels_by_sigma: dict[str, dict[str, np.ndarray]] = {}
        stats_by_sigma: dict[str, dict[str, dict[str, float]]] = {}
        for sigma in SAMPLES:
            key = f"{sigma:.1f}"
            kernels = {route: source_kernel(sigma, route) for route in ("cpu_reflect101", "gpu_clamp_equation")}
            kernels_by_sigma[key] = kernels
            profiles = {
                route: (
                    impulse_profile(kernel) * kernel[len(kernel) // 2]
                    if fixture_name == "opaque_center_impulse_linear_1"
                    else line_profile(kernel) * kernel[len(kernel) // 2]
                )
                for route, kernel in kernels.items()
            }
            outputs_by_sigma[key] = profiles
            stats_by_sigma[key] = {route: profile_stats(profile) for route, profile in profiles.items()}
        parity_rows = []
        for sigma in SAMPLES:
            key = f"{sigma:.1f}"
            parity_rows.append({
                "sigma": sigma,
                "cpu_radius_px": radii(sigma)[0],
                "gpu_radius_px": radii(sigma)[1],
                **full_image_route_difference(
                    kernels_by_sigma[key]["cpu_reflect101"],
                    kernels_by_sigma[key]["gpu_clamp_equation"],
                    fixture_name,
                ),
            })
        adjacent = []
        for index in range(1, len(SAMPLES)):
            before, after = SAMPLES[index - 1], SAMPLES[index]
            pair = {}
            for route in ("cpu_reflect101", "gpu_clamp_equation"):
                p0 = outputs_by_sigma[f"{before:.1f}"][route]
                p1 = outputs_by_sigma[f"{after:.1f}"][route]
                pair[route] = diff_metrics(p1, p0)
                pair[route]["full_image_rmse"] = image_rmse_from_separable_profiles(
                    p1, p0, fixture_name,
                    kernels_by_sigma[f"{after:.1f}"][route],
                    kernels_by_sigma[f"{before:.1f}"][route],
                )
                pair[route]["peak_relative_change"] = float(abs(float(p1.max()) - float(p0.max())) / max(abs(float(p0.max())), 1e-12))
                pair[route]["spread_delta_px"] = stats_by_sigma[f"{after:.1f}"][route]["spread_sigma_px"] - stats_by_sigma[f"{before:.1f}"][route]["spread_sigma_px"]
                extra_rmse, radius_from, radius_to = support_transition_extra_rmse(before, after, route, fixture_name)
                pair[route]["support_transition_extra_rmse"] = extra_rmse
                pair[route]["support_radius_from_to"] = [radius_from, radius_to]
            adjacent.append({"sigma_from": before, "sigma_to": after, "radius_cpu_from_to": [radii(before)[0], radii(after)[0]], "radius_gpu_from_to": [radii(before)[1], radii(after)[1]], "routes": pair})
        transition_rows = []
        for sigma in TRANSITIONS:
            key = f"{sigma:.1f}"
            profiles = outputs_by_sigma[key]
            transition_rows.append({
                "sigma": sigma,
                "cpu_radius_px": radii(sigma)[0],
                "gpu_radius_px": radii(sigma)[1],
                "route_profiles": stats_by_sigma[key],
                "route_difference": diff_metrics(profiles["gpu_clamp_equation"], profiles["cpu_reflect101"]),
                "route_difference_full_image": diff_metrics(
                    np.outer(*route_image_profiles(kernels_by_sigma[key]["gpu_clamp_equation"], fixture_name)).astype(np.float32),
                    np.outer(*route_image_profiles(kernels_by_sigma[key]["cpu_reflect101"], fixture_name)).astype(np.float32),
                ),
            })
        def peak_steps(route: str, metric: str, count: int = 12) -> list[dict[str, object]]:
            rows = []
            for item in adjacent:
                value = item["routes"][route][metric]
                rows.append({"sigma_from": item["sigma_from"], "sigma_to": item["sigma_to"], "value": value, "cpu_radius_from_to": item["radius_cpu_from_to"], "gpu_radius_from_to": item["radius_gpu_from_to"]})
            return sorted(rows, key=lambda row: abs(row["value"]), reverse=True)[:count]
        support_transition_summary = {}
        for route in ("cpu_reflect101", "gpu_clamp_equation"):
            support_rows = [
                {
                    "sigma_from": item["sigma_from"],
                    "sigma_to": item["sigma_to"],
                    "value": item["routes"][route]["support_transition_extra_rmse"],
                    "radius_from_to": item["routes"][route]["support_radius_from_to"],
                    "ordinary_step_full_image_rmse": item["routes"][route]["full_image_rmse"],
                }
                for item in adjacent
                if item["routes"][route]["support_radius_from_to"][0] != item["routes"][route]["support_radius_from_to"][1]
            ]
            support_transition_summary[route] = {
                "support_transition_count": len(support_rows),
                "max_extra_rmse": max(support_rows, key=lambda row: row["value"]) if support_rows else None,
                "p95_extra_rmse": float(np.percentile([row["value"] for row in support_rows], 95)) if support_rows else None,
                "top_12_extra_rmse": sorted(support_rows, key=lambda row: row["value"], reverse=True)[:12],
                "largest_ordinary_step_rmse_when_support_unchanged": max(
                    (item["routes"][route]["full_image_rmse"] for item in adjacent if item["routes"][route]["support_radius_from_to"][0] == item["routes"][route]["support_radius_from_to"][1]),
                    default=0.0,
                ),
            }
        by_fixture[fixture_name] = {
            "fixture_sha256_float32_row_major": fixture_hashes[fixture_name],
            "shape_yx": [SIZE, SIZE],
            "input_value_range": [float(image.min()), float(image.max())],
            "adjacent_sigma_step_count": len(adjacent),
            "max_adjacent_change": {
                route: {
                    "rmse": peak_steps(route, "full_image_rmse")[0],
                    "peak_relative_change": peak_steps(route, "peak_relative_change")[0],
                    "spread_delta_px": peak_steps(route, "spread_delta_px")[0],
                }
                for route in ("cpu_reflect101", "gpu_clamp_equation")
            },
            "largest_adjacent_full_image_rmse_steps": {route: peak_steps(route, "full_image_rmse") for route in ("cpu_reflect101", "gpu_clamp_equation")},
            "largest_adjacent_peak_relative_steps": {route: peak_steps(route, "peak_relative_change") for route in ("cpu_reflect101", "gpu_clamp_equation")},
            "support_transition_extra_response": support_transition_summary,
            "cpu_gpu_full_image_difference_summary": {
                "max_rmse": max(parity_rows, key=lambda row: row["rmse"]),
                "p95_rmse": float(np.percentile([row["rmse"] for row in parity_rows], 95)),
                "max_abs": max(parity_rows, key=lambda row: row["max_abs"]),
                "worst_12_rmse": sorted(parity_rows, key=lambda row: row["rmse"], reverse=True)[:12],
            },
            "cpu_gpu_full_image_difference_all_sigma": parity_rows,
            "transition_samples": transition_rows,
            "metrics_scope": "linear float32 profiles; full 513x513 image RMSE between adjacent outputs, profile peak relative change, and mass centroid/spread/r50/r95; no display transform or perceptual threshold",
        }
        outputs_by_fixture[fixture_name] = outputs_by_sigma
    write_preview(outputs_by_fixture)
    report = {
        "study": "Gaussian sigma-step response continuity on opaque impulse and HDR line",
        "run_date": "2026-11-12",
        "evidence_class": "source-equation model only; not Artifact output, GPU dispatch, visual review, or perceptual acceptance",
        "source": "Artifact/src/Effects/Blur/GauusianBlur.cpp",
        "source_sha256": sha256(SOURCE),
        "runner": "experiments/blur-rd/gaussian_sigma_continuity.py",
        "runner_sha256": sha256(Path(__file__)),
        "base_model": "experiments/blur-rd/gaussian_boundary_energy_model.py",
        "base_model_sha256": sha256(Path(__file__).with_name("gaussian_boundary_energy_model.py")),
        "preview": "experiments/blur-rd/results/previews/gaussian_sigma_continuity_2026-11-12.png",
        "preview_sha256": sha256(PREVIEW),
        "conditions": {
            "dimensions_px": [SIZE, SIZE],
            "sigma_ui_min": 0.1,
            "sigma_ui_max": 64.0,
            "sigma_ui_step": 0.1,
            "sigma_sample_count": len(SAMPLES),
            "cpu": "OpenCV float32 GaussianBlur using source kernel width and REFLECT_101 border",
            "gpu_equation_mirror": "float32 normalized HLSL weights, ceil(3*sigma) radius capped at64, separable clamp sampling",
            "alpha": "opaque only; alpha behavior excluded",
            "color": "linear scene-referred float32 scalar replicated as spatial response; impulse amplitude1 and 3px line amplitude4",
            "fixtures": fixture_hashes,
            "candidate_step_metric": "adjacent 0.1 sigma outputs compared directly; support-transition extra response is actual output delta minus a counterfactual delta with both endpoints evaluated at their shared larger finite radius; numerical characterization only, no acceptance threshold",
        },
        "fixtures": by_fixture,
        "kernel_factor_fidelity_controls": model_fidelity_controls(fixtures),
        "interpretation": "Adjacent output differences include intended Gaussian strength change as well as discrete support effects; this study does not isolate perceptual discontinuity. Profiles are computed from source 1D kernels; for the centered impulse and horizontal line, full-image RMSE is derived using separability. Compare routes only as equation mirrors and do not infer product quality, runtime route selection, alpha correctness, or performance.",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(OUTPUT)
    for name, block in by_fixture.items():
        print(name)
        for route, metrics in block["max_adjacent_change"].items():
            print(f"  {route}: max RMSE step sigma {metrics['rmse']['sigma_from']:.1f}->{metrics['rmse']['sigma_to']:.1f}, value={metrics['rmse']['value']:.9g}; max peak-relative step {metrics['peak_relative_change']['sigma_from']:.1f}->{metrics['peak_relative_change']['sigma_to']:.1f}, value={metrics['peak_relative_change']['value']:.9g}")


if __name__ == "__main__":
    main()
