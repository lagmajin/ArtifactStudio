"""Compare grain spatial scales after matching a display-domain strength metric."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from grain_resolution_scale_study import (  # noqa: E402
    ASSETS,
    CAPTURE,
    RESOLUTIONS,
    SEED,
    load_asset,
    source_resize,
    make_noise,
    spectral_metrics,
    ratio_hash,
)

TARGET_LINEAR_LUMA_RMS = 0.015
METHODS = ("pixel-fixed", "relative-0.01", "relative-0.02", "relative-0.04")
LUMA = np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)


def srgb_to_linear(values: np.ndarray) -> np.ndarray:
    return np.where(values <= 0.04045, values / 12.92, ((values + 0.055) / 1.055) ** 2.4)


def output_for(source: np.ndarray, unit_noise: np.ndarray, amplitude: float) -> tuple[np.ndarray, float]:
    rgb = source[:, :, :3].astype(np.float32) / 255.0
    raw = rgb + unit_noise[:, :, None] * amplitude
    visible = source[:, :, 3] > 0
    clipped_fraction = float(np.mean(((raw < 0.0) | (raw > 1.0))[visible])) if np.any(visible) else 0.0
    output_rgb = np.uint8(np.clip(raw * 255.0 + 0.5, 0, 255))
    return np.dstack((output_rgb, source[:, :, 3])), clipped_fraction


def composited_linear_luma_rms(source: np.ndarray, output: np.ndarray) -> float:
    # Equal alpha cancels a uniform linear-light background term exactly.
    original = srgb_to_linear(source[:, :, :3].astype(np.float32) / 255.0)
    changed = srgb_to_linear(output[:, :, :3].astype(np.float32) / 255.0)
    alpha = source[:, :, 3].astype(np.float32)[:, :, None] / 255.0
    delta = (changed - original) * alpha
    luma = np.sum(delta * LUMA[None, None, :], axis=2)
    mask = source[:, :, 3] > 0
    return float(np.mean(luma[mask] ** 2) ** 0.5) if np.any(mask) else 0.0


def match_amplitude(source: np.ndarray, unit_noise: np.ndarray) -> tuple[float, np.ndarray, float, float]:
    low, high = 0.0, 1.0
    # Expand the search ceiling if clipping still leaves the target out of reach.
    for _ in range(5):
        candidate, _ = output_for(source, unit_noise, high)
        if composited_linear_luma_rms(source, candidate) >= TARGET_LINEAR_LUMA_RMS:
            break
        high *= 2.0
    for _ in range(22):
        mid = (low + high) * 0.5
        candidate, _ = output_for(source, unit_noise, mid)
        if composited_linear_luma_rms(source, candidate) < TARGET_LINEAR_LUMA_RMS:
            low = mid
        else:
            high = mid
    candidates = []
    for amplitude in (low, high):
        image, clipped = output_for(source, unit_noise, amplitude)
        achieved = composited_linear_luma_rms(source, image)
        candidates.append((abs(achieved - TARGET_LINEAR_LUMA_RMS), amplitude, image, achieved, clipped))
    _, amplitude, image, achieved, clipped = min(candidates, key=lambda item: item[0])
    return amplitude, image, achieved, clipped


def make_sheet(asset: str, rows: list[dict[str, object]]) -> Path:
    tile, label_h = 176, 22
    sheet = Image.new("RGB", (tile * len(RESOLUTIONS), (tile + label_h) * len(rows)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row_index, row in enumerate(rows):
        for col, size in enumerate(RESOLUTIONS):
            x, y = col * tile, row_index * (tile + label_h)
            draw.text((x + 3, y + 3), f"{row['label']} · {size}px", fill=(245, 245, 245), font=font)
            panel = Image.fromarray(row["images"][size], "RGBA").resize((tile, tile), Image.Resampling.NEAREST)
            panel = Image.alpha_composite(Image.new("RGBA", panel.size, (128, 128, 128, 255)), panel)
            sheet.paste(panel.convert("RGB"), (x, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / f"{asset}-grain-matched-strength.png"
    sheet.save(path)
    return path


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(384, h, w)
    crop = rgba[(h - side) // 2:(h + side) // 2, (w - side) // 2:(w + side) // 2]
    rows = [{"label": "source", "images": {}}]
    rows.extend({"label": method, "images": {}} for method in METHODS)
    result = {}
    for size in RESOLUTIONS:
        source = source_resize(crop, size)
        rows[0]["images"][size] = source
        per_method = {}
        for row in rows[1:]:
            method = row["label"]
            base_noise, cell = make_noise(size, method, SEED + ratio_hash(method))
            unit_noise = base_noise / max(float(np.std(base_noise)), 1e-12)
            amplitude, output, achieved, clipped = match_amplitude(source, unit_noise)
            if not np.array_equal(output[:, :, 3], source[:, :, 3]):
                raise ValueError("matched grain changed alpha")
            row["images"][size] = output
            per_method[method] = {
                "nominal cell pixels": cell,
                "matched code-value sigma": amplitude,
                "matched composited linear-luma RMS": achieved,
                "preclip channel sample fraction outside [0,1]": clipped,
                "noise spectrum": spectral_metrics(unit_noise),
                "same RMS for black/gray/white backgrounds": True,
            }
        result[str(size)] = per_method
    sheet = make_sheet(spec[3], rows)
    return {"asset": spec[3], "source": spec[0], "source sha256": digest, "provenance": provenance,
            "crop size": side, "metrics by resolution": result, "contact sheet": str(sheet.relative_to(ROOT))}


def main() -> dict[str, object]:
    report = {
        "question": "When pixel-fixed and image-relative grain are matched to equal composited linear-luma RMS, how much input sigma does each scale need?",
        "method": "Reuses the 256/512/1024 outputs and four grain fields from experiment 090. For each asset/resolution/field, binary-search an additive sRGB-code sigma so the linear-light Rec.709 luma RMS of the source-over difference is 0.015 on visible pixels. Since alpha is unchanged, a uniform linear-light background term cancels exactly; the same luma RMS applies on black/gray/white. Report matched input sigma, visible-only clipping fraction, and spatial spectrum.",
        "limits": "The matched metric is a numeric display-strength proxy, not perceived equivalence or an artistic target. Three still CC0 cutouts, one target value and synthetic mono fields. Noise phase differs by method; temporal behavior and Core/GPU/runtime are not tested.",
        "target mid-gray linear luma RMS": TARGET_LINEAR_LUMA_RMS,
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-equal-display-strength-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Matched display-domain grain strength for {len(result['assets'])} CC0 assets.")
