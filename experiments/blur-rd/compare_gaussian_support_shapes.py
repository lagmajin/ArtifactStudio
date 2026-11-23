"""Measure CPU/resident Gaussian support differences on fixed opaque fixtures."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
BUNDLE = Path(__file__).resolve().parent / "fixtures" / "gaussian-r1"
OUT = Path(__file__).resolve().parent / "results" / "gaussian_support_shape_comparison_2026-11-12.json"
PREVIEW = Path(__file__).resolve().parent / "results" / "previews" / "gaussian_support_shape_comparison.png"
ANALYZER_PATH = Path(__file__).resolve().parent / "analyze_gaussian_fixture_bundle.py"
spec = importlib.util.spec_from_file_location("gaussian_fixture_analyzer", ANALYZER_PATH)
assert spec and spec.loader
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def profile_metrics_for_error(values: np.ndarray) -> dict[str, float | int]:
    absolute = np.abs(values.astype(np.float64)).reshape(-1)
    if not absolute.size:
        return {"components": 0, "rmse": 0.0, "p95": 0.0, "max": 0.0,
                "components_over_1_255": 0}
    return {
        "components": int(absolute.size),
        "rmse": float(np.sqrt(np.mean(absolute * absolute))),
        "p95": float(np.quantile(absolute, 0.95)),
        "max": float(absolute.max()),
        "components_over_1_255": int(np.count_nonzero(absolute > 1.0 / 255.0)),
    }


def profile_metrics(profile: np.ndarray, origin: int) -> dict[str, float | int]:
    p = profile.astype(np.float64)
    energy = float(p.sum())
    x = np.arange(len(p), dtype=np.float64)
    centroid = float(np.sum(p * x) / energy) if energy else float(origin)
    variance = float(np.sum(p * (x - centroid) ** 2) / energy) if energy else 0.0
    radial_order = np.argsort(np.abs(x - origin), kind="stable")
    cumulative = np.cumsum(np.maximum(p[radial_order], 0.0))
    r95_index = min(int(np.searchsorted(cumulative, 0.95 * energy)), len(p) - 1)
    r95 = int(abs(x[radial_order[r95_index]] - origin)) if energy else 0
    return {
        "positive_energy": energy,
        "centroid_px": centroid,
        "centroid_offset_from_authored_center_px": centroid - origin,
        "effective_sigma_px": float(np.sqrt(max(0.0, variance))),
        "r95_from_authored_center_px": r95,
        "peak": float(np.max(p)) if len(p) else 0.0,
    }


def edge_spread_10_90(profile: np.ndarray, alpha_scale: float = 1.0) -> dict[str, float | None]:
    # Logical red in the fixture transitions from 1.4 on the left to .06 on the right.
    high, low = 1.4 * alpha_scale, 0.06 * alpha_scale
    normalized = (high - profile.astype(np.float64)) / (high - low)

    def crossing(target: float) -> float | None:
        indices = np.flatnonzero(normalized >= target)
        if not len(indices):
            return None
        i = int(indices[0])
        if i == 0:
            return 0.0
        a, b = normalized[i - 1], normalized[i]
        return float(i if b == a else (i - 1) + (target - a) / (b - a))

    x10, x90 = crossing(0.1), crossing(0.9)
    return {"x10_px": x10, "x90_px": x90,
            "edge_spread_10_90_px": abs(x90 - x10) if x10 is not None and x90 is not None else None}


def edge_width(profile: np.ndarray) -> dict[str, float | None]:
    # Logical red transitions from 1.4 to .06 in linear numeric values.
    high, low = 1.4, 0.06
    normalized = (high - profile.astype(np.float64)) / (high - low)

    def crossing(target: float) -> float | None:
        indices = np.flatnonzero(normalized >= target)
        if not len(indices):
            return None
        i = int(indices[0])
        if i == 0:
            return 0.0
        before, after = normalized[i - 1], normalized[i]
        if after == before:
            return float(i)
        return float((i - 1) + (target - before) / (after - before))

    x10, x90 = crossing(0.1), crossing(0.9)
    return {"x10_px": x10, "x90_px": x90,
            "edge_spread_10_90_px": abs(x90 - x10) if x10 is not None and x90 is not None else None}


def main() -> None:
    manifest_path = BUNDLE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    fixtures = {}
    entries = {}
    base_names = {}
    alpha_scales = {}
    for name in ("opaque_hdr_color_edge", "opaque_three_pixel_detail"):
        entry = next(item for item in manifest["fixtures"] if item["name"] == name)
        path = ROOT / entry["path"]
        if sha256(path) != entry["sha256"]:
            raise SystemExit(f"fixture hash mismatch: {name}")
        image = np.load(path, allow_pickle=False)
        fixtures[name] = image
        entries[name] = entry
        base_names[name] = name
        alpha_scales[name] = 1.0
        half_name = f"{name}_half_alpha"
        half = image.copy()
        half[..., :3] *= 0.5
        half[..., 3] *= 0.5
        fixtures[half_name] = half
        entries[half_name] = entry
        base_names[half_name] = name
        alpha_scales[half_name] = 0.5

    sigmas = (16.0, 21.3333333333, 32.0, 48.0, 64.0)
    cases = []
    preview_items = []
    for name, image in fixtures.items():
        center = image.shape[1] // 2
        for sigma in sigmas:
            straight_input = analyzer.straight_from_premultiplied(image)
            cpu_straight = analyzer.blur_cpu_wrapper(straight_input, sigma)
            resident_straight = analyzer.blur_standalone_gpu_wrapper(straight_input, sigma)
            cpu = cpu_straight.copy()
            cpu[..., :3] *= cpu[..., 3:4]
            resident = resident_straight.copy()
            resident[..., :3] *= resident[..., 3:4]
            cpu_radius = len(analyzer.gaussian_weights(sigma, resident=False)) // 2
            resident_radius = len(analyzer.gaussian_weights(sigma, resident=True)) // 2
            # Region around the authored feature; edge fixture spans full height.
            if base_names[name] == "opaque_hdr_color_edge":
                roi = (slice(0, image.shape[0]), slice(max(0, center - 3 * cpu_radius), min(image.shape[1], center + 3 * cpu_radius)))
                profile_y = center
            else:
                roi = (slice(max(0, center - 3 * cpu_radius), min(image.shape[0], center + 3 * cpu_radius)),
                       slice(max(0, center - 3 * cpu_radius), min(image.shape[1], center + 3 * cpu_radius)))
                profile_y = center
            delta = cpu[roi] - resident[roi]
            backgrounds = {
                "black": np.zeros(3, dtype=np.float32),
                "gray_18": np.full(3, 0.18, dtype=np.float32),
                "white": np.ones(3, dtype=np.float32),
                "green_bgra": np.array((0.08, 0.45, 0.12), dtype=np.float32),
            }
            exposure_composites = {}
            for bg_name, bg_bgra in backgrounds.items():
                cpu_comp = cpu[..., :3] + bg_bgra * (1.0 - cpu[..., 3:4])
                gpu_comp = resident[..., :3] + bg_bgra * (1.0 - resident[..., 3:4])
                by_exposure = {}
                for ev in (0, 2, 4):
                    gain = float(2 ** ev)
                    display_delta = (
                        analyzer.linear_to_srgb(cpu_comp * gain)
                        - analyzer.linear_to_srgb(gpu_comp * gain)
                    )
                    by_exposure[f"plus_{ev}EV"] = {
                        "full_image_srgb_difference": profile_metrics_for_error(display_delta),
                        "feature_roi_srgb_difference": profile_metrics_for_error(display_delta[roi]),
                    }
                exposure_composites[bg_name] = by_exposure
            channel_profiles = {}
            for physical_index, logical_name, authored_x in ((2, "R", center), (1, "G", center), (0, "B", center)):
                cpu_profile = cpu[profile_y, :, physical_index]
                gpu_profile = resident[profile_y, :, physical_index]
                channel_profiles[logical_name] = {
                    "profile_rmse": float(np.sqrt(np.mean((cpu_profile.astype(np.float64) - gpu_profile.astype(np.float64)) ** 2))),
                    "profile_max_abs": float(np.max(np.abs(cpu_profile.astype(np.float64) - gpu_profile.astype(np.float64)))),
                }
                if base_names[name] == "opaque_hdr_color_edge":
                    if logical_name == "R":
                        channel_profiles[logical_name]["cpu_edge_spread_10_90"] = edge_spread_10_90(cpu_profile, alpha_scales[name])
                        channel_profiles[logical_name]["resident_edge_spread_10_90"] = edge_spread_10_90(gpu_profile, alpha_scales[name])
                else:
                    channel_profiles[logical_name]["cpu_energy_profile"] = profile_metrics(cpu_profile, authored_x)
                    channel_profiles[logical_name]["resident_energy_profile"] = profile_metrics(gpu_profile, authored_x)
            cases.append({
                "fixture": name,
                "fixture_base": base_names[name],
                "fixture_sha256": entries[name]["sha256"],
                "derived_global_alpha_scale": alpha_scales[name],
                "alpha_association": "opaque" if alpha_scales[name] == 1.0 else "premultiplied, constant alpha=.5",
                "sigma_px": sigma,
                "dimensions_px": list(image.shape[1::-1]),
                "cpu_kernel_width_px": 2 * cpu_radius + 1,
                "cpu_radius_px": cpu_radius,
                "resident_radius_px": resident_radius,
                "effective_sigma_cap_active": resident_radius < int(np.ceil(3.0 * min(64.0, sigma))),
                "authored_roi_xyxy": [roi[1].start, roi[0].start, roi[1].stop, roi[0].stop],
                "authored_roi_rgba_rmse": float(np.sqrt(np.mean(delta.astype(np.float64) ** 2))),
                "authored_roi_rgba_max_abs": float(np.max(np.abs(delta))),
                "channel_center_row_profiles": channel_profiles,
                "cpu_alpha_range": [float(cpu[..., 3].min()), float(cpu[..., 3].max())],
                "resident_alpha_range": [float(resident[..., 3].min()), float(resident[..., 3].max())],
                "cpu_nonfinite_components": int(np.size(cpu) - np.count_nonzero(np.isfinite(cpu))),
                "resident_nonfinite_components": int(np.size(resident) - np.count_nonzero(np.isfinite(resident))),
                "exposure_composites": exposure_composites,
            })
            if sigma in (16.0, 64.0):
                preview_items.append((name, sigma, image, cpu, resident))

    result = {
        "schema": "artifact.blur-rd.gaussian-support-shape-comparison.v1",
        "runner": Path(__file__).relative_to(ROOT).as_posix(),
        "runner_sha256": sha256(Path(__file__).resolve()),
        "analyzer_sha256": sha256(ANALYZER_PATH),
        "fixture_manifest_sha256": sha256(manifest_path),
        "evidence_class": "fixed synthetic fixture source-equation comparison; not Artifact capture",
        "fixture_contract": "float32 linear numeric BGRA; each hash-verified opaque fixture also has a derived premultiplied global-alpha=.5 variant; each input is unassociated before both routes and output is reassociated for visible comparison",
        "equations": "CPU wrapper OpenCV Reflect101 versus current GaussianBlurCS HLSL wrapper equation (clamp sampling, per-pass RGB*alpha accumulation/unassociation, radius cap64); visible comparisons reassociate both outputs",
        "sigma_values_px": sigmas,
        "measurements": cases,
        "preview": PREVIEW.relative_to(ROOT).as_posix(),
        "scope_limit": "No runtime dispatch/route eligibility, active view transform, visual acceptance, or performance. HDR edge is vertical, not slanted; preview is simple clamped sRGB OETF (edge 0EV; detail +8EV display-only). Composite variants scale premultiplied color and alpha together and do not model an alpha edge. The previous output compared direct RGBA convolution against the current HLSL route and is superseded by this regeneration.",
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")

    tile_w, tile_h, label_h = 320, 160, 22
    canvas = Image.new("RGB", (tile_w * 2, (tile_h + label_h) * len(preview_items)), (28, 31, 36))
    draw = ImageDraw.Draw(canvas)
    for row, (name, sigma, image, cpu, resident) in enumerate(preview_items):
        center = image.shape[1] // 2
        y = image.shape[0] // 2
        radius = min(192, int(np.ceil(3 * sigma)))
        x0, x1 = max(0, center - radius), min(image.shape[1], center + radius + 1)
        if name == "opaque_hdr_color_edge":
            y0, y1 = max(0, y - 64), min(image.shape[0], y + 64)
        else:
            y0, y1 = max(0, y - 64), min(image.shape[0], y + 64)
        for col, (label, output) in enumerate((("CPU", cpu), ("resident equation", resident))):
            preview_ev = 8 if name.startswith("opaque_three_pixel_detail") else 0
            gain = float(2 ** preview_ev)
            rgb = analyzer.linear_to_srgb(output[y0:y1, x0:x1, :3][..., ::-1] * gain)
            rgb8 = np.asarray(np.rint(np.clip(rgb, 0.0, 1.0) * 255.0), dtype=np.uint8)
            tile = Image.fromarray(rgb8, mode="RGB").resize((tile_w, tile_h), Image.Resampling.NEAREST)
            top = row * (tile_h + label_h) + label_h
            canvas.paste(tile, (col * tile_w, top))
            draw.text((col * tile_w + 4, row * (tile_h + label_h) + 3), f"{name}, sigma={sigma:g}, +{preview_ev}EV: {label}", fill=(245, 245, 245))
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(PREVIEW)
    print(OUT)
    print(PREVIEW)


if __name__ == "__main__":
    main()
