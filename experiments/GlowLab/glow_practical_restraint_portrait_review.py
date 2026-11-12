"""Review neon-discovered restrained Glow controls on portrait face detail."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from glow_role_detail_matrix import CAPTURE_ROOT, PLATES, capture_stem, make_sheet, normalized_crop, record_metrics
from glow_role_intensity_review import read_rgba


ROOT = Path(__file__).resolve().parents[2]
PROFILE_PATH = ROOT / "experiments/GlowLab/glow_practical_restraint_portrait_profiles.json"
BASELINE_DIR = CAPTURE_ROOT / "glow-suite-role-candidates-portrait-cpu-rd1"
CAPTURE_DIR = CAPTURE_ROOT / "glow-suite-practical-restraint-portrait-rd1"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, default=CAPTURE_DIR)
    args = parser.parse_args()
    profiles = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))["profiles"]
    face_roi = PLATES["portrait"]["rois"]["face"]
    records: list[dict[str, object]] = []

    for profile_name, profile in profiles.items():
        effect_id = profile["effect_id"]
        baseline_stem = capture_stem(effect_id)
        candidate_stem = f"{profile_name}-{effect_id}-cpu"
        paths = (
            ("baseline", BASELINE_DIR / f"{baseline_stem}-fixture-input.png",
             BASELINE_DIR / f"{baseline_stem}.png", baseline_stem),
            ("restrained", args.output_dir / f"{candidate_stem}-fixture-input.png",
             args.output_dir / f"{candidate_stem}.png", candidate_stem),
        )
        roi_records: list[dict[str, object]] = []
        for label, input_path, output_path, stem in paths:
            source_image = read_rgba(input_path)
            output_image = read_rgba(output_path)
            if source_image.shape != output_image.shape:
                raise RuntimeError(f"input/output size mismatch: {stem}")
            source_crop = normalized_crop(source_image, face_roi).copy()
            output_crop = normalized_crop(output_image, face_roi).copy()
            metrics = record_metrics(source_crop, output_crop)
            roi_records.append({
                "plate": "portrait",
                "roi": "face",
                "effect": f"{effect_id} | {label}",
                "source_crop": source_crop,
                "result_crop": output_crop,
                "metrics": metrics,
            })
            records.append({
                "profile": profile_name,
                "effect_id": effect_id,
                "capture": label,
                "capture_stem": stem,
                "roi": "face",
                "roi_fraction": list(face_roi),
                "input_sha256": digest(input_path),
                "output_sha256": digest(output_path),
                **metrics,
            })
        source_hashes = {row["input_sha256"] for row in records[-2:]}
        if len(source_hashes) != 1:
            raise RuntimeError(f"baseline and candidate inputs differ: {profile_name}")
        make_sheet(roi_records, args.output_dir / f"{profile_name}-face-contact-sheet.png")

    metrics_path = args.output_dir / "portrait-restraint-metrics.json"
    metrics_path.write_text(json.dumps({
        "capture_class": "ArtifactUiTest installed-effect CPU reference output",
        "not_final_viewer_or_GPU_output": True,
        "profile": str(PROFILE_PATH.relative_to(ROOT)),
        "records": records,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Saved portrait comparison sheets and {metrics_path}")
    for row in records:
        print(f"{row['profile']} {row['capture']}: "
              f"dL={row['linear_luma_delta_mean']:.5f}, "
              f"p95={row['linear_luma_delta_p95']:.5f}, "
              f"clip={row['newly_clipped_channel_fraction']:.3%}, "
              f"alphaMAE={row['alpha_mae']:.5f}")


if __name__ == "__main__":
    main()
