from __future__ import annotations

import hashlib
import json
import math
import platform
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
PLAN = ROOT / "plans/grain-suite-temporal-response-r1.json"
CREATIVE_SOURCE = REPO / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
ADD_SOURCE = REPO / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
OUT = ROOT / "captures/grain-suite-temporal-response-r1"
OUT.mkdir(parents=True, exist_ok=True)
WIDTH = HEIGHT = 28
FRAMES = tuple(range(128))
FILM_SEED = ADD_SEED = 1977
REFERENCE_SEED = 20261112
FILM_AMOUNT = np.float32(0.075)
FILM_SIZE = 1.4
ADD_SIZE = 1.0
MATCH_SIZE = 1.15
SOURCE_TONE = np.float32(0.5)
MATCH_LUMA = np.float32(1.0)
MATCH_CHROMA = np.float32(0.65)
MATCH_SHADOW = np.float32(0.25)
MATCH_HIGHLIGHT = np.float32(-0.2)
MATCH_JITTER = np.float32(1.0)
U32_MASK = 0xFFFFFFFF
CREATIVE_TEXT = CREATIVE_SOURCE.read_text(encoding="utf-8")
ADD_TEXT = ADD_SOURCE.read_text(encoding="utf-8")
MARKERS = {
    "film_frame_seed": "1664525ull" in CREATIVE_TEXT and "1013904223ull" in CREATIVE_TEXT,
    "film_grunge_hash_and_low16": "73856093" in CREATIVE_TEXT and "19349663" in CREATIVE_TEXT and "blockHash & 0xffffu" in CREATIVE_TEXT,
    "film_grunge_channel_gains": "channel == 2 ? 1.05f : channel == 0 ? 0.92f" in CREATIVE_TEXT,
    "film_grunge_isolated_additive_term": "destination[offset + channel] =" in CREATIVE_TEXT and "chromaGrain + damage" in CREATIVE_TEXT,
    "addnoise_cpu_hash": "0x7feb352du" in ADD_TEXT and "0x846ca68bu" in ADD_TEXT,
    "addnoise_color_salts_and_floor": "noiseAt(ux, uy, 3u)" in ADD_TEXT and "noiseAt(ux, uy, 2u)" in ADD_TEXT and "std::floor(static_cast<float>(x) / size_)" in ADD_TEXT,
    "addnoise_clamps_rgb": "std::clamp(p[0] + noiseAt" in ADD_TEXT and "std::clamp(p[2] + noiseAt" in ADD_TEXT,
    "match_frame_hash_and_wrapped_shift": "static_cast<int>(impl_->frame), 17, 9127" in CREATIVE_TEXT and "(x + jitterX % width + width) % width" in CREATIVE_TEXT,
    "match_gaussian_residual_and_luma_chroma": "cv::GaussianBlur(channels[channel], lowFrequency" in CREATIVE_TEXT and "chromaResidual * impl_->chromaAmount" in CREATIVE_TEXT,
    "match_rgb_unclamped_and_alpha_copy": "output[offset + channel] = source[offset + channel] +" in CREATIVE_TEXT and "output[offset + 3] = source[offset + 3];" in CREATIVE_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)


def sha_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha(path: Path) -> str:
    return sha_bytes(path.read_bytes())


def u32(value: int) -> int:
    return value & U32_MASK


def center_green(field: np.ndarray) -> np.ndarray:
    green = field[..., 1].astype(np.float64)
    return green - green.mean()


def std_green(field: np.ndarray) -> float:
    return float(center_green(field).std())


