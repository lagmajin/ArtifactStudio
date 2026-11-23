"""Compare restrained Glow product captures on matching neon ROIs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from glow_role_detail_matrix import (
    CAPTURE_ROOT,
    PLATES,
    capture_stem,
    make_sheet,
    normalized_crop,
    record_metrics,
)
from glow_role_intensity_review import read_rgba


ROOT = Path(__file__).resolve().parents[2]
PROFILE_PATH = ROOT / "experiments/GlowLab/glow_practical_restraint_profiles.json"
BASELINE_DIR = CAPTURE_ROOT / "glow-suite-role-candidates-neon-cpu-rd1"
RESTRAINT_DIR = CAPTURE_ROOT / "glow-suite-practical-restraint-rd1"
ROI_NAMES = ("magenta_ring", "emissive_text", "horizontal_tube")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=RESTRAINT_DIR)
    args = parser.parse_args()
    config = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
    roi_map = PLATES["neon"]["rois"]
    output_records: list[dict[str, object]] = []

    for profile_name, profile in config["profiles"].items():
        effect_id = profile["effect_id"]
        baseline_stem = capture_stem(effect_id)
        candidate_stem = f"{profile_name}-{effect_id}-cpu"
        baseline_input_path = BASELINE_DIR / f"{baseline_stem}-fixture-input.png"
        baseline_output_path = BASELINE_DIR / f"{baseline_stem}.png"
        candidate_input_path = args.output_dir / f"{candidate_stem}-fixture-input.png"
        candidate_output_path = args.output_dir / f"{candidate_stem}.png"
        baseline_input = read_rgba(baseline_input_path)
        baseline_output = read_rgba(baseline_output_path)
        candidate_input = read_rgba(candidate_input_path)
        candidate_output = read_rgba(candidate_output_path)
        if not (baseline_input.shape == baseline_output.shape ==
                candidate_input.shape == candidate_output.shape):
            raise RuntimeError(f"capture dimensions differ for {profile_name}")
        source_hashes = {digest(baseline_input_path), digest(candidate_input_path)}
        if len(source_hashes) != 1:
            raise RuntimeError(f"baseline and candidate inputs differ: {profile_name}")

        for roi_name in ROI_NAMES:
            rect = roi_map[roi_name]
            roi_records: list[dict[str, object]] = []
            for label, input_image, result_image, stem, input_path, output_path in (
                ("baseline", baseline_input, baseline_output, baseline_stem,
                 baseline_input_path, baseline_output_path),
                ("restrained", candidate_input, candidate_output, candidate_stem,
                 candidate_input_path, candidate_output_path),
            ):
                source_crop = normalized_crop(input_image, rect).copy()
                result_crop = normalized_crop(result_image, rect).copy()
                metrics = record_metrics(source_crop, result_crop)
                effect_label = f"{effect_id} | {label}"
                roi_records.append({
                    "plate": "neon",
                    "roi": roi_name,
                    "effect": effect_label,
                    "source_crop": source_crop,
                    "result_crop": result_crop,
                    "metrics": metrics,
                })
                output_records.append({
                    "profile": profile_name,
                    "effect_id": effect_id,
                    "capture": label,
                    "capture_stem": stem,
                    "roi": roi_name,
                    "roi_fraction": list(rect),
                    "input_sha256": digest(input_path),
                    "output_sha256": digest(output_path),
                    **metrics,
                })
            make_sheet(roi_records,
                       args.output_dir / f"{profile_name}-{roi_name}-contact-sheet.png")

    metrics_path = args.output_dir / "practical-restraint-metrics.json"
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "profile": str(PROFILE_PATH.relative_to(ROOT)),
        "records": output_records,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved comparison sheets and {metrics_path}")
    for row in output_records:
        print(f"{row['profile']} {row['capture']} {row['roi']}: "
              f"dL={row['linear_luma_delta_mean']:.5f}, "
              f"clip={row['newly_clipped_channel_fraction']:.3%}, "
              f"alphaMAE={row['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
