"""Compare the offline candidate with saved ArtifactUiTest CPU Glow PNGs.

Reads existing capture files only; it never launches ArtifactUiTest. Results
are an SDR look comparison against a historical binary, not source-current
verification, HDR evaluation, or an acceptance verdict.
"""

from __future__ import annotations

import hashlib
import json
import math
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import cv2
from PIL import Image

from atmospheric_glow_blur_lab import (atmospheric_glow_blur, blur, linear_to_srgb,
                                       smooth_threshold, srgb_to_linear)


ROOT = Path(__file__).resolve().parents[2]
CAPTURE = ROOT / "experiments" / "GlowLab" / "captures" / "glow-suite-foundation-neon-gain-sweep-cpu-rd1"
PRODUCT_MANIFEST = CAPTURE / "product-suite-manifest.json"
REVIEW_METRICS = CAPTURE / "review" / "glow-foundation-neon-gain-metrics.json"
OUTPUT = Path(__file__).resolve().parent / "captures" / "product-glow-sdr-rd1"
EXE = ROOT / "out" / "build" / "x64-Debug-Agent" / "bin" / "Debug" / "ArtifactUiTest.exe"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_rgba(path: Path) -> np.ndarray:
    with Image.open(path) as im:
        return np.asarray(im.convert("RGBA"), np.float32) / 255.0


def preview_sdr(linear_rgba: np.ndarray) -> np.ndarray:
    rgba = np.concatenate((np.clip(linear_to_srgb(np.maximum(linear_rgba[..., :3], 0.0)), 0, 1),
                           np.clip(linear_rgba[..., 3:4], 0, 1)), axis=2)
    return np.rint(rgba * 255.0).astype(np.uint8)


def label(image: np.ndarray, text: str) -> np.ndarray:
    tile = Image.fromarray(image.copy())
    from PIL import ImageDraw
    draw = ImageDraw.Draw(tile)
    draw.rectangle((0, 0, tile.width, 30), fill=(12, 15, 22, 255))
    draw.text((8, 7), text, fill=(244, 246, 250, 255))
    return np.asarray(tile)


