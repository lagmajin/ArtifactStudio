"""rd2 follow-up: soft-edge sharpening, clamping, threshold, and straight alpha.

Motivated by rd1 findings (registered before this run in EVALUATION_PLAN.md):

1. On a 1 px hard step, output clamped to source local extrema showed zero
   measurable edge steepening. A pixel-sharp edge has no headroom to sharpen
   without overshoot, so the interesting case is a realistic soft edge.
2. Luma detail applied without alpha weighting increased the composite error
   at a straight-alpha edge (26 codes on black vs 16 for the product mirror).

rd2 also separates the threshold effect from the clamp effect with a threshold-0
ablation, because the rd1 preset threshold (0.02) alone suppressed most of the
soft-edge detail.

This script reuses the rd1 fixtures, models, and metric definitions. It does
not change rd1 thresholds and does not call Artifact binaries.
"""

from __future__ import annotations

import json
import platform
import sys
from datetime import datetime, timezone

import cv2
import numpy as np
from PIL import Image

from detail_sharpen_lab import (
    LAB,
    _luma_detail,
    alpha_metrics,
    contact_sheet,
    crop_zoom,
    gaussian,
    halo_limited,
    luma,
    luma_usm_soft,
    make_alpha_edge,
    make_grating_sequence,
    preview_rgb,
    product_usm_mirror,
    save_array,
    save_preview,
    sha256,
    smoothstep,
    step_edge_metrics,
)


OUT = LAB / "captures" / "rd2"
SOFT_EDGE_SIGMA = 1.2


def make_soft_edge(width: int = 960, height: int = 540) -> tuple[np.ndarray, dict]:
    """Blurred step (declared synthetic PSF) with flat noise fields on both sides."""
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    px = xx + 0.5
    edge = 0.50 * width
    transition = smoothstep(edge - 2.0 * SOFT_EDGE_SIGMA, edge + 2.0 * SOFT_EDGE_SIGMA, px)
    value = 0.03 + 0.19 * transition
    rgb = np.repeat(value[..., None], 3, axis=2).astype(np.float32)
    rgb[:, : int(0.04 * width)] = 0.02
    rgb[:, int(0.96 * width):] = 0.02

    rng = np.random.default_rng(20261114)
    low = (xx > 0.06 * width) & (xx < 0.30 * width) & (yy > 0.20 * height) & (yy < 0.45 * height)
    high = (xx > 0.70 * width) & (xx < 0.94 * width) & (yy > 0.55 * height) & (yy < 0.80 * height)
    rgb[low] += rng.normal(0.0, 0.004, (int(low.sum()), 3)).astype(np.float32)
    rgb[high] += rng.normal(0.0, 0.020, (int(high.sum()), 3)).astype(np.float32)

    alpha = np.ones((height, width, 1), np.float32)
    rgba = np.concatenate((np.maximum(rgb, 0.0), alpha), axis=2)
    meta = {
        "edge_x_px": round(edge, 3),
        "soft_edge_sigma_px": SOFT_EDGE_SIGMA,
        "rois": {
            "step_edge": [int(0.40 * width), int(0.15 * height),
                          int(0.60 * width), int(0.35 * height)],
            "noise_low": [int(0.06 * width), int(0.20 * height),
                          int(0.30 * width), int(0.45 * height)],
            "noise_high": [int(0.70 * width), int(0.55 * height),
                           int(0.94 * width), int(0.80 * height)],
        },
    }
    return rgba, meta


