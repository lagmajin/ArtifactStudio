"""Review installed Still Light Leak angle captures on a street-light plate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


ANGLES = (-24, -12, 0, 12, 24)


def read_rgba(path: Path) -> np.ndarray:
    raw = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if raw is None:
        raise RuntimeError(f"Could not read {path}")
    if raw.ndim == 2:
        rgb = cv2.cvtColor(raw, cv2.COLOR_GRAY2RGB)
        alpha = np.full(raw.shape, 255, dtype=np.uint8)
        return np.dstack((rgb, alpha)).astype(np.float32) / 255.0
    if raw.shape[2] == 3:
        rgb = cv2.cvtColor(raw, cv2.COLOR_BGR2RGB)
        alpha = np.full(raw.shape[:2], 255, dtype=np.uint8)
        return np.dstack((rgb, alpha)).astype(np.float32) / 255.0
    return cv2.cvtColor(raw, cv2.COLOR_BGRA2RGBA).astype(np.float32) / 255.0


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = np.where(rgb <= 0.04045, rgb / 12.92,
                      ((rgb + 0.055) / 1.055) ** 2.4)
    return linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stem_for(angle: int) -> str:
    return f"angle-{'m' + str(abs(angle)) if angle < 0 else 'p' + str(angle)}"


def run(captures: Path, output: Path) -> None:
    source_hashes: set[str] = set()
    panels: list[np.ndarray] = []
    records: list[dict[str, object]] = []
    for angle in ANGLES:
        stem = stem_for(angle)
        source_path = captures / f"{stem}-fixture-input-cpu.png"
        result_path = captures / f"{stem}-streak-cpu.png"
        state_path = Path(str(result_path) + ".state.txt")
        if not all(path.is_file() for path in (source_path, result_path, state_path)):
            raise RuntimeError(f"Missing source/output/state for {angle} degrees")
        source_hashes.add(sha256(source_path))
        source = read_rgba(source_path)
        result = read_rgba(result_path)
        if source.shape != result.shape:
            raise RuntimeError(f"Input/output dimensions differ at {angle} degrees")
        state = state_path.read_text(encoding="utf-8")
        if "compute=0" not in state or "captureSource=Installed CPU reference" not in state:
            raise RuntimeError(f"Unexpected capture type: {state_path}")
        delta = linear_luma(result[..., :3]) - linear_luma(source[..., :3])
        h, w = delta.shape
        lamp_roi = (slice(int(0.055 * h), int(0.19 * h)),
                    slice(int(0.20 * w), int(0.80 * w)))
        background_roi = (slice(int(0.19 * h), int(0.36 * h)),
                          slice(int(0.20 * w), int(0.80 * w)))
        newly_clipped = ((result[..., :3] >= 254.5 / 255.0) &
                         (source[..., :3] < 254.5 / 255.0))
        alpha_delta = np.abs(result[..., 3] - source[..., 3])
        display = np.clip(result[..., :3] * 255.0, 0, 255).astype(np.uint8)
        panels.append(add_label(cv2.cvtColor(display, cv2.COLOR_RGB2BGR),
                                f"STREAK ANGLE {angle:+d} DEG  source=(.50,.12)  strength=.25"))
        records.append({
            "angle_degrees": angle,
            "source_position": [0.50, 0.12],
            "strength": 0.25,
            "lamp_band_mean_linear_luma_delta": float(delta[lamp_roi].mean()),
            "background_band_mean_linear_luma_delta": float(delta[background_roi].mean()),
            "newly_clipped_channel_fraction_full": float(newly_clipped.mean()),
            "newly_clipped_channel_fraction_lamp_band": float(newly_clipped[lamp_roi].mean()),
            "alpha_mae": float(alpha_delta.mean()),
            "output_sha256": sha256(result_path),
            "state_sidecar": state_path.name,
        })
    if len(source_hashes) != 1:
        raise RuntimeError("Angle captures do not share one identical fixture source")

    output.mkdir(parents=True, exist_ok=True)
    panels = [cv2.resize(panel, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA)
              for panel in panels]
    sheet_path = output / "light-leak-streetlamp-angle-contact-sheet.jpg"
    cv2.imwrite(str(sheet_path), np.vstack(panels), [cv2.IMWRITE_JPEG_QUALITY, 94])
    report = {
        "capture_class": "ArtifactUiTest installed Still Light Leak CPU reference",
        "look": "Streak",
        "input": "experiments/GlowLab/assets/cc0-street-light-night.jpg",
        "input_dimensions": [576, 1280],
        "shared_fixture_input_sha256": next(iter(source_hashes)),
        "fixed_strength": 0.25,
        "fixed_source_position": [0.50, 0.12],
        "lamp_band_roi_normalized": [0.20, 0.055, 0.80, 0.19],
        "background_band_roi_normalized": [0.20, 0.19, 0.80, 0.36],
        "results": records,
        "limitations": [
            "single CC0 street-light photograph and manually defined rectangular ROIs",
            "installed CPU reference output, not final Viewer, HDR, or GPU parity",
            "ROI luminance does not measure visual alignment or aesthetic quality",
        ],
    }
    metrics_path = output / "light-leak-streetlamp-angle-metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {metrics_path}")
    for row in records:
        print("angle={angle_degrees:+d}: lamp ΔL={lamp_band_mean_linear_luma_delta:.6f}, "
              "background ΔL={background_band_mean_linear_luma_delta:.6f}, "
              "clip={newly_clipped_channel_fraction_full:.5%}, "
              "alpha MAE={alpha_mae:.6f}".format(**row))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.captures, args.output)


if __name__ == "__main__":
    main()
