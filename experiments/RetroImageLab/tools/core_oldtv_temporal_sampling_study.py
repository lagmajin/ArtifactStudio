"""Map OldTV's time-coordinate strides without pretending to render Perlin output."""

from __future__ import annotations

import json
import math
from fractions import Fraction
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
PERMUTATION_PERIOD = 256
FRAME_RATES = (
    ("23.976", 24000, 1001),
    ("24", 24, 1),
    ("25", 25, 1),
    ("29.97", 30000, 1001),
    ("30", 30, 1),
    ("50", 50, 1),
    ("59.94", 60000, 1001),
    ("60", 60, 1),
    ("120", 120, 1),
)
COMPONENTS = {
    "global flicker": {"time multiplier": 50, "flicker coefficient": 1.0, "spatial multiplier": None},
    "row jitter": {"time multiplier": 10, "flicker coefficient": 2.0, "spatial multiplier": 0.5},
    "grain": {"time multiplier": 60, "flicker coefficient": 0.05, "spatial multiplier": 10.0},
}


def sampled_period_frames(rate_num: int, rate_den: int, multiplier: int) -> int:
    """Guaranteed frame count returning the time coordinate mod 256."""
    numerator = PERMUTATION_PERIOD * rate_num
    return numerator // math.gcd(multiplier * rate_den, numerator)


def integer_pixel_repeat(multiplier: float) -> int:
    ratio = Fraction(str(multiplier))
    numerator = ratio.numerator
    denominator = ratio.denominator
    return (PERMUTATION_PERIOD * denominator) // math.gcd(
        numerator, PERMUTATION_PERIOD * denominator
    )


def main() -> dict[str, object]:
    rows = []
    for rate_name, rate_num, rate_den in FRAME_RATES:
        for name, component in COMPONENTS.items():
            multiplier = int(component["time multiplier"])
            continuous_period = PERMUTATION_PERIOD / multiplier
            rate = rate_num / rate_den
            sampled_frames = sampled_period_frames(rate_num, rate_den, multiplier)
            rows.append({
                "frame rate": rate_name,
                "frame rate numerator": rate_num,
                "frame rate denominator": rate_den,
                "frame rate fps": rate,
                "component": name,
                "time coordinate multiplier per second": multiplier,
                "noise coordinate step per frame": multiplier * rate_den / rate_num,
                "guaranteed continuous field repeat period seconds": continuous_period,
                "samples per continuous repeat": rate * continuous_period,
                "guaranteed sampled coordinate wrap frames": sampled_frames,
                "guaranteed sampled coordinate wrap seconds": sampled_frames * rate_den / rate_num,
                "coefficient per flicker unit": component["flicker coefficient"],
            })

    report = {
        "source": "ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp and ArtifactCore/src/Math/NoiseGenerator.cpp",
        "source facts": {
            "time coordinates": {name: value["time multiplier"] for name, value in COMPONENTS.items()},
            "flicker coefficients": {name: value["flicker coefficient"] for name, value in COMPONENTS.items()},
            "spatial coordinates": {name: value["spatial multiplier"] for name, value in COMPONENTS.items()},
            "noise lattice indexing": "floor(coordinate) & 255 on each axis; the permutation table repeats every 256 lattice units.",
        },
        "spatial repeats": {
            name: ({
                "source coordinate multiplier per pixel": component["spatial multiplier"],
                "continuous field repeat pixels": PERMUTATION_PERIOD / component["spatial multiplier"],
                "guaranteed integer pixel sample repeat": integer_pixel_repeat(
                    component["spatial multiplier"]
                ) if component["spatial multiplier"] is not None else None,
                "note": "For half-integer multipliers the guaranteed integer repeat uses exact rational coordinate arithmetic; see formula note." if component["spatial multiplier"] == 0.5 else "" ,
            } if component["spatial multiplier"] is not None else None)
            for name, component in COMPONENTS.items()
        },
        "model assumption": "context.time is sampled at n/(FPS numerator/denominator) and the legacy noise permutation remains unchanged. No Perlin values, images, or temporal quality scores are modeled.",
        "period formula": "For rational FPS A/B and integer temporal multiplier k, frame stride is kB/A and the exact sampled repeat is N=256A/gcd(kB,256A). For spatial coordinate multiplier q=a/b in lowest terms, the exact integer-pixel repeat is 256b/gcd(a,256b).",
        "interpretation limit": "Coordinate stride and full 256-lattice wrap are exact under the stated assumption. The wrap is a guaranteed repeat bound, not necessarily the fundamental repeat of the scalar Perlin output. Aliasing, flicker visibility, temporal smoothness, jitter displacement, and perceived grain require actual Perlin/runtime output and display conditions.",
        "results": rows,
    }
    output_dir = ROOT / "captures" / "core-oldtv-temporal"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "core-oldtv-temporal-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")

    width, cell_w, cell_h = 1050, 175, 48
    height = 42 + len(FRAME_RATES) * cell_h
    image = Image.new("RGB", (width, height), (30, 31, 35))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    draw.text((10, 10), "OldTV time-domain sampling | coordinate stride per frame / guaranteed 256-lattice wrap", fill=(240, 240, 240), font=font)
    headers = ("FPS", "global flicker ×50", "row jitter ×10", "grain ×60")
    x_positions = (10, 110, 425, 740)
    for x, label in zip(x_positions, headers):
        draw.text((x, 28), label, fill=(180, 205, 220), font=font)
    by_key = {(row["frame rate"], row["component"]): row for row in rows}
    for row_index, (rate_name, _, _) in enumerate(FRAME_RATES):
        y = 48 + row_index * cell_h
        draw.text((10, y + 8), rate_name, fill=(245, 245, 245), font=font)
        for col, name in enumerate(COMPONENTS):
            entry = by_key[(rate_name, name)]
            label = (f"Δ {entry['noise coordinate step per frame']:.3f} units | "
                     f"wrap {entry['guaranteed sampled coordinate wrap frames']}f")
            x = x_positions[col + 1]
            step = entry["noise coordinate step per frame"]
            shade = max(64, min(235, int(235 - min(step, 3.0) * 48)))
            draw.rounded_rectangle((x - 6, y, x + cell_w - 12, y + 36), radius=4,
                                   fill=(shade // 2, shade, min(255, shade + 12)))
            draw.text((x, y + 8), label, fill=(18, 22, 25), font=font)
    image.save(output_dir / "core-oldtv-temporal-sampling-sheet.png")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Mapped {len(result['results'])} OldTV component/frame-rate combinations")
