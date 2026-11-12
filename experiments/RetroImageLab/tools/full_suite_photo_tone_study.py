"""Profile-normalized photo tonal response for the independent retro suite."""

from __future__ import annotations

import hashlib
import io
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_tonal_response_study import distribution, error_stats  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import BAYER_4, PRESETS, apply_retro, ordered_quantize, srgb_to_linear  # noqa: E402

INPUT_DIR = ROOT / "eval-inputs" / "real-tone"
MANIFEST = INPUT_DIR / "provenance.json"
CAPTURE = ROOT / "captures" / "full-suite-photo-tone"
SEED = 31111
MAX_EDGE = 1024
ASSET_NAMES = (
    "face_portrait_unsplash_cc0.jpg",
    "black_white_high_iso_portrait_cc0.jpg",
    "milky_way_grundlsee_cc0.jpg",
    "clouds_sun_behind_cc0.jpg",
    "blue_sky_clouds_cc0.jpg",
)


def load_source(name: str, manifest: dict[str, object]) -> tuple[Image.Image, dict[str, object]]:
    path = INPUT_DIR / name
    provenance = manifest[name]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != provenance["download_sha256"]:
        raise ValueError(f"source SHA-256 mismatch: {name}")
    with Image.open(path) as opened:
        image = ImageOps.exif_transpose(opened)
        embedded = image.info.get("icc_profile")
        profile_name = "none"
        if embedded:
            profile_name = ImageCms.getProfileName(ImageCms.ImageCmsProfile(io.BytesIO(embedded))).strip()
            src_profile = ImageCms.ImageCmsProfile(io.BytesIO(embedded))
            dst_profile = ImageCms.createProfile("sRGB")
            image = ImageCms.profileToProfile(image, src_profile, dst_profile, outputMode="RGB")
        else:
            image = image.convert("RGB")  # Explicit assumption: untagged sources use sRGB code values.
        original_size = image.size
        scale = min(1.0, MAX_EDGE / max(image.size))
        if scale < 1.0:
            out_size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
            image = image.resize(out_size, Image.Resampling.LANCZOS)
    info = {"file": name, "download SHA-256": digest, "source dimensions after EXIF orientation": list(original_size),
            "study dimensions": list(image.size), "resize": "Pillow Lanczos, proportional, longest edge <=1024",
            "embedded ICC profile detected": bool(embedded), "embedded profile name": profile_name,
            "working color condition": "profile-tagged input converted to sRGB with Pillow ImageCms; untagged input assumed sRGB",
            "provenance": provenance}
    return image, info


def make_regions(rgb: np.ndarray) -> dict[str, np.ndarray]:
    luma = srgb_to_linear(rgb) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    dx = np.zeros_like(luma)
    dy = np.zeros_like(luma)
    dx[:, 1:] = np.abs(luma[:, 1:] - luma[:, :-1])
    dy[1:, :] = np.abs(luma[1:, :] - luma[:-1, :])
    grad = dx + dy
    result = {"whole image": np.ones(luma.shape, dtype=bool),
              "shadow luma <.02": luma < .02,
              "deep shadow luma .02-.08": (luma >= .02) & (luma < .08),
              "midtone luma .08-.6": (luma >= .08) & (luma < .6),
              "highlight luma >=.8": luma >= .8,
              "smooth tone gradient <.004": grad < .004,
              "detail gradient >.025": grad > .025}
    return result


def apply_quantizer_only(image: Image.Image, preset: dict[str, object]) -> np.ndarray:
    base = image
    if float(preset["scale"]) < 1:
        small = (max(1, round(image.width * float(preset["scale"]))),
                 max(1, round(image.height * float(preset["scale"]))))
        base = image.resize(small, Image.Resampling.BOX).resize(image.size, Image.Resampling.NEAREST)
    rgb = np.asarray(base, dtype=np.float32) / 255.0
    result = ordered_quantize(rgb, int(preset["colors"]), BAYER_4, float(preset["dither"]))
    return np.clip(result, 0, 1)


