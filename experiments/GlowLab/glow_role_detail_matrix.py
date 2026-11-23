"""Review every registered Glow role on matched portrait and neon details."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_lab import add_label
from glow_role_intensity_review import linear_luma, read_rgba


ROOT = Path(__file__).resolve().parents[2]
CAPTURE_ROOT = ROOT / "experiments/GlowLab/captures"
PROFILE_PATH = ROOT / "experiments/GlowLab/glow_role_candidate_profiles.json"
PLATES = {
    "portrait": {
        "capture_dir": "glow-suite-role-candidates-portrait-cpu-rd1",
        "rois": {
            "face": (0.37, 0.08, 0.49, 0.25),
        },
    },
    "neon": {
        "capture_dir": "glow-suite-role-candidates-neon-cpu-rd1",
        "rois": {
            "magenta_ring": (0.15, 0.18, 0.40, 0.58),
            "emissive_text": (0.36, 0.35, 0.58, 0.54),
            "compact_cyan_practical": (0.70, 0.28, 0.82, 0.48),
            "compact_amber_practical": (0.74, 0.43, 0.84, 0.56),
            "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
        },
    },
    "walkway": {
        "capture_dir": "glow-suite-role-candidates-walkway-cpu-rd1",
        "rois": {
            "bright_sky": (0.02, 0.02, 0.98, 0.27),
            "bridge_span": (0.02, 0.40, 0.98, 0.69),
            "ice_and_water": (0.02, 0.70, 0.98, 0.98),
            "dark_water": (0.30, 0.72, 0.66, 0.92),
        },
    },
    "walkway_highkey": {
        "capture_dir": "glow-suite-highkey-restraint-walkway-cpu-rd1",
        "rois": {
            "bright_sky": (0.02, 0.02, 0.98, 0.27),
            "bridge_span": (0.02, 0.40, 0.98, 0.69),
            "ice_and_water": (0.02, 0.70, 0.98, 0.98),
            "dark_water": (0.30, 0.72, 0.66, 0.92),
        },
    },
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture_stem(effect_id: str, plate_name: str | None = None) -> str:
    effect_key = effect_id.replace(".", "_")
    if plate_name == "walkway":
        return f"app-role-walkway-{effect_key}-{effect_id}-cpu"
    if plate_name == "walkway_highkey":
        return f"app-role-highkey-{effect_key}-{effect_id}-cpu"
    return f"app-role-{effect_key}-{effect_id}-cpu"


def normalized_crop(image: np.ndarray, rect: tuple[float, float, float, float]) -> np.ndarray:
    height, width = image.shape[:2]
    x0, y0, x1, y1 = rect
    left, top = int(round(x0 * width)), int(round(y0 * height))
    right, bottom = int(round(x1 * width)), int(round(y1 * height))
    if left < 0 or top < 0 or right > width or bottom > height or left >= right or top >= bottom:
        raise ValueError(f"invalid ROI {rect} for {width}x{height}")
    return image[top:bottom, left:right]


def record_metrics(source: np.ndarray, result: np.ndarray) -> dict[str, float]:
    source_rgb = source[..., :3].astype(np.float32) / 255.0
    result_rgb = result[..., :3].astype(np.float32) / 255.0
    delta = linear_luma(result[..., :3]) - linear_luma(source[..., :3])
    newly_clipped = (result[..., :3] >= 254) & (source[..., :3] < 254)
    changed = np.abs(result[..., :3].astype(np.int16) -
                     source[..., :3].astype(np.int16)) > 1
    return {
        "encoded_rgb_mae": float(np.abs(result_rgb - source_rgb).mean()),
        "linear_luma_delta_mean": float(delta.mean()),
        "linear_luma_delta_p95": float(np.percentile(delta, 95)),
        "linear_luma_delta_max": float(delta.max()),
        "changed_channel_fraction_gt_1_255": float(changed.mean()),
        "newly_clipped_channel_fraction": float(newly_clipped.mean()),
        "alpha_mae": float(np.abs(result[..., 3].astype(np.int16) -
                                  source[..., 3].astype(np.int16)).mean() / 255.0),
    }


def make_sheet(records: list[dict[str, object]], output_path: Path) -> None:
    rows: list[np.ndarray] = []
    first_source = records[0]["source_crop"]
    assert isinstance(first_source, np.ndarray)
    sheet_width = first_source.shape[1] * 3
    heading = np.zeros((42, sheet_width, 4), dtype=np.uint8)
    heading[..., :3] = (7, 9, 15)
    heading[..., 3] = 255
    cv2.putText(heading, "INPUT 1:1 | PRODUCT 1:1 | ABS DIFF x5",
                (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                (230, 235, 245, 255), 1, cv2.LINE_AA)
    rows.append(heading)
    for record in records:
        source = record["source_crop"]
        result = record["result_crop"]
        assert isinstance(source, np.ndarray) and isinstance(result, np.ndarray)
        delta = np.clip(np.abs(result.astype(np.float32) -
                               source.astype(np.float32)) * 5.0,
                        0, 255).astype(np.uint8)
        row = np.hstack((source, result, delta))
        metrics = record["metrics"]
        assert isinstance(metrics, dict)
        label = np.zeros((42, sheet_width, 4), dtype=np.uint8)
        label[..., :3] = (7, 9, 15)
        label[..., 3] = 255
        cv2.putText(label,
            f"{record['plate']} | {record['roi']} | {record['effect']} "
            f"| dL={metrics['linear_luma_delta_mean']:.5f} "
            f"clip={metrics['newly_clipped_channel_fraction']:.3%}",
            (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
            (230, 235, 245, 255), 1, cv2.LINE_AA)
        rows.extend((label, row))
    sheet = np.vstack(rows)
    if not cv2.imwrite(str(output_path), cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write sheet: {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path,
                        default=CAPTURE_ROOT / "glow-suite-role-detail-matrix-rd1")
    args = parser.parse_args()

    profiles = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))["profiles"]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    summaries: list[dict[str, object]] = []

    for plate_name, plate in PLATES.items():
        capture_dir = CAPTURE_ROOT / plate["capture_dir"]
        for roi_name, rect in plate["rois"].items():
            records: list[dict[str, object]] = []
            for effect_id in profiles:
                stem = capture_stem(effect_id, plate_name)
                source_path = capture_dir / f"{stem}-fixture-input.png"
                result_path = capture_dir / f"{stem}.png"
                source = read_rgba(source_path)
                result = read_rgba(result_path)
                if source.shape != result.shape:
                    raise RuntimeError(f"input/output dimensions differ: {stem}")
                source_crop = normalized_crop(source, rect).copy()
                result_crop = normalized_crop(result, rect).copy()
                metrics = record_metrics(source_crop, result_crop)
                records.append({
                    "plate": plate_name,
                    "roi": roi_name,
                    "effect": effect_id,
                    "source_crop": source_crop,
                    "result_crop": result_crop,
                    "metrics": metrics,
                })
                summaries.append({
                    "plate": plate_name,
                    "roi": roi_name,
                    "roi_fraction": list(rect),
                    "effect_id": effect_id,
                    "capture_stem": stem,
                    "dimensions": [int(source.shape[1]), int(source.shape[0])],
                    "input_sha256": sha256(source_path),
                    "output_sha256": sha256(result_path),
                    **metrics,
                })
            make_sheet(records, args.output_dir / f"{plate_name}-{roi_name}-contact-sheet.png")

    metrics_path = args.output_dir / "detail-roi-metrics.json"
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "profile": str(PROFILE_PATH.relative_to(ROOT)),
        "difference_gain": 5.0,
        "records": summaries,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved detail sheets and {metrics_path}")
    for row in summaries:
        print(f"{row['plate']}/{row['roi']} {row['effect_id']}: "
              f"dL={row['linear_luma_delta_mean']:.5f}, "
              f"clip={row['newly_clipped_channel_fraction']:.3%}, "
              f"alphaMAE={row['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
