"""Compare full-suite grain response before and after the suite's final RGB8 conversion."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_grain_seed_variance_study import (  # noqa: E402
    ASSET_NAMES, CROP_RECTS, MANIFEST, SELECTED_SEEDS, load_source, resize_long_edge,
)
sys.path.insert(0, str(ROOT))
from full_suite_photo_tone_study import make_regions  # noqa: E402
from retro_lab import BAYER_4, PRESETS, apply_retro, channel_offset, ordered_quantize, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-quantization-counterfactual"
SIZES = (1024, 2048)
FILTERS = {"bilinear": Image.Resampling.BILINEAR, "lanczos": Image.Resampling.LANCZOS}
DISPLAY_EDGE = 512
DISPLAY_METHODS = ("sRGB float", "sRGB RGB8")


def render_prebyte_float(image: Image.Image, preset: dict[str, object], seed: int,
                         grain_delta: np.ndarray | None = None,
                         return_stages: bool = False) -> np.ndarray | tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Reproduce apply_retro through clipping; optionally return pre-grain/pre-clamp stages."""
    rgb_image = image.convert("RGB")
    width, height = rgb_image.size
    scale = float(preset["scale"])
    if scale < 1.0:
        reduced = (max(1, round(width * scale)), max(1, round(height * scale)))
        rgb_image = rgb_image.resize(reduced, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)
    rgb = np.asarray(rgb_image, dtype=np.float32) / 255.0
    rgb = ordered_quantize(rgb, max(2, int(preset["colors"])), BAYER_4,
                           min(max(float(preset["dither"]), 0.0), 1.0))
    rgb = channel_offset(rgb, float(preset["aberration"]))
    scanlines = float(preset["scanlines"])
    if scanlines > 0:
        row_gain = np.where(np.arange(height)[:, None, None] % 2 == 1,
                            1.0 - min(scanlines, 0.9), 1.0)
        rgb *= row_gain
    if return_stages:
        pregrain = rgb.copy()
    grain = float(preset["grain"])
    if grain > 0:
        if grain_delta is None:
            rng = np.random.default_rng(seed)
            rgb += rng.normal(0.0, grain, size=(height, width, 1)).astype(np.float32)
        else:
            if grain_delta.shape == (height, width):
                rgb += grain_delta.astype(np.float32, copy=False)[:, :, None]
            elif grain_delta.shape == (height, width, 3):
                rgb += grain_delta.astype(np.float32, copy=False)
            else:
                raise ValueError(
                    f"grain delta shape {grain_delta.shape} does not match {(height, width)} or {(height, width, 3)}"
                )
    if return_stages:
        preclamp = rgb.copy()
        clipped = np.clip(rgb, 0.0, 1.0)
        return pregrain, preclamp, clipped
    return np.clip(rgb, 0.0, 1.0)


def resize_srgb_float(rgb: np.ndarray, size: tuple[int, int], resampler: int) -> np.ndarray:
    channels = [np.asarray(Image.fromarray(rgb[:, :, c].astype(np.float32), mode="F")
                           .resize(size, resampler), dtype=np.float32) for c in range(3)]
    return np.clip(np.stack(channels, axis=2), 0.0, 1.0)


def resize_rgb8(rgb: np.ndarray, size: tuple[int, int], resampler: int) -> np.ndarray:
    byte = np.uint8(np.clip(rgb * 255.0, 0.0, 255.0))  # Exact current apply_retro truncation.
    image = Image.fromarray(byte, mode="RGB").resize(size, resampler)
    return np.asarray(image, dtype=np.float32) / 255.0


def encode_for_sheet(value: np.ndarray, domain: str) -> np.ndarray:
    if domain == "sRGB float":
        return np.uint8(np.clip(value * 255.0 + 0.5, 0.0, 255.0))
    return np.uint8(np.clip(value * 255.0 + 0.5, 0.0, 255.0))


def roi_stats(on: np.ndarray, off: np.ndarray, q_on: np.ndarray, q_off: np.ndarray,
              mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "metrics": None}
    float_delta = srgb_to_linear(on[mask]) - srgb_to_linear(off[mask])
    byte_delta = srgb_to_linear(q_on[mask]) - srgb_to_linear(q_off[mask])
    q_delta = byte_delta - float_delta
    weights = np.array([.2126, .7152, .0722], dtype=np.float32)
    float_luma = float_delta @ weights
    byte_luma = byte_delta @ weights
    q_luma = q_delta @ weights
    return {"pixel count": count, "metrics": {
        "sRGB float paired luma RMS": float(np.sqrt(np.mean(float_luma * float_luma))),
        "sRGB RGB8 paired luma RMS": float(np.sqrt(np.mean(byte_luma * byte_luma))),
        "paired luma RMS ratio RGB8/float": float(np.sqrt(np.mean(byte_luma * byte_luma)) /
                                                      max(float(np.sqrt(np.mean(float_luma * float_luma))), 1e-12)),
        "quantization interaction luma RMS": float(np.sqrt(np.mean(q_luma * q_luma))),
        "sRGB float paired luma signed mean": float(np.mean(float_luma)),
        "sRGB RGB8 paired luma signed mean": float(np.mean(byte_luma)),
        "sRGB float paired luma P95 absolute": float(np.percentile(np.abs(float_luma), 95)),
        "sRGB RGB8 paired luma P95 absolute": float(np.percentile(np.abs(byte_luma), 95)),
        "RGB8 paired changed-pixel ratio": float(np.mean(np.any(q_on[mask] != q_off[mask], axis=1))),
    }}


