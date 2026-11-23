"""Measure palette flicker between overlapping pans of CC0 RGBA assets."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_weighting_study import (  # noqa: E402
    ASSETS,
    assign_palette,
    load_asset,
    median_cut_palette,
)

CAPTURE = ROOT / "captures" / "adaptive-palette-temporal-stability"
PALETTE_SIZE = 8
TILE = 160


def crop_positions(width: int, height: int, size: int) -> list[tuple[int, int]]:
    def covered_positions(extent: int) -> list[int]:
        travel = max(0, extent - size)
        count = max(2, int(np.ceil(travel / max(1.0, size * 0.5))) + 1)
        return sorted(set(int(round(value)) for value in np.linspace(0, travel, count)))

    xs = covered_positions(width)
    ys = covered_positions(height)
    positions = []
    for row, y in enumerate(ys):
        row_xs = xs if row % 2 == 0 else list(reversed(xs))
        positions.extend((x, y) for x in row_xs)
    return positions


def get_masks(alpha: np.ndarray) -> dict[str, np.ndarray]:
    visible = alpha > 0
    partial = (alpha > 0) & (alpha < 255)
    h, w = alpha.shape
    padded = np.pad(visible, 1, constant_values=True)
    edge = partial.copy()
    for dy in range(3):
        for dx in range(3):
            if (dy, dx) != (1, 1):
                edge |= visible != padded[dy:dy + h, dx:dx + w]
    return {"visible": visible, "partial alpha": partial, "alpha edge": edge}


def compare_overlap(rgb_a: np.ndarray, rgb_b: np.ndarray, alpha: np.ndarray,
                    regions: dict[str, np.ndarray]) -> dict[str, object]:
    delta_code = rgb_b.astype(np.float32) - rgb_a.astype(np.float32)
    delta = delta_code / 255.0
    a = alpha.astype(np.float32) / 255.0
    result = {}
    for region, mask in regions.items():
        values = delta[mask]
        values_code = delta_code[mask]
        weights = a[mask, None]
        result[region] = {
            "pixel count": int(np.count_nonzero(mask)),
            "uncomposited RGB8 RMSE": float(np.mean(values_code ** 2) ** 0.5) if values_code.size else 0.0,
            "changed visible pixel ratio": float(np.mean(np.max(np.abs(values), axis=1) > 1.0 / 255.0)) if values.size else 0.0,
            "alpha-weighted RGB8 RMSE": float((np.sum((values_code ** 2) * weights) / max(1e-12, 3.0 * np.sum(weights))) ** 0.5) if values_code.size else 0.0,
        }
    return result


def make_sheet(frame_rows: list[dict[str, object]], path: Path) -> None:
    if len(frame_rows) > 9:
        indices = np.linspace(0, len(frame_rows) - 1, 9).round().astype(int)
        frame_rows = [frame_rows[index] for index in indices]
    columns = len(frame_rows)
    label_h = 20
    sheet = Image.new("RGB", (columns * TILE, 3 * (TILE + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for col, frame in enumerate(frame_rows):
        for row, key in enumerate(("source", "per-frame palette", "shared palette")):
            x, y = col * TILE, row * (TILE + label_h)
            draw.text((x + 3, y + 3), f"F{frame['frame']:02d} {key}", fill=(245, 245, 245), font=font)
            panel = frame[key].resize((TILE, TILE), Image.Resampling.NEAREST)
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
    shared_palette = median_cut_palette(rgba8[:, :, :3], alpha_full, PALETTE_SIZE, "alpha coverage", "srgb")
    crops: list[dict[str, object]] = []
    frame_rows: list[dict[str, object]] = []

    for frame_index, (x, y) in enumerate(positions):
        crop8 = rgba8[y:y + size, x:x + size]
        crop_rgb8, crop_alpha = crop8[:, :, :3], crop8[:, :, 3]
        crop_has_visible_pixels = bool(np.any(crop_alpha > 0))
        local_palette = (
            median_cut_palette(crop_rgb8, crop_alpha, PALETTE_SIZE, "alpha coverage", "srgb")
            if crop_has_visible_pixels else shared_palette
        )
        local_rgb, local_indices = assign_palette(crop_rgb8, local_palette, "srgb")
        shared_rgb, shared_indices = assign_palette(crop_rgb8, shared_palette, "srgb")
        local8 = np.uint8(np.clip(local_rgb * 255.0 + .5, 0, 255))
        shared8 = np.uint8(np.clip(shared_rgb * 255.0 + .5, 0, 255))
        if local8.shape[:2] != crop_alpha.shape or shared8.shape[:2] != crop_alpha.shape:
            raise ValueError("palette output dimensions changed")
        palette_local_rgba = Image.fromarray(np.dstack((local8, crop_alpha)), "RGBA")
        palette_shared_rgba = Image.fromarray(np.dstack((shared8, crop_alpha)), "RGBA")
        source_rgba = Image.fromarray(crop8, "RGBA")
        frame_rows.append({
            "frame": frame_index,
            "source": source_rgba,
            "per-frame palette": palette_local_rgba,
            "shared palette": palette_shared_rgba,
        })
        crops.append({
            "frame": frame_index, "xy": [x, y], "rgb local": local8,
            "rgb shared": shared8, "alpha": crop_alpha,
            "local palette": local_palette, "local used": int(np.unique(local_indices).size),
            "shared used": int(np.unique(shared_indices).size),
            "local training fallback to shared": not crop_has_visible_pixels,
        })

    adjacent = []
    for index, (first, second) in enumerate(zip(crops, crops[1:])):
        ax, ay = first["xy"]
        bx, by = second["xy"]
        x0, y0 = max(ax, bx), max(ay, by)
        x1, y1 = min(ax + size, bx + size), min(ay + size, by + size)
        if x1 <= x0 or y1 <= y0:
            raise ValueError(f"pan frames {index} and {index + 1} do not overlap")
        a_slice = (slice(y0 - ay, y1 - ay), slice(x0 - ax, x1 - ax))
        b_slice = (slice(y0 - by, y1 - by), slice(x0 - bx, x1 - bx))
        alpha = first["alpha"][a_slice]
        world_alpha = alpha_full[y0:y1, x0:x1]
        if not np.array_equal(alpha, world_alpha) or not np.array_equal(second["alpha"][b_slice], world_alpha):
            raise ValueError("overlap alpha samples do not map to the same source coordinates")
        region_masks = get_masks(world_alpha)
        local_change = compare_overlap(first["rgb local"][a_slice], second["rgb local"][b_slice], alpha, region_masks)
        shared_change = compare_overlap(first["rgb shared"][a_slice], second["rgb shared"][b_slice], alpha, region_masks)
        local_palette = first["local palette"]
        next_palette = second["local palette"]
        nearest = np.min(np.linalg.norm(local_palette[:, None, :] - next_palette[None, :, :], axis=2), axis=1)
        reverse_nearest = np.min(np.linalg.norm(next_palette[:, None, :] - local_palette[None, :, :], axis=2), axis=1)
        adjacent.append({
            "frames": [index, index + 1],
            "overlap xyxy": [x0, y0, x1, y1],
            "local palette symmetric nearest-color distance mean RGB8": float((nearest.mean() + reverse_nearest.mean()) * 127.5),
            "per-frame palette output change": local_change,
            "shared palette output change": shared_change,
        })

    sheet_path = CAPTURE / f"{spec[3]}-pan-palette-stability.png"
    make_sheet(frame_rows, sheet_path)
    return {
        "asset": spec[3], "source file": spec[0], "source SHA-256": digest,
        "provenance": provenance, "source dimensions": [w, h],
        "frame crop size": [size, size], "frame positions xy": [row["xy"] for row in crops],
        "frame palette used colors": {"per-frame": [row["local used"] for row in crops], "shared": [row["shared used"] for row in crops]},
        "empty crop fallback count": int(sum(row["local training fallback to shared"] for row in crops)),
        "adjacent overlap measurements": adjacent,
        "contact sheet": str(sheet_path.relative_to(ROOT)),
    }


def main() -> dict[str, object]:
    report = {
        "question": "Does relearning an alpha-weighted 8-color adaptive palette for every panned frame change unchanged source pixels more than reusing a single shot-wide palette?",
        "method": "For each CC0 image, visit overlapping square crops covering the full available pan travel in a serpentine path. Crop count is chosen so adjacent starts are no more than half a crop apart; count varies by source dimensions. Compare alpha-weighted, sRGB, 8-color median cut trained on each crop against one palette trained on the full source. On each adjacent frame pair, compare palette-mapped RGB at identical world coordinates in their overlap. Alpha is preserved. No dither or resampling.",
        "metrics": "RGB8 RMSE is measured in code-value units. Changed-visible-pixel ratio uses normalized sRGB code delta >1/255 (strictly greater than one 8-bit code step). Alpha-edge and partial-alpha ROIs are reported separately.",
        "limits": "This is an offline deterministic model, not a video render or Core implementation. The pan traverses a single static image and only tests crop-dependent palette retraining. Metrics describe pixel changes and are not perceptual flicker scores. Source color values are assumed sRGB; color profiles are not applied. Only three existing CC0 asset families are used.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-temporal-stability-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    report = main()
    frame_count = sum(len(asset["frame positions xy"]) for asset in report["assets"])
    print(f"Measured palette stability for {len(report['assets'])} CC0 assets across {frame_count} overlapping pan crops.")
