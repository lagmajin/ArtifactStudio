"""Compare EMA palette interpolation domains and world-locked ordered dither."""

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
    crop_positions,
    get_masks,
    load_asset,
    median_cut_palette,
)
from retro_lab import BAYER_4, linear_to_srgb, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "adaptive-palette-domain-dither"
EMA = 0.25
DITHER_STRENGTH = 0.5
TILE = 140
STRATEGIES = (
    "per-frame",
    "EMA sRGB",
    "EMA linear",
    "EMA sRGB + Bayer",
    "EMA linear + Bayer",
    "shot-shared",
)


def ema_palette(previous: np.ndarray, current_srgb: np.ndarray, amount: float, domain: str) -> np.ndarray:
    if domain == "linear":
        previous_linear = srgb_to_linear(previous)
        current_linear = srgb_to_linear(current_srgb)
        return linear_to_srgb(previous_linear * (1.0 - amount) + current_linear * amount)
    return previous * (1.0 - amount) + current_srgb * amount


def dither_assign(rgb8: np.ndarray, palette: np.ndarray, origin: tuple[int, int],
                  strength: float = DITHER_STRENGTH, world_locked: bool = True) -> np.ndarray:
    """Apply exploratory scalar Bayer perturbation with source/world-locked phase."""
    distances = np.sqrt(np.sum((palette[:, None] - palette[None, :]) ** 2, axis=2))
    distances[distances == 0] = np.inf
    nearest_spacing = np.min(distances, axis=1)
    finite_spacing = nearest_spacing[np.isfinite(nearest_spacing)]
    spacing = float(np.median(finite_spacing)) if finite_spacing.size else 0.0
    h, w = rgb8.shape[:2]
    ox, oy = origin if world_locked else (0, 0)
    rows = (np.arange(h) + oy) % BAYER_4.shape[0]
    cols = (np.arange(w) + ox) % BAYER_4.shape[1]
    threshold = BAYER_4[np.ix_(rows, cols)]
    source = rgb8.astype(np.float32) / 255.0
    perturbed = np.uint8(np.clip(source + threshold[:, :, None] * spacing * strength, 0.0, 1.0) * 255.0 + 0.5)
    result, _ = assign_palette(perturbed, palette, "srgb")
    return np.uint8(np.clip(result * 255.0 + 0.5, 0, 255))


