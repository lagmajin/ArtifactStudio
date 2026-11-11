"""Review portrait Glow recipe branches on face, practical-light, and dark-detail ROIs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from glow_role_detail_matrix import make_sheet, normalized_crop, read_rgba, record_metrics


ROIS = {
    "face": (0.37, 0.08, 0.49, 0.25),
    "streetlamp": (0.72, 0.16, 0.76, 0.22),
    "streetlamp_context": (0.67, 0.10, 0.82, 0.30),
    "shopfront_practicals": (0.82, 0.20, 0.99, 0.40),
    "dark_coat": (0.28, 0.24, 0.46, 0.66),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    capture_dir, output_dir = args.capture_dir.resolve(), args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((capture_dir / "suite-recipe-manifest.json").read_text(encoding="utf-8"))
    foundation_entry = manifest["entries"][0]
    foundation_path = capture_dir / foundation_entry["product_output"]
    foundation = read_rgba(foundation_path)
    source_sha = digest(foundation_path)
    records_by_roi: dict[str, list[dict[str, object]]] = {name: [] for name in ROIS}

    for entry in manifest["entries"][1:]:
        if entry.get("status") != "captured":
            continue
        output_path = capture_dir / entry["product_output"]
        source_path = capture_dir / entry["captured_input"]
        state_path = capture_dir / entry["state_sidecar"]
        source, result = read_rgba(source_path), read_rgba(output_path)
        if source.shape != foundation.shape or result.shape != foundation.shape:
            raise RuntimeError(f"dimension mismatch: {entry['label']}")
        if not (source == foundation).all():
            raise RuntimeError(f"branch source does not match shared foundation: {entry['label']}")
        state = state_path.read_text(encoding="utf-8")
        for roi_name, bounds in ROIS.items():
            source_crop = normalized_crop(source, bounds).copy()
            result_crop = normalized_crop(result, bounds).copy()
            record = {
                "plate": "portrait recipe",
                "roi": roi_name,
                "effect": entry['label'].removeprefix("suite-foundation-plus-"),
                "recipe": entry["label"],
                "effect_id": entry["effect_id"],
                "normalized_roi": list(bounds),
                "foundation_sha256": source_sha,
                "product_output": output_path.name,
                "product_output_sha256": digest(output_path),
                "properties": entry["properties"],
                "state": state,
                "metrics": record_metrics(source_crop, result_crop),
                "source_crop": source_crop,
                "result_crop": result_crop,
            }
            records_by_roi[roi_name].append(record)

    compact_records = []
    for roi_name, records in records_by_roi.items():
        if not records:
            continue
        make_sheet(records, output_dir / f"portrait-recipe-{roi_name}-roi-contact-sheet.png")
        for record in records:
            record.pop("source_crop")
            record.pop("result_crop")
            compact_records.append(record)

    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU chained PNG proxy",
        "not_single_float_stack_or_final_viewer": True,
        "plate": "AI-generated low-light portrait, 1536x1024",
        "input_sha256": manifest["input_sha256"],
        "foundation_effect": foundation_entry["effect_id"],
        "foundation_output_sha256": source_sha,
        "foundation_properties": foundation_entry["properties"],
        "roi_bounds_normalized": ROIS,
        "records": compact_records,
    }
    (output_dir / "portrait-recipe-roi-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Reviewed {len(manifest['entries']) - 1} portrait recipe branches across {len(ROIS)} ROIs.")


if __name__ == "__main__":
    main()
