"""Sweep ordered-dither amplitude and coordinate phase during palette EMA pans."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_domain_dither_study import dither_assign  # noqa: E402
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
from retro_lab import linear_to_srgb, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "adaptive-palette-dither-phase-sweep"
EMA = 0.25
STRENGTHS = (0.25, 0.5, 1.0)
PHASES = ("source/world locked", "crop/screen locked")
TILE = 128


def make_sheet(frames: list[dict[str, object]], keys: list[str], path: Path) -> None:
    if len(frames) > 8:
        frames = [frames[index] for index in np.linspace(0, len(frames) - 1, 8).round().astype(int)]
    label_h = 17
    sheet = Image.new("RGB", (len(frames) * TILE, len(keys) * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for col, frame in enumerate(frames):
        for row, key in enumerate(keys):
            x, y = col * TILE, row * (TILE + label_h)
            if key == "per-frame":
                short = "local"
            elif key == "EMA no dither":
                short = "EMA"
            elif key == "shot-shared":
                short = "shared"
            else:
                _, strength, _, phase, _ = key.split(" ", 4)
                short = f"B{strength} {'W' if phase == 'source/world' else 'S'}"
            draw.text((x + 2, y + 2), f"F{frame['index']:02d} {short}", fill=(245, 245, 245), font=font)
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
        palette = median_cut_palette(crop[:, :, :3], alpha, PALETTE_SIZE, "alpha coverage", "srgb") if np.any(alpha) else shared
        crops.append(crop)
        local_palettes.append(palette)
    ema_palettes = [local_palettes[0]]
    aligned = [local_palettes[0]]
    for index in range(1, len(local_palettes)):
        next_palette = align_palette(aligned[-1], local_palettes[index])
        aligned.append(next_palette)
        ema_palettes.append(ema_palettes[-1] * (1.0 - EMA) + next_palette * EMA)

    keys = ["per-frame", "EMA no dither", "shot-shared"]
    for strength in STRENGTHS:
        for phase in PHASES:
            keys.append(f"Bayer {strength:g} / {phase}")
    mapped = {key: [] for key in keys}
    errors = {key: [] for key in keys}
    sheet_frames = []
    for index, (crop, origin) in enumerate(zip(crops, positions)):
        rgb8, alpha = crop[:, :, :3], crop[:, :, 3]
        frame = {"index": index}
        for key in keys:
            if key == "per-frame":
                output, _ = assign_palette(rgb8, local_palettes[index], "srgb")
                output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
            elif key == "EMA no dither":
                output, _ = assign_palette(rgb8, ema_palettes[index], "srgb")
                output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
            elif key == "shot-shared":
                output, _ = assign_palette(rgb8, shared, "srgb")
                output8 = np.uint8(np.clip(output * 255.0 + 0.5, 0, 255))
            else:
                _, strength_text, _, phase_text, _ = key.split(" ", 4)
                strength = float(strength_text)
                world_locked = phase_text == "source/world"
                output8 = dither_assign(rgb8, ema_palettes[index], origin, strength, world_locked)
            output_rgba = np.dstack((output8, alpha))
            if not np.array_equal(output_rgba[:, :, 3], alpha):
                raise ValueError(f"alpha changed at {spec[3]}/{key}/{index}")
            mapped[key].append(output8)
            errors[key].append(source_error(rgb8, output8.astype(np.float32) / 255.0, alpha))
            frame[key] = output_rgba
        sheet_frames.append(frame)

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
        pair = {"frames": [index, index + 1], "strategies": {}}
        for key in keys:
            pair["strategies"][key] = compare_overlap(mapped[key][index][a_slice], mapped[key][index + 1][b_slice], alpha, masks)
        adjacent.append(pair)
    summary = {}
    for key in keys:
        summary[key] = {
            "mean source display sRGB RMSE on mid-gray by region": {
                region: float(np.mean([entry[region] for entry in errors[key]]))
                for region in ("visible", "partial alpha", "alpha edge")
            },
            "mean adjacent changed visible pixel ratio >1/255": float(np.mean([
                pair["strategies"][key]["visible"]["changed visible pixel ratio"] for pair in adjacent
            ])),
            "mean adjacent visible RGB8 RMSE": float(np.mean([
                pair["strategies"][key]["visible"]["uncomposited RGB8 RMSE"] for pair in adjacent
            ])),
            "mean adjacent alpha-edge RGB8 RMSE": float(np.mean([
                pair["strategies"][key]["alpha edge"]["uncomposited RGB8 RMSE"] for pair in adjacent
            ])),
        }
    sheet_path = CAPTURE / f"{spec[3]}-dither-phase-sweep.png"
    make_sheet(sheet_frames, keys, sheet_path)
    return {
        "asset": spec[3], "source file": spec[0], "source SHA-256": digest, "provenance": provenance,
        "dimensions": [w, h], "crop size": [size, size], "frames": len(crops),
        "crop positions xy": [list(position) for position in positions],
        "crop origin Bayer phase xy mod 4": [[x % 4, y % 4] for x, y in positions],
        "EMA factor": EMA, "dither strengths as median NN palette spacing": list(STRENGTHS),
        "summary by strategy": summary, "adjacent pairs": adjacent,
        "contact sheet": str(sheet_path.relative_to(ROOT)),
    }


def main() -> dict[str, object]:
    report = {
        "question": "How do ordered-dither amplitude and crop/screen versus source/world phase affect flicker and source error for an EMA-smoothed palette?",
        "method": "Use EMA .25 in sRGB palette space and the same aligned 8-color pan palette sequence. Sweep Bayer perturbation amplitude at .25/.5/1.0 times median nearest-neighbor palette spacing measured in sRGB, and compare phase held fixed in full-source coordinates versus reset to each crop origin. Dither perturbation uses the same scalar threshold on RGB channels, then nearest-palette assignment in sRGB. Per-frame/no-dither and shot-shared/no-dither are reference conditions.",
        "limits": "Amplitude scaling and scalar RGB perturbation are exploratory choices, not production dither semantics. The sequence is a static image pan; crop-locked phase proxies fixed viewport coordinates. No actual motion, resampling, approved look, runtime, GPU, or performance evaluation. Three CC0 assets only.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-dither-phase-sweep-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    print(f"Measured Bayer strength and phase variants across {len(report['assets'])} CC0 pan sequences.")
