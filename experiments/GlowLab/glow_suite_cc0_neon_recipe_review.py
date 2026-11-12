"""Review shared-foundation Glow branches on the CC0 aerial night-city plate."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

from glow_role_detail_matrix import make_sheet, normalized_crop, read_rgba, record_metrics


ROIS = {
    "central_intersection": (0.38, 0.42, 0.67, 0.72),
    "bright_roads": (0.08, 0.34, 0.90, 0.72),
    "red_practicals": (0.71, 0.34, 0.96, 0.61),
    "dark_city_blocks": (0.34, 0.03, 0.70, 0.28),
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_full_sheet(records: list[dict[str, object]], output_path: Path) -> None:
    tile_size = (512, 370)
    canvas = Image.new("RGB", (tile_size[0] * 3, tile_size[1] * 4), (23, 25, 31))
    for index, record in enumerate(records):
        source = record["source_crop"]
        result = record["result_crop"]
        assert isinstance(source, np.ndarray) and isinstance(result, np.ndarray)
        image = cv2.cvtColor(result[..., :3], cv2.COLOR_RGBA2RGB)
        tile = Image.fromarray(image)
        tile.thumbnail((tile_size[0] - 12, tile_size[1] - 42), Image.Resampling.LANCZOS)
        x, y = (index % 3) * tile_size[0], (index // 3) * tile_size[1]
        canvas.paste(tile, (x + (tile_size[0] - tile.width) // 2, y + 32))
        metric = record["metrics"]
        assert isinstance(metric, dict)
        ImageDraw.Draw(canvas).text(
            (x + 10, y + 8),
            f"{record['effect']} | dL {metric['linear_luma_delta_mean']:.4f} | "
            f"clip {metric['newly_clipped_channel_fraction']:.2%}",
            fill=(235, 238, 245))
    canvas.save(output_path, quality=92)


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
    records_by_roi: dict[str, list[dict[str, object]]] = {name: [] for name in ROIS}
    full_records: list[dict[str, object]] = []

    for entry in manifest["entries"][1:]:
        if entry.get("status") != "captured":
            continue
        output_path = capture_dir / entry["product_output"]
        source_path = capture_dir / entry["captured_input"]
        source, result = read_rgba(source_path), read_rgba(output_path)
        if source.shape != foundation.shape or result.shape != foundation.shape:
            raise RuntimeError(f"dimension mismatch: {entry['label']}")
        if not np.array_equal(source, foundation):
            raise RuntimeError(f"branch input differs from shared foundation: {entry['label']}")
        state = (capture_dir / entry["state_sidecar"]).read_text(encoding="utf-8")
        full_records.append({
            "effect": entry["label"].removeprefix("suite-foundation-plus-"),
            "source_crop": foundation,
            "result_crop": result,
            "metrics": record_metrics(foundation, result),
        })
        for roi_name, bounds in ROIS.items():
            source_crop = normalized_crop(source, bounds).copy()
            result_crop = normalized_crop(result, bounds).copy()
            records_by_roi[roi_name].append({
                "plate": "CC0 aerial neon city",
                "roi": roi_name,
                "effect": entry["label"].removeprefix("suite-foundation-plus-"),
                "recipe": entry["label"],
                "effect_id": entry["effect_id"],
                "normalized_roi": list(bounds),
                "foundation_sha256": digest(foundation_path),
                "product_output": output_path.name,
                "product_output_sha256": digest(output_path),
                "properties": entry["properties"],
                "state": state,
                "metrics": record_metrics(source_crop, result_crop),
                "source_crop": source_crop,
                "result_crop": result_crop,
            })

    make_full_sheet(full_records, output_dir / "cc0-neon-recipe-full-frame-contact-sheet.jpg")
    compact_records = []
    for roi_name, records in records_by_roi.items():
        if not records:
            continue
        make_sheet(records, output_dir / f"cc0-neon-recipe-{roi_name}-contact-sheet.png")
        for record in records:
            record.pop("source_crop")
            record.pop("result_crop")
            compact_records.append(record)

    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU chained PNG proxy",
        "not_single_float_stack_or_final_viewer": True,
        "plate": "CC0 aerial neon streets, 1536x1025",
        "input_sha256": manifest["input_sha256"],
        "foundation_effect": foundation_entry["effect_id"],
        "foundation_output_sha256": digest(foundation_path),
        "foundation_properties": foundation_entry["properties"],
        "roi_bounds_normalized": ROIS,
        "full_frame_metrics": [
            {"effect": item["effect"], "metrics": item["metrics"]} for item in full_records
        ],
        "records": compact_records,
    }
    (output_dir / "cc0-neon-recipe-roi-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Reviewed {len(full_records)} recipe branches over {len(ROIS)} fixed ROIs.")


if __name__ == "__main__":
    main()
