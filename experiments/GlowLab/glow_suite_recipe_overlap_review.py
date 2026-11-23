"""Compare the spatial signatures of ordered Glow suite recipes."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

from glow_role_intensity_review import digest, read_rgba
from glow_suite_ordered_stack_review import ROI_PROFILES

ROOT = Path(__file__).resolve().parents[2]


def to_linear(rgb: np.ndarray) -> np.ndarray:
    srgb = rgb.astype(np.float32) / 255.0
    return np.where(srgb <= 0.04045, srgb / 12.92,
                    ((srgb + 0.055) / 1.055) ** 2.4)


def crop(array: np.ndarray, bounds: tuple[float, float, float, float]) -> np.ndarray:
    height, width = array.shape[:2]
    x0, y0, x1, y1 = bounds
    return array[int(y0 * height):int(y1 * height),
                 int(x0 * width):int(x1 * width)]


def cosine(left: np.ndarray, right: np.ndarray) -> float | None:
    a = left.reshape(-1).astype(np.float64)
    b = right.reshape(-1).astype(np.float64)
    denominator = float(np.linalg.norm(a) * np.linalg.norm(b))
    if denominator <= 1e-20:
        return None
    return float(np.dot(a, b) / denominator)


def run(capture_dir: Path, output_dir: Path, roi_profile: str) -> None:
    capture_dir, output_dir = capture_dir.resolve(), output_dir.resolve()
    manifest_path = capture_dir / "ordered-stack-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("not_single_float_stack_or_final_viewer") is not True:
        raise RuntimeError("Capture manifest does not describe the expected PNG-chain proxy")
    if manifest.get("capture_class") != \
            "Ordered ArtifactUiTest installed-effect CPU output with 8-bit PNG handoff":
        raise RuntimeError("Unexpected capture class")
    if manifest.get("foundation", {}).get("status") != "captured":
        raise RuntimeError("Shared Glow foundation capture did not complete")

    manifest_input = Path(str(manifest["input"]))
    if not manifest_input.is_absolute():
        manifest_input = (ROOT / manifest_input).resolve()
    if manifest_input.is_file() and digest(manifest_input) != manifest["input_sha256"]:
        raise RuntimeError("Original source hash differs from the ordered capture manifest")
    recipes = manifest.get("recipes", [])
    if len(recipes) < 2:
        raise RuntimeError("At least two captured recipes are required")

    foundation_record = manifest["foundation"]
    foundation_path = capture_dir / foundation_record["product_output"]
    source_path = capture_dir / foundation_record["captured_input"]
    if digest(source_path) != foundation_record["captured_input_sha256"]:
        raise RuntimeError("Foundation input hash mismatch")
    if digest(foundation_path) != foundation_record["product_output_sha256"]:
        raise RuntimeError("Foundation output hash mismatch")
    source_rgba = read_rgba(source_path)
    foundation_rgba = read_rgba(foundation_path)
    if source_rgba.shape != foundation_rgba.shape:
        raise RuntimeError("Source and foundation dimensions differ")
    foundation_rgb = to_linear(foundation_rgba[..., :3])

    final_outputs: dict[str, dict[str, object]] = {}
    for recipe in recipes:
        name = str(recipe["recipe"])
        if recipe.get("status") != "captured" or len(recipe.get("stages", [])) != 2:
            raise RuntimeError(f"Recipe is not a complete two-stage capture: {name}")
        stages = recipe["stages"]
        if stages[0].get("input_sha256") != foundation_record["product_output_sha256"]:
            raise RuntimeError(f"Recipe {name} did not start from the common foundation")
        expected_input_pixels = foundation_rgba
        for stage in stages:
            input_path = capture_dir / stage["captured_input"]
            output_path = capture_dir / stage["product_output"]
            if digest(input_path) != stage["captured_input_sha256"]:
                raise RuntimeError(f"Input hash mismatch in {name}")
            if digest(output_path) != stage["product_output_sha256"]:
                raise RuntimeError(f"Output hash mismatch in {name}")
            stage_input = read_rgba(input_path)
            stage_output = read_rgba(output_path)
            if stage_input.shape != expected_input_pixels.shape or not np.array_equal(
                    stage_input, expected_input_pixels):
                raise RuntimeError(f"Ordered pixel input mismatch in {name}")
            expected_input_pixels = stage_output
        final_record = stages[-1]
        final_rgba = read_rgba(capture_dir / final_record["product_output"])
        if final_rgba.shape != source_rgba.shape:
            raise RuntimeError(f"Final output dimensions differ for {name}")
        final_rgb = to_linear(final_rgba[..., :3])
        final_outputs[name] = {
            "delta_rgb": final_rgb - foundation_rgb,
            "final_rgba": final_rgba,
            "output_sha256": final_record["product_output_sha256"],
            "stages": [stage["effect_id"] for stage in stages],
        }

    bounds_by_name = ROI_PROFILES[roi_profile]
    recipe_names = list(final_outputs)
    results: dict[str, object] = {}
    output_dir.mkdir(parents=True, exist_ok=True)
    for roi_name, bounds in bounds_by_name.items():
        base_roi = crop(foundation_rgba, bounds)[..., :3]
        roi_height, roi_width = base_roi.shape[:2]
        tile_width, tile_height, label_height, pad = roi_width, roi_height, 24, 8
        atlas = Image.new(
            "RGB",
            (3 * (tile_width + pad), len(recipe_names) * (tile_height + label_height + pad)),
            (21, 23, 29),
        )
        draw = ImageDraw.Draw(atlas)
        bands: dict[str, dict[str, np.ndarray]] = {
            "signed_rgb_delta": {},
            "broad_luma_sigma6": {},
            "fine_luma_sigma3_residual": {},
            "chroma_rg_bg": {},
        }
        for recipe_index, (recipe_name, record) in enumerate(final_outputs.items()):
            delta = record["delta_rgb"]
            delta_luma = delta @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
            smooth = cv2.GaussianBlur(delta_luma, (0, 0), 3.0)
            broad = cv2.GaussianBlur(delta_luma, (0, 0), 6.0)
            chroma = np.stack((delta[..., 0] - delta[..., 1],
                               delta[..., 2] - delta[..., 1]), axis=-1)
            bands["signed_rgb_delta"][recipe_name] = crop(delta, bounds)
            bands["broad_luma_sigma6"][recipe_name] = crop(broad, bounds)
            bands["fine_luma_sigma3_residual"][recipe_name] = crop(delta_luma - smooth, bounds)
            bands["chroma_rg_bg"][recipe_name] = crop(chroma, bounds)

            final_roi = crop(record["final_rgba"], bounds)[..., :3]
            difference = np.clip(
                np.abs(final_roi.astype(np.float32) - base_roi.astype(np.float32)) * 5.0,
                0.0, 255.0,
            ).astype(np.uint8)
            row_y = recipe_index * (tile_height + label_height + pad)
            for column, (image_data, label) in enumerate((
                    (base_roi, "shared foundation"),
                    (final_roi, f"{recipe_name} final"),
                    (difference, f"{recipe_name} delta x5"))):
                x = column * (tile_width + pad)
                atlas.paste(Image.fromarray(image_data), (x, row_y + label_height))
                draw.text((x + 2, row_y + 4), label, fill=(238, 240, 246))

        atlas_path = output_dir / f"recipe-role-atlas-{roi_name}.jpg"
        atlas.save(atlas_path, quality=93)

        roi_result: dict[str, object] = {"normalized_bounds": list(bounds), "bands": {}}
        roi_result["native_scale_atlas"] = atlas_path.name
        for band_name, recipe_values in bands.items():
            matrix: list[list[float | None]] = []
            norms = {name: float(np.linalg.norm(value.reshape(-1)))
                     for name, value in recipe_values.items()}
            for left in recipe_names:
                matrix.append([cosine(recipe_values[left], recipe_values[right])
                               for right in recipe_names])
            pairs = []
            for i, left in enumerate(recipe_names):
                for j in range(i + 1, len(recipe_names)):
                    right = recipe_names[j]
                    value = matrix[i][j]
                    if value is not None:
                        pairs.append({"recipe_a": left, "recipe_b": right,
                                      "signed_delta_cosine": value})
            pairs.sort(key=lambda item: abs(float(item["signed_delta_cosine"])), reverse=True)
            csv_path = output_dir / f"recipe-overlap-{roi_name}-{band_name}.csv"
            with csv_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.writer(stream)
                writer.writerow(["recipe", *recipe_names])
                for index, name in enumerate(recipe_names):
                    writer.writerow([name, *[
                        "" if value is None else f"{value:.8f}" for value in matrix[index]
                    ]])
            roi_result["bands"][band_name] = {
                "delta_l2_norm": norms,
                "signed_delta_cosine_matrix": matrix,
                "highest_absolute_similarity_pairs": pairs[:8],
                "csv": csv_path.name,
            }
        results[roi_name] = roi_result

    payload = {
        "capture_class": manifest["capture_class"],
        "input_sha256": manifest["input_sha256"],
        "foundation_output_sha256": foundation_record["product_output_sha256"],
        "dimensions": [int(source_rgba.shape[1]), int(source_rgba.shape[0])],
        "roi_profile": roi_profile,
        "comparison": "cosine similarity among linear-light final-minus-foundation recipe deltas",
        "recipe_outputs": {
            name: {"stages": record["stages"], "output_sha256": record["output_sha256"]}
            for name, record in final_outputs.items()
        },
        "rois": results,
        "interpretation": "exploratory role-overlap signal only; not a visual-quality or redundancy verdict",
        "limitations": [
            "two-effect ordered CPU references with 8-bit PNG handoff",
            "cosine similarity can reflect shared scene geometry or glow foundation interactions",
            "does not establish approved aesthetics, float-stack behavior, Viewer/GPU parity, HDR, or performance",
        ],
    }
    json_path = output_dir / "glow-suite-recipe-overlap.json"
    json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {json_path}")
    for roi_name, roi_data in results.items():
        summary = roi_data["bands"]["broad_luma_sigma6"]["highest_absolute_similarity_pairs"][:2]
        pairs = ", ".join(
            f"{item['recipe_a']}/{item['recipe_b']} {item['signed_delta_cosine']:.3f}"
            for item in summary)
        print(f"{roi_name}: {pairs}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--roi-profile", choices=tuple(ROI_PROFILES), default="portrait")
    args = parser.parse_args()
    run(args.capture_dir, args.output_dir, args.roi_profile)


if __name__ == "__main__":
    main()
