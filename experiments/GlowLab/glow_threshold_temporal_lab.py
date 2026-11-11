"""Compare fixed and frame-adaptive highlight extraction on a video clip."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label, to_u8


def extract_and_glow(rgb: np.ndarray, threshold: float) -> tuple[np.ndarray, np.ndarray]:
    luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
    knee = max(0.025, threshold * 0.16)
    ramp = np.clip((luma - (threshold - knee)) / (2.0 * knee), 0.0, 1.0)
    mask = ramp * ramp * (3.0 - 2.0 * ramp)
    seed = rgb * mask[..., None]
    near = cv2.GaussianBlur(seed, (0, 0), 7.0, borderType=cv2.BORDER_REPLICATE)
    broad = cv2.GaussianBlur(seed, (0, 0), 23.0, borderType=cv2.BORDER_REPLICATE)
    glow = (near * 0.75 + broad * 0.25) * 0.85 * 0.2
    return np.clip(rgb + glow, 0.0, 1.0), glow


def run(video_path: Path, output: Path, start_frame: int, frame_count: int,
        percentile: float, ema_seconds: float) -> None:
    output.mkdir(parents=True, exist_ok=True)
    cap = cv2.VideoCapture(str(video_path))
    if not cap.isOpened():
        raise RuntimeError(f"cannot open input video: {video_path}")
    fps = float(cap.get(cv2.CAP_PROP_FPS))
    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    alpha = 1.0 - math.exp(-1.0 / max(1e-4, fps * ema_seconds))
    previous_threshold: float | None = None
    previous_rgb: np.ndarray | None = None
    previous_glow: dict[str, np.ndarray] = {}
    rows: list[dict[str, float | int]] = []
    panels: list[np.ndarray] = []
    sample_stride = max(1, frame_count // 6)

    for offset in range(frame_count):
        ok, bgr = cap.read()
        if not ok:
            break
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        luma = rgb @ np.asarray((0.299, 0.587, 0.114), np.float32)
        raw_threshold = float(np.percentile(luma, percentile))
        previous_threshold = raw_threshold if previous_threshold is None else (
            previous_threshold + alpha * (raw_threshold - previous_threshold))
        thresholds = {
            "fixed_062": 0.62,
            "percentile_raw": raw_threshold,
            "percentile_ema": previous_threshold,
        }
        frame_index = start_frame + offset
        frame_row: dict[str, float | int] = {
            "frame": frame_index,
            "raw_percentile_threshold": raw_threshold,
            "ema_percentile_threshold": previous_threshold,
            "input_luma_p95": float(np.percentile(luma, 95.0)),
        }
        current_glow: dict[str, np.ndarray] = {}
        rendered_by_mode: dict[str, np.ndarray] = {}
        for name, threshold in thresholds.items():
            rendered, glow = extract_and_glow(rgb, threshold)
            current_glow[name] = glow
            rendered_by_mode[name] = rendered
            frame_row[f"{name}_glow_mean"] = float(glow.mean())
            frame_row[f"{name}_new_clip_fraction"] = float(np.mean(
                (rgb < 0.999) & (rendered >= 0.999)))

        if previous_rgb is not None:
            prev_gray = cv2.cvtColor((previous_rgb * 255.0).astype(np.uint8), cv2.COLOR_RGB2GRAY)
            curr_gray = cv2.cvtColor((rgb * 255.0).astype(np.uint8), cv2.COLOR_RGB2GRAY)
            flow = cv2.calcOpticalFlowFarneback(curr_gray, prev_gray, None,
                                                0.5, 3, 15, 3, 5, 1.2, 0)
            yy, xx = np.mgrid[0:rgb.shape[0], 0:rgb.shape[1]].astype(np.float32)
            map_x, map_y = xx + flow[..., 0], yy + flow[..., 1]
            for name, glow in current_glow.items():
                aligned_previous = cv2.remap(previous_glow[name], map_x, map_y,
                                             cv2.INTER_LINEAR,
                                             borderMode=cv2.BORDER_CONSTANT)
                frame_row[f"{name}_flow_aligned_glow_residual"] = float(
                    np.mean(np.abs(glow - aligned_previous)))
        rows.append(frame_row)

        if offset % sample_stride == 0 or offset == frame_count - 1:
            source_rgba = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
            panels.append(add_label(to_u8(source_rgba), f"SOURCE frame {frame_index}"))
            for name, rendered in rendered_by_mode.items():
                result_rgba = np.dstack((rendered, np.ones(rendered.shape[:2], np.float32)))
                panels.append(add_label(to_u8(result_rgba),
                                        f"{name} threshold={thresholds[name]:.3f}"))
        previous_rgb = rgb
        previous_glow = current_glow

    cap.release()
    if not rows:
        raise RuntimeError("no frames were decoded")
    small = [cv2.resize(panel, (480, 270), interpolation=cv2.INTER_AREA) for panel in panels]
    columns = 4
    padded = small + [np.zeros_like(small[0])] * ((-len(small)) % columns)
    contact = np.vstack([np.hstack(padded[i:i + columns])
                         for i in range(0, len(padded), columns)])
    cv2.imwrite(str(output / "threshold-temporal-contact-sheet.png"),
                cv2.cvtColor(contact, cv2.COLOR_RGBA2BGRA))
    metrics = {
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "input_path": str(video_path),
        "input_sha256": hashlib.sha256(video_path.read_bytes()).hexdigest(),
        "frame_range": [start_frame, start_frame + len(rows) - 1],
        "frame_count": len(rows),
        "fps": fps,
        "percentile": percentile,
        "ema_seconds": ema_seconds,
        "ema_alpha_per_frame": alpha,
        "model": "normalized SDR, source-derived soft-knee bloom, common strength scale 0.20",
        "summary": {
            name: {
                "threshold_std": float(np.std([
                    (0.62 if name == "fixed_062" else
                     r["raw_percentile_threshold"] if name == "percentile_raw" else
                     r["ema_percentile_threshold"])
                    for r in rows])),
                "mean_threshold": float(np.mean([
                    (0.62 if name == "fixed_062" else
                     r["raw_percentile_threshold"] if name == "percentile_raw" else
                     r["ema_percentile_threshold"])
                    for r in rows])),
                "mean_glow_mean": float(np.mean([r[f"{name}_glow_mean"] for r in rows])),
                "mean_flow_aligned_residual": float(np.mean([
                    r[f"{name}_flow_aligned_glow_residual"] for r in rows
                    if f"{name}_flow_aligned_glow_residual" in r])),
                "max_new_clip_fraction": float(np.max([
                    r[f"{name}_new_clip_fraction"] for r in rows])),
            }
            for name in thresholds
        },
        "per_frame": rows,
    }
    (output / "threshold-temporal-metrics.json").write_text(
        json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(metrics["summary"], indent=2))
    print(f"Saved threshold temporal captures to {output.resolve()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path,
                        default=Path("tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4"))
    parser.add_argument("--output", type=Path,
                        default=Path("experiments/GlowLab/captures/glow-threshold-temporal-rd1"))
    parser.add_argument("--start-frame", type=int, default=0)
    parser.add_argument("--frame-count", type=int, default=60)
    parser.add_argument("--percentile", type=float, default=95.0)
    parser.add_argument("--ema-seconds", type=float, default=0.4)
    args = parser.parse_args()
    if not 0.0 < args.percentile < 100.0:
        parser.error("--percentile must be between 0 and 100")
    run(args.input, args.output, args.start_frame, args.frame_count,
        args.percentile, args.ema_seconds)
