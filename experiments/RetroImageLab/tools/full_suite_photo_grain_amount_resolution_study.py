"""Factor current-suite grain amount and input resolution with an exact float replay gate."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_grain_quantization_counterfactual_study import (  # noqa: E402
    render_prebyte_float, resize_rgb8, resize_srgb_float,
)
from full_suite_photo_grain_seed_variance_study import (  # noqa: E402
    ASSET_NAMES, CROP_RECTS, MANIFEST, SELECTED_SEEDS, load_source, resize_long_edge,
)
sys.path.insert(0, str(ROOT))
from full_suite_photo_tone_study import make_regions  # noqa: E402
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-amount-resolution"
SIZES = (512, 1024, 2048)
AMOUNT_FACTORS = (0.5, 1.0, 1.5, 2.0)
DISPLAY_EDGE = 512
RESAMPLER = Image.Resampling.LANCZOS
RESAMPLER_NAME = "lanczos"  # 512 condition is native/no resize.
DISPLAY_METHODS = ("sRGB float", "sRGB RGB8")


def normalize_float(rgb: np.ndarray, dimensions: tuple[int, int]) -> np.ndarray:
    return rgb if (rgb.shape[1], rgb.shape[0]) == dimensions else resize_srgb_float(rgb, dimensions, RESAMPLER)


def normalize_rgb8(rgb: np.ndarray, dimensions: tuple[int, int]) -> np.ndarray:
    return resize_rgb8(rgb, dimensions, RESAMPLER)


def region_metrics(float_on: np.ndarray, float_off: np.ndarray,
                   byte_on: np.ndarray, byte_off: np.ndarray,
                   mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "metrics": None}
    float_delta = srgb_to_linear(float_on[mask]) - srgb_to_linear(float_off[mask])
    byte_delta = srgb_to_linear(byte_on[mask]) - srgb_to_linear(byte_off[mask])
    quant_delta = byte_delta - float_delta
    weights = np.array([.2126, .7152, .0722], dtype=np.float32)
    f_luma, b_luma, q_luma = float_delta @ weights, byte_delta @ weights, quant_delta @ weights
    return {"pixel count": count, "metrics": {
        "sRGB float grain luma RMS": float(np.sqrt(np.mean(f_luma * f_luma))),
        "sRGB RGB8 grain luma RMS": float(np.sqrt(np.mean(b_luma * b_luma))),
        "RGB8/float grain luma RMS ratio": float(np.sqrt(np.mean(b_luma * b_luma)) /
                                                    max(float(np.sqrt(np.mean(f_luma * f_luma))), 1e-12)),
        "quantization interaction luma RMS": float(np.sqrt(np.mean(q_luma * q_luma))),
        "sRGB float grain luma signed mean": float(np.mean(f_luma)),
        "sRGB RGB8 grain luma signed mean": float(np.mean(b_luma)),
        "sRGB float grain luma P95 absolute": float(np.percentile(np.abs(f_luma), 95)),
        "sRGB RGB8 grain luma P95 absolute": float(np.percentile(np.abs(b_luma), 95)),
        "RGB8 grain changed pixel ratio": float(np.mean(np.any(byte_on[mask] != byte_off[mask], axis=1))),
        "float output low endpoint channel ratio": float(np.mean(float_on[mask] <= 0.0)),
        "float output high endpoint channel ratio": float(np.mean(float_on[mask] >= 1.0)),
        "RGB8 output low endpoint channel ratio": float(np.mean(byte_on[mask] <= 0.0)),
        "RGB8 output high endpoint channel ratio": float(np.mean(byte_on[mask] >= 1.0)),
    }}


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    records: list[dict[str, object]] = []
    row_count = len(ASSET_NAMES) * len(PRESETS)
    columns = [(size, factor) for size in SIZES for factor in AMOUNT_FACTORS]
    cell_w, cell_h, label_h = 128, 84, 22
    sheet_width = len(columns) * len(DISPLAY_METHODS) * cell_w
    overview = Image.new("RGB", (sheet_width, row_count * (cell_h + label_h)), (27, 28, 31))
    detail = Image.new("RGB", overview.size, (27, 28, 31))
    overview_draw, overview_font = ImageDraw.Draw(overview), ImageFont.load_default()
    detail_draw, detail_font = ImageDraw.Draw(detail), ImageFont.load_default()
    row = 0

    for source_name in ASSET_NAMES:
        original, source_meta = load_source(source_name, manifest)
        sources[source_name] = source_meta
        inputs = {size: resize_long_edge(original, size) for size in SIZES}
        view = resize_long_edge(original, DISPLAY_EDGE)
        display_dims = view.size
        source_mask = np.asarray(view.convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(source_mask)
        crop_rect = CROP_RECTS[source_name]

        for preset_name, base_preset in PRESETS.items():
            for size in SIZES:
                image = inputs[size].convert("RGBA")
                off_preset = dict(base_preset)
                off_preset["grain"] = 0.0
                off_float_native = render_prebyte_float(image, off_preset, SELECTED_SEEDS[0])
                off_byte_check = np.asarray(apply_retro(image, **off_preset, seed=SELECTED_SEEDS[0])
                                            .convert("RGB"), dtype=np.uint8)
                if not np.array_equal(np.uint8(off_float_native * 255.0), off_byte_check):
                    raise RuntimeError(f"grain-off parity failed: {source_name}/{preset_name}/{size}")
                off_float = normalize_float(off_float_native, display_dims)
                off_byte = normalize_rgb8(off_float_native, display_dims)

                for factor in AMOUNT_FACTORS:
                    preset = dict(base_preset)
                    preset["grain"] = float(base_preset["grain"]) * factor
                    seed_records = []
                    selected_outputs: dict[str, np.ndarray] = {}
                    for seed in SELECTED_SEEDS:
                        on_float_native = render_prebyte_float(image, preset, seed)
                        on_byte = np.uint8(on_float_native * 255.0)  # Current apply_retro conversion.
                        if factor == 1.0:
                            current_rgba = apply_retro(image, **preset, seed=seed)
                            current = np.asarray(current_rgba.convert("RGB"), dtype=np.uint8)
                            if not np.array_equal(on_byte, current):
                                raise RuntimeError(f"nominal RGB8 parity failed: {source_name}/{preset_name}/{size}/{seed}")
                            if not np.array_equal(np.asarray(image.getchannel("A")),
                                                  np.asarray(current_rgba.getchannel("A"))):
                                raise RuntimeError(f"alpha mismatch: {source_name}/{preset_name}/{size}/{seed}")
                        on_float = normalize_float(on_float_native, display_dims)
                        on_rgb8 = normalize_rgb8(on_float_native, display_dims)
                        seed_records.append({"seed": seed, "regions": {
                            name: region_metrics(on_float, off_float, on_rgb8, off_byte, mask)
                            for name, mask in regions.items()},
                            "nominal apply_retro RGB8 parity gate":
                                "passed" if factor == 1.0 else "not applicable to sigma counterfactual"})
                        if seed == SELECTED_SEEDS[0]:
                            selected_outputs["sRGB float"] = on_float
                            selected_outputs["sRGB RGB8"] = on_rgb8

                    for method, pixels in selected_outputs.items():
                        col = columns.index((size, factor)) * len(DISPLAY_METHODS) + DISPLAY_METHODS.index(method)
                        for crop_enabled, target, draw, font in (
                            (False, overview, overview_draw, overview_font),
                            (True, detail, detail_draw, detail_font),
                        ):
                            image_for_sheet = Image.fromarray(
                                np.uint8(np.clip(pixels * 255.0 + .5, 0, 255)), "RGB")
                            if crop_enabled:
                                u0, v0, u1, v1 = crop_rect
                                image_for_sheet = image_for_sheet.crop((
                                    round(u0 * image_for_sheet.width), round(v0 * image_for_sheet.height),
                                    round(u1 * image_for_sheet.width), round(v1 * image_for_sheet.height)))
                            thumb = image_for_sheet.copy()
                            thumb.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
                            x, y = col * cell_w, row * (cell_h + label_h)
                            draw.text((x + 2, y + 3), f"{size} ×{factor:g} {method}"[:24],
                                      fill=(245, 245, 245), font=font)
                            target.paste(thumb, (x + (cell_w - thumb.width) // 2,
                                                 y + label_h + (cell_h - thumb.height) // 2))

                    records.append({"source": source_name, "preset": preset_name,
                                    "input long edge": size, "grain amount factor": factor,
                                    "absolute grain sigma": float(preset["grain"]),
                                    "output resampler": "none/native" if size == DISPLAY_EDGE else RESAMPLER_NAME,
                                    "seeds": seed_records})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    overview_path = CAPTURE / "grain-amount-resolution-overview.png"
    detail_path = CAPTURE / "grain-amount-resolution-detail-crops.png"
    overview.save(overview_path)
    detail.save(detail_path)
    summaries: dict[str, object] = {}
    for preset_name in PRESETS:
        summaries[preset_name] = {}
        for size in SIZES:
            summaries[preset_name][str(size)] = {}
            for factor in AMOUNT_FACTORS:
                selected = [r for r in records if r["preset"] == preset_name and
                            r["input long edge"] == size and r["grain amount factor"] == factor]
                summaries[preset_name][str(size)][str(factor)] = {}
                for domain_metric in ("sRGB float grain luma RMS", "sRGB RGB8 grain luma RMS",
                                      "RGB8/float grain luma RMS ratio", "quantization interaction luma RMS"):
                    vals = [seed["regions"]["whole image"]["metrics"][domain_metric]
                            for rec in selected for seed in rec["seeds"]]
                    summaries[preset_name][str(size)][str(factor)][domain_metric] = {
                        "median over photos and selected seeds": float(np.median(vals)),
                        "min/max": [float(np.min(vals)), float(np.max(vals))],
                    }

    report = {
        "question": "How do grain sigma factor and input resolution independently change current-suite grain residual after a fixed 512px display normalization?",
        "evaluation plan": {
            "materials": "Same five SHA-256 verified photos, EXIF/ICC handling and provenance as experiments 102–112.",
            "conditions": f"Four current presets; long edges {list(SIZES)}; amount factors {list(AMOUNT_FACTORS)}; seeds {list(SELECTED_SEEDS)}; common display dimensions from 512 input; Lanczos for larger inputs.",
            "factorization": "Change only PRESETS grain sigma by the declared multiplier. All other stage settings/order and the deterministic seed remain fixed. Grain-off control is same source/preset/size with grain=0.",
            "parity": "For every nominal-factor condition, pre-byte float replay is required to exactly match current apply_retro RGB8; grain-off replay parity is also checked for every source/preset/size. RGB8 path uses the current truncating uint8 conversion before output resize.",
            "domains and metrics": "Compare pre-byte sRGB float and current RGB8 output paths after common-display resizing. Seven source tone/detail ROIs: paired linear-light Rec.709 luma RMS/P95/signed mean, RGB8/float response ratio, quantization interaction RMS, changed pixels and output endpoint ratios.",
            "visuals": "Overview/detail crops show the selected seed's float and RGB8 output. They are rendered evidence, not calibrated preference judgments.",
            "decision": "Factor-response characterization only; no target, acceptance threshold, preset ranking, or suggested user setting.",
            "unverified": "Human preference, animated temporal grain, actual ArtifactCore/GPU path, display calibration, performance/memory.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "amount factors": list(AMOUNT_FACTORS), "seeds": list(SELECTED_SEEDS),
        "records": records, "summary": summaries,
        "visuals": {"overview": str(overview_path.relative_to(ROOT)),
                    "detail crops": str(detail_path.relative_to(ROOT))},
        "limits": "This study multiplies existing preset sigma and replays the suite's pre-byte equations in Python. Nominal-byte parity verifies the replay at current settings; altered sigma conditions characterize that model but are not selected product defaults. Float display resizing is a counterfactual. Small deterministic still-seed set does not assess temporal stability.",
    }
    (CAPTURE / "photo-grain-amount-resolution-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Measured {len(records)} groups and {sum(len(r['seeds']) for r in records)} seed cases.")
    return report


if __name__ == "__main__":
    main()
