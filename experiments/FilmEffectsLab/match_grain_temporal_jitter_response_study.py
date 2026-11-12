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
PLAN = ROOT / "plans/match-grain-temporal-jitter-response-r1.json"
SOURCE = REPO / "Artifact/src/Effect/ArtifactCreativeEffects.cpp"
OUT = ROOT / "captures/match-grain-temporal-jitter-response-r1"
OUT.mkdir(parents=True, exist_ok=True)
SEED = 20261112
SIZES = ((512, 256), (1024, 512))
FRAMES = tuple(range(240))
JITTERS = (0.0, 0.05, 0.1, 0.25, 0.5, 1.0)
GRAIN_SIZE = 1.15
AMOUNT = 1.0
LUMA_AMOUNT = 1.0
CHROMA_AMOUNT = 0.65
MIX = 1.0
SHADOW_RESPONSE = 0.25
HIGHLIGHT_RESPONSE = -0.2
SOURCE_LUMA = np.float32(0.5)
U32_MASK = 0xFFFFFFFF
SOURCE_TEXT = SOURCE.read_text(encoding="utf-8")
MARKERS = {
    "creative_hash_uint32_constants": "0x8da6b343u" in SOURCE_TEXT and "0xd8163841u" in SOURCE_TEXT and "0xcb1ab31fu" in SOURCE_TEXT,
    "creative_hash_final_low16_float": "value & 0xffffu" in SOURCE_TEXT and "65535.0f" in SOURCE_TEXT,
    "jitter_x_frame_hash": "static_cast<int>(impl_->frame), 17, 9127" in SOURCE_TEXT,
    "jitter_y_frame_hash": "31, static_cast<int>(impl_->frame), 1777" in SOURCE_TEXT,
    "jitter_float_then_int_cast": (
        "static_cast<int>((creativeHash(" in SOURCE_TEXT and
        "* width *\n        impl_->temporalJitter);" in SOURCE_TEXT and
        "* height *\n        impl_->temporalJitter);" in SOURCE_TEXT),
    "cyclic_lookup": "(y + jitterY % height + height) % height" in SOURCE_TEXT and "(x + jitterX % width + width) % width" in SOURCE_TEXT,
    "grain_sigma_floor": "std::max(0.1f, impl_->grainSize)" in SOURCE_TEXT,
}
if not all(MARKERS.values()):
    raise RuntimeError(MARKERS)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def u32(value: int) -> int:
    return value & U32_MASK


def creative_hash(x: int, y: int, seed: int) -> np.float32:
    value = u32(u32(x) * 0x8DA6B343) ^ u32(u32(y) * 0xD8163841) ^ u32(u32(seed) * 0xCB1AB31F)
    value ^= value >> 13
    value = u32(value * 0x85EBCA6B)
    value ^= value >> 16
    return np.float32(np.float32(value & 0xFFFF) / np.float32(65535.0))


def jitter_for(frame: int, width: int, height: int, amount: float) -> tuple[int, int]:
    hx = creative_hash(frame, 17, 9127)
    hy = creative_hash(31, frame, 1777)
    x_value = np.float32(np.float32(np.float32(hx - np.float32(0.5)) * np.float32(width)) * np.float32(amount))
    y_value = np.float32(np.float32(np.float32(hy - np.float32(0.5)) * np.float32(height)) * np.float32(amount))
    return int(x_value), int(y_value)  # C++ float-to-int truncates toward zero.


