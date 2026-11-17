"""Record actual Core measurements on the registered Middlebury chart pair."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd24_colorchecker_core"
ASSETS = ROOT / "assets" / "MiddleburyColorChecker24" / "CanonEOS20D"
TEST_SOURCE = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE_SOURCE = REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUN_LOG = REPO / "temp" / "rd24_core_full.log"
METHODS = ("Reinhard", "MeanStddev", "Histogram")
INTENSITIES = ("0.25", "0.5", "1")
REGIONS = {"skin_tone_proxy": 7200, "white_patch": 3600, "neutral_gray": 18000}
PATCHES = {
    "skin_tone_proxy": ((20, 20), (121, 20)),
    "white_patch": ((20, 323),),
    "neutral_gray": ((121, 323), (222, 323), (323, 323), (424, 323), (525, 323)),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def patch_strip(image: Image.Image, origins: tuple[tuple[int, int], ...]) -> Image.Image:
    scale, gap = 4, 8
    strip = Image.new("RGB", (len(origins) * 60 * scale + (len(origins) - 1) * gap, 60 * scale),
                      (25, 26, 28))
    for index, (x, y) in enumerate(origins):
        patch = image.crop((x, y, x + 60, y + 60)).resize(
            (60 * scale, 60 * scale), Image.Resampling.NEAREST)
        strip.paste(patch, (index * (60 * scale + gap), 0))
    return strip


def main() -> None:
    csv_path = CAPTURE / "core_patch_metrics.csv"
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected = {(method, intensity, region) for method in METHODS
                for intensity in INTENSITIES for region in REGIONS}
    if len(rows) != 27 or {
            (row["method"], row["intensity"], row["region"]) for row in rows
    } != expected:
        raise RuntimeError("RD24 Core metric matrix is incomplete")
    numeric_fields = ("intensity", "pixels", "source_mean_delta_e76",
                      "source_p95_delta_e76", "source_max_delta_e76",
                      "source_pct_over_2", "wb2_mean_delta_e76",
                      "wb2_p95_delta_e76", "wb2_max_delta_e76",
                      "output_mean_L", "output_mean_a",
                      "output_mean_b", "output_mean_Cstar")
    for row in rows:
        if int(row["pixels"]) != REGIONS[row["region"]]:
            raise RuntimeError(f"RD24 ROI count mismatch: {row}")
        for field in numeric_fields:
            if not math.isfinite(float(row[field])):
                raise RuntimeError(f"Non-finite RD24 metric {field}: {row}")

    columns = (
        ("WB1 Source", CAPTURE / "source_wb1.png"),
        ("WB2 Camera Auto-WB", CAPTURE / "reference_wb2.png"),
        ("Core Reinhard 1.0", CAPTURE / "Reinhard_1.0.png"),
        ("Core MeanStddev 1.0", CAPTURE / "MeanStddev_1.0.png"),
        ("Core Histogram 1.0", CAPTURE / "Histogram_1.0.png"),
    )
    images = [(label, Image.open(path).convert("RGB")) for label, path in columns]
    tile_w, header_h, full_h, patch_h, gap = 240, 32, 160, 240, 8
    row_names = ("Whole chart", "Skin-tone proxy patches", "White patch", "Neutral-gray patches")
    sheet_h = header_h + full_h + gap + len(row_names) * (header_h + patch_h + gap)
    sheet = Image.new("RGB", (tile_w * len(images), sheet_h), (32, 33, 36))
    draw = ImageDraw.Draw(sheet)
    for column, (label, image) in enumerate(images):
        x = column * tile_w
        draw.text((x + 8, 8), label, fill=(245, 245, 245))
        full = image.copy()
        full.thumbnail((tile_w - 12, full_h - 8), Image.Resampling.LANCZOS)
        sheet.paste(full, (x + (tile_w - full.width) // 2,
                           header_h + (full_h - full.height) // 2))
        y = header_h + full_h + gap
        for region in row_names[1:]:
            draw.text((x + 8, y + 8), region, fill=(205, 213, 222))
            key = {"Skin-tone proxy patches": "skin_tone_proxy",
                   "White patch": "white_patch",
                   "Neutral-gray patches": "neutral_gray"}[region]
            strip = patch_strip(image, PATCHES[key])
            sheet.paste(strip, (x + 8, y + header_h + 4))
            y += header_h + patch_h + gap
    review_path = CAPTURE / "core_visual_review.png"
    sheet.save(review_path, format="PNG", optimize=False)

    source_asset = ASSETS / "wb1i2e3-jpg.png"
    reference_asset = ASSETS / "wb2i2e3-jpg.png"
    run_log_text = RUN_LOG.read_text(encoding="utf-8", errors="replace")
    descriptor_lines = [
        line[line.index("RD24_INPUT_DESCRIPTOR,"):]
        for line in run_log_text.splitlines()
        if "RD24_INPUT_DESCRIPTOR," in line
    ]
    if len(descriptor_lines) != 2 or {
            line.split(",")[1] for line in descriptor_lines
    } != {"source", "reference"}:
        raise RuntimeError("RD24 full test log is missing source/reference descriptor observations")
    output_names = ("source_wb1.png", "reference_wb2.png", "Reinhard_0.5.png",
                    "Reinhard_1.0.png", "MeanStddev_0.5.png", "MeanStddev_1.0.png",
                    "Histogram_0.5.png", "Histogram_1.0.png")
    manifest = {
        "rd": "rd24-middlebury-colorchecker-actual-core-wb-pair",
        "status": "actual Core WB-pair chart diagnostic; exploratory, no acceptance gate",
        "source_pair": {
            "dataset": "Middlebury Color Datasets, registered ColorChecker24",
            "source_page": "https://vision.middlebury.edu/color/data/",
            "permission": "dataset page permits use and publication with citation; not marked CC0",
            "citation": "A. Chakrabarti, D. Scharstein, and T. Zickler, An empirical camera model for Internet color vision, BMVC 2009",
            "camera": "Canon EOS 20D",
            "conditions": "WB1 fixed tungsten vs WB2 camera auto-WB, daylight illuminant i2, exposure e3 (nominal 0 EV)",
            "dimensions": [606, 404],
            "source": {"path": "experiments/ColorMatchLab/assets/MiddleburyColorChecker24/CanonEOS20D/wb1i2e3-jpg.png", "sha256": sha(source_asset)},
            "reference": {"path": "experiments/ColorMatchLab/assets/MiddleburyColorChecker24/CanonEOS20D/wb2i2e3-jpg.png", "sha256": sha(reference_asset), "role": "camera auto-WB rendition; not calibrated/colorimetric ground truth"},
        },
        "measurement": {
            "route": "actual ArtifactCore AutoColorMatcher::match(ImageF32x4_RGBA&, ...) output",
            "transfer": "camera JPEG renditions treated as encoded sRGB for D65/2-degree Lab reporting; derived PNGs contain no embedded ICC profile, so this is an assumption",
            "profile_observation": "Pillow inspection of both 606x404 RGB PNG files found no ICC profile or other metadata chunks; the loaded Core images also report unknown transfer and unknown primaries",
            "interpretation_limit": "Lab values use an analyst sRGB assumption; ImageF32x4_RGBA did not provide known transfer/primary metadata for these inputs",
            "roi": "registered 6x4 chart; central 60x60 pixels per patch; skin proxy row 0 columns 0-1; white row 3 column 0; neutral gray row 3 columns 1-5",
            "methods": list(METHODS),
            "intensities": [0.25, 0.5, 1.0],
            "metrics": "per ROI mean, nearest-rank p95, max, and descriptive percent above DeltaE76 2 to source and WB2; output mean Lab and C*ab",
            "delta_e_2_cutoff": "descriptive only; not a quality threshold",
        },
        "provenance": {
            "test_name": "AutoColorMatcherContractTest.CoreImageOverloadMatchesMiddleburyColorCheckerMirrorPreviews",
            "test_source_sha256": sha(TEST_SOURCE),
            "core_source_sha256": sha(CORE_SOURCE),
            "test_log": "temp/rd24_core_full.log",
            "test_log_sha256": sha(RUN_LOG),
            "loaded_image_descriptors": descriptor_lines,
            "metrics_csv": "core_patch_metrics.csv",
            "metrics_sha256": sha(csv_path),
            "visual_sheet": review_path.name,
            "visual_sheet_sha256": sha(review_path),
            "single_unblinded_review": True,
            "acceptance": False,
        },
        "metrics_rows": rows,
        "outputs": {name: sha(CAPTURE / name) for name in output_names},
        "limits": [
            "ColorChecker skin-tone patches are standardized proxies, not people",
            "the white/gray patches are chart materials, not natural white objects",
            "WB2 is an in-camera auto-WB rendition, not calibrated target ground truth",
            "single camera, lighting condition and exposure",
            "visual sheet is unblinded and not independently accepted",
            "no product caller, GPU path, Release performance or memory evaluation",
        ],
    }
    manifest_path = CAPTURE / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"rows": len(rows), "visual_review": str(review_path),
                      "metrics_sha256": sha(csv_path),
                      "test_source_sha256": manifest["provenance"]["test_source_sha256"]}, indent=2))


if __name__ == "__main__":
    main()
