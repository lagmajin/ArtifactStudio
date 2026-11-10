"""Probe a moving foreground subject under a screen-space flash fill."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from glow_lab import srgb_to_linear, to_u8


def make_background(width: int, height: int) -> np.ndarray:
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    xn, yn = x / max(width - 1, 1), y / max(height - 1, 1)
    base = 0.035 + 0.045 * (1.0 - yn) + 0.025 * (1.0 - xn)
    image = np.stack((base * 0.76, base * 0.89, base * 1.12), axis=-1).astype(np.float32)
    # Brick wall on the left and wet ground below, both stationary in frame.
    brick_color = np.array([0.12, 0.075, 0.055], np.float32)
    wall = (xn < 0.31) & (yn < 0.72)
    image[wall] = brick_color
    for row, yy in enumerate(range(18, int(height * 0.72), 34)):
        offset = 22 if row % 2 else 0
        for xx in range(-offset, int(width * 0.32), 68):
            cv2.line(image, (xx, yy), (xx + 52, yy), (0.20, 0.12, 0.085), 1, cv2.LINE_AA)
            cv2.line(image, (xx, yy), (xx, yy + 28), (0.17, 0.10, 0.075), 1, cv2.LINE_AA)
    ground = yn >= 0.68
    reflections = (np.sin(xn * 88.0 + yn * 13.0) * 0.5 + 0.5) ** 8
    image[ground] += np.stack((0.07 * reflections, 0.075 * reflections,
                               0.085 * reflections), axis=-1)[ground]
    # Practical lights stay fixed in scene coordinates.
    for (cx, cy), color, radius in [((int(width * 0.16), int(height * 0.21)),
                                     (0.95, 0.53, 0.22), 5),
                                    ((int(width * 0.84), int(height * 0.27)),
                                     (0.34, 0.62, 0.95), 4)]:
        glow = np.exp(-(((x - cx) / (radius * 7)) ** 2 + ((y - cy) / (radius * 7)) ** 2))
        image += glow[..., None] * np.asarray(color, np.float32) * 0.16
        cv2.circle(image, (cx, cy), radius, color, -1, cv2.LINE_AA)
    return np.clip(image, 0.0, 1.0)


def composite_subject(background: np.ndarray, center_x: float, frame_index: int) -> tuple[np.ndarray, np.ndarray]:
    height, width = background.shape[:2]
    layer = np.zeros_like(background)
    matte = np.zeros((height, width), np.float32)
    cx, ground_y = int(round(center_x * width)), int(height * 0.79)
    # Soft ground contact shadow, then a compact human silhouette with stable
    # texture marks that move with the body rather than the flash field.
    shadow = np.exp(-(((np.arange(width)[None, :] - cx) / 32.0) ** 2 +
                       ((np.arange(height)[:, None] - ground_y) / 8.0) ** 2))
    result = background.copy() * (1.0 - shadow[..., None] * 0.55)
    cv2.ellipse(matte, (cx, ground_y - 4), (25, 7), 0, 0, 360, 0.9, -1, cv2.LINE_AA)
    # Body, coat, head, arms, and legs.
    cv2.ellipse(layer, (cx, ground_y - 96), (24, 57), 0, 0, 360,
                (0.075, 0.09, 0.12), -1, cv2.LINE_AA)
    cv2.circle(layer, (cx, ground_y - 166), 16, (0.42, 0.29, 0.22), -1, cv2.LINE_AA)
    cv2.ellipse(layer, (cx - 17, ground_y - 92), (9, 49), -9, 0, 360,
                (0.085, 0.095, 0.12), -1, cv2.LINE_AA)
    cv2.ellipse(layer, (cx + 18, ground_y - 92), (9, 49), 9, 0, 360,
                (0.085, 0.095, 0.12), -1, cv2.LINE_AA)
    step = int(6 * np.sin(frame_index * 0.6))
    cv2.line(layer, (cx - 8, ground_y - 54), (cx - 13 + step, ground_y - 5),
             (0.055, 0.065, 0.085), 11, cv2.LINE_AA)
    cv2.line(layer, (cx + 8, ground_y - 54), (cx + 14 - step, ground_y - 5),
             (0.06, 0.07, 0.09), 11, cv2.LINE_AA)
    # Broad moving edge highlights make orientation visible without a flat mask.
    cv2.line(layer, (cx - 16, ground_y - 135), (cx - 20, ground_y - 90),
             (0.16, 0.14, 0.12), 2, cv2.LINE_AA)
    body_mask = np.zeros((height, width), np.float32)
    cv2.ellipse(body_mask, (cx, ground_y - 96), (25, 60), 0, 0, 360, 1.0, -1, cv2.LINE_AA)
    cv2.circle(body_mask, (cx, ground_y - 166), 17, 1.0, -1, cv2.LINE_AA)
    cv2.ellipse(body_mask, (cx - 17, ground_y - 92), (10, 50), -9, 0, 360, 1.0, -1, cv2.LINE_AA)
    cv2.ellipse(body_mask, (cx + 18, ground_y - 92), (10, 50), 9, 0, 360, 1.0, -1, cv2.LINE_AA)
    cv2.line(body_mask, (cx - 8, ground_y - 54), (cx - 13 + step, ground_y - 5), 1.0, 12, cv2.LINE_AA)
    cv2.line(body_mask, (cx + 8, ground_y - 54), (cx + 14 - step, ground_y - 5), 1.0, 12, cv2.LINE_AA)
    result = result * (1.0 - body_mask[..., None]) + layer * body_mask[..., None]
    matte = np.maximum(matte, body_mask)
    return np.dstack((np.clip(result, 0.0, 1.0), np.ones((height, width), np.float32))), matte


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(rgb)
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "motion")
    parser.add_argument("--frames", type=int, default=21)
    parser.add_argument("--duration-seconds", type=float, default=1.0)
    args = parser.parse_args()
    if args.frames < 2 or args.duration_seconds <= 0.0:
        parser.error("frames must be at least 2 and duration must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    width, height = 640, 360
    background = make_background(width, height)
    times = np.linspace(0.0, 1.0, args.frames, dtype=np.float32)
    sample_indices = sorted(set(int(round(t * (args.frames - 1))) for t in (0, 0.25, 0.5, 0.75, 1)))
    xx = np.broadcast_to(np.arange(width, dtype=np.int32)[None, :], (height, width))
    metrics: dict[str, object] = {
        "frames": args.frames,
        "duration_seconds": args.duration_seconds,
        "effective_fps": (args.frames - 1) / args.duration_seconds,
        "source_position": [0.08, 0.5],
        "subject_center_x_start_end": [0.31, 0.69],
        "samples": [],
    }
    preview_panels = []
    for index, progress in enumerate(times):
        center_x = 0.31 + 0.38 * float(progress)
        frame, subject_matte = composite_subject(background, center_x, index)
        effect = flash_fill(
            frame, float(progress), mode="radial", source_position=(0.08, 0.5),
            strength=4.6, softness=0.20, attack_duration=0.72, peak_time=0.76,
            decay_rate=7.0 * args.duration_seconds, scatter_strength=1.0,
            global_fill=0.055, front_duration=0.70, radial_falloff=2.5)
        assert np.isfinite(effect).all()
        assert np.array_equal(effect[..., 3], frame[..., 3])
        frame_bgra = cv2.cvtColor(to_u8(frame), cv2.COLOR_RGBA2BGRA)
        effect_bgra = cv2.cvtColor(to_u8(effect), cv2.COLOR_RGBA2BGRA)
        cv2.imwrite(str(args.output / f"source-frame-{index:04d}.png"), frame_bgra)
        cv2.imwrite(str(args.output / f"flash-frame-{index:04d}.png"), effect_bgra)
        source_luma = linear_luma(frame[..., :3])
        output_luma = linear_luma(effect[..., :3])
        subject = subject_matte > 0.5
        background_region = (subject_matte < 0.02) & (xx > width * 0.74)
        static_frame, static_matte = composite_subject(background, 0.50, index)
        static_effect = flash_fill(
            static_frame, float(progress), mode="radial", source_position=(0.08, 0.5),
            strength=4.6, softness=0.20, attack_duration=0.72, peak_time=0.76,
            decay_rate=7.0 * args.duration_seconds, scatter_strength=1.0,
            global_fill=0.055, front_duration=0.70, radial_falloff=2.5)
        assert np.isfinite(static_effect).all()
        assert np.array_equal(static_effect[..., 3], static_frame[..., 3])
        static_source_luma = linear_luma(static_frame[..., :3])
        static_output_luma = linear_luma(static_effect[..., :3])
        static_subject = static_matte > 0.5
        metrics["samples"].append({
            "frame": index,
            "time_seconds": float(progress) * args.duration_seconds,
            "subject_center_x": center_x,
            "subject_source_mean_luma": float(source_luma[subject].mean()),
            "subject_effect_mean_luma": float(output_luma[subject].mean()),
            "subject_flash_gain_stops": float(np.log2(max(output_luma[subject].mean(), 1e-6) /
                                                       max(source_luma[subject].mean(), 1e-6))),
            "static_subject_center_x": 0.50,
            "static_subject_flash_gain_stops": float(np.log2(max(static_output_luma[static_subject].mean(), 1e-6) /
                                                              max(static_source_luma[static_subject].mean(), 1e-6))),
            "fixed_background_flash_gain_stops": float(np.log2(max(output_luma[background_region].mean(), 1e-6) /
                                                                max(source_luma[background_region].mean(), 1e-6))),
        })
        if index in sample_indices:
            for title, image in (("MOTION / SOURCE", frame_bgra), ("MOTION / FLASH", effect_bgra)):
                thumb = cv2.resize(image, (480, 270), interpolation=cv2.INTER_AREA)
                header = np.full((36, 480, 4), (22, 27, 37, 255), dtype=np.uint8)
                cv2.putText(header, f"{title}  {float(progress) * args.duration_seconds:.2f}s", (10, 24),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.43, (245, 245, 245, 255), 1, cv2.LINE_AA)
                preview_panels.append(np.vstack((header, thumb)))
    # Arrange each time as a source/flash pair, with time increasing left to right.
    pairs = [np.hstack((preview_panels[i], preview_panels[i + 1]))
             for i in range(0, len(preview_panels), 2)]
    cv2.imwrite(str(args.output / "flash-moving-subject-comparison.png"), np.hstack(pairs))
    with (args.output / "flash-moving-subject-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(metrics, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved moving-subject flash study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
