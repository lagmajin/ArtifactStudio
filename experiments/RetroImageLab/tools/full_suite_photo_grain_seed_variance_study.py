"""Measure seed-to-seed variation in the current photo suite by ROI."""

from __future__ import annotations

import hashlib
import io
import json
import platform
import statistics
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_tone_study import ASSET_NAMES, INPUT_DIR, MANIFEST, make_regions, region_error  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-seed-variance"
SIZES = (512, 2048)
REFERENCE_SIZE = 512
SEEDS = tuple(range(31111, 31127))
SELECTED_SEEDS = (SEEDS[0], SEEDS[5], SEEDS[10], SEEDS[15])
CROP_RECTS = {
    "face_portrait_unsplash_cc0.jpg": (.30, .15, .70, .65),
    "black_white_high_iso_portrait_cc0.jpg": (.30, .35, .70, .75),
    "milky_way_grundlsee_cc0.jpg": (.28, .18, .68, .82),
    "clouds_sun_behind_cc0.jpg": (.25, .20, .65, .60),
    "blue_sky_clouds_cc0.jpg": (.30, .25, .70, .65),
}


def load_source(name: str, manifest: dict[str, object]) -> tuple[Image.Image, dict[str, object]]:
    path = INPUT_DIR / name
    provenance = manifest[name]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != provenance["download_sha256"]:
        raise ValueError(f"source SHA-256 mismatch: {name}")
    with Image.open(path) as opened:
        image = ImageOps.exif_transpose(opened)
        profile = image.info.get("icc_profile")
        profile_name = "none"
        if profile:
            src = ImageCms.ImageCmsProfile(io.BytesIO(profile))
            profile_name = ImageCms.getProfileName(src).strip()
            image = ImageCms.profileToProfile(image, src, ImageCms.createProfile("sRGB"), outputMode="RGB")
        else:
            image = image.convert("RGB")
        dimensions = list(image.size)
        image.load()
        image = image.copy()
    return image, {"file": name, "SHA-256": digest, "source dimensions after EXIF orientation": dimensions,
                   "embedded ICC profile": profile_name, "working color condition": "ICC to sRGB; untagged assumed sRGB",
                   "provenance": provenance}


def resize_long_edge(image: Image.Image, size: int) -> Image.Image:
    scale = size / max(image.size)
    dimensions = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    return image.resize(dimensions, Image.Resampling.LANCZOS)


def common_view(image: Image.Image, dimensions: tuple[int, int]) -> np.ndarray:
    rgb = image.convert("RGB")
    if rgb.size != dimensions:
        rgb = rgb.resize(dimensions, Image.Resampling.LANCZOS)
    return np.asarray(rgb, dtype=np.uint8)


