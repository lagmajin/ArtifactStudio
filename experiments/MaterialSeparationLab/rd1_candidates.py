"""RD1 candidate acquisition for the material separation R&D.

Runs the three candidate families (A self-guided decomposition, B frequency
separation, C emphasis) over the RD0 fixtures and records descriptive
evidence only. No pass/fail judgment and no ranking: gate thresholds must be
fixed by a separate reviewer before any final comparison. Offline only.
"""

from __future__ import annotations

import argparse
import json
import platform
import time
import tracemalloc
from pathlib import Path

import cv2
import numpy as np

import generate_fixtures as gf
import material_reference as mr
from rd0_baseline import (
    contact_sheet,
    diff_metrics,
    distribution,
    local_contrast,
    luma,
    ramp_profile,
    roi_metrics,
    sha256,
    temporal,
)

CANDIDATES = {
    "ms_a_guided": {"model": "A", "radius": 4, "eps": 1e-4},
    "ms_a_guided_wide": {"model": "A", "radius": 10, "eps": 1e-4},
    "ms_b_freq": {"model": "B", "sigma": 2.0},
    "ms_c_emphasis": {"model": "C", "radius_fine": 2, "radius_coarse": 10, "eps": 1e-4,
                      "coarse_gain": 1.0, "fine_gain": 1.0},
    "ms_c_emphasis_boost": {"model": "C", "radius_fine": 2, "radius_coarse": 10, "eps": 1e-4,
                             "coarse_gain": 1.4, "fine_gain": 1.8},
}
BLIND_SEED = gf.SEED + 1500
# visibility threshold matches the unpremultiply contract in material_reference
# (alpha <= ALPHA_EPS is written as RGB = 0 and carries hidden RGB in the input)
VISIBLE_ALPHA = mr.ALPHA_EPS
NEAR_ALPHA = 0.5


def _neutral_spec(spec: dict) -> dict:
    """All gains at 1.0: the model must reproduce its own input."""
    neutral = dict(spec)
    neutral["base_gain"] = 1.0
    neutral["detail_gain"] = 1.0
    neutral["coarse_gain"] = 1.0
    neutral["fine_gain"] = 1.0
    return neutral


def _zero_spec(spec: dict) -> dict:
    """Detail amplitude 0: only the low frequency base survives."""
    zero = dict(spec)
    zero["base_gain"] = 1.0
    zero["detail_gain"] = 0.0
    zero["coarse_gain"] = 0.0
    zero["fine_gain"] = 0.0
    return zero


def gradient_reversal(rgba: np.ndarray, rect: list[int], reference: np.ndarray | None = None) -> dict:
    """Gradient magnitude change of `rgba` inside rect against `reference` (input by default)."""
    x0, y0, x1, y1 = rect
    h, w = rgba.shape[:2]
    src = rgba if reference is None else reference
    ref = src[max(0, y0):min(h, y1), max(0, x0):min(w, x1)][..., :3].astype(np.float64)
    out = rgba[max(0, y0):min(h, y1), max(0, x0):min(w, x1)][..., :3].astype(np.float64)
    if ref.shape != out.shape or ref.size == 0:
        return {"compared_pixels": 0}
    def grad(img: np.ndarray) -> np.ndarray:
        lm = luma(img)
        gx = cv2.Sobel(lm, cv2.CV_64F, 1, 0, ksize=3)
        gy = cv2.Sobel(lm, cv2.CV_64F, 0, 1, ksize=3)
        return np.hypot(gx, gy)
    gr, go = grad(ref), grad(out)
    return {
        "compared_pixels": int(ref.shape[0] * ref.shape[1]),
        "gradient_mean_ref": float(gr.mean()), "gradient_mean_out": float(go.mean()),
        "gradient_abs_delta_mean": float(np.abs(gr - go).mean()),
        "gradient_abs_delta_max": float(np.abs(gr - go).max()),
    }


