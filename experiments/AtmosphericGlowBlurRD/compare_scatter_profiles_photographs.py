"""Matched-energy scatter profiles on the two provenance-checked CC0 plates."""

from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np

from atmospheric_glow_blur_lab import (ASSETS, blur, load_cc0_plate, preview_bgra,
                                       region_masks, scale_boxes, smooth_threshold,
                                       srgb_to_linear)
from compare_scatter_profiles import (AMOUNT, BROAD_FRACTION, BROAD_SIGMA, GLOW_SIGMAS,
                                      GLOW_WEIGHTS, KNEE, LOCAL_SIGMA, THRESHOLD,
                                      VEIL_FRACTION, VEIL_SIGMA, apply_model,
                                      impulse_profile, labeled)


LAB = Path(__file__).resolve().parent
OUTPUT = LAB / "captures" / "scatter-photo-rd1"
WIDTH, HEIGHT = 960, 540
LUMA = np.array((0.2126, 0.7152, 0.0722), np.float64)
MODELS = ("single_gaussian_glow_rms", "multiscale_glow",
          "single_gaussian_atmosphere_rms", "local_broad_veil")
PHOTO_ROIS = {
    "cc0-neon": ((1536, 1025), {
        "central_intersection": [690, 360, 980, 700],
        "right_practicals": [1130, 330, 1500, 720],
        "lower_road_detail": [450, 640, 850, 980]}),
    "cc0-streetlight": ((576, 1280), {
        "left_lamp": [80, 390, 205, 525],
        "right_lamp": [310, 390, 435, 525],
        "sky_field": [120, 120, 450, 360]}),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    impulse_records = {}
    sigma_values = {}
    for model in MODELS:
        response, sigma = impulse_profile(model)
        energy = np.maximum(response, 0.0)
        total = float(energy.sum())
        yy, xx = np.mgrid[:energy.shape[0], :energy.shape[1]].astype(np.float64)
        cx, cy = float((xx * energy).sum() / total), float((yy * energy).sum() / total)
        radius = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2).ravel()
        order = np.argsort(radius)
        cdf = np.cumsum(energy.ravel()[order]) / total
        impulse_records[model] = {"sigma_or_rms_px": sigma,
                                  "r50_r95_r99_px": [float(radius[order[np.searchsorted(cdf, q)]])
                                                       for q in (.50, .95, .99)]}
        sigma_values[model] = sigma

    cases: dict[str, object] = {}
    assets = {
        "cc0-neon": ("cc0-neon-streets-1536.jpg", (1536, 1025), {
            "central_intersection": [690, 360, 980, 700],
            "right_practicals": [1130, 330, 1500, 720],
            "lower_road_detail": [450, 640, 850, 980]}),
        "cc0-streetlight": ("cc0-street-light-night.jpg", (576, 1280), {
            "left_lamp": [80, 390, 205, 525],
            "right_lamp": [310, 390, 435, 525],
            "sky_field": [120, 120, 450, 360]}),
    }
    for case, (filename, native_size, native_rois) in assets.items():
        path = ASSETS / filename
        source, input_hash, image_metadata = load_cc0_plate(path, (WIDTH, HEIGHT))
        boxes = scale_boxes(native_rois, native_size, (WIDTH, HEIGHT))
        masks = region_masks(WIDTH, HEIGHT, boxes)
        luma = source[..., :3] @ np.array((0.2126, 0.7152, 0.0722), np.float32)
        selection = smooth_threshold(luma, THRESHOLD, KNEE)
        seed = source[..., :3] * selection[..., None]
        raw = {model: apply_model(seed, model, sigma_values[model]) for model in MODELS}
        target_energy = float(np.sum(raw["multiscale_glow"] @ LUMA))
        records: dict[str, object] = {}
        tiles = [labeled(preview_bgra(source), "INPUT")]
        for model in MODELS:
            contribution = raw[model]
            raw_energy = float(np.sum(contribution @ LUMA))
            scale = target_energy / raw_energy
            matched = contribution * np.float32(scale)
            output = np.concatenate((source[..., :3] + matched, source[..., 3:4]), axis=2)
            np.save(OUTPUT / f"{case}.{model}.contribution.rgb.npy", matched, allow_pickle=False)
            output_path = OUTPUT / f"{case}.{model}.output.rgba.npy"
            np.save(output_path, output.astype(np.float32), allow_pickle=False)
            preview_path = OUTPUT / f"{case}.{model}.preview.png"
            preview = preview_bgra(output)
            cv2.imwrite(str(preview_path), preview)
            tiles.append(labeled(preview, model))
            display_rgb = preview[..., (2, 1, 0)].astype(np.float32) / 255.0
            display_luma = srgb_to_linear(display_rgb) @ np.array((0.2126, 0.7152, 0.0722), np.float32)
            regions = {}
            for region, mask in masks.items():
                added_luma = matched[mask].astype(np.float64) @ LUMA
                input_luma = source[mask, :3].astype(np.float64) @ LUMA
                values = output[mask, :3].astype(np.float64)
                output_luma = values @ LUMA
                regions[region] = {
                    "pixels": int(mask.sum()),
                    "added_luma_sum": float(added_luma.sum()),
                    "added_luma_mean": float(added_luma.mean()),
                    "input_luma_std": float(np.std(input_luma)),
                    "output_luma_std": float(np.std(output_luma)),
                    "luma_std_change": float(np.std(output_luma) - np.std(input_luma)),
                    "output_luma_p50_p95_p99": np.percentile(output_luma, (50, 95, 99)).tolist(),
                    "preview_display_luma_mean_p50_p95": [float(np.mean(display_luma[mask])),
                                                           *np.percentile(display_luma[mask], (50, 95)).tolist()],
                    "output_rgb_mean": np.mean(values, axis=0).tolist(),
                    "non_finite": int(values.size - np.isfinite(values).sum()),
                }
            records[model] = {"raw_added_luma_energy": raw_energy,
                              "energy_match_scale": scale,
                              "matched_added_luma_energy": float(np.sum(matched @ LUMA)),
                              "regions": regions}
        contact_path = OUTPUT / f"{case}.equal-energy-contact.review.png"
        cv2.imwrite(str(contact_path), cv2.hconcat(tiles))
        cases[case] = {
            "input": {"path": f"experiments/GlowLab/assets/{filename}",
                      "sha256": input_hash, "dimensions": [WIDTH, HEIGHT],
                      "original_dimensions": [1536, 1025] if case == "cc0-neon" else [576, 1280],
                      "license": "CC0 1.0 per experiments/GlowLab/assets/SOURCES.md",
                      "metadata": image_metadata},
            "roi_boxes_xyxy": boxes,
            "highlight_selection": {"space": "scene-linear Rec.709 luma",
                                     "threshold": THRESHOLD, "knee": KNEE},
            "normalization_reference_added_luma_energy": target_energy,
            "models": records,
            "contact_sheet": contact_path.name,
        }
    manifest = {
        "title": "Matched-energy scatter profile response on CC0 photographic plates",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator_sha256": sha256(Path(__file__).resolve()),
        "dependency_script_sha256": sha256(LAB / "compare_scatter_profiles.py"),
        "shared_settings": {"dimensions": [WIDTH, HEIGHT], "threshold": THRESHOLD, "knee": KNEE,
                            "local_sigma": LOCAL_SIGMA, "glow_sigmas": GLOW_SIGMAS,
                            "glow_weights": GLOW_WEIGHTS, "broad_sigma": BROAD_SIGMA,
                            "broad_fraction": BROAD_FRACTION, "veil_sigma": VEIL_SIGMA,
                            "veil_fraction": VEIL_FRACTION, "amount": AMOUNT,
                            "preview_transform": "Reinhard x/(1+x) on linear RGB, then sRGB OETF; display-luma metrics decode the resulting 8-bit PNG as sRGB",
                            "normalization": "per-image total added scene-linear Rec.709 luma equals that image's multiscale Glow-like control"},
        "impulse_psf_shape": impulse_records,
        "cases": cases,
        "interpretation_limits": [
            "Photographic assets are examples, not physical or perceptual ground truth.",
            "Energy normalization is per image and does not imply equal perceived brightness.",
            "Offline CPU equation responses only; no product, HDR viewer, GPU, temporal, or performance result.",
        ],
    }
    json_path = OUTPUT / "scatter_photo_manifest.json"
    files = {p.name: {"sha256": sha256(p), "bytes": p.stat().st_size}
             for p in sorted(OUTPUT.iterdir()) if p.is_file() and p.name not in
             (json_path.name, "scatter_photo_manifest.sha256")}
    manifest["outputs"] = files
    json_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8", newline="\n")
    (OUTPUT / "scatter_photo_manifest.sha256").write_text(
        f"{sha256(json_path)}  scatter_photo_manifest.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"output": str(OUTPUT), "cases": list(cases),
                      "manifest_sha256": sha256(json_path)}, indent=2))


if __name__ == "__main__":
    main()
