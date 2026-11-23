"""rd3b: block-wise noise floor, highlight rolloff, clamp radius, CC0 plates.

Corrected run registered in EVALUATION_PLAN.md section 11 after the first rd3
attempt showed that a whole-frame median detail estimate collapses to zero on
plates dominated by noise-free flat areas. Offline models only.
"""

from __future__ import annotations

import hashlib
import json
import platform
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

from detail_sharpen_lab import (
    LAB,
    _luma_detail,
    chart_metrics,
    contact_sheet,
    crop_zoom,
    gaussian,
    luma,
    make_chart,
    preview_rgb,
    product_usm_mirror,
    save_array,
    save_preview,
    sha256,
    smoothstep,
    step_edge_metrics,
)
from followup_soft_edge import make_soft_edge


OUT = LAB / "captures" / "rd3"
ROOT = LAB.parents[1]
NOISE_SIGMA = 0.004
PHOTOS = {
    "neon_streets": {
        "path": ROOT / "experiments" / "GlowLab" / "assets" / "cc0-neon-streets-1536.jpg",
        "source_sha256": "8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4",
        "resize": (960, 641),
        "provenance": "CC0 1.0, JINDONG H., Wikimedia Commons; see GlowLab/assets/SOURCES.md",
    },
    "street_light": {
        "path": ROOT / "experiments" / "GlowLab" / "assets" / "cc0-street-light-night.jpg",
        "source_sha256": "b01e663da4cada43f95b936d88ed377244680b56c41929d2d621d087d7224eb8",
        "resize": (432, 960),
        "provenance": "CC0 1.0, Reshmamohamed, Wikimedia Commons; see GlowLab/assets/SOURCES.md",
    },
}


def srgb_to_linear(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4).astype(np.float32)


def load_photo(entry: dict) -> tuple[np.ndarray, dict]:
    image = Image.open(entry["path"]).convert("RGB").resize(entry["resize"], Image.LANCZOS)
    encoded = np.asarray(image, np.float32) / 255.0
    rgb = srgb_to_linear(encoded)
    alpha = np.ones(rgb.shape[:2] + (1,), np.float32)
    rgba = np.concatenate((rgb, alpha), axis=2)
    return rgba, {"source_file": entry["path"].name,
                  "source_sha256": entry["source_sha256"],
                  "source_sha256_verified": sha256(entry["path"]),
                  "resize": list(entry["resize"]),
                  "provenance": entry["provenance"],
                  "encoding": "sRGB JPEG decoded, sRGB EOTF assumed, ICC not applied"}


def noise_sigma_blockwise(rgba: np.ndarray, block: int = 16,
                          percentile: float = 10.0) -> float:
    """10th-percentile block MAD of the high-pass luma: a noise-floor estimate."""
    lum = luma(rgba[..., :3])
    hp = lum - gaussian(lum, 1.0)
    height, width = hp.shape
    values = []
    for y in range(0, height - block + 1, block):
        for x in range(0, width - block + 1, block):
            patch = hp[y:y + block, x:x + block]
            median = np.median(patch)
            values.append(1.4826 * float(np.median(np.abs(patch - median))))
    return float(max(np.percentile(values, percentile), 1e-6))


