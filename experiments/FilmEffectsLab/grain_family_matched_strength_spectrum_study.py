from __future__ import annotations

import hashlib
import json
import math
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
PLAN = ROOT / "plans/grain-family-matched-strength-spectrum-r1.json"
ADD_SOURCE = REPO / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
GRUNGE_SOURCE = REPO / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
OUT = ROOT / "captures/grain-family-matched-strength-spectrum-r1"
OUT.mkdir(parents=True, exist_ok=True)
F32 = np.float32
U32_MASK = 0xFFFFFFFF
SEEDS = tuple(range(128))
SIZES = (1.0, 1.5, 2.0, 3.0, 5.0, 8.0)
TONE_LEVELS = (0.02, 0.10, 0.50, 0.90, 0.98)
WIDTH = HEIGHT = 28
GRAIN_DEFAULT = F32(0.075)
FRAME = 0
DISPLAY_SEED = 73

ADD_TEXT = ADD_SOURCE.read_text(encoding="utf-8")
GRUNGE_TEXT = GRUNGE_SOURCE.read_text(encoding="utf-8")
MARKERS = {
    "grunge_frame_seed": "1664525ull" in GRUNGE_TEXT and "1013904223ull" in GRUNGE_TEXT,
    "grunge_positive_round_block": "static_cast<int>(std::round(impl_->grainSize))" in GRUNGE_TEXT,
    "grunge_hash_constants": "73856093" in GRUNGE_TEXT and "19349663" in GRUNGE_TEXT,
    "grunge_low16_mapping": "blockHash & 0xffffu" in GRUNGE_TEXT and "32767.5f - 1.0f" in GRUNGE_TEXT,
    "grunge_channel_scale": "channel == 2 ? 1.05f : channel == 0 ? 0.92f" in GRUNGE_TEXT,
    "grunge_other_controls_zeroable": "scratchAmount = 0.22f" in GRUNGE_TEXT and "vignette = 0.18f" in GRUNGE_TEXT,
    "addnoise_floor_sample": "std::floor(static_cast<float>(x) / size_)" in ADD_TEXT and "std::floor(static_cast<float>(y) / size_)" in ADD_TEXT,
    "addnoise_hash_constants": "0x7feb352du" in ADD_TEXT and "0x846ca68bu" in ADD_TEXT,
    "addnoise_low24_mapping": "0x00ffffffu" in ADD_TEXT and "16777215.0f" in ADD_TEXT,
    "addnoise_channel_salts": "noiseAt(ux, uy, 3u)" in ADD_TEXT and "noiseAt(ux, uy, 2u)" in ADD_TEXT,
    "addnoise_rgb_clamp": "std::clamp(p[0] + noiseAt" in ADD_TEXT and "std::clamp(p[2] + noiseAt" in ADD_TEXT,
    "addnoise_cpu_preserves_alpha": "dst = src;" in ADD_TEXT and "p[3]" not in ADD_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def u32(value: int) -> int:
    return value & U32_MASK


def add_hash(x: int, y: int, channel: int, seed: int) -> int:
    value = u32(u32(x * 73856093) ^ u32(y * 19349663) ^
                u32(channel * 83492791) ^ u32(seed))
    value ^= value >> 16
    value = u32(value * 0x7FEB352D)
    value ^= value >> 15
    value = u32(value * 0x846CA68B)
    value ^= value >> 16
    return u32(value)


def add_noise_unit(seed: int, size: float, mode: str) -> np.ndarray:
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    sx = np.floor(xx / max(float(size), 0.1)).astype(np.int64)
    sy = np.floor(yy / max(float(size), 0.1)).astype(np.int64)
    field = np.zeros((HEIGHT, WIDTH, 3), dtype=F32)
    salts = (1, 1, 1) if mode == "monochrome" else (3, 2, 1)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            for channel in range(3):
                value = add_hash(int(sx[y, x]), int(sy[y, x]), salts[channel], seed)
                noise = F32(F32(value & 0x00FFFFFF) / F32(16777215.0) * F32(2.0) - F32(1.0))
                field[y, x, channel] = noise
    return field


def grunge_unit(seed: int, size: float) -> np.ndarray:
    grain_block = max(1, int(math.floor(float(size) + 0.5)))
    frame_seed = u32(seed * 1664525 + FRAME * 1013904223)
    field = np.zeros((HEIGHT, WIDTH, 3), dtype=F32)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            value = u32(frame_seed ^
                        u32((x // grain_block) * 73856093) ^
                        u32((y // grain_block) * 19349663))
            grain = F32((F32(value & 0xFFFF) / F32(32767.5) - F32(1.0)))
            field[y, x, 0] = grain * F32(0.92)
            field[y, x, 1] = grain
            field[y, x, 2] = grain * F32(1.05)
    return field


def center_each_seed(fields: np.ndarray) -> np.ndarray:
    return fields - fields.mean(axis=(1, 2), keepdims=True, dtype=np.float64).astype(F32)


def pooled_std(fields: np.ndarray) -> float:
    centered = center_each_seed(fields.astype(F32))
    return float(np.sqrt(np.mean(centered.astype(np.float64) ** 2)))


def corr(a: np.ndarray, b: np.ndarray) -> float:
    av = a.astype(np.float64).ravel()
    bv = b.astype(np.float64).ravel()
    av -= av.mean()
    bv -= bv.mean()
    denominator = math.sqrt(float(np.mean(av * av) * np.mean(bv * bv)))
    return float(np.mean(av * bv) / denominator) if denominator else 0.0


def spatial_metrics(fields: np.ndarray) -> dict:
    centered = center_each_seed(fields.astype(F32)).astype(np.float64)
    green = centered[..., 1]
    variance_by_seed = np.mean(green * green, axis=(1, 2))
    total_power = np.zeros((HEIGHT, WIDTH // 2 + 1), dtype=np.float64)
    for plane in green:
        spectrum = np.fft.rfft2(plane)
        total_power += np.abs(spectrum) ** 2 / float(WIDTH * HEIGHT)
    total_power /= len(green)
    fy = np.fft.fftfreq(HEIGHT)[:, None]
    fx = np.fft.rfftfreq(WIDTH)[None, :]
    radial = np.sqrt(fx * fx + fy * fy)
    valid = radial > 0
    power_total = float(total_power[valid].sum())

    def spectral_fraction(mask: np.ndarray) -> float:
        return float(total_power[mask].sum() / power_total) if power_total else 0.0

    lags = {}
    for lag in (1, 2, 4, 8):
        vx0 = centered[:, :, :-lag, 1]
        vx1 = centered[:, :, lag:, 1]
        vy0 = centered[:, :-lag, :, 1]
        vy1 = centered[:, lag:, :, 1]
        lags[str(lag)] = {
            "x": float(np.mean(vx0 * vx1) / max(float(variance_by_seed.mean()), 1e-30)),
            "y": float(np.mean(vy0 * vy1) / max(float(variance_by_seed.mean()), 1e-30)),
        }
    adjacent_x = green[:, :, :-1] == green[:, :, 1:]
    adjacent_y = green[:, :-1, :] == green[:, 1:, :]
    channels = centered.reshape(len(centered), -1, 3).transpose(2, 0, 1).reshape(3, -1)
    channel_corr = np.corrcoef(channels)
    grunge_or_addnoise_zero_values = fields == 0
    return {
        "pooled_green_residual_std": float(np.sqrt(variance_by_seed.mean())),
        "green_std_by_seed_p10_p50_p90": [float(x) for x in np.quantile(np.sqrt(variance_by_seed), [0.10, 0.50, 0.90])],
        "green_mean_by_seed_pooled": float(fields[..., 1].mean()),
        "rgb_channel_std": [float(fields[..., c].std(dtype=np.float64)) for c in range(3)],
        "rgb_channel_correlation": channel_corr.tolist(),
        "lag_correlation_xy": lags,
        "identical_neighbor_fraction_xy": [float(adjacent_x.mean()), float(adjacent_y.mean())],
        "green_unique_level_count": int(np.unique(fields[..., 1]).size),
        "green_zero_fraction": float(grunge_or_addnoise_zero_values[..., 1].mean()),
        "power_fraction_below_0p125_cycles_per_pixel": spectral_fraction(valid & (radial < 0.125)),
        "power_fraction_0p125_to_0p25": spectral_fraction((radial >= 0.125) & (radial < 0.25)),
        "power_fraction_above_0p25": spectral_fraction(radial >= 0.25),
        "power_total": power_total,
    }


def run_lengths(field: np.ndarray) -> dict:
    values = field[..., 1]

    def lengths(sequences: np.ndarray) -> list[int]:
        result = []
        for row in sequences:
            count = 1
            for prev, curr in zip(row, row[1:]):
                if prev == curr:
                    count += 1
                else:
                    result.append(count)
                    count = 1
            result.append(count)
        return result

    x_runs = lengths(values)
    y_runs = lengths(values.T)
    return {
        "mean_run_length_x": float(np.mean(x_runs)),
        "mean_run_length_y": float(np.mean(y_runs)),
        "max_run_length_x": max(x_runs),
        "max_run_length_y": max(y_runs),
    }


def clip_stats(outputs: np.ndarray) -> dict:
    values = outputs.astype(np.float64)
    return {
        "rgb_min": [float(values[..., c].min()) for c in range(3)],
        "rgb_max": [float(values[..., c].max()) for c in range(3)],
        "below_zero_fraction": float(np.mean(values < 0)),
        "above_one_fraction": float(np.mean(values > 1)),
        "exact_endpoint_fraction": float(np.mean((values == 0) | (values == 1))),
        "within_1e_6_endpoint_fraction": float(np.mean((values <= 1e-6) | (values >= 1 - 1e-6))),
        "nonfinite_count": int(np.count_nonzero(~np.isfinite(values))),
    }


# Build unit-amount seed ensembles for all matched sizes.
unit_fields = {}
for size in SIZES:
    unit_fields[("grunge", size, "native")] = np.stack([grunge_unit(seed, size) for seed in SEEDS])
    for mode in ("monochrome", "color"):
        unit_fields[("addnoise", size, mode)] = np.stack([add_noise_unit(seed, size, mode) for seed in SEEDS])

calibrations = {}
records = []
tone_records = []
display_fields = {}

for size in SIZES:
    grunge_unit_fields = unit_fields[("grunge", size, "native")]
    target_std = pooled_std(grunge_unit_fields[..., 1] * GRAIN_DEFAULT)
    grunge_fields = grunge_unit_fields * GRAIN_DEFAULT
    grunge_stats = spatial_metrics(grunge_fields)
    grunge_stats["run_lengths"] = run_lengths(grunge_fields[SEEDS.index(DISPLAY_SEED)])
    grunge_stats["effective_grain_block"] = max(1, int(math.floor(size + 0.5)))
    grunge_stats["amount"] = float(GRAIN_DEFAULT)
    records.append({"effect": "film_grunge", "mode": "shared_grain", "size": size,
                    "target_green_std": target_std, "calibration_amount": float(GRAIN_DEFAULT),
                    "stats": grunge_stats})

    for mode in ("monochrome", "color"):
        add_unit = unit_fields[("addnoise", size, mode)]
        add_unit_std = pooled_std(add_unit[..., 1])
        amount = target_std / add_unit_std if add_unit_std else 0.0
        if not 0.0 <= amount <= 1.0:
            raise RuntimeError(f"calibrated Add Noise Amount outside source range: {size=} {mode=} {amount=}")
        add_fields = add_unit * F32(amount)
        add_stats = spatial_metrics(add_fields)
        add_stats["run_lengths"] = run_lengths(add_fields[SEEDS.index(DISPLAY_SEED)])
        add_stats["sample_mapping"] = "floor(x/size), floor(y/size)"
        add_stats["amount"] = amount
        match_error = abs(add_stats["pooled_green_residual_std"] - target_std)
        if match_error > 1e-6:
            raise RuntimeError(f"matched-strength tolerance failed: {size=} {mode=} {match_error=}")
        calibrations[f"{size:g}:{mode}"] = {
            "film_grunge_default_amount": float(GRAIN_DEFAULT),
            "film_grunge_target_green_std": target_std,
            "addnoise_calibrated_amount": amount,
            "addnoise_achieved_green_std": add_stats["pooled_green_residual_std"],
            "absolute_match_error": match_error,
        }
        records.append({"effect": "add_noise", "mode": mode, "size": size,
                        "target_green_std": target_std, "calibration_amount": amount,
                        "stats": add_stats})

        if size == 1.5:
            display_seed_index = SEEDS.index(DISPLAY_SEED)
            if mode == "monochrome":
                display_fields[("add_noise", mode)] = add_fields[display_seed_index]
            else:
                display_fields[("add_noise", mode)] = add_fields[display_seed_index]

        # Measure calibrated amount response on fixed tone ramps over every seed.
        for tone in TONE_LEVELS:
            grunge_outputs = F32(tone) + grunge_fields
            grunge_clip = clip_stats(grunge_outputs)
            add_outputs = np.clip(F32(tone) + add_fields, F32(0), F32(1))
            add_clip = clip_stats(add_outputs)
            tone_records.append({
                "size": size,
                "tone": tone,
                "effect": "film_grunge",
                "mode": "shared_grain",
                "amount": float(GRAIN_DEFAULT),
                "output": grunge_clip,
            })
            tone_records.append({
                "size": size,
                "tone": tone,
                "effect": "add_noise",
                "mode": mode,
                "amount": amount,
                "output": add_clip,
            })

# Fixed seed 73 contact sheet; all tiles use the same residual gain and clipping.
contact_modes = (("film_grunge", "shared_grain"), ("add_noise", "monochrome"), ("add_noise", "color"))
tile = 112
label_w = 88
header_h = 20
contact = Image.new("RGB", (label_w + len(SIZES) * tile, header_h + len(contact_modes) * tile), (24, 26, 30))
draw = ImageDraw.Draw(contact)
for col_index, size in enumerate(SIZES):
    draw.text((label_w + col_index * tile + 4, 3), f"Size {size:g}", fill=(245, 245, 245))
for row_index, (effect, mode) in enumerate(contact_modes):
    row_label = "Grunge" if effect == "film_grunge" else ("AddNoise M" if mode == "monochrome" else "AddNoise C")
    y = header_h + row_index * tile
    draw.text((3, y + 5), row_label, fill=(245, 245, 245))
    for col_index, size in enumerate(SIZES):
        if effect == "film_grunge":
            field = unit_fields[("grunge", size, "native")][SEEDS.index(DISPLAY_SEED)] * GRAIN_DEFAULT
        else:
            amount = calibrations[f"{size:g}:{mode}"]["addnoise_calibrated_amount"]
            field = unit_fields[("addnoise", size, mode)][SEEDS.index(DISPLAY_SEED)] * F32(amount)
        preview = np.clip(F32(0.5) + field * F32(4.0), 0, 1)
        image = Image.fromarray(np.uint8(preview[..., ::-1] * 255 + 0.5), mode="RGB")
        image = image.resize((tile, tile), Image.Resampling.NEAREST)
        x = label_w + col_index * tile
        contact.paste(image, (x, y))
contact.save(OUT / "matched-grain-strength-spectrum-contact.png")

zero_amount_plate = np.full((HEIGHT, WIDTH, 3), F32(0.5), dtype=F32)
zero_amount_identity = all(
    np.array_equal(np.clip(zero_amount_plate + F32(0.0) * grunge_unit(0, 1.0), 0, 1), zero_amount_plate)
    and np.array_equal(np.clip(zero_amount_plate + F32(0.0) * add_noise_unit(0, 1.0, mode), 0, 1), zero_amount_plate)
    for mode in ("monochrome", "color")
)
if not zero_amount_identity:
    raise RuntimeError("zero amount identity check failed")
if any(x["output"]["nonfinite_count"] for x in tone_records):
    raise RuntimeError("nonfinite tone output")

# Aggregate tone behavior at the control range's shared Size=1.5 while preserving each mode's calibrated Amount.
tone_summaries = [x for x in tone_records if x["size"] == 1.5]
metrics = {
    "study": "Add Noise versus Film Grunge matched grain strength and spectrum r1",
    "environment": {"python": platform.python_version(), "numpy": np.__version__},
    "input_sha256": {
        "plan": sha(PLAN),
        "add_noise_source": sha(ADD_SOURCE),
        "film_grunge_source": sha(GRUNGE_SOURCE),
        "runner": sha(Path(__file__)),
    },
    "source_markers": MARKERS,
    "conditions": {
        "dimensions": [WIDTH, HEIGHT],
        "seed_count": len(SEEDS),
        "seed_min_max": [min(SEEDS), max(SEEDS)],
        "display_seed": DISPLAY_SEED,
        "frame": FRAME,
        "sizes": list(SIZES),
        "tone_levels": list(TONE_LEVELS),
        "film_grunge_grain_amount": float(GRAIN_DEFAULT),
        "film_grunge_effects_disabled": True,
        "addnoise_modes": ["monochrome", "color"],
        "tone_statistics_at_size1p5": tone_summaries,
        "film_grain_consumer_scope": "Core NoiseType::FilmGrain and legacy FilmEffectProcessor excluded: repository searches found no active production callers; compare only Add Noise and registered Film Grunge.",
    },
    "calibration_by_size_and_mode": calibrations,
    "spatial_results": records,
    "tone_response": tone_records,
    "verification": {
        "source_markers_all_true": all(MARKERS.values()),
        "finite_tone_outputs": all(x["output"]["nonfinite_count"] == 0 for x in tone_records),
        "zero_amount_identity": zero_amount_identity,
        "alpha_identity": "CPU apply clones dst=src and writes only RGB channels; alpha preservation is a source-level check, not an image measurement.",
    },
    "limitations": [
        "Independent NumPy equation models; no compiled C++/GPU or app output.",
        "28x28 bounded coordinate domain avoids Film Grunge signed-int hash multiplication overflow; spectra and spatial statistics are small-domain estimates pooled over 128 seeds.",
        "Matching green residual standard deviation is a calibration convention, not perceptual equivalence or preferred visual quality.",
        "Tone charts are synthetic neutral grayscale; no colored scene, alpha, transfer-function, display or temporal acceptance evidence.",
        "Core FilmGrain and legacy FilmEffectProcessor are excluded because current searches found no active production call sites.",
    ],
}
(OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
checksums = {
    path.name: sha(path)
    for path in sorted(OUT.iterdir())
    if path.is_file() and path.name != "checksums.json"
}
(OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
print(json.dumps({
    "sizes": len(SIZES),
    "seed_fields": len(SEEDS),
    "spatial_records": len(records),
    "tone_records": len(tone_records),
    "source_markers_all_true": all(MARKERS.values()),
    "max_calibration_error": max(x["absolute_match_error"] for x in calibrations.values()),
    "outputs": len(checksums),
}))
