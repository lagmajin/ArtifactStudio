"""Independent, source-derived image models for the Artifact glow suite.

This lab reproduces the broad CPU formulas for visual exploration. It is not
the product effect implementation and cannot establish product parity.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label, to_u8
from light_leak_lab import load_input


def smooth_glow(rgb: np.ndarray, threshold: float = 0.62, radius: float = 8.0,
                amount: float = 0.55, wide: float = 0.25) -> np.ndarray:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    mask = np.clip((luma - threshold) / max(1e-4, 1.0 - threshold), 0.0, 1.0)
    seed = rgb * mask[..., None]
    near = cv2.GaussianBlur(seed, (0, 0), radius, borderType=cv2.BORDER_REPLICATE)
    broad = cv2.GaussianBlur(seed, (0, 0), radius * 3.0,
                             borderType=cv2.BORDER_REPLICATE)
    return np.clip(rgb + amount * (near * (1.0 - wide) + broad * wide), 0.0, 1.0)


def percentile_soft_bloom(rgb: np.ndarray, percentile: float) -> np.ndarray:
    """Exposure-relative extraction candidate; frame-to-frame smoothing is absent."""
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    threshold = float(np.percentile(luma, percentile))
    knee = max(0.025, threshold * 0.16)
    t = np.clip((luma - (threshold - knee)) / (2.0 * knee), 0.0, 1.0)
    mask = t * t * (3.0 - 2.0 * t)
    seed = rgb * mask[..., None]
    near = cv2.GaussianBlur(seed, (0, 0), 7.0, borderType=cv2.BORDER_REPLICATE)
    broad = cv2.GaussianBlur(seed, (0, 0), 23.0, borderType=cv2.BORDER_REPLICATE)
    return np.clip(rgb + (near * 0.75 + broad * 0.25) * 0.85, 0.0, 1.0)


def edge_bloom(rgb: np.ndarray) -> np.ndarray:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    laplacian = np.abs(cv2.Laplacian(luma, cv2.CV_32F, ksize=3,
                                    borderType=cv2.BORDER_REPLICATE))
    highlight = np.clip((luma - 0.65) / 0.35, 0.0, 1.0)
    mask = np.clip(laplacian * 0.45 + highlight, 0.0, 1.0)
    mask = cv2.GaussianBlur(mask, (0, 0), 8.0, borderType=cv2.BORDER_REPLICATE)
    tint_mix = 0.35
    glow = mask[..., None] * ((1.0 - tint_mix) + rgb * tint_mix)
    return np.clip(rgb + glow * 0.55, 0.0, 1.0)


def reactive_glow(rgb: np.ndarray) -> np.ndarray:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    saturation = rgb.max(axis=2) - rgb.min(axis=2)
    bright = np.clip((luma - 0.55) / 0.45, 0.0, 1.0)
    response = np.clip(bright * 0.8 + saturation * 0.38, 0.0, 1.0)
    bloom = cv2.GaussianBlur(rgb * response[..., None], (0, 0), 12.0,
                             borderType=cv2.BORDER_REPLICATE)
    return np.clip(rgb + bloom * 0.48, 0.0, 1.0)


def chromatic_glow(rgb: np.ndarray) -> np.ndarray:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    mask = np.clip((luma - 0.62) / 0.38, 0.0, 1.0)
    bloom = cv2.GaussianBlur(rgb * mask[..., None], (0, 0), 9.0,
                             borderType=cv2.BORDER_REPLICATE)
    height, width = luma.shape
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    shift = 2.25
    dx, dy = np.cos(np.deg2rad(35.0)) * shift, np.sin(np.deg2rad(35.0)) * shift
    red = cv2.remap(bloom[..., 0], (xx + dx).astype(np.float32),
                    (yy + dy).astype(np.float32), cv2.INTER_LINEAR,
                    borderMode=cv2.BORDER_REPLICATE)
    blue = cv2.remap(bloom[..., 2], (xx - dx).astype(np.float32),
                     (yy - dy).astype(np.float32), cv2.INTER_LINEAR,
                     borderMode=cv2.BORDER_REPLICATE)
    spectral = np.stack((blue * 0.8 + bloom[..., 0] * 0.2,
                         bloom[..., 1], red * 0.8 + bloom[..., 2] * 0.2), axis=-1)
    return np.clip(rgb + spectral * 0.62, 0.0, 1.0)


def liquid_glow(rgb: np.ndarray, phase: float = 0.0) -> np.ndarray:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    mask = np.clip((luma - 0.55) / 0.45, 0.0, 1.0)
    bloom = cv2.GaussianBlur(rgb * mask[..., None], (0, 0), 15.0,
                             borderType=cv2.BORDER_REPLICATE)
    height, width = luma.shape
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    nx, ny = xx / 42.0, yy / 42.0
    flow_x = np.sin(ny * 1.73 + phase) + 0.5 * np.sin((nx + ny) * 0.71 - phase * 0.63)
    flow_y = np.cos(nx * 1.37 - phase * 0.81) + 0.5 * np.cos((nx - ny) * 0.83 + phase)
    flowed = cv2.remap(bloom, xx + flow_x * 5.0, yy + flow_y * 5.0,
                       cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT_101)
    return np.clip(rgb + flowed * 0.62, 0.0, 1.0)


def residual_sequence(frames: list[np.ndarray], decay: float = 0.82) -> tuple[np.ndarray, np.ndarray]:
    history: np.ndarray | None = None
    current_glow = np.zeros_like(frames[-1][..., :3])
    for frame in frames:
        rgb = frame[..., :3]
        luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
        mask = np.clip((luma - 0.6) / 0.4, 0.0, 1.0)
        current_glow = cv2.GaussianBlur(rgb * mask[..., None], (0, 0), 12.0,
                                       borderType=cv2.BORDER_REPLICATE)
        history = current_glow.copy() if history is None else np.maximum(current_glow, history * decay)
    assert history is not None
    latest = frames[-1][..., :3]
    return np.clip(latest + history * 0.55, 0.0, 1.0), np.clip(latest + current_glow * 0.55, 0.0, 1.0)


def save_rgba(path: Path, rgb: np.ndarray, alpha: np.ndarray) -> None:
    rgba = np.dstack((rgb, alpha))
    cv2.imwrite(str(path), cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGRA))


def metrics(source: np.ndarray, output: np.ndarray) -> dict[str, float | bool]:
    delta = output[..., :3] - source[..., :3]
    newly_clipped = (source[..., :3] < 0.999) & (output[..., :3] >= 0.999)
    source_luma = source[..., :3] @ np.asarray((0.2126, 0.7152, 0.0722), np.float32)
    output_luma = output[..., :3] @ np.asarray((0.2126, 0.7152, 0.0722), np.float32)
    return {
        "rgb_mae": float(np.mean(np.abs(delta))),
        "mean_input_space_luma_delta": float(np.mean(output_luma - source_luma)),
        "newly_clipped_channel_fraction": float(np.mean(newly_clipped)),
        "output_clipped_channel_fraction": float(np.mean(output[..., :3] >= 0.999)),
        "alpha_exact": bool(np.array_equal(source[..., 3], output[..., 3])),
        "finite": bool(np.isfinite(output).all()),
    }


def render(output: Path, input_path: Path | None, frame_index: int,
           strength_scale: float) -> None:
    output.mkdir(parents=True, exist_ok=True)
    if input_path is None:
        from glow_lab import make_fixture
        source = make_fixture()
        residual_frames: list[np.ndarray] = []
    else:
        source = cv2.resize(load_input(input_path, frame_index), (960, 540),
                            interpolation=cv2.INTER_AREA)
        residual_frames = []
        if input_path.suffix.lower() in {".mp4", ".mov", ".avi", ".mkv"} and frame_index >= 3:
            residual_frames = [cv2.resize(load_input(input_path, i), (960, 540),
                                          interpolation=cv2.INTER_AREA)
                               for i in range(frame_index - 3, frame_index + 1)]

    rgb, alpha = source[..., :3], source[..., 3]
    full_strength: dict[str, np.ndarray] = {
        "bloom_soft_halo": smooth_glow(rgb),
        "edge_bloom": edge_bloom(rgb),
        "reactive_glow": reactive_glow(rgb),
        "chromatic_glow": chromatic_glow(rgb),
        "liquid_glow": liquid_glow(rgb),
        "percentile_soft_bloom_p95": percentile_soft_bloom(rgb, 95.0),
        "percentile_soft_bloom_p99": percentile_soft_bloom(rgb, 99.0),
    }
    if residual_frames:
        sequence_glow, current_only = residual_sequence(residual_frames)
        full_strength["residual_glow_sequence"] = sequence_glow
        full_strength["residual_current_only_control"] = current_only
    variants = {
        name: np.clip(rgb + (result - rgb) * strength_scale, 0.0, 1.0)
        for name, result in full_strength.items()
    }

    panels = [add_label(to_u8(source), "SOURCE")]
    report: dict[str, object] = {}
    for name, result in variants.items():
        save_rgba(output / f"{name}.png", result, alpha)
        rendered = np.dstack((result, alpha))
        report[name] = metrics(source, rendered)
        panels.append(add_label(to_u8(rendered), name.replace("_", " ").upper()))
        print(f"{name}: {json.dumps(report[name], sort_keys=True)}")
    if "residual_glow_sequence" in variants:
        report["residual_sequence_vs_current_only"] = {
            "rgb_mae": float(np.mean(np.abs(
                variants["residual_glow_sequence"] -
                variants["residual_current_only_control"]))),
            "interpretation": "isolates history contribution for the supplied four-frame window",
        }

    labeled = [cv2.resize(panel, (480, 270), interpolation=cv2.INTER_AREA) for panel in panels]
    columns = 2
    rows = (len(labeled) + columns - 1) // columns
    blank = np.zeros_like(labeled[0])
    while len(labeled) < rows * columns:
        labeled.append(blank.copy())
    sheet_rows = [np.hstack(labeled[i:i + columns]) for i in range(0, len(labeled), columns)]
    cv2.imwrite(str(output / "glow-suite-contact-sheet.png"), cv2.cvtColor(
        np.vstack(sheet_rows), cv2.COLOR_RGBA2BGRA))

    x0, y0 = source.shape[1] // 4, source.shape[0] // 4
    x1, y1 = source.shape[1] * 3 // 4, source.shape[0] * 3 // 4
    crops = [add_label(to_u8(source[y0:y1, x0:x1]), "SOURCE DETAIL")]
    crops.extend(add_label(to_u8(np.dstack((result[y0:y1, x0:x1], alpha[y0:y1, x0:x1]))),
                             name.upper()) for name, result in variants.items())
    crop_height, crop_width = crops[0].shape[:2]
    crop_rows = []
    for i in range(0, len(crops), 2):
        row = crops[i:i + 2]
        if len(row) == 1:
            row.append(np.zeros((crop_height, crop_width, 4), dtype=np.uint8))
        crop_rows.append(np.hstack(row))
    cv2.imwrite(str(output / "glow-suite-detail-crops.png"), cv2.cvtColor(
        np.vstack(crop_rows), cv2.COLOR_RGBA2BGRA))

    source_info = {
        "path": str(input_path) if input_path else "synthetic neon fixture",
        "frame_index": frame_index if input_path else None,
        "file_sha256": hashlib.sha256(input_path.read_bytes()).hexdigest() if input_path else None,
        "pixel_sha256": hashlib.sha256(source.tobytes()).hexdigest(),
        "luminance_percentiles_input_space": {
            f"p{percentile}": float(np.percentile(
                source[..., :3] @ np.asarray((0.299, 0.587, 0.114), np.float32), percentile))
            for percentile in (90, 95, 99)
        },
        "fraction_luminance_above_0_62": float(np.mean(
            (source[..., :3] @ np.asarray((0.299, 0.587, 0.114), np.float32)) > 0.62)),
    }
    manifest = {
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "classification": "independent source-derived CPU models; not Artifact product output",
        "source": source_info,
        "resolution": [int(source.shape[1]), int(source.shape[0])],
        "color_note": "SDR normalized float input from GlowLab loader; no HDR or display transform study",
        "alpha_note": "source alpha preserved in these still-image models",
        "sequence_frames_for_residual": len(residual_frames),
        "common_strength_scale": strength_scale,
        "models": report,
    }
    (output / "glow-suite-metrics.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Saved source-derived Glow suite captures to {output.resolve()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path,
                        help="Optional still or video; omit for deterministic neon plate")
    parser.add_argument("--frame-index", type=int, default=20)
    parser.add_argument("--strength-scale", type=float, default=0.2,
                        help="common blend factor applied to each full-strength model delta")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "glow-suite")
    args = parser.parse_args()
    if not 0.0 <= args.strength_scale <= 1.0:
        parser.error("--strength-scale must be within [0, 1]")
    render(args.output, args.input, args.frame_index, args.strength_scale)
