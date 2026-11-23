"""Map Core Posterize's float-level quantizer over SDR and extended values."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
LEVELS = (2.0, 2.5, 3.0, 4.0, 4.5, 8.0, 16.0, 64.0)
DOMAINS = ((0.0, 1.0, "SDR [0,1]"), (-0.5, 1.5, "negative / >1"), (0.0, 4.0, "HDR [0,4]"))
WIDTH = 2049


def core_quantize(values: np.ndarray, levels: float) -> np.ndarray:
    """Mirror PosterizeEffect.cpp float arithmetic and floor(x + .5)."""
    n = np.maximum(np.float32(2.0), np.float32(levels))
    step_scale = np.float32(n - np.float32(1.0))
    scaled = np.float32(np.float32(values) * step_scale + np.float32(0.5))
    return np.float32(np.floor(scaled) / step_scale)


def resident_quantize(values: np.ndarray, levels: float) -> np.ndarray:
    """Model PointwiseEffectFusion's explicit saturate before the same quantizer."""
    return core_quantize(np.clip(values, 0.0, 1.0), levels)


def stats(source: np.ndarray, output: np.ndarray, low: float, high: float) -> dict[str, object]:
    clipped = np.clip(output, low, high)
    return {
        "unique output levels": int(np.unique(output).size),
        "output min": float(np.min(output)),
        "output max": float(np.max(output)),
        "output samples below zero": int(np.count_nonzero(output < 0.0)),
        "output samples above one": int(np.count_nonzero(output > 1.0)),
        "range-clipped output fraction": float(np.mean((output < low) | (output > high))),
        "mean abs error in declared domain": float(np.mean(np.abs(output - source))),
        "mean abs error after domain clipping": float(np.mean(np.abs(clipped - source))),
    }


def main() -> dict[str, object]:
    output_dir = ROOT / "captures" / "core-posterize-domain"
    fixture_dir = ROOT / "fixtures"
    output_dir.mkdir(parents=True, exist_ok=True)
    fixture_dir.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    panels: list[tuple[str, float, float, float, np.ndarray, np.ndarray, np.ndarray]] = []

    for low, high, domain_name in DOMAINS:
        ramp = np.linspace(low, high, WIDTH, dtype=np.float32)
        for levels in LEVELS:
            result = core_quantize(ramp, levels)
            resident = resident_quantize(ramp, levels)
            parity_delta = np.abs(result - resident)
            rows.append({
                "domain": domain_name,
                "input range": [low, high],
                "levels parameter": levels,
                **stats(ramp, result, low, high),
                "Core CPU vs resident changed sample ratio": float(np.mean(parity_delta != 0.0)),
                "Core CPU vs resident max abs difference": float(np.max(parity_delta)),
                "Core CPU vs resident mean abs difference": float(np.mean(parity_delta)),
            })
            panels.append((domain_name, low, high, levels, ramp, result, resident))

    # Probe half-step boundaries from both sides and at the tie itself.
    tie_rows = []
    for levels in LEVELS:
        denominator = np.float32(max(2.0, levels) - 1.0)
        ties = np.arange(-2, int(math.ceil(float(denominator) * 4.0)) + 3, dtype=np.float32)
        boundary = np.float32((ties + np.float32(0.5)) / denominator)
        probes = np.stack((np.nextafter(boundary, np.float32(-np.inf)), boundary,
                           np.nextafter(boundary, np.float32(np.inf))), axis=1).reshape(-1)
        quantized = core_quantize(probes, levels)
        products = np.float32(probes * denominator)
        exact_tie = (products - np.floor(products)) == np.float32(0.5)
        exact_tie_outputs = quantized[exact_tie]
        expected_tie_indices = np.floor(products[exact_tie]) + np.float32(1.0)
        actual_tie_indices = np.rint(np.float32(exact_tie_outputs * denominator))
        tie_rows.append({
            "levels parameter": levels,
            "probe count": int(probes.size),
            "nondecreasing": bool(np.all(np.diff(quantized) >= 0.0)),
            "exactly representable half-step count": int(np.count_nonzero(exact_tie)),
            "exact half-step rounds upward": bool(np.array_equal(actual_tie_indices, expected_tie_indices)),
            "negative output count": int(np.count_nonzero(quantized < 0.0)),
            "greater-than-one output count": int(np.count_nonzero(quantized > 1.0)),
        })

    report = {
        "source": "ArtifactCore/src/Graphics/Effect/PosterizeEffect.cpp",
        "equation": "n=max(2, levels); output=floor(input*(n-1)+0.5)/(n-1), evaluated as float32; no explicit [0,1] clamp.",
        "additional source comparison": "Artifact CPU Posterize uses the same unbounded equation; PointwiseEffectFusion resident HLSL applies saturate(input) before quantization. This model compares source equations and is not GPU runtime parity evidence.",
        "limits": "Source-derived Python model only. VideoFrame float transfer/scene-linear contract, production call path, HDR acceptance, runtime parity, and visual quality are not established.",
        "conditions": len(rows),
        "results": rows,
        "half-step probes": tie_rows,
    }
    report_path = output_dir / "core-posterize-domain-metrics.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")

    panel_w, panel_h, label_h = 384, 112, 22
    sheet = Image.new("RGB", (len(LEVELS) * panel_w, len(DOMAINS) * (panel_h + label_h)), (30, 31, 35))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for index, (domain_name, low, high, levels, ramp, result, resident) in enumerate(panels):
        col, row = index % len(LEVELS), index // len(LEVELS)
        left, top = col * panel_w, row * (panel_h + label_h)
        label = f"{domain_name} | levels={levels:g}"
        draw.text((left + 4, top + 4), label, fill=(245, 245, 245), font=font)
        x_axis = np.linspace(0, panel_w - 1, WIDTH).astype(np.int32)
        # Normalize each declared input range to display luminance; mark output steps orange.
        input_y = np.rint((1.0 - (ramp - low) / (high - low)) * (panel_h - 1)).astype(np.int32)
        output_y = np.rint((1.0 - (result - low) / (high - low)) * (panel_h - 1)).astype(np.int32)
        resident_y = np.rint((1.0 - (resident - low) / (high - low)) * (panel_h - 1)).astype(np.int32)
        input_y = np.clip(input_y, 0, panel_h - 1)
        output_y = np.clip(output_y, 0, panel_h - 1)
        resident_y = np.clip(resident_y, 0, panel_h - 1)
        for x, iy, oy in zip(x_axis, input_y, output_y):
            sheet.putpixel((left + int(x), top + label_h + int(iy)), (170, 180, 190))
            sheet.putpixel((left + int(x), top + label_h + int(oy)), (255, 133, 52))
        for x, ry in zip(x_axis, resident_y):
            sheet.putpixel((left + int(x), top + label_h + int(ry)), (69, 198, 230))
        y0 = top + label_h
        draw.line((left, y0 + panel_h // 2, left + panel_w - 1, y0 + panel_h // 2), fill=(68, 69, 73))
    sheet_path = output_dir / "core-posterize-domain-sheet.png"
    sheet.save(sheet_path)
    manifest = {
        "origin": "Synthetic float ramps generated by this script; no external assets.",
        "levels": list(LEVELS),
        "domains": [{"range": [low, high], "label": label} for low, high, label in DOMAINS],
        "ramp width": WIDTH,
        "metrics sha256": hashlib.sha256(report_path.read_bytes()).hexdigest(),
    }
    (fixture_dir / "core-posterize-domain-manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Mapped {result['conditions']} Posterize domain/level combinations")
