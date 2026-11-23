"""Evaluate a registered natural-scene RAW-render/JPEG pair with source mirrors."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
ASSET_DIR = ROOT / "assets" / "MiddleburyNaturalScenes" / "CanonEOS5D"
OUT = ROOT / "captures" / "rd7_natural_scene_pair"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
SOURCE_PATH = ASSET_DIR / "scaled-im00-raw.png"
REFERENCE_PATH = ASSET_DIR / "scaled-im00-jpg.png"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import apply_histogram, apply_mean_std, apply_reinhard, rgb_to_lab, sha  # noqa: E402

WIDTH, HEIGHT = 1000, 667
METHODS = (("Reinhard", apply_reinhard),
           ("MeanStddev", apply_mean_std),
           ("Histogram", apply_histogram))
INTENSITIES = (0.25, 0.5, 1.0)
ROIS = {
    "whole_frame": None,
    "light_wall_candidate": (20, 108, 155, 164),
    "refrigerator_detail_candidate": (317, 240, 490, 434),
}


def load_rgb(path: Path) -> np.ndarray:
    image = Image.open(path).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise RuntimeError(f"Unexpected image dimensions: {image.size}")
    return np.asarray(image, dtype=np.float64) / 255.0


def linear_to_srgb(linear: np.ndarray) -> np.ndarray:
    linear = np.clip(linear, 0.0, 1.0)
    return np.where(linear <= 0.0031308, 12.92 * linear,
                    1.055 * np.maximum(linear, 0.0) ** (1.0 / 2.4) - 0.055)


def roi_mask(name: str, box: tuple[int, int, int, int] | None) -> np.ndarray:
    if box is None:
        return np.ones((HEIGHT, WIDTH), dtype=bool)
    x0, y0, x1, y1 = box
    mask = np.zeros((HEIGHT, WIDTH), dtype=bool)
    mask[y0:y1, x0:x1] = True
    return mask


def luminance(srgb: np.ndarray) -> np.ndarray:
    clipped = np.clip(srgb, 0.0, 1.0)
    linear = np.where(clipped <= 0.04045, clipped / 12.92,
                      ((clipped + 0.055) / 1.055) ** 2.4)
    return linear @ np.array([0.2126, 0.7152, 0.0722])


def gradient_stats(srgb: np.ndarray, mask: np.ndarray) -> dict:
    y = luminance(srgb)
    gy, gx = np.gradient(y)
    magnitude = np.hypot(gx, gy)[mask]
    return {"gradient_magnitude_mean": float(magnitude.mean()),
            "gradient_magnitude_p95": float(np.percentile(magnitude, 95))}


def metrics(output: np.ndarray, source: np.ndarray, reference: np.ndarray,
            mask: np.ndarray) -> dict:
    o = output[mask]
    s = source[mask]
    r = reference[mask]
    out_clip = np.clip(o, 0.0, 1.0)
    ref_clip = np.clip(r, 0.0, 1.0)
    delta_e = np.linalg.norm(rgb_to_lab(out_clip) - rgb_to_lab(ref_clip), axis=-1)
    rgb_error = np.abs(o - r)
    local_mask = mask
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean": float(delta_e.mean()),
        "deltaE76_p95": float(np.percentile(delta_e, 95)),
        "encoded_sRGB_MAE": float(rgb_error.mean()),
        "encoded_sRGB_p95": float(np.percentile(rgb_error, 95)),
        "output_clip_low_ratio": float(np.mean(o < 0)),
        "output_clip_high_ratio": float(np.mean(o > 1)),
        "non_finite_values": int(o.size - np.count_nonzero(np.isfinite(o))),
        "source_luminance_gradient": gradient_stats(source, local_mask),
        "output_luminance_gradient": gradient_stats(output, local_mask),
        "reference_luminance_gradient": gradient_stats(reference, local_mask),
    }


def save_srgb(path: Path, image: np.ndarray) -> None:
    encoded = np.uint8(np.clip(np.rint(np.clip(image, 0, 1) * 255.0), 0, 255))
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def main() -> None:
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit the equation mirrors")
    raw_linear = load_rgb(SOURCE_PATH)
    source = linear_to_srgb(raw_linear)
    reference = load_rgb(REFERENCE_PATH)
    OUT.mkdir(parents=True, exist_ok=True)
    np.save(OUT / "source_srgb_from_linear_raw.npy", source.astype(np.float32), allow_pickle=False)
    np.save(OUT / "reference_camera_jpeg_srgb.npy", reference.astype(np.float32), allow_pickle=False)
    save_srgb(OUT / "source_srgb_from_linear_raw.png", source)
    save_srgb(OUT / "reference_camera_jpeg.png", reference)

    masks = {name: roi_mask(name, box) for name, box in ROIS.items()}
    for name, box in ROIS.items():
        if box is not None:
            Image.fromarray(masks[name].astype(np.uint8) * 255, "L").save(
                OUT / f"roi_{name}.png", format="PNG", optimize=False)
    results = [{"method": "unchanged_source", "intensity": 0.0,
                "regions": {name: metrics(source, source, reference, mask)
                            for name, mask in masks.items()}}]
    items = [("RAW linear→sRGB", "source_srgb_from_linear_raw.png"),
             ("Camera JPEG", "reference_camera_jpeg.png")]
    for method, function in METHODS:
        for intensity in INTENSITIES:
            output = function(source, reference, intensity)
            key = f"{method}_i{intensity:.2f}"
            np.save(OUT / f"{key}.npy", output.astype(np.float32), allow_pickle=False)
            save_srgb(OUT / f"{key}.png", output)
            results.append({"method": method, "intensity": intensity,
                            "regions": {name: metrics(output, source, reference, mask)
                                        for name, mask in masks.items()}})
            if intensity in (0.5, 1.0):
                items.append((f"{method} {intensity:.2f}", f"{key}.png"))

    tile_w, tile_h, label_h = 333, 222, 28
    sheet = Image.new("RGB", (tile_w * len(items), tile_h + label_h), (32, 32, 32))
    draw = ImageDraw.Draw(sheet)
    for index, (label, filename) in enumerate(items):
        image = Image.open(OUT / filename).convert("RGB")
        image.thumbnail((tile_w, tile_h))
        x = index * tile_w + (tile_w - image.width) // 2
        y = label_h + (tile_h - image.height) // 2
        sheet.paste(image, (x, y))
        draw.text((index * tile_w + 5, 6), label, fill=(245, 245, 245))
    sheet.save(OUT / "comparison_sheet.png", format="PNG", optimize=False)

    files = {p.name: sha(p) for p in sorted(OUT.iterdir())
             if p.is_file() and p.name != "manifest.json"}
    manifest = {
        "rd": "rd7-middlebury-natural-scene-raw-jpeg-pair",
        "status": "registered natural-scene rendering comparison; source-mirror only; no acceptance gate",
        "dataset": {"source_page": "https://vision.middlebury.edu/color/data/",
            "archive": "other-scaled.zip", "archive_sha256": "6fce6384c086a8088de08497038ab3681dabafe22b0f49832b45961e1a882d98",
            "permission_note": "dataset page grants use and publication with citation; not designated CC0",
            "citation": "A. Chakrabarti, D. Scharstein, and T. Zickler, An empirical camera model for Internet color vision, BMVC 2009"},
        "pair": {"camera": "Canon EOS 5D", "scene": "warm-lit kitchen interior", "source": SOURCE_PATH.name,
            "source_sha256": sha(SOURCE_PATH), "source_transfer": "dataset standard linear RGB, converted to sRGB OETF",
            "reference": REFERENCE_PATH.name, "reference_sha256": sha(REFERENCE_PATH),
            "reference_transfer": "camera JPEG treated as sRGB; no ICC profile in PNG container",
            "dimensions": [WIDTH, HEIGHT], "alignment": "registered RAW/JPG natural scene pair per dataset page",
            "reference_role": "camera rendition; not colorimetric ground truth"},
        "roi": {name: {"xyxy": list(box) if box else None,
                "pixels": int(masks[name].sum()),
                "status": "unreviewed candidate region" if name != "whole_frame" else "full frame"}
            for name, box in ROIS.items()},
        "methods": ["Reinhard", "MeanStddev", "Histogram"], "intensities": list(INTENSITIES),
        "measurement": "CIE Lab D65/2-degree ΔE76 and encoded-sRGB errors; linear Rec.709 luminance-gradient magnitude for source/output/reference",
        "method_source": {"path": "ArtifactCore/src/Color/AutoColorMatch.cpp", "sha256": sha(CORE_SOURCE),
            "mirror_runner": "run_rd1_source_mirror.py", "runner_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
            "script": Path(__file__).name, "script_sha256": sha(Path(__file__)),
            "evidence": "independent Python transcription only; no compiled C++ differential"},
        "files": files, "results": results,
        "limitations": ["no human subject or skin ROI", "wall ROI is not verified neutral white",
            "camera JPEG rendering may include tone/color processing", "single scene and camera",
            "no Core execution, ICC/HDR, alpha, performance, GPU path, or blinded review"],
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "baseline": results[0]["regions"],
                      "candidate_count": len(results)-1}, indent=2))


if __name__ == "__main__":
    main()