def make_reference_luma(width: int, height: int) -> np.ndarray:
    rng = np.random.default_rng(SEED)
    coarse = rng.normal(0, 0.06, (height, width, 3)).astype(np.float32)
    fine = rng.normal(0, 0.045, (height, width, 3)).astype(np.float32)
    mid = cv2.GaussianBlur(coarse, (0, 0), 2.2)
    texture = 0.5 + fine + mid
    texture[..., 0] *= 0.92
    texture[..., 2] *= 1.04
    reference = np.clip(texture, 0.0, 1.0).astype(np.float32)
    low = cv2.GaussianBlur(reference, (0, 0), GRAIN_SIZE)
    residual = reference - low
    residual_luma = (residual[..., 2] * np.float32(0.2126) +
                     residual[..., 1] * np.float32(0.7152) +
                     residual[..., 0] * np.float32(0.0722))
    source_luma_clamped = np.clip(SOURCE_LUMA, 0.0, 1.0)
    response = np.maximum(np.float32(0.0), np.float32(
        1.0 + (1.0 - source_luma_clamped) * SHADOW_RESPONSE +
        source_luma_clamped * HIGHLIGHT_RESPONSE))
    # Luma projection of the RGB luma/chroma equation collapses to residualLuma
    # because the source's three luma coefficients sum to 1.
    return (residual_luma * np.float32(LUMA_AMOUNT) * np.float32(AMOUNT) *
            response * np.float32(MIX)).astype(np.float32)


def corr(a: np.ndarray, b: np.ndarray) -> float:
    av = a.astype(np.float64).ravel()
    bv = b.astype(np.float64).ravel()
    av -= av.mean()
    bv -= bv.mean()
    denominator = math.sqrt(float(np.mean(av * av) * np.mean(bv * bv)))
    return float(np.mean(av * bv) / denominator) if denominator else 0.0


def quantiles(values: np.ndarray) -> dict[str, float]:
    return {"p50": float(np.quantile(values, 0.50)),
            "p90": float(np.quantile(values, 0.90)),
            "p95": float(np.quantile(values, 0.95)),
            "max": float(np.max(values))}


def shifted(field: np.ndarray, offset: tuple[int, int]) -> np.ndarray:
    dx, dy = offset
    height, width = field.shape
    ys = (np.arange(height) + dy) % height
    xs = (np.arange(width) + dx) % width
    return field[ys[:, None], xs[None, :]]


def analyze_field(field: np.ndarray, width: int, height: int, amount: float) -> dict:
    offsets = np.array([jitter_for(frame, width, height, amount) for frame in FRAMES], dtype=np.int64)
    step = np.diff(offsets, axis=0)
    step_magnitude = np.hypot(step[:, 0], step[:, 1])
    normalized_step = np.hypot(step[:, 0] / width, step[:, 1] / height)
    diagonal_fraction = step_magnitude / math.hypot(width, height)
    wrap = 1.0 - (1.0 - np.abs(offsets[:, 0]) / width) * (1.0 - np.abs(offsets[:, 1]) / height)
    frame_fields = []
    delta_rmse = []
    frame_corr = []
    aligned_rmse = []
    previous = None
    for frame, offset in enumerate(offsets):
        current = shifted(field, (int(offset[0]), int(offset[1])))
        frame_fields.append(current if frame in (0, 1, 2, 3) else None)
        if previous is not None:
            delta = current.astype(np.float64) - previous.astype(np.float64)
            delta_rmse.append(float(np.sqrt(np.mean(delta * delta))))
            frame_corr.append(corr(previous, current))
            previous_offset = offsets[frame - 1]
            delta_x = int(offset[0] - previous_offset[0])
            delta_y = int(offset[1] - previous_offset[1])
            aligned = np.roll(current, shift=(delta_y, delta_x), axis=(0, 1))
            aligned_rmse.append(float(np.sqrt(np.mean((aligned.astype(np.float64) - previous) ** 2))))
        previous = current
    abs_x = np.abs(offsets[:, 0])
    abs_y = np.abs(offsets[:, 1])
    result = {
        "temporal_jitter": amount,
        "frame_count": len(FRAMES),
        "offset_x_min_max": [int(offsets[:, 0].min()), int(offsets[:, 0].max())],
        "offset_y_min_max": [int(offsets[:, 1].min()), int(offsets[:, 1].max())],
        "offset_x_abs_quantiles_px": quantiles(abs_x),
        "offset_y_abs_quantiles_px": quantiles(abs_y),
        "offset_x_abs_quantiles_frame_fraction": quantiles(abs_x / width),
        "offset_y_abs_quantiles_frame_fraction": quantiles(abs_y / height),
        "unique_xy_offsets": int(np.unique(offsets, axis=0).shape[0]),
        "zero_offset_frame_fraction": float(np.mean(np.all(offsets == 0, axis=1))),
        "adjacent_step_xy_abs_px_quantiles": {
            "x": quantiles(np.abs(step[:, 0])), "y": quantiles(np.abs(step[:, 1])),
            "euclidean": quantiles(step_magnitude)},
        "adjacent_step_xy_frame_fraction_quantiles": {
            "euclidean": quantiles(normalized_step)},
        "adjacent_step_frame_diagonal_fraction_quantiles": quantiles(diagonal_fraction),
        "adjacent_zero_step_fraction": float(np.mean(np.all(step == 0, axis=1))),
        "cyclic_wrap_coverage_fraction_quantiles": quantiles(wrap),
        "adjacent_residual_luma_delta_rmse": {
            "p50": float(np.quantile(delta_rmse, .5)), "p95": float(np.quantile(delta_rmse, .95)),
            "max": float(np.max(delta_rmse))},
        "adjacent_residual_luma_correlation": {
            "p50": float(np.quantile(frame_corr, .5)), "p05": float(np.quantile(frame_corr, .05)),
            "p95": float(np.quantile(frame_corr, .95))},
        "known_offset_reverse_alignment_rmse_max": float(np.max(aligned_rmse)),
        "residual_luma_std": float(field.std(dtype=np.float64)),
        "residual_luma_min_max": [float(field.min()), float(field.max())],
        "nonfinite_count": int(np.count_nonzero(~np.isfinite(field))),
        "display_fields": frame_fields[:4],
        "offsets": offsets,
    }
    return result


