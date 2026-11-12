"""Measure resolution dependence of the current photo retro suite."""

from __future__ import annotations

import hashlib
import io
import json
import statistics
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_tone_study import ASSET_NAMES, INPUT_DIR, MANIFEST, make_regions, region_error  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-resolution"
SIZES = (512, 1024, 2048)  # longest edge, proportional aspect preserved
REFERENCE_SIZE = 512
SEED = 31111
WARMUPS = 1
REPEATS = 3


def load_original_source(name: str, manifest: dict[str, object]) -> tuple[Image.Image, dict[str, object]]:
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
            src_profile = ImageCms.ImageCmsProfile(io.BytesIO(embedded))
            profile_name = ImageCms.getProfileName(src_profile).strip()
            image = ImageCms.profileToProfile(image, src_profile, ImageCms.createProfile("sRGB"), outputMode="RGB")
        else:
            image = image.convert("RGB")
        original_size = image.size
        image.load()
        image = image.copy()
    return image, {"file": name, "sha256": digest, "source dimensions": list(original_size),
                   "embedded ICC profile": profile_name, "sRGB handling": "ICC to sRGB; untagged assumed sRGB",
                   "provenance": provenance}


def resize_long_edge(image: Image.Image, long_edge: int) -> Image.Image:
    scale = long_edge / max(image.size)
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    return image.resize(size, Image.Resampling.LANCZOS)


def to_display_size(image: Image.Image) -> Image.Image:
    if max(image.size) == REFERENCE_SIZE:
        return image.convert("RGB")
    return resize_long_edge(image.convert("RGB"), REFERENCE_SIZE)


def normalize_to_size(image: Image.Image, target: tuple[int, int]) -> Image.Image:
    if image.size == target:
        return image.convert("RGB")
    return image.convert("RGB").resize(target, Image.Resampling.LANCZOS)


def measure_distribution(rgb: np.ndarray) -> dict[str, float]:
    luma = srgb_to_linear(rgb) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    centered = luma - np.mean(luma)
    window = np.outer(np.hanning(luma.shape[0]), np.hanning(luma.shape[1])).astype(np.float32)
    power = np.abs(np.fft.fftshift(np.fft.fft2(centered * window))) ** 2
    fy = np.fft.fftshift(np.fft.fftfreq(luma.shape[0]))
    fx = np.fft.fftshift(np.fft.fftfreq(luma.shape[1]))
    yy, xx = np.meshgrid(fy, fx, indexing="ij")
    radius = np.sqrt(xx * xx + yy * yy)
    valid = radius > 0
    total = float(np.sum(power[valid]))
    high = float(np.sum(power[valid & (radius >= .20)]) / total) if total else 0.0
    order = np.argsort(radius[valid])
    radii = radius[valid][order]
    cumulative = np.cumsum(power[valid][order])
    spectral_median = float(radii[np.searchsorted(cumulative, cumulative[-1] * .5)]) if cumulative.size and cumulative[-1] else 0.0
    return {"linear Rec709 luma variance": float(np.var(luma)),
            "power fraction >=.20 cycles/display-pixel": high,
            "spectral median cycles/display-pixel": spectral_median,
            "horizontal mean adjacent correlation": float(np.corrcoef(luma[:, :-1].ravel(), luma[:, 1:].ravel())[0, 1]),
            "unique RGB8 colors": int(np.unique(np.uint8(np.clip(rgb * 255 + .5, 0, 255)).reshape(-1, 3), axis=0).shape[0])}


def sample_stats(samples_ms: list[float]) -> dict[str, object]:
    return {"median ms": float(statistics.median(samples_ms)),
            "p95 ms": float(np.percentile(samples_ms, 95)),
            "samples": len(samples_ms)}


