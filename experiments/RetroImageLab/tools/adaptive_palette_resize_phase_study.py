"""Measure palette temporal stability under BOX downscale and nearest upscale phase."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_smoothing_study import align_palette, source_error  # noqa: E402
from adaptive_palette_temporal_stability_study import (  # noqa: E402
    ASSETS,
    PALETTE_SIZE,
    assign_palette,
    compare_overlap,
    get_masks,
    load_asset,
    median_cut_palette,
)

CAPTURE = ROOT / "captures" / "adaptive-palette-resize-phase"
SCALES = (0.5, 0.75)
FRAME_COUNT = 9
EMA = 0.25
SOURCE_CROP = 384
TILE = 128
STRATEGIES = ("per-frame", "EMA 0.25", "shot-shared")
PHASES = ("phase locked", "phase cycling")


def pan_origins(extent: int, crop_size: int, alignment: int) -> tuple[list[int], list[int]]:
    travel = max(0, extent - crop_size)
    locked_end = travel - travel % alignment
    exact = np.linspace(0, locked_end, FRAME_COUNT)
    locked = sorted(set(int(round(value / alignment)) * alignment for value in exact))
    cycling = []
    for index, value in enumerate(locked):
        if index == 0 or index == FRAME_COUNT - 1:
            position = int(round(value))
        else:
            position = int(value) + index % alignment
        minimum = cycling[-1] + 1 if cycling else 0
        maximum = locked_end - (FRAME_COUNT - index - 1)
        cycling.append(min(maximum, max(minimum, position)))
    if len(locked) < 2 or len(cycling) < 2:
        raise ValueError("pan trajectory collapsed to fewer than two crop positions")
    return locked, cycling


def downscale(crop: np.ndarray, scale: float) -> tuple[np.ndarray, np.ndarray]:
    height, width = crop.shape[:2]
    size = (max(1, int(round(width * scale))), max(1, int(round(height * scale))))
    rgb_image = Image.fromarray(crop[:, :, :3], "RGB").resize(size, Image.Resampling.BOX)
    alpha_image = Image.fromarray(crop[:, :, 3], "L").resize(size, Image.Resampling.BOX)
    return np.asarray(rgb_image, dtype=np.uint8), np.asarray(alpha_image, dtype=np.uint8)


def upscale_nearest(rgb8: np.ndarray, size: int) -> np.ndarray:
    return np.asarray(Image.fromarray(rgb8, "RGB").resize((size, size), Image.Resampling.NEAREST), dtype=np.uint8)


def make_sheet(frames: dict[str, list[dict[str, object]]], asset: str, scale: float, path: Path) -> None:
    label_h = 18
    columns = FRAME_COUNT
    rows = len(PHASES) * len(STRATEGIES)
    sheet = Image.new("RGB", (columns * TILE, rows * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for phase_index, phase in enumerate(PHASES):
        for strategy_index, strategy in enumerate(STRATEGIES):
            row = phase_index * len(STRATEGIES) + strategy_index
            for col, frame in enumerate(frames[phase]):
                x, y = col * TILE, row * (TILE + label_h)
                phase_code = "L" if phase_index == 0 else "C"
                draw.text((x + 2, y + 2), f"{phase_code}{frame['index']:02d} {strategy}", fill=(245, 245, 245), font=font)
                panel = Image.fromarray(frame[strategy], "RGBA").resize((TILE, TILE), Image.Resampling.NEAREST)
                panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
                sheet.paste(panel.convert("RGB"), (x, y + label_h))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def process_phase(rgba8: np.ndarray, positions: list[int], y0: int, scale: float,
                  shared_palette: np.ndarray) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    frames: list[dict[str, object]] = []
    outputs: list[dict[str, object]] = []
    local_palettes = []
    crop_arrays = []
    lowres_arrays = []
    for x0 in positions:
        crop = rgba8[y0:y0 + SOURCE_CROP, x0:x0 + SOURCE_CROP]
        low_rgb, low_alpha = downscale(crop, scale)
        palette = median_cut_palette(low_rgb, low_alpha, PALETTE_SIZE, "alpha coverage", "srgb") if np.any(low_alpha) else shared_palette
        crop_arrays.append(crop)
        lowres_arrays.append((low_rgb, low_alpha))
        local_palettes.append(palette)

    ema_palettes = [local_palettes[0]]
    aligned = [local_palettes[0]]
    for index in range(1, len(local_palettes)):
        aligned_palette = align_palette(aligned[-1], local_palettes[index])
        aligned.append(aligned_palette)
        ema_palettes.append(ema_palettes[-1] * (1.0 - EMA) + aligned_palette * EMA)

    for index, (x0, crop, low_arrays) in enumerate(zip(positions, crop_arrays, lowres_arrays)):
        low_rgb, _ = low_arrays
        variants = {"per-frame": local_palettes[index], "EMA 0.25": ema_palettes[index], "shot-shared": shared_palette}
        frame: dict[str, object] = {"index": index}
        mapped = {}
        for strategy, palette in variants.items():
            low_output, _ = assign_palette(low_rgb, palette, "srgb")
            low8 = np.uint8(np.clip(low_output * 255.0 + 0.5, 0, 255))
            high8 = upscale_nearest(low8, SOURCE_CROP)
            high_rgba = np.dstack((high8, crop[:, :, 3]))
            if not np.array_equal(high_rgba[:, :, 3], crop[:, :, 3]):
                raise ValueError(f"alpha changed in {strategy} frame {index}")
            mapped[strategy] = high8
            frame[strategy] = high_rgba
        frames.append(frame)
        outputs.append({"x": x0, "mapped": mapped, "alpha": crop[:, :, 3]})
    return frames, outputs


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba8, provenance, digest = load_asset(spec)
    h, w = rgba8.shape[:2]
    if min(h, w) < SOURCE_CROP:
        raise ValueError(f"asset smaller than analysis crop: {spec[3]}")
    alpha_full = rgba8[:, :, 3]
    all_scales = {}
    for scale in SCALES:
        alignment = 2 if scale == 0.5 else 4
        locked_x, cycling_x = pan_origins(w, SOURCE_CROP, alignment)
        center_y = max(0, (h - SOURCE_CROP) // 2)
        locked_y = center_y - center_y % alignment
        # Train a scale-specific shared palette using the complete source after BOX reduction.
        full_rgb, full_alpha = downscale(rgba8, scale)
        shared_palette = median_cut_palette(full_rgb, full_alpha, PALETTE_SIZE, "alpha coverage", "srgb")
        phase_positions = {"phase locked": locked_x, "phase cycling": cycling_x}
        phase_frames = {}
        phase_outputs = {}
        for phase in PHASES:
            x_positions = phase_positions[phase]
            y0 = locked_y
            frames, outputs = process_phase(rgba8, x_positions, y0, scale, shared_palette)
            phase_frames[phase] = frames
            phase_outputs[phase] = outputs

        metrics = {}
        for phase in PHASES:
            positions = phase_positions[phase]
            outputs = phase_outputs[phase]
            y_origin = locked_y
            adjacent = []
            for index, (x0, x1) in enumerate(zip(positions, positions[1:])):
                left, right = outputs[index], outputs[index + 1]
                overlap_x0, overlap_x1 = max(x0, x1), min(x0 + SOURCE_CROP, x1 + SOURCE_CROP)
                overlap_y0, overlap_y1 = y_origin, y_origin + SOURCE_CROP
                a_slice = (slice(None), slice(overlap_x0 - x0, overlap_x1 - x0))
                b_slice = (slice(None), slice(overlap_x0 - x1, overlap_x1 - x1))
                source_alpha = alpha_full[overlap_y0:overlap_y1, overlap_x0:overlap_x1]
                masks = get_masks(source_alpha)
                pair = {"frames": [index, index + 1], "source overlap xyxy": [overlap_x0, overlap_y0, overlap_x1, overlap_y1], "strategies": {}}
                for strategy in STRATEGIES:
                    pair["strategies"][strategy] = compare_overlap(
                        left["mapped"][strategy][a_slice], right["mapped"][strategy][b_slice], source_alpha, masks
                    )
                adjacent.append(pair)
            phase_summary = {}
            source_errors = {strategy: [] for strategy in STRATEGIES}
            for output, crop in zip(outputs, (rgba8[y_origin:y_origin + SOURCE_CROP, x:x + SOURCE_CROP] for x in positions)):
                for strategy in STRATEGIES:
                    source_errors[strategy].append(source_error(
                        crop[:, :, :3], output["mapped"][strategy].astype(np.float32) / 255.0, crop[:, :, 3]
                    ))
            for strategy in STRATEGIES:
                phase_summary[strategy] = {
                    "mean adjacent changed visible pixel ratio >1/255": float(np.mean([
                        pair["strategies"][strategy]["visible"]["changed visible pixel ratio"] for pair in adjacent
                    ])),
                    "mean adjacent visible RGB8 RMSE": float(np.mean([
                        pair["strategies"][strategy]["visible"]["uncomposited RGB8 RMSE"] for pair in adjacent
                    ])),
                    "mean adjacent alpha-edge RGB8 RMSE": float(np.mean([
                        pair["strategies"][strategy]["alpha edge"]["uncomposited RGB8 RMSE"] for pair in adjacent
                    ])),
                    "mean source display sRGB RMSE on mid-gray by region": {
                        region: float(np.mean([entry[region] for entry in source_errors[strategy]]))
                        for region in ("visible", "partial alpha", "alpha edge")
                    },
                }
            metrics[phase] = {"summary by strategy": phase_summary, "adjacent pairs": adjacent}
        sheet_path = CAPTURE / f"{spec[3]}-scale-{scale:.2f}-phase.png"
        make_sheet(phase_frames, spec[3], scale, sheet_path)
        all_scales[f"{scale:.2f}"] = {
            "scale": scale, "alignment unit source pixels": alignment,
            "phase locked x origins": locked_x, "phase cycling x origins": cycling_x,
            "phase locked y origin": locked_y, "phase cycling y origin": locked_y,
            "phase cycling origin mod alignment": [x % alignment for x in cycling_x],
            "frame count": {phase: len(phase_positions[phase]) for phase in PHASES},
            "metrics by phase": metrics,
            "contact sheet": str(sheet_path.relative_to(ROOT)),
        }
    return {
        "asset": spec[3], "source file": spec[0], "source SHA-256": digest,
        "provenance": provenance, "dimensions": [w, h], "source crop size": SOURCE_CROP,
        "scales": all_scales,
    }


def main() -> dict[str, object]:
    report = {
        "question": "Does crop-local palette EMA stability survive BOX downscale plus nearest upscale, and how does crop origin phase affect it?",
        "method": "For scales 0.5 and 0.75, take nine horizontal pan crops. Downscale RGB and alpha independently with Pillow BOX, learn 8-color alpha-weighted sRGB median-cut palettes at low resolution, map colors, then nearest-upscale palette output to source crop size while retaining original alpha. Compare per-crop palette, aligned EMA .25, and a scale-specific full-source palette. The phase-locked path keeps crop origins at multiples of 2/4 source pixels (scale denominator); phase-cycling uses integer-spaced origins across the same available travel. Compare identical world coordinates in adjacent crop overlaps.",
        "limits": "Research model of the RetroImageLab resize order and an adaptive palette stage; not a Core or Artifact runtime render. RGB and alpha are BOX-filtered separately (straight RGB), Bayer dither is disabled to isolate spatial resize phase, and no motion interpolation or scene changes are included. Metrics are pixel-code differences, not perceptual judgments. Three CC0 assets only; embedded color profiles are not applied.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-resize-phase-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured resize phase and palette temporal stability across {len(report['assets'])} CC0 assets at scales {SCALES}.")
