"""Evaluate full-suite alpha-edge response across source/output resolutions."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, get_masks, load_asset  # noqa: E402
from adaptive_palette_alpha_resize_study import BACKGROUNDS, composite  # noqa: E402
from retro_lab import PRESETS, apply_retro  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-alpha-resolution"
SOURCE_SIZE = 512
SIZES = (128, 256, 512, 1024)
ALPHA_POLICIES = ("straight", "premultiplied")
ASSET_NAMES = ("clover_full", "silver_feather", "pixel_hair_atlas")
SEED = 81327


def load_source(name: str) -> tuple[np.ndarray, dict[str, object]]:
    spec = next(spec for spec in ASSETS if spec[3] == name)
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(h, w)
    x0, y0 = (w - side) // 2, (h - side) // 2
    crop = Image.fromarray(rgba[y0:y0 + side, x0:x0 + side], "RGBA").resize(
        (SOURCE_SIZE, SOURCE_SIZE), Image.Resampling.LANCZOS)
    return np.asarray(crop, dtype=np.uint8), {
        "source": spec[0], "sha256": digest, "provenance": provenance,
        "original dimensions": [w, h], "center crop": [x0, y0, side, side],
        "standardized source crop": [SOURCE_SIZE, SOURCE_SIZE],
    }


def resize_plane(values: np.ndarray, size: int) -> np.ndarray:
    image = Image.fromarray(values.astype(np.float32), "F")
    return np.asarray(image.resize((size, size), Image.Resampling.LANCZOS), dtype=np.float32)


def resize_rgba(source: np.ndarray, size: int, association: str) -> np.ndarray:
    rgba = source.astype(np.float32) / 255.0
    alpha = rgba[:, :, 3]
    rgb = rgba[:, :, :3]
    if association == "premultiplied":
        rgb = rgb * alpha[:, :, None]
    filtered = np.stack([resize_plane(rgb[:, :, channel], size) for channel in range(3)], axis=2)
    resized_alpha = resize_plane(alpha, size)
    resized_alpha = np.clip(resized_alpha, 0.0, 1.0)
    if association == "premultiplied":
        filtered = np.divide(filtered, resized_alpha[:, :, None], out=np.zeros_like(filtered), where=resized_alpha[:, :, None] > 1e-6)
    out = np.concatenate((np.clip(filtered, 0.0, 1.0), resized_alpha[:, :, None]), axis=2)
    return np.uint8(np.clip(out * 255.0 + 0.5, 0, 255))


def edge_regions(alpha: np.ndarray) -> dict[str, np.ndarray]:
    return get_masks(alpha)


def composite_error(source_rgba: np.ndarray, output_rgba: np.ndarray) -> dict[str, object]:
    source_alpha = source_rgba[:, :, 3]
    output_alpha = output_rgba[:, :, 3]
    regions = edge_regions(source_alpha)
    metrics: dict[str, object] = {
        "alpha changed ratio": float(np.mean(output_alpha != source_alpha)),
        "alpha changed pixels": int(np.count_nonzero(output_alpha != source_alpha)),
        "region pixels": {name: int(np.count_nonzero(mask)) for name, mask in regions.items()},
    }
    for bg_name, bg in BACKGROUNDS.items():
        baseline = composite(source_rgba[:, :, :3], source_alpha, bg)
        candidate = composite(output_rgba[:, :, :3], output_alpha, bg)
        delta = np.abs(candidate - baseline)
        metrics[bg_name] = {
            "whole sRGB-code MAE": float(np.mean(delta)),
            "whole sRGB-code P95": float(np.percentile(delta, 95)),
            "regions": {
                name: {
                    "sRGB-code MAE": float(np.mean(delta[mask])) if np.any(mask) else 0.0,
                    "sRGB-code P95": float(np.percentile(delta[mask], 95)) if np.any(mask) else 0.0,
                    "max channel error": float(np.max(delta[mask])) if np.any(mask) else 0.0,
                }
                for name, mask in regions.items()
            },
        }
    return metrics


def compare_policy_outputs(a: np.ndarray, b: np.ndarray, alpha: np.ndarray) -> dict[str, object]:
    regions = edge_regions(alpha)
    result: dict[str, object] = {}
    for bg_name, bg in BACKGROUNDS.items():
        comp_a = composite(a[:, :, :3], a[:, :, 3], bg)
        comp_b = composite(b[:, :, :3], b[:, :, 3], bg)
        delta = np.abs(comp_a - comp_b)
        result[bg_name] = {
            "edge sRGB-code MAE": float(np.mean(delta[regions["alpha edge"]])) if np.any(regions["alpha edge"]) else 0.0,
            "partial-alpha sRGB-code MAE": float(np.mean(delta[regions["partial alpha"]])) if np.any(regions["partial alpha"]) else 0.0,
            "whole sRGB-code MAE": float(np.mean(delta)),
        }
    return result


def displayed(rgb8: np.ndarray, alpha8: np.ndarray, size: int = 256) -> np.ndarray:
    comp = composite(rgb8, alpha8, BACKGROUNDS["mid-gray"])
    code = np.uint8(np.clip(comp * 255.0 + 0.5, 0, 255))
    return np.asarray(Image.fromarray(code, "RGB").resize((size, size), Image.Resampling.LANCZOS), dtype=np.uint8)


def save_resolution_sheet(records: list[dict[str, object]]) -> Path:
    tile, label, cols = 160, 18, 4
    sheet = Image.new("RGB", (cols * tile, len(records) * (tile + label)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, item in enumerate(records):
        y = row * (tile + label)
        draw.text((3, y + 3), f"{item['asset']} / {item['preset']} / straight source resize", fill=(245, 245, 245), font=font)
        for col, size in enumerate(SIZES):
            thumb = Image.fromarray(item["views"][str(size)], "RGB").resize((tile, tile), Image.Resampling.NEAREST)
            sheet.paste(thumb, (col * tile, y + label))
            draw.text((col * tile + 3, y + label + 2), str(size), fill=(255, 255, 255), font=font)
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "full-suite-alpha-resolution-sheet.png"
    sheet.save(path)
    return path


def save_policy_sheet(records: list[dict[str, object]]) -> Path:
    tile, label, cols = 160, 18, 4
    sheet = Image.new("RGB", (cols * tile, len(records) * (tile + label)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    titles = ("input / mid-gray", "suite straight", "suite premultiplied", "policy delta ×8")
    for col, title in enumerate(titles):
        draw.text((col * tile + 3, 3), title, fill=(245, 245, 245), font=font)
    for row, item in enumerate(records):
        y = label + row * (tile + label)
        draw.text((3, y + 3), f"{item['asset']} / {item['preset']} / 256px", fill=(245, 245, 245), font=font)
        for col, pixels in enumerate(item["tiles"]):
            sheet.paste(Image.fromarray(pixels, "RGB").resize((tile, tile), Image.Resampling.NEAREST), (col * tile, y + label))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "full-suite-alpha-policy-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    sources = {name: load_source(name) for name in ASSET_NAMES}
    results: list[dict[str, object]] = []
    resolution_sheet_records: list[dict[str, object]] = []
    policy_sheet_records: list[dict[str, object]] = []
    for asset_name, (source, provenance) in sources.items():
        for preset_name, preset in PRESETS.items():
            view_rows: dict[str, dict[str, np.ndarray]] = {policy: {} for policy in ALPHA_POLICIES}
            native_outputs: dict[tuple[int, str], np.ndarray] = {}
            for size in SIZES:
                resized_by_policy = {policy: resize_rgba(source, size, policy) for policy in ALPHA_POLICIES}
                policy_metrics: dict[str, object] = {}
                for policy in ALPHA_POLICIES:
                    resized = resized_by_policy[policy]
                    image = Image.fromarray(resized, "RGBA")
                    output = np.asarray(apply_retro(image, **preset, seed=SEED, alpha_resize=policy), dtype=np.uint8)
                    native_outputs[(size, policy)] = output
                    base_metrics = composite_error(resized, output)
                    view_rows[policy][str(size)] = displayed(output[:, :, :3], output[:, :, 3])
                    policy_metrics[policy] = {
                        "source resize vs other alpha policy": None,
                        "suite output vs resized source": base_metrics,
                    }
                pair_metrics = compare_policy_outputs(
                    native_outputs[(size, "straight")], native_outputs[(size, "premultiplied")],
                    resized_by_policy["straight"][:, :, 3])
                policy_metrics["straight vs premultiplied suite output"] = pair_metrics
                source_pair = compare_policy_outputs(
                    resized_by_policy["straight"], resized_by_policy["premultiplied"],
                    resized_by_policy["straight"][:, :, 3])
                policy_metrics["straight vs premultiplied resized input"] = source_pair
                for policy in ALPHA_POLICIES:
                    out_alpha = native_outputs[(size, policy)][:, :, 3]
                    policy_metrics[policy]["suite alpha exact to resized input"] = bool(np.array_equal(out_alpha, resized_by_policy[policy][:, :, 3]))
                results.append({"asset": asset_name, "source identity": provenance,
                                "preset": preset_name, "preset settings": preset,
                                "output size": [size, size], "input resize policies": policy_metrics,
                                "alpha edge pixel count straight": int(np.count_nonzero(edge_regions(resized_by_policy["straight"][:, :, 3])["alpha edge"]))})
            resolution_sheet_records.append({"asset": asset_name, "preset": preset_name,
                                             "views": view_rows["straight"]})
            size = 256
            source_resized = resize_rgba(source, size, "straight")
            straight = native_outputs[(size, "straight")]
            premul = native_outputs[(size, "premultiplied")]
            base = displayed(source_resized[:, :, :3], source_resized[:, :, 3])
            straight_view = displayed(straight[:, :, :3], straight[:, :, 3])
            premul_view = displayed(premul[:, :, :3], premul[:, :, 3])
            delta = np.uint8(np.clip(np.abs(straight_view.astype(np.float32) - premul_view.astype(np.float32)) * 8, 0, 255))
            policy_sheet_records.append({"asset": asset_name, "preset": preset_name,
                                         "tiles": [base, straight_view, premul_view, delta]})
    resolution_sheet = save_resolution_sheet(resolution_sheet_records)
    policy_sheet = save_policy_sheet(policy_sheet_records)
    report = {
        "question": "How do output resolution and source alpha-resize association affect full-suite edge composites?",
        "evaluation plan": {
            "objective": "Extend the opaque full-suite resolution sweep to natural alpha cutouts and explicitly compare straight vs premultiplied source resize and suite-internal pixelation policy.",
            "baseline": "At each native size, the association-resized RGBA input itself is the no-suite reference. Outputs are composited on the same black/mid-gray/white background before error measurement.",
            "assets": "SHA/provenance verified CC0 clover, silver-feather, and pixel-hair cutouts; center square crop standardized to 512x512.",
            "conditions": "Native dimensions 128/256/512/1024; all current PRESETS; seed 81327. Source resize filters RGB and alpha separately for straight; for premultiplied it filters RGB*alpha and alpha, then unassociates. The same policy is passed to apply_retro internal pixelation. RGB values use sRGB code-value filtering; compositing uses the lab's linear-light helper.",
            "metrics": "Native same-size baseline error whole/visible/partial/alpha-edge under black/mid-gray/white; alpha code equality; straight-vs-premultiplied source and output pair difference; sheet composites normalized to 256 only for visual comparison.",
            "visual review": "Full-resolution sheet: 3 assets × 4 presets × four native output sizes normalized to 256, straight policy; policy sheet: 256 output input/straight/premultiplied/difference. Researcher reviewed thumbnails and alpha-sensitive cases.",
            "acceptance": "No approved edge target or threshold; no pass/fail quality claim.",
            "unverified": "Actual Artifact source decode alpha descriptor/hidden RGB, GPU/Core parity, ICC/HDR, display pipeline, performance/memory, human preference, preview/export route.",
        },
        "sources": {name: provenance for name, (_, provenance) in sources.items()},
        "sizes": list(SIZES),
        "results": results,
        "resolution sheet": str(resolution_sheet.relative_to(ROOT)),
        "alpha policy sheet": str(policy_sheet.relative_to(ROOT)),
        "limits": "Per-resolution source input is resampled from the same standardized crop; comparisons isolate suite changes relative to that resampled input, while source resize policy comparison includes resampling itself. Straight/premultiplied code-value filtering is a lab model, not a claim about application decoder contracts.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "full-suite-alpha-resolution-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Measured full-suite alpha-edge response across three assets and four resolutions.")
