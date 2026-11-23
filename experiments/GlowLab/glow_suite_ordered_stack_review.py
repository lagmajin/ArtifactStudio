"""Review staged Glow recipe captures against the source and shared foundation."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

from glow_role_detail_matrix import linear_luma, normalized_crop, read_rgba, record_metrics


ROOT = Path(__file__).resolve().parents[2]


ROI_PROFILES = {
    "neon": {
        "central_intersection": (0.38, 0.42, 0.67, 0.72),
        "bright_roads": (0.08, 0.34, 0.90, 0.72),
        "red_practicals": (0.71, 0.34, 0.96, 0.61),
        "dark_city_blocks": (0.34, 0.03, 0.70, 0.28),
    },
    "portrait": {
        "face": (0.37, 0.08, 0.49, 0.25),
        "streetlamp": (0.72, 0.16, 0.76, 0.22),
        "streetlamp_context": (0.67, 0.10, 0.82, 0.30),
        "shopfront_practicals": (0.82, 0.20, 0.99, 0.40),
        "dark_coat": (0.28, 0.24, 0.46, 0.66),
    },
    "streetlight": {
        "both_bulbs": (0.12, 0.18, 0.84, 0.38),
        "source_bulb": (0.16, 0.19, 0.42, 0.38),
        "dark_sky": (0.05, 0.02, 0.95, 0.16),
        "pole_and_wires": (0.15, 0.28, 0.90, 0.62),
        "lower_practicals": (0.00, 0.62, 1.00, 0.95),
    },
    "walkway_highkey": {
        "bright_sky": (0.02, 0.02, 0.98, 0.27),
        "bridge_span": (0.02, 0.40, 0.98, 0.69),
        "ice_and_water": (0.02, 0.70, 0.98, 0.98),
        "dark_water": (0.30, 0.72, 0.66, 0.92),
    },
    "point_source": {
        "primary_emitter": (0.24, 0.24, 0.45, 0.48),
        "secondary_blue": (0.67, 0.40, 0.81, 0.55),
        "dark_buildings": (0.44, 0.60, 0.60, 0.88),
    },
    "synthetic_neon": {
        "magenta_ring": (0.15, 0.18, 0.40, 0.58),
        "emissive_text": (0.36, 0.35, 0.58, 0.54),
        "cyan_point": (0.70, 0.28, 0.82, 0.48),
        "amber_point": (0.76, 0.43, 0.83, 0.55),
        "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
        "dark_field": (0.40, 0.76, 0.62, 0.95),
    },
    "material_neon": {
        "magenta_ring": (0.15, 0.18, 0.40, 0.58),
        "emissive_text": (0.36, 0.35, 0.58, 0.54),
        "cyan_point": (0.70, 0.28, 0.82, 0.48),
        "amber_point": (0.76, 0.43, 0.83, 0.55),
        "horizontal_tube": (0.08, 0.58, 0.90, 0.69),
        "brick_wall": (0.02, 0.08, 0.15, 0.58),
        "wet_road": (0.03, 0.66, 0.97, 0.96),
        "dark_field": (0.62, 0.70, 0.97, 0.96),
    },
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_luma_stats(image: np.ndarray) -> dict[str, float]:
    luma = linear_luma(image[..., :3])
    return {
        "minimum": float(luma.min()),
        "mean": float(luma.mean()),
        "p50": float(np.percentile(luma, 50)),
        "p95": float(np.percentile(luma, 95)),
        "maximum": float(luma.max()),
    }


def to_rgb_tile(image: np.ndarray, size: tuple[int, int]) -> Image.Image:
    rgb = cv2.cvtColor(image[..., :3], cv2.COLOR_RGBA2RGB)
    tile = Image.fromarray(rgb)
    tile.thumbnail(size, Image.Resampling.LANCZOS)
    return tile


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--roi-profile", choices=tuple(ROI_PROFILES), default="neon")
    args = parser.parse_args()
    rois = ROI_PROFILES[args.roi_profile]
    capture_dir, output_dir = args.capture_dir.resolve(), args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((capture_dir / "ordered-stack-manifest.json").read_text(encoding="utf-8"))
    if manifest["foundation"].get("status") != "captured":
        raise RuntimeError("shared Glow foundation did not capture")
    foundation_entry = manifest["foundation"]
    if foundation_entry.get("identity_input"):
        foundation_path = Path(manifest["input"])
        if not foundation_path.is_absolute():
            foundation_path = ROOT / foundation_path
        foundation_input_path = foundation_path
    else:
        foundation_path = capture_dir / foundation_entry["product_output"]
        foundation_input_path = capture_dir / foundation_entry["captured_input"]
    source = read_rgba(foundation_input_path)
    foundation = read_rgba(foundation_path)
    if source.shape != foundation.shape:
        raise RuntimeError("source and shared foundation dimensions differ")

    max_stage_count = max((len(recipe.get("stages", [])) for recipe in manifest["recipes"]), default=1)
    overview_cols, overview_rows = max_stage_count + 2, max(1, len(manifest["recipes"]))
    tile_w, tile_h = 420, 330
    overview = Image.new("RGB", (tile_w * overview_cols, tile_h * overview_rows), (21, 23, 29))
    overview_draw = ImageDraw.Draw(overview)
    all_recipe_records = []
    for recipe_index, recipe in enumerate(manifest["recipes"]):
        if recipe.get("status") != "captured" or not recipe.get("stages"):
            raise RuntimeError(f"incomplete recipe: {recipe['recipe']}")
        stage_arrays = []
        previous_path = foundation_path
        previous_image = foundation
        stage_records = []
        for stage in recipe["stages"]:
            input_path = capture_dir / stage["captured_input"]
            output_path = capture_dir / stage["product_output"]
            input_image, output_image = read_rgba(input_path), read_rgba(output_path)
            if input_image.shape != source.shape or output_image.shape != source.shape:
                raise RuntimeError(f"dimension mismatch in {recipe['recipe']} stage {stage['stage_index']}")
            expected_input_hash = digest(previous_path)
            declared_input_hash = stage.get("previous_stage_input_sha256")
            if declared_input_hash != expected_input_hash or not np.array_equal(input_image, previous_image):
                raise RuntimeError(f"stage input mismatch in {recipe['recipe']} stage {stage['stage_index']}")
            state = (capture_dir / stage["state_sidecar"]).read_text(encoding="utf-8")
            if "compute=0" not in state or "captureSource=Installed CPU reference" not in state:
                raise RuntimeError(f"unexpected compute route in {stage['state_sidecar']}")
            metrics = {
                "against_previous_stage": record_metrics(previous_image, output_image),
                "against_original_source": record_metrics(source, output_image),
                "against_glow_foundation": record_metrics(foundation, output_image),
                "output_luma_distribution": image_luma_stats(output_image),
                "roi_metrics": {},
            }
            for roi_name, bounds in rois.items():
                src_roi = normalized_crop(source, bounds)
                foundation_roi = normalized_crop(foundation, bounds)
                previous_roi = normalized_crop(previous_image, bounds)
                output_roi = normalized_crop(output_image, bounds)
                metrics["roi_metrics"][roi_name] = {
                    "against_previous_stage": record_metrics(previous_roi, output_roi),
                    "against_original_source": record_metrics(src_roi, output_roi),
                    "against_glow_foundation": record_metrics(foundation_roi, output_roi),
                }
            stage_records.append({
                "stage_index": stage["stage_index"],
                "effect_id": stage["effect_id"],
                "properties": stage["properties"],
                "input_sha256": digest(input_path),
                "output": output_path.name,
                "output_sha256": digest(output_path),
                "state_sidecar": stage["state_sidecar"],
                "capture_elapsed_seconds": stage["capture_elapsed_seconds"],
                "metrics": metrics,
            })
            stage_arrays.append(output_image)
            previous_path, previous_image = output_path, output_image

        images = [source, foundation, *stage_arrays]
        labels = ["original input", "shared foundation" if not foundation_entry.get("identity_input")
                  else "identity input", *[stage["effect_id"] for stage in recipe["stages"]]]
        y = recipe_index * tile_h
        for col, (image, label) in enumerate(zip(images, labels)):
            x = col * tile_w
            overview.paste(to_rgb_tile(image, (tile_w - 12, tile_h - 42)), (x + 6, y + 30))
            overview_draw.text((x + 8, y + 8), f"{recipe['recipe']} / {label}", fill=(238, 240, 246))

        final = stage_arrays[-1]
        cumulative_full = record_metrics(foundation, final)
        cumulative_rois = {
            name: record_metrics(normalized_crop(foundation, bounds), normalized_crop(final, bounds))
            for name, bounds in rois.items()
        }
        clip_gate = {
            "full_frame_new_clip_fraction": cumulative_full["newly_clipped_channel_fraction"],
            "full_frame_within_1_percent": cumulative_full["newly_clipped_channel_fraction"] <= 0.01,
            "roi_new_clip_fraction": {
                name: value["newly_clipped_channel_fraction"] for name, value in cumulative_rois.items()
            },
            "all_rois_within_3_percent": all(
                value["newly_clipped_channel_fraction"] <= 0.03 for value in cumulative_rois.values()
            ),
        }
        all_recipe_records.append({
            "recipe": recipe["recipe"],
            "stage_count": len(stage_records),
            "foundation_sha256": digest(foundation_path),
            "final_output": recipe["stages"][-1]["product_output"],
            "final_output_sha256": recipe["stages"][-1]["product_output_sha256"],
            "final_alpha_mae_against_source": record_metrics(source, final)["alpha_mae"],
            "incremental_clip_gate_against_foundation": clip_gate,
            "stages": stage_records,
        })

        roi_sheet = Image.new("RGB", (tile_w * len(images), len(rois) * 260), (21, 23, 29))
        roi_draw = ImageDraw.Draw(roi_sheet)
        difference_sheet = Image.new("RGB", (tile_w * (len(stage_arrays) + 2), len(rois) * 260),
                                     (21, 23, 29))
        difference_draw = ImageDraw.Draw(difference_sheet)
        for roi_index, (roi_name, bounds) in enumerate(rois.items()):
            row_y = roi_index * 260
            roi_images = [normalized_crop(image, bounds) for image in images]
            for col, (image, label) in enumerate(zip(roi_images, labels)):
                x = col * tile_w
                roi_sheet.paste(to_rgb_tile(image, (tile_w - 12, 225)), (x + 6, row_y + 30))
                roi_draw.text((x + 8, row_y + 8), f"{roi_name} / {label}", fill=(238, 240, 246))
            previous_roi = roi_images[1].astype(np.float32)
            difference_images = [roi_images[1]]
            difference_labels = ["foundation" if not foundation_entry.get("identity_input")
                                 else "identity input"]
            for stage_index, stage_roi_image in enumerate(roi_images[2:], start=1):
                current_roi = stage_roi_image.astype(np.float32)
                difference_images.append(np.dstack((
                    np.clip(np.abs(current_roi - previous_roi) * 5.0, 0, 255),
                    np.full(current_roi.shape[:2], 255.0, dtype=np.float32))).astype(np.uint8))
                difference_labels.append(f"stage {stage_index} delta x5")
                previous_roi = current_roi
            final_roi = roi_images[-1].astype(np.float32)
            base_roi = roi_images[1].astype(np.float32)
            difference_images.append(np.dstack((
                np.clip(np.abs(final_roi - base_roi) * 5.0, 0, 255),
                np.full(base_roi.shape[:2], 255.0, dtype=np.float32))).astype(np.uint8))
            difference_labels.append("total delta x5")
            for col, (image, label) in enumerate(zip(difference_images, difference_labels)):
                x = col * tile_w
                difference_sheet.paste(to_rgb_tile(image, (tile_w - 12, 225)), (x + 6, row_y + 30))
                difference_draw.text((x + 8, row_y + 8), f"{roi_name} / {label}", fill=(238, 240, 246))
        roi_sheet.save(output_dir / f"ordered-stack-{recipe['recipe']}-roi-sheet.jpg", quality=93)
        difference_sheet.save(output_dir / f"ordered-stack-{recipe['recipe']}-roi-differences-x5.jpg", quality=93)

    overview.save(output_dir / "ordered-stack-suite-overview.jpg", quality=93)
    payload = {
        "capture_class": manifest["capture_class"],
        "not_single_float_stack_or_final_viewer": True,
        "input_sha256": manifest["input_sha256"],
        "source_dimensions": [source.shape[1], source.shape[0]],
        "foundation_effect_id": foundation_entry["effect_id"],
        "foundation_properties": foundation_entry["properties"],
        "foundation_sha256": digest(foundation_path),
        "foundation_metrics_against_source": record_metrics(source, foundation),
        "foundation_roi_metrics_against_source": {
            name: record_metrics(normalized_crop(source, bounds), normalized_crop(foundation, bounds))
            for name, bounds in rois.items()
        },
        "roi_profile": args.roi_profile,
        "roi_bounds_normalized": rois,
        "recipe_results": all_recipe_records,
        "limitations": ["8-bit PNG handoff between effects", "CPU installed-effect reference only",
                        "not final Composition Viewer or GPU parity", "SDR display clipping proxy"],
    }
    metrics_path = output_dir / "ordered-stack-suite-metrics.json"
    metrics_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Reviewed {len(all_recipe_records)} ordered recipes; metrics: {metrics_path}")
    for result in all_recipe_records:
        gate = result["incremental_clip_gate_against_foundation"]
        print(f"{result['recipe']}: full clip +{gate['full_frame_new_clip_fraction']:.2%}, "
              f"all ROI within 3%={gate['all_rois_within_3_percent']}, "
              f"alpha MAE={result['final_alpha_mae_against_source']:.6f}")


if __name__ == "__main__":
    main()
