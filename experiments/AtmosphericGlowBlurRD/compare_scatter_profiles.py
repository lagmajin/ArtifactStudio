"""Compare source-selected scattering kernels at matched total luma energy.

Offline equation study only. It uses the same highlight mask and source seed
for every model; it does not execute Artifact code or claim acceptance.
"""

from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

from atmospheric_glow_blur_lab import (ROOT, blur, preview_bgra, region_masks,
                                       smooth_threshold, summarize, synthetic_fixture)


LAB = Path(__file__).resolve().parent
OUTPUT = LAB / "captures" / "scatter-profile-rd1"
WIDTH, HEIGHT = 960, 540
THRESHOLD, KNEE = 0.75, 0.15
AMOUNT, BROAD_FRACTION, VEIL_FRACTION = 0.70, 0.35, 0.08
LOCAL_SIGMA, GLOW_SIGMAS, GLOW_WEIGHTS = 5.0, (5.0, 9.5, 14.0), (0.5, 0.3, 0.2)
BROAD_SIGMA, VEIL_SIGMA = 24.0, 60.0
LUMA = np.array((0.2126, 0.7152, 0.0722), np.float64)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def impulse_profile(model: str, size: int = 513) -> tuple[np.ndarray, float]:
    impulse = np.zeros((size, size), np.float32)
    impulse[size // 2, size // 2] = 1.0
    if model == "single_gaussian_glow_rms":
        # Match the second moment of the fixed three-scale Glow-like profile.
        sigma = float(np.sqrt(sum(w * s * s for w, s in zip(GLOW_WEIGHTS, GLOW_SIGMAS))))
        response = blur(impulse, sigma)
    elif model == "multiscale_glow":
        sigma = float(np.sqrt(sum(w * s * s for w, s in zip(GLOW_WEIGHTS, GLOW_SIGMAS))))
        response = sum(w * blur(impulse, s) for w, s in zip(GLOW_WEIGHTS, GLOW_SIGMAS))
    elif model == "single_gaussian_atmosphere_rms":
        weights = (AMOUNT * (1.0 - BROAD_FRACTION), AMOUNT * BROAD_FRACTION, VEIL_FRACTION)
        sigmas = (LOCAL_SIGMA, BROAD_SIGMA, VEIL_SIGMA)
        sigma = float(np.sqrt(sum(w * s * s for w, s in zip(weights, sigmas)) / sum(weights)))
        response = blur(impulse, sigma)
    elif model == "local_broad_veil":
        sigma = float(np.sqrt((AMOUNT * (1.0 - BROAD_FRACTION) * LOCAL_SIGMA ** 2 +
                               AMOUNT * BROAD_FRACTION * BROAD_SIGMA ** 2 +
                               VEIL_FRACTION * VEIL_SIGMA ** 2) /
                              (AMOUNT + VEIL_FRACTION)))
        response = ((AMOUNT * (1.0 - BROAD_FRACTION)) * blur(impulse, LOCAL_SIGMA) +
                    (AMOUNT * BROAD_FRACTION) * blur(impulse, BROAD_SIGMA) +
                    VEIL_FRACTION * blur(impulse, VEIL_SIGMA))
    else:
        raise ValueError(model)
    return response.astype(np.float64), sigma


def psf_metrics(response: np.ndarray) -> dict[str, object]:
    energy = np.maximum(response.astype(np.float64), 0.0)
    total = float(energy.sum())
    yy, xx = np.mgrid[0:energy.shape[0], 0:energy.shape[1]].astype(np.float64)
    cy = float((yy * energy).sum() / total)
    cx = float((xx * energy).sum() / total)
    dx, dy = xx - cx, yy - cy
    covariance = np.array(((np.sum(energy * dx * dx), np.sum(energy * dx * dy)),
                           (np.sum(energy * dx * dy), np.sum(energy * dy * dy))), np.float64) / total
    radius = np.sqrt(dx * dx + dy * dy).ravel()
    sorted_idx = np.argsort(radius)
    cumulative = np.cumsum(energy.ravel()[sorted_idx]) / total
    radii = {f"r{p}": float(radius[sorted_idx[np.searchsorted(cumulative, p / 100.0)]])
             for p in (50, 90, 95, 99)}
    return {"sum": total, "centroid_xy": [cx, cy], "covariance_xy": covariance.tolist(),
            "radial_percentiles_px": radii, "max": float(energy.max()),
            "pixels_above_1e-5_peak": int(np.count_nonzero(energy > energy.max() * 1e-5))}


def apply_model(seed: np.ndarray, model: str, sigma: float) -> np.ndarray:
    if model == "single_gaussian_glow_rms":
        return blur(seed, sigma)
    if model == "multiscale_glow":
        return sum(w * blur(seed, s) for w, s in zip(GLOW_WEIGHTS, GLOW_SIGMAS))
    if model == "single_gaussian_atmosphere_rms":
        return blur(seed, sigma)
    if model == "local_broad_veil":
        return ((AMOUNT * (1.0 - BROAD_FRACTION)) * blur(seed, LOCAL_SIGMA) +
                (AMOUNT * BROAD_FRACTION) * blur(seed, BROAD_SIGMA) +
                VEIL_FRACTION * blur(seed, VEIL_SIGMA))
    raise ValueError(model)


def labeled(image: np.ndarray, title: str) -> np.ndarray:
    tile = Image.fromarray(image.copy())
    draw = ImageDraw.Draw(tile)
    draw.rectangle((0, 0, tile.width, 28), fill=(12, 15, 22, 255))
    draw.text((8, 6), title, fill=(244, 246, 250, 255))
    return np.asarray(tile)


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source, boxes = synthetic_fixture(WIDTH, HEIGHT)
    luma = source[..., :3] @ np.array((0.2126, 0.7152, 0.0722), np.float32)
    selection = smooth_threshold(luma, THRESHOLD, KNEE)
    seed = source[..., :3] * selection[..., None]
    source_rgb_energy = float(np.sum(seed @ LUMA))
    models = ("single_gaussian_glow_rms", "multiscale_glow",
              "single_gaussian_atmosphere_rms", "local_broad_veil")
    raw_contributions: dict[str, np.ndarray] = {}
    impulse_records: dict[str, dict[str, object]] = {}
    sigma_values: dict[str, float] = {}
    for model in models:
        impulse, sigma = impulse_profile(model)
        sigma_values[model] = sigma
        impulse_records[model] = {"sigma_parameter_or_rms_reference_px": sigma,
                                  **psf_metrics(impulse)}
        raw_contributions[model] = apply_model(seed, model, sigma)

    target_added_energy = float(np.sum((raw_contributions["multiscale_glow"] @ LUMA)))
    outputs: dict[str, np.ndarray] = {}
    contribution_metrics: dict[str, object] = {}
    masks = region_masks(WIDTH, HEIGHT, boxes)
    baseline_halo: np.ndarray | None = None
    multipliers: dict[str, float] = {}
    for model, contribution in raw_contributions.items():
        added_energy = float(np.sum(contribution @ LUMA))
        scale = target_added_energy / added_energy
        multipliers[model] = scale
        matched = contribution * np.float32(scale)
        output = np.concatenate((source[..., :3] + matched, source[..., 3:4]), axis=2)
        outputs[model] = output.astype(np.float32)
        if model == "multiscale_glow":
            baseline_halo = matched
        np.save(OUTPUT / f"{model}.contribution.rgb.npy", matched, allow_pickle=False)
        np.save(OUTPUT / f"{model}.output.rgba.npy", outputs[model], allow_pickle=False)
        cv2.imwrite(str(OUTPUT / f"{model}.preview.png"), preview_bgra(outputs[model]))
        per_region: dict[str, object] = {}
        for name, mask in masks.items():
            added = matched[mask].astype(np.float64)
            added_luma = added @ LUMA
            out_rgb = output[mask, :3].astype(np.float64)
            per_region[name] = {
                "pixels": int(mask.sum()),
                "added_luma_sum": float(added_luma.sum()),
                "added_luma_p50_p95_p99": np.percentile(added_luma, (50, 95, 99)).tolist(),
                "output_rgb_min_max_mean": [np.min(out_rgb, axis=0).tolist(),
                                            np.max(out_rgb, axis=0).tolist(),
                                            np.mean(out_rgb, axis=0).tolist()],
                "output_non_finite": int(out_rgb.size - np.isfinite(out_rgb).sum()),
            }
        fine = masks["fine_detail"]
        detail_in = source[fine, :3].astype(np.float64)
        detail_out = output[fine, :3].astype(np.float64)
        detail_metrics = {
            "input_luma_std": float(np.std(detail_in @ LUMA)),
            "output_luma_std": float(np.std(detail_out @ LUMA)),
            "luma_std_change": float(np.std(detail_out @ LUMA) - np.std(detail_in @ LUMA)),
            "added_energy_in_fine_detail_roi": per_region["fine_detail"]["added_luma_sum"],
        }
        contribution_metrics[model] = {
            "raw_added_luma_energy": added_energy,
            "energy_match_scale": scale,
            "matched_added_luma_energy": float(np.sum(matched @ LUMA)),
            "detail_roi": detail_metrics,
            "regions": per_region,
            "output_summary": {name: summarize(output, mask) for name, mask in masks.items()},
        }

    if baseline_halo is None:
        raise RuntimeError("missing equal-energy reference")
    comparison_tiles = [labeled(preview_bgra(source), "INPUT")]
    for model in models:
        comparison_tiles.append(labeled(preview_bgra(outputs[model]), model))
    contact = cv2.hconcat(comparison_tiles)
    cv2.imwrite(str(OUTPUT / "equal-energy-contact.review.png"), contact)

    psf_impulse = np.zeros((513, 513), np.float32)
    psf_impulse[256, 256] = 1.0
    for model in models:
        sigma = sigma_values[model]
        raw = apply_model(psf_impulse, model, sigma)
        raw_energy = float(np.sum(raw))
        normalized = raw / np.float32(raw_energy)
        np.save(OUTPUT / f"{model}.normalized-psf.npy", normalized.astype(np.float32), allow_pickle=False)

    record = {
        "title": "Matched-energy source-selected scatter profile comparison",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator_sha256": sha256(Path(__file__).resolve()),
        "input": {"kind": "deterministic synthetic HDR chart", "dimensions": [WIDTH, HEIGHT],
                  "generator_sha256": sha256(LAB / "atmospheric_glow_blur_lab.py"),
                  "highlight_selection": {"method": "smoothstep on scene-linear Rec.709 luma",
                                          "threshold": THRESHOLD, "knee": KNEE},
                  "source_seed_luma_energy": source_rgb_energy},
        "shared_conditions": {"same_source_image": True, "same_selection_mask": True,
                              "same_source_seed": True, "same_scene_linear_color_contract": True,
                              "same_border_mode": "OpenCV BORDER_REFLECT_101",
                              "normalization": "scale each model's scattered RGB so total added scene-linear Rec.709 luma equals the fixed multiscale Glow-like profile on this image"},
        "models": {
            "single_gaussian_glow_rms": {"definition": "one Gaussian on selected seed; sigma matches three-scale Glow-like RMS radius"},
            "multiscale_glow": {"definition": "selected seed blurred at fixed sigmas 5/9.5/14 with weights .5/.3/.2"},
            "single_gaussian_atmosphere_rms": {"definition": "one Gaussian on selected seed; sigma matches atmospheric local/broad/veil RMS radius"},
            "local_broad_veil": {"definition": "selected seed; local sigma5 and broad sigma24 mixed .65/.35 at amount .70 plus sigma60 veil at .08"},
        },
        "normalization_reference_total_added_luma_energy": target_added_energy,
        "impulse_psf_shape": impulse_records,
        "full_image_results": contribution_metrics,
        "interpretation_limits": [
            "This isolates spatial response while holding threshold/source selection constant; it is not a comparison against full-frame Gaussian blur without highlight extraction.",
            "Equal integrated luma energy is an analytical normalization, not equal perceived brightness under any display transform.",
            "Synthetic offline CPU equations only; no Artifact product path, HDR viewer, GPU, temporal behavior, or performance result is measured.",
        ],
    }
    json_path = OUTPUT / "scatter_profile_manifest.json"
    json_path.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    output_files = sorted(p for p in OUTPUT.iterdir() if p.is_file() and p.name not in
                          (json_path.name, "scatter_profile_manifest.sha256"))
    hashes = {p.name: {"sha256": sha256(p), "bytes": p.stat().st_size} for p in output_files}
    record["outputs"] = hashes
    json_path.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    (OUTPUT / "scatter_profile_manifest.sha256").write_text(
        f"{sha256(json_path)}  scatter_profile_manifest.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"output": str(OUTPUT), "models": list(models),
                      "manifest_sha256": sha256(json_path),
                      "matched_energy": target_added_energy}, indent=2))


if __name__ == "__main__":
    main()
