"""Separate hidden-RGB sensitivity from grain placement at alpha cutout edges."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import get_masks  # noqa: E402
from adaptive_palette_alpha_resize_study import BACKGROUNDS, composite  # noqa: E402
from grain_pixelation_alpha_interaction_study import CASES, SEED, SIZE, load_center_crop, pixelate  # noqa: E402
from retro_lab import BAYER_4, ordered_quantize  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-hidden-rgb"
HIDDEN_FILLS = {"black": (0, 0, 0), "magenta": (255, 0, 255), "green": (0, 255, 0)}
ASSET_NAMES = ("clover_full", "silver_feather")


def apply_pipeline(source_rgb8: np.ndarray, alpha8: np.ndarray, scale: float, levels: int,
                   dither: float, scanlines: float, sigma: float,
                   noise: np.ndarray | None, grain_position: str) -> np.ndarray:
    rgb = source_rgb8.astype(np.float32) / 255.0
    if grain_position == "before pixelation" and noise is not None:
        rgb = np.clip(rgb + noise[:, :, None] * sigma, 0.0, 1.0)
    rgb = pixelate(rgb, scale)
    rgb = ordered_quantize(rgb, levels, BAYER_4, dither)
    row_gain = np.where(np.arange(rgb.shape[0])[:, None, None] % 2 == 1, 1.0 - scanlines, 1.0)
    rgb = np.clip(rgb * row_gain, 0.0, 1.0)
    if grain_position == "after quantize" and noise is not None:
        rgb = np.clip(rgb + noise[:, :, None] * sigma, 0.0, 1.0)
    return np.uint8(np.clip(rgb * 255.0 + 0.5, 0, 255))


def make_variant(source: np.ndarray, hidden_fill: tuple[int, int, int]) -> np.ndarray:
    result = source.copy()
    transparent = result[:, :, 3] == 0
    result[transparent, :3] = hidden_fill
    return result


def composite_float(rgb8: np.ndarray, alpha8: np.ndarray, background: tuple[float, float, float]) -> np.ndarray:
    return composite(rgb8, alpha8, background)


def diff_metrics(output_a: np.ndarray, output_b: np.ndarray, alpha: np.ndarray,
                 edge: np.ndarray) -> dict[str, object]:
    result: dict[str, object] = {}
    for bg_name, bg in BACKGROUNDS.items():
        a = composite_float(output_a, alpha, bg)
        b = composite_float(output_b, alpha, bg)
        delta = np.abs(a - b)
        result[bg_name] = {
            "whole composite sRGB-code MAE": float(np.mean(delta)),
            "edge composite sRGB-code MAE": float(np.mean(delta[edge])) if np.any(edge) else 0.0,
            "edge composite P95": float(np.percentile(delta[edge], 95)) if np.any(edge) else 0.0,
            "edge pixels": int(np.count_nonzero(edge)),
        }
    result["RGB8 whole MAE before alpha composite"] = float(np.mean(np.abs(output_a.astype(np.float32) - output_b.astype(np.float32))) / 255.0)
    result["alpha changed ratio"] = 0.0
    return result


def make_sheet(rows: list[dict[str, object]]) -> Path:
    tile_w, tile_h, label_h = 160, 160, 20
    headers = ("clean / black", "clean / magenta", "suite grain / magenta", "pre-resize / magenta", "sensitivity delta ×512")
    sheet = Image.new("RGB", (tile_w * len(headers), label_h + len(rows) * (tile_h + label_h)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for col, title in enumerate(headers):
        draw.text((col * tile_w + 3, 3), title, fill=(245, 245, 245), font=font)
    for row, record in enumerate(rows):
        y = label_h + row * (tile_h + label_h)
        draw.text((3, y + 3), f"{record['asset']} / {record['hidden comparison']}", fill=(245, 245, 245), font=font)
        for col, pixels in enumerate(record["display tiles"]):
            tile = Image.fromarray(pixels, "RGB").resize((tile_w, tile_h), Image.Resampling.NEAREST)
            sheet.paste(tile, (col * tile_w, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-hidden-rgb-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    rng = np.random.default_rng(SEED)
    noise = rng.normal(0.0, 1.0, (SIZE, SIZE)).astype(np.float32)
    asset_info: dict[str, dict[str, object]] = {}
    results: list[dict[str, object]] = []
    visual_rows: list[dict[str, object]] = []
    for asset_name in ASSET_NAMES:
        source, provenance = load_center_crop(asset_name)
        masks = get_masks(source[:, :, 3])
        edge = masks["alpha edge"]
        asset_info[asset_name] = provenance
        variants = {name: make_variant(source, fill) for name, fill in HIDDEN_FILLS.items()}
        for preset, scale, levels, dither, scanlines, sigma in CASES:
            clean = {name: apply_pipeline(image[:, :, :3], source[:, :, 3], scale, levels, dither, scanlines, sigma, None, "none")
                     for name, image in variants.items()}
            post = {name: apply_pipeline(image[:, :, :3], source[:, :, 3], scale, levels, dither, scanlines, sigma, noise, "after quantize")
                    for name, image in variants.items()}
            pre = {name: apply_pipeline(image[:, :, :3], source[:, :, 3], scale, levels, dither, scanlines, sigma, noise, "before pixelation")
                   for name, image in variants.items()}
            for hidden_name in ("magenta", "green"):
                pair_rows = []
                for order_name, outputs in (("clean", clean), ("suite-post-resize-grain", post), ("grain-before-resize", pre)):
                    metrics = diff_metrics(outputs["black"], outputs[hidden_name], source[:, :, 3], edge)
                    pair_rows.append((order_name, metrics))
                    results.append({"asset": asset_name, "preset": preset, "scale": scale,
                                    "grain sigma": sigma, "hidden pair": ["black", hidden_name],
                                    "pipeline": order_name, "metrics": metrics})
                base_metrics = pair_rows[0][1]
                for order_name, metrics in pair_rows[1:]:
                    interaction = {}
                    for bg in BACKGROUNDS:
                        interaction[bg] = {
                            "whole interaction delta": abs(metrics[bg]["whole composite sRGB-code MAE"] - base_metrics[bg]["whole composite sRGB-code MAE"]),
                            "edge interaction delta": abs(metrics[bg]["edge composite sRGB-code MAE"] - base_metrics[bg]["edge composite sRGB-code MAE"]),
                        }
                    results.append({"asset": asset_name, "preset": preset, "scale": scale,
                                    "hidden pair": ["black", hidden_name],
                                    "grain-induced change in hidden-RGB sensitivity": {"pipeline": order_name, "metrics": interaction}})
                if hidden_name == "magenta":
                    alpha = source[:, :, 3]
                    def display(rgb8: np.ndarray) -> np.ndarray:
                        return np.uint8(np.clip(composite(rgb8, alpha, BACKGROUNDS["mid-gray"]) * 255 + 0.5, 0, 255))
                    clean_pair = np.abs(display(clean["black"]).astype(np.float32) - display(clean[hidden_name]).astype(np.float32)) / 255.0
                    post_pair = np.abs(display(post["black"]).astype(np.float32) - display(post[hidden_name]).astype(np.float32)) / 255.0
                    delta = np.clip(np.abs(post_pair - clean_pair) * 512, 0, 1)
                    tiles = [display(clean["black"]), display(clean[hidden_name]), display(post[hidden_name]),
                             display(pre[hidden_name]), np.uint8(np.clip(delta * 255 + 0.5, 0, 255))]
                    visual_rows.append({"asset": asset_name, "hidden comparison": f"black↔magenta / {preset}",
                                        "display tiles": [np.uint8(x) for x in tiles]})
    sheet = make_sheet(visual_rows)
    report = {
        "question": "How much of the apparent grain/pixelation edge change is caused by alpha-zero hidden RGB, and does the grain placement alter that sensitivity?",
        "evaluation plan": {
            "objective": "Hold all visible RGB and alpha values fixed while changing only alpha-zero RGB, then compare hidden-color sensitivity with grain disabled, suite post-resize grain, and grain before resize.",
            "baseline": "Black alpha-zero RGB; comparison fills are magenta and green. These are synthetic controls, not claims about natural source hidden RGB.",
            "pipeline": "BOX/nearest pixelation -> ordered quantization -> scanline -> optional grain. Original alpha retained. Grain uses one shared field and preset sigma; all other settings follow handheld/arcade/print.",
            "assets": "SHA/provenance-verified CC0 clover and silver-feather cutouts, center-cropped/Lanczos-resized 256x256.",
            "metrics": "Pairwise hidden-fill output difference after black/mid-gray/white linear-light compositing; edge MAE/P95 and whole MAE. Grain-induced sensitivity is absolute change in pairwise MAE from clean pipeline. RGB-only effects preserve alpha by construction.",
            "visual review": "Contact sheet compares clean hidden-RGB variants, post-resize grain, pre-resize grain, and amplified difference at nearest enlargement; reviewed by research author.",
            "acceptance": "No approved look or perceptual threshold; descriptive sensitivity only.",
            "unverified": "Real source decoder hidden RGB, alpha association contract, Core/GPU path parity, HDR/ICC, performance, human acceptability, hidden-RGB normalization in product runtime.",
        },
        "hidden RGB fills": {name: list(rgb) for name, rgb in HIDDEN_FILLS.items()},
        "assets": asset_info,
        "results": results,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "Synthetic changes are restricted to exactly alpha=0 pixels; partial alpha and visible pixels are unchanged. Their source origin does not imply production decoder behavior. Interaction values are differences of absolute MAE and discard sign; full signed RGB deltas are not included.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-hidden-rgb-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Separated hidden-RGB sensitivity from grain placement across two alpha assets.")
