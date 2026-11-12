"""Compare straight and premultiplied BOX resize under crop phase changes."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_resize_phase_study import (  # noqa: E402
    CAPTURE as PHASE_CAPTURE,
    EMA,
    FRAME_COUNT,
    PHASES,
    SCALES,
    SOURCE_CROP,
    STRATEGIES,
    pan_origins,
    upscale_nearest,
)
from adaptive_palette_temporal_smoothing_study import align_palette  # noqa: E402
from adaptive_palette_temporal_stability_study import (  # noqa: E402
    ASSETS,
    PALETTE_SIZE,
    assign_palette,
    compare_overlap,
    get_masks,
    load_asset,
    median_cut_palette,
)
from retro_lab import linear_to_srgb, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "adaptive-palette-alpha-resize"
ASSOCIATIONS = ("straight", "premultiplied")
BACKGROUNDS = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
TILE = 112


def box_rgb(crop: np.ndarray, scale: float, association: str) -> tuple[np.ndarray, np.ndarray]:
    h, w = crop.shape[:2]
    size = (max(1, int(round(w * scale))), max(1, int(round(h * scale))))
    alpha = crop[:, :, 3].astype(np.float32) / 255.0
    rgb = crop[:, :, :3].astype(np.float32) / 255.0
    if association == "premultiplied":
        rgb = rgb * alpha[:, :, None]
    rgb8 = np.uint8(np.clip(rgb * 255 + .5, 0, 255))
    low_rgb = np.asarray(Image.fromarray(rgb8, "RGB").resize(size, Image.Resampling.BOX), dtype=np.uint8).astype(np.float32) / 255.0
    low_alpha = np.asarray(Image.fromarray(crop[:, :, 3], "L").resize(size, Image.Resampling.BOX), dtype=np.uint8).astype(np.float32) / 255.0
    if association == "premultiplied":
        low_rgb = np.divide(low_rgb, low_alpha[:, :, None], out=np.zeros_like(low_rgb), where=low_alpha[:, :, None] > 1e-6)
    return np.uint8(np.clip(low_rgb * 255 + .5, 0, 255)), np.uint8(np.clip(low_alpha * 255 + .5, 0, 255))


def composite(rgb8: np.ndarray, alpha8: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    rgb = srgb_to_linear(rgb8.astype(np.float32) / 255.0)
    alpha = alpha8.astype(np.float32)[:, :, None] / 255.0
    bg = srgb_to_linear(np.asarray(background, dtype=np.float32))[None, None, :]
    return linear_to_srgb(rgb * alpha + bg * (1 - alpha))


def paired_metrics(a: np.ndarray, b: np.ndarray, alpha: np.ndarray, background: tuple[float, float, float]) -> dict[str, float]:
    masks = get_masks(alpha)
    ca, cb = composite(a, alpha, background), composite(b, alpha, background)
    result = {}
    for region, mask in masks.items():
        delta = ca[mask] - cb[mask]
        result[region + " composited sRGB RMSE"] = float(np.mean(delta ** 2) ** .5) if delta.size else 0.0
    edge = masks["alpha edge"]
    raw_delta = a[edge].astype(np.float32) - b[edge].astype(np.float32)
    result["alpha-edge RGB8 RMSE"] = float(np.mean(raw_delta ** 2) ** .5) if raw_delta.size else 0.0
    return result


def render(crop: np.ndarray, scale: float, association: str, palette: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    low_rgb, low_alpha = box_rgb(crop, scale, association)
    mapped, _ = assign_palette(low_rgb, palette, "srgb")
    low8 = np.uint8(np.clip(mapped * 255 + .5, 0, 255))
    high = upscale_nearest(low8, SOURCE_CROP)
    return high, crop[:, :, 3], low_alpha


def make_sheet(frames: dict[str, list[dict[str, object]]], asset: str, scale: float, path: Path) -> None:
    phases = list(PHASES)
    keys = [(phase, assoc, "shot-shared") for phase in phases for assoc in ASSOCIATIONS]
    sheet = Image.new("RGB", (FRAME_COUNT * TILE, len(keys) * (TILE + 18)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, (phase, assoc, strategy) in enumerate(keys):
        for col, frame in enumerate(frames[phase]):
            x, y = col * TILE, row * (TILE + 18)
            draw.text((x + 2, y + 2), f"{'L' if phase == PHASES[0] else 'C'}{col:02d} {assoc}", fill=(245, 245, 245), font=font)
            panel = Image.fromarray(frame[assoc][strategy], "RGBA").resize((TILE, TILE), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
            sheet.paste(panel.convert("RGB"), (x, y + 18))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    results = {}
    for scale in SCALES:
        align = 2 if scale == .5 else 4
        locked, cycling = pan_origins(w, SOURCE_CROP, align)
        y0 = max(0, (h - SOURCE_CROP) // 2)
        y0 -= y0 % align
        shared = {}
        for assoc in ASSOCIATIONS:
            low_rgb, low_alpha = box_rgb(rgba, scale, assoc)
            shared[assoc] = median_cut_palette(low_rgb, low_alpha, PALETTE_SIZE, "alpha coverage", "srgb")
        phase_data = {}
        for phase, positions in ((PHASES[0], locked), (PHASES[1], cycling)):
            frames = []
            for x in positions:
                crop = rgba[y0:y0 + SOURCE_CROP, x:x + SOURCE_CROP]
                row = {"x": x, "crop": crop, "palettes": {}, "mapped": {}, "alphas": {}}
                for assoc in ASSOCIATIONS:
                    low_rgb, low_alpha = box_rgb(crop, scale, assoc)
                    row["palettes"][assoc] = median_cut_palette(low_rgb, low_alpha, PALETTE_SIZE, "alpha coverage", "srgb")
                    row["alphas"][assoc] = crop[:, :, 3]
                frames.append(row)
            for assoc in ASSOCIATIONS:
                ema_palettes = [frames[0]["palettes"][assoc]]
                aligned = frames[0]["palettes"][assoc]
                for index in range(1, len(frames)):
                    aligned = align_palette(aligned, frames[index]["palettes"][assoc])
                    ema_palettes.append(ema_palettes[-1] * (1 - EMA) + aligned * EMA)
                for index, row in enumerate(frames):
                    variants = {"per-frame": row["palettes"][assoc], "EMA 0.25": ema_palettes[index], "shot-shared": shared[assoc]}
                    row.setdefault(assoc, {})
                    row[assoc]["low alpha"] = box_rgb(row["crop"], scale, assoc)[1]
                    row[assoc]["mapped"] = {}
                    for strategy, palette in variants.items():
                        output, alpha, _ = render(row["crop"], scale, assoc, palette)
                        if not np.array_equal(alpha, row["crop"][:, :, 3]):
                            raise ValueError("output alpha changed")
                        row[assoc]["mapped"][strategy] = output
            phase_data[phase] = frames

        metrics = {}
        full_alpha = rgba[:, :, 3]
        for phase, positions in ((PHASES[0], locked), (PHASES[1], cycling)):
            rows = phase_data[phase]
            strategy_metrics = {}
            for assoc in ASSOCIATIONS:
                strategy_metrics[assoc] = {}
                for strategy in STRATEGIES:
                    adjacent = []
                    for i, (x0, x1) in enumerate(zip(positions, positions[1:])):
                        ox0, ox1 = max(x0, x1), min(x0 + SOURCE_CROP, x1 + SOURCE_CROP)
                        sl = (slice(None), slice(ox0 - x0, ox1 - x0))
                        sr = (slice(None), slice(ox0 - x1, ox1 - x1))
                        alpha = full_alpha[y0:y0 + SOURCE_CROP, ox0:ox1]
                        adjacent.append(compare_overlap(rows[i][assoc]["mapped"][strategy][sl], rows[i + 1][assoc]["mapped"][strategy][sr], alpha, get_masks(alpha)))
                    strategy_metrics[assoc][strategy] = {
                        "mean same-world changed visible ratio": float(np.mean([x["visible"]["changed visible pixel ratio"] for x in adjacent])),
                        "mean same-world visible RGB8 RMSE": float(np.mean([x["visible"]["uncomposited RGB8 RMSE"] for x in adjacent])),
                        "mean same-world alpha-edge RGB8 RMSE": float(np.mean([x["alpha edge"]["uncomposited RGB8 RMSE"] for x in adjacent])),
                    }
            paired = {}
            for strategy in STRATEGIES:
                a = rows[0]["straight"]["mapped"][strategy]
                b = rows[0]["premultiplied"]["mapped"][strategy]
                alpha = rows[0]["crop"][:, :, 3]
                paired[strategy] = {bg: paired_metrics(a, b, alpha, color) for bg, color in BACKGROUNDS.items()}
            metrics[phase] = {"temporal by association": strategy_metrics, "straight vs premultiplied on first crop": paired}
        sheet = CAPTURE / f"{spec[3]}-{scale:.2f}-alpha-phase.png"
        visual_rows = {phase: [{"index": i, **{assoc: {"shot-shared": np.dstack((row[assoc]["mapped"]["shot-shared"], row["crop"][:, :, 3]))} for assoc in ASSOCIATIONS}} for i, row in enumerate(phase_data[phase])] for phase in PHASES}
        make_sheet(visual_rows, spec[3], scale, sheet)
        results[f"{scale:.2f}"] = {"phase locked x": locked, "phase cycling x": cycling, "y": y0, "metrics": metrics, "contact sheet": str(sheet.relative_to(ROOT))}
    return {"asset": spec[3], "source": spec[0], "sha256": digest, "provenance": provenance, "dimensions": [w, h], "scales": results}


def main() -> dict[str, object]:
    report = {
        "question": "How does straight versus premultiplied BOX downsampling affect palette output at transparent edges and temporal stability under locked/cycling crop phase?",
        "method": "Three SHA-verified CC0 RGBA sources; 384px crops, nine pan positions, BOX downscale at .5/.75, 8-color alpha-weighted sRGB median-cut palette, nearest upscale, original full-resolution alpha restored. Premultiplied condition filters RGB*alpha and alpha separately, then unassociates low-resolution RGB before palette training/mapping. Compare per-frame, aligned EMA .25 and full-source shot-shared palettes. Temporal metrics compare identical world pixels. Association difference is also measured on first crop after linear-light source-over against black, gray and white.",
        "limits": "Python research model, not Artifact runtime or Core implementation. Pillow BOX input is quantized back to uint8 between premultiply and filtering. Low-res alpha is used for palette training only; final alpha is original. No ICC transforms, moving content, human quality ratings, GPU parity or performance measurements.",
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "adaptive-palette-alpha-resize-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Compared straight/premultiplied resize on {len(result['assets'])} assets.")
