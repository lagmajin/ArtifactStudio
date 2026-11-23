"""Measure grain bin-crossing and clipping by photo tone/detail region."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_display_scale_hypothesis_study import (  # noqa: E402
    ASSET_NAMES, MANIFEST, PRESETS, REFERENCE_SIZE, SEED, SIZES, STAGES,
    apply_retro, display_scaled_apply, load_source, make_regions, normalized_axis, resize_long_edge,
)
from full_suite_photo_grain_factor_study import VARIANTS  # noqa: E402
from retro_lab import BAYER_4, channel_offset  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-roi"


def pregrain_rgb(image: Image.Image, preset: dict[str, object], reference_dimensions: tuple[int, int]) -> np.ndarray:
    rgba = image.convert("RGBA")
    rgb_image = rgba.convert("RGB")
    width, height = rgb_image.size
    ref_width, ref_height = reference_dimensions
    ratio = max(width / ref_width, height / ref_height)
    pixel_scale = min(1.0, float(preset["scale"]) / ratio)
    if pixel_scale < 1:
        small = (max(1, round(width * pixel_scale)), max(1, round(height * pixel_scale)))
        rgb_image = rgb_image.resize(small, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
    rgb = np.asarray(rgb_image, dtype=np.float32) / 255.0
    y = normalized_axis(height, ref_height)
    x = normalized_axis(width, ref_width)
    matrix = BAYER_4[y[:, None] % 4, x[None, :] % 4]
    levels = max(2, int(preset["colors"]))
    step = 1.0 / (levels - 1)
    dither = min(max(float(preset["dither"]), 0.0), 1.0)
    rgb = np.clip(np.floor(np.clip(rgb + matrix[:, :, None] * step * dither, 0.0, 1.0) / step + 0.5) * step,
                  0.0, 1.0)
    rgb = channel_offset(rgb, float(preset["aberration"]) * width / ref_width)
    if float(preset["scanlines"]) > 0:
        rows = normalized_axis(height, ref_height) % 2 == 1
        gain = np.where(rows[:, None, None], 1.0 - min(float(preset["scanlines"]), 0.9), 1.0)
        rgb *= gain
    return rgb


def grain_field(image: Image.Image, preset: dict[str, object], reference_dimensions: tuple[int, int],
                variant: str, size: int) -> np.ndarray:
    width, height = image.size
    ref_width, ref_height = reference_dimensions
    sigma = float(preset["grain"])
    if variant.endswith("amplitude_scaled"):
        sigma *= max(width / ref_width, height / ref_height)
    if variant.startswith("native_pixel"):
        return np.random.default_rng(SEED).normal(0.0, sigma, (height, width, 1)).astype(np.float32)
    if "independent" in variant and size != REFERENCE_SIZE:
        seed = SEED + size * 101 + 17
    else:
        seed = SEED
    field = np.random.default_rng(seed).normal(0.0, sigma, (ref_height, ref_width, 1)).astype(np.float32)
    yi = normalized_axis(height, ref_height)
    xi = normalized_axis(width, ref_width)
    return field[yi[:, None], xi[None, :], :]


def region_interaction(base: np.ndarray, noise: np.ndarray, levels: int,
                       mask: np.ndarray) -> dict[str, object]:
    pixel_count = int(np.count_nonzero(mask))
    if not pixel_count:
        return {"pixel count": 0, "metrics": None}
    raw = base + noise
    clipped = np.clip(raw, 0.0, 1.0)
    level_scale = levels - 1
    base_bin = np.rint(np.clip(base, 0.0, 1.0) * level_scale).astype(np.int16)
    result_bin = np.rint(clipped * level_scale).astype(np.int16)
    bin_changed = base_bin != result_bin
    changed_pixel = np.any(bin_changed, axis=2)
    low = raw < 0.0
    high = raw > 1.0
    masked_noise = noise[mask, 0]
    return {"pixel count": pixel_count, "metrics": {
        "noise sigma observed in code-value domain": float(np.std(masked_noise)),
        "noise RMS in code-value domain": float(np.sqrt(np.mean(masked_noise * masked_noise))),
        "clamp-low component ratio before output clipping": float(np.mean(low[mask])),
        "clamp-high component ratio before output clipping": float(np.mean(high[mask])),
        "output code-0 component ratio": float(np.mean(clipped[mask] <= 0.0)),
        "output code-1 component ratio": float(np.mean(clipped[mask] >= 1.0)),
        "nearest preset-quantization-bin changed component ratio": float(np.mean(bin_changed[mask])),
        "nearest preset-quantization-bin changed pixel ratio": float(np.mean(changed_pixel[mask])),
    }}


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    records: list[dict[str, object]] = []
    for source_name in ASSET_NAMES:
        original, meta = load_source(source_name, manifest)
        sources[source_name] = meta
        inputs = {str(size): resize_long_edge(original, size) for size in SIZES}
        reference_dimensions = inputs[str(REFERENCE_SIZE)].size
        ref_rgb = np.asarray(inputs[str(REFERENCE_SIZE)].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(ref_rgb)
        for preset_name, preset in PRESETS.items():
            for variant in VARIANTS:
                for size in SIZES:
                    key = str(size)
                    image = inputs[key].convert("RGBA")
                    base = pregrain_rgb(image, preset, reference_dimensions)
                    noise = grain_field(image, preset, reference_dimensions, variant, size)
                    expected_rgb8 = np.uint8(np.clip(base + noise, 0.0, 1.0) * 255.0)
                    noise_kind = VARIANTS[variant][0]
                    field_seed = SEED if noise_kind == "native_pixel" or "aligned" in variant or size == REFERENCE_SIZE else SEED + size * 101 + 17
                    preset_variant = dict(preset)
                    if variant.endswith("amplitude_scaled"):
                        preset_variant["grain"] *= max(image.width / reference_dimensions[0],
                                                        image.height / reference_dimensions[1])
                    stage_set = set(STAGES) if noise_kind == "display_cell" else set(STAGES) - {"grain"}
                    reference_result = display_scaled_apply(image, preset_variant, reference_dimensions,
                                                            field_seed, stage_set)
                    reference_rgb8 = np.asarray(reference_result.convert("RGB"))
                    if not np.array_equal(expected_rgb8, reference_rgb8):
                        raise RuntimeError(f"pregrain equation parity failed: {source_name}/{preset_name}/{variant}/{size}")
                    if size == REFERENCE_SIZE:
                        current = apply_retro(image, **preset, seed=SEED)
                        if not np.array_equal(np.asarray(current.convert("RGB")), reference_rgb8):
                            raise RuntimeError(f"512px current-suite parity failed: {source_name}/{preset_name}/{variant}")

                    native_regions: dict[str, np.ndarray] = {}
                    for region, mask in regions.items():
                        if mask.shape == base.shape[:2]:
                            native_regions[region] = mask
                        else:
                            mask_image = Image.fromarray(np.uint8(mask) * 255, "L")
                            native_regions[region] = np.asarray(mask_image.resize(image.size, Image.Resampling.NEAREST)) > 0
                    by_region = {region: region_interaction(base, noise, int(preset["colors"]), mask)
                                 for region, mask in native_regions.items()}
                    records.append({"source": source_name, "preset": preset_name, "grain variant": variant,
                                    "resolution": size, "native dimensions": list(image.size),
                                    "grain sigma multiplier": float(preset_variant["grain"]) / max(float(preset["grain"]), 1e-12),
                                    "grain field seed": field_seed,
                                    "regions": by_region})

    summaries: dict[str, object] = {}
    metric_names = (
        "noise RMS in code-value domain",
        "clamp-low component ratio before output clipping",
        "clamp-high component ratio before output clipping",
        "nearest preset-quantization-bin changed component ratio",
        "nearest preset-quantization-bin changed pixel ratio",
    )
    for variant in VARIANTS:
        summaries[variant] = {}
        for preset_name in PRESETS:
            summaries[variant][preset_name] = {}
            for size in SIZES:
                matching = [r for r in records if r["grain variant"] == variant and r["preset"] == preset_name and r["resolution"] == size]
                regions_summary = {}
                region_names = list(matching[0]["regions"])
                for region in region_names:
                    regions_summary[region] = {}
                    for metric in metric_names:
                        values = [r["regions"][region]["metrics"][metric] for r in matching
                                  if r["regions"][region]["metrics"] is not None]
                        regions_summary[region][metric] = {
                            "median across five photos": float(np.median(values)) if values else None,
                            "min/max": [float(np.min(values)), float(np.max(values))] if values else None,
                            "valid photo count": len(values),
                        }
                summaries[variant][preset_name][str(size)] = regions_summary

    CAPTURE.mkdir(parents=True, exist_ok=True)
    report = {
        "question": "At which source-tone/detail regions does grain cross the preset quantization grid or push values outside [0,1] before clamp?",
        "evaluation plan": {
            "baseline": "The pre-grain float RGB state in the experiment-105 display-scaled equation model; not a desired image.",
            "sources": "Same five provenance/hash-verified photos, EXIF orientation and ICC-to-sRGB handling as experiments 102–106.",
            "conditions": f"Four current presets, grain variants from experiment 106, seed {SEED}, long edge {list(SIZES)}. Other stages use the display-scaled model. Native-resolution masks are nearest-mapped from 512-input masks.",
            "pipeline checkpoint": "Capture float RGB immediately after pixelation, ordered quantization, channel offset and scanlines, immediately before grain. Add the corresponding noise field, then measure theoretical low/high excursions and rounded preset-bin crossing before code clipping.",
            "bin crossing definition": "Round pre-grain and post-grain/clamped channel values to nearest code index using the preset's colors-1 uniform code-value grid. This is a diagnostic grid proxy after channel offset/scanline; it is not the intermediate ordered-quantizer's palette membership.",
            "metrics": "By whole/shadow/deep-shadow/midtone/highlight/smooth/detail ROI: noise RMS, pre-clamp low/high component rate, final endpoint rate and nearest preset-grid channel/pixel crossing rate.",
            "validation": "Pregrain-plus-noise float equation must exactly reproduce display_scaled_apply RGB8 at every size/variant; every 512px variant must exactly match current apply_retro RGB8.",
            "acceptance": "Descriptive interaction analysis only. No approved target, threshold, or perceptual quality ranking.",
            "unverified": "Display appearance acceptance, temporal behavior, product/Core/GPU routes, memory/performance, HDR and wider profile/scene coverage.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "summary median across five photos": summaries,
        "records": records,
        "limits": "The preset quantization grid is a diagnostic reference even though later channel offset and scanlines move values off it. Pre-clamp excursions count RGB components, not defective pixels, and postclip endpoint rates can include source or prior-stage endpoints. Region masks are source-luma/gradient masks and not semantic segmentation.",
    }
    (CAPTURE / "photo-grain-roi-interaction-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {len(result['records'])} photo/preset/variant/resolution ROI sets.")