def _absolute_probe(rgba: np.ndarray) -> dict:
    a = rgba[..., 3]
    rgb = rgba[..., :3]
    visible = a > 0.5
    near = (a > VISIBLE_ALPHA) & (a <= NEAR_ALPHA)
    hidden = a <= VISIBLE_ALPHA
    return {
        "visible_max": float(rgb[visible].max()) if visible.any() else None,
        "near_transparent_max": float(rgb[near].max()) if near.any() else None,
        "near_transparent_channels_gt_1": int(np.sum(rgb[near] > 1.0)) if near.any() else 0,
        "near_transparent_channels_gt_10": int(np.sum(rgb[near] > 10.0)) if near.any() else 0,
        "hidden_rgb_max": float(rgb[hidden].max()) if hidden.any() else None,
        "nonfinite_count": int(np.sum(~np.isfinite(rgb))),
        "alpha_min": float(a.min()), "alpha_max": float(a.max()),
    }


def _recompose_error(layers: dict, spec: dict) -> dict:
    premul, alpha, divisor = mr.recompose(layers, _neutral_spec(spec))
    orig = layers["premul"]
    diff = np.abs(premul.astype(np.float64) - orig.astype(np.float64))
    return {
        "premul_max_abs_delta": float(diff.max()),
        "premul_mean_abs_delta": float(diff.mean()),
        "alpha_max_abs_delta": float(np.abs(alpha.astype(np.float64) - layers["alpha"].astype(np.float64)).max()),
        "divisor_max_abs_delta": float(np.abs(divisor.astype(np.float64) - layers["alpha"].astype(np.float64)).max()),
        "note": "base + detail must reconstruct the premultiplied input (float rounding only)",
    }


