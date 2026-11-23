"""Compare original and high-key Glow role profiles on matched ROIs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_role_detail_matrix import PLATES, normalized_crop, record_metrics
from glow_role_intensity_review import linear_luma, read_rgba


ROOT = Path(__file__).resolve().parents[2]
CAPTURE_ROOT = ROOT / "experiments/GlowLab/captures"
BASELINE_PROFILE = ROOT / "experiments/GlowLab/glow_role_candidate_profiles.json"
HIGHKEY_PROFILE = ROOT / "experiments/GlowLab/glow_highkey_restraint_profiles.json"
BASELINE_DIR = CAPTURE_ROOT / "glow-suite-role-candidates-walkway-cpu-rd1"
HIGHKEY_DIR = CAPTURE_ROOT / "glow-suite-highkey-restraint-walkway-cpu-rd1"
SOURCE_IMAGE = BASELINE_DIR / "source-frame20.png"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture_stem(effect_id: str, highkey: bool) -> str:
    key = effect_id.replace(".", "_")
    prefix = "app-role-highkey" if highkey else "app-role-walkway"
    return f"{prefix}-{key}-{effect_id}-cpu"


def title_strip(width: int, text: str) -> np.ndarray:
    strip = np.zeros((42, width, 4), dtype=np.uint8)
    strip[..., :3] = (7, 9, 15)
    strip[..., 3] = 255
    cv2.putText(strip, text, (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                (230, 235, 245, 255), 1, cv2.LINE_AA)
    return strip


def make_comparison_sheet(records: list[dict[str, object]], path: Path) -> None:
    first = records[0]
    input_crop = first["input_crop"]
    assert isinstance(input_crop, np.ndarray)
    cell_width = input_crop.shape[1]
    sheet_width = cell_width * 5
    rows: list[np.ndarray] = [title_strip(
        sheet_width,
        "INPUT | ORIGINAL ROLE | HIGH-KEY RESTRAINT | ORIGINAL DIFF x2 | RESTRAINED DIFF x2",
    )]
    for record in records:
        source = record["input_crop"]
        original = record["original_crop"]
        restrained = record["restrained_crop"]
        assert all(isinstance(x, np.ndarray) for x in (source, original, restrained))
        assert source.shape == original.shape == restrained.shape
        diff_original = np.clip(np.abs(original.astype(np.float32) -
                                        source.astype(np.float32)) * 2,
                                0, 255).astype(np.uint8)
        diff_restrained = np.clip(np.abs(restrained.astype(np.float32) -
                                         source.astype(np.float32)) * 2,
                                  0, 255).astype(np.uint8)
        metrics = record["metrics"]
        assert isinstance(metrics, dict)
        label = (
            f"{record['roi']} | {record['effect_id']} | "
            f"dL {metrics['original_linear_luma_delta_mean']:.4f} -> "
            f"{metrics['restrained_linear_luma_delta_mean']:.4f} | "
            f"clip {metrics['original_newly_clipped_channel_fraction']:.2%} -> "
            f"{metrics['restrained_newly_clipped_channel_fraction']:.2%}"
        )
        row = np.hstack((source, original, restrained, diff_original, diff_restrained))
        rows.extend((title_strip(sheet_width, label), row))
    path.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(path), cv2.cvtColor(np.vstack(rows), cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write comparison sheet: {path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path,
                        default=CAPTURE_ROOT / "glow-suite-highkey-role-comparison-rd1")
    args = parser.parse_args()
    base_profiles = json.loads(BASELINE_PROFILE.read_text(encoding="utf-8"))["profiles"]
    highkey_profiles = json.loads(HIGHKEY_PROFILE.read_text(encoding="utf-8"))["profiles"]
    if list(base_profiles) != list(highkey_profiles):
        raise RuntimeError("baseline/high-key profile effect order differs")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    source_hashes: set[str] = set()
    summaries: list[dict[str, object]] = []
    walkway_rois = PLATES["walkway"]["rois"]

    for roi_name, rect in walkway_rois.items():
        rows: list[dict[str, object]] = []
        for effect_id in base_profiles:
            original_stem = capture_stem(effect_id, False)
            restrained_stem = capture_stem(effect_id, True)
            original_input_path = BASELINE_DIR / f"{original_stem}-fixture-input.png"
            original_output_path = BASELINE_DIR / f"{original_stem}.png"
            restrained_input_path = HIGHKEY_DIR / f"{restrained_stem}-fixture-input.png"
            restrained_output_path = HIGHKEY_DIR / f"{restrained_stem}.png"
            original_input = read_rgba(original_input_path)
            original_output = read_rgba(original_output_path)
            restrained_input = read_rgba(restrained_input_path)
            restrained_output = read_rgba(restrained_output_path)
            if not (original_input.shape == original_output.shape ==
                    restrained_input.shape == restrained_output.shape):
                raise RuntimeError(f"capture dimensions differ for {effect_id}")
            if not np.array_equal(original_input, restrained_input):
                raise RuntimeError(f"baseline/high-key fixture inputs differ for {effect_id}")
            source_hashes.update((digest(original_input_path), digest(restrained_input_path)))

            source_crop = normalized_crop(original_input, rect).copy()
            original_crop = normalized_crop(original_output, rect).copy()
            restrained_crop = normalized_crop(restrained_output, rect).copy()
            original_metrics = record_metrics(source_crop, original_crop)
            restrained_metrics = record_metrics(source_crop, restrained_crop)
            base_delta = original_metrics["linear_luma_delta_mean"]
            restraint_delta = restrained_metrics["linear_luma_delta_mean"]
            ratio = (restraint_delta / base_delta) if abs(base_delta) > 1e-8 else None
            metrics = {
                "original_linear_luma_delta_mean": base_delta,
                "restrained_linear_luma_delta_mean": restraint_delta,
                "restrained_to_original_delta_ratio": ratio,
                "original_newly_clipped_channel_fraction":
                    original_metrics["newly_clipped_channel_fraction"],
                "restrained_newly_clipped_channel_fraction":
                    restrained_metrics["newly_clipped_channel_fraction"],
                "original_rgb_mae": original_metrics["encoded_rgb_mae"],
                "restrained_rgb_mae": restrained_metrics["encoded_rgb_mae"],
                "alpha_mae_max": max(original_metrics["alpha_mae"],
                                      restrained_metrics["alpha_mae"]),
                "original_luma_delta_p95": original_metrics["linear_luma_delta_p95"],
                "restrained_luma_delta_p95": restrained_metrics["linear_luma_delta_p95"],
            }
            row = {
                "roi": roi_name,
                "effect_id": effect_id,
                "input_crop": source_crop,
                "original_crop": original_crop,
                "restrained_crop": restrained_crop,
                "metrics": metrics,
            }
            rows.append(row)
            summaries.append({
                "plate": "cc0_walkway_frame20",
                "roi": roi_name,
                "roi_fraction": list(rect),
                "effect_id": effect_id,
                "dimensions": [int(original_input.shape[1]), int(original_input.shape[0])],
                "input_sha256": digest(original_input_path),
                "original_output_sha256": digest(original_output_path),
                "restrained_input_sha256": digest(restrained_input_path),
                "restrained_output_sha256": digest(restrained_output_path),
                **metrics,
            })
        make_comparison_sheet(rows,
                              args.output_dir / f"{roi_name}-original-vs-highkey.png")

    if len(source_hashes) != 1:
        raise RuntimeError(f"expected one shared app input hash, found {len(source_hashes)}")
    metrics_path = args.output_dir / "original-vs-highkey-metrics.json"
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "original_profile": str(BASELINE_PROFILE.relative_to(ROOT)),
        "restrained_profile": str(HIGHKEY_PROFILE.relative_to(ROOT)),
        "input_frame_metadata": str((BASELINE_DIR / "source-frame20.json").relative_to(ROOT)),
        "shared_app_fixture_input_sha256": next(iter(source_hashes)),
        "records": summaries,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved comparison sheets and {metrics_path}")
    for row in summaries:
        print(f"{row['roi']} {row['effect_id']}: "
              f"dL {row['original_linear_luma_delta_mean']:.5f} -> "
              f"{row['restrained_linear_luma_delta_mean']:.5f}; "
              f"clip {row['original_newly_clipped_channel_fraction']:.2%} -> "
              f"{row['restrained_newly_clipped_channel_fraction']:.2%}")


if __name__ == "__main__":
    main()
