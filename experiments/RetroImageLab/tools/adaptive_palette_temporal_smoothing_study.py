"""Compare per-crop palette updates with aligned temporal palette smoothing."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import (  # noqa: E402
    ASSETS,
    PALETTE_SIZE,
    assign_palette,
    compare_overlap,
    crop_positions,
    get_masks,
    load_asset,
    median_cut_palette,
)
from retro_lab import linear_to_srgb, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "adaptive-palette-temporal-smoothing"
SMOOTHING = (0.25, 0.5)
TILE = 144
STRATEGIES = ("per-frame", "EMA 0.25", "EMA 0.50", "shot-shared")


def align_palette(reference: np.ndarray, candidate: np.ndarray) -> np.ndarray:
    """Return candidate colors in the slot order minimizing total squared distance."""
    count = len(reference)
    costs = np.sum((reference[:, None, :] - candidate[None, :, :]) ** 2, axis=2)
    states: dict[int, tuple[float, tuple[int, ...]]] = {0: (0.0, ())}
    for row in range(count):
        next_states: dict[int, tuple[float, tuple[int, ...]]] = {}
        for mask, (cost, path) in states.items():
            for column in range(count):
                bit = 1 << column
                if mask & bit:
                    continue
                new_mask = mask | bit
                value = cost + float(costs[row, column])
                prior = next_states.get(new_mask)
                if prior is None or value < prior[0]:
                    next_states[new_mask] = (value, path + (column,))
        states = next_states
    permutation = states[(1 << count) - 1][1]
    return candidate[np.asarray(permutation, dtype=np.int32)]


def make_contact_sheet(frames: list[dict[str, Image.Image]], path: Path) -> None:
    selected = frames
    if len(frames) > 9:
        indices = np.linspace(0, len(frames) - 1, 9).round().astype(int)
        selected = [frames[index] for index in indices]
    label_h = 20
    sheet = Image.new("RGB", (len(selected) * TILE, len(STRATEGIES) * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    keys = ("per-frame", "EMA 0.25", "EMA 0.50", "shot-shared")
    for column, frame in enumerate(selected):
        for row, key in enumerate(keys):
            x, y = column * TILE, row * (TILE + label_h)
            draw.text((x + 3, y + 3), f"F{frame['index']:02d} {key}", fill=(245, 245, 245), font=font)
            panel = frame[key].resize((TILE, TILE), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
            sheet.paste(panel.convert("RGB"), (x, y + label_h))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def source_error(rgb8: np.ndarray, output: np.ndarray, alpha: np.ndarray) -> dict[str, float]:
    source = rgb8.astype(np.float32) / 255.0
    a = alpha.astype(np.float32)[:, :, None] / 255.0
    source_linear, output_linear = srgb_to_linear(source), srgb_to_linear(output)
    bg = np.full((1, 1, 3), 0.5, dtype=np.float32)
    source_display = linear_to_srgb(source_linear * a + srgb_to_linear(bg) * (1.0 - a))
    output_display = linear_to_srgb(output_linear * a + srgb_to_linear(bg) * (1.0 - a))
    delta = output_display - source_display
    masks = get_masks(alpha)
    return {
        region: float(np.mean(delta[mask] ** 2) ** 0.5) if np.any(mask) else 0.0
        for region, mask in masks.items()
    }


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba8, provenance, digest = load_asset(spec)
    h, w = rgba8.shape[:2]
    size = min(384, h, w)
    positions = crop_positions(w, h, size)
    alpha_full = rgba8[:, :, 3]
    shared_palette = median_cut_palette(rgba8[:, :, :3], alpha_full, PALETTE_SIZE, "alpha coverage", "srgb")

    local_palettes = []
    crop_inputs = []
    for x, y in positions:
        crop = rgba8[y:y + size, x:x + size]
        crop_alpha = crop[:, :, 3]
        if np.any(crop_alpha > 0):
            palette = median_cut_palette(crop[:, :, :3], crop_alpha, PALETTE_SIZE, "alpha coverage", "srgb")
        else:
            palette = shared_palette
        local_palettes.append(palette)
        crop_inputs.append(crop)

    smoothed: dict[float, list[np.ndarray]] = {amount: [local_palettes[0]] for amount in SMOOTHING}
    aligned_palettes = [local_palettes[0]]
    for index in range(1, len(local_palettes)):
        aligned = align_palette(aligned_palettes[-1], local_palettes[index])
        aligned_palettes.append(aligned)
        for amount in SMOOTHING:
            prior = smoothed[amount][-1]
            smoothed[amount].append(prior * (1.0 - amount) + aligned * amount)

    mapped: dict[str, list[np.ndarray]] = {key: [] for key in STRATEGIES}
    source_errors: dict[str, list[dict[str, float]]] = {key: [] for key in STRATEGIES}
    frame_images: list[dict[str, Image.Image]] = []
    for index, crop in enumerate(crop_inputs):
        alpha = crop[:, :, 3]
        variants = {
            "per-frame": local_palettes[index],
            "EMA 0.25": smoothed[0.25][index],
            "EMA 0.50": smoothed[0.5][index],
            "shot-shared": shared_palette,
        }
        frame = {"index": index}
        for strategy, palette in variants.items():
            output, _ = assign_palette(crop[:, :, :3], palette, "srgb")
            output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
            if output8.shape[:2] != alpha.shape:
                raise ValueError("palette output dimensions changed")
            mapped[strategy].append(output8)
            source_errors[strategy].append(source_error(crop[:, :, :3], output, alpha))
            frame[strategy] = Image.fromarray(np.dstack((output8, alpha)), "RGBA")
        frame_images.append(frame)

    adjacent = []
    for index, (first_pos, second_pos) in enumerate(zip(positions, positions[1:])):
        ax, ay = first_pos
        bx, by = second_pos
        x0, y0, x1, y1 = max(ax, bx), max(ay, by), min(ax + size, bx + size), min(ay + size, by + size)
        if x1 <= x0 or y1 <= y0:
            raise ValueError(f"pan frames {index} and {index + 1} do not overlap")
        a_slice = (slice(y0 - ay, y1 - ay), slice(x0 - ax, x1 - ax))
        b_slice = (slice(y0 - by, y1 - by), slice(x0 - bx, x1 - bx))
        alpha = alpha_full[y0:y1, x0:x1]
        pair = {"frames": [index, index + 1], "overlap xyxy": [x0, y0, x1, y1], "strategies": {}}
        region_masks = get_masks(alpha)
        for strategy in STRATEGIES:
            pair["strategies"][strategy] = compare_overlap(
                mapped[strategy][index][a_slice], mapped[strategy][index + 1][b_slice], alpha, region_masks
            )
        adjacent.append(pair)

    summary = {}
    for strategy in STRATEGIES:
        errors = source_errors[strategy]
        summary[strategy] = {
            "mean source display sRGB RMSE on mid-gray by region": {
                region: float(np.mean([entry[region] for entry in errors]))
                for region in ("visible", "partial alpha", "alpha edge")
            },
            "mean adjacent changed visible pixel ratio >1/255": float(np.mean([
                pair["strategies"][strategy]["visible"]["changed visible pixel ratio"]
                for pair in adjacent
            ])),
            "mean adjacent visible RGB8 RMSE": float(np.mean([
                pair["strategies"][strategy]["visible"]["uncomposited RGB8 RMSE"]
                for pair in adjacent
            ])),
            "mean adjacent alpha-edge RGB8 RMSE": float(np.mean([
                pair["strategies"][strategy]["alpha edge"]["uncomposited RGB8 RMSE"]
                for pair in adjacent
            ])),
        }

    sheet_path = CAPTURE / f"{spec[3]}-palette-smoothing.png"
    make_contact_sheet(frame_images, sheet_path)
    return {
        "asset": spec[3], "source file": spec[0], "source SHA-256": digest,
        "provenance": provenance, "source dimensions": [w, h], "crop size": [size, size],
        "frame count": len(positions), "empty crop fallback count": sum(not np.any(crop[:, :, 3] > 0) for crop in crop_inputs),
        "summary by strategy": summary, "adjacent pairs": adjacent,
        "contact sheet": str(sheet_path.relative_to(ROOT)),
    }


def main() -> dict[str, object]:
    report = {
        "question": "Can palette alignment and temporal EMA reduce crop-local adaptive palette changes while retaining more local color adaptation than a shot-wide palette?",
        "method": "Reuse the 8-color alpha-weighted sRGB median-cut pan sequence. Align each crop palette to the previous crop using a minimum-cost one-to-one RGB assignment, then apply EMA factors 0.25 or 0.50 in aligned sRGB palette space. Compare per-frame raw palette and one full-source shared palette. Measure unchanged world pixels across adjacent crop overlap and source error after linear-light composition over mid-gray, encoded to sRGB.",
        "limits": "Offline deterministic model only. Palette slot alignment and sRGB interpolation are study choices, not a production temporal palette contract. Metrics describe code differences and source fidelity; no perceptual flicker threshold, motion, resampling, dither, codec, runtime, or performance evaluation. Three CC0 asset families; source profiles are not applied.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-temporal-smoothing-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured {len(report['assets'])} CC0 pan sequences across per-frame, two EMA, and shot-shared palette strategies.")