def alpha_weighted_limited(rgba: np.ndarray, p: dict) -> np.ndarray:
    """Halo-limited luma detail scaled by straight alpha; alpha is never modified."""
    rgb = rgba[..., :3]
    alpha = rgba[..., 3:4]
    detail = _luma_detail(rgb, p["sigma"], p["amount"], p["threshold"], p["knee"])
    radius = max(1, int(np.ceil(3.0 * p["sigma"])))
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    low = cv2.erode(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(rgb, kernel, borderType=cv2.BORDER_REFLECT)
    result = np.clip(rgb + detail[..., None] * alpha, low, high)
    result = np.maximum(result, 0.0)
    return np.concatenate((result.astype(np.float32), alpha), axis=2)


def detail_capped(rgba: np.ndarray, p: dict) -> np.ndarray:
    """Luma USM with the added detail soft-limited by a fraction of local contrast."""
    rgb = rgba[..., :3]
    detail = _luma_detail(rgb, p["sigma"], p["amount"], p["threshold"], p["knee"])
    radius = max(1, int(np.ceil(3.0 * p["sigma"])))
    k = 2 * radius + 1
    kernel = np.ones((k, k), np.uint8)
    lum = luma(rgb)
    low = cv2.erode(lum, kernel, borderType=cv2.BORDER_REFLECT)
    high = cv2.dilate(lum, kernel, borderType=cv2.BORDER_REFLECT)
    cap = np.maximum((high - low) * float(p.get("cap_fraction", 0.25)), 1e-4)
    limited = detail / (1.0 + np.abs(detail) / cap)
    result = np.maximum(rgb + limited[..., None], 0.0)
    return np.concatenate((result.astype(np.float32), alpha := rgba[..., 3:4]), axis=2)


def alpha_weighted_luma(rgba: np.ndarray, p: dict) -> np.ndarray:
    """Unclamped luma USM with the detail scaled by straight alpha."""
    rgb = rgba[..., :3]
    alpha = rgba[..., 3:4]
    detail = _luma_detail(rgb, p["sigma"], p["amount"], p["threshold"], p["knee"])
    result = np.maximum(rgb + detail[..., None] * alpha, 0.0)
    return np.concatenate((result.astype(np.float32), alpha), axis=2)


MODELS = {
    "product_usm_mirror": product_usm_mirror,
    "luma_usm_soft": luma_usm_soft,
    "halo_limited": halo_limited,
    "alpha_weighted_limited": alpha_weighted_limited,
    "alpha_weighted_luma": alpha_weighted_luma,
    "detail_capped": detail_capped,
}


def settings_for(threshold: float, **extra) -> dict:
    preset = {"amount": 1.0, "sigma": 1.0, "threshold": threshold,
              "knee": 0.01 if threshold > 0.0 else 0.0}
    preset.update(extra)
    return preset


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    soft, soft_meta = make_soft_edge()
    alpha_edge, _ = make_alpha_edge()

    variants = [
        ("product_usm_mirror_t0", "product_usm_mirror", settings_for(0.0)),
        ("luma_usm_t0", "luma_usm_soft", settings_for(0.0)),
        ("halo_limited_t0", "halo_limited", settings_for(0.0)),
        ("alpha_weighted_limited_t0", "alpha_weighted_limited", settings_for(0.0)),
        ("alpha_weighted_luma_t0", "alpha_weighted_luma", settings_for(0.0)),
        ("detail_capped_t0", "detail_capped", settings_for(0.0, cap_fraction=0.25)),
        ("luma_usm_soft_t02", "luma_usm_soft", settings_for(0.02)),
        ("halo_limited_t02", "halo_limited", settings_for(0.02)),
    ]

    source_npz = OUT / "source__soft_edge.npz"
    save_array(source_npz, soft)
    save_preview(OUT / "source__soft_edge.preview.png", soft)

    cases = []
    panels = [("source", preview_rgb(soft))]
    crops = [("source edge x3", crop_zoom(soft, soft_meta["rois"]["step_edge"], 3))]
    for label, model_name, preset in variants:
        result = MODELS[model_name](soft, preset)
        npz = OUT / f"soft_edge__{label}.npz"
        save_array(npz, result)
        save_preview(OUT / f"soft_edge__{label}.preview.png", result)
        step = step_edge_metrics(soft[..., :3], result[..., :3], soft_meta, preset["sigma"])
        src_l = luma(soft[..., :3])
        out_l = luma(result[..., :3])
        noise = {}
        for key in ("noise_low", "noise_high"):
            x0, y0, x1, y1 = soft_meta["rois"][key]
            a = src_l[y0:y1, x0:x1]
            b = out_l[y0:y1, x0:x1]
            noise[key] = float(b.std() / max(a.std(), 1e-9))
        cases.append({
            "variant": label,
            "model": model_name,
            "settings": preset,
            "output_npz": npz.name,
            "output_sha256": sha256(npz),
            "step_edge": step,
            "noise_std_ratio": noise,
            "rise_gain": 1.0 - step["rise_20_80_px"] / step["source_rise_20_80_px"],
        })
        panels.append((label, preview_rgb(result)))
        crops.append((f"{label} edge x3",
                      crop_zoom(result, soft_meta["rois"]["step_edge"], 3)))

    # Amount trade-off on the soft edge: halo/overshoot vs edge steepening.
    tradeoff = []
    for label, model_name in (("mirror", "product_usm_mirror"),
                              ("luma_t0", "luma_usm_soft"),
                              ("halo_limited_t0", "halo_limited"),
                              ("capped_t0", "detail_capped")):
        for amount in (0.25, 0.5, 1.0, 2.0):
            preset = settings_for(0.0, amount=amount)
            result = MODELS[model_name](soft, preset)
            step = step_edge_metrics(soft[..., :3], result[..., :3], soft_meta, 1.0)
            tradeoff.append({
                "model": label,
                "amount": amount,
                "rise_px": step["rise_20_80_px"],
                "rise_gain": 1.0 - step["rise_20_80_px"] / step["source_rise_20_80_px"],
                "halo_fraction": step["halo_fraction_outside_transition"],
                "overshoot_linear": step["overshoot_linear"],
                "undershoot_linear": step["undershoot_linear"],
            })

    alpha_cases = []
    for label, model_name, preset in variants:
        result = MODELS[model_name](alpha_edge, preset)
        npz = OUT / f"alpha_edge__{label}.npz"
        save_array(npz, result)
        save_preview(OUT / f"alpha_edge__{label}.preview.png", result, 0.18)
        alpha_cases.append({"variant": label, "model": model_name,
                            "settings": preset, "output_npz": npz.name,
                            "output_sha256": sha256(npz),
                            "metrics": alpha_metrics(alpha_edge, result)})

    # Temporal: moving low-amplitude grating, threshold-0 ablation so the
    # clamped candidate is not a no-op.
    temporal = {}
    frames, roi = make_grating_sequence()
    x0, y0, x1, y1 = roi
    src_hp = np.stack([luma(f[..., :3]) - gaussian(luma(f[..., :3]), 1.0) for f in frames])
    for label, model_name in (("mirror_t0", "product_usm_mirror"),
                              ("luma_t0", "luma_usm_soft"),
                              ("halo_limited_t0", "halo_limited")):
        preset = settings_for(0.0)
        processed = [MODELS[model_name](frame, preset) for frame in frames]
        out_hp = np.stack([luma(f[..., :3]) - gaussian(luma(f[..., :3]), 1.0)
                           for f in processed])
        src_roi = src_hp[:, y0:y1, x0:x1]
        out_roi = out_hp[:, y0:y1, x0:x1]
        src_std = float(src_roi.std(axis=0).mean())
        out_std = float(out_roi.std(axis=0).mean())
        temporal[label] = {
            "settings": preset,
            "hp_temporal_std_source": src_std,
            "hp_temporal_std_output": out_std,
            "temporal_std_ratio": out_std / max(src_std, 1e-12),
            "frame_diff_ratio": float(np.abs(np.diff(out_roi, axis=0)).mean() /
                                      max(np.abs(np.diff(src_roi, axis=0)).mean(), 1e-12)),
            "frames": len(frames),
            "roi": roi,
        }

    contact_sheet(OUT / "soft_edge.contact.png", panels, 0.6)
    contact_sheet(OUT / "soft_edge.roi-crops.png", crops, 1.0)

    manifest = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "lab": "DetailSharpeningLab",
        "round": "rd2",
        "motivation": {
            "hard_edge_clamp": "rd1: halo_limited rise gain was 0.002 on a 1 px hard step.",
            "alpha_composite": "rd1: luma_usm_soft composite error on black was 26.1 codes "
                               "vs 15.9 for the product mirror.",
            "threshold_ablation": "rd2 first attempt: threshold 0.02 alone suppressed the "
                                  "soft-edge response to zero; threshold 0 isolates the clamp.",
        },
        "environment": {
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "pillow": Image.__version__,
        },
        "soft_edge_fixture": {
            "transition_sigma_px": SOFT_EDGE_SIGMA,
            "rois": soft_meta["rois"],
            "npz": source_npz.name,
            "sha256": sha256(source_npz),
        },
        "cases": cases,
        "amount_tradeoff": tradeoff,
        "temporal": temporal,
        "alpha_cases": alpha_cases,
        "gate_thresholds_reused": {
            "halo_fraction_le": 0.01,
            "overshoot_linear_le": 0.004,
            "rise_gain_ge": 0.15,
            "composite_max_error_codes_le": 2.0,
        },
        "gates": {
            case["variant"]: {
                "rise_gain_ge_15pct": case["rise_gain"] >= 0.15,
                "halo_fraction_le_1pct":
                    case["step_edge"]["halo_fraction_outside_transition"] <= 0.01,
                "overshoot_le_0.004_linear": case["step_edge"]["overshoot_linear"] <= 0.004,
                "composite_error_le_2codes":
                    max(alpha_cases[i]["metrics"]["composite_max_error_codes"].values()) <= 2.0,
            }
            for i, case in enumerate(cases)
        },
        "limitations": [
            "Offline models only; not a product capture.",
            "Soft edge is a synthetic Gaussian step, not a measured lens PSF.",
            "Alpha weighting is a straight-alpha concession, not a premultiplied contract decision.",
            "No GPU, final Viewer, real photograph, or allocation evidence.",
        ],
    }
    manifest_path = OUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8",
                             newline="\n")
    (OUT / "manifest.sha256").write_text(sha256(manifest_path) + "  manifest.json\n",
                                        encoding="utf-8", newline="\n")

    print(f"wrote {manifest_path}")
    for case in cases:
        step = case["step_edge"]
        print(f"  {case['variant']:26s} rise={step['rise_20_80_px']:.2f} "
              f"(src {step['source_rise_20_80_px']:.2f}) gain={case['rise_gain']:+.3f} "
              f"halo={step['halo_fraction_outside_transition']*100:5.2f}% "
              f"ovs={step['overshoot_linear']:+.4f} und={step['undershoot_linear']:+.4f}")
    print("  trade-off (amount)")
    for row in tradeoff:
        print(f"    {row['model']:16s} a={row['amount']:.2f} gain={row['rise_gain']:+.3f} "
              f"halo={row['halo_fraction']*100:5.2f}% ovs={row['overshoot_linear']:+.4f}")
    for case in alpha_cases:
        m = case["metrics"]
        print(f"  alpha {case['variant']:26s} composite="
              f"{ {k: round(v, 2) for k, v in m['composite_max_error_codes'].items()} }")
    for label, values in temporal.items():
        print(f"  temporal {label:18s} std_ratio={values['temporal_std_ratio']:.3f} "
              f"diff_ratio={values['frame_diff_ratio']:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
