#!/usr/bin/env python3
"""Measure Film Grunge deterministic hash-grain changes over Composition frames."""
from __future__ import annotations

import hashlib
import json
import math
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/grunge-grain-temporal-field-correlation-r1.json"
SOURCE = ROOT / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/grunge-grain-temporal-field-correlation-r1"
WIDTH = HEIGHT = 28
FRAME_COUNT = 4096
FRAME_SEED = 1977
AMOUNT = np.float32(.075)
GRAIN_RGB = np.asarray((.92, 1.0, 1.05), np.float32)
GRAIN_SIZES = (1.0, 1.5, 3.0, 8.0)
LAGS = (1, 2, 3, 12, 24, 60, 120, 240, 480, 960, 2048)
SAMPLE_FRAMES = (0, 1, 2, 3, 12, 24, 60, 120)
RANDOM_ACCESS = (2048, 37, 4095, 0, 37, 2048)
MASK32 = 0xFFFFFFFF


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def frame_seed(frame: int) -> np.uint32:
    value = (np.uint64(FRAME_SEED) * np.uint64(1664525) +
             np.uint64(frame) * np.uint64(1013904223)) & np.uint64(MASK32)
    return np.uint32(value)


def float32_grain(hash_values: np.ndarray) -> np.ndarray:
    low16 = (hash_values & np.uint32(0xFFFF)).astype(np.float32)
    normalized = np.float32(low16 / np.float32(32767.5))
    centered = np.float32(normalized - np.float32(1.0))
    return np.float32(centered * AMOUNT)


def field_for_frame(frame: int, grain_block: int) -> np.ndarray:
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.int32)
    x_index, y_index = xx // grain_block, yy // grain_block
    x_hash = (x_index * np.int32(73856093)).astype(np.uint32)
    y_hash = (y_index * np.int32(19349663)).astype(np.uint32)
    block_hash = np.uint32(frame_seed(frame)) ^ x_hash ^ y_hash
    return float32_grain(block_hash)


def tone_chart() -> tuple[np.ndarray, dict[str, np.ndarray]]:
    source = np.empty((HEIGHT, WIDTH), np.float32)
    source[:, :9] = np.float32(.10)
    source[:, 9:18] = np.float32(.50)
    source[:, 18:] = np.float32(.90)
    regions = {"all": np.ones((HEIGHT, WIDTH), bool),
               "shadow": np.zeros((HEIGHT, WIDTH), bool),
               "midtone": np.zeros((HEIGHT, WIDTH), bool),
               "highlight": np.zeros((HEIGHT, WIDTH), bool)}
    regions["shadow"][:, :9] = True
    regions["midtone"][:, 9:18] = True
    regions["highlight"][:, 18:] = True
    return source, regions


def stats(values: np.ndarray) -> dict:
    x = np.asarray(values, np.float64).reshape(-1)
    if x.size == 0:
        return {"count": 0, "mean": 0.0, "std": 0.0, "rms": 0.0,
                "min": 0.0, "max": 0.0, "p95_abs": 0.0}
    return {"count": int(x.size), "mean": float(x.mean()), "std": float(x.std()),
            "rms": float(np.sqrt(np.mean(x * x))), "min": float(x.min()),
            "max": float(x.max()), "p95_abs": float(np.quantile(np.abs(x), .95))}


def centered_temporal_correlation(fields: np.ndarray, lag: int) -> float:
    centered = fields.astype(np.float64) - fields.astype(np.float64).mean(axis=0, keepdims=True)
    a, b = centered[:-lag], centered[lag:]
    denominator = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
    return float(np.sum(a * b) / denominator) if denominator else 0.0