def heatmap(rgb_delta: np.ndarray, gain: float = 64.0) -> np.ndarray:
    luma = rgb_delta @ np.array([.2126, .7152, .0722], dtype=np.float32)
    pos = np.clip(luma * gain, 0.0, 1.0)
    neg = np.clip(-luma * gain, 0.0, 1.0)
    mid = np.maximum(pos, neg) * .12
    return np.uint8(np.clip(np.stack((pos, mid, neg), axis=2) * 255 + .5, 0, 255))


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    records: list[dict[str, object]] = []
    row_count = len(ASSET_NAMES) * len(PRESETS)
    labels = [(size, f, method) for size in SIZES for f in FILTERS for method in DISPLAY_METHODS]
    delta_labels = [(size, f) for size in SIZES for f in FILTERS]
    cell_w, cell_h, label_h = 126, 82, 22
    output_sheet = Image.new("RGB", (len(labels) * cell_w, row_count * (cell_h + label_h)), (27, 28, 31))
    delta_sheet = Image.new("RGB", (len(delta_labels) * cell_w, row_count * (cell_h + label_h)), (27, 28, 31))
    output_draw, output_font = ImageDraw.Draw(output_sheet), ImageFont.load_default()
    delta_draw, delta_font = ImageDraw.Draw(delta_sheet), ImageFont.load_default()
    row = 0
    for source_name in ASSET_NAMES:
        original, source_meta = load_source(source_name, manifest)
        sources[source_name] = source_meta
        inputs = {size: resize_long_edge(original, size) for size in SIZES}
        view_dims = resize_long_edge(original, DISPLAY_EDGE).size
        source_mask_rgb = np.asarray(resize_long_edge(original, DISPLAY_EDGE).convert("RGB"),
                                     dtype=np.float32) / 255.0
        regions = make_regions(source_mask_rgb)
        crop_rect = CROP_RECTS[source_name]
        for preset_name, preset in PRESETS.items():
            for size in SIZES:
                image = inputs[size].convert("RGBA")
                no_grain = dict(preset)
                no_grain["grain"] = 0.0
                float_off_native = render_prebyte_float(image, no_grain, SELECTED_SEEDS[0])
                byte_off_image = apply_retro(image, **no_grain, seed=SELECTED_SEEDS[0])
                byte_off_native = np.asarray(byte_off_image.convert("RGB"), dtype=np.uint8)
                if not np.array_equal(np.uint8(float_off_native * 255.0), byte_off_native):
                    raise RuntimeError(f"grain-off RGB8 parity failed: {source_name}/{preset_name}/{size}")
                for filter_name, filter_value in FILTERS.items():
                    off_float = resize_srgb_float(float_off_native, view_dims, filter_value)
                    off_byte = resize_rgb8(float_off_native, view_dims, filter_value)
                    seed_records = []
                    first_seed_quant_delta: np.ndarray | None = None
                    first_seed_float: np.ndarray | None = None
                    first_seed_byte: np.ndarray | None = None
                    for seed in SELECTED_SEEDS:
                        on_float_native = render_prebyte_float(image, preset, seed)
                        byte_on_image = apply_retro(image, **preset, seed=seed)
                        byte_on_native = np.asarray(byte_on_image.convert("RGB"), dtype=np.uint8)
                        parity = np.uint8(on_float_native * 255.0)
                        if not np.array_equal(parity, byte_on_native):
                            mismatch = int(np.count_nonzero(np.any(parity != byte_on_native, axis=2)))
                            raise RuntimeError(f"RGB8 parity failed: {source_name}/{preset_name}/{size}/{seed}: {mismatch}")
                        if not np.array_equal(np.asarray(image.getchannel("A")),
                                              np.asarray(byte_on_image.getchannel("A"))):
                            raise RuntimeError(f"alpha mismatch: {source_name}/{preset_name}/{size}/{seed}")
                        on_float = resize_srgb_float(on_float_native, view_dims, filter_value)
                        on_byte = resize_rgb8(on_float_native, view_dims, filter_value)
                        region_results = {name: roi_stats(on_float, off_float, on_byte, off_byte, mask)
                                          for name, mask in regions.items()}
                        seed_records.append({"seed": seed, "regions": region_results,
                                             "native RGB8 parity exact": True,
                                             "alpha exact match": True})
                        if seed == SELECTED_SEEDS[0]:
                            first_seed_quant_delta = (srgb_to_linear(on_byte) - srgb_to_linear(off_byte) -
                                                      (srgb_to_linear(on_float) - srgb_to_linear(off_float)))
                            first_seed_float, first_seed_byte = on_float, on_byte

                    assert first_seed_quant_delta is not None and first_seed_float is not None and first_seed_byte is not None
                    for method, value in (("sRGB float", first_seed_float), ("sRGB RGB8", first_seed_byte)):
                        col = labels.index((size, filter_name, method))
                        thumb = Image.fromarray(encode_for_sheet(value, method), "RGB")
                        u0, v0, u1, v1 = crop_rect
                        thumb = thumb.crop((round(u0 * thumb.width), round(v0 * thumb.height),
                                            round(u1 * thumb.width), round(v1 * thumb.height)))
                        thumb.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
                        x, y = col * cell_w, row * (cell_h + label_h)
                        output_draw.text((x + 2, y + 3), f"{size} {filter_name} {method}"[:23],
                                         fill=(245, 245, 245), font=output_font)
                        output_sheet.paste(thumb, (x + (cell_w - thumb.width) // 2,
                                                   y + label_h + (cell_h - thumb.height) // 2))
                    col = delta_labels.index((size, filter_name))
                    delta_thumb = Image.fromarray(heatmap(first_seed_quant_delta), "RGB")
                    u0, v0, u1, v1 = crop_rect
                    delta_thumb = delta_thumb.crop((round(u0 * delta_thumb.width), round(v0 * delta_thumb.height),
                                                    round(u1 * delta_thumb.width), round(v1 * delta_thumb.height)))
                    delta_thumb.thumbnail((cell_w, cell_h), Image.Resampling.NEAREST)
                    x, y = col * cell_w, row * (cell_h + label_h)
                    delta_draw.text((x + 2, y + 3), f"{size} {filter_name} quant delta x64",
                                    fill=(245, 245, 245), font=delta_font)
                    delta_sheet.paste(delta_thumb, (x + (cell_w - delta_thumb.width) // 2,
                                                    y + label_h + (cell_h - delta_thumb.height) // 2))
                    records.append({"source": source_name, "preset": preset_name, "input long edge": size,
                                    "resampler": filter_name, "grain amount": float(preset["grain"]),
                                    "grain-off RGB8 parity exact": True, "seeds": seed_records})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    output_path = CAPTURE / "grain-outputs-float-vs-rgb8-detail.png"
    delta_path = CAPTURE / "grain-quantization-interaction-detail-x64.png"
    output_sheet.save(output_path)
    delta_sheet.save(delta_path)
    report = {
        "question": "For current full-suite photo renders, how does final RGB8 quantization alter paired grain residual after the same sRGB-domain output resize?",
        "evaluation plan": {
            "materials": "Same five SHA-256 verified photos, EXIF orientation and ICC-to-sRGB provenance as studies 102–111.",
            "conditions": f"Four current presets; input long edges {list(SIZES)}; output dimensions match 512px input; selected seeds {list(SELECTED_SEEDS)}; BILINEAR/LANCZOS output resize.",
            "pipeline parity": "Standalone float replay follows retro_lab.apply_retro through its post-grain clamp. The study requires np.uint8(float_rgb*255) to exactly match apply_retro RGB8 for every grain-on/off case before measuring. This isolates only final RGB8 conversion while keeping all preceding suite operations identical.",
            "display counterfactuals": "sRGB float resizes each encoded RGB channel in Pillow mode F, clamps resized values, then converts to linear for measurement. sRGB RGB8 first applies the current truncating uint8 conversion, then resizes RGB8 and converts the result to linear.",
            "pairing and metrics": "Each grain-on seed is compared against same-source/preset/size/filter grain=0 output in each display path. Seven source tone/detail ROIs: paired luma RMS/P95/signed mean, RGB8/float residual ratio, and RMS of the difference between the two paired residuals.",
            "visualization": "Quantization interaction heatmap uses fixed ×64 linear-luma gain (red positive/blue negative); it can saturate and is not the quantitative measurement.",
            "alpha": "Opaque photo inputs; exact alpha passthrough is checked on current apply_retro outputs.",
            "decision": "Counterfactual numerical study only; no approved target, visual acceptance threshold or product recommendation.",
            "unverified": "GPU/Core path, preview/export equivalence, calibrated display, animation/temporal behavior, performance/memory, perceptual preference.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "selected seeds": list(SELECTED_SEEDS), "records": records,
        "visuals": {"outputs": str(output_path.relative_to(ROOT)),
                    "quantization residual difference x64": str(delta_path.relative_to(ROOT))},
        "limits": "The sRGB float display path is a counterfactual built from the suite's pre-byte output, not an existing product path. The paired residual difference isolates response changes caused by byte conversion only to the extent the replay parity gate holds. Resampling filters are Pillow implementations. Small selected still-seed set and ROI masks do not establish subjective quality or temporal stability.",
    }
    (CAPTURE / "photo-grain-quantization-counterfactual-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Verified RGB8 parity and measured {len(records)} groups / {len(records) * len(SELECTED_SEEDS)} paired renders.")
    return report


if __name__ == "__main__":
    main()