def make_sheet(cases: list[dict[str, object]], key: str, filename: str) -> Path:
    cell_w, cell_h, label_h = 200, 200, 22
    cols = len(SIZES)
    rows = len(cases)
    sheet = Image.new("RGB", (cols * cell_w, rows * (cell_h + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, case in enumerate(cases):
        for col, size in enumerate(SIZES):
            x, y = col * cell_w, row * (cell_h + label_h)
            label = f"{case['asset']} / {case['preset']} / {size}px"
            draw.text((x + 3, y + 4), label[:34], fill=(245, 245, 245), font=font)
            image = case[key][str(size)].convert("RGB")
            image.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
            px = x + (cell_w - image.width) // 2
            py = y + label_h + (cell_h - image.height) // 2
            sheet.paste(image, (px, py))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / filename
    sheet.save(path)
    return path


def make_detail_sheet(cases: list[dict[str, object]]) -> tuple[Path, dict[str, list[int]]]:
    crop_rects = {
        "face_portrait_unsplash_cc0.jpg": (.30, .15, .70, .65),
        "black_white_high_iso_portrait_cc0.jpg": (.30, .35, .70, .75),
        "milky_way_grundlsee_cc0.jpg": (.28, .18, .68, .82),
        "clouds_sun_behind_cc0.jpg": (.25, .20, .65, .60),
        "blue_sky_clouds_cc0.jpg": (.30, .25, .70, .65),
    }
    tile, label_h, cols = 256, 20, len(SIZES)
    sheet = Image.new("RGB", (cols * tile, len(cases) * (tile + label_h)), (26, 27, 30))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    crop_manifest: dict[str, list[int]] = {}
    for row, case in enumerate(cases):
        name = case["source file"]
        for col, size in enumerate(SIZES):
            source_size = case["source dimensions by resolution"][str(size)]
            u0, v0, u1, v1 = crop_rects[name]
            x0, y0 = round(u0 * source_size[0]), round(v0 * source_size[1])
            x1, y1 = round(u1 * source_size[0]), round(v1 * source_size[1])
            crop_manifest[f"{case['asset']}/{case['preset']}/{size}"] = [x0, y0, x1 - x0, y1 - y0]
            crop = case["outputs"][str(size)].crop((x0, y0, x1, y1)).convert("RGB")
            crop.thumbnail((tile, tile), Image.Resampling.LANCZOS)
            x, y = col * tile, row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{case['asset'][:12]} {case['preset']} {size}px", fill=(245, 245, 245), font=font)
            px = x + (tile - crop.width) // 2
            py = y + label_h + (tile - crop.height) // 2
            sheet.paste(crop, (px, py))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "photo-resolution-detail-crops-normalized-display.png"
    sheet.save(path)
    return path, crop_manifest


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_info: dict[str, object] = {}
    records: list[dict[str, object]] = []
    sheet_cases: list[dict[str, object]] = []
    for source_name in ASSET_NAMES:
        original, provenance = load_original_source(source_name, manifest)
        source_info[source_name] = provenance
        inputs: dict[str, Image.Image] = {str(size): resize_long_edge(original, size) for size in SIZES}
        reference_dimensions = inputs[str(REFERENCE_SIZE)].size
        input_display = {size: normalize_to_size(image, reference_dimensions) for size, image in inputs.items()}
        input_reference = np.asarray(input_display[str(REFERENCE_SIZE)], dtype=np.float32) / 255.0
        regions = make_regions(input_reference)
        for preset_name, preset in PRESETS.items():
            outputs: dict[str, Image.Image] = {}
            displays: dict[str, Image.Image] = {}
            timings: dict[str, dict[str, object]] = {}
            alpha_exact_by_size: dict[str, bool] = {}
            for size in SIZES:
                image = inputs[str(size)]
                for _ in range(WARMUPS):
                    apply_retro(image, **preset, seed=SEED)
                samples = []
                result = None
                for _ in range(REPEATS):
                    start = time.perf_counter_ns()
                    result = apply_retro(image, **preset, seed=SEED)
                    samples.append((time.perf_counter_ns() - start) / 1_000_000.0)
                assert result is not None
                input_rgba = np.asarray(image.convert("RGBA"))
                result_rgba = np.asarray(result.convert("RGBA"))
                alpha_exact_by_size[str(size)] = bool(np.array_equal(input_rgba[:, :, 3], result_rgba[:, :, 3]))
                outputs[str(size)] = result.convert("RGB")
                displays[str(size)] = normalize_to_size(result, reference_dimensions)
                timings[str(size)] = sample_stats(samples)
            baseline_display = displays[str(REFERENCE_SIZE)]
            baseline_array = np.asarray(baseline_display, dtype=np.float32) / 255.0
            baseline_input = input_reference
            normalized: dict[str, object] = {}
            for size in SIZES:
                display_array = np.asarray(displays[str(size)], dtype=np.float32) / 255.0
                input_array = np.asarray(input_display[str(size)], dtype=np.float32) / 255.0
                input_resampling_delta = {
                    region: region_error(baseline_input, input_array, mask)
                    for region, mask in regions.items()
                }
                output_vs_reference = {
                    region: region_error(baseline_array, display_array, mask)
                    for region, mask in regions.items()
                }
                effect_residual = {
                    region: region_error(input_array, display_array, mask)
                    for region, mask in regions.items()
                }
                normalized[str(size)] = {
                    "input resampling delta from 512px input": input_resampling_delta,
                    "full output delta from 512px full output": output_vs_reference,
                    "effect residual from same-size input": effect_residual,
                    "output texture and distribution": measure_distribution(display_array),
                }
            record = {"asset": Path(source_name).stem, "source file": source_name,
                      "preset": preset_name, "seed": SEED,
                      "source dimensions by resolution": {key: list(value.size) for key, value in inputs.items()},
                      "alpha exact match by resolution": alpha_exact_by_size,
                      "alpha exact match": all(alpha_exact_by_size.values()),
                      "timings": timings, "normalized-to-512px comparisons": normalized,
                      "outputs": outputs}
            records.append(record)
            sheet_cases.append({"asset": Path(source_name).stem, "preset": preset_name,
                                "source file": source_name,
                                "source dimensions by resolution": {key: list(value.size) for key, value in inputs.items()},
                                "outputs": outputs})
    output_sheet = make_sheet(sheet_cases, "outputs", "photo-resolution-suite-512-display-overview.png")
    input_sheet = make_sheet([
        {"asset": Path(name).stem, "preset": "input", "outputs": {size: to_display_size(resize_long_edge(load_original_source(name, manifest)[0], int(size))) for size in map(str, SIZES)}}
        for name in ASSET_NAMES
    ], "outputs", "photo-resolution-input-512-display-overview.png")
    detail_sheet, crop_manifest = make_detail_sheet(sheet_cases)
    report = {
        "question": "When the same ICC-normalized photographs are processed by the current suite at different pixel dimensions, what changes in the effect output at a fixed 512px display size, and how does CPU wall time scale?",
        "evaluation plan": {
            "baseline": "512px-long-edge input and its full-suite output are comparison references only; neither is an approved target.",
            "conditions": f"Five provenance/hash-verified photos, current four PRESETS, proportional longest edge {list(SIZES)}, seed {SEED}, EXIF orientation and ICC-to-sRGB treatment matching the prior photo study. Lanczos is used to prepare each resolution; full-suite apply uses those pixels directly.",
            "display normalization": "Every input/output is downsampled with Lanczos to a common 512px longest edge while preserving aspect. Separately record input-resampling-only drift from the 512 input, full-output drift from the 512 full output, and per-resolution effect residual (same-size input vs processed output), all measured at common display dimensions.",
            "regions": "Whole, shadow, deep shadow, midtone, highlight, smooth tone and detail masks from the 512px reference input; exact thresholds follow the photo tone study. Empty regions remain null.",
            "metrics": "Linear RGB MAE/RMSE/P95 and changed-pixel ratio by region; linear-luma variance, radial FFT high-frequency fraction (>=.20 cycles/display-pixel), median spectral radius, horizontal adjacent correlation, unique RGB8 colors. Time uses one warm-up and three perf_counter_ns repetitions per asset/preset/resolution; median and p95 reported.",
            "alpha": "All selected photo sources are opaque. Alpha differences are not applicable; input/output alpha exact-match is checked as an opaque passthrough contract.",
            "time stability": "Not applicable: still frames, fixed seed, no temporal sequence.",
            "performance": "Python CPU wall time only, one workstation/process. No target budget. Traced/process memory, Artifact/Core/GPU time, preview/export and IO are not measured.",
            "visual review": "512px display overview and normalized subject-region crops compare sizes side by side. A crop maps one normalized source rectangle to a common 256px tile; it is display-matched, not native-pixel scale. No calibrated display transform or approved target.",
            "acceptance": "Descriptive research. No preset ranking, visual pass/fail, or quality threshold is defined.",
            "unverified": "Native-resolution perception beyond 2048px, product resolution route, real preview/final export, GPU/backend parity, more source diversity, HDR/high-bit-depth, memory scaling, multi-machine variance, human preference.",
        },
        "source provenance and decode": source_info,
        "environment": {"python": sys.version, "numpy": np.__version__, "pillow": Image.__version__, "platform": sys.platform,
                        "warmups": WARMUPS, "timed repetitions": REPEATS},
        "records": [{k: v for k, v in r.items() if k != "outputs"} for r in records],
        "input overview": str(input_sheet.relative_to(ROOT)),
        "suite output overview": str(output_sheet.relative_to(ROOT)),
        "normalized detail crops": str(detail_sheet.relative_to(ROOT)),
        "detail crop rectangles per case": crop_manifest,
        "limits": "This measures the standalone Python recipe at explicitly resampled input sizes. Common-size comparisons include interactions between source resampling and the suite, but the input-only drift and same-size effect residuals are reported separately. FFT descriptors and pixel deltas do not determine perceived quality.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "photo-resolution-suite-metrics.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Compared {len(report['records'])} photo/preset records over {len(SIZES)} resolutions.")
