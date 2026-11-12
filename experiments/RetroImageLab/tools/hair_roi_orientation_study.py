"""Evaluate directional spectrum stability on manually labeled CC0 hair ROIs."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402


SOURCES = (
    {
        "name": "dugi",
        "file": "hair_0_dugi.jpg",
        "manual visual label": "Predominantly vertical hair bundles; local waves and highlights are present.",
        "expected strand tangent degrees": [65.0, 115.0],
    },
    {
        "name": "kleenstudios",
        "file": "hair_fur_texture.png",
        "manual visual label": "Predominantly vertical stylized hair strokes; local values are low contrast.",
        "expected strand tangent degrees": [70.0, 110.0],
    },
)
ROI_ORIGINS = (("upper-left", 64, 64), ("center", 384, 384), ("lower-right", 704, 704))
ROI_SIZES = (128, 256)


def axial_delta_degrees(left: float, right: float) -> float:
    return abs((left - right + 90.0) % 180.0 - 90.0)


def spectrum_metrics(image: Image.Image) -> dict[str, float]:
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    linear = srgb_to_linear(rgb)
    luminance = np.sum(linear * np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32), axis=2)
    height, width = luminance.shape
    centered = luminance - float(np.mean(luminance))
    window = np.outer(np.hanning(height), np.hanning(width))
    power = np.abs(np.fft.rfft2(centered * window)) ** 2
    weights = np.full(power.shape, 2.0, dtype=np.float64)
    weights[:, 0] = 1.0
    if width % 2 == 0:
        weights[:, -1] = 1.0
    power *= weights
    fy = np.fft.fftfreq(height)[:, None]
    fx = np.fft.rfftfreq(width)[None, :]
    radius = np.sqrt(fx * fx + fy * fy)
    theta = np.arctan2(fy, fx)
    directional_band = (radius >= 0.03) & (radius <= 0.35)
    band_power = power * directional_band
    total = float(np.sum(band_power))
    moment = np.sum(band_power * np.exp(2j * theta)) / max(total, 1e-20)
    total_power = float(np.sum(power))
    high_band = (radius >= 0.20) & (radius <= 0.50)
    return {
        "dominant frequency-axis degrees": float((np.degrees(np.angle(moment) * 0.5) + 180.0) % 180.0),
        "directional anisotropy": float(abs(moment)),
        "luminance high-band power fraction 0.20-0.50 cycles/pixel": float(np.sum(power[high_band]) / max(total_power, 1e-20)),
        "linear luminance variance": float(np.var(luminance)),
    }


def main(seed: int = 7) -> dict[str, object]:
    input_dir = ROOT / "eval-inputs" / "cc0-hair-fur"
    output_dir = ROOT / "captures" / "hair-frequency" / "manual-roi-orientation"
    output_dir.mkdir(parents=True, exist_ok=True)
    provenance = json.loads((input_dir / "provenance.json").read_text(encoding="utf-8"))
    treatment_labels: list[str] = ["baseline"]
    for preset in PRESETS:
        treatment_labels.extend((f"{preset} grain-off", f"{preset} actual"))

    source_reports = []
    for source_spec in SOURCES:
        input_path = input_dir / source_spec["file"]
        source_bytes = input_path.read_bytes()
        source_sha = hashlib.sha256(source_bytes).hexdigest()
        provenance_entry = provenance.get(source_spec["file"], {})
        expected_sha = provenance_entry.get("download_sha256", provenance_entry.get("source_sha256"))
        if expected_sha and source_sha != expected_sha:
            raise ValueError(f"Source hash mismatch for {input_path.name}: expected {expected_sha}, got {source_sha}")
        source = Image.open(input_path).convert("RGBA")
        treatment_images: dict[str, Image.Image] = {"baseline": source}
        for preset, settings in PRESETS.items():
            no_grain = settings.copy()
            no_grain["grain"] = 0.0
            treatment_images[f"{preset} grain-off"] = apply_retro(source, **no_grain, seed=seed)
            treatment_images[f"{preset} actual"] = apply_retro(source, **settings, seed=seed)

        roi_results = []
        for roi_name, x, y in ROI_ORIGINS:
            for size in ROI_SIZES:
                box = (x, y, x + size, y + size)
                crops = {label: image.crop(box).convert("RGB") for label, image in treatment_images.items()}
                baseline_metrics = spectrum_metrics(crops["baseline"])
                baseline_frequency_axis = baseline_metrics["dominant frequency-axis degrees"]
                outputs = []
                for label in treatment_labels:
                    metrics = spectrum_metrics(crops[label])
                    metrics["frequency-axis deviation from baseline degrees"] = axial_delta_degrees(
                        metrics["dominant frequency-axis degrees"], baseline_frequency_axis
                    )
                    metrics["anisotropy retention vs baseline"] = metrics["directional anisotropy"] / max(
                        baseline_metrics["directional anisotropy"], 1e-12
                    )
                    metrics["high-band power fraction delta vs baseline"] = (
                        metrics["luminance high-band power fraction 0.20-0.50 cycles/pixel"]
                        - baseline_metrics["luminance high-band power fraction 0.20-0.50 cycles/pixel"]
                    )
                    outputs.append({"treatment": label, **metrics})
                roi_results.append({
                    "roi": roi_name,
                    "box xywh": [x, y, size, size],
                    "manually labeled dominant strand tangent degrees": source_spec["expected strand tangent degrees"],
                    "manual note": source_spec["manual visual label"],
                    "source baseline spectrum": baseline_metrics,
                    "treatments": outputs,
                })

        source_reports.append({
            "source": source_spec["file"],
            "source sha256": source_sha,
            "provenance": provenance_entry,
            "dimensions": list(source.size),
            "alpha extrema": list(source.getchannel("A").getextrema()),
            "manually labeled tangent range": source_spec["expected strand tangent degrees"],
            "manual label note": source_spec["manual visual label"],
            "regions": roi_results,
        })

        for size in ROI_SIZES:
            selected_rois = [r for r in roi_results if r["box xywh"][2] == size]
            tile = size * 2
            label_h = 22
            sheet = Image.new("RGB", (5 * tile, len(selected_rois) * (tile + label_h)), (28, 28, 32))
            draw = ImageDraw.Draw(sheet)
            display_treatments = ["baseline", "handheld actual", "arcade actual", "print actual", "vhs actual"]
            for row, roi in enumerate(selected_rois):
                x, y, _, _ = roi["box xywh"]
                original_crop = source.crop((x, y, x + size, y + size)).convert("RGB")
                for col, label in enumerate(display_treatments):
                    top = row * (tile + label_h)
                    left = col * tile
                    draw.text((left + 4, top + 3), f"{roi['roi']} | {label}", fill=(245, 245, 245), font=ImageFont.load_default())
                    if label == "baseline":
                        crop = original_crop
                    else:
                        crop = treatment_images[label].crop((x, y, x + size, y + size)).convert("RGB")
                    sheet.paste(crop.resize((tile, tile), Image.Resampling.NEAREST), (left, top + label_h))
            sheet.save(output_dir / f"{source_spec['name']}-{size}-roi-sheet.png")

    report = {
        "seed": seed,
        "protocol": "Source baseline is the reference; four existing RetroImageLab presets are measured with grain-off and actual settings. Linear-sRGB luminance FFT with Hann window; dominant frequency axis is axial (0-180 degrees), anisotropy is the normalized second angular moment over 0.03-0.35 cycles/pixel; high-band fraction covers 0.20-0.50 cycles/pixel.",
        "manual ROI protocol": "Three previously used source positions (64,64), (384,384), (704,704), evaluated at 128 and 256 square. Visual labels are approximate dominant strand tangent intervals, not pixelwise centerline annotations or ground truth.",
        "limitations": [
            "Two opaque texture assets only; no transparent strand cutouts.",
            "ROI selection and tangent intervals are analyst annotations without independent reviewer agreement.",
            "Source baseline is not an approved appearance target; metrics are descriptive, not pass criteria or preset rankings.",
            "No Artifact application, GPU/runtime path, temporal, or performance evaluation.",
        ],
        "sources": source_reports,
    }
    (output_dir / "manual-roi-orientation-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )
    return report


if __name__ == "__main__":
    main()
