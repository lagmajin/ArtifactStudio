"""Compare high-key recipe branches on matched sky, bridge, ice, and water ROIs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from glow_role_detail_matrix import PLATES, normalized_crop, read_rgba, record_metrics


ROOT = Path(__file__).resolve().parents[2]
ROIS = PLATES["walkway"]["rois"]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def title_strip(width: int, title: str) -> np.ndarray:
    image = np.zeros((38, width, 4), dtype=np.uint8)
    image[..., :3] = (7, 9, 15)
    image[..., 3] = 255
    cv2.putText(image, title, (14, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.58,
                (230, 235, 245, 255), 1, cv2.LINE_AA)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    capture_dir = args.capture_dir.resolve()
    manifest = json.loads((capture_dir / "suite-recipe-manifest.json")
                          .read_text(encoding="utf-8"))
    foundation_entry = manifest["entries"][0]
    foundation_path = capture_dir / foundation_entry["product_output"]
    foundation = read_rgba(foundation_path)
    records: list[dict[str, object]] = []
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for entry in manifest["entries"][1:]:
        if entry.get("status") != "captured":
            continue
        output_path = capture_dir / entry["product_output"]
        captured_input = capture_dir / entry["captured_input"]
        source, output = read_rgba(captured_input), read_rgba(output_path)
        if source.shape != foundation.shape or output.shape != foundation.shape:
            raise RuntimeError(f"dimension mismatch for {entry['label']}")
        if not np.array_equal(source, foundation):
            raise RuntimeError(f"branch source pixels differ from shared foundation: {entry['label']}")
        state_path = capture_dir / entry["state_sidecar"]
        state = state_path.read_text(encoding="utf-8")
        for roi_name, rect in ROIS.items():
            source_crop = normalized_crop(source, rect).copy()
            output_crop = normalized_crop(output, rect).copy()
            metrics = record_metrics(source_crop, output_crop)
            records.append({
                "recipe": entry["label"],
                "effect_id": entry["effect_id"],
                "roi": roi_name,
                "normalized_roi": rect,
                "foundation_sha256": digest(foundation_path),
                "product_output": output_path.name,
                "product_output_sha256": digest(output_path),
                "properties": entry["properties"],
                "state": state,
                "metrics": metrics,
                "source_crop": source_crop,
                "output_crop": output_crop,
            })

    summary_records: list[dict[str, object]] = []
    for roi_name in ROIS:
        matching = [record for record in records if record["roi"] == roi_name]
        if not matching:
            continue
        first_crop = matching[0]["source_crop"]
        assert isinstance(first_crop, np.ndarray)
        width = first_crop.shape[1] * 3
        rows = [title_strip(width, f"{roi_name}: SHARED FOUNDATION | RECIPE OUTPUT | ABS DIFF x4")]
        for record in matching:
            source = record["source_crop"]
            output = record["output_crop"]
            assert isinstance(source, np.ndarray) and isinstance(output, np.ndarray)
            diff = np.clip(np.abs(output.astype(np.float32) -
                                  source.astype(np.float32)) * 4.0,
                           0, 255).astype(np.uint8)
            rows.extend((title_strip(width, f"{record['recipe']} | "
                                     f"dL {record['metrics']['linear_luma_delta_mean']:.4f} | "
                                     f"clip {record['metrics']['newly_clipped_channel_fraction']:.2%}"),
                         np.hstack((source, output, diff))))
        sheet = np.vstack(rows)
        cv2.imwrite(str(args.output_dir / f"recipe-{roi_name}-roi-comparison.png"),
                    cv2.cvtColor(sheet, cv2.COLOR_RGBA2BGRA))

    for record in records:
        record.pop("source_crop")
        record.pop("output_crop")
        summary_records.append(record)
    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU chained PNG proxy",
        "input_plate": "CC0 walkway-orbit frame 20, 360x640",
        "shared_foundation_sha256": digest(foundation_path),
        "roi_order": list(ROIS),
        "records": summary_records,
    }
    (args.output_dir / "recipe-roi-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
