"""Measure Core OldTV scanline frequency in pixel and normalized coordinates."""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
HEIGHTS = (240, 480, 720, 1080, 2160)
DENSITIES = (0.0, 0.125, 0.25, 0.5, 0.75, 1.0)
BASELINE_HEIGHT = 240


def scanline_profile(height: int, density: float) -> np.ndarray:
    y = np.arange(height, dtype=np.float64)
    # Exact source equation with curvature=0 and flicker=0, so sy == integer y.
    return 1.0 - np.abs(np.sin(y * density * 2.0)) * 0.2


def count_minima(profile: np.ndarray) -> int:
    return int(np.count_nonzero((profile[1:-1] < profile[:-2]) & (profile[1:-1] <= profile[2:])))


def main() -> dict[str, object]:
    rows = []
    profiles: dict[tuple[int, float], np.ndarray] = {}
    reference_y = np.linspace(0.0, 1.0, BASELINE_HEIGHT)
    for height in HEIGHTS:
        y = np.arange(height, dtype=np.float64)
        for density in DENSITIES:
            profile = scanline_profile(height, density)
            profiles[(height, density)] = profile
            normalized_y = y / max(height - 1, 1)
            reference = scanline_profile(BASELINE_HEIGHT, density)
            normalized_reference = np.interp(normalized_y, reference_y, reference)
            rows.append({
                "height px": height,
                "scanline density": density,
                "source angular frequency rad/row": 2.0 * density,
                "source absolute-sine period rows": (math.pi / (2.0 * density)) if density else None,
                "local dark minima count": count_minima(profile),
                "dark minima per normalized frame": count_minima(profile) / max(height - 2, 1),
                "gain min": float(np.min(profile)),
                "gain max": float(np.max(profile)),
                "gain mean": float(np.mean(profile)),
                "profile RMSE vs 240p mapped by normalized y": float(
                    np.sqrt(np.mean((profile - normalized_reference) ** 2))),
            })

    report = {
        "source": "ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp: sl = 1 - abs(sin(sy * scanlineDensity * 2)) * 0.2",
        "isolation": "Curvature=0, flicker=0, fringe=0; constant input. Thus sy equals integer source row y and only the scanline multiplier is measured.",
        "limits": "Source-derived 1D model only. No product caller, resampling/display scale, temporal behavior, image-quality target, or CPU/GPU parity measurement.",
        "comparison": "Same parameter at several frame heights; normalized-y RMSE linearly maps each profile to the 240-row reference grid. This is a resolution-sensitivity descriptor, not a quality score.",
        "results": rows,
    }
    output_dir = ROOT / "captures" / "core-oldtv-scanline"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "core-oldtv-scanline-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")

    cell_w, cell_h, label_h = 512, 100, 22
    sheet = Image.new("RGB", (len(DENSITIES) * cell_w, len(HEIGHTS) * (cell_h + label_h)), (30, 31, 35))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for row, height in enumerate(HEIGHTS):
        top = row * (cell_h + label_h)
        for col, density in enumerate(DENSITIES):
            left = col * cell_w
            draw.text((left + 4, top + 4), f"height={height} | density={density:g}", fill=(245, 245, 245), font=font)
            profile = profiles[(height, density)]
            sample_y = np.linspace(0, height - 1, cell_w)
            sampled = np.interp(sample_y, np.arange(height), profile)
            image_values = np.rint(sampled * 255.0).astype(np.uint8)
            for x, value in enumerate(image_values):
                draw.line((left + x, top + label_h, left + x, top + label_h + cell_h - 1),
                          fill=(int(value), int(value), int(value)))
    sheet.save(output_dir / "core-oldtv-scanline-sheet.png")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured {len(result['results'])} resolution/density combinations")
