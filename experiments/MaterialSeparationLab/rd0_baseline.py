"""RD0 baseline acquisition for the material separation R&D.

Records the untouched input and two simple controls (low-pass only, detail
only), descriptive statistics, review sheets and blank gate forms. No pass/fail
thresholds are set: a separate reviewer must fix them after seeing the
baseline. Offline only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

import generate_fixtures as gf

CONTROLS = {
    "baseline_raw": {"description": "input unchanged"},
    "control_lowpass": {"description": "gaussian low-pass base only (sigma 2.0, premultiplied)"},
    "control_detail_only": {"description": "high-frequency detail only (premultiplied input minus low-pass)"},
}
LOWPASS_SIGMA = 2.0


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stats_1d(values: np.ndarray, clip_high: float | None = None) -> dict:
    v = np.asarray(values, dtype=np.float64).ravel()
    finite = v[np.isfinite(v)]
    if finite.size == 0:
        return {"count": int(v.size), "nonfinite": int(v.size)}
    out = {
        "count": int(v.size), "nonfinite": int(v.size - finite.size),
        "min": float(np.min(finite)), "max": float(np.max(finite)),
        "mean": float(np.mean(finite)), "median": float(np.median(finite)),
        "std": float(np.std(finite)),
        "p1": float(np.percentile(finite, 1)), "p5": float(np.percentile(finite, 5)),
        "p95": float(np.percentile(finite, 95)), "p99": float(np.percentile(finite, 99)),
        "clip_low": int(np.sum(finite < 0.0)),
    }
    if clip_high is not None:
        out["clip_high"] = int(np.sum(finite > clip_high))
    return out


def luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def distribution(rgba: np.ndarray) -> dict:
    rgb = rgba[..., :3].astype(np.float64)
    out = {
        "R": stats_1d(rgb[..., 0], 1.0), "G": stats_1d(rgb[..., 1], 1.0),
        "B": stats_1d(rgb[..., 2], 1.0), "luma_linear": stats_1d(luma(rgb), 1.0),
        "alpha": stats_1d(rgba[..., 3].astype(np.float64)),
    }
    out["srgb8_luma"] = stats_1d(gf.linear_to_srgb_u8(rgb).mean(axis=2), 255.0)
    return out


def crop(rgba: np.ndarray, rect: list[int]) -> np.ndarray:
    x0, y0, x1, y1 = rect
    h, w = rgba.shape[:2]
    return rgba[max(0, y0):min(h, y1), max(0, x0):min(w, x1)]


def roi_metrics(rgba: np.ndarray, rect: list[int]) -> dict:
    c = crop(rgba, rect)
    rgb = c[..., :3].astype(np.float64)
    lm = luma(rgb)
    gx = cv2.Sobel(lm, cv2.CV_64F, 1, 0, ksize=3)
    gy = cv2.Sobel(lm, cv2.CV_64F, 0, 1, ksize=3)
    grad = np.hypot(gx, gy).ravel()
    gray8 = cv2.cvtColor(gf.linear_to_srgb_u8(rgb), cv2.COLOR_RGB2GRAY)
    return {
        "rect": rect, "pixels": int(rgb.shape[0] * rgb.shape[1]),
        "mean_rgb_linear": [float(v) for v in rgb.mean(axis=(0, 1))],
        "std_rgb_linear": [float(v) for v in rgb.std(axis=(0, 1))],
        "local_contrast_std_over_mean": float(rgb.std() / (rgb.mean() + 1e-6)),
        "luma_std": float(lm.std()),
        "gradient_mean": float(np.mean(grad)), "gradient_p95": float(np.percentile(grad, 95)),
        "unique_srgb8_gray_levels": int(np.unique(gray8).size),
        "alpha_mean": float(c[..., 3].mean()),
    }


def diff_metrics(a: np.ndarray, b: np.ndarray, meta: dict) -> dict:
    ar, br = a[..., :3].astype(np.float64), b[..., :3].astype(np.float64)
    d = ar - br
    ad = gf.linear_to_srgb_u8(ar).astype(np.float64) - gf.linear_to_srgb_u8(br).astype(np.float64)
    return {
        "reference": meta["description"],
        "linear": {
            "mae_per_channel": [float(v) for v in np.mean(np.abs(d), axis=(0, 1))],
            "rmse": float(np.sqrt(np.mean(d * d))), "max_abs": float(np.max(np.abs(d))),
            "p99_abs": float(np.percentile(np.abs(d), 99)),
        },
        "srgb8_codes": {"mae": float(np.mean(np.abs(ad))), "max_abs": float(np.max(np.abs(ad)))},
    }


def patch_colors(rgba: np.ndarray, rois: dict) -> dict:
    out = {}
    for key, rect in rois.items():
        if not key.startswith("patch_"):
            continue
        mean = crop(rgba, rect)[..., :3].astype(np.float64).mean(axis=(0, 1))
        srgb8 = gf.linear_to_srgb_u8(mean)
        hsv = cv2.cvtColor(np.uint8([[srgb8]]), cv2.COLOR_RGB2HSV)[0, 0]
        out[key] = {
            "mean_rgb_linear": [float(v) for v in mean], "srgb8": [int(v) for v in srgb8],
            "hue_opencv_0_179": int(hsv[0]), "sat_opencv": int(hsv[1]),
            "hue_defined": bool(hsv[1] >= 13),
        }
    return out


def ramp_profile(rgba: np.ndarray, rect: list[int]) -> dict:
    """Column-wise mean luma of the grey ramp plus the step statistics."""
    c = crop(rgba, rect)
    lm = luma(c[..., :3].astype(np.float64)).mean(axis=0)
    steps = np.diff(lm)
    return {
        "samples": int(lm.size),
        "first": float(lm[0]), "last": float(lm[-1]),
        "step_mean": float(steps.mean()), "step_std": float(steps.std()),
        "step_min": float(steps.min()), "step_max": float(steps.max()),
        "step_p01": float(np.percentile(steps, 1)), "step_p99": float(np.percentile(steps, 99)),
        "monotonic_fraction": float(np.mean(steps > 0.0)),
        "flat_step_rate": float(np.mean(np.abs(steps) < 1e-6)),
        "profile": [float(v) for v in lm],
    }


def local_contrast(rgba: np.ndarray, rois: dict) -> dict:
    """std of luma in the flat region vs the textured region."""
    out = {}
    pairs = (("flat_region", "texture_region"), ("flat_bg", "gray_ramp"), ("flat_low", "highlight"))
    for flat_key, busy_key in pairs:
        if flat_key in rois and busy_key in rois:
            flat = luma(crop(rgba, rois[flat_key])[..., :3].astype(np.float64))
            busy = luma(crop(rgba, rois[busy_key])[..., :3].astype(np.float64))
            out[f"{flat_key}_vs_{busy_key}"] = {
                "flat_std": float(flat.std()), "busy_std": float(busy.std()),
                "ratio": float(busy.std() / (flat.std() + 1e-9)),
            }
    return out


def alpha_contract(straight: np.ndarray, premul: np.ndarray) -> dict:
    a = straight[..., 3:4]
    valid = a[..., 0] > 0.01
    recovered = premul[..., :3] / np.maximum(a, 1e-6)
    composites = {}
    for name, bg in (("black", 0.0), ("white", 1.0), ("mid_gray", 0.5)):
        cs = straight[..., :3] * a + bg * (1.0 - a)
        cp = premul[..., :3] + bg * (1.0 - a)
        composites[name] = {
            "max_abs_delta": float(np.max(np.abs(cs - cp))),
            "mean_abs_delta": float(np.mean(np.abs(cs - cp))),
        }
    return {
        "recovered_straight_rgb_max_delta": float(np.max(np.abs(recovered - straight[..., :3])[valid])),
        "composite_over_backgrounds": composites,
    }


def temporal(frames: list[np.ndarray], rois: dict) -> dict:
    steps = []
    for k in range(1, len(frames)):
        a, b = frames[k][..., :3].astype(np.float64), frames[k - 1][..., :3].astype(np.float64)
        steps.append({
            "step": k, "global_mae": float(np.abs(a - b).mean()),
            "static_flat_mae": float(np.abs(crop(frames[k], rois["static_flat"])[..., :3].astype(np.float64)
                                              - crop(frames[k - 1], rois["static_flat"])[..., :3].astype(np.float64)).mean()),
        })
    return {"per_step": steps, "note": "descriptive; no gate set"}


def apply_control(rgba: np.ndarray, name: str) -> np.ndarray:
    src = np.asarray(rgba, dtype=np.float32)
    alpha = np.clip(src[..., 3:4], 0.0, 1.0)
    premul = (src[..., :3] * alpha).astype(np.float32)
    if name == "baseline_raw":
        return src.copy()
    stack = np.concatenate([premul, alpha], axis=2)
    blurred = cv2.GaussianBlur(stack, (0, 0), sigmaX=LOWPASS_SIGMA, sigmaY=LOWPASS_SIGMA,
                               borderType=cv2.BORDER_REFLECT)
    out = np.empty_like(src)
    if name == "control_lowpass":
        out[..., :3] = blurred[..., :3] / np.maximum(alpha, 1e-6)
    elif name == "control_detail_only":
        out[..., :3] = (premul - blurred[..., :3]) / np.maximum(alpha, 1e-6)
    else:
        raise ValueError(name)
    out[..., 3:4] = alpha
    return np.clip(out, 0.0, None).astype(np.float32)


def contact_sheet(items: list[tuple[str, Path]], cols: int, cell_w: int, cell_h: int) -> Image.Image:
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell_w, rows * (cell_h + 16)), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    for i, (label, path) in enumerate(items):
        img = Image.open(path).convert("RGB")
        img.thumbnail((cell_w - 4, cell_h - 4))
        x, y = (i % cols) * cell_w, (i // cols) * (cell_h + 16)
        sheet.paste(img, (x + 2, y + 2))
        draw.text((x + 3, y + cell_h), label, fill=(230, 230, 230))
    return sheet


def write_forms(review_dir: Path) -> None:
    (review_dir / "BASELINE_REVIEW_FORM.md").write_text(
        "# RD0 baseline review form (blank)\n\nDo not assign pass/fail at RD0.\n\n"
        "| ID | fixture | control | full notes | ROI notes | defects | reviewer | date |\n"
        "|---|---|---|---|---|---|---|---|\n"
        + "".join(f"| B{i:02d} | | | | | | | |\n" for i in range(1, 13)),
        encoding="utf-8",
    )
    (review_dir / "GATE_FORM.md").write_text(
        "# RD0 gate form (thresholds intentionally blank)\n\n"
        "A reviewer independent of the implementer fills tolerances after viewing the RD0 baseline, "
        "before candidate comparison. Unfilled items remain unevaluated.\n\n"
        "| item | region | evidence | tolerance (BLANK) | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n"
        + "".join(f"| {item} | | | | | | |\n" for item in (
            "5.1 効果の正しさ", "5.2 画質と細部", "5.3 色と alpha", "5.4 時間安定性",
            "5.5 経路と解像度", "5.6 性能", "5.7 効果固有項目",
        )),
        encoding="utf-8",
    )
    (review_dir / "BLIND_REVIEW_FORM.md").write_text(
        "# Candidate blind review form (blank, for later RD stages)\n\n"
        "Do not use until GATE_FORM.md thresholds are fixed.\n\n"
        "| blind ID | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n",
        encoding="utf-8",
    )


def write_tables(review_dir: Path, metrics: dict) -> None:
    lines = [
        "# RD0 tables (descriptive; gates unset)", "",
        "数値は記述的 baseline である。合否判定・順位付けは行っていない。", "",
        "## 入力と control の分布（small 解像度）", "",
        "| fixture | control | luma mean | luma std | srgb8 luma mean | alpha mean |",
        "|---|---|---|---|---|---|",
    ]
    for key, per in metrics["fixtures"].items():
        for name in CONTROLS:
            d = per[name]["distribution"]
            lines.append(
                f"| {key} | {name} | {d['luma_linear']['mean']:.6f} | {d['luma_linear']['std']:.6f} "
                f"| {d['srgb8_luma']['mean']:.3f} | {d['alpha']['mean']:.6f} |")
    lines += ["", "## 灰色ランプの段差（入力 vs control）", "",
              "| fixture | control | step mean | step std | step min | step max | monotonic fraction |",
              "|---|---|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for name in CONTROLS:
            if "ramp_profile" not in per[name]:
                continue
            r = per[name]["ramp_profile"]
            lines.append(
                f"| {key} | {name} | {r['step_mean']:.6f} | {r['step_std']:.6f} "
                f"| {r['step_min']:.6f} | {r['step_max']:.6f} | {r['monotonic_fraction']:.4f} |")
    lines += ["", "## 局所コントラスト（平坦部 std / 忙丙部 std）", "",
              "| fixture | control | pair | flat std | busy std | ratio |",
              "|---|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for name in CONTROLS:
            for pair, v in per[name]["local_contrast"].items():
                lines.append(
                    f"| {key} | {name} | {pair} | {v['flat_std']:.6f} | {v['busy_std']:.6f} | {v['ratio']:.3f} |")
    (review_dir / "RD0_TABLES.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run(output: Path) -> dict:
    fm = json.loads((output / "fixtures_manifest.json").read_text(encoding="utf-8"))
    baseline_dir = output / "baselines"
    baseline_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {"lab": "MaterialSeparationLab", "rd": "rd0", "stage": "baseline", "gates": "unset",
               "controls": CONTROLS, "fixtures": {}}
    outputs = {}
    full_items = []
    roi_items = []
    for key, entry in fm["fixtures"].items():
        if key == "sequence_small":
            continue
        rgba = np.load(output / "fixtures" / entry["npy"])
        rois = entry["rois"]
        per = {}
        for name in CONTROLS:
            result = apply_control(rgba, name)
            stem = f"{key}_{name}"
            np.save(baseline_dir / (stem + ".npy"), result.astype(np.float32))
            cv2.imwrite(str(baseline_dir / (stem + ".png")),
                        cv2.cvtColor(gf.linear_to_srgb_u8(result[..., :3]), cv2.COLOR_RGB2BGR))
            npy, png = baseline_dir / (stem + ".npy"), baseline_dir / (stem + ".png")
            outputs[stem] = {"npy": npy.name, "npy_sha256": sha256(npy.read_bytes()),
                             "png": png.name, "png_sha256": sha256(png.read_bytes())}
            per[name] = {
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, CONTROLS[name]),
                "patch_colors": patch_colors(result, rois),
            }
            if "gray_ramp" in rois:
                per[name]["ramp_profile"] = ramp_profile(result, rois["gray_ramp"])
            per[name]["local_contrast"] = local_contrast(result, rois)
            full_items.append((stem, png))
        per["_source"] = {"distribution": distribution(rgba), "patch_colors": patch_colors(rgba, rois)}
        if "gray_ramp" in rois:
            per["_source"]["ramp_profile"] = ramp_profile(rgba, rois["gray_ramp"])
        per["_source"]["local_contrast"] = local_contrast(rgba, rois)
        if "premultiplied" in entry:
            premul = np.load(output / "fixtures" / entry["premultiplied"]["npy"])
            per["_source"]["alpha_contract"] = alpha_contract(rgba, premul)
        for label, rect in rois.items():
            c = crop(rgba, rect)
            p = baseline_dir / f"{key}_roi_{label}.png"
            cv2.imwrite(str(p), cv2.cvtColor(gf.linear_to_srgb_u8(c[..., :3]), cv2.COLOR_RGB2BGR))
            roi_items.append((f"{key}-{label}", p))
        metrics["fixtures"][key] = per

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output / "fixtures" / f["npy"]) for f in seq["frames"]]
    metrics["sequence_small"] = temporal(frames, seq["rois"])
    probe = {}
    for name in ("color_ramp", "thin_lines", "texture", "flat_highlight", "alpha_boundary"):
        s = metrics["fixtures"].get(f"{name}_small", {}).get("_source", {})
        l = metrics["fixtures"].get(f"{name}_large", {}).get("_source", {})
        if s and l:
            sm, lm = s["distribution"]["luma_linear"]["mean"], l["distribution"]["luma_linear"]["mean"]
            probe[name] = {"small_luma_mean": sm, "large_luma_mean": lm, "abs_delta": abs(sm - lm)}
    metrics["resolution_probe"] = probe

    metrics_path = output / "metrics_rd0.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    sheet = review_dir / "rd0_full_sheet.png"
    roi_sheet = review_dir / "rd0_roi_sheet.png"
    contact_sheet(full_items[:24], 3, 384, 256).save(sheet)
    contact_sheet(roi_items[:24], 4, 240, 200).save(roi_sheet)
    write_forms(review_dir)
    write_tables(review_dir, metrics)
    manifest = {
        "lab": "MaterialSeparationLab", "rd": "rd0", "stage": "baseline", "date": "2026-11-18",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "seed": gf.SEED, "controls": CONTROLS, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "full_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "roi_sheet": {"file": f"review/{roi_sheet.name}", "sha256": sha256(roi_sheet.read_bytes())},
            "tables": {"file": "review/RD0_TABLES.md",
                       "sha256": sha256((review_dir / "RD0_TABLES.md").read_bytes())},
        },
        "gates": "unset; separate reviewer must fix tolerances before candidate comparison",
        "unverified": ["product effect / UI / undo / save-reload", "preview and final render paths",
                       "D3D12 / Vulkan parity", "HDR quality above 1.0", "production performance at 1080p / 4K"],
    }
    manifest_path = output / "manifest_rd0.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="captures/rd0")
    args = parser.parse_args()
    output = Path(args.output)
    if not (output / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing fixtures manifest: {output}")
    manifest = run(output)
    print(f"RD0 baseline complete: {len(manifest['outputs'])} outputs -> {output}")


if __name__ == "__main__":
    main()