def contact_sheet(grain_block: int, fields_by_frame: dict[int, np.ndarray], source: np.ndarray) -> Path:
    columns, tile, label_h = 4, 128, 24
    rows = math.ceil(len(SAMPLE_FRAMES) / columns)
    sheet = Image.new("RGB", (columns * tile, rows * (tile + label_h)), (26, 27, 30))
    draw = ImageDraw.Draw(sheet)
    for index, frame in enumerate(SAMPLE_FRAMES):
        output = np.float32(source[..., None] + fields_by_frame[frame][..., None] * GRAIN_RGB)
        rgb8 = np.rint(np.clip(output, 0, 1) * 255).astype(np.uint8)
        image = Image.fromarray(rgb8, "RGB")
        image = image.resize((tile, tile), Image.Resampling.NEAREST)
        x, y = (index % columns) * tile, (index // columns) * (tile + label_h)
        sheet.paste(image, (x, y))
        draw.text((x + 4, y + tile + 4), f"frame {frame} / block {grain_block}", fill=(235, 235, 235))
    path = OUT / f"contact_grainBlock-{grain_block}.png"
    sheet.save(path)
    return path


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    source_text = SOURCE.read_text(encoding="utf-8")
    markers = {
        "frame_seed_equation": "static_cast<std::uint64_t>(impl_->seed) * 1664525ull" in source_text and "static_cast<std::uint64_t>(impl_->frame) * 1013904223ull" in source_text,
        "grain_block_round": "std::round(impl_->grainSize)" in source_text,
        "grain_hash_x_factor": "(x / grainBlock) * 73856093" in source_text,
        "grain_hash_y_factor": "(y / grainBlock) * 19349663" in source_text,
        "grain_low16_and_scale": "blockHash & 0xffffu" in source_text and "32767.5f - 1.0f" in source_text,
        "grain_adds_after_weave": "shiftedRow[x][channel] * exposure * vignetteFactor * stainWave +" in source_text and "chromaGrain + damage" in source_text,
        "channel_grain_multipliers": "channel == 2 ? 1.05f : channel == 0 ? 0.92f : 1.0f" in source_text,
    }
    source, regions = tone_chart()
    max_x_product = (WIDTH - 1) * 73856093
    max_y_product = (HEIGHT - 1) * 19349663
    assert max_x_product <= 0x7FFFFFFF and max_y_product <= 0x7FFFFFFF
    frames = np.arange(FRAME_COUNT, dtype=np.uint64)
    frame_seeds = ((np.uint64(FRAME_SEED) * np.uint64(1664525) +
                    frames * np.uint64(1013904223)) & np.uint64(MASK32)).astype(np.uint32)
    residual_unique_per_pixel_period = int(np.unique(float32_grain(
        (np.arange(65536, dtype=np.uint32)[:, None] ^ np.uint32(0)))).size)
    low16_full_cycle = ((np.uint64(FRAME_SEED * 1664525) +
                         np.arange(65536, dtype=np.uint64) * np.uint64(1013904223)) & np.uint64(0xFFFF)).astype(np.uint16)
    low16_period_evidence = {
        "increment_mod_65536": int(1013904223 % 65536),
        "gcd_increment_modulus": math.gcd(1013904223, 65536),
        "unique_low16_values_in_65536_frames": int(np.unique(low16_full_cycle).size),
        "unique_grain_residual_levels": residual_unique_per_pixel_period,
        "period_frames": 65536 if np.unique(low16_full_cycle).size == 65536 else None,
    }
    low16_sequential = (frame_seeds & np.uint32(0xFFFF)).astype(np.uint16)
    every_adjacent_low16_differs = bool(np.all(low16_sequential[1:] != low16_sequential[:-1]))
    cases, previews = [], []
    for grain_size in GRAIN_SIZES:
        grain_block = int(math.floor(grain_size + .5))  # positive std::round behavior
        xx = np.arange(WIDTH, dtype=np.int32)[None, :] // grain_block
        yy = np.arange(HEIGHT, dtype=np.int32)[:, None] // grain_block
        x_hash = (xx * np.int32(73856093)).astype(np.uint32)
        y_hash = (yy * np.int32(19349663)).astype(np.uint32)
        frame_hashes = frame_seeds[:, None, None] ^ x_hash[None, :, :] ^ y_hash[None, :, :]
        fields = float32_grain(frame_hashes)
        channel_fields = np.float32(fields[..., None] * GRAIN_RGB)
        sample_fields = {frame: fields[frame].copy() for frame in SAMPLE_FRAMES}
        zero_amount_identity = bool(np.array_equal(
            np.float32(source + field_for_frame(0, grain_block) * np.float32(0.0)), source))
        spatial_block_checks = []
        for frame in (0, 1, 37, 2048, 4095):
            field = fields[frame]
            exact = True
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    x0, y0 = (x // grain_block) * grain_block, (y // grain_block) * grain_block
                    x1, y1 = min(WIDTH, x0 + grain_block), min(HEIGHT, y0 + grain_block)
                    if not np.all(field[y0:y1, x0:x1] == field[y, x]):
                        exact = False
                        break
                if not exact:
                    break
            spatial_block_checks.append({"frame": frame, "exact_within_block": exact})
        frame_means = fields.mean(axis=(1, 2), dtype=np.float64)
        frame_stds = fields.std(axis=(1, 2), dtype=np.float64)
        frame_deltas = np.diff(fields, axis=0)
        pixel_temporal_means = fields.mean(axis=0, dtype=np.float64)
        pixel_temporal_stds = fields.std(axis=0, dtype=np.float64)
        unique_pixel_frames = np.unique(fields.reshape(FRAME_COUNT, -1), axis=0).shape[0]
        lag_correlations = {str(lag): centered_temporal_correlation(fields, lag) for lag in LAGS}
        random_fields = {frame: field_for_frame(frame, grain_block) for frame in set(RANDOM_ACCESS)}
        random_access_order = [random_fields[frame] for frame in RANDOM_ACCESS]
        repeated_37_exact = bool(np.array_equal(random_access_order[1], random_access_order[4]))
        repeated_2048_exact = bool(np.array_equal(random_access_order[0], random_access_order[5]))
        sequential_model_matches_access = bool(all(np.array_equal(random_fields[f], fields[f]) for f in random_fields if f < FRAME_COUNT))
        record = {
            "grain_size_requested": grain_size,
            "grain_block": grain_block,
            "coordinate_hash_products_safe_int32": max_x_product <= 0x7FFFFFFF and max_y_product <= 0x7FFFFFFF,
            "max_coordinate_products": {"x": max_x_product, "y": max_y_product},
            "grain_amount_zero_identity": zero_amount_identity,
            "fields_sha256": sha(fields.tobytes()),
            "channel_fields_sha256": sha(channel_fields.tobytes()),
            "frame_seed": {"first": int(frame_seeds[0]), "last": int(frame_seeds[-1]),
                           "unique_in_4096": int(np.unique(frame_seeds).size)},
            "temporal": {
                "frame_mean_stats": stats(frame_means),
                "frame_std_stats": stats(frame_stds),
                "per_pixel_time_mean_stats": stats(pixel_temporal_means),
                "per_pixel_time_std_stats": stats(pixel_temporal_stds),
                "frame_to_frame_residual_delta": stats(frame_deltas),
                "fraction_of_changed_pixels_each_transition": float(np.mean(frame_deltas != 0)),
                "unique_full_fields_in_4096_frames": int(unique_pixel_frames),
                "lag_correlation": lag_correlations,
                "random_access_sequence": RANDOM_ACCESS,
                "repeat_frame_37_exact": repeated_37_exact,
                "repeat_frame_2048_exact": repeated_2048_exact,
                "random_access_matches_sequential_model": sequential_model_matches_access,
            },
            "spatial_block_checks": spatial_block_checks,
            "channel_grain_scale": {"rgb_component_0": .92, "rgb_component_1": 1.0, "rgb_component_2": 1.05},
            "channel_temporal_residual": {
                "component_0_scale_0_92": stats(channel_fields[..., 0]),
                "component_1_scale_1_00": stats(channel_fields[..., 1]),
                "component_2_scale_1_05": stats(channel_fields[..., 2]),
                "cochannel_correlation": {
                    "component_0_vs_1": float(np.corrcoef(channel_fields[..., 0].reshape(-1), channel_fields[..., 1].reshape(-1))[0, 1]),
                    "component_1_vs_2": float(np.corrcoef(channel_fields[..., 1].reshape(-1), channel_fields[..., 2].reshape(-1))[0, 1]),
                    "component_0_vs_2": float(np.corrcoef(channel_fields[..., 0].reshape(-1), channel_fields[..., 2].reshape(-1))[0, 1]),
                },
            },
            "tone_regions": {},
        }
        for region_name, mask in regions.items():
            region_fields = fields[:, mask]
            record["tone_regions"][region_name] = {
                "pixels_per_frame": int(mask.sum()),
                "residual_distribution_over_all_frames": stats(region_fields),
                "frame_delta_rmse": float(np.sqrt(np.mean(np.diff(region_fields, axis=0).astype(np.float64) ** 2))),
            }
        cases.append(record)
        previews.append(contact_sheet(grain_block, sample_fields, source))

    metrics = {
        "study": plan["plan_id"],
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "pillow": Image.__version__},
        "input_sha256": {"plan": sha(PLAN.read_bytes()), "source": sha(SOURCE.read_bytes()),
                         "runner": sha(Path(__file__).read_bytes()), "tone_fixture": sha(source.tobytes())},
        "source_markers": markers,
        "low16_period_evidence": low16_period_evidence,
        "every_adjacent_frame_low16_seed_differs": every_adjacent_low16_differs,
        "conditions": {"dimensions_wh": [WIDTH, HEIGHT], "frame_indices": [0, FRAME_COUNT - 1],
                       "grain_amount": float(AMOUNT), "seed": FRAME_SEED,
                       "grain_sizes": GRAIN_SIZES, "grain_blocks": [1, 2, 3, 8], "lags": LAGS,
                       "channel_grain_scales_rgb": GRAIN_RGB.tolist()},
        "preview_contact_sheets": [str(path.relative_to(ROOT)).replace("\\", "/") for path in previews],
        "cases": cases,
        "limitations": plan["scope"]["excluded"],
    }
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    checksums = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path.read_bytes())
                 for path in sorted(OUT.rglob("*")) if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(cases), "source_markers": markers,
                      "low16_period_evidence": low16_period_evidence,
                      "every_adjacent_frame_low16_seed_differs": every_adjacent_low16_differs,
                      "all_adjacent_pixels_changed": all(c["temporal"]["fraction_of_changed_pixels_each_transition"] == 1.0 for c in cases),
                      "output": str(OUT)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
