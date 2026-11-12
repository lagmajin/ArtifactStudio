"""Isolate output downsampling filter and scale effects on current-suite grain."""

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
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-resample-filter"
SIZES = (512, 1024, 2048)
FILTERS = {
    "box": Image.Resampling.BOX,
    "bilinear": Image.Resampling.BILINEAR,
    "bicubic": Image.Resampling.BICUBIC,
    "lanczos": Image.Resampling.LANCZOS,
}
LABELS = tuple([("512 native", 512, None)] +
               [(f"{size} {name}", size, name) for size in SIZES[1:] for name in FILTERS])


def resize_to_view(image: Image.Image, dimensions: tuple[int, int], filter_name: str | None) -> np.ndarray:
    rgb = image.convert("RGB")
    if rgb.size != dimensions:
        if filter_name is None:
            raise ValueError("a resampling filter is required when dimensions change")
        rgb = rgb.resize(dimensions, FILTERS[filter_name])
    return np.asarray(rgb, dtype=np.uint8)


def residual_metrics(on: np.ndarray, off: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if count == 0:
        return {"pixel count": 0, "metrics": None}
    delta = srgb_to_linear(on.astype(np.float32) / 255.0) - srgb_to_linear(off.astype(np.float32) / 255.0)
    luma = delta @ np.array([.2126, .7152, .0722], dtype=np.float32)
    vals = luma[mask]
    return {"pixel count": count, "metrics": {
        "linear luma residual RMS": float(np.sqrt(np.mean(vals * vals))),
        "linear luma residual P95 absolute": float(np.percentile(np.abs(vals), 95)),
        "linear luma residual signed mean": float(np.mean(vals)),
        "linear RGB residual RMSE": float(np.sqrt(np.mean(delta[mask] ** 2))),
        "RGB8 changed pixel ratio": float(np.mean(np.any(on != off, axis=2)[mask])),
    }}


def residual_spread(seed_residuals: list[np.ndarray], mask: np.ndarray) -> dict[str, float]:
    if not np.any(mask):
        return {"seed luma std RMS": None, "seed luma std P95": None}
    stack = np.stack(seed_residuals, axis=0)
    # Per-pixel seed standard deviation after subtracting each seed's own grain-off control.
    luma_std = np.std(stack, axis=0, ddof=1)
    values = luma_std[mask]
    return {"seed luma std RMS": float(np.sqrt(np.mean(values * values))),
            "seed luma std P95": float(np.percentile(values, 95))}


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    records: list[dict[str, object]] = []
    rows = len(ASSET_NAMES) * len(PRESETS)
    tile_w, tile_h, label_h = 122, 86, 22
    sheet = Image.new("RGB", (len(LABELS) * tile_w, rows * (tile_h + label_h)), (27, 28, 31))
    detail = Image.new("RGB", sheet.size, (27, 28, 31))
    draws = [(img, ImageDraw.Draw(img), ImageFont.load_default()) for img in (sheet, detail)]
    row = 0
    for source_name in ASSET_NAMES:
        original, provenance = load_source(source_name, manifest)
        sources[source_name] = provenance
        inputs = {size: resize_long_edge(original, size) for size in SIZES}
        view_dims = inputs[512].size
        source_view = np.asarray(inputs[512].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(source_view)
        crop_rect = CROP_RECTS[source_name]
        for preset_name, preset in PRESETS.items():
            for size in SIZES:
                image = inputs[size].convert("RGBA")
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off_native = apply_retro(image, **off_settings, seed=SELECTED_SEEDS[0])
                filter_names: tuple[str | None, ...] = (None,) if size == 512 else tuple(FILTERS)
                for filter_name in filter_names:
                    label_name = "512 native" if size == 512 else f"{size} {filter_name}"
                    column = next(i for i, item in enumerate(LABELS) if item[0] == label_name)
                    off = resize_to_view(off_native, view_dims, filter_name)
                    residuals: list[np.ndarray] = []
                    seed_records = []
                    for seed in SELECTED_SEEDS:
                        on_native = apply_retro(image, **preset, seed=seed)
                        if not np.array_equal(np.asarray(image.getchannel("A")),
                                              np.asarray(on_native.getchannel("A"))):
                            raise RuntimeError(f"alpha mismatch {source_name}/{preset_name}/{size}/{seed}")
                        on = resize_to_view(on_native, view_dims, filter_name)
                        residuals.append(srgb_to_linear(on.astype(np.float32) / 255.0) -
                                         srgb_to_linear(off.astype(np.float32) / 255.0))
                        seed_records.append({"seed": seed, "regions": {
                            name: residual_metrics(on, off, mask) for name, mask in regions.items()}})

                        if seed == SELECTED_SEEDS[0]:
                            for image_index, (_, draw, font) in enumerate(draws):
                                thumb = Image.fromarray(on, "RGB")
                                if image_index:
                                    u0, v0, u1, v1 = crop_rect
                                    thumb = thumb.crop((round(u0 * thumb.width), round(v0 * thumb.height),
                                                        round(u1 * thumb.width), round(v1 * thumb.height)))
                                thumb.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
                                x, y = column * tile_w, row * (tile_h + label_h)
                                draw.text((x + 2, y + 3), f"{preset_name} {label_name}"[:23],
                                          fill=(245, 245, 245), font=font)
                                target = sheet if image_index == 0 else detail
                                target.paste(thumb, (x + (tile_w - thumb.width) // 2,
                                                     y + label_h + (tile_h - thumb.height) // 2))

                    records.append({"source": source_name, "preset": preset_name, "input long edge": size,
                                    "resampler": "none/native" if filter_name is None else filter_name,
                                    "seed count": len(SELECTED_SEEDS), "regions": seed_records,
                                    "seed spread by region": {name: residual_spread(
                                        [residual @ np.array([.2126, .7152, .0722], dtype=np.float32)
                                         for residual in residuals], mask)
                                        for name, mask in regions.items()}})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet.save(CAPTURE / "grain-resampler-overview.png")
    detail.save(CAPTURE / "grain-resampler-detail-crops.png")
    report = {
        "question": "How much do output reduction ratio and resampling kernel independently affect current-suite grain residual at common display dimensions?",
        "evaluation plan": {
            "pipeline": "Current apply_retro output is converted to RGB8 and resized with Pillow in encoded sRGB code values, matching the existing photo-suite common-view handoff.",
            "conditions": f"Five SHA-verified photos; four current presets; input long edges {list(SIZES)}; 512 reference dimensions; 4 deterministic seeds {list(SELECTED_SEEDS)}; native at 512, and BOX/BILINEAR/BICUBIC/LANCZOS at 1024 and 2048.",
            "pairing": "Each grain-on render is differenced from same-size, same-preset grain=0 output after the same resampler. Other retro stages remain enabled.",
            "regions and metrics": "Seven source tone/detail regions from the 512 input; signed linear-light Rec.709 and RGB residual RMS/P95/change ratio, plus per-pixel seed spread. Masks and RGB8 outputs use a common 512-input display canvas.",
            "alpha": "RGBA alpha exact-match checked before RGB display resize.",
            "decision": "Descriptive filter/scale study only. No approved target, acceptance threshold, or quality ranking.",
            "unverified": "Linear-light resampling, non-Pillow product display, observer preference, calibrated display, temporal coherence, GPU/Core path, performance/memory.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "seeds": list(SELECTED_SEEDS), "records": records,
        "visuals": {"overview": str((CAPTURE / "grain-resampler-overview.png").relative_to(ROOT)),
                    "detail crops": str((CAPTURE / "grain-resampler-detail-crops.png").relative_to(ROOT))},
        "limits": "Grain residual includes nonlinear interaction with quantization, channel offsets, scanlines, clipping, RGB8 conversion, and encoded-value filtering. Filter results characterize this Python pipeline only; seed spread uses four selected deterministic still seeds, not a temporal sequence.",
    }
    (CAPTURE / "photo-grain-resample-filter-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Rendered {len(records)} filter/scale groups across {len(records) * len(SELECTED_SEEDS)} paired cases.")
    return report


if __name__ == "__main__":
    main()
