"""Compare pixel-fixed and image-relative grain across output resolutions."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, load_asset, get_masks  # noqa: E402
from adaptive_palette_alpha_resize_study import composite  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-resolution-scale"
RESOLUTIONS = (256, 512, 1024)
RELATIVE_SIZES = (0.01, 0.02, 0.04)
SIGMA = 0.035
CROP_SIZE = 384
SEED = 9071
BACKGROUNDS = {"black": (0.0, 0.0, 0.0), "gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}


def make_noise(size: int, method: str, seed: int) -> tuple[np.ndarray, float]:
    rng = np.random.default_rng(seed)
    if method == "pixel-fixed":
        noise = rng.normal(0.0, SIGMA, (size, size)).astype(np.float32)
        return noise, 1.0
    ratio = float(method.removeprefix("relative-"))
    cell = max(2, int(round(size * ratio)))
    grid_size = max(2, int(np.ceil(size / cell)))
    low = rng.normal(0.0, 1.0, (grid_size, grid_size)).astype(np.float32)
    noise = np.array(Image.fromarray(low, "F").resize((size, size), Image.Resampling.BILINEAR), dtype=np.float32, copy=True)
    std = float(np.std(noise))
    if std > 0:
        noise *= SIGMA / std
    return noise, cell


def spectral_metrics(noise: np.ndarray) -> dict[str, float]:
    n = noise.shape[0]
    window = np.hanning(n).astype(np.float32)
    field = (noise - float(np.mean(noise))) * np.outer(window, window)
    power = np.abs(np.fft.fftshift(np.fft.fft2(field))) ** 2
    freq = np.fft.fftshift(np.fft.fftfreq(n))
    yy, xx = np.meshgrid(freq, freq, indexing="ij")
    radius = np.sqrt(xx * xx + yy * yy)
    valid = radius > (1.0 / n)
    weights = power[valid]
    radii = radius[valid]
    order = np.argsort(radii)
    cumulative = np.cumsum(weights[order])
    median = float(radii[order[np.searchsorted(cumulative, cumulative[-1] * 0.5)]]) if cumulative[-1] > 0 else 0.0
    total = float(np.sum(power[valid]))
    high = float(np.sum(power[valid & (radius >= 0.2)]) / total) if total > 0 else 0.0
    centered = noise - np.mean(noise)
    denom = float(np.mean(centered * centered))
    corr_x = float(np.mean(centered[:, :-1] * centered[:, 1:]) / denom) if denom > 0 else 0.0
    corr_y = float(np.mean(centered[:-1, :] * centered[1:, :]) / denom) if denom > 0 else 0.0
    return {"noise standard deviation": float(np.std(noise)), "spectral median radius cycles/pixel": median,
            "spectral median radius cycles/image": median * n, "power fraction >=0.2 cycles/pixel": high,
            "adjacent x correlation": corr_x, "adjacent y correlation": corr_y}


def source_resize(rgba8: np.ndarray, size: int) -> np.ndarray:
    image = Image.fromarray(rgba8, "RGBA").resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.uint8)


def output_metrics(source: np.ndarray, output: np.ndarray) -> dict[str, object]:
    source_rgb, source_alpha = source[:, :, :3], source[:, :, 3]
    output_rgb, output_alpha = output[:, :, :3], output[:, :, 3]
    if not np.array_equal(source_alpha, output_alpha):
        raise ValueError("grain changed alpha")
    masks = get_masks(source_alpha)
    s = source_rgb.astype(np.float32) / 255
    o = output_rgb.astype(np.float32) / 255
    delta = o - s
    result: dict[str, object] = {
        "alpha exact": True,
        "RGB residual RMS on visible": float(np.mean(delta[masks["visible"]] ** 2) ** .5) if np.any(masks["visible"]) else 0.0,
    }
    for bg, color in BACKGROUNDS.items():
        a = composite(source_rgb, source_alpha, color)
        b = composite(output_rgb, output_alpha, color)
        for region, mask in masks.items():
            values = b[mask] - a[mask]
            result[f"{bg} composite residual RMS/{region}"] = float(np.mean(values ** 2) ** .5) if values.size else 0.0
    return result


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
    path = CAPTURE / f"{asset}-grain-resolution.png"
    sheet.save(path)
    return path


def study_asset(spec: tuple[str, str, str, str]) -> dict[str, object]:
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    y0, x0 = max(0, (h - CROP_SIZE) // 2), max(0, (w - CROP_SIZE) // 2)
    crop = rgba[y0:y0 + min(CROP_SIZE, h), x0:x0 + min(CROP_SIZE, w)]
    if crop.shape[0] != crop.shape[1]:
        side = min(crop.shape[:2])
        crop = crop[:side, :side]
    rows = [{"label": "source", "images": {}}, {"label": "pixel-fixed", "images": {}}]
    for ratio in RELATIVE_SIZES:
        rows.append({"label": f"relative-{ratio:.02f}", "images": {}})
    metric_by_size = {}
    for size in RESOLUTIONS:
        source = source_resize(crop, size)
        rows[0]["images"][size] = source
        size_data = {}
        for row in rows[1:]:
            method = row["label"]
            noise, nominal_cell = make_noise(size, method, SEED + int(ratio_hash(method)))
            rgb = source[:, :, :3].astype(np.float32) / 255
            output_rgb = np.uint8(np.clip((rgb + noise[:, :, None]) * 255 + .5, 0, 255))
            output = np.dstack((output_rgb, source[:, :, 3]))
            row["images"][size] = output
            size_data[method] = {
                "nominal cell pixels": nominal_cell,
                "grain spectrum": spectral_metrics(noise),
                "source composite error": output_metrics(source, output),
            }
        metric_by_size[str(size)] = size_data
    sheet = make_sheet(spec[3], rows)
    return {"asset": spec[3], "source": spec[0], "source sha256": digest, "provenance": provenance,
            "original dimensions": [w, h], "crop xywh": [x0, y0, crop.shape[1], crop.shape[0]],
            "metrics by resolution": metric_by_size, "contact sheet": str(sheet.relative_to(ROOT))}


def ratio_hash(value: str) -> int:
    return sum((index + 1) * ord(char) for index, char in enumerate(value))


def main() -> dict[str, object]:
    report = {
        "question": "Should grain size be fixed in output pixels or scale with image dimensions when the same crop is delivered at different resolutions?",
        "method": "Three provenance-checked CC0 RGBA assets; center square crops; outputs resized to 256/512/1024 square with Pillow Lanczos. Apply equal target preclip sigma=.035 monochrome additive noise in sRGB code values. Pixel-fixed uses independent per-pixel noise. Relative modes use low-resolution Gaussian fields bilinearly enlarged so cell spacing is 1%, 2%, or 4% of frame width. Measure residual spectrum in cycles/pixel and cycles/image, spatial neighbor correlation, source composite error on black/gray/white, and preserve alpha exactly.",
        "limits": "Synthetic noise model, not a film-emulsion or product grain implementation. Luminance envelope, RGB-colored grain, temporal animation, color profiles, Core/GPU parity and human preference are out of scope. Equal preclip sigma does not imply equal perceived grain.",
        "noise sigma": SIGMA, "seed": SEED, "resolutions": RESOLUTIONS,
        "assets": [study_asset(spec) for spec in ASSETS],
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-resolution-scale-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Measured grain scale behavior across {len(result['assets'])} CC0 assets and {len(result['resolutions'])} output resolutions.")
