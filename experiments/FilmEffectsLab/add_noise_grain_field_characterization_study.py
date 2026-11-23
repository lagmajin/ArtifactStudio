#!/usr/bin/env python3
"""Characterize Add Noise's hash field and bounded tone response offline."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/add-noise-grain-field-characterization-r1.json"
SOURCE = ROOT / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
INTERFACE = ROOT / "Artifact/include/Effects/AddNoise/AddNoiseEffect.ixx"
OUT = ROOT / "experiments/FilmEffectsLab/captures/add-noise-grain-field-characterization-r1"
SIZES = (.5, 1.0, 1.5, 3.0, 8.0)
AMOUNTS = (.1, .4)
MODES = ("color", "mono", "luma")
SEEDS = (0, 73, 1977, 4099)
RESOLUTIONS = ((512, 256), (1024, 512))
TONE_VALUES = (.02, .10, .50, .90, .98)
F32, U32 = np.float32, np.uint32


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def hash_values(x: np.ndarray, y: np.ndarray, channel: int, seed: int) -> np.ndarray:
    """Independent uint32 mirror of AddNoiseEffectCPUImpl::noiseAt."""
    with np.errstate(over="ignore"):
        value = (x.astype(U32) * U32(73856093) ^ y.astype(U32) * U32(19349663) ^
                 U32(channel) * U32(83492791) ^ U32(seed))
        value ^= value >> U32(16)
        value *= U32(0x7FEB352D)
        value ^= value >> U32(15)
        value *= U32(0x846CA68B)
        value ^= value >> U32(16)
    return ((value & U32(0x00FFFFFF)).astype(F32) / F32(16777215.0) * F32(2.0) - F32(1.0))


def fields(width: int, height: int, size: float, seed: int) -> dict[str, np.ndarray]:
    sx = np.floor(np.arange(width, dtype=F32) / F32(max(size, .1))).astype(U32)[None, :]
    sy = np.floor(np.arange(height, dtype=F32) / F32(max(size, .1))).astype(U32)[:, None]
    return {"r": hash_values(sx, sy, 1, seed),
            "g": hash_values(sx, sy, 2, seed),
            "b": hash_values(sx, sy, 3, seed)}


def corr_lag(a: np.ndarray, axis: int, lag: int) -> float:
    if axis == 1:
        left, right = a[:, :-lag], a[:, lag:]
    else:
        left, right = a[:-lag, :], a[lag:, :]
    left = left.astype(np.float64) - float(a.mean())
    right = right.astype(np.float64) - float(a.mean())
    denom = float(np.sqrt(np.mean(left * left) * np.mean(right * right)))
    return float(np.mean(left * right) / denom) if denom else 0.0


def spatial_metrics(a: np.ndarray) -> dict:
    x = a.astype(np.float64)
    centered = x - x.mean()
    power = np.abs(np.fft.rfft2(centered)) ** 2
    h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    radius = np.sqrt(fx * fx + fy * fy)
    total = float(power.sum()) or 1.0
    low = float(power[radius < .125].sum() / total)
    high = float(power[(radius >= .25) & (radius <= .5)].sum() / total)
    horizontal = float(power[np.abs(fx) > 2 * np.abs(fy)].sum())
    vertical = float(power[np.abs(fy) > 2 * np.abs(fx)].sum())
    anisotropy = horizontal / vertical if vertical else 0.0
    return {"acf_horizontal": {str(k): corr_lag(a, 1, k) for k in (1, 2, 4, 8, 16)},
            "acf_vertical": {str(k): corr_lag(a, 0, k) for k in (1, 2, 4, 8, 16)},
            "psd_low_energy_fraction_below_0_125_cyc_px": low,
            "psd_high_energy_fraction_0_25_to_0_5_cyc_px": high,
            "psd_horizontal_to_vertical_axis_energy_ratio": anisotropy}


def cell_runs(width: int, size: float) -> list[int]:
    ids = np.floor(np.arange(width, dtype=F32) / F32(max(size, .1))).astype(np.int32)
    starts = np.r_[0, np.flatnonzero(ids[1:] != ids[:-1]) + 1, width]
    return np.diff(starts).astype(int).tolist()


def flat_fixture(width: int, height: int) -> tuple[np.ndarray, dict[str, np.ndarray]]:
    gutters = 4
    panel_width = (width - gutters * (len(TONE_VALUES) + 1)) // len(TONE_VALUES)
    image = np.zeros((height, width, 4), np.float32)
    image[..., 3] = 1.0
    masks = {}
    x = gutters
    for i, value in enumerate(TONE_VALUES):
        end = x + panel_width
        image[:, x:end, :3] = F32(value)
        mask = np.zeros((height, width), bool)
        mask[:, x:end] = True
        label = ("shadow_002" if i == 0 else "shadow_010" if i == 1 else
                 "midtone_050" if i == 2 else "highlight_090" if i == 3 else "highlight_098")
        masks[label] = mask
        x = end + gutters
    masks["whole"] = np.logical_or.reduce(list(masks.values()))
    return image, masks


def visual_fixture(width: int, height: int) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    u, v = xx / (width - 1), yy / (height - 1)
    base = np.stack((.02 + .96 * u, .05 + .90 * u, .10 + .85 * u), axis=-1)
    for i, color in enumerate(((.04, .05, .07), (.12, .14, .18), (.5, .5, .5),
                               (.9, .9, .9), (.98, .98, .98))):
        lo, hi = int(i * width / 5), int((i + 1) * width / 5)
        base[:, lo:hi] = color
    checker = ((xx.astype(np.int32) // max(2, width // 128) +
                yy.astype(np.int32) // max(2, height // 64)) % 2) == 0
    base[checker & (u > .62) & (u < .96)] = (.1, .75, .92)
    base[~checker & (u > .62) & (u < .96)] = (.9, .16, .65)
    for col in range(width // 10, width, max(4, width // 32)):
        base[:, col:col + max(1, width // 256)] = 1.0
    for row in range(height // 8, height, max(4, height // 16)):
        base[row:row + max(1, height // 256), :] = .02
    bgra = np.empty((height, width, 4), np.float32)
    bgra[..., :3] = base[..., ::-1]
    bgra[..., 3] = 1.0
    return bgra


def render(source: np.ndarray, field: dict[str, np.ndarray], amount: float, mode: str) -> np.ndarray:
    out = source.copy()
    # CPU storage is BGRA; mode-specific hash channels follow the source equations.
    if mode == "color":
        out[..., 0] = np.clip(source[..., 0] + field["b"] * amount, 0, 1)
        out[..., 1] = np.clip(source[..., 1] + field["g"] * amount, 0, 1)
        out[..., 2] = np.clip(source[..., 2] + field["r"] * amount, 0, 1)
    elif mode == "mono":
        n = hash_values_from_field(field, "r") * F32(amount)
        out[..., :3] = np.clip(source[..., :3] + n[..., None], 0, 1)
    else:
        n = hash_values_from_field(field, "r") * F32(amount)
        b, g, r = source[..., 0], source[..., 1], source[..., 2]
        lum = b * F32(.114) + g * F32(.587) + r * F32(.299)
        new_lum = np.clip(lum + n, 0, 1)
        diff = new_lum - lum
        out[..., 0] = np.clip(b + diff, 0, 1)
        out[..., 1] = np.clip(g + diff, 0, 1)
        out[..., 2] = np.clip(r + diff, 0, 1)
    return out


def hash_values_from_field(field: dict[str, np.ndarray], key: str) -> np.ndarray:
    # The color-R salt is the luminance/monochrome key used by the CPU formula.
    return field[key]


def summarize(values: np.ndarray) -> dict:
    x = values.astype(np.float64).reshape(-1)
    return {"mean": float(x.mean()), "std": float(x.std()),
            "p01": float(np.quantile(x, .01)), "p50": float(np.quantile(x, .50)),
            "p99": float(np.quantile(x, .99)), "clip_at_0_fraction": float(np.mean(x <= 0)),
            "clip_at_1_fraction": float(np.mean(x >= 1)),
            "within_1e_6_of_0_fraction": float(np.mean(x <= 1e-6)),
            "within_1e_6_of_1_fraction": float(np.mean(x >= 1 - 1e-6)),
            "clip_fraction": float(np.mean((x <= 0) | (x >= 1)))}


def save_png(path: Path, bgra: np.ndarray) -> None:
    rgb8 = np.rint(np.clip(bgra[..., :3][..., ::-1], 0, 1) * 255).astype(np.uint8)
    cv2.imwrite(str(path), rgb8[..., ::-1])


def make_contact(paths: list[tuple[str, Path]], output: Path) -> None:
    thumbs = []
    for label, path in paths:
        im = Image.open(path).convert("RGB")
        im.thumbnail((384, 192))
        tile = Image.new("RGB", (400, 220), (38, 38, 38))
        tile.paste(im, ((400 - im.width) // 2, 22))
        ImageDraw.Draw(tile).text((8, 4), label, fill=(245, 245, 245))
        thumbs.append(tile)
    cols = 4
    sheet = Image.new("RGB", (cols * 400, ((len(thumbs) + cols - 1) // cols) * 220), (24, 24, 24))
    for i, tile in enumerate(thumbs):
        sheet.paste(tile, ((i % cols) * 400, (i // cols) * 220))
    sheet.save(output)


def main() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    source_text = SOURCE.read_text(encoding="utf-8")
    interface_text = INTERFACE.read_text(encoding="utf-8")
    markers = {
        "numeric_seed_conversion": "seed = static_cast<uint32_t>(seed_)" in source_text,
        "floor_coordinates": "std::floor(static_cast<float>(x) / size_)" in source_text,
        "hash_constants": all(s in source_text for s in ("73856093u", "19349663u", "83492791u", "0x7feb352du", "0x846ca68bu")),
        "rgb_channel_clamp": "std::clamp(p[0] + noiseAt" in source_text and "std::clamp(p[2] + noiseAt" in source_text,
        "mode_precedence": "if (monochrome_)" in source_text and "else if (colorNoise_)" in source_text,
        "resident_size_scaled": "node.resolutionScaledParameterMask = (1u << 1)" in interface_text,
        "frame_not_in_hash_key": "x * 73856093u ^ y * 19349663u" in source_text,
    }
    if not all(markers.values()):
        raise RuntimeError(f"Source marker mismatch: {markers}")

    report = {"plan_id": "add-noise-grain-field-characterization-r1",
              "plan_sha256": sha(PLAN), "script_sha256": sha(Path(__file__)),
              "source_sha256": sha(SOURCE), "interface_sha256": sha(INTERFACE),
              "python": platform.python_version(), "numpy": np.__version__, "opencv": cv2.__version__,
              "dimensions": {}, "cross_size_same_seed_pattern": {},
              "cross_resolution_coordinate_overlap": {}, "source_markers_verified": markers,
              "limits": "Synthetic CPU-equation mirror only; no actual app, compiled GPU, user image, alpha edge, or aesthetic approval.",
              "seed_set": list(SEEDS), "sizes": list(SIZES), "amounts": list(AMOUNTS), "modes": list(MODES)}
    visual_outputs = []

    for width, height in RESOLUTIONS:
        flat, regions = flat_fixture(width, height)
        visual = visual_fixture(width, height)
        dim_key = f"{width}x{height}"
        dim = {"pixel_cell_mapping": {}, "field_metrics": {}, "tone_mode_response": {},
               "cross_seed_correlation": {}, "region_pixel_counts": {k: int(v.sum()) for k, v in regions.items()}}
        for size in SIZES:
            size_key = str(size)
            prior_fields = {}
            dim["pixel_cell_mapping"][size_key] = {
                "horizontal_run_lengths_first_64_pixels": cell_runs(min(width, 64), size),
                "unique_x_cells": int(np.unique(np.floor(np.arange(width, dtype=F32) / F32(size))).size),
                "unique_y_cells": int(np.unique(np.floor(np.arange(height, dtype=F32) / F32(size))).size)}
            for seed in SEEDS:
                field = fields(width, height, size, seed)
                seed_key = str(seed)
                dim["field_metrics"].setdefault(size_key, {})[seed_key] = {}
                for channel in ("r", "g", "b"):
                    f = field[channel]
                    stat = spatial_metrics(f)
                    stat.update({"mean": float(f.mean()), "std": float(f.std()),
                                 "p01": float(np.quantile(f, .01)), "p50": float(np.quantile(f, .50)),
                                 "p99": float(np.quantile(f, .99)),
                                 "unique_hash_cell_values": int(np.unique(f[::max(1, int(size)), ::max(1, int(size))]).size)})
                    dim["field_metrics"][size_key][seed_key][channel] = stat
                # Cross-channel residual correlation at a neutral midtone is not clipped.
                residuals = np.stack([field[k] for k in ("r", "g", "b")], axis=-1).astype(np.float64)
                dim["field_metrics"][size_key][seed_key]["cross_channel_correlation"] = np.corrcoef(residuals.reshape(-1, 3), rowvar=False).tolist()
                for other_seed, other in prior_fields.items():
                    if other_seed == seed_key:
                        continue
                    pair = f"{other_seed}_vs_{seed_key}"
                    dim["cross_seed_correlation"].setdefault(size_key, {})[pair] = {
                        "r": float(np.corrcoef(other.reshape(-1), field["r"].reshape(-1))[0, 1])}
                prior_fields[seed_key] = field["r"].copy()

                for amount in AMOUNTS:
                    for mode in MODES:
                        candidate = render(flat, field, amount, mode)
                        key = f"{size_key}|{amount}|{mode}|{seed_key}"
                        tone_record = {}
                        for region_name, mask in regions.items():
                            region_stats = summarize(candidate[mask, :3])
                            if mode == "luma":
                                src = flat[mask]
                                lum = src[:, 0] * F32(.114) + src[:, 1] * F32(.587) + src[:, 2] * F32(.299)
                                preclamp = lum + field["r"][mask] * F32(amount)
                                region_stats["luma_intermediate_at_0_fraction"] = float(np.mean(preclamp <= 0))
                                region_stats["luma_intermediate_at_1_fraction"] = float(np.mean(preclamp >= 1))
                            else:
                                region_stats["luma_intermediate_at_0_fraction"] = None
                                region_stats["luma_intermediate_at_1_fraction"] = None
                            tone_record[region_name] = region_stats
                        dim["tone_mode_response"][key] = tone_record

                if seed == 73:
                    if size in (.5, 1.5, 3.0, 8.0):
                        out = render(visual, field, .4, "color")
                        path = OUT / f"color-seed73-amount0.4-size{size}-{dim_key}.png"
                        save_png(path, out)
                        visual_outputs.append((f"{dim_key} color size {size}", path))
                    if size == 1.5:
                        for mode in ("mono", "luma"):
                            out = render(visual, field, .4, mode)
                            path = OUT / f"{mode}-seed73-amount0.4-size1.5-{dim_key}.png"
                            save_png(path, out)
                            visual_outputs.append((f"{dim_key} {mode} size 1.5", path))
            # Cross-seed correlations above compare the same hash channel and Size.
        source_path = OUT / f"input-{dim_key}.png"
        save_png(source_path, visual)
        visual_outputs.append((f"{dim_key} input", source_path))
        report["dimensions"][dim_key] = dim

        # Measure whether each Size redraws the field and changes its spatial scale.
        size_fields = {str(size): fields(width, height, size, 73) for size in SIZES}
        for i, size_a in enumerate(SIZES):
            for size_b in SIZES[i + 1:]:
                key = f"{size_a}_vs_{size_b}"
                report["cross_size_same_seed_pattern"].setdefault(dim_key, {})[key] = {}
                for channel in ("r", "g", "b"):
                    a = size_fields[str(size_a)][channel].astype(np.float64)
                    b = size_fields[str(size_b)][channel].astype(np.float64)
                    report["cross_size_same_seed_pattern"][dim_key][key][channel] = {
                        "correlation": float(np.corrcoef(a.reshape(-1), b.reshape(-1))[0, 1]),
                        "rmse": float(np.sqrt(np.mean((a - b) ** 2)))}
        del size_fields

    for size in SIZES:
        size_key = str(size)
        report["cross_resolution_coordinate_overlap"][size_key] = {}
        for seed in SEEDS:
            small = fields(512, 256, size, seed)
            large = fields(1024, 512, size, seed)
            report["cross_resolution_coordinate_overlap"][size_key][str(seed)] = {}
            for channel in ("r", "g", "b"):
                a = small[channel].astype(np.float64)
                b = large[channel][:256, :512].astype(np.float64)
                diff = a - b
                report["cross_resolution_coordinate_overlap"][size_key][str(seed)][channel] = {
                    "exact_channel_fraction": float(np.mean(diff == 0)),
                    "rmse": float(np.sqrt(np.mean(diff * diff))),
                    "max_abs_difference": float(np.max(np.abs(diff)))}

    make_contact(visual_outputs, OUT / "contact-sheet.png")
    report["preview_sha256"] = {p.name: sha(p) for _, p in visual_outputs}
    report["preview_sha256"]["contact-sheet.png"] = sha(OUT / "contact-sheet.png")
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    (OUT / "metrics.sha256").write_text(f"{sha(metrics_path)}  metrics.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"metrics": str(metrics_path), "plan_sha256": report["plan_sha256"],
                      "script_sha256": report["script_sha256"], "markers": markers,
                      "preview_count": len(visual_outputs) + 1}, indent=2))
    return report


if __name__ == "__main__":
    main()
