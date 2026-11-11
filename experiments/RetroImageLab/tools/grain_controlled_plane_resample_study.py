"""Measure resampling response on controlled grain planes in linear and sRGB domains."""

from __future__ import annotations

import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = ROOT / "captures" / "grain-controlled-plane-resample"
SIZES = (1024, 2048)
DISPLAY_EDGE = 512
FILTERS = {
    "box": Image.Resampling.BOX,
    "bilinear": Image.Resampling.BILINEAR,
    "bicubic": Image.Resampling.BICUBIC,
    "lanczos": Image.Resampling.LANCZOS,
}
BASE_LEVELS = (0.18, 0.50, 0.82)
SIGMAS = (0.01, 0.05)
SEEDS = tuple(range(7331, 7339))
SELECTED_SEED = SEEDS[0]
DOMAINS = ("linear float", "sRGB float", "sRGB RGB8")


def linear_to_srgb(value: np.ndarray) -> np.ndarray:
    value = np.maximum(value, 0.0)
    return np.where(value <= 0.0031308, value * 12.92,
                    1.055 * np.power(value, 1.0 / 2.4) - 0.055)


def srgb_to_linear(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.04045, value / 12.92,
                    np.power((value + 0.055) / 1.055, 2.4))


def resize_linear_float(field: np.ndarray, output_size: tuple[int, int], resampler: int) -> np.ndarray:
    return np.asarray(Image.fromarray(field.astype(np.float32), mode="F").resize(output_size, resampler),
                      dtype=np.float32)


def resize_srgb_float(field_linear: np.ndarray, output_size: tuple[int, int], resampler: int) -> tuple[np.ndarray, np.ndarray]:
    raw_clipped = np.clip(field_linear, 0.0, 1.0)
    encoded = np.clip(linear_to_srgb(raw_clipped), 0.0, 1.0).astype(np.float32)
    resized = resize_linear_float(encoded, output_size, resampler)
    return srgb_to_linear(np.clip(resized, 0.0, 1.0)), raw_clipped


def resize_encoded_rgb8(field_linear: np.ndarray, output_size: tuple[int, int], resampler: int) -> tuple[np.ndarray, np.ndarray]:
    raw_clipped = np.clip(field_linear, 0.0, 1.0)
    encoded = np.clip(linear_to_srgb(raw_clipped), 0.0, 1.0)
    rgb8 = np.uint8(encoded * 255.0 + 0.5)
    image = Image.fromarray(np.repeat(rgb8[:, :, None], 3, axis=2), mode="RGB")
    resized = np.asarray(image.resize(output_size, resampler), dtype=np.float32) / 255.0
    return srgb_to_linear(resized[:, :, 0]), raw_clipped


def metrics(residual: np.ndarray, clipped_input: np.ndarray, sigma: float) -> dict[str, float]:
    return {
        "linear luma residual RMS": float(np.sqrt(np.mean(residual * residual))),
        "linear luma residual P95 absolute": float(np.percentile(np.abs(residual), 95)),
        "linear luma residual signed mean": float(np.mean(residual)),
        "output/input sigma ratio": float(np.sqrt(np.mean(residual * residual)) / sigma),
        "input low-clamp pixel ratio": float(np.mean(clipped_input <= 0.0)),
        "input high-clamp pixel ratio": float(np.mean(clipped_input >= 1.0)),
    }


def heatmap(residual: np.ndarray, gain: float = 20.0) -> np.ndarray:
    pos = np.clip(residual * gain, 0.0, 1.0)
    neg = np.clip(-residual * gain, 0.0, 1.0)
    mid = np.maximum(pos, neg) * 0.12
    return np.uint8(np.clip(np.stack((pos, mid, neg), axis=2) * 255.0 + .5, 0, 255))


