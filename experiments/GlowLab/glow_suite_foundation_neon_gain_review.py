"""Review Glow-only foundation gain calibration on a fixed CC0 night photo."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

from glow_role_detail_matrix import linear_luma, normalized_crop, read_rgba, record_metrics


ROIS = {
    "central_intersection": (0.38, 0.42, 0.67, 0.72),
    "bright_roads": (0.08, 0.34, 0.90, 0.72),
    "red_practicals": (0.71, 0.34, 0.96, 0.61),
    "dark_city_blocks": (0.34, 0.03, 0.70, 0.28),
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
    manifest = json.loads((capture_dir / "product-suite-manifest.json").read_text(encoding="utf-8"))
    first = manifest["entries"][0]
    source_path = capture_dir / first["source_capture"]
    source = read_rgba(source_path)
    tiles = [source_path]
    results = []
    for entry in manifest["entries"]:
        output_path = capture_dir / entry["product_output"]
        input_capture = capture_dir / entry["source_capture"]
        input_image, output_image = read_rgba(input_capture), read_rgba(output_path)
        if not np.array_equal(input_image, source):
            raise RuntimeError(f"source input differs across gain captures: {entry['profile_key']}")
        luma = linear_luma(output_image[..., :3])
        record = {
            "profile_key": entry["profile_key"],
            "effect_id": entry["effect_id"],
            "properties": entry["properties"],
            "output": output_path.name,
            "output_sha256": digest(output_path),
            "state_sidecar": entry["state_sidecar"],
            "full_frame_metrics": record_metrics(source, output_image),
            "linear_luma_distribution": {
                "minimum": float(luma.min()),
                "mean": float(luma.mean()),
                "p50": float(np.percentile(luma, 50)),
                "p95": float(np.percentile(luma, 95)),
                "maximum": float(luma.max()),
            },
            "roi_metrics": {},
        }
        for roi_name, bounds in ROIS.items():
            src_crop, out_crop = normalized_crop(source, bounds), normalized_crop(output_image, bounds)
            crop_luma = linear_luma(out_crop[..., :3])
            record["roi_metrics"][roi_name] = {
                **record_metrics(src_crop, out_crop),
                "linear_luma_mean": float(crop_luma.mean()),
                "linear_luma_p95": float(np.percentile(crop_luma, 95)),
                "linear_luma_maximum": float(crop_luma.max()),
            }
        results.append(record)
        tiles.append(output_path)

    tile_w, tile_h = 500, 365
    sheet = Image.new("RGB", (tile_w * 3, tile_h * 2), (22, 24, 29))
    labels = ["source"] + [str(item["properties"]["glowGain"]) for item in manifest["entries"]]
    for i, path in enumerate(tiles):
        image = cv2.imread(str(path), cv2.IMREAD_COLOR)
        image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        tile = Image.fromarray(image)
        tile.thumbnail((tile_w - 12, tile_h - 42), Image.Resampling.LANCZOS)
        x, y = i % 3 * tile_w, i // 3 * tile_h
        sheet.paste(tile, (x + (tile_w - tile.width) // 2, y + 32))
        ImageDraw.Draw(sheet).text((x + 10, y + 8), f"Glow gain {labels[i]}", fill=(240, 240, 245))
    sheet_path = output_dir / "glow-foundation-neon-gain-contact-sheet.jpg"
    sheet.save(sheet_path, quality=93)

    payload = {
        "capture_class": "ArtifactUiTest installed-effect CPU reference",
        "plate": "CC0 aerial neon streets 1536x1025, opaque 8-bit input",
        "input_sha256": manifest["input_sha256"],
        "capture_manifest": str((capture_dir / "product-suite-manifest.json").resolve()),
        "roi_bounds_normalized": ROIS,
        "results": results,
        "limitations": ["not final Composition Viewer or GPU output", "SDR 8-bit clipping proxy"]
    }
    (output_dir / "glow-foundation-neon-gain-metrics.json").write_text(
        json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(sheet_path)
    for item in results:
        print(item["properties"]["glowGain"],
              f"full clip={item['full_frame_metrics']['newly_clipped_channel_fraction']:.2%}",
              f"intersection clip={item['roi_metrics']['central_intersection']['newly_clipped_channel_fraction']:.2%}",
              f"intersection ΔL={item['roi_metrics']['central_intersection']['linear_luma_delta_mean']:.4f}")


if __name__ == "__main__":
    main()