def clamp_detail(rgb: np.ndarray, detail: np.ndarray, radius: int) -> np.ndarray:
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    low = cv2.erode(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    result = np.clip(rgb + detail[..., None], low, high)
    return np.maximum(result, 0.0)


def luma_guard(rgb: np.ndarray, detail: np.ndarray, radius: int) -> np.ndarray:
    """Clamp the luma delta to the source's local luma extrema, keep chroma."""
    lum = luma(rgb)
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    low = cv2.erode(lum, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(lum, kernel, borderType=cv2.BORDER_REFLECT)
    guarded = np.clip(lum + detail, low, high) - lum
    return np.maximum(rgb + guarded[..., None], 0.0)


def rolloff_factor(rgb: np.ndarray, detail: np.ndarray, start: float) -> np.ndarray:
    predicted = np.maximum(rgb + detail[..., None], 0.0)
    peak = predicted.max(axis=2)
    return 1.0 - smoothstep(start, 1.0, peak)


def make_models() -> dict:
    def noise_rel(rgba, p):
        threshold = p["k"] * p["sigma_hat"]
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], threshold, threshold)
        result = np.maximum(rgba[..., :3] + detail[..., None], 0.0)
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    def per_channel_clamp(rgba, p):
        threshold = p["k"] * p["sigma_hat"]
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], threshold, threshold)
        detail = detail * rolloff_factor(rgba[..., :3], detail, p["rolloff_start"])
        result = clamp_detail(rgba[..., :3], detail, max(1, int(np.ceil(3 * p["sigma"]))))
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    def luma_guard_combo(rgba, p):
        threshold = p["k"] * p["sigma_hat"]
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], threshold, threshold)
        detail = detail * rolloff_factor(rgba[..., :3], detail, p["rolloff_start"])
        radius = max(1, int(np.ceil(3 * p["sigma"])))
        result = luma_guard(rgba[..., :3], detail, radius)
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    def halo_rolloff(rgba, p):
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], 0.0, 0.0)
        detail = detail * rolloff_factor(rgba[..., :3], detail, p["rolloff_start"])
        result = clamp_detail(rgba[..., :3], detail,
                              max(1, int(np.ceil(3 * p["sigma"]))))
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    def halo_limited_t0(rgba, p):
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], 0.0, 0.0)
        result = clamp_detail(rgba[..., :3], detail,
                              max(1, int(np.ceil(3 * p["sigma"]))))
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    def clamp_radius(rgba, p):
        detail = _luma_detail(rgba[..., :3], p["sigma"], p["amount"], 0.0, 0.0)
        result = clamp_detail(rgba[..., :3], detail, max(1, int(p["radius"])))
        return np.concatenate((result, rgba[..., 3:4]), axis=2)

    return {"mirror": product_usm_mirror,
            "noise_rel_k1": noise_rel,
            "noise_rel_k2": noise_rel,
            "noise_rel_k3": noise_rel,
            "halo_limited_t0": halo_limited_t0,
            "halo_noise_rel_k3_rolloff": per_channel_clamp,
            "halo_rolloff": halo_rolloff,
            "luma_guard_combo": luma_guard_combo,
            "clamp_radius": clamp_radius}


def settings_for(img: np.ndarray, **extra) -> dict:
    settings = {"amount": 1.0, "sigma": 1.0, "threshold": 0.0, "knee": 0.0,
                "sigma_hat": noise_sigma_blockwise(img), "k": 3.0,
                "rolloff_start": 0.85}
    settings.update(extra)
    return settings


def auto_rois(rgba: np.ndarray) -> tuple[dict, list[dict]]:
    lum = luma(rgba[..., :3])
    height, width = lum.shape
    cells = []
    for gy in range(3):
        for gx in range(4):
            x0, x1 = int(gx * width / 4), int((gx + 1) * width / 4)
            y0, y1 = int(gy * height / 3), int((gy + 1) * height / 3)
            cell = lum[y0:y1, x0:x1]
            hp = cell - gaussian(cell, 1.0)
            cells.append({"name": f"g{gy}{gx}", "box": [x0, y0, x1, y1],
                          "mean": float(cell.mean()), "variance": float(cell.var()),
                          "highpass_energy": float(np.mean(hp ** 2))})
    picks = {
        "bright": max(cells, key=lambda c: c["mean"]),
        "dark": min(cells, key=lambda c: c["mean"]),
        "flat": min(cells, key=lambda c: c["variance"]),
        "detail": max(cells, key=lambda c: c["highpass_energy"]),
    }
    return {name: cell["box"] for name, cell in picks.items()}, cells


