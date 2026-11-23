"""Study the Core ChromaticAberrationEffect integer-angle response in Python."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "fixtures" / "core-chromatic-angle-sweep.png"
OUTPUT = ROOT / "captures" / "core-chromatic-angle"
WIDTH, HEIGHT = 256, 192
AMOUNTS = (0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0, 32.0, 50.0)
PREVIEW_AMOUNTS = (0.5, 1.0, 1.5, 2.0, 4.0, 50.0)
PREVIEW_ANGLES = (0.0, 30.0, 45.0, 60.0, 90.0, 120.0, 180.0, 270.0)
MEMORY_FRAME_SIZES = ((1280, 720), (1920, 1080), (2560, 1440), (3840, 2160), (7680, 4320))


def round_away_from_zero(value: float) -> int:
    """Match std::round for finite values, including negative half ties."""
    return math.floor(value + 0.5) if value >= 0.0 else math.ceil(value - 0.5)


def make_fixture() -> Image.Image:
    y, x = np.mgrid[0:HEIGHT, 0:WIDTH]
    hidden = np.asarray(((255, 0, 255), (0, 255, 40), (10, 35, 255), (245, 210, 0)), dtype=np.uint8)
    selector = ((x // 8 + y // 8) % len(hidden)).astype(np.int32)
    rgba = np.empty((HEIGHT, WIDTH, 4), dtype=np.uint8)
    rgba[:, :, :3] = hidden[selector]
    rgba[:, :, 3] = 0
    image = Image.fromarray(rgba, "RGBA")
    draw = ImageDraw.Draw(image)

    # Distinct RGB edges with opaque and partial-alpha coverage, including all four borders.
    draw.line((0, 20, WIDTH - 1, 20), fill=(245, 52, 45, 255), width=3)
    draw.line((20, 0, 20, HEIGHT - 1), fill=(25, 220, 245, 255), width=3)
    draw.line((0, HEIGHT - 1, WIDTH - 1, 0), fill=(240, 238, 220, 190), width=2)
    draw.line((0, 112, WIDTH - 1, 112), fill=(45, 235, 85, 155), width=5)
    draw.line((142, 0, 142, HEIGHT - 1), fill=(238, 200, 45, 220), width=4)
    draw.rectangle((42, 50, 105, 105), fill=(238, 55, 210, 255))
    draw.rectangle((68, 72, 122, 126), fill=(40, 88, 245, 190))
    draw.line((34, 145, 170, 48), fill=(255, 255, 255, 255), width=1)
    draw.line((132, 142, 230, 170), fill=(20, 220, 120, 255), width=2)
    return image


def source_shifts(amount: float, angle_degrees: float) -> tuple[int, int]:
    angle = math.radians(angle_degrees)
    return round_away_from_zero(amount * math.cos(angle)), round_away_from_zero(amount * math.sin(angle))


def apply_core_model(source: Image.Image, amount: float, angle_degrees: float) -> tuple[Image.Image, dict[str, object]]:
    rgba = np.asarray(source.convert("RGBA"), dtype=np.float32) / 255.0
    height, width, _ = rgba.shape
    shift_x, shift_y = source_shifts(amount, angle_degrees)
    yy, xx = np.mgrid[0:height, 0:width]
    red_x = np.clip(xx + shift_x, 0, width - 1)
    red_y = np.clip(yy + shift_y, 0, height - 1)
    blue_x = np.clip(xx - shift_x, 0, width - 1)
    blue_y = np.clip(yy - shift_y, 0, height - 1)
    output = rgba.copy()
    output[:, :, 0] = rgba[red_y, red_x, 0]
    output[:, :, 2] = rgba[blue_y, blue_x, 2]
    # Quantize to the fixture's 8-bit channels for the displayed and measured output.
    output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
    clamped = ((xx + shift_x != red_x) | (yy + shift_y != red_y))
    meta = {
        "shift R xy": [shift_x, shift_y],
        "shift B xy": [-shift_x, -shift_y],
        "clamped sample pixel ratio per channel": float(np.mean(clamped)),
    }
    return Image.fromarray(output8, "RGBA"), meta


def angle_summary(amount: float) -> dict[str, object]:
    rows = []
    errors = []
    direction_errors = []
    unique = set()
    zero_count = 0
    for angle in range(360):
        sx, sy = source_shifts(amount, float(angle))
        unique.add((sx, sy))
        magnitude = math.hypot(sx, sy)
        ideal_x = amount * math.cos(math.radians(angle))
        ideal_y = amount * math.sin(math.radians(angle))
        error = math.hypot(sx - ideal_x, sy - ideal_y)
        errors.append(error)
        if magnitude == 0.0 and amount > 0.0:
            zero_count += 1
        elif amount > 0.0:
            actual_angle = math.degrees(math.atan2(sy, sx))
            direction_errors.append(abs((actual_angle - angle + 180.0) % 360.0 - 180.0))
    rows = [
        {"angle degrees": angle, "shift R xy": list(source_shifts(amount, float(angle)))}
        for angle in range(360)
    ]
    return {
        "amount px": amount,
        "unique integer shift vectors across 1-degree sweep": len(unique),
        "zero-shift angle ratio": zero_count / 360.0 if amount > 0.0 else 0.0,
        "mean displacement-vector error px": float(np.mean(errors)),
        "maximum displacement-vector error px": float(np.max(errors)),
        "mean direction error among nonzero shifts degrees": float(np.mean(direction_errors)) if direction_errors else 0.0,
        "maximum direction error among nonzero shifts degrees": float(np.max(direction_errors)) if direction_errors else 0.0,
        "per-degree shifts": rows,
    }


def composite_code(image: Image.Image, background: tuple[int, int, int]) -> np.ndarray:
    rgba = np.asarray(image, dtype=np.float32) / 255.0
    bg = np.asarray(background, dtype=np.float32)[None, None, :] / 255.0
    return rgba[:, :, :3] * rgba[:, :, 3:4] + bg * (1.0 - rgba[:, :, 3:4])


def main() -> dict[str, object]:
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = make_fixture()
    source.save(FIXTURE)
    source_sha = hashlib.sha256(FIXTURE.read_bytes()).hexdigest()
    source_array = np.asarray(source, dtype=np.uint8)
    alpha = source_array[:, :, 3]
    visible = alpha > 0
    edge = np.zeros_like(visible)
    padded = np.pad(visible, 1, constant_values=False)
    for dy in range(3):
        for dx in range(3):
            if dx == 1 and dy == 1:
                continue
            edge |= visible != padded[dy:dy + HEIGHT, dx:dx + WIDTH]
    regions = {"whole": np.ones_like(visible), "visible": visible, "edge plus one pixel": edge, "alpha zero": alpha == 0}
    backgrounds = {"black": (0, 0, 0), "mid-gray": (128, 128, 128), "white": (255, 255, 255)}
    selected_cases = []
    for amount in PREVIEW_AMOUNTS:
        for angle in PREVIEW_ANGLES:
            output, shift_meta = apply_core_model(source, amount, angle)
            output_alpha = np.asarray(output.getchannel("A"), dtype=np.uint8)
            case_metrics = {"amount px": amount, "angle degrees": angle, **shift_meta}
            case_metrics["alpha max code difference"] = int(np.max(np.abs(output_alpha.astype(np.int16) - alpha.astype(np.int16))))
            case_metrics["alpha changed ratio"] = float(np.mean(output_alpha != alpha))
            for bg_name, bg in backgrounds.items():
                source_comp = composite_code(source, bg)
                output_comp = composite_code(output, bg)
                delta = np.abs(output_comp - source_comp)
                for region_name, mask in regions.items():
                    case_metrics[f"{bg_name} {region_name} MAE"] = float(np.mean(delta[mask])) if np.any(mask) else 0.0
                    case_metrics[f"{bg_name} {region_name} P95"] = float(np.percentile(delta[mask], 95)) if np.any(mask) else 0.0
            alt = source_array.copy()
            hidden = alpha == 0
            alt[hidden, :3] = np.asarray((18, 235, 77), dtype=np.uint8)
            alt_output, _ = apply_core_model(Image.fromarray(alt, "RGBA"), amount, angle)
            hidden_delta = np.abs(composite_code(output, backgrounds["mid-gray"]) -
                                  composite_code(alt_output, backgrounds["mid-gray"]))
            case_metrics["hidden-RGB pair mid-gray edge MAE"] = float(np.mean(hidden_delta[edge])) if np.any(edge) else 0.0
            selected_cases.append(case_metrics)

    manifest = {
        "fixture": FIXTURE.name,
        "sha256": source_sha,
        "dimensions": f"{WIDTH}x{HEIGHT} RGBA8",
        "origin": "Deterministic synthetic control generated by tools/core_chromatic_angle_study.py.",
        "content": "Hidden RGB checker under alpha zero; opaque and partial-alpha colored lines, cross, boxes, diagonal, and image-border contacts.",
        "core-source-equivalent behavior": "R shift=(round(amount*cos(angle)), round(amount*sin(angle))); B shift is its inverse; source coordinates clamp to image bounds; G and alpha stay at destination pixel. Rounding matches C++ std::round (half away from zero).",
        "measurement composite": "sRGB code-value alpha compositing; not linear-light.",
        "run conditions": {"angles full sweep degrees": "0..359 inclusive, 1-degree steps", "amounts px": AMOUNTS, "preview amounts px": PREVIEW_AMOUNTS, "preview angles degrees": PREVIEW_ANGLES},
    }
    (FIXTURE.parent / "core-chromatic-angle-sweep.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )
    report = {
        "source": manifest,
        "model limits": "Python source-derived equation model only; no Core runtime, arbitrary float image, HDR, GPU, or performance validation.",
        "allocation model": {
            "basis": "ArtifactCore ChromaticAberrationEffect.cpp source: two std::vector<float> temporaries sized width*height; each channel sample pass reads source and writes one temporary; std::copy then reads each temporary and writes its destination channel.",
            "nonzero integer shift": [
                {
                    "frame": f"{width}x{height}",
                    "pixels": width * height,
                    "temporary vector allocations before guard": 2,
                    "temporary capacity bytes before guard": width * height * 2 * 4,
                    "sample and copy traffic bytes": width * height * 8 * 4,
                    "extra value-initialization bytes if zero stores materialize": width * height * 2 * 4,
                    "source-level nominal traffic range bytes": [width * height * 8 * 4, width * height * 10 * 4],
                    "temporary vector allocations after zero-vector guard": 2,
                    "temporary capacity bytes after guard": width * height * 2 * 4,
                }
                for width, height in MEMORY_FRAME_SIZES
            ],
            "zero integer shift after source patch": {
                "temporary vector allocations": 0,
                "temporary capacity bytes": 0,
                "image-plane read/write passes": 0,
            },
            "timing": "No C++ wall-time benchmark was run. Python sweep time is not used as Core performance evidence.",
        },
        "sweep": [angle_summary(amount) for amount in AMOUNTS],
        "selected output cases": selected_cases,
        "regions": {name: int(np.count_nonzero(mask)) for name, mask in regions.items()},
    }
    (OUTPUT / "core-chromatic-angle-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )

    tile_w, tile_h, label_h = WIDTH, HEIGHT, 22
    sheet = Image.new("RGB", (len(PREVIEW_ANGLES) * tile_w, len(PREVIEW_AMOUNTS) * (tile_h + label_h)), (28, 28, 32))
    draw = ImageDraw.Draw(sheet)
    for row, amount in enumerate(PREVIEW_AMOUNTS):
        for col, angle in enumerate(PREVIEW_ANGLES):
            output, shift_meta = apply_core_model(source, amount, angle)
            left, top = col * tile_w, row * (tile_h + label_h)
            sx, sy = shift_meta["shift R xy"]
            draw.text((left + 3, top + 3), f"amount {amount:g}px | {angle:g} deg | R {sx},{sy}",
                      fill=(245, 245, 245), font=ImageFont.load_default())
            display = np.uint8(np.clip(composite_code(output, (128, 128, 128)) * 255.0 + 0.5, 0, 255))
            sheet.paste(Image.fromarray(display, "RGB"), (left, top + label_h))
    sheet.save(OUTPUT / "core-chromatic-angle-sheet.png")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Saved angle sweep for {len(report['sweep'])} amounts and {len(PREVIEW_AMOUNTS) * len(PREVIEW_ANGLES)} preview cases")
