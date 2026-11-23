"""Review installed Light Leak CPU captures across exposure and look strength."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label


LOOKS = ("edge-wash", "streak", "organic")
STRENGTHS = (0.15, 0.30)


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


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(input_path: Path, capture_dir: Path, output_dir: Path) -> None:
    source = read_rgba(input_path)
    h, w = source.shape[:2]
    face = np.zeros((h, w), dtype=bool)
    face[int(0.08 * h):int(0.25 * h), int(0.37 * w):int(0.49 * w)] = True
    upper = np.zeros((h, w), dtype=bool)
    upper[:int(0.08 * h), :] = True
    practical = np.zeros((h, w), dtype=bool)
    practical[int(0.08 * h):int(0.28 * h), int(0.63 * w):int(0.82 * w)] = True
    rows: list[dict[str, object]] = []
    panels: list[np.ndarray] = []
    fixture_hashes: set[str] = set()
    source_linear = linear(source[..., :3])

    for strength in STRENGTHS:
        token = f"{strength:.2f}".replace(".", "p")
        fixture_path = capture_dir / f"light-leak-strength-{token}-fixture-input-cpu.png"
        if not fixture_path.exists():
            raise RuntimeError(f"Missing shared fixture capture: {fixture_path}")
        fixture = read_rgba(fixture_path)
        if fixture.shape != source.shape:
            raise RuntimeError("Fixture dimensions do not match the declared +2 stop source")
        fixture_hashes.add(sha256(fixture_path))
        for look in LOOKS:
            output_path = capture_dir / f"light-leak-strength-{token}-{look}-cpu.png"
            state_path = Path(str(output_path) + ".state.txt")
            if not output_path.exists() or not state_path.exists():
                raise RuntimeError(f"Missing output or state sidecar for {output_path}")
            state = state_path.read_text(encoding="utf-8")
            if "compute=0" not in state or "captureSource=Installed CPU reference" not in state:
                raise RuntimeError(f"Unexpected capture provenance in {state_path}")
            result = read_rgba(output_path)
            if result.shape != source.shape:
                raise RuntimeError(f"Unexpected dimensions: {output_path}")
            delta = linear(result[..., :3]) - source_linear
            luma_delta = delta @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
            newly_clipped = (result[..., :3] >= 254.5 / 255.0) & (source[..., :3] < 254.5 / 255.0)
            alpha_mae = float(np.abs(result[..., 3] - source[..., 3]).mean())
            rows.append({
                "look": look,
                "strength": strength,
                "mean_linear_luma_delta_face_roi": float(luma_delta[face].mean()),
                "mean_linear_luma_delta_upper_frame": float(luma_delta[upper].mean()),
                "mean_linear_luma_delta_practical_roi": float(luma_delta[practical].mean()),
                "face_to_practical_delta_ratio": float(luma_delta[face].mean() /
                                                         max(float(luma_delta[practical].mean()), 1e-12)),
                "newly_clipped_channel_fraction_full": float(newly_clipped.mean()),
                "newly_clipped_channel_fraction_face": float(newly_clipped[face].mean()),
                "alpha_mae": alpha_mae,
                "capture_sha256": sha256(output_path),
                "state_sidecar": state_path.name,
            })
            panel = cv2.cvtColor(np.clip(result[..., :3] * 255.0, 0, 255).astype(np.uint8),
                                 cv2.COLOR_RGB2BGR)
            panels.append(add_label(panel, f"{look} / strength {strength:.2f}"))

    if len(fixture_hashes) != 1:
        raise RuntimeError("Strength sweeps did not use the same fixture input")
    output_dir.mkdir(parents=True, exist_ok=True)
    panel_width = 640
    resized = [cv2.resize(panel, (panel_width, round(h * panel_width / w)),
                           interpolation=cv2.INTER_AREA) for panel in panels]
    sheet = np.vstack([np.hstack(resized[i:i + 3]) for i in (0, 3)])
    sheet_path = output_dir / "light-leak-portrait-plus2-contact-sheet.jpg"
    cv2.imwrite(str(sheet_path), sheet, [cv2.IMWRITE_JPEG_QUALITY, 94])
    metrics = {
        "capture_class": "ArtifactUiTest installed Still Light Leak CPU reference",
        "input": str(input_path),
        "input_sha256": sha256(input_path),
        "shared_fixture_capture_sha256": next(iter(fixture_hashes)),
        "dimensions": [w, h],
        "exposure_transform": "linear-light +2 stops; deterministic transform, not a new photograph",
        "face_roi_normalized": [0.37, 0.08, 0.49, 0.25],
        "upper_frame_roi_normalized": [0.0, 0.0, 1.0, 0.08],
        "practical_context_roi_normalized": [0.63, 0.08, 0.82, 0.28],
        "results": rows,
        "limitations": [
            "one manually selected low-light portrait after deterministic exposure transform",
            "rectangular ROIs do not identify semantic masks",
            "CPU installed-effect output, not final Composition Viewer or GPU parity",
            "PNG captures do not establish HDR behavior or effect quality acceptance",
        ],
    }
    metrics_path = output_dir / "light-leak-portrait-plus2-metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {metrics_path}")
    for row in rows:
        print("{look:11s} s={strength:.2f}: face ΔL={mean_linear_luma_delta_face_roi:.5f}, "
              "practical ΔL={mean_linear_luma_delta_practical_roi:.5f}, "
              "clip={newly_clipped_channel_fraction_full:.4%}, "
              "face clip={newly_clipped_channel_fraction_face:.4%}, "
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