def metrics(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    av = a[mask].astype(np.float64)
    bv = b[mask].astype(np.float64)
    delta = av[..., :3] - bv[..., :3]
    la = srgb_to_linear((av[..., :3] / 255.0).astype(np.float32)).astype(np.float64)
    lb = srgb_to_linear((bv[..., :3] / 255.0).astype(np.float32)).astype(np.float64)
    luma_delta = (la - lb) @ np.array((.2126, .7152, .0722), np.float64)
    return {
        "pixels": int(mask.sum()),
        "encoded_rgb_mae_code": float(np.mean(np.abs(delta))),
        "encoded_rgb_mae_normalized": float(np.mean(np.abs(delta)) / 255.0),
        "encoded_rgb_rmse_code": float(np.sqrt(np.mean(delta * delta))),
        "encoded_rgb_abs_p95_max_code": [float(np.percentile(np.abs(delta), 95)), float(np.max(np.abs(delta)))],
        "decoded_srgb_linear_luma_signed_mean_delta": float(np.mean(luma_delta)),
        "decoded_srgb_linear_luma_abs_p95_max": [float(np.percentile(np.abs(luma_delta), 95)),
                                                   float(np.max(np.abs(luma_delta)))],
        "alpha_mae_code": float(np.mean(np.abs(av[:, 3] - bv[:, 3]))),
        "alpha_mae_normalized": float(np.mean(np.abs(av[:, 3] - bv[:, 3])) / 255.0),
        "candidate_clipped_candidate_unclipped_product_fraction": float(np.mean(
            (bv[:, :3] >= 254.5) & (av[:, :3] < 254.5))),
    }


def glow_cpu_source_equation(source_rgba: np.ndarray, gain: float, *,
                            input_mode: str) -> np.ndarray:
    """Offline mirror of GlowEffectCPUImpl::applyCPU; no product code is called."""
    if input_mode == "linearized_srgb":
        color = srgb_to_linear(source_rgba[..., :3]).astype(np.float32)
    elif input_mode == "raw_srgb_codes":
        color = source_rgba[..., :3].astype(np.float32)
    else:
        raise ValueError(input_mode)
    luma = color[..., 0] * np.float32(.299) + color[..., 1] * np.float32(.587) + color[..., 2] * np.float32(.114)
    mask = np.where(luma > np.float32(.6), luma, np.float32(0.0)).astype(np.float32)
    mask *= np.float32(gain)
    mask = np.minimum(mask, np.float32(1.0))
    seed = color * mask[..., None]

    glow_accum = np.zeros_like(color)
    layer_count = 4
    base_sigma = 6.0
    sigma_growth = 1.8
    base_alpha = .3
    alpha_falloff = .6
    for index in range(layer_count):
        sigma = max(.1, base_sigma + index * sigma_growth)
        estimated = math.ceil(max(.5, sigma) * 2.5)
        kernel_size = max(3, estimated * 2 + 1)
        layer = cv2.GaussianBlur(seed, (kernel_size, kernel_size), sigmaX=sigma,
                                 sigmaY=sigma, borderType=cv2.BORDER_REPLICATE)
        alpha_weight = base_alpha * math.pow(max(0.0, alpha_falloff), index)
        glow_accum += layer * np.float32(alpha_weight)
    alpha_norm = max(0.0001, base_alpha * (1.0 - math.pow(alpha_falloff, layer_count)) /
                     max(0.0001, 1.0 - alpha_falloff))
    result = np.clip(color + glow_accum / np.float32(alpha_norm), 0.0, 1.0)
    return np.concatenate((result, source_rgba[..., 3:4].astype(np.float32)), axis=2)


def atmospheric_transfer_ablation(source_srgb: np.ndarray, amount: float,
                                  *, calculation_space: str) -> np.ndarray:
    """Hold source-selection mask fixed; vary convolution/composite transfer domain."""
    rgb_srgb = source_srgb[..., :3].astype(np.float32)
    rgb_linear = srgb_to_linear(rgb_srgb)
    luma_linear = rgb_linear @ np.array((.2126, .7152, .0722), np.float32)
    mask = smooth_threshold(luma_linear, .75, .15)
    if calculation_space == "scene_linear":
        color = rgb_linear
    elif calculation_space == "encoded_srgb":
        color = rgb_srgb
    else:
        raise ValueError(calculation_space)
    seed = color * mask[..., None]
    local = blur(seed, 5.0 * 1.6)
    broad = blur(seed, 24.0 * 1.6)
    veil = blur(seed, 60.0 * 1.6) * .08
    contribution = (local * .65 + broad * .35) * amount + veil
    result = color + contribution
    return np.concatenate((result, source_srgb[..., 3:4].astype(np.float32)), axis=2)


def run() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    product_manifest = json.loads(PRODUCT_MANIFEST.read_text(encoding="utf-8"))
    review = json.loads(REVIEW_METRICS.read_text(encoding="utf-8"))
    assert product_manifest["capture_class"] == "ArtifactUiTest installed-effect product output"
    assert product_manifest["input_sha256"] == "8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4"
    assert product_manifest["requested_compute"] == "cpu"
    assert review["input_sha256"] == product_manifest["input_sha256"]

    records = product_manifest["entries"]
    source_capture = CAPTURE / records[0]["source_capture"]
    source_digest = sha256(source_capture)
    source = read_rgba(source_capture)
    h, w = source.shape[:2]
    assert [w, h] == [1536, 1025]
    for record in records:
        assert record["source_capture_sha256"] == source_digest
        assert record["properties"]["layerCount"] == 4
        assert record["properties"]["baseSigma"] == 6.0
    for row in review["results"]:
        assert row["effect_id"] == "glow"

    # The profile's normalized manual ROIs are reused as recorded in GlowLab's review.
    roi_masks: dict[str, np.ndarray] = {"whole": np.ones((h, w), bool)}
    for name, (x0, y0, x1, y1) in review["roi_bounds_normalized"].items():
        roi_masks[name] = ((np.arange(w)[None, :] >= int(x0 * w)) &
                           (np.arange(w)[None, :] < int(x1 * w)) &
                           (np.arange(h)[:, None] >= int(y0 * h)) &
                           (np.arange(h)[:, None] < int(y1 * h)))

    source_linear = source.copy()
    source_linear[..., :3] = srgb_to_linear(source[..., :3])
    source_linear = source_linear.astype(np.float32)
    candidate_amounts = [0.10, 0.25, 0.50, 0.70, 1.00]
    scale = w / 960.0
    candidates: dict[float, np.ndarray] = {}
    for amount in candidate_amounts:
        out, _components = atmospheric_glow_blur(
            source_linear, amount=amount, local_sigma=5.0 * scale,
            broad_sigma=24.0 * scale, veil_sigma=60.0 * scale)
        candidates[amount] = preview_sdr(out)

    pair_results: list[dict[str, object]] = []
    for record in records:
        gain = float(record["properties"]["glowGain"])
        product_path = CAPTURE / record["product_output"]
        product_digest = sha256(product_path)
        assert product_digest == record["product_output_sha256"]
        product = read_rgba(product_path)
        assert product.shape == source.shape
        product_u8 = np.rint(product * 255.0).astype(np.uint8)
        for amount, candidate_u8 in candidates.items():
            case_name = f"product-glow-{gain:.2f}_candidate-amount-{amount:.2f}"
            case_metrics = {name: metrics(product_u8, candidate_u8, mask)
                            for name, mask in roi_masks.items()}
            pair_results.append({
                "case": case_name, "product_effect": "glow",
                "product_properties": record["properties"],
                "candidate_parameters": {"amount": amount, "threshold": .75, "knee": .15,
                                          "local_sigma_at_960": 5.0, "broad_sigma_at_960": 24.0,
                                          "broad_fraction": .35, "veil_sigma_at_960": 60.0,
                                          "veil_fraction": .08},
                "product_output": record["product_output"],
                "product_output_sha256": product_digest,
                "candidate_vs_product_sdr_metrics": case_metrics,
            })
        candidate_u8 = candidates[gain]
        diff = np.abs(candidate_u8[..., :3].astype(np.int16) - product_u8[..., :3].astype(np.int16))
        diff_view = np.concatenate((np.clip(diff * 8, 0, 255).astype(np.uint8),
                                    np.full((h, w, 1), 255, np.uint8)), axis=2)
        contact = np.concatenate((label(np.rint(source * 255).astype(np.uint8), "CAPTURED INPUT"),
                                  label(product_u8, f"ARTIFACT CPU GLOW gain={gain:.2f}"),
                                  label(candidate_u8, f"OFFLINE MODEL amount={gain:.2f}"),
                                  label(diff_view, "ABS DIFF x8 (8-bit code values)")), axis=1)
        contact_path = OUTPUT / f"gain-{gain:.2f}-paired-contact.png"
        Image.fromarray(contact).save(contact_path)

    transfer_ablation_records: list[dict[str, object]] = []
    for record in records:
        gain = float(record["properties"]["glowGain"])
        product_path = CAPTURE / record["product_output"]
        product = np.rint(read_rgba(product_path) * 255.0).astype(np.uint8)
        candidate_modes: dict[str, np.ndarray] = {}
        for mode in ("scene_linear", "encoded_srgb"):
            candidate = atmospheric_transfer_ablation(source, gain, calculation_space=mode)
            if mode == "scene_linear":
                candidate_u8 = preview_sdr(candidate)
            else:
                candidate_u8 = np.rint(np.clip(candidate, 0, 1) * 255.0).astype(np.uint8)
            candidate_modes[mode] = candidate_u8
            np.save(OUTPUT / f"gain-{gain:.2f}-candidate-{mode}.rgba.npy", candidate,
                    allow_pickle=False)
        candidate_scene_u8 = candidate_modes["scene_linear"]
        candidate_encoded_u8 = candidate_modes["encoded_srgb"]
        transfer_ablation_records.append({
            "product_gain": gain,
            "candidate_amount": gain,
            "source_selection_mask": "same smooth mask from decoded sRGB linear luminance for both modes",
            "candidate_scene_linear_vs_product": {name: metrics(product, candidate_scene_u8, mask)
                                                    for name, mask in roi_masks.items()},
            "candidate_encoded_srgb_filter_composite_vs_product": {
                name: metrics(product, candidate_encoded_u8, mask) for name, mask in roi_masks.items()},
        })
        scene_diff = np.abs(candidate_scene_u8[..., :3].astype(np.int16) - product[..., :3].astype(np.int16))
        encoded_diff = np.abs(candidate_encoded_u8[..., :3].astype(np.int16) - product[..., :3].astype(np.int16))
        def diff_tile(diff: np.ndarray, title: str) -> np.ndarray:
            rgba = np.concatenate((np.clip(diff * 8, 0, 255).astype(np.uint8),
                                   np.full((h, w, 1), 255, np.uint8)), axis=2)
            return label(rgba, title)
        contact = np.concatenate((label(np.rint(source * 255).astype(np.uint8), "CAPTURED INPUT"),
                                  label(product, f"ARTIFACT GLOW gain={gain:.2f}"),
                                  label(candidate_scene_u8, "CANDIDATE SCENE-LINEAR"),
                                  label(candidate_encoded_u8, "CANDIDATE ENCODED-SPACE"),
                                  diff_tile(scene_diff, "LINEAR DIFF x8"),
                                  diff_tile(encoded_diff, "ENCODED-SPACE DIFF x8")), axis=1)
        Image.fromarray(contact).save(OUTPUT / f"gain-{gain:.2f}-transfer-ablation.review.png")

    source_equation_records: list[dict[str, object]] = []
    for input_mode in ("raw_srgb_codes", "linearized_srgb"):
        for record in records:
            gain = float(record["properties"]["glowGain"])
            product_path = CAPTURE / record["product_output"]
            product = np.rint(read_rgba(product_path) * 255.0).astype(np.uint8)
            mirrored = glow_cpu_source_equation(source, gain, input_mode=input_mode)
            if input_mode == "linearized_srgb":
                mirror_u8 = preview_sdr(mirrored)
            else:
                mirror_u8 = np.rint(np.clip(mirrored, 0.0, 1.0) * 255.0).astype(np.uint8)
            region_metrics = {name: metrics(product, mirror_u8, mask)
                              for name, mask in roi_masks.items()}
            source_equation_records.append({
                "input_mode": input_mode,
                "gain": gain,
                "parameters": {"layerCount": 4, "baseSigma": 6.0,
                               "sigmaGrowth_source_default": 1.8,
                               "baseAlpha_source_default": .3,
                               "alphaFalloff_source_default": .6,
                               "threshold": .6, "comparison_input": "opaque RGBA8 captured PNG"},
                "product_output_sha256": sha256(product_path),
                "mirror_vs_product_metrics": region_metrics,
            })
            if input_mode == "raw_srgb_codes":
                diff = np.abs(mirror_u8[..., :3].astype(np.int16) - product[..., :3].astype(np.int16))
                diff_view = np.concatenate((np.clip(diff * 8, 0, 255).astype(np.uint8),
                                            np.full((h, w, 1), 255, np.uint8)), axis=2)
                contact = np.concatenate((label(np.rint(source * 255).astype(np.uint8), "CAPTURED INPUT"),
                                          label(product, f"ARTIFACT GLOW gain={gain:.2f}"),
                                          label(mirror_u8, "CURRENT-SOURCE EQUATION MIRROR"),
                                          label(diff_view, "ABS DIFF x8")), axis=1)
                Image.fromarray(contact).save(OUTPUT / f"source-equation-gain-{gain:.2f}.review.png")

    binary_hash = sha256(EXE) if EXE.is_file() else None
    recorded_binary_hash = product_manifest.get("exe_sha256")
    glow_source_rel = Path("src/Effects/Glow/GlowEffect.cpp")
    layer_source_rel = Path("src/Layer/ArtifactImageLayer.cpp")
    artifact_revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT / "Artifact",
                                       capture_output=True, text=True, check=True).stdout.strip()
    source_worktree_clean = subprocess.run(
        ["git", "diff", "--quiet", "HEAD", "--", str(glow_source_rel), str(layer_source_rel)],
        cwd=ROOT / "Artifact", check=False).returncode == 0
    manifest: dict[str, object] = {
        "title": "Historical Artifact installed Glow CPU output versus offline Atmospheric Glow Blur model",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator_sha256": sha256(Path(__file__).resolve()),
        "capture_input": {"path": str(source_capture.relative_to(ROOT)), "sha256": source_digest,
                          "dimensions": [w, h], "mode": "RGBA8 PNG; treated as sRGB by this comparison"},
        "product_capture_manifest": {"path": str(PRODUCT_MANIFEST.relative_to(ROOT)),
                                     "sha256": sha256(PRODUCT_MANIFEST)},
        "product_effect": "glow (installed CPU reference), gain .10/.25/.50/1.00, 4 layers, baseSigma 6",
        "binary": {"recorded_sha256": recorded_binary_hash,
                   "local_binary_sha256": binary_hash,
                   "binary_hash_matches_capture": binary_hash == recorded_binary_hash,
                   "source_revision_recorded": False},
        "product_capture_class": product_manifest["capture_class"],
        "not_final_viewer_or_gpu_output": True,
        "candidate_class": "offline CPU equation; no Artifact product code invoked",
        "candidate_values": candidate_amounts,
        "matched_source_scale_sigma": {"resolution_scale": scale, "local_sigma": 5 * scale,
                                        "broad_sigma": 24 * scale, "veil_sigma": 60 * scale},
        "transfer_assumption": "decode captured PNG RGB as sRGB; candidate encoded with sRGB OETF and clipped to SDR for comparison",
        "roi_basis": review["roi_bounds_normalized"],
        "metrics_note": "Look comparison only. Product result is 8-bit; no HDR, verified current-source parity, visual acceptance, or performance conclusion.",
        "pairs": pair_results,
        "current_source_equation_mirror": {
            "source_file": "Artifact/src/Effects/Glow/GlowEffect.cpp::GlowEffectCPUImpl::applyCPU",
            "input_preparation_source_file": "Artifact/src/Layer/ArtifactImageLayer.cpp::toFrameBuffer",
            "artifact_repository_revision": artifact_revision,
            "current_source_file_sha256": {
                str(glow_source_rel): sha256(ROOT / "Artifact" / glow_source_rel),
                str(layer_source_rel): sha256(ROOT / "Artifact" / layer_source_rel),
            },
            "these_source_files_clean_at_capture": source_worktree_clean,
            "formula_facts": ["hard luma threshold at 0.6", "four Gaussian layers",
                              "sigma=baseSigma+i*sigmaGrowth", "kernel width from ceil(2.5*sigma)",
                              "weights baseAlpha*alphaFalloff^i then normalize",
                              "RGB add and clamp to [0,1]", "source alpha is copied"],
            "input_interpretation_cases": ["raw_srgb_codes", "linearized_srgb"],
            "records": source_equation_records,
            "interpretation_limit": "Equation mirror comparison to a historical binary capture. Binary hash matches the local executable, but source revision is unrecorded; this is not verified runtime parity.",
        },
        "candidate_transfer_space_ablation": {
            "purpose": "Test how much SDR difference changes when only the candidate convolution/composite domain changes while its source-selection mask is held fixed.",
            "selection_mask": "smooth threshold of decoded-sRGB scene-linear luma, threshold .75, knee .15",
            "models": ["scene_linear", "encoded_srgb"],
            "records": transfer_ablation_records,
            "interpretation_limit": "The encoded-space variant is a diagnostic control, not the preferred physically-based model or a product implementation.",
        },
    }
    manifest_path = OUTPUT / "comparison_manifest.json"
    output_files = sorted(p for p in OUTPUT.iterdir() if p.is_file() and p.name != manifest_path.name)
    manifest["outputs"] = {p.name: {"sha256": sha256(p), "bytes": p.stat().st_size}
                            for p in output_files}
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest_hash = sha256(manifest_path)
    (OUTPUT / "comparison_manifest.sha256").write_text(
        f"{manifest_hash}  comparison_manifest.json\n", encoding="ascii")
    print(json.dumps({"output": str(OUTPUT.resolve()), "pair_count": len(pair_results),
                      "recorded_binary_hash": recorded_binary_hash,
                      "local_binary_hash_matches": binary_hash == recorded_binary_hash,
                      "manifest_sha256": manifest_hash}, indent=2))


if __name__ == "__main__":
    run()