def ensemble_variability(stack: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    pixel_count = int(np.count_nonzero(mask))
    if not pixel_count:
        return {"pixel count": 0, "metrics": None}
    linear = srgb_to_linear(stack.astype(np.float32) / 255.0)
    channel_std = np.std(linear, axis=0, ddof=1)
    pixel_std = np.sqrt(np.mean(channel_std * channel_std, axis=2))
    luma_stack = linear @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    luma_std = np.std(luma_stack, axis=0, ddof=1)
    selected_pixel_std = pixel_std[mask]
    selected_luma_std = luma_std[mask]
    return {"pixel count": pixel_count, "metrics": {
        "seed-ensemble linear RGB std RMS": float(np.sqrt(np.mean(selected_pixel_std ** 2))),
        "seed-ensemble per-pixel RGB std median": float(np.median(selected_pixel_std)),
        "seed-ensemble per-pixel RGB std P95": float(np.percentile(selected_pixel_std, 95)),
        "seed-ensemble linear Rec709 luma std RMS": float(np.sqrt(np.mean(selected_luma_std ** 2))),
        "seed-ensemble luma std P95": float(np.percentile(selected_luma_std, 95)),
    }}


def paste_sheet_cell(sheet: Image.Image, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont,
                     view: np.ndarray, row: int, column: int, label: str,
                     source_name: str, crop: bool) -> None:
    cell_w, cell_h, label_h = 160, 120, 20
    x, y = column * cell_w, row * (cell_h + label_h)
    draw.text((x + 3, y + 3), label[:26], fill=(245, 245, 245), font=font)
    image = Image.fromarray(view, "RGB")
    if crop:
        u0, v0, u1, v1 = CROP_RECTS[source_name]
        image = image.crop((round(u0 * image.width), round(v0 * image.height),
                            round(u1 * image.width), round(v1 * image.height)))
    image.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
    sheet.paste(image, (x + (cell_w - image.width) // 2,
                        y + label_h + (cell_h - image.height) // 2))


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    seed_records: list[dict[str, object]] = []
    case_records: list[dict[str, object]] = []
    row_count = len(ASSET_NAMES) * len(PRESETS)
    columns = len(SIZES) * len(SELECTED_SEEDS)
    sheets = {False: Image.new("RGB", (columns * 160, row_count * 140), (27, 28, 31)),
              True: Image.new("RGB", (columns * 160, row_count * 140), (27, 28, 31))}
    draws = {crop: (ImageDraw.Draw(sheet), ImageFont.load_default()) for crop, sheet in sheets.items()}

    row = 0
    for source_name in ASSET_NAMES:
        original, source_meta = load_source(source_name, manifest)
        sources[source_name] = source_meta
        inputs = {str(size): resize_long_edge(original, size) for size in SIZES}
        reference_dimensions = inputs[str(REFERENCE_SIZE)].size
        input_reference = common_view(inputs[str(REFERENCE_SIZE)], reference_dimensions).astype(np.float32) / 255.0
        regions = make_regions(input_reference)
        for preset_name, preset in PRESETS.items():
            for size in SIZES:
                key = str(size)
                image = inputs[key].convert("RGBA")
                grainless_settings = dict(preset)
                grainless_settings["grain"] = 0.0
                grainless = apply_retro(image, **grainless_settings, seed=SEEDS[0])
                grainless_view = common_view(grainless, reference_dimensions)
                sample_views: list[np.ndarray] = []
                per_seed_metrics: list[dict[str, object]] = []
                output_endpoint_by_seed: list[dict[str, float]] = []

                for seed in SEEDS:
                    result = apply_retro(image, **preset, seed=seed)
                    source_alpha = np.asarray(image.getchannel("A"))
                    result_alpha = np.asarray(result.getchannel("A"))
                    if not np.array_equal(source_alpha, result_alpha):
                        raise RuntimeError(f"alpha mismatch {source_name}/{preset_name}/{size}/{seed}")
                    view = common_view(result, reference_dimensions)
                    sample_views.append(view)
                    view_float = view.astype(np.float32) / 255.0
                    base_float = grainless_view.astype(np.float32) / 255.0
                    region_effect = {region: region_error(base_float, view_float, mask)
                                     for region, mask in regions.items()}
                    endpoint = {}
                    for region, mask in regions.items():
                        endpoint[region] = {
                            "pixel count": int(np.count_nonzero(mask)),
                            "code-0 channel ratio": float(np.mean(view[mask] == 0)) if np.any(mask) else None,
                            "code-255 channel ratio": float(np.mean(view[mask] == 255)) if np.any(mask) else None,
                        }
                    output_endpoint_by_seed.append(endpoint)
                    seed_metrics = {"source": source_name, "preset": preset_name,
                                    "size": size, "seed": seed,
                                    "grain residual versus same-size grainless": region_effect,
                                    "common-display endpoint occupancy": endpoint}
                    per_seed_metrics.append(seed_metrics)
                    seed_records.append(seed_metrics)
                    if seed in SELECTED_SEEDS:
                        col = SIZES.index(size) * len(SELECTED_SEEDS) + SELECTED_SEEDS.index(seed)
                        label = f"{preset_name} {size} s{seed}"
                        for crop in (False, True):
                            draw, font = draws[crop]
                            paste_sheet_cell(sheets[crop], draw, font, view, row, col,
                                             label, source_name, crop)

                stack = np.stack(sample_views, axis=0)
                variability = {region: ensemble_variability(stack, mask) for region, mask in regions.items()}
                seed_residual_summary = {}
                for region in regions:
                    values = [m["grain residual versus same-size grainless"][region]["metrics"]
                              for m in per_seed_metrics]
                    rms = [value["linear RGB RMSE"] for value in values if value is not None]
                    changed = [value["pixel ratio max channel delta >1/255 linear"]
                               for value in values if value is not None]
                    seed_residual_summary[region] = {
                        "grain residual RGB RMSE median across seeds": float(np.median(rms)) if rms else None,
                        "grain residual RGB RMSE seed min/max": [float(np.min(rms)), float(np.max(rms))] if rms else None,
                        "changed-pixel ratio median across seeds": float(np.median(changed)) if changed else None,
                    }
                case_records.append({"source": source_name, "preset": preset_name, "size": size,
                                    "native dimensions": list(image.size), "seed count": len(SEEDS),
                                    "seed range": [SEEDS[0], SEEDS[-1]],
                                    "grainless baseline": "apply_retro with same preset and grain=0; normalized to common 512-input dimensions",
                                    "grain residual summary by region": seed_residual_summary,
                                    "seed-ensemble output variability by region": variability,
                                    "common-display endpoint occupancy median across seeds": {
                                        region: {
                                            "code-0 component ratio": (float(np.median(values0)) if values0 else None),
                                            "code-255 component ratio": (float(np.median(values255)) if values255 else None),
                                        }
                                        for region in regions
                                        for values0 in [[ep[region]["code-0 channel ratio"] for ep in output_endpoint_by_seed
                                                        if ep[region]["code-0 channel ratio"] is not None]]
                                        for values255 in [[ep[region]["code-255 channel ratio"] for ep in output_endpoint_by_seed
                                                          if ep[region]["code-255 channel ratio"] is not None]]
                                    }})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheets[False].save(CAPTURE / "grain-seed-variance-overview.png")
    sheets[True].save(CAPTURE / "grain-seed-variance-detail-crops.png")

    report = {
        "question": "How much do current-suite grain outputs and ROI statistics vary across deterministic seeds at native 512/2048 input sizes?",
        "evaluation plan": {
            "baseline": "Same-size current apply_retro preset with grain=0 isolates the fixed non-grain pipeline; it is not a desired appearance target.",
            "sources": "Same five SHA-256 verified photos; EXIF orientation, embedded ICC to sRGB, untagged assumed sRGB.",
            "conditions": f"Current PRESETS and unmodified apply_retro at input long edge {list(SIZES)}; seeds {list(SEEDS)}. Outputs normalize to exact 512px-input dimensions for paired seed statistics.",
            "regions": "Whole, shadow <.02, deep-shadow .02-.08, midtone .08-.6, highlight >=.8, smooth gradient <.004, detail gradient >.025 from the common 512 input. Masks used at normalized display dimensions.",
            "metrics": "Per-seed linear RGB residual versus same-size grainless output and endpoint occupancy; across-seed unbiased per-pixel linear RGB std, luma std, P95 and per-region median/range.",
            "sampling": f"{len(SEEDS)} deterministic consecutive seed values; this finite seed set is not a probability confidence interval or independent machine sample.",
            "visual review": "Eight selected seeds per photo/preset row (four seeds at each input size), shown at common display dimensions and normalized source ROI crops. No calibrated display or approved target.",
            "alpha and temporal": "Photo input is opaque and output alpha exact-match is checked. Different still seeds do not simulate playback or establish temporal stability.",
            "performance": "Not measured. Artifact/Core/GPU and end-to-end image decode paths are not invoked.",
            "acceptance": "Descriptive stochastic variability only; no preset quality ranking or threshold.",
            "unverified": "More/randomly sampled seeds, motion-footage temporal behavior, other hardware/Python environments, approved appearance criteria, product preview/export and memory/performance.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "seed count": len(SEEDS), "seed values": list(SEEDS),
        "case summaries": case_records,
        "per-seed records": seed_records,
        "visuals": {"overview": str((CAPTURE / "grain-seed-variance-overview.png").relative_to(ROOT)),
                    "detail crops": str((CAPTURE / "grain-seed-variance-detail-crops.png").relative_to(ROOT))},
        "limits": "Variation is measured after RGB8 conversion and common Lanczos display normalization. Spatial std across seeds is not temporal flicker, preference, or a probability guarantee beyond this fixed seed list.",
    }
    (CAPTURE / "photo-grain-seed-variance-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {len(result['case summaries'])} cases and {len(result['per-seed records'])} seeded outputs.")
