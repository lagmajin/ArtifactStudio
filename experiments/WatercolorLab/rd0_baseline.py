"""RD0 baseline acquisition for the watercolor-style R&D.

Generates the no-op baseline and two pseudo-watercolor controls from the
deterministic fixtures, records descriptive statistics, review sheets and
blank gate forms. No pass/fail thresholds are set here: per EVALUATION_PLAN.md
they must be fixed by a separate reviewer after this baseline, before any
candidate comparison. Offline only: no build, no Artifact launch.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import sys
import time
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

import generate_fixtures as gf


CONTROLS = {
    "baseline_raw": {"description": "input unchanged"},
    "control_gaussian": {"description": "uniform Gaussian blur on RGB", "sigma": 2.0},
    "control_noise": {"description": "single additive Gaussian noise on RGB", "sigma_linear": 0.02},
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stats_1d(values: np.ndarray, clip_high: float | None = None) -> dict:
    v = np.asarray(values, dtype=np.float64).ravel()
    finite = v[np.isfinite(v)]
    if finite.size == 0:
        return {"count": int(v.size), "nonfinite": int(v.size - finite.size)}
    out = {
        "count": int(v.size),
        "nonfinite": int(v.size - finite.size),
        "min": float(np.min(finite)),
        "max": float(np.max(finite)),
        "mean": float(np.mean(finite)),
        "median": float(np.median(finite)),
        "p1": float(np.percentile(finite, 1)),
        "p5": float(np.percentile(finite, 5)),
        "p95": float(np.percentile(finite, 95)),
        "p99": float(np.percentile(finite, 99)),
        "clip_low": int(np.sum(finite < 0.0)),
    }
    if clip_high is not None:
        out["clip_high"] = int(np.sum(finite > clip_high))
    return out


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def distribution(rgba: np.ndarray) -> dict:
    rgb = rgba[..., :3].astype(np.float64)
    out = {
        "R": stats_1d(rgb[..., 0], clip_high=1.0),
        "G": stats_1d(rgb[..., 1], clip_high=1.0),
        "B": stats_1d(rgb[..., 2], clip_high=1.0),
        "luma_linear": stats_1d(linear_luma(rgb), clip_high=1.0),
        "alpha": stats_1d(rgba[..., 3].astype(np.float64)),
    }
    srgb8 = gf.linear_to_srgb_u8(rgb)
    out["srgb8_luma"] = stats_1d(srgb8.mean(axis=2), clip_high=255.0)
    return out


def crop(rgba: np.ndarray, rect: list[int]) -> np.ndarray:
    x0, y0, x1, y1 = rect
    h, w = rgba.shape[:2]
    return rgba[max(0, y0) : min(h, y1), max(0, x0) : min(w, x1)]


def roi_metrics(rgba: np.ndarray, rect: list[int]) -> dict:
    c = crop(rgba, rect)
    rgb = c[..., :3].astype(np.float64)
    luma = linear_luma(rgb)
    gx = cv2.Sobel(luma, cv2.CV_64F, 1, 0, ksize=3)
    gy = cv2.Sobel(luma, cv2.CV_64F, 0, 1, ksize=3)
    grad = np.hypot(gx, gy).ravel()
    srgb8 = gf.linear_to_srgb_u8(rgb)
    gray8 = cv2.cvtColor(srgb8, cv2.COLOR_RGB2GRAY)
    means = rgb.mean(axis=(0, 1))
    return {
        "rect": rect,
        "pixels": int(rgb.shape[0] * rgb.shape[1]),
        "mean_rgb_linear": [float(v) for v in means],
        "std_rgb_linear": [float(v) for v in rgb.std(axis=(0, 1))],
        "local_contrast_std_over_mean": float(rgb.std() / (means.mean() + 1e-6)),
        "gradient_mean": float(np.mean(grad)),
        "gradient_p95": float(np.percentile(grad, 95)),
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
            "rmse": float(np.sqrt(np.mean(d * d))),
            "max_abs": float(np.max(np.abs(d))),
            "p99_abs": float(np.percentile(np.abs(d), 99)),
            "exceed_1_over_255_rate": float(np.mean(np.abs(d) > (1.0 / 255.0))),
        },
        "srgb8_codes": {
            "mae": float(np.mean(np.abs(ad))),
            "max_abs": float(np.max(np.abs(ad))),
        },
        "note": "exceed_1_over_255 is a descriptive fixed probe, not an acceptance threshold",
    }


def patch_color_metrics(rgba: np.ndarray, rois: dict) -> dict:
    out = {}
    for key, rect in rois.items():
        if not key.startswith("patch_"):
            continue
        c = crop(rgba, rect)
        mean_linear = c[..., :3].astype(np.float64).mean(axis=(0, 1))
        srgb8 = gf.linear_to_srgb_u8(mean_linear)
        hsv = cv2.cvtColor(np.uint8([[srgb8]]), cv2.COLOR_RGB2HSV)[0, 0]
        out[key] = {
            "mean_rgb_linear": [float(v) for v in mean_linear],
            "srgb8": [int(v) for v in srgb8],
            "hue_opencv_0_179": int(hsv[0]),
            "sat_opencv": int(hsv[1]),
            "value_opencv": int(hsv[2]),
            "hue_defined": bool(hsv[1] >= 13),
        }
    return out


def alpha_contract_metrics(straight: np.ndarray, premul: np.ndarray) -> dict:
    a = straight[..., 3:4]
    valid = a[..., 0] > 0.01
    recovered = premul[..., :3] / np.maximum(a, 1e-6)
    delta = np.abs(recovered - straight[..., :3])[valid]
    composites = {}
    for name, bg in (("black", 0.0), ("white", 1.0), ("mid_gray", 0.5)):
        cs = straight[..., :3] * a + bg * (1.0 - a)
        cp = premul[..., :3] + bg * (1.0 - a)
        composites[name] = {
            "max_abs_delta": float(np.max(np.abs(cs - cp))),
            "mean_abs_delta": float(np.mean(np.abs(cs - cp))),
        }
    return {
        "alpha_gt_0_01_pixels": int(np.sum(valid)),
        "recovered_straight_rgb_max_delta": float(np.max(delta)) if delta.size else 0.0,
        "composite_over_backgrounds": composites,
        "note": "descriptive contract probe; acceptance tolerance fixed after RD0 by separate reviewer",
    }


def temporal_metrics(frames: list[np.ndarray], rois: dict) -> dict:
    per_step = []
    for k in range(1, len(frames)):
        d = np.abs(frames[k][..., :3].astype(np.float64) - frames[k - 1][..., :3].astype(np.float64))
        step = {
            "step": k,
            "global_mae": float(d.mean()),
            "static_flat_mae": float(np.abs(crop(frames[k], rois["static_flat"])[..., :3].astype(np.float64)
                                             - crop(frames[k - 1], rois["static_flat"])[..., :3].astype(np.float64)).mean()),
            "moving_patch_mae": float(np.abs(crop(frames[k], rois["moving_patch"])[..., :3].astype(np.float64)
                                             - crop(frames[k - 1], rois["moving_patch"])[..., :3].astype(np.float64)).mean()),
        }
        per_step.append(step)
    aligned = []
    base = frames[0]
    h, w = base.shape[:2]
    for k in range(1, len(frames)):
        m = np.array([[1.0, 0.0, 0.5 * k], [0.0, 1.0, 0.0]], dtype=np.float32)
        warped = cv2.warpAffine(base, m, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
        d = np.abs(warped[..., :3].astype(np.float64) - frames[k][..., :3].astype(np.float64))
        aligned.append({"frame": k, "aligned_mae": float(d.mean())})
    return {
        "per_step": per_step,
        "integer_alignment_probe": aligned,
        "note": "motion and intentional changes must not be counted as flicker; no gate set at RD0",
    }


def apply_control(rgba: np.ndarray, name: str, rng: np.random.Generator) -> np.ndarray:
    if name == "baseline_raw":
        return rgba.copy()
    out = rgba.copy()
    if name == "control_gaussian":
        sigma = CONTROLS[name]["sigma"]
        k = max(1, int(round(sigma * 2)))
        k += 1 if k % 2 == 0 else 0
        out[..., :3] = cv2.GaussianBlur(rgba[..., :3], (k, k), sigmaX=sigma, sigmaY=sigma)
    elif name == "control_noise":
        out[..., :3] = rgba[..., :3] + rng.normal(0.0, CONTROLS[name]["sigma_linear"], size=rgba[..., :3].shape).astype(np.float32)
    else:
        raise ValueError(name)
    return out


def save_output(path: Path, rgba: np.ndarray) -> dict:
    return gf.save_rgba(path, rgba)


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
        "# RD0 baseline review form (blank)\n\n"
        "Same display conditions for all images. Do not assign pass/fail at RD0.\n\n"
        "| ID | fixture | control | full image notes | ROI notes | defects observed | reviewer | date |\n"
        "|---|---|---|---|---|---|---|---|\n"
        "| B01 | color_ramp_small | baseline_raw | | | | | |\n"
        "| B02 | color_ramp_small | control_gaussian | | | | | |\n"
        "| B03 | color_ramp_small | control_noise | | | | | |\n"
        "| B04 | thin_lines_small | baseline_raw | | | | | |\n"
        "| B05 | alpha_boundary_small | baseline_raw | | | | | |\n"
        "| B06 | flat_highlight_large | baseline_raw | | | | | |\n",
        encoding="utf-8",
    )
    (review_dir / "GATE_FORM.md").write_text(
        "# RD0 gate form (thresholds intentionally blank)\n\n"
        "Per EVALUATION_PLAN.md section 5.0: a reviewer independent of the implementer "
        "fills the tolerance column after viewing the RD0 baseline full images and crops, "
        "and before any candidate comparison. Unfilled items remain unevaluated.\n\n"
        "| item | region | baseline evidence | tolerance (BLANK until fixed) | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n"
        "| 5.1 効果の正しさ | all / patch / ramp | metrics.json | | | | |\n"
        "| 5.2 画質と細部 | thin_lines / flat / boundary | rd0_full_sheet, roi_sheet | | | | |\n"
        "| 5.3 色と alpha | patches / alpha boundary | metrics.json alpha_contract | | | | |\n"
        "| 5.4 時間安定性 | sequence | metrics.json temporal | | | | |\n"
        "| 5.5 経路と解像度 | small vs large | metrics.json | | | | |\n"
        "| 5.6 性能 | offline model | timing in manifest_rd0.json | | | | |\n"
        "| 5.7 bleed / paper / edge | per-component ROI | metrics.json | | | | |\n",
        encoding="utf-8",
    )
    (review_dir / "BLIND_REVIEW_FORM.md").write_text(
        "# Candidate blind review form (blank, for later RD stages)\n\n"
        "Do not use until thresholds in GATE_FORM.md are filled.\n\n"
        "| blind ID | fixture | candidate | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|---|---|\n",
        encoding="utf-8",
    )


def run(output: Path) -> dict:
    fm = json.loads((output / "fixtures_manifest.json").read_text(encoding="utf-8"))
    baseline_dir = output / "baselines"
    baseline_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(gf.SEED + 900)
    metrics: dict = {
        "lab": "WatercolorLab",
        "rd": "rd0",
        "stage": "baseline",
        "gates": "unset (see review/GATE_FORM.md)",
        "fixtures": {},
        "definitions": {
            "luma_linear": "Rec.709 weights on linear RGB",
            "exceed_1_over_255": "descriptive probe only, not an acceptance threshold",
        },
    }
    outputs: dict = {}
    timings: dict = {}
    full_items: list[tuple[str, Path]] = []
    roi_items: list[tuple[str, Path]] = []

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small":
            continue
        rgba = np.load(output / "fixtures" / entry["npy"])
        rois = entry["rois"]
        per_control = {}
        for name, meta in CONTROLS.items():
            t0 = time.perf_counter()
            result = apply_control(rgba, name, rng)
            elapsed = time.perf_counter() - t0
            stem = f"{key}_{name}"
            hashes = save_output(baseline_dir / stem, result)
            outputs[stem] = hashes
            timings[stem] = {"seconds": float(elapsed), "pixels": int(result.shape[0] * result.shape[1])}
            per_control[name] = {
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, meta),
            }
            full_items.append((f"{key}-{name}", baseline_dir / (stem + ".png")))

        per_control["_source"] = {
            "distribution": distribution(rgba),
            "patch_colors": patch_color_metrics(rgba, rois),
        }
        if "premultiplied" in entry:
            premul = np.load(output / "fixtures" / entry["premultiplied"]["npy"])
            per_control["_source"]["alpha_contract"] = alpha_contract_metrics(rgba, premul)
        for label, rect in rois.items():
            c = crop(rgba, rect)
            preview = baseline_dir / f"{key}_roi_{label}.png"
            cv2.imwrite(str(preview), cv2.cvtColor(gf.linear_to_srgb_u8(c[..., :3]), cv2.COLOR_RGB2BGR))
            roi_items.append((f"{key}-{label}", preview))
        metrics["fixtures"][key] = per_control

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output / "fixtures" / f["npy"]) for f in seq["frames"]]
    metrics["sequence_small"] = temporal_metrics(frames, seq["rois"])

    resolution_probe = {}
    for name in ("color_ramp", "thin_lines", "alpha_boundary", "flat_highlight"):
        small = metrics["fixtures"].get(f"{name}_small", {}).get("_source", {})
        large = metrics["fixtures"].get(f"{name}_large", {}).get("_source", {})
        if not small or not large:
            continue
        sm = small["distribution"]["luma_linear"]["mean"]
        lm = large["distribution"]["luma_linear"]["mean"]
        resolution_probe[name] = {
            "small_luma_mean": sm,
            "large_luma_mean": lm,
            "abs_delta": abs(sm - lm),
            "note": "descriptive only; tolerance fixed after RD0",
        }
    metrics["resolution_probe"] = resolution_probe
    metrics["effect_specific"] = {
        "status": "not applicable to no-op baseline and control outputs",
        "defined_in": "EVALUATION_PLAN.md section 5.7 (bleed / paper / edge)",
    }

    metrics_path = output / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    full_sheet = review_dir / "rd0_full_sheet.png"
    roi_sheet = review_dir / "rd0_roi_sheet.png"
    contact_sheet(full_items[:24], cols=3, cell_w=384, cell_h=256).save(full_sheet)
    contact_sheet(roi_items[:24], cols=4, cell_w=240, cell_h=200).save(roi_sheet)
    write_forms(review_dir)

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd0",
        "stage": "baseline",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "opencv": cv2.__version__,
            "platform": platform.platform(),
        },
        "seed": gf.SEED + 900,
        "controls": CONTROLS,
        "outputs": outputs,
        "timings": timings,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "full_sheet": {"file": f"review/{full_sheet.name}", "sha256": sha256(full_sheet.read_bytes())},
            "roi_sheet": {"file": f"review/{roi_sheet.name}", "sha256": sha256(roi_sheet.read_bytes())},
            "forms": sorted(p.name for p in review_dir.glob("*.md")),
        },
        "gates": "unset; separate reviewer must fix tolerances before candidate comparison",
        "unverified": [
            "product effect / UI / undo / save-reload",
            "preview and final render paths",
            "D3D12 / Vulkan parity",
            "HDR quality above 1.0",
            "production performance at 1080p / 4K",
        ],
    }
    manifest_path = output / "manifest_rd0.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="captures/rd0", help="directory containing fixtures_manifest.json")
    args = parser.parse_args()
    output = Path(args.output)
    if not (output / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing fixtures manifest; run generate_fixtures.py --output {output} first")
    manifest = run(output)
    print(f"RD0 baseline complete: {len(manifest['outputs'])} outputs -> {output}")


if __name__ == "__main__":
    main()