def photo_metrics(src: np.ndarray, out: np.ndarray, rois: dict, sigma: float) -> dict:
    src_rgb, out_rgb = src[..., :3], out[..., :3]
    src_l, out_l = luma(src_rgb), luma(out_rgb)
    radius = max(1, int(np.ceil(3.0 * sigma)))
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    low = cv2.erode(src_l, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(src_l, kernel, borderType=cv2.BORDER_REFLECT)
    beyond = (out_l > high + 1e-3) | (out_l < low - 1e-3)

    regions = {}
    for name, box in rois.items():
        x0, y0, x1, y1 = box
        a, b = src_l[y0:y1, x0:x1], out_l[y0:y1, x0:x1]
        hp_a = a - gaussian(a, 1.0)
        hp_b = b - gaussian(b, 1.0)
        regions[name] = {
            "box": box,
            "luma_mean_src": float(a.mean()),
            "luma_mean_out": float(b.mean()),
            "noise_std_ratio": float(b.std() / max(a.std(), 1e-9)),
            "highpass_energy_ratio": float(np.mean(hp_b ** 2) / max(np.mean(hp_a ** 2), 1e-15)),
            "extrema_exceedance_fraction": float(beyond[y0:y1, x0:x1].mean()),
        }

    fringe_src = np.stack((src_rgb[..., 0] - src_rgb[..., 1],
                           src_rgb[..., 2] - src_rgb[..., 1]), axis=2)
    fringe_out = np.stack((out_rgb[..., 0] - out_rgb[..., 1],
                           out_rgb[..., 2] - out_rgb[..., 1]), axis=2)
    chroma = np.abs(fringe_out - fringe_src)
    return {
        "regions": regions,
        "frame_new_clip_fraction": float(np.any(out_rgb > 1.0, axis=2).mean()),
        "frame_extrema_exceedance_fraction": float(beyond.mean()),
        "chroma_delta_mean_codes": float(chroma.mean() * 255.0),
        "chroma_delta_max_codes": float(chroma.max() * 255.0),
        "luma_p99": float(np.percentile(out_l, 99)),
        "luma_max": float(out_l.max()),
        "alpha_max_abs_diff": float(np.abs(src[..., 3] - out[..., 3]).max()),
    }


def array_hash(array: np.ndarray) -> str:
    return hashlib.sha256(np.ascontiguousarray(array, dtype=np.float32).tobytes()).hexdigest()


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    models = make_models()

    chart, chart_meta = make_chart(960, 540, np.random.default_rng(20261114))
    noise = np.random.default_rng(20261114).normal(
        0.0, NOISE_SIGMA, chart[..., :3].shape).astype(np.float32)
    noisy = np.concatenate((np.maximum(chart[..., :3] + noise, 0.0), chart[..., 3:4]), axis=2)
    soft, soft_meta = make_soft_edge()

    save_array(OUT / "source__chart_960_noisy.npz", noisy)
    save_preview(OUT / "source__chart_960_noisy.preview.png", noisy)

    model_extras = {
        "mirror": {},
        "noise_rel_k1": {"k": 1.0},
        "noise_rel_k2": {"k": 2.0},
        "noise_rel_k3": {"k": 3.0},
        "halo_limited_t0": {"k": 0.0},
        "halo_rolloff": {"k": 0.0},
        "halo_noise_rel_k3_rolloff": {"k": 3.0},
        "luma_guard_combo": {"k": 3.0},
    }

    chart_cases = []
    for plate_name, image in (("chart_960", chart), ("chart_960_noisy", noisy)):
        for name, extra in model_extras.items():
            settings = {"amount": 1.0, "sigma": 1.0, "threshold": 0.0, "knee": 0.0}
            if name != "mirror":
                settings = settings_for(image, **extra)
            result = models[name](image, settings)
            metrics = chart_metrics(image, result, chart_meta, settings)
            record = {"plate": plate_name, "model": name, "settings": settings,
                      "metrics": metrics, "output_sha256": array_hash(result)}
            save_preview(OUT / f"{plate_name}__{name}.preview.png", result)
            if name in ("mirror", "luma_guard_combo"):
                npz = OUT / f"{plate_name}__{name}.npz"
                save_array(npz, result)
                record["output_npz"] = npz.name
            chart_cases.append(record)

    soft_cases = []
    soft_panels = [("source", preview_rgb(soft))]
    for name in ("mirror", "noise_rel_k3", "halo_limited_t0", "halo_rolloff",
                 "luma_guard_combo"):
        settings = settings_for(soft)
        result = models[name](soft, settings)
        step = step_edge_metrics(soft[..., :3], result[..., :3], soft_meta, settings["sigma"])
        soft_cases.append({"model": name, "settings": settings, "step_edge": step,
                           "rise_gain": 1.0 - step["rise_20_80_px"] / step["source_rise_20_80_px"]})
        soft_panels.append((name, preview_rgb(result)))

    radius_sweep = []
    for radius in (1, 2, 3, 6, 12):
        settings = settings_for(soft, radius=float(radius))
        result = models["clamp_radius"](soft, settings)
        step = step_edge_metrics(soft[..., :3], result[..., :3], soft_meta, 1.0)
        radius_sweep.append({
            "radius_px": radius,
            "rise_gain": 1.0 - step["rise_20_80_px"] / step["source_rise_20_80_px"],
            "halo_fraction": step["halo_fraction_outside_transition"],
            "overshoot_linear": step["overshoot_linear"],
        })

    photo_records = {}
    for plate_name, entry in PHOTOS.items():
        photo, provenance = load_photo(entry)
        rois, cells = auto_rois(photo)
        src_npz = OUT / f"photo_{plate_name}__source.npz"
        save_array(src_npz, photo)
        save_preview(OUT / f"photo_{plate_name}__source.preview.png", photo)

        settings = settings_for(photo)
        results, cases = {}, []
        for name in ("mirror", "noise_rel_k3", "halo_noise_rel_k3_rolloff",
                     "halo_rolloff", "luma_guard_combo"):
            result = models[name](photo, settings)
            results[name] = result
            record = {"model": name, "settings": settings,
                      "metrics": photo_metrics(photo, result, rois, settings["sigma"]),
                      "output_sha256": array_hash(result)}
            if name in ("mirror", "luma_guard_combo"):
                npz = OUT / f"photo_{plate_name}__{name}.npz"
                save_array(npz, result)
                record["output_npz"] = npz.name
            cases.append(record)

        panels = [("source", preview_rgb(photo)),
                  ("mirror", preview_rgb(results["mirror"])),
                  ("luma_guard_combo", preview_rgb(results["luma_guard_combo"]))]
        crops = []
        for label, box in (("detail", rois["detail"]), ("bright", rois["bright"]),
                           ("flat", rois["flat"])):
            for name in ("source", "mirror", "luma_guard_combo"):
                image = photo if name == "source" else results[name]
                crops.append((f"{name} {label}", crop_zoom(image, box, 3)))
        contact_sheet(OUT / f"photo_{plate_name}.contact.png", panels, 0.6)
        contact_sheet(OUT / f"photo_{plate_name}.roi-crops.png", crops, 1.0)

        photo_records[plate_name] = {
            "provenance": provenance,
            "source_npz": src_npz.name,
            "source_sha256": sha256(src_npz),
            "rois": rois, "grid_cells": cells,
            "sigma_hat": settings["sigma_hat"],
            "cases": cases,
        }

    contact_sheet(OUT / "soft_edge.contact.png", soft_panels, 0.6)

    timings = {}
    for name, extra in (("mirror", {}), ("luma_guard_combo", {"k": 3.0})):
        settings = {"amount": 1.0, "sigma": 1.0, "threshold": 0.0, "knee": 0.0}
        if name != "mirror":
            settings = settings_for(chart, **extra)
        samples = []
        for _ in range(3):
            start = time.perf_counter()
            models[name](chart, settings)
            samples.append((time.perf_counter() - start) * 1000.0)
        timings[name] = float(np.median(samples))

    by_plate = {}
    for case in chart_cases:
        by_plate.setdefault(case["plate"], {})[case["model"]] = case["metrics"]
    soft_by_model = {case["model"]: case for case in soft_cases}
    noisy_rise_gain = 1.0 - (
        by_plate["chart_960_noisy"]["noise_rel_k3"]["step_edge"]["rise_20_80_px"] /
        by_plate["chart_960_noisy"]["noise_rel_k3"]["step_edge"]["source_rise_20_80_px"])

    gates = {
        "N1_noise_rel_k3": {
            "noise_low_std_ratio_le_1.15":
                by_plate["chart_960_noisy"]["noise_rel_k3"]["noise"]["noise_low"]["std_ratio"] <= 1.15,
            "noisy_plate_rise_gain_ge_10pct": noisy_rise_gain >= 0.10,
        },
        "N2_halo_rolloff": {
            "highlight_new_clip_le_0.1pct":
                by_plate["chart_960"]["halo_rolloff"]["highlight_new_clip_fraction"] <= 0.001,
            "soft_rise_gain_ge_90pct_of_clamp":
                soft_by_model["halo_rolloff"]["rise_gain"]
                >= 0.90 * soft_by_model["halo_limited_t0"]["rise_gain"],
        },
        "R1_luma_guard_combo_soft_edge": {
            "halo_fraction_le_1pct":
                soft_by_model["luma_guard_combo"]["step_edge"][
                    "halo_fraction_outside_transition"] <= 0.01,
            "overshoot_le_0.004_linear":
                soft_by_model["luma_guard_combo"]["step_edge"]["overshoot_linear"] <= 0.004,
            "rise_gain_ge_15pct": soft_by_model["luma_guard_combo"]["rise_gain"] >= 0.15,
        },
    }
    photo_gate = {}
    for plate_name, record in photo_records.items():
        by_model = {case["model"]: case["metrics"] for case in record["cases"]}
        candidate = by_model["luma_guard_combo"]
        photo_gate[plate_name] = {
            "new_clip_le_0.05pct": candidate["frame_new_clip_fraction"] <= 0.0005,
            "zero_extrema_exceedance_in_bright_dark_flat": all(
                candidate["regions"][key]["extrema_exceedance_fraction"] == 0.0
                for key in ("bright", "dark", "flat")),
            "alpha_unchanged": candidate["alpha_max_abs_diff"] == 0.0,
            "chroma_le_mirror": candidate["chroma_delta_mean_codes"]
            <= by_model["mirror"]["chroma_delta_mean_codes"],
        }
    gates["N3_photographic"] = photo_gate

    manifest = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "lab": "DetailSharpeningLab",
        "round": "rd3b",
        "first_attempt_manifest": "manifest_first_attempt.json",
        "environment": {"python": sys.version.split()[0],
                        "platform": platform.platform(),
                        "numpy": np.__version__,
                        "opencv": cv2.__version__,
                        "pillow": Image.__version__},
        "sigma_hat_definition": "1.4826 * median(|hp - median(hp)|) per 16x16 block, "
                                "10th percentile across blocks (noise floor)",
        "noisy_plate": {"noise_sigma_linear": NOISE_SIGMA, "seed": 20261114,
                        "note": "Gaussian noise applied to every pixel of chart_960"},
        "chart_cases": chart_cases,
        "soft_edge_cases": soft_cases,
        "radius_sweep": radius_sweep,
        "photos": photo_records,
        "raw_array_policy": "float32 .npz retained only for mirror and luma_guard_combo; "
                            "all models are measured from the in-memory float32 result and "
                            "recorded as metrics plus previews",
        "timings_ms_960x540_median_of_3": timings,
        "gates": gates,
        "evaluation_status": {
            "photographic_material": "measured on two CC0 plates; response evidence only",
            "hdr": "unverified (8-bit SDR source)",
            "gpu_backend_parity": "unverified",
            "final_viewer_and_render_queue": "unverified",
            "allocation_profile": "unverified",
            "independent_visual_review": "not performed",
        },
        "limitations": [
            "Offline models, no product capture.",
            "Noise floor is estimated from the quietest 10% of blocks; mixed-grain and "
            "compressed sources are not covered.",
            "8-bit sRGB JPEG sources; linearization cannot recover HDR or clipped detail.",
            "Extrema exceedance is a spatial halo proxy, not a perceptual measurement.",
            "No GPU, Viewer, real video, motion-compensated temporal, or allocation evidence.",
        ],
    }
    manifest_path = OUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8", newline="\n")
    (OUT / "manifest.sha256").write_text(sha256(manifest_path) + "  manifest.json\n",
                                        encoding="utf-8", newline="\n")

    print(f"wrote {manifest_path}")
    for plate, cases in by_plate.items():
        for name, m in cases.items():
            print(f"  {plate:16s} {name:28s} hlclip={m['highlight_new_clip_fraction']*100:5.2f}% "
                  f"clip={m['frame_new_clip_fraction']*100:5.2f}% "
                  f"nlow={m['noise']['noise_low']['std_ratio']:.3f}")
    for case in soft_cases:
        step = case["step_edge"]
        print(f"  soft {case['model']:24s} gain={case['rise_gain']:+.3f} "
              f"halo={step['halo_fraction_outside_transition']*100:5.2f}% "
              f"ovs={step['overshoot_linear']:+.4f}")
    for row in radius_sweep:
        print(f"  radius {row['radius_px']:2d} gain={row['rise_gain']:+.3f} "
              f"halo={row['halo_fraction']*100:.2f}%")
    for plate_name, record in photo_records.items():
        print(f"  photo {plate_name} sigma_hat={record['sigma_hat']:.6f}")
        for case in record["cases"]:
            m = case["metrics"]
            print(f"    {case['model']:28s} clip={m['frame_new_clip_fraction']*100:6.3f}% "
                  f"exceed={m['frame_extrema_exceedance_fraction']*100:6.3f}% "
                  f"chroma={m['chroma_delta_mean_codes']:5.2f} "
                  f"flat_noise={m['regions']['flat']['noise_std_ratio']:.3f}")
    print("  gates", json.dumps(gates, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
