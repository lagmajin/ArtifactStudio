"""Capture soft-knee Glow parameter looks on a deterministic or CC0 still."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np

from glow_lab import (add_label, linear_to_srgb, soft_knee_glow,
                      srgb_to_linear, to_u8)
from light_leak_lab import load_input


LOOKS = (
    ("warm-near", 0.48, 0.22, 0.95, 7.0, (1.0, 0.92, 0.80)),
    ("balanced", 0.45, 0.30, 1.15, 8.0, (1.0, 0.94, 0.86)),
    ("dream-wide", 0.42, 0.38, 1.05, 10.0, (0.84, 0.92, 1.0)),
)


def headroom_glow(source: np.ndarray, threshold: float, knee: float,
                  amount: float, radius: float,
                  tint: tuple[float, float, float]) -> np.ndarray:
    """Keep the additive glow shape while asymptotically preserving display headroom."""
    linear = srgb_to_linear(np.maximum(source[..., :3], 0.0))
    luminance = linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
    knee_width = max(knee, 1.0e-5)
    soft = np.clip((luminance - threshold + knee_width) / (2.0 * knee_width), 0.0, 1.0)
    soft = soft * soft * (3.0 - 2.0 * soft)
    seed = linear * soft[..., None]
    bloom = np.zeros_like(seed)
    for weight, scale in ((0.48, 0.72), (0.30, 1.8), (0.15, 4.2), (0.07, 9.0)):
        sigma = max(0.35, radius * scale)
        bloom += cv2.GaussianBlur(seed, (0, 0), sigmaX=sigma, sigmaY=sigma,
                                  borderType=cv2.BORDER_REPLICATE) * weight
    bloom *= np.asarray(tint, dtype=np.float32)[None, None, :] * amount
    headroom = np.maximum(1.0 - linear, 1.0e-6)
    result = linear + headroom * (1.0 - np.exp(-bloom / headroom))
    return np.dstack((np.clip(linear_to_srgb(result), 0.0, 1.0), source[..., 3]))


def clip_fraction(rgba: np.ndarray) -> float:
    return float(np.mean(rgba[..., :3] >= 0.999))


def new_clip_fraction(source: np.ndarray, result: np.ndarray) -> float:
    newly_clipped = (source[..., :3] < 0.999) & (result[..., :3] >= 0.999)
    return float(np.mean(newly_clipped))


def run(output: Path, input_path: Path | None, frame_index: int) -> None:
    output.mkdir(parents=True, exist_ok=True)
    if input_path:
        source = load_input(input_path, frame_index)
        source = cv2.resize(source, (960, 540), interpolation=cv2.INTER_AREA)
    else:
        from glow_lab import make_fixture
        source = make_fixture()

    panels = [add_label(to_u8(source), "SOURCE")]
    source_linear = srgb_to_linear(np.maximum(source[..., :3], 0.0))
    y, x = np.mgrid[0:source.shape[0], 0:source.shape[1]]
    xn, yn = x / max(source.shape[1] - 1, 1), y / max(source.shape[0] - 1, 1)
    regions = {
        "center": (xn > 0.25) & (xn < 0.75) & (yn > 0.25) & (yn < 0.75),
        "edge": (xn < 0.12) | (xn > 0.88) | (yn < 0.12) | (yn > 0.88),
    }
    source_luma = source_linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
    for name, threshold, knee, amount, radius, tint in LOOKS:
        for method, render in (("additive", soft_knee_glow),
                               ("headroom", headroom_glow)):
            result = render(source, threshold, knee, amount, radius, tint)
            assert np.isfinite(result).all()
            assert np.array_equal(result[..., 3], source[..., 3])
            cv2.imwrite(str(output / f"{name}-{method}.png"), cv2.cvtColor(
                to_u8(result), cv2.COLOR_RGBA2BGRA))
            panels.append(add_label(to_u8(result), f"{name.upper()} / {method.upper()}"))
            result_linear = srgb_to_linear(np.maximum(result[..., :3], 0.0))
            result_luma = result_linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
            print(f"{name}/{method}: output_clip={clip_fraction(result):.4%}, "
                  f"new_clip={new_clip_fraction(source, result):.4%}; " + ", ".join(
                f"{region}_linear_luma_delta={float((result_luma - source_luma)[mask].mean()):.5f}"
                for region, mask in regions.items()))

    sheet = np.vstack([cv2.resize(panel, None, fx=0.5, fy=0.5,
                                  interpolation=cv2.INTER_AREA) for panel in panels])
    cv2.imwrite(str(output / "glow-look-sweep.png"), cv2.cvtColor(
        sheet, cv2.COLOR_RGBA2BGRA))
    print(f"Saved Glow look sweep to {output.resolve()}")
    if input_path:
        print(f"Input: {input_path.resolve()} frame={frame_index}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path,
                        help="Optional still or video frame; omit for the neon fixture")
    parser.add_argument("--frame-index", type=int, default=20)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "glow-look-sweep")
    args = parser.parse_args()
    run(args.output, args.input, args.frame_index)
