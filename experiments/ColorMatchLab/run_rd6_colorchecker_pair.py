"""Evaluate a licensed registered ColorChecker WB pair with source mirrors."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
ASSET_DIR = ROOT / "assets" / "MiddleburyColorChecker24" / "CanonEOS20D"
OUT = ROOT / "captures" / "rd6_colorchecker_pair"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard, rgb_to_lab, sha,
)

SOURCE_PATH = ASSET_DIR / "wb1i2e3-jpg.png"
REFERENCE_PATH = ASSET_DIR / "wb2i2e3-jpg.png"
HEIGHT, WIDTH = 404, 606
GRID_ROWS, GRID_COLS, CELL = 4, 6, 101
INNER_START, INNER_SIZE = 20, 60
METHODS = (("Reinhard", apply_reinhard),
           ("MeanStddev", apply_mean_std),
           ("Histogram", apply_histogram))
INTENSITIES = (0.25, 0.5, 1.0)


def load_srgb(path: Path) -> np.ndarray:
    image = Image.open(path).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise RuntimeError(f"Unexpected registered image size: {path} {image.size}")
    return np.asarray(image, dtype=np.float64) / 255.0


def make_patch_masks() -> tuple[dict[str, np.ndarray], list[dict]]:
    masks = {}
    patch_specs = []
    for row in range(GRID_ROWS):
        for col in range(GRID_COLS):
            x0 = col * CELL + INNER_START
            y0 = row * CELL + INNER_START
            x1, y1 = x0 + INNER_SIZE, y0 + INNER_SIZE
            mask = np.zeros((HEIGHT, WIDTH), dtype=bool)
            mask[y0:y1, x0:x1] = True
            name = f"patch_r{row}_c{col}"
            masks[name] = mask
            patch_specs.append({"name": name, "row": row, "column": col,
                                "xyxy": [x0, y0, x1, y1], "pixels": int(mask.sum())})
    masks["skin_tone_proxy"] = masks["patch_r0_c0"] | masks["patch_r0_c1"]
    masks["white_patch"] = masks["patch_r3_c0"]
    masks["neutral_gray_patches"] = np.logical_or.reduce(
        [masks[f"patch_r3_c{col}"] for col in range(1, 6)])
    masks["all_chart_patches"] = np.logical_or.reduce(
        [masks[f"patch_r{row}_c{col}"] for row in range(4) for col in range(6)])
    return masks, patch_specs


def metrics(output: np.ndarray, reference: np.ndarray, mask: np.ndarray) -> dict:
    out = np.clip(output[mask], 0.0, 1.0)
    ref = np.clip(reference[mask], 0.0, 1.0)
    lab_out, lab_ref = rgb_to_lab(out), rgb_to_lab(ref)
    delta_e = np.linalg.norm(lab_out - lab_ref, axis=-1)
    rgb_delta = np.abs(output[mask] - reference[mask])
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean": float(delta_e.mean()),
        "deltaE76_p95": float(np.percentile(delta_e, 95)),
        "deltaE76_max": float(delta_e.max()),
        "encoded_sRGB_MAE": float(rgb_delta.mean()),
        "encoded_sRGB_p95": float(np.percentile(rgb_delta, 95)),
        "output_linearized_mean_rgb": [float(v) for v in np.mean(
            np.where(out <= 0.04045, out / 12.92, ((out + 0.055) / 1.055) ** 2.4), axis=0)],
        "reference_linearized_mean_rgb": [float(v) for v in np.mean(
            np.where(ref <= 0.04045, ref / 12.92, ((ref + 0.055) / 1.055) ** 2.4), axis=0)],
        "output_clip_low_ratio": float(np.mean(output[mask] < 0.0)),
        "output_clip_high_ratio": float(np.mean(output[mask] > 1.0)),
        "non_finite_values": int(output[mask].size - np.count_nonzero(np.isfinite(output[mask]))),
    }


def save_srgb(path: Path, image: np.ndarray) -> None:
    encoded = np.uint8(np.clip(np.rint(np.clip(image, 0, 1) * 255.0), 0, 255))
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def main() -> None:
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit the equation mirrors")
    source = load_srgb(SOURCE_PATH)
    reference = load_srgb(REFERENCE_PATH)
    masks, patch_specs = make_patch_masks()
    OUT.mkdir(parents=True, exist_ok=True)
    np.save(OUT / "source_wb1_srgb.npy", source.astype(np.float32), allow_pickle=False)
    np.save(OUT / "reference_wb2_srgb.npy", reference.astype(np.float32), allow_pickle=False)
    save_srgb(OUT / "source_wb1.png", source)
    save_srgb(OUT / "reference_wb2.png", reference)
    for name in ("skin_tone_proxy", "white_patch", "neutral_gray_patches", "all_chart_patches"):
        Image.fromarray(masks[name].astype(np.uint8) * 255, "L").save(
            OUT / f"roi_{name}.png", format="PNG", optimize=False)

    outputs = {"unchanged_source": source}
    results = [{"method": "unchanged_source", "intensity": 0.0,
                "regions": {name: metrics(source, reference, mask)
                            for name, mask in masks.items()}}]
    for method_name, function in METHODS:
        for intensity in INTENSITIES:
            output = function(source, reference, intensity)
            key = f"{method_name}_i{intensity:.2f}"
            outputs[key] = output
            np.save(OUT / f"{key}.npy", output.astype(np.float32), allow_pickle=False)
            save_srgb(OUT / f"{key}.png", output)
            results.append({"method": method_name, "intensity": intensity,
                            "regions": {name: metrics(output, reference, mask)
                                        for name, mask in masks.items()}})

    panel_items = [("WB1 source", "source_wb1.png"), ("WB2 Auto target", "reference_wb2.png")]
    panel_items += [(f"{method} {intensity:.2f}", f"{method}_i{intensity:.2f}.png")
                    for method, _ in METHODS for intensity in (0.5, 1.0)]
    tile_w, tile_h, label_h = 303, 202, 28
    sheet = Image.new("RGB", (tile_w * len(panel_items), tile_h + label_h), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, filename) in enumerate(panel_items):
        image = Image.open(OUT / filename).convert("RGB")
        image.thumbnail((tile_w, tile_h))
        x = index * tile_w + (tile_w - image.width) // 2
        y = label_h + (tile_h - image.height) // 2
        sheet.paste(image, (x, y))
        draw.text((index * tile_w + 5, 6), label, fill=(245, 245, 245))
    sheet.save(OUT / "comparison_sheet.png", format="PNG", optimize=False)
    (OUT / "patch_grid.json").write_text(json.dumps(patch_specs, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")

    files = {path.name: sha(path) for path in sorted(OUT.iterdir())
             if path.is_file() and path.name != "manifest.json"}
    manifest = {
        "rd": "rd6-middlebury-colorchecker-wb-pair",
        "status": "registered camera-WB paired probe; source-mirror only; no acceptance gate",
        "dataset": {"source_page": "https://vision.middlebury.edu/color/data/",
            "archive": "checker24-RAW-JPG.zip",
            "archive_sha256": "e0f58600116121406004717a1dbc1d4e41296f7fac615005988400945a8a3990",
            "permission_note": "dataset page says images may be used and published with citation; not designated CC0",
            "citation": "A. Chakrabarti, D. Scharstein, and T. Zickler, An empirical camera model for Internet color vision, BMVC 2009"},
        "pair": {"camera": "Canon EOS 20D", "source": SOURCE_PATH.name,
            "source_sha256": sha(SOURCE_PATH), "reference": REFERENCE_PATH.name,
            "reference_sha256": sha(REFERENCE_PATH), "conditions": "wb1 fixed tungsten vs wb2 auto; i2 daylight; e3 nominal 0 EV",
            "dimensions": [WIDTH, HEIGHT], "alignment": "registered ColorChecker composite per dataset description",
            "target_role": "camera auto-WB rendition; not colorimetric ground truth",
            "transfer": "camera JPEG sRGB renditions; inputs represented as encoded [0,1] RGB",
            "raw_controls": ["wb1i2e3-raw.png", "wb2i2e3-raw.png"],
            "raw_controls_used_in_scoring": False},
        "roi": {"grid": "6 columns × 4 rows, 101×101 cells", "sample": "central 60×60 pixels per patch",
            "skin_tone_proxy": ["patch_r0_c0", "patch_r0_c1"],
            "white": ["patch_r3_c0"], "neutral_gray": ["patch_r3_c1", "patch_r3_c2", "patch_r3_c3", "patch_r3_c4", "patch_r3_c5"],
            "meaning": "physical chart skin-tone and neutral patches; not human skin or natural white materials"},
        "methods": ["Reinhard", "MeanStddev", "Histogram"], "intensities": list(INTENSITIES),
        "measurement": "CIE Lab D65/2-degree ΔE76 on clipped display sRGB decode; encoded-sRGB MAE/p95 and clipping ratios, per patch and ROI",
        "method_source": {"path": "ArtifactCore/src/Color/AutoColorMatch.cpp", "sha256": sha(CORE_SOURCE),
            "mirror_runner": "run_rd1_source_mirror.py", "runner_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
            "script": Path(__file__).name, "script_sha256": sha(Path(__file__)),
            "evidence": "independent Python transcription only; no compiled C++ differential"},
        "files": files, "results": results,
        "limitations": ["WB2 is a camera auto setting rather than calibrated GT",
            "chart patches do not substitute for real skin/white objects",
            "one camera, one illumination, one exposure", "no alpha, HDR, ICC, performance or GPU path",
            "no blinded visual review or acceptance gate"],
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "pair": manifest["pair"],
                      "baseline": results[0]["regions"],
                      "candidate_count": len(results)-1}, indent=2))


if __name__ == "__main__":
    main()