def run(output_rd0: Path, output_rd1: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd1.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd1 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd1 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "MaterialSeparationLab", "rd": "rd1", "stage": "candidate_outputs",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "candidates": CANDIDATES, "fixtures": {}, "descriptor_check": {}, "timing": {},
        "definitions": {
            "space": "premultiplied linear float; unpremultiply once at the end",
            "neutral": "base_gain = detail_gain = coarse_gain = fine_gain = 1.0",
            "zero_detail": "detail gains 0.0 (base only)",
            "alpha_eps": mr.ALPHA_EPS,
        },
    }
    outputs = {}
    blind_records = []

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small":
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        rois = entry["rois"]
        metrics["fixtures"][key] = {}
        for cand_name, spec in CANDIDATES.items():
            layers = mr.decompose(rgba, spec, "straight")
            neutral = mr.apply_candidate(rgba, _neutral_spec(spec), descriptor="straight")
            zero = mr.apply_candidate(rgba, _zero_spec(spec), descriptor="straight")
            result = mr.apply_candidate(rgba, spec, descriptor="straight")

            stem = f"{key}_{cand_name}"
            np.save(cand_dir / (stem + ".npy"), result.astype(np.float32))
            cv2.imwrite(str(cand_dir / (stem + ".png")),
                        cv2.cvtColor(gf.linear_to_srgb_u8(result[..., :3]), cv2.COLOR_RGB2BGR))
            npy, png = cand_dir / (stem + ".npy"), cand_dir / (stem + ".png")
            outputs[stem] = {"npy": npy.name, "npy_sha256": sha256(npy.read_bytes()),
                             "png": png.name, "png_sha256": sha256(png.read_bytes())}

            # base / detail layer previews for visual review
            base_rgb = layers["base"]
            detail_rgb = layers["detail"]
            for label, layer in (("base", base_rgb), ("detail", detail_rgb)):
                p = cand_dir / f"{stem}_{label}.png"
                if label == "detail":
                    vis = np.clip(layer * 4.0 + 0.5, 0.0, 1.0)
                else:
                    vis = np.clip(layer, 0.0, 1.0)
                cv2.imwrite(str(p), cv2.cvtColor(gf.linear_to_srgb_u8(vis), cv2.COLOR_RGB2BGR))
                outputs[f"{stem}_{label}"] = {"png": p.name, "png_sha256": sha256(p.read_bytes())}
            p = cand_dir / f"{stem}_baseonly.png"
            cv2.imwrite(str(p), cv2.cvtColor(gf.linear_to_srgb_u8(zero[..., :3]), cv2.COLOR_RGB2BGR))
            outputs[f"{stem}_baseonly"] = {"png": p.name, "png_sha256": sha256(p.read_bytes())}

            visible = rgba[..., 3] > VISIBLE_ALPHA
            opaque = rgba[..., 3] > 0.5
            near_visible = rgba[..., 3] > VISIBLE_ALPHA
            neutral_delta = float(np.max(np.abs(neutral[..., :3].astype(np.float64) - rgba[..., :3].astype(np.float64))[visible]))
            zero_delta = float(np.max(np.abs(zero[..., :3].astype(np.float64) - rgba[..., :3].astype(np.float64))[visible]))
            neutral_layers = mr.decompose(rgba, _neutral_spec(spec), "straight")
            neutral_premul, _, _ = mr.recompose(neutral_layers, _neutral_spec(spec))
            premul_delta = float(np.max(np.abs(neutral_premul.astype(np.float64)
                                                - neutral_layers["premul"].astype(np.float64))))

            per = {
                "detail_amplitude": mr.detail_statistics(layers),
                "recompose_error": _recompose_error(layers, spec),
                "neutrality": {
                    "premul_max_abs_delta_vs_input": premul_delta,
                    "max_abs_delta_vs_input_visible": neutral_delta,
                    "max_abs_delta_vs_input_alpha_gt_eps": float(np.max(np.abs(
                        neutral[..., :3].astype(np.float64) - rgba[..., :3].astype(np.float64))[near_visible])),
                    "hidden_rgb_note": ("pixels with alpha <= ALPHA_EPS carry hidden RGB in the input and "
                                        "RGB = 0 in the output by the unpremultiply contract; they are "
                                        "excluded from the visible figures by the same threshold"),
                    "max_abs_delta_vs_input_opaque": float(np.max(np.abs(
                        neutral[..., :3].astype(np.float64) - rgba[..., :3].astype(np.float64))[opaque])),
                    "srgb8_max_abs_delta_vs_input_visible": int(np.max(np.abs(
                        gf.linear_to_srgb_u8(neutral[..., :3]).astype(np.int32)
                        - gf.linear_to_srgb_u8(rgba[..., :3]).astype(np.int32))[visible])),
                    "srgb8_max_abs_delta_vs_input_opaque": int(np.max(np.abs(
                        gf.linear_to_srgb_u8(neutral[..., :3]).astype(np.int32)
                        - gf.linear_to_srgb_u8(rgba[..., :3]).astype(np.int32))[opaque])),
                    "note": ("neutral gains must reproduce the input; the premultiplied-space delta is exact, "
                             "the straight-space delta also carries the unpremultiply division at near-transparent pixels"),
                },
                "zero_detail_output": {
                    "max_abs_delta_vs_input_visible": zero_delta,
                    "absolute_probe": _absolute_probe(zero),
                },
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, {"description": "input image (no-op baseline)"}),
                "absolute_probe": _absolute_probe(result),
            }
            if "gray_ramp" in rois:
                per["ramp_profile"] = ramp_profile(result, rois["gray_ramp"])
                per["ramp_profile_input"] = ramp_profile(rgba, rois["gray_ramp"])
            per["local_contrast"] = local_contrast(result, rois)
            per["local_contrast_base_layer"] = local_contrast(
                np.concatenate([np.clip(layers["base"], 0.0, None), np.ones(rgba.shape[:2] + (1,), np.float32)], axis=2), rois)
            if "boundary" in rois:
                per["halo_probe"] = gradient_reversal(result, rois["boundary"], reference=rgba)
                per["halo_probe_baseonly"] = gradient_reversal(zero, rois["boundary"], reference=rgba)

            if "premultiplied" in entry:
                premul_in = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
                premul_result = mr.apply_candidate(premul_in, spec, descriptor="premultiplied")
                delta = float(np.max(np.abs(premul_result[..., :3].astype(np.float64) - result[..., :3].astype(np.float64))))
                per["alpha_contract"] = {
                    "recovered_straight_rgb_max_delta": delta,
                    "alpha_max_abs_delta": float(np.max(np.abs(
                        premul_result[..., 3].astype(np.float64) - result[..., 3].astype(np.float64)))),
                    "near_transparent_abs": _absolute_probe(result),
                    "near_transparent_abs_premultiplied_input": _absolute_probe(premul_result),
                    "note": "straight and premultiplied inputs must recover the same straight RGB",
                }
                metrics["descriptor_check"][stem] = {
                    "recovered_straight_rgb_max_delta": delta,
                    "note": "descriptive; tolerance fixed by reviewer",
                }

            metrics["fixtures"][key][cand_name] = per
            blind_records.append({"fixture": key, "candidate": cand_name})

    # temporal behaviour on the 0.5 px/frame sequence (descriptive only)
    seq = fm["fixtures"]["sequence_small"]
    seq_frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    metrics["sequence_small"] = {"input": temporal(seq_frames, seq["rois"]), "candidates": {}}
    for cand_name, spec in CANDIDATES.items():
        out_frames = [mr.apply_candidate(f, spec, descriptor="straight") for f in seq_frames]
        metrics["sequence_small"]["candidates"][cand_name] = temporal(out_frames, seq["rois"])

    # timing / memory on the large fixture (512x320 and 1536x1024)
    for res in ("small", "large"):
        key = f"texture_{res}"
        if key not in fm["fixtures"]:
            continue
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][key]["npy"])
        for cand_name, spec in CANDIDATES.items():
            mr.apply_candidate(rgba, _neutral_spec(spec), descriptor="straight")
            tracemalloc.start()
            t0 = time.perf_counter()
            for _ in range(3):
                mr.apply_candidate(rgba, spec, descriptor="straight")
            elapsed = (time.perf_counter() - t0) / 3.0
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            metrics["timing"][f"{key}_{cand_name}"] = {
                "seconds_per_run": elapsed,
                "tracemalloc_peak_bytes": int(peak),
                "pixels": int(rgba.shape[0] * rgba.shape[1]),
                "note": "Python heap only; offline reference model",
            }

    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(blind_records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"C{i + 1:02d}"
        rec = blind_records[int(idx)]
        blind_key[bid] = rec
        items.append((bid, cand_dir / (rec["fixture"] + "_" + rec["candidate"] + ".png")))
    sheet = review_dir / "rd1_blind_sheet.png"
    contact_sheet(items, 5, 240, 170).save(sheet)
    key_path = output_rd1 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "CANDIDATE_BLIND_FORM.md"
    form.write_text(
        "# RD1 candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed by a separate reviewer.\n\n"
        "| blind ID | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n"
        + "".join(f"| {bid} | | | | | |\n" for bid in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd1 / "metrics_rd1.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_tables(review_dir, metrics)
    manifest = {
        "lab": "MaterialSeparationLab", "rd": "rd1", "stage": "candidate_outputs", "date": "2026-11-18",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "blind_seed": BLIND_SEED, "candidates": CANDIDATES, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
            "tables": {"file": "review/RD1_TABLES.md",
                       "sha256": sha256((review_dir / "RD1_TABLES.md").read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": ["human visual review and quality gate", "reference material photos",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd1 / "manifest_rd1.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def write_tables(review_dir: Path, metrics: dict) -> None:
    lines = [
        "# RD1 tables (descriptive; gates unset)", "",
        "数値は記述的な測定値である。合否判定・順位付けは行っていない。gate は別レビュー者が baseline 取得後に記入する。", "",
        "## 中立性（gain=1.0 で入力が再現されるか）", "",
        "| fixture | candidate | premul max abs delta | straight max abs delta (alpha>eps) | straight max abs delta (opaque) | srgb8 max (opaque) |",
        "|---|---|---|---|---|---|",
    ]
    for key, per in metrics["fixtures"].items():
        for cand, v in per.items():
            n = v["neutrality"]
            lines.append(f"| {key} | {cand} | {n['premul_max_abs_delta_vs_input']:.3e} "
                         f"| {n['max_abs_delta_vs_input_alpha_gt_eps']:.3e} "
                         f"| {n['max_abs_delta_vs_input_opaque']:.3e} "
                         f"| {n['srgb8_max_abs_delta_vs_input_opaque']} |")
    lines += ["", "## descriptor 差（straight 入力と premultiplied 入力から復元した straight RGB の最大差）", "",
              "| fixture | candidate | max abs delta | alpha max abs delta |",
              "|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for cand, v in per.items():
            if "alpha_contract" not in v:
                continue
            a = v["alpha_contract"]
            lines.append(f"| {key} | {cand} | {a['recovered_straight_rgb_max_delta']:.3e} "
                         f"| {a['alpha_max_abs_delta']:.3e} |")
    lines += ["", "## detail 層の振幅（premultiplied 空間）", "",
              "| fixture | candidate | detail abs mean | abs std | abs p95 | abs p99 | abs max | relative mean |",
              "|---|---|---|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for cand, v in per.items():
            d = v["detail_amplitude"]
            lines.append(f"| {key} | {cand} | {d['detail_abs_mean']:.6f} | {d['detail_abs_std']:.6f} "
                         f"| {d['detail_abs_p95']:.6f} | {d['detail_abs_p99']:.6f} | {d['detail_abs_max']:.6f} "
                         f"| {d['detail_relative_mean']:.4f} |")
    lines += ["", "## 再合成誤差（base + detail が premultiplied 入力を復元するか）", "",
              "| fixture | candidate | premul max abs delta | premul mean abs delta | alpha max abs delta |",
              "|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for cand, v in per.items():
            r = v["recompose_error"]
            lines.append(f"| {key} | {cand} | {r['premul_max_abs_delta']:.3e} "
                         f"| {r['premul_mean_abs_delta']:.3e} | {r['alpha_max_abs_delta']:.3e} |")
    lines += ["", "## 中間調ランプ（灰色ランプの段差）", "",
              "| fixture | candidate | step mean | step std | step min | step max | monotonic fraction |",
              "|---|---|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        for cand, v in per.items():
            if "ramp_profile" not in v:
                continue
            r = v["ramp_profile"]
            lines.append(f"| {key} | {cand} | {r['step_mean']:.6f} | {r['step_std']:.6f} "
                         f"| {r['step_min']:.6f} | {r['step_max']:.6f} | {r['monotonic_fraction']:.4f} |")
    lines += ["", "## 局所コントラスト（平坦部 std / 忙丙部 std、texture fixture）", "",
              "| candidate | pair | flat std | busy std | ratio |", "|---|---|---|---|---|"]
    for cand, v in metrics["fixtures"].get("texture_small", {}).items():
        for pair, lv in v["local_contrast"].items():
            lines.append(f"| {cand} | {pair} | {lv['flat_std']:.6f} | {lv['busy_std']:.6f} | {lv['ratio']:.3f} |")
    lines += ["", "## 絶対値・非有限値（近透明領域、alpha 境界 fixture）", "",
              "| fixture | candidate | near-transparent max | channels > 1 | nonfinite | visible max |",
              "|---|---|---|---|---|---|"]
    for key, per in metrics["fixtures"].items():
        if not key.startswith("alpha_boundary"):
            continue
        for cand, v in per.items():
            a = v["absolute_probe"]
            lines.append(f"| {key} | {cand} | {a['near_transparent_max']} | {a['near_transparent_channels_gt_1']} "
                         f"| {a['nonfinite_count']} | {a['visible_max']} |")
    lines += ["", "## 実行時間・メモリ（Python heap、3 回平均）", "",
              "| fixture | candidate | seconds / run | tracemalloc peak (MB) | pixels |",
              "|---|---|---|---|---|"]
    for k, v in metrics["timing"].items():
        lines.append(f"| {k.rsplit('_', 1)[0] if False else k} | {CANDIDATES_NAME(k)} | "
                     f"{v['seconds_per_run']:.4f} | {v['tracemalloc_peak_bytes'] / (1024 * 1024):.1f} | {v['pixels']} |")
    (review_dir / "RD1_TABLES.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def CANDIDATES_NAME(key: str) -> str:
    for name in CANDIDATES:
        if key.endswith("_" + name):
            return name
    return "?"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="captures/rd0")
    parser.add_argument("--output", default="captures/rd1")
    args = parser.parse_args()
    rd0, rd1 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, rd1)
    print(f"RD1 candidate outputs complete: {len(manifest['outputs'])} outputs -> {rd1}")


if __name__ == "__main__":
    main()
