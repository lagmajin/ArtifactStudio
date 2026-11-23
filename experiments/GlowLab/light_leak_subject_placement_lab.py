"""Compare anamorphic light-leak placement against a protected subject ROI.

This is an independent source-derived image model, not Artifact product output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label, to_u8
from light_leak_lab import anamorphic_streak, load_input


def mean_linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = np.where(rgb <= 0.04045, rgb / 12.92,
                      ((rgb + 0.055) / 1.055) ** 2.4)
    return linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)


def run(input_path: Path, output_dir: Path, face_roi: tuple[float, float, float, float],
        positions: tuple[float, ...], strength: float, angle: float) -> None:
    source = np.clip(load_input(input_path, 0), 0.0, 1.0).astype(np.float32)
    height, width = source.shape[:2]
    x0, y0, x1, y1 = face_roi
    if not (0 <= x0 < x1 <= 1 and 0 <= y0 < y1 <= 1):
        raise ValueError("face ROI must be normalized as x0 y0 x1 y1")
    face_mask = np.zeros((height, width), dtype=bool)
    face_mask[int(y0 * height):int(y1 * height), int(x0 * width):int(x1 * width)] = True
    top_mask = np.zeros_like(face_mask)
    top_mask[:max(1, int(y0 * height)), :] = True

    output_dir.mkdir(parents=True, exist_ok=True)
    source_luma = mean_linear_luma(source[..., :3])
    panels = [add_label(to_u8(source), "SOURCE")]
    records: list[dict[str, object]] = []
    for source_y in positions:
        result = anamorphic_streak(source, strength, (0.01, source_y), angle)
        delta = mean_linear_luma(result[..., :3]) - source_luma
        label = f"SOURCE Y={source_y:.2f}  ANGLE={angle:.1f} DEG"
        safe = f"{source_y:.3f}".replace(".", "p")
        out_path = output_dir / f"streak-source-y-{safe}.png"
        cv2.imwrite(str(out_path), cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))
        panels.append(add_label(to_u8(result), label))
        records.append({
            "source_position": [0.01, float(source_y)],
            "angle_degrees": float(angle),
            "strength": float(strength),
            "mean_linear_luma_delta_face_roi": float(delta[face_mask].mean()),
            "mean_linear_luma_delta_upper_frame": float(delta[top_mask].mean()),
            "face_roi_to_upper_delta_ratio": float(
                delta[face_mask].mean() / max(float(delta[top_mask].mean()), 1e-9)),
            "newly_clipped_channel_fraction": float(
                np.count_nonzero((result[..., :3] >= 0.999) &
                                 (source[..., :3] < 0.999)) / (height * width * 3)),
            "alpha_preserved": bool(np.array_equal(result[..., 3], source[..., 3])),
            "capture": out_path.name,
        })

    panels = [cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
              for panel in panels]
    sheet = np.vstack(panels)
    sheet_path = output_dir / "light-leak-subject-placement.png"
    cv2.imwrite(str(sheet_path), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))
    source_hash = hashlib.sha256(input_path.read_bytes()).hexdigest()
    metadata = {
        "model": "independent source-derived anamorphic streak; not product output",
        "input": str(input_path),
        "input_sha256": source_hash,
        "input_dimensions": [width, height],
        "face_roi_normalized": list(face_roi),
        "results": records,
    }
    (output_dir / "metrics.json").write_text(json.dumps(metadata, indent=2) + "\n",
                                               encoding="utf-8")
    print(f"Saved {sheet_path}")
    for item in records:
        print(f"y={item['source_position'][1]:.2f}: face ΔL={item['mean_linear_luma_delta_face_roi']:.6f}, "
              f"upper ΔL={item['mean_linear_luma_delta_upper_frame']:.6f}, "
              f"ratio={item['face_roi_to_upper_delta_ratio']:.3f}, "
              f"clip={item['newly_clipped_channel_fraction']:.5%}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path,
                        default=Path("experiments/GlowLab/assets/flash-photo-plate.png"))
    parser.add_argument("--output", type=Path,
                        default=Path("experiments/GlowLab/captures/light-leak-placement-portrait-rd1"))
    parser.add_argument("--face-roi", type=float, nargs=4, default=(0.37, 0.08, 0.49, 0.25),
                        metavar=("X0", "Y0", "X1", "Y1"))
    parser.add_argument("--source-y", type=float, nargs="+", default=(0.01, 0.06, 0.11, 0.16, 0.21))
    parser.add_argument("--strength", type=float, default=0.35)
    parser.add_argument("--angle", type=float, default=2.58)
    args = parser.parse_args()
    run(args.input, args.output, tuple(args.face_roi), tuple(args.source_y),
        args.strength, args.angle)


if __name__ == "__main__":
    main()
