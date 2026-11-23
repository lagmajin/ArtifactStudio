"""Review registered Still Light Leak streak placement across source X values."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


def read_rgba(path: Path) -> np.ndarray:
    raw = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if raw is None:
        raise RuntimeError(f"Could not read image: {path}")
    if raw.ndim == 2:
        rgb = cv2.cvtColor(raw, cv2.COLOR_GRAY2RGB)
        alpha = np.full(raw.shape, 255, dtype=np.uint8)
        return np.dstack((rgb, alpha)).astype(np.float32) / 255.0
    if raw.shape[2] == 3:
        rgb = cv2.cvtColor(raw, cv2.COLOR_BGR2RGB)
        alpha = np.full(raw.shape[:2], 255, dtype=np.uint8)
        return np.dstack((rgb, alpha)).astype(np.float32) / 255.0
    return cv2.cvtColor(raw, cv2.COLOR_BGRA2RGBA).astype(np.float32) / 255.0


def linear(rgb: np.ndarray) -> np.ndarray:
    return np.where(rgb <= 0.04045, rgb / 12.92,
                    ((rgb + 0.055) / 1.055) ** 2.4)


def luma(rgb: np.ndarray) -> np.ndarray:
    return linear(rgb) @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(input_path: Path, capture_dir: Path, output_dir: Path) -> None:
    positions = (0.01, 0.25, 0.50, 0.75, 0.99)
    input_rgba = read_rgba(input_path)
    h, w = input_rgba.shape[:2]
    face = np.zeros((h, w), dtype=bool)
    face[int(0.08 * h):int(0.25 * h), int(0.37 * w):int(0.49 * w)] = True
    upper = np.zeros((h, w), dtype=bool)
    upper[:int(0.08 * h), :] = True
    panels: list[np.ndarray] = []
    records: list[dict[str, object]] = []
    source_hashes: set[str] = set()

    for x in positions:
        safe = f"{x:.2f}".replace(".", "p")
        fixture_path = capture_dir / f"source-x-{safe}-fixture-input-cpu.png"
        output_path = capture_dir / f"source-x-{safe}-streak-cpu.png"
        state_path = Path(str(output_path) + ".state.txt")
        if not fixture_path.exists() or not output_path.exists() or not state_path.exists():
            raise RuntimeError(f"Missing capture or state sidecar for source X={x:.2f}")
        fixture = read_rgba(fixture_path)
        result = read_rgba(output_path)
        if fixture.shape != result.shape or fixture.shape[:2] != (h, w):
            raise RuntimeError(f"Unexpected capture dimensions at source X={x:.2f}")
        state = state_path.read_text(encoding="utf-8")
        if "compute=0" not in state or "captureSource=Installed CPU reference" not in state:
            raise RuntimeError(f"Capture state is not the expected CPU reference: {state_path}")
        source_hashes.add(sha256(fixture_path))
        delta = luma(result[..., :3]) - luma(fixture[..., :3])
        newly_clipped = (result[..., :3] >= 254.5 / 255.0) & (fixture[..., :3] < 254.5 / 255.0)
        alpha_delta = np.abs(result[..., 3] - fixture[..., 3])
        panels.append(add_label(cv2.cvtColor(
            np.clip(result[..., :3] * 255.0, 0, 255).astype(np.uint8),
            cv2.COLOR_RGB2BGR), f"SOURCE X={x:.2f}  Y=.015  ANGLE=2.58 DEG"))
        records.append({
            "source_x": x,
            "source_y": 0.015,
            "strength": 0.35,
            "angle_degrees": 2.58,
            "mean_linear_luma_delta_face_roi": float(delta[face].mean()),
            "mean_linear_luma_delta_upper_frame": float(delta[upper].mean()),
            "face_to_upper_delta_ratio": float(delta[face].mean() /
                                                  max(float(delta[upper].mean()), 1e-12)),
            "newly_clipped_channel_fraction_full": float(newly_clipped.mean()),
            "newly_clipped_channel_fraction_face": float(newly_clipped[face].mean()),
            "alpha_mae": float(alpha_delta.mean()),
            "capture_sha256": sha256(output_path),
            "state_sidecar": state_path.name,
        })

    if len(source_hashes) != 1:
        raise RuntimeError("The five captures do not share an identical fixture-input hash")
    output_dir.mkdir(parents=True, exist_ok=True)
    resized = [cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
               for panel in panels]
    sheet_path = output_dir / "light-leak-streak-source-x-contact-sheet.jpg"
    cv2.imwrite(str(sheet_path), np.vstack(resized), [cv2.IMWRITE_JPEG_QUALITY, 94])
    metrics = {
        "capture_class": "ArtifactUiTest installed Still Light Leak CPU reference",
        "input": str(input_path),
        "input_sha256": sha256(input_path),
        "shared_fixture_capture_sha256": next(iter(source_hashes)),
        "dimensions": [w, h],
        "face_roi_normalized": [0.37, 0.08, 0.49, 0.25],
        "upper_frame_roi_normalized": [0.0, 0.0, 1.0, 0.08],
        "results": records,
        "limitations": [
            "single manually selected portrait and rectangular face ROI",
            "CPU installed-effect output, not final Composition Viewer or GPU parity",
            "placement results do not define a universal default",
        ],
    }
    metrics_path = output_dir / "light-leak-streak-source-x-metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {metrics_path}")
    for row in records:
        print("x={source_x:.2f}: face ΔL={mean_linear_luma_delta_face_roi:.6f}, "
              "upper ΔL={mean_linear_luma_delta_upper_frame:.6f}, "
              "ratio={face_to_upper_delta_ratio:.3f}, clip={newly_clipped_channel_fraction_full:.5%}, "
              "alpha MAE={alpha_mae:.6f}".format(**row))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.input, args.captures, args.output)


if __name__ == "__main__":
    main()