def grunge_unit(frame: int) -> np.ndarray:
    block = max(1, int(math.floor(FILM_SIZE + 0.5)))
    frame_seed = u32(FILM_SEED * 1664525 + frame * 1013904223)
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    field = np.empty((HEIGHT, WIDTH, 3), dtype=np.float32)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            value = (frame_seed ^ u32((x // block) * 73856093) ^
                     u32((y // block) * 19349663))
            grain = np.float32(np.float32(value & 0xFFFF) /
                               np.float32(32767.5) - np.float32(1.0))
            field[y, x, 0] = np.float32(grain * np.float32(0.92))
            field[y, x, 1] = grain
            field[y, x, 2] = np.float32(grain * np.float32(1.05))
    return field


def add_noise_hash(x: int, y: int, channel: int, seed: int) -> int:
    value = u32(u32(x * 73856093) ^ u32(y * 19349663) ^
                u32(channel * 83492791) ^ u32(seed))
    value ^= value >> 16
    value = u32(value * 0x7FEB352D)
    value ^= value >> 15
    value = u32(value * 0x846CA68B)
    value ^= value >> 16
    return u32(value)


def add_noise_unit() -> np.ndarray:
    field = np.empty((HEIGHT, WIDTH, 3), dtype=np.float32)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            sx = math.floor(x / ADD_SIZE)
            sy = math.floor(y / ADD_SIZE)
            for channel, salt in enumerate((3, 2, 1)):  # physical B,G,R storage
                value = add_noise_hash(sx, sy, salt, ADD_SEED)
                random = np.float32(np.float32(value & 0x00FFFFFF) /
                                    np.float32(16777215.0) * np.float32(2.0) -
                                    np.float32(1.0))
                field[y, x, channel] = random
    return field


def creative_hash(x: int, y: int, seed: int) -> np.float32:
    value = u32(u32(x) * 0x8DA6B343) ^ u32(u32(y) * 0xD8163841) ^ u32(u32(seed) * 0xCB1AB31F)
    value ^= value >> 13
    value = u32(value * 0x85EBCA6B)
    value ^= value >> 16
    return np.float32(np.float32(value & 0xFFFF) / np.float32(65535.0))


def match_offset(frame: int) -> tuple[int, int]:
    hx = creative_hash(frame, 17, 9127)
    hy = creative_hash(31, frame, 1777)
    dx_value = np.float32(np.float32(np.float32(hx - np.float32(.5)) * np.float32(WIDTH)) * MATCH_JITTER)
    dy_value = np.float32(np.float32(np.float32(hy - np.float32(.5)) * np.float32(HEIGHT)) * MATCH_JITTER)
    return int(dx_value), int(dy_value)


def make_match_reference_residual() -> np.ndarray:
    rng = np.random.default_rng(REFERENCE_SEED)
    coarse = rng.normal(0, .06, (HEIGHT, WIDTH, 3)).astype(np.float32)
    fine = rng.normal(0, .045, (HEIGHT, WIDTH, 3)).astype(np.float32)
    reference = (.5 + fine + cv2.GaussianBlur(coarse, (0, 0), 2.2)).astype(np.float32)
    reference[..., 0] *= np.float32(.92)
    reference[..., 2] *= np.float32(1.04)
    reference = np.clip(reference, 0, 1).astype(np.float32)
    low = cv2.GaussianBlur(reference, (0, 0), MATCH_SIZE)
    return (reference - low).astype(np.float32)


def match_green_from_shifted(residual: np.ndarray, amount: float, frame: int) -> tuple[np.ndarray, tuple[int, int]]:
    dx, dy = match_offset(frame)
    xs = (np.arange(WIDTH) + dx) % WIDTH
    ys = (np.arange(HEIGHT) + dy) % HEIGHT
    shifted = residual[ys[:, None], xs[None, :], :]
    residual_luma = (shifted[..., 2] * np.float32(.2126) +
                     shifted[..., 1] * np.float32(.7152) +
                     shifted[..., 0] * np.float32(.0722))
    source_luma = np.clip(SOURCE_TONE, 0, 1)
    response = np.maximum(np.float32(0), np.float32(
        1 + (1 - source_luma) * MATCH_SHADOW + source_luma * MATCH_HIGHLIGHT))
    output = np.empty_like(shifted)
    for channel in range(3):
        chroma = shifted[..., channel] - residual_luma
        grain = (residual_luma * MATCH_LUMA + chroma * MATCH_CHROMA) * np.float32(amount) * response
        output[..., channel] = grain
    return output, (dx, dy)


def correlation(a: np.ndarray, b: np.ndarray) -> float:
    av = a.astype(np.float64).ravel()
    bv = b.astype(np.float64).ravel()
    av -= av.mean()
    bv -= bv.mean()
    denominator = math.sqrt(float(np.mean(av * av) * np.mean(bv * bv)))
    return float(np.mean(av * bv) / denominator) if denominator else 0.0


def quantile_summary(values: list[float]) -> dict[str, float]:
    return {"p05": float(np.quantile(values, .05)), "p50": float(np.quantile(values, .5)),
            "p95": float(np.quantile(values, .95)), "max": float(np.max(values))}


def pooled_temporal_autocorrelation(fields: np.ndarray, lag: int) -> float | None:
    values = fields.astype(np.float64)
    centered = values - values.mean(axis=0, keepdims=True)
    a, b = centered[:-lag], centered[lag:]
    denominator = math.sqrt(float(np.sum(a * a) * np.sum(b * b)))
    return float(np.sum(a * b) / denominator) if denominator else None


def analyze_temporal(name: str, fields: np.ndarray, rgb_fields: np.ndarray,
                     amount: float, target_std: float, clamp: bool) -> tuple[dict, np.ndarray]:
    centered = fields.astype(np.float64) - fields.astype(np.float64).mean(axis=(1, 2), keepdims=True)
    pair_corr = [correlation(fields[i], fields[i + 1]) for i in range(len(fields) - 1)]
    delta = fields[1:].astype(np.float64) - fields[:-1].astype(np.float64)
    centered_delta = centered[1:] - centered[:-1]
    delta_rmse = np.sqrt(np.mean(delta * delta, axis=(1, 2)))
    centered_delta_rmse = np.sqrt(np.mean(centered_delta * centered_delta, axis=(1, 2)))
    field_hashes = [sha_bytes(np.ascontiguousarray(frame).tobytes()) for frame in fields]
    per_pixel_temporal_std = fields.astype(np.float64).std(axis=0)
    channel_corr = np.corrcoef(rgb_fields[0].reshape(-1, 3).T)
    output = np.float32(SOURCE_TONE) + rgb_fields
    if clamp:
        output = np.clip(output, 0.0, 1.0).astype(np.float32)
    return {
        "effect": name,
        "calibrated_amount": float(amount),
        "green_std_target_frame0": target_std,
        "green_std_frame0": float(fields[0].std(dtype=np.float64)),
        "green_frame_mean_p05_p50_p95": [float(x) for x in np.quantile(fields.mean(axis=(1, 2)), [.05, .5, .95])],
        "green_frame_std_p05_p50_p95": [float(x) for x in np.quantile(fields.std(axis=(1, 2), dtype=np.float64), [.05, .5, .95])],
        "green_frame_min_max_envelope": [float(fields.min()), float(fields.max())],
        "adjacent_green_correlation": quantile_summary(pair_corr),
        "lag4_adjacent_frame_spatial_correlation_p50": float(np.quantile([correlation(fields[i], fields[i + 4]) for i in range(len(fields) - 4)], .5)),
        "pooled_per_pixel_temporal_autocorrelation": {
            "lag1": pooled_temporal_autocorrelation(fields, 1),
            "lag4": pooled_temporal_autocorrelation(fields, 4),
            "definition": "Remove each pixel's mean over the full 128-frame sequence, then pool correlation products across all frame/pixel pairs at the stated lag.",
        },
        "adjacent_green_delta_rmse": quantile_summary(delta_rmse.tolist()),
        "adjacent_green_delta_rmse_over_target_std": quantile_summary((delta_rmse / target_std).tolist()),
        "mean_centered_adjacent_green_delta_rmse": quantile_summary(centered_delta_rmse.tolist()),
        "per_pixel_temporal_std_mean_over_target_std": float(per_pixel_temporal_std.mean() / target_std),
        "unique_green_fields": len(set(field_hashes)),
        "frame0_equals_frame127": bool(np.array_equal(fields[0], fields[-1])),
        "frame0_rgb_channel_correlation": channel_corr.tolist(),
        "output_rgb_min_max": [[float(output[..., c].min()), float(output[..., c].max())] for c in range(3)],
        "output_rgb_below0_above1_fraction": [float(np.mean(output < 0)), float(np.mean(output > 1))],
        "output_rgb_exact_endpoint_fraction": float(np.mean((output == 0) | (output == 1))),
        "nonfinite_count": int(np.count_nonzero(~np.isfinite(fields)) + np.count_nonzero(~np.isfinite(output))),
        "static_field_identity": bool(all(np.array_equal(fields[0], fields[i]) for i in range(1, len(fields)))) if name == "add_noise" else None,
    }, output


def make_contact_sheet(rgb_by_effect: dict[str, np.ndarray]) -> None:
    selected_frames = (0, 1, 2, 3, 4, 5)
    label_w, cell_w, cell_h, header_h = 78, 112, 112, 22
    names = ("film_grunge", "add_noise", "match_grain")
    sheet = Image.new("RGB", (label_w + len(selected_frames) * cell_w,
                               header_h + len(names) * cell_h), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    for col, frame in enumerate(selected_frames):
        draw.text((label_w + col * cell_w + 3, 4), f"frame {frame}", fill=(245, 245, 245))
    labels = {"film_grunge": "Grunge", "add_noise": "Add Noise", "match_grain": "Match Grain"}
    for row, name in enumerate(names):
        draw.text((3, header_h + row * cell_h + 5), labels[name], fill=(245, 245, 245))
        for col, frame in enumerate(selected_frames):
            preview_bgr = np.clip(SOURCE_TONE + rgb_by_effect[name][frame] * np.float32(4.0), 0, 1)
            preview_rgb = np.uint8(np.rint(preview_bgr[..., ::-1] * 255))
            tile = Image.fromarray(preview_rgb, mode="RGB").resize((cell_w, cell_h), Image.Resampling.NEAREST)
            sheet.paste(tile, (label_w + col * cell_w, header_h + row * cell_h))
    sheet.save(OUT / "grain-suite-temporal-contact.png")


def main() -> None:
    grain_unit = np.stack([grunge_unit(frame) for frame in FRAMES])
    grunge_base = grain_unit * FILM_AMOUNT
    target_std = std_green(grunge_base[0])
    add_unit = add_noise_unit()
    add_amount = target_std / std_green(add_unit)
    match_reference_residual = make_match_reference_residual()
    match_unit_frame0, _ = match_green_from_shifted(match_reference_residual, 1.0, 0)
    match_amount = target_std / std_green(match_unit_frame0)
    if not 0 <= add_amount <= 1 or not 0 <= match_amount <= 4:
        raise RuntimeError({"add_amount": add_amount, "match_amount": match_amount})

    grunge_rgb = grunge_base.astype(np.float32)
    add_rgb = (add_unit * np.float32(add_amount)).astype(np.float32)
    match_frame_outputs = [match_green_from_shifted(match_reference_residual, match_amount, frame)[0]
                           for frame in FRAMES]
    match_rgb = np.stack(match_frame_outputs)
    add_rgb_sequence = np.broadcast_to(add_rgb, (len(FRAMES), HEIGHT, WIDTH, 3)).copy()
    sequences = {"film_grunge": grunge_rgb, "add_noise": add_rgb_sequence, "match_grain": match_rgb}
    green_sequences = {name: values[..., 1] for name, values in sequences.items()}
    stats = []
    outputs = {}
    for name in ("film_grunge", "add_noise", "match_grain"):
        result, output = analyze_temporal(name, green_sequences[name], sequences[name],
                                          FILM_AMOUNT if name == "film_grunge" else (add_amount if name == "add_noise" else match_amount),
                                          target_std, clamp=name == "add_noise")
        stats.append(result)
        outputs[name] = output

    match_offset0 = match_offset(0)
    aligned_errors = []
    for frame in FRAMES:
        dx, dy = match_offset(frame)
        current = match_rgb[frame, ..., 1]
        aligned = np.roll(current, shift=(dy - match_offset0[1], dx - match_offset0[0]), axis=(0, 1))
        aligned_errors.append(float(np.sqrt(np.mean((aligned.astype(np.float64) - match_rgb[0, ..., 1]) ** 2))))
    add_identity = all(np.array_equal(add_rgb_sequence[0], add_rgb_sequence[frame]) for frame in FRAMES)
    zero_amount_identity = (
        np.array_equal(np.clip(SOURCE_TONE + add_unit * np.float32(0), 0, 1), np.full_like(add_unit, SOURCE_TONE)) and
        np.array_equal(SOURCE_TONE + grunge_unit(0) * np.float32(0), np.full((HEIGHT, WIDTH, 3), SOURCE_TONE, np.float32)) and
        np.array_equal(SOURCE_TONE + match_unit_frame0 * np.float32(0), np.full_like(match_unit_frame0, SOURCE_TONE))
    )
    if not add_identity or not zero_amount_identity or max(aligned_errors) > 1e-7:
        raise RuntimeError({"add_noise_identity": add_identity,
                            "zero_amount_identity": zero_amount_identity,
                            "match_known_offset_alignment_max_rmse": max(aligned_errors)})
    if any(item["nonfinite_count"] for item in stats):
        raise RuntimeError("nonfinite outputs")

    make_contact_sheet(sequences)
    metrics = {
        "study": "Add Noise / Film Grunge / Match Grain temporal response at matched initial green RMS r1",
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "opencv": cv2.__version__},
        "input_sha256": {"plan": sha(PLAN), "creative_source": sha(CREATIVE_SOURCE),
                         "add_noise_source": sha(ADD_SOURCE), "runner": sha(Path(__file__))},
        "source_markers": MARKERS,
        "synthetic_match_reference_residual_sha256": sha_bytes(np.ascontiguousarray(match_reference_residual).tobytes()),
        "conditions": {"dimensions_wh": [WIDTH, HEIGHT], "frames_inclusive": [FRAMES[0], FRAMES[-1]],
                       "frame_count": len(FRAMES), "film_seed_addnoise_seed": FILM_SEED,
                       "reference_seed": REFERENCE_SEED, "source_tone": float(SOURCE_TONE),
                       "film_grunge_size_block": [FILM_SIZE, max(1, int(math.floor(FILM_SIZE + .5)))],
                       "add_noise_size_mode": [ADD_SIZE, "color"], "match_grain_size_jitter": [MATCH_SIZE, float(MATCH_JITTER)],
                       "target_green_residual_std_frame0": target_std,
                       "calibrated_amounts": {"film_grunge": float(FILM_AMOUNT), "add_noise": add_amount, "match_grain": match_amount}},
        "source_model_checks": {"all_markers_true": all(MARKERS.values()), "add_noise_temporal_identity": add_identity,
                                "zero_amount_identity_all_effect_models": zero_amount_identity,
                                "match_grain_reverse_offset_alignment_rmse_max": max(aligned_errors),
                                "all_outputs_finite": all(item["nonfinite_count"] == 0 for item in stats)},
        "effect_results": stats,
        "match_grain_offsets_first_six": [list(match_offset(frame)) for frame in range(6)],
        "limitations": [
            "Independent equation models only; no C++ or Artifact app output.",
            "Initial green residual standard deviation is matched; RGB spectrum, channel correlation and appearance remain distinct.",
            "Film Grunge models only its grain term; other components are set to zero analytically and its seeded random machinery is not executed.",
            "Add Noise uses its CPU color equation with fixed seed and no frame input; other GPU routes are not modeled.",
            "Match Grain uses a fixed small synthetic reference and default frame-hash cyclic shift; real named-input routing and texture metadata are unknown.",
            "Static neutral source, opaque alpha, 28x28 safe coordinate domain, and 128 frames do not represent playback quality or product dimensions.",
            "No perceived flicker threshold, aesthetic acceptance, GPU/preview/final parity, performance, or recommended control values are established.",
        ],
    }
    (OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    checksums = {path.name: sha(path) for path in sorted(OUT.iterdir())
                 if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"markers_all_true": all(MARKERS.values()), "effect_records": len(stats),
                      "frames": len(FRAMES), "target_green_std": target_std,
                      "calibrated_add_amount": add_amount, "calibrated_match_amount": match_amount,
                      "add_noise_static_identity": add_identity,
                      "match_alignment_rmse": max(aligned_errors), "outputs": len(checksums)}))


if __name__ == "__main__":
    main()
