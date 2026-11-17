"""Measure visible output sensitivity to RGB under fully transparent pixels."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd13_transparent_hidden_rgb"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
METHODS = {"Reinhard": "apply_reinhard", "MeanStddev": "apply_mean_std",
           "Histogram": "apply_histogram"}
INTENSITIES = (0.5, 1.0)
HIDDEN_RGB = {"black": [0.0, 0.0, 0.0], "magenta": [1.0, 0.0, 1.0]}
BACKGROUNDS = {"black": [0.0, 0.0, 0.0], "mid_gray": [0.5, 0.5, 0.5],
               "white": [1.0, 1.0, 1.0]}

from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard, rgb_to_lab,
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def deltae76(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    return np.linalg.norm(rgb_to_lab(a) - rgb_to_lab(b), axis=-1)


def encode_preview(rgb: np.ndarray) -> Image.Image:
    values = np.uint8(np.clip(np.rint(np.clip(rgb, 0.0, 1.0) * 255.0), 0, 255))
    return Image.fromarray(values, "RGB")


def composite(rgb: np.ndarray, alpha: np.ndarray, background: list[float]) -> np.ndarray:
    return rgb * alpha[..., None] + np.asarray(background)[None, None, :] * (1.0 - alpha[..., None])


def summarize_pixels(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict:
    a_roi, b_roi = a[mask], b[mask]
    de = deltae76(np.clip(a_roi, 0, 1), np.clip(b_roi, 0, 1))
    difference = np.abs(a_roi - b_roi)
    return {
        "pixels": int(mask.sum()),
        "encoded_sRGB_MAE_between_variants": float(difference.mean()),
        "encoded_sRGB_p95_between_variants": float(np.percentile(difference, 95)),
        "DeltaE76_mean_between_variants": float(de.mean()),
        "DeltaE76_p95_between_variants": float(np.percentile(de, 95)),
        "DeltaE76_max_between_variants": float(de.max()),
        "variant_a_range": [float(a_roi.min()), float(a_roi.max())],
        "variant_b_range": [float(b_roi.min()), float(b_roi.max())],
        "variant_a_non_finite_values": int(a_roi.size - np.count_nonzero(np.isfinite(a_roi))),
        "variant_b_non_finite_values": int(b_roi.size - np.count_nonzero(np.isfinite(b_roi))),
        "variant_a_clip_low_ratio": float(np.mean(a_roi < 0.0)),
        "variant_a_clip_high_ratio": float(np.mean(a_roi > 1.0)),
        "variant_b_clip_low_ratio": float(np.mean(b_roi < 0.0)),
        "variant_b_clip_high_ratio": float(np.mean(b_roi > 1.0)),
    }


def summarize_output(output: np.ndarray, source: np.ndarray, target: np.ndarray,
                    mask: np.ndarray) -> dict:
    out = output[mask]
    src = source[mask]
    ref = target[mask]
    clipped_out = np.clip(out, 0.0, 1.0)
    lab = rgb_to_lab(clipped_out)
    lab_src = rgb_to_lab(np.clip(src, 0.0, 1.0))
    lab_ref = rgb_to_lab(np.clip(ref, 0.0, 1.0))
    delta_source = np.linalg.norm(lab - lab_src, axis=-1)
    delta_target = np.linalg.norm(lab - lab_ref, axis=-1)
    mean_a, mean_b = float(lab[:, 1].mean()), float(lab[:, 2].mean())
    angles = np.arctan2(lab[:, 2], lab[:, 1])
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean_to_source": float(delta_source.mean()),
        "deltaE76_p95_to_source": float(np.percentile(delta_source, 95)),
        "deltaE76_mean_to_target": float(delta_target.mean()),
        "deltaE76_p95_to_target": float(np.percentile(delta_target, 95)),
        "encoded_sRGB_MAE_to_target": float(np.abs(out - ref).mean()),
        "Lab_mean": [float(lab[:, 0].mean()), mean_a, mean_b],
        "Cstar_from_mean_ab": float(np.hypot(mean_a, mean_b)),
        "hue_circular_mean_deg": float((np.rad2deg(np.arctan2(np.sin(angles).mean(),
                                                                np.cos(angles).mean())) + 360.0) % 360.0),
        "non_finite_values": int(out.size - np.count_nonzero(np.isfinite(out))),
        "channel_clip_low_ratio": float(np.mean(out < 0.0)),
        "channel_clip_high_ratio": float(np.mean(out > 1.0)),
    }


def save_variant_sheet(source: np.ndarray, alpha: np.ndarray, target: np.ndarray,
                       outputs: dict[tuple[str, float, str], np.ndarray],
                       method: str, intensity: float) -> str:
    cell_w, cell_h, header = 320, 213, 26
    columns = 4
    rows = 4
    sheet = Image.new("RGB", (columns * cell_w, rows * (cell_h + header)), (28, 28, 28))
    draw = ImageDraw.Draw(sheet)
    panels: list[tuple[str, np.ndarray]] = [("source / black hidden", source)]
    magenta_source = source.copy()
    magenta_source[alpha == 0.0] = HIDDEN_RGB["magenta"]
    panels.append(("source / magenta hidden", magenta_source))
    for variant in ("black", "magenta"):
        panels.append((f"{variant} hidden / output", outputs[(method, intensity, variant)]))
        for bg_name, bg in BACKGROUNDS.items():
            panels.append((f"{variant} / {bg_name} composite",
                           composite(outputs[(method, intensity, variant)], alpha, bg)))
    panels.append(("constructed target", target))
    for idx, (label, rgb) in enumerate(panels):
        x, y = (idx % columns) * cell_w, (idx // columns) * (cell_h + header)
        preview = encode_preview(rgb)
        preview.thumbnail((cell_w, cell_h))
        sheet.paste(preview, (x + (cell_w - preview.width) // 2, y + header))
        draw.text((x + 5, y + 5), label, fill=(245, 245, 245))
    path = OUT / f"{method}_i{intensity:.2f}_comparison.png"
    sheet.save(path, format="PNG", optimize=False)
    return path.name


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit equation mirrors")
    prep_manifest = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    h, w = source.shape[:2]
    alpha = np.ones((h, w), dtype=np.float64)
    strip_width = int(w * 0.20)
    alpha[:, :strip_width] = 0.0
    target_info = prep_manifest["synthetic_reference"]
    target = np.clip(source * np.asarray(target_info["scale"]) +
                     np.asarray(target_info["offset"]), 0.0, 1.0)
    masks = {name: np.asarray(Image.open(PREP / roi["mask"]).convert("L")) > 0
             for name, roi in prep_manifest["roi"].items()}
    visible = alpha > 0.0
    masks = {"visible_pixels": visible,
             **{name: mask & visible for name, mask in masks.items()}}

    OUT.mkdir(parents=True, exist_ok=True)
    variants = {}
    for name, hidden in HIDDEN_RGB.items():
        candidate = source.copy()
        candidate[~visible] = hidden
        variants[name] = candidate
        rgba = np.dstack((candidate, alpha))
        np.save(OUT / f"source_{name}_hidden_rgba.npy", rgba.astype(np.float32), allow_pickle=False)
        Image.fromarray(np.uint8(np.rint(rgba * 255.0)), "RGBA").save(
            OUT / f"source_{name}_hidden_rgba.png", format="PNG", optimize=False)
    np.save(OUT / "constructed_target_rgb.npy", target.astype(np.float32), allow_pickle=False)
    np.save(OUT / "shared_alpha.npy", alpha.astype(np.float32), allow_pickle=False)

    functions = {"Reinhard": apply_reinhard, "MeanStddev": apply_mean_std,
                 "Histogram": apply_histogram}
    outputs: dict[tuple[str, float, str], np.ndarray] = {}
    records = []
    for method, function in functions.items():
        for intensity in INTENSITIES:
            for variant, candidate in variants.items():
                output = function(candidate, target, intensity)
                outputs[(method, intensity, variant)] = output
                key = f"{method}_i{intensity:.2f}_{variant}"
                np.save(OUT / f"{key}.npy", output.astype(np.float32), allow_pickle=False)
                encode_preview(output).save(OUT / f"{key}.png", format="PNG", optimize=False)
            output_a = outputs[(method, intensity, "black")]
            output_b = outputs[(method, intensity, "magenta")]
            records.append({
                "method": method,
                "intensity": intensity,
                "regions": {name: summarize_pixels(output_a, output_b, mask)
                            for name, mask in masks.items()},
                "variant_output_metrics": {
                    variant: {name: summarize_output(outputs[(method, intensity, variant)],
                                                     variants[variant], target, mask)
                              for name, mask in masks.items()}
                    for variant in HIDDEN_RGB
                },
                "alpha_preserved_exactly": True,
                "alpha_nonzero_area_changed_only_by_same_method": True,
            })

    sheets = []
    for method in functions:
        for intensity in INTENSITIES:
            sheets.append(save_variant_sheet(source, alpha, target, outputs, method, intensity))
    files = {path.name: sha(path) for path in sorted(OUT.iterdir())
             if path.is_file() and path.name != "manifest.json"}
    manifest = {
        "rd": "rd13-transparent-hidden-rgb-output-sensitivity",
        "status": "predeclared synthetic RGBA mechanism probe; equation mirrors only; no product quality gate",
        "measurement": "paired RGB output difference over alpha>0 pixels; CIE Lab D65/2-degree DeltaE76 mean/p95/max; RGB MAE/p95; clipping/range and finite-value checks; fixed straight-alpha black/mid-gray/white preview composites",
        "source": {"file": "RD9 portrait", "prep_manifest_sha256": sha(PREP / "manifest.json"),
                   "source_sha256": sha(PREP / "source.png"), "dimensions": [w, h]},
        "target": {"construction": target_info["operation"],
                   "scale": target_info["scale"], "offset": target_info["offset"],
                   "sha256": sha(PREP / "synthetic_cast_reference.png"),
                   "role": "same synthetic target for both hidden-RGB variants"},
        "alpha": {"model": "straight alpha for exploratory preview composites; alpha is binary here",
                  "mask": "full-height left strip alpha=0 at x<floor(width*0.20); alpha=1 elsewhere",
                  "transparent_pixel_count": int(np.count_nonzero(~visible)),
                  "visible_pixel_count": int(np.count_nonzero(visible)),
                  "variants_have_identical_alpha": True,
                  "variants_have_identical_visible_rgb": bool(np.array_equal(variants["black"][visible], variants["magenta"][visible]))},
        "hidden_rgb_variants": HIDDEN_RGB,
        "backgrounds": BACKGROUNDS,
        "methods": list(METHODS),
        "intensities": list(INTENSITIES),
        "condition_pair_count": len(records),
        "paired_results": records,
        "visual_review": {"sheets": sheets,
                          "review_status": "unblinded exploratory comparison; not independent review or acceptance gate"},
        "implementation_boundary": {
            "core_source_sha256": sha(CORE),
            "mirror_source": "run_rd1_source_mirror.py",
            "mirror_source_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
            "script": Path(__file__).name,
            "script_sha256": sha(Path(__file__)),
            "limitations": ["not compiled Core output or C++ differential",
                            "one portrait, one binary-alpha strip, one synthetic reference",
                            "no independent review, calibrated reference, or quality threshold",
                            "straight-alpha composite previews are diagnostic only"]},
        "files": files,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "paired_conditions": len(records),
                      "visual_sheets": len(sheets), "artifacts": len(files)}, indent=2))


if __name__ == "__main__":
    main()
