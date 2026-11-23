"""Assemble the lowest-clipping captured high-key profile for each suite recipe."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
CAPTURES = ROOT / "experiments/GlowLab/captures"
SELECTED = {
    "contour-streak": "rd3",
    "reactive-spectrum": "rd2",
    "film-optical": "rd2",
    "refractive-ridge": "rd3",
    "point-rays": "rd2",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fit(image: np.ndarray, width: int, height: int) -> np.ndarray:
    source_h, source_w = image.shape[:2]
    scale = min(width / source_w, height / source_h)
    resized = cv2.resize(image, (max(1, round(source_w * scale)),
                                 max(1, round(source_h * scale))),
                         interpolation=cv2.INTER_AREA if scale < 1 else cv2.INTER_CUBIC)
    canvas = np.zeros((height, width, 3), dtype=np.uint8)
    x = (width - resized.shape[1]) // 2
    y = (height - resized.shape[0]) // 2
    canvas[y:y + resized.shape[0], x:x + resized.shape[1]] = resized
    return canvas


def run(output: Path) -> None:
    source_path = CAPTURES / "glow-suite-highkey-composition-rd1/source-walkway-frame20.png"
    source = cv2.imread(str(source_path), cv2.IMREAD_COLOR)
    if source is None:
        raise RuntimeError(f"Could not read high-key source: {source_path}")
    source_hash = digest(source_path)
    foundation_hashes: set[str] = set()
    rows: list[np.ndarray] = []
    evidence: dict[str, object] = {}
    for recipe, revision in SELECTED.items():
        capture_dir = CAPTURES / f"glow-suite-ordered-stacks-highkey-cpu-{revision}"
        metrics = json.loads((capture_dir / "review/ordered-stack-suite-metrics.json").read_text(encoding="utf-8"))
        manifest = json.loads((capture_dir / "ordered-stack-manifest.json").read_text(encoding="utf-8"))
        if metrics["input_sha256"] != source_hash or manifest["input_sha256"] != source_hash:
            raise RuntimeError(f"High-key source hash mismatch for {recipe}")
        recipe_row = next(row for row in metrics["recipe_results"] if row["recipe"] == recipe)
        foundation_path = capture_dir / manifest["foundation"]["product_output"]
        final_path = capture_dir / recipe_row["final_output"]
        foundation_hashes.add(digest(foundation_path))
        foundation = cv2.imread(str(foundation_path), cv2.IMREAD_COLOR)
        final = cv2.imread(str(final_path), cv2.IMREAD_COLOR)
        if foundation is None or final is None or source.shape != foundation.shape or source.shape != final.shape:
            raise RuntimeError(f"Invalid source/foundation/final dimensions for {recipe}")
        delta = np.clip(np.abs(final.astype(np.float32) - foundation.astype(np.float32)) * 5.0,
                        0, 255).astype(np.uint8)
        panels = (source, foundation, final, delta)
        width, height, label_h = 240, 420, 30
        row = np.zeros((height + label_h, width * 4, 3), dtype=np.uint8)
        for index, (panel, label) in enumerate(zip(
                panels, ("INPUT", "GLOW FOUNDATION", recipe, "SPECIALIST DELTA X5"))):
            x = index * width
            cv2.putText(row, label, (x + 5, 21), cv2.FONT_HERSHEY_SIMPLEX,
                        0.47, (225, 230, 238), 1, cv2.LINE_AA)
            row[label_h:, x:x + width] = fit(panel, width, height)
        rows.append(row)
        gate = recipe_row["incremental_clip_gate_against_foundation"]
        evidence[recipe] = {
            "revision": revision,
            "profile": manifest["profile"],
            "output": str(final_path.relative_to(ROOT)),
            "output_sha256": digest(final_path),
            "foundation_sha256": digest(foundation_path),
            "alpha_mae_against_source": recipe_row["final_alpha_mae_against_source"],
            "full_frame_new_clipped_channel_fraction": gate["full_frame_new_clip_fraction"],
            "roi_new_clipped_channel_fractions": gate["roi_new_clip_fraction"],
            "within_review_gate": (gate["full_frame_within_1_percent"] and
                                   gate["all_rois_within_3_percent"]),
            "difference_display": "absolute encoded RGB difference from shared Glow foundation, multiplied by 5 and clipped to 8-bit",
        }
    if len(foundation_hashes) != 1:
        raise RuntimeError("Selected recipes do not share pixel-identical Glow foundation outputs")
    output.mkdir(parents=True, exist_ok=True)
    sheet_path = output / "glow-suite-highkey-candidate-atlas.jpg"
    cv2.imwrite(str(sheet_path), np.vstack(rows), [cv2.IMWRITE_JPEG_QUALITY, 95])
    report = {
        "intent": "review the lowest-clipping captured high-key candidate from each of five Glow recipes",
        "source": str(source_path.relative_to(ROOT)),
        "source_sha256": source_hash,
        "shared_foundation_sha256": next(iter(foundation_hashes)),
        "recipes": evidence,
        "limitations": [
            "selected high-key candidates mix rd2 and rd3 intensity revisions",
            "still image, 8-bit PNG handoff, installed CPU effect reference",
            "x5 difference helps locate small changes but does not establish visual acceptance",
            "not a float-preserving stack, final Viewer, GPU parity, or HDR evaluation",
        ],
    }
    report_path = output / "glow-suite-highkey-candidate-atlas.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {sheet_path}")
    print(f"Saved {report_path}")
    for name, item in evidence.items():
        print(f"{name}: full={item['full_frame_new_clipped_channel_fraction']:.3%}, "
              f"gate={item['within_review_gate']}, alpha MAE={item['alpha_mae_against_source']:.6f}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)


if __name__ == "__main__":
    main()