def make_sheet(frames: list[dict[str, object]], path: Path) -> None:
    selected = frames
    if len(frames) > 8:
        selected = [frames[index] for index in np.linspace(0, len(frames) - 1, 8).round().astype(int)]
    label_h = 19
    sheet = Image.new("RGB", (len(selected) * TILE, len(STRATEGIES) * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for column, frame in enumerate(selected):
        for row, key in enumerate(STRATEGIES):
            x, y = column * TILE, row * (TILE + label_h)
            draw.text((x + 2, y + 2), f"F{frame['index']:02d} {key}", fill=(245, 245, 245), font=font)
            panel = Image.fromarray(frame[key], "RGBA").resize((TILE, TILE), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
            sheet.paste(panel.convert("RGB"), (x, y + label_h))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba8, provenance, digest = load_asset(spec)
    h, w = rgba8.shape[:2]
    size = min(384, h, w)
    positions = crop_positions(w, h, size)
    alpha_full = rgba8[:, :, 3]
    shared = median_cut_palette(rgba8[:, :, :3], alpha_full, PALETTE_SIZE, "alpha coverage", "srgb")
    crops, local_palettes = [], []
    for x, y in positions:
        crop = rgba8[y:y + size, x:x + size]
        alpha = crop[:, :, 3]
        local = median_cut_palette(crop[:, :, :3], alpha, PALETTE_SIZE, "alpha coverage", "srgb") if np.any(alpha) else shared
        crops.append(crop)
        local_palettes.append(local)

    ema_srgb = [local_palettes[0]]
    ema_linear = [local_palettes[0]]
    aligned = [local_palettes[0]]
    for index in range(1, len(local_palettes)):
        next_aligned = align_palette(aligned[-1], local_palettes[index])
        aligned.append(next_aligned)
        ema_srgb.append(ema_palette(ema_srgb[-1], next_aligned, EMA, "srgb"))
        ema_linear.append(ema_palette(ema_linear[-1], next_aligned, EMA, "linear"))

    mapped = {strategy: [] for strategy in STRATEGIES}
    errors = {strategy: [] for strategy in STRATEGIES}
    frame_sheets = []
    for index, (crop, (x, y)) in enumerate(zip(crops, positions)):
        alpha, rgb8 = crop[:, :, 3], crop[:, :, :3]
        palettes = {
            "per-frame": local_palettes[index],
            "EMA sRGB": ema_srgb[index],
            "EMA linear": ema_linear[index],
            "EMA sRGB + Bayer": ema_srgb[index],
            "EMA linear + Bayer": ema_linear[index],
            "shot-shared": shared,
        }
        frame = {"index": index}
        for strategy, palette in palettes.items():
            if strategy.endswith("+ Bayer"):
                output8 = dither_assign(rgb8, palette, (x, y))
            else:
                output, _ = assign_palette(rgb8, palette, "srgb")
                output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
            if not np.array_equal(np.dstack((output8, alpha))[:, :, 3], alpha):
                raise ValueError(f"alpha changed: {spec[3]}/{strategy}/{index}")
            mapped[strategy].append(output8)
            errors[strategy].append(source_error(rgb8, output8.astype(np.float32) / 255.0, alpha))
            frame[strategy] = np.dstack((output8, alpha))
        frame_sheets.append(frame)

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
        masks = get_masks(alpha)
        pair = {"frames": [index, index + 1], "overlap area": int(alpha.size), "strategies": {}}
        for strategy in STRATEGIES:
            pair["strategies"][strategy] = compare_overlap(
                mapped[strategy][index][a_slice], mapped[strategy][index + 1][b_slice], alpha, masks
            )
        adjacent.append(pair)

    summary = {}
    for strategy in STRATEGIES:
        summary[strategy] = {
            "mean source display sRGB RMSE on mid-gray by region": {
                region: float(np.mean([entry[region] for entry in errors[strategy]]))
                for region in ("visible", "partial alpha", "alpha edge")
            },
            "mean adjacent changed visible pixel ratio >1/255": float(np.mean([
                pair["strategies"][strategy]["visible"]["changed visible pixel ratio"] for pair in adjacent
            ])),
            "mean adjacent visible RGB8 RMSE": float(np.mean([
                pair["strategies"][strategy]["visible"]["uncomposited RGB8 RMSE"] for pair in adjacent
            ])),
            "mean adjacent alpha-edge RGB8 RMSE": float(np.mean([
                pair["strategies"][strategy]["alpha edge"]["uncomposited RGB8 RMSE"] for pair in adjacent
            ])),
        }
    sheet_path = CAPTURE / f"{spec[3]}-domain-dither.png"
    make_sheet(frame_sheets, sheet_path)
    return {
        "asset": spec[3], "source file": spec[0], "source SHA-256": digest, "provenance": provenance,
        "source dimensions": [w, h], "crop size": [size, size], "frame count": len(crops),
        "summary by strategy": summary, "adjacent pairs": adjacent,
        "contact sheet": str(sheet_path.relative_to(ROOT)),
    }


def main() -> dict[str, object]:
    report = {
        "question": "How do sRGB versus linear-light EMA interpolation and world-locked Bayer perturbation change temporal stability and source fidelity?",
        "method": "Reuse the same 8-color, alpha-weighted sRGB median-cut pan sequence. Align palette slots one-to-one, then apply EMA 0.25 in either sRGB code values or decoded linear RGB. For dither variants, perturb source RGB by a 4x4 Bayer threshold in sRGB using half-strength times the median nearest-neighbor palette spacing measured in sRGB; Bayer phase is locked to source coordinates. Nearest-palette assignment remains in sRGB for every variant.",
        "limits": "The Bayer amplitude rule is an exploratory model, not a production coefficient or effect. Interpolation changes palette centers only; the assignment domain is fixed to sRGB. Temporal metrics compare unchanged world pixels and are not perceptual flicker measures. Three CC0 still images only; no moving video, resize, codec, GPU, runtime, or performance testing.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-domain-dither-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured EMA domain and ordered dither variants across {len(report['assets'])} CC0 assets.")