def make_sheet(items: list[dict[str, object]], output: Path, original: bool = False) -> None:
    tile, label_h, cols = 220, 22, 4
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tile, rows * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for index, item in enumerate(items):
        x, y = (index % cols) * tile, (index // cols) * (tile + label_h)
        if original:
            label = f"{item['asset']} / sRGB input"
            image = item["source image"]
        else:
            label = f"{item['asset']} / {item['preset']}"
            image = item["result image"]
        thumb = image.copy()
        thumb.thumbnail((tile, tile), Image.Resampling.LANCZOS)
        draw.text((x + 3, y + 4), label[:37], fill=(245, 245, 245), font=font)
        sheet.paste(thumb.convert("RGB"), (x, y + label_h))
    sheet.save(output)


def region_error(source: np.ndarray, result: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if count == 0:
        return {"pixel count": 0, "metrics": None}
    return {"pixel count": count, "metrics": error_stats(source, result, mask)}


def make_detail_sheet(source_images: dict[str, Image.Image], outputs: dict[tuple[str, str], Image.Image]) -> tuple[Path, dict[str, list[int]]]:
    crops = {
        "face_portrait_unsplash_cc0.jpg": (320, 100, 256, 256),
        "black_white_high_iso_portrait_cc0.jpg": (384, 400, 256, 256),
        "milky_way_grundlsee_cc0.jpg": (512, 70, 256, 256),
        "clouds_sun_behind_cc0.jpg": (384, 170, 256, 256),
        "blue_sky_clouds_cc0.jpg": (384, 240, 256, 256),
    }
    tile, label_h = 256, 20
    names = list(source_images)
    sheet = Image.new("RGB", (5 * tile, len(names) * (tile + label_h)), (26, 27, 30))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    crop_map: dict[str, list[int]] = {}
    for row, name in enumerate(names):
        x0, y0, w, h = crops[name]
        crop_map[name] = [x0, y0, w, h]
        variants = [("input", source_images[name])]
        variants.extend((preset, outputs[(name, preset)]) for preset in PRESETS)
        for col, (label, image) in enumerate(variants):
            x, y = col * tile, row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{Path(name).stem[:15]} / {label}", fill=(245, 245, 245), font=font)
            crop = image.crop((x0, y0, x0 + w, y0 + h)).convert("RGB")
            sheet.paste(crop, (x, y + label_h))
    path = CAPTURE / "real-photo-suite-detail-crops-1to1.png"
    sheet.save(path)
    return path, crop_map


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records: list[dict[str, object]] = []
    result_items: list[dict[str, object]] = []
    source_items: list[dict[str, object]] = []
    source_images: dict[str, Image.Image] = {}
    output_images: dict[tuple[str, str], Image.Image] = {}
    source_info: dict[str, object] = {}
    for name in ASSET_NAMES:
        image, info = load_source(name, manifest)
        source_info[name] = info
        source_rgba = image.convert("RGBA")
        source_array = np.asarray(source_rgba, dtype=np.uint8)
        source_rgb = source_array[:, :, :3].astype(np.float32) / 255.0
        alpha = source_array[:, :, 3]
        regions = make_regions(source_rgb)
        source_items.append({"asset": Path(name).stem, "source image": image})
        source_images[name] = image
        for preset_name, preset in PRESETS.items():
            quant = apply_quantizer_only(image, preset)
            result = apply_retro(source_rgba, **preset, seed=SEED)
            result_array = np.asarray(result, dtype=np.uint8)
            result_rgb = result_array[:, :, :3].astype(np.float32) / 255.0
            record = {"asset": Path(name).stem, "preset": preset_name,
                      "source dimensions": list(image.size), "seed": SEED,
                      "alpha exact match": bool(np.array_equal(alpha, result_array[:, :, 3])),
                      "regions": {}}
            for region_name, mask in regions.items():
                record["regions"][region_name] = {
                    "pixel count": int(np.count_nonzero(mask)),
                    "source distribution": distribution(source_rgb, mask, alpha),
                    "quantizer only distribution": distribution(quant, mask, alpha),
                    "full suite distribution": distribution(result_rgb, mask, alpha),
                    "quantizer only delta": region_error(source_rgb, quant, mask),
                    "full suite delta": region_error(source_rgb, result_rgb, mask),
                }
            records.append(record)
            result_items.append({"asset": Path(name).stem, "preset": preset_name, "result image": result})
            output_images[(name, preset_name)] = result.convert("RGB")
    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet_path = CAPTURE / "real-photo-suite-contact-sheet.png"
    source_sheet = CAPTURE / "real-photo-source-sheet.png"
    make_sheet(result_items, sheet_path)
    make_sheet(source_items, source_sheet, original=True)
    detail_sheet, detail_crops = make_detail_sheet(source_images, output_images)
    report = {
        "question": "How do the existing retro suite presets alter tone distributions, shadows, highlights, gradients, and details in provenance-checked photographs?",
        "evaluation plan": {
            "objective": "Describe output response on photographs; do not rank presets or assert an approved look.",
            "comparison": "Each profile-normalized input is the baseline; the same current preset is measured at quantizer-only (after pixelation where active) and full-suite output. No ground truth or approved target is available, so baseline differences are descriptive only.",
            "inputs": "Five photos: color portrait, high-ISO B&W portrait, Milky Way at night, backlit clouds/sun, and blue sky/clouds. File SHA-256 checked against provenance.json before decode.",
            "resolution": f"EXIF orientation applied; proportional Lanczos downsample only when longest edge exceeds {MAX_EDGE}px. No crop. The tested resolution is fixed for this study, not a resolution-parity claim.",
            "color": "Embedded ICC converted to sRGB before effect; untagged images assumed sRGB. This makes a defined lab input condition; apply_retro itself does not perform ICC management. The output is not encoded with an ICC profile.",
            "regions": "Whole frame, scene-linear Rec.709 luma shadow/midtone/highlight bins, smooth neighbor-luma gradient (<.004), and detail gradient (>.025). Masks are source-derived and threshold definitions are exploratory, not quality cutoffs.",
            "metrics": "RGB8 unique colors; linear-light Rec.709 luma distribution/32-bin alpha-weighted entropy; low/high channel clipping; linear RGB MAE/RMSE/P95 and changed-pixel ratio for input deltas. All images are opaque; alpha composite comparison is not applicable.",
            "visual review": "Source and output contact sheets use the same aspect-preserving thumbnail size; they are overview only. Output crops must be inspected at source pixel scale for fine detail; no product display transform is modeled.",
            "time stability": "Not applicable: still-image effect response; grain seed is fixed, no frame sequence.",
            "routes and performance": "Not measured in this study. Python only; no Artifact.exe, Core, GPU, preview/export path, or performance budget.",
            "acceptance": "No human-approved target or image-quality threshold exists. Descriptive research only; do not treat RMSE, color count, or histogram entropy as a quality score.",
            "rights caveat": "Both portrait Commons pages show CC0 provenance but the Unsplash import is review-needed and the other includes a personality-rights warning; depicted-person rights/model release are not established. Keep portraits for internal research only; do not use in product/demo material or redistribute them.",
            "unverified": "Additional skin tones/lighting, multi-image statistical coverage, HDR/high-bit-depth, product image decoding/profile route, GPU/backend parity, human preference, approved retro look, and runtime performance.",
        },
        "source provenance and decode": source_info,
        "environment": {"python": sys.version, "numpy": np.__version__, "pillow": Image.__version__, "platform": sys.platform},
        "records": records,
        "source contact sheet": str(source_sheet.relative_to(ROOT)),
        "suite output contact sheet": str(sheet_path.relative_to(ROOT)),
        "source pixel 1:1 detail crops": str(detail_sheet.relative_to(ROOT)),
        "detail crop xywh per input": detail_crops,
        "limits": "Small, curated corpus with two identifiable-person portraits; source rights metadata does not establish publicity/model release. Photos represent a limited set of lighting, camera, and subject conditions. sRGB conversion and downsampling are explicit lab choices, not verified product import behavior.",
    }
    (CAPTURE / "real-photo-suite-tone-metrics.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Analyzed {len(report['records'])} photo/preset cases after provenance checks.")