def write_contact_sheet(all_results: dict) -> None:
    frames = (0, 1, 2, 3)
    jitter_grid = JITTERS
    cell_w, cell_h, label_w, header_h = 256, 128, 80, 22
    sheet = Image.new("RGB", (label_w + len(frames) * cell_w,
                               header_h + len(jitter_grid) * cell_h), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    for col, frame in enumerate(frames):
        draw.text((label_w + col * cell_w + 5, 4), f"frame {frame}", fill=(245, 245, 245))
    width, height = SIZES[0]
    for row, jitter in enumerate(jitter_grid):
        result = all_results[(width, height, jitter)]
        draw.text((3, header_h + row * cell_h + 5), f"j={jitter:g}", fill=(245, 245, 245))
        base_fields = result["display_fields"]
        for col, value in enumerate(base_fields):
            preview = np.clip(0.5 + value * 4.0, 0.0, 1.0)
            gray = np.uint8(np.rint(preview * 255.0))
            rgb = np.repeat(gray[..., None], 3, axis=2)
            tile = Image.fromarray(rgb, mode="RGB").resize((cell_w, cell_h), Image.Resampling.NEAREST)
            sheet.paste(tile, (label_w + col * cell_w, header_h + row * cell_h))
    sheet.save(OUT / "match-grain-temporal-jitter-contact.png")


def main() -> None:
    results = {}
    records = []
    fields = {}
    for width, height in SIZES:
        field = make_reference_luma(width, height)
        fields[(width, height)] = field
        for jitter in JITTERS:
            analyzed = analyze_field(field, width, height, jitter)
            record = {key: value for key, value in analyzed.items()
                      if key not in ("offsets", "display_fields")}
            record["dimensions_wh"] = [width, height]
            results[(width, height, jitter)] = analyzed
            records.append(record)

    # Cross-resolution normalized displacement should match the same hash sample
    # up to integer truncation, because offsets scale with width and height.
    cross_resolution = []
    small_w, small_h = SIZES[0]
    large_w, large_h = SIZES[1]
    for jitter in JITTERS:
        small = results[(small_w, small_h, jitter)]["offsets"]
        large = results[(large_w, large_h, jitter)]["offsets"]
        diff = np.hypot(large[:, 0] / large_w - small[:, 0] / small_w,
                        large[:, 1] / large_h - small[:, 1] / small_h)
        cross_resolution.append({"temporal_jitter": jitter,
                                 "normalized_displacement_difference_p95": float(np.quantile(diff, .95)),
                                 "normalized_displacement_difference_max": float(diff.max())})

    zero = [results[(width, height, 0.0)] for width, height in SIZES]
    zero_identity = all(
        item["zero_offset_frame_fraction"] == 1.0 and
        item["adjacent_zero_step_fraction"] == 1.0 and
        item["known_offset_reverse_alignment_rmse_max"] == 0.0 and
        item["adjacent_residual_luma_delta_rmse"]["max"] == 0.0
        for item in zero)
    alignment_error = max(item["known_offset_reverse_alignment_rmse_max"] for item in results.values())
    if not zero_identity or alignment_error > 1e-12:
        raise RuntimeError({"zero_identity": zero_identity, "alignment_error": alignment_error})
    if any(item["nonfinite_count"] for item in results.values()):
        raise RuntimeError("nonfinite residual field")

    write_contact_sheet(results)
    metrics = {
        "study": "Match Grain temporalJitter displacement and static-reference response r1",
        "environment": {"python": platform.python_version(), "numpy": np.__version__, "opencv": cv2.__version__},
        "input_sha256": {"plan": sha(PLAN), "source": sha(SOURCE), "runner": sha(Path(__file__))},
        "synthetic_residual_sha256": {
            f"{width}x{height}": hashlib.sha256(np.ascontiguousarray(field).tobytes()).hexdigest()
            for (width, height), field in fields.items()
        },
        "source_markers": MARKERS,
        "conditions": {
            "dimensions_wh": [list(size) for size in SIZES],
            "frames_inclusive": [min(FRAMES), max(FRAMES)], "frame_count": len(FRAMES),
            "frame_rate_interpretation": "24 fps, 10 seconds; no playback simulated",
            "jitter_values": list(JITTERS), "grain_size": GRAIN_SIZE,
            "amount_luma_chroma_mix": [AMOUNT, LUMA_AMOUNT, CHROMA_AMOUNT, MIX],
            "shadow_highlight_response": [SHADOW_RESPONSE, HIGHLIGHT_RESPONSE],
            "source_luma": float(SOURCE_LUMA), "reference_seed": SEED,
        },
        "source_model_checks": {
            "all_source_markers_true": all(MARKERS.values()),
            "zero_jitter_identity": zero_identity,
            "max_known_offset_alignment_rmse": alignment_error,
            "finite_reference_residuals": all(item["nonfinite_count"] == 0 for item in results.values()),
        },
        "results": records,
        "cross_resolution_normalized_displacement": cross_resolution,
        "limitations": [
            "Independent Python/NumPy/OpenCV equation model, not compiled Artifact output or application capture.",
            "Static source luma and fixed reference isolate the temporal hash shift; motion, changing source/reference, playback scheduling and dropped frames are absent.",
            "Frame range is 0..239 (10 seconds interpreted at 24 fps), not a full hash period or a perceptual temporal acceptance test.",
            "Cyclic wrap is the source behavior; wrap coverage describes texture lookup, not a frame border defect.",
            "Alpha is opaque and unchanged by the modeled source equation; descriptor, import and compositing semantics remain unverified.",
            "Resolution comparison uses independently generated fixed-seed synthetic references; normalized displacement scaling is the controlled comparison, not identical reference pixels across resolutions.",
            "No runtime performance, GPU parity, user preference, or recommended temporalJitter value is established.",
        ],
    }
    (OUT / "metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    checksums = {path.name: sha(path) for path in sorted(OUT.iterdir())
                 if path.is_file() and path.name != "checksums.json"}
    (OUT / "checksums.json").write_text(json.dumps(checksums, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"source_markers_all_true": all(MARKERS.values()), "records": len(records),
                      "zero_jitter_identity": zero_identity, "max_alignment_rmse": alignment_error,
                      "output_files": len(checksums)}))


if __name__ == "__main__":
    main()
