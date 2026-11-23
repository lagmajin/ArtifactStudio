"""Create a face-area comparison sheet for restrained product Glow captures."""

from __future__ import annotations

import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label
from glow_role_intensity_review import linear_luma, read_rgba


ROOT = Path(__file__).resolve().parents[2]
CAPTURES = ROOT / "experiments/GlowLab/captures"
ROI = (0.32, 0.02, 0.55, 0.30)

CASES = [
    ("Edge Bloom | Edge Boost .15", "glow-suite-edgebloom-edgeboost-rd1",
     "app-portrait-edgeboost-0p15-edge_bloom-cpu"),
    ("Edge Bloom | Edge Boost .30", "glow-suite-edgebloom-edgeboost-rd1",
     "app-portrait-edgeboost-0p30-edge_bloom-cpu"),
    ("Reactive Glow | restrained", "glow-suite-restrained-portrait-cpu-rd1",
     "app-reactive_glow-restrained-reactive_glow-cpu"),
    ("4-ray Glint | restrained", "glow-suite-restrained-portrait-cpu-rd1",
     "app-builtin.glint_star_filter-restrained-builtin.glint_star_filter-cpu"),
    ("Optical Glow | primary", "glow-suite-restrained-portrait-cpu-rd1",
     "app-optical-primary-only-builtin.optical_glow-cpu"),
    ("Optical Glow | secondary", "glow-suite-restrained-portrait-cpu-rd1",
     "app-optical-secondary-only-builtin.optical_glow-cpu"),
    ("Optical Glow | restrained stack", "glow-suite-restrained-portrait-cpu-rd1",
     "app-optical-restrained-stack-builtin.optical_glow-cpu"),
]


def crop(image: np.ndarray) -> np.ndarray:
    height, width = image.shape[:2]
    x0, y0, x1, y1 = ROI
    return image[int(y0 * height):int(y1 * height),
                 int(x0 * width):int(x1 * width)]


def main() -> None:
    output_dir = CAPTURES / "glow-suite-restrained-portrait-cpu-rd1"
    panels: list[np.ndarray] = []
    metrics: list[dict[str, object]] = []
    target_width = 420

    for title, folder, stem in CASES:
        capture_dir = CAPTURES / folder
        source_path = capture_dir / f"{stem}-fixture-input.png"
        result_path = capture_dir / f"{stem}.png"
        source = crop(read_rgba(source_path))
        result = crop(read_rgba(result_path))
        if source.shape != result.shape:
            raise RuntimeError(f"crop size mismatch for {stem}")

        source_rgb = source[..., :3].astype(np.float32) / 255.0
        result_rgb = result[..., :3].astype(np.float32) / 255.0
        delta = linear_luma(result[..., :3]) - linear_luma(source[..., :3])
        newly_clipped = ((result[..., :3] >= 254) & (source[..., :3] < 254))
        diff = np.clip(np.abs(result.astype(np.float32) - source.astype(np.float32)) * 5,
                       0, 255).astype(np.uint8)
        panels_for_case = []
        for image, label in ((source, "INPUT CROP"), (result, "PRODUCT CROP"),
                             (diff, "ABS DIFF x5")):
            scaled_height = round(image.shape[0] * target_width / image.shape[1])
            scaled = cv2.resize(image, (target_width, scaled_height),
                                interpolation=cv2.INTER_AREA)
            panels_for_case.append(add_label(scaled, label))
        row = np.hstack(panels_for_case)
        panels.append(add_label(row,
            f"{title} | ROI dL={float(delta.mean()):.5f} "
            f"clip={float(newly_clipped.mean()):.3%}"))
        metrics.append({
            "profile": title,
            "roi_fraction": list(ROI),
            "crop_dimensions": [int(source.shape[1]), int(source.shape[0])],
            "encoded_rgb_mae": float(np.abs(result_rgb - source_rgb).mean()),
            "linear_luma_delta_mean": float(delta.mean()),
            "linear_luma_delta_p95": float(np.percentile(delta, 95)),
            "newly_clipped_channel_fraction": float(newly_clipped.mean()),
            "alpha_mae": float(np.abs(result[..., 3].astype(np.int16) -
                                      source[..., 3].astype(np.int16)).mean() / 255.0),
        })

    sheet_path = output_dir / "portrait-face-detail-contact-sheet.png"
    metrics_path = output_dir / "portrait-face-detail-metrics.json"
    sheet = np.vstack(panels)
    if not cv2.imwrite(str(sheet_path), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write contact sheet: {sheet_path}")
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "roi_fraction": list(ROI),
        "records": metrics,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path} and {metrics_path}")
    for metric in metrics:
        print(f"{metric['profile']}: dL={metric['linear_luma_delta_mean']:.5f}, "
              f"clip={metric['newly_clipped_channel_fraction']:.3%}, "
              f"alphaMAE={metric['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