def main() -> dict[str, object]:
    records: list[dict[str, object]] = []
    columns = [(size, name, domain) for size in SIZES for name in FILTERS for domain in DOMAINS]
    tile_w, tile_h, label_h = 128, 92, 24
    rows = len(BASE_LEVELS) * len(SIGMAS)
    sheet = Image.new("RGB", (len(columns) * tile_w, rows * (tile_h + label_h)), (27, 28, 31))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    output_dims_for_size: dict[int, tuple[int, int]] = {}

    for level_index, base in enumerate(BASE_LEVELS):
        for sigma_index, sigma in enumerate(SIGMAS):
            row = level_index * len(SIGMAS) + sigma_index
            for size in SIZES:
                height = max(1, round(size * 0.75))
                input_dims = (size, height)
                output_dims = (DISPLAY_EDGE, max(1, round(height * DISPLAY_EDGE / size)))
                output_dims_for_size[size] = output_dims
                for seed in SEEDS:
                    rng = np.random.default_rng(seed)
                    noise = rng.standard_normal((height, size), dtype=np.float32)
                    plane = np.float32(base) + np.float32(sigma) * noise
                    clipped = np.clip(plane, 0.0, 1.0)
                    for filter_name, filter_value in FILTERS.items():
                        # Same seed-specific noise field is used for every filter and transfer path.
                        linear_result = resize_linear_float(plane, output_dims, filter_value)
                        srgb_float_result, srgb_float_clipped = resize_srgb_float(plane, output_dims, filter_value)
                        srgb_result, encoded_clipped = resize_encoded_rgb8(plane, output_dims, filter_value)
                        linear_residual = linear_result - np.float32(base)
                        srgb_float_residual = srgb_float_result - np.float32(base)
                        base_code = np.float32(linear_to_srgb(np.asarray(base, dtype=np.float32)))
                        encoded_base = np.float32(srgb_to_linear(
                            np.float32(np.floor(base_code * 255.0 + 0.5) / 255.0)))
                        encoded_residual = srgb_result - encoded_base
                        common = {"input size": size, "input dimensions": list(input_dims),
                                  "display dimensions": list(output_dims), "filter": filter_name,
                                  "base linear level": base, "input grain sigma": sigma, "seed": seed}
                        records.append({**common, "domain": "linear float",
                                        "metrics": metrics(linear_residual, clipped, sigma)})
                        records.append({**common, "domain": "sRGB float",
                                        "metrics": metrics(srgb_float_residual, srgb_float_clipped, sigma)})
                        records.append({**common, "domain": "sRGB RGB8",
                                        "metrics": metrics(encoded_residual, encoded_clipped, sigma)})

                        if seed == SELECTED_SEED:
                            for domain_index, residual in enumerate((linear_residual, srgb_float_residual,
                                                                      encoded_residual)):
                                column = next(i for i, c in enumerate(columns)
                                              if c == (size, filter_name, DOMAINS[domain_index]))
                                x, y = column * tile_w, row * (tile_h + label_h)
                                draw.text((x + 2, y + 3),
                                          f"{size} {filter_name} {DOMAINS[domain_index]}"[:23],
                                          fill=(245, 245, 245), font=font)
                                tile = Image.fromarray(heatmap(residual), mode="RGB")
                                tile.thumbnail((tile_w, tile_h), Image.Resampling.NEAREST)
                                sheet.paste(tile, (x + (tile_w - tile.width) // 2,
                                                   y + label_h + (tile_h - tile.height) // 2))

    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet_path = CAPTURE / "controlled-noise-residual-heatmaps.png"
    sheet.save(sheet_path)
    # Group records by condition; report median and seed spread for each domain.
    summary: dict[str, object] = {}
    for size in SIZES:
        summary[str(size)] = {}
        for base in BASE_LEVELS:
            summary[str(size)][str(base)] = {}
            for sigma in SIGMAS:
                summary[str(size)][str(base)][str(sigma)] = {}
                for filter_name in FILTERS:
                    summary[str(size)][str(base)][str(sigma)][filter_name] = {}
                    for domain in DOMAINS:
                        selected = [r["metrics"]["linear luma residual RMS"] for r in records
                                    if r["input size"] == size and r["base linear level"] == base
                                    and r["input grain sigma"] == sigma and r["filter"] == filter_name
                                    and r["domain"] == domain]
                        ratios = [r["metrics"]["output/input sigma ratio"] for r in records
                                  if r["input size"] == size and r["base linear level"] == base
                                  and r["input grain sigma"] == sigma and r["filter"] == filter_name
                                  and r["domain"] == domain]
                        summary[str(size)][str(base)][str(sigma)][filter_name][domain] = {
                            "residual RMS median": float(np.median(selected)),
                            "residual RMS min/max": [float(np.min(selected)), float(np.max(selected))],
                            "output/input sigma ratio median": float(np.median(ratios)),
                        }

    report = {
        "question": "How do the resize kernel and transfer/quantization domain affect controlled additive white-grain response?",
        "evaluation plan": {
            "fixture": "Synthetic uniform linear-light grayscale plane plus iid Gaussian noise; no retro-suite stages, source texture, or spatial tone variation.",
            "conditions": f"Long edges {list(SIZES)} reduced to 512; aspect ratio 4:3; base linear levels {list(BASE_LEVELS)}; sigmas {list(SIGMAS)}; filters {list(FILTERS)}; seeds {list(SEEDS)}.",
            "paths": "Linear float resamples without clipping or transfer encoding. sRGB float clamps input, applies the sRGB transfer, resamples float encoded values, then decodes. sRGB RGB8 adds 8-bit code quantization before encoded-value resampling and decode.",
            "pairing": "Same seed-specific noise plane is shared across filters and all paths. Residual uses the matching grain-free constant base, including its RGB8-quantized code for the 8-bit path.",
            "metrics": "Linear residual RMS, P95 absolute, signed mean, RMS/input sigma ratio, and pre-resampling input clamp occupancy.",
            "visuals": "Selected seed 7331 residual heatmaps with fixed ×20 gain, red positive and blue negative; display visualization only.",
            "alpha and temporal": "Not applicable: grayscale opaque static synthetic planes; no motion or alpha variation.",
            "decision": "Controlled model diagnostic; no acceptance threshold, quality ranking, or product recommendation.",
            "unverified": "Pillow equivalence to application/GPU filters, scene content interaction, perceptual preference, HDR, temporal behavior, and performance.",
        },
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "seeds": list(SEEDS), "records": records, "summary": summary,
        "visual": str(sheet_path.relative_to(ROOT)),
        "limits": "Iid Gaussian static plane isolates filter averaging and transfer/8-bit effects but is not a retro image or target appearance. sRGB path clamps before transfer; clipping occupancy is reported from the linear input plane. The linear float path intentionally has no clamp so kernel response remains separately measurable.",
    }
    (CAPTURE / "controlled-noise-resample-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Measured {len(records)} controlled resampling conditions.")
    return report


if __name__ == "__main__":
    main()
