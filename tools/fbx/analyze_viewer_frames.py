#!/usr/bin/env python3
"""Analyze captured FBX viewer frames for framing and visible-texture regressions.

Input images should be viewport-only captures, or use --viewport X,Y,W,H to
crop a fixed viewport rectangle from full-window screenshots. This is a triage
tool: it reports measurable visual changes and never claims to identify every
semantic mesh/UV defect automatically.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import cv2
import numpy as np


IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}


def parse_viewport(value: str | None) -> tuple[int, int, int, int] | None:
    if value is None:
        return None
    try:
        x, y, width, height = (int(part.strip()) for part in value.split(","))
    except (TypeError, ValueError) as exc:
        raise argparse.ArgumentTypeError("viewport must be X,Y,W,H") from exc
    if min(x, y) < 0 or min(width, height) <= 0:
        raise argparse.ArgumentTypeError("viewport coordinates and size are invalid")
    return x, y, width, height


def read_frame(path: Path, viewport: tuple[int, int, int, int] | None) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"cannot decode image: {path}")
    if viewport is not None:
        x, y, width, height = viewport
        if x + width > image.shape[1] or y + height > image.shape[0]:
            raise ValueError(f"viewport crop exceeds image bounds: {path}")
        image = image[y:y + height, x:x + width]
    if min(image.shape[:2]) < 64:
        raise ValueError(f"viewport image is too small: {path}")
    return image


def foreground_mask(
    image: np.ndarray,
) -> tuple[np.ndarray, tuple[int, int, int], list[int]]:
    height, width = image.shape[:2]
    border = max(2, min(height, width) // 40)
    edge = np.concatenate((
        image[:border].reshape(-1, 3), image[-border:].reshape(-1, 3),
        image[:, :border].reshape(-1, 3), image[:, -border:].reshape(-1, 3),
    ))
    # Median border color is robust to a small amount of overlay/text at edges.
    background = np.median(edge, axis=0).astype(np.int16)
    delta = np.max(np.abs(image.astype(np.int16) - background), axis=2)
    # A small blur stabilizes the silhouette threshold against isolated JPEG
    # ringing without removing the full-resolution color data used below.
    delta = cv2.GaussianBlur(delta.astype(np.uint8), (3, 3), 0)
    mask = (delta >= 12).astype(np.uint8)
    # Fill tiny holes and suppress isolated antialias/compression noise.
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask, 8)
    if count <= 1:
        return (np.zeros((height, width), np.uint8),
                tuple(int(v) for v in background), [])
    candidates = [(stats[i, cv2.CC_STAT_AREA], i) for i in range(1, count)
                  if stats[i, cv2.CC_STAT_AREA] >= 12]
    if not candidates:
        return (np.zeros((height, width), np.uint8),
                tuple(int(v) for v in background), [])
    _, largest = max(candidates)
    main = (labels == largest).astype(np.uint8)
    proximity = cv2.dilate(main, cv2.getStructuringElement(
        cv2.MORPH_ELLIPSE, (17, 17)))
    included_labels = {largest}
    for area, label in candidates:
        component = labels == label
        if label == largest or np.any(component & (proximity > 0)):
            main[component] = 1
            included_labels.add(label)
    # The analysis mask includes the connected/proximal model body so distant
    # UI/noise does not become the model bbox. Detached component area remains
    # separately visible as a diagnostic metric.
    detached_areas = [int(stats[label, cv2.CC_STAT_AREA])
                      for _, label in candidates if label not in included_labels]
    return main, tuple(int(v) for v in background), sorted(detached_areas,
                                                            reverse=True)


def frame_metrics(
    path: Path, image: np.ndarray,
) -> tuple[dict, np.ndarray, np.ndarray]:
    mask, background, component_areas = foreground_mask(image)
    height, width = mask.shape
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return ({"file": path.name, "status": "FAIL",
                 "issue": "no foreground model detected"}, mask,
                np.zeros_like(mask))
    x0, x1 = int(xs.min()), int(xs.max())
    y0, y1 = int(ys.min()), int(ys.max())
    area = int(mask.sum())
    # Saturated pixel coverage indicates visible color variation, not correctness
    # of a UV mapping. It is intentionally informational without a reference.
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    crop_mask = mask > 0
    saturated_fraction = float(np.mean(hsv[:, :, 1][crop_mask] >= 35))
    edge_margin = max(2, int(min(width, height) * 0.01))
    touches = []
    if x0 <= edge_margin: touches.append("left")
    if x1 >= width - 1 - edge_margin: touches.append("right")
    if y0 <= edge_margin: touches.append("top")
    if y1 >= height - 1 - edge_margin: touches.append("bottom")
    result = {
        "file": path.name,
        "status": "FAIL" if touches else "PASS",
        "background_bgr": background,
        "model_bbox_xyxy": [x0, y0, x1, y1],
        "foreground_area_fraction": round(area / (width * height), 5),
        "model_frame_occupancy": round(
            ((x1 - x0 + 1) * (y1 - y0 + 1)) / (width * height), 5),
        "visible_saturated_pixel_fraction": round(saturated_fraction, 5),
        "detached_component_count": len(component_areas),
        "detached_component_area_fraction": round(
            sum(component_areas) / max(1, area + sum(component_areas)), 5),
        "touches_viewport_edge": touches,
    }
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_SIMPLE)
    silhouette = np.zeros_like(mask)
    cv2.drawContours(silhouette, contours, -1, 1, thickness=cv2.FILLED)
    return result, mask, silhouette


def compare_to_reference(
    reference: np.ndarray,
    reference_surface_mask: np.ndarray,
    reference_silhouette: np.ndarray,
    image: np.ndarray,
    surface_mask: np.ndarray,
    silhouette: np.ndarray,
) -> dict:
    ref = reference_silhouette > 0
    current = silhouette > 0
    union = int(np.count_nonzero(ref | current))
    intersection = int(np.count_nonzero(ref & current))
    silhouette_iou = intersection / union if union else 1.0
    reference_surface = reference_surface_mask > 0
    current_surface = surface_mask > 0
    surface_union = reference_surface | current_surface
    color_region = surface_union | ref | current
    if np.any(color_region):
        delta = np.abs(reference.astype(np.int16) - image.astype(np.int16))
        color_mae = float(np.mean(delta[color_region]))
        ref_hsv = cv2.cvtColor(reference, cv2.COLOR_BGR2HSV)
        image_hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        ref_hist = cv2.calcHist([ref_hsv], [0, 1], reference_surface_mask,
                                [24, 16], [0, 180, 0, 256])
        image_hist = cv2.calcHist([image_hsv], [0, 1], surface_mask,
                                  [24, 16], [0, 180, 0, 256])
        cv2.normalize(ref_hist, ref_hist, alpha=1, norm_type=cv2.NORM_L1)
        cv2.normalize(image_hist, image_hist, alpha=1, norm_type=cv2.NORM_L1)
        histogram_similarity = max(0.0, 1.0 - cv2.compareHist(
            ref_hist, image_hist, cv2.HISTCMP_BHATTACHARYYA))
    else:
        color_mae = 255.0
        histogram_similarity = 0.0
    union_mask = ref | current
    ys, xs = np.nonzero(union_mask)
    local_color_max = 255.0
    local_color_cell = None
    local_shape_max = 0.0
    local_shape_cell = None
    local_shape_changed_pixels = 0
    bottom_region_shape_iou = None
    bottom_region_shape_changed_pixels = 0
    bottom_region_color_mae = None
    if len(xs):
        x0, x1 = int(xs.min()), int(xs.max()) + 1
        y0, y1 = int(ys.min()), int(ys.max()) + 1
        # For an upright human capture, the lowest 20% of the model bbox is a
        # useful foot/toe review area. It is a visual locator, not an anatomy
        # or foot-direction classifier.
        bottom_y = y0 + int((y1 - y0) * 0.80)
        bottom_ref = ref[bottom_y:y1, x0:x1]
        bottom_current = current[bottom_y:y1, x0:x1]
        bottom_union = bottom_ref | bottom_current
        bottom_union_count = int(np.count_nonzero(bottom_union))
        if bottom_union_count:
            bottom_intersection = int(np.count_nonzero(
                bottom_ref & bottom_current))
            bottom_region_shape_iou = bottom_intersection / bottom_union_count
            bottom_region_shape_changed_pixels = int(np.count_nonzero(
                bottom_ref ^ bottom_current))
            bottom_color_region = (reference_surface[bottom_y:y1, x0:x1] |
                                   current_surface[bottom_y:y1, x0:x1] |
                                   bottom_union)
            if np.any(bottom_color_region):
                bottom_delta = delta[bottom_y:y1, x0:x1]
                bottom_region_color_mae = float(np.mean(
                    bottom_delta[bottom_color_region]))
        # A finer grid catches localized map/mesh changes that disappear in a
        # whole-model average. Cell coordinates are relative to this crop.
        grid_columns, grid_rows = 16, 32
        for row in range(grid_rows):
            cy0 = y0 + (y1 - y0) * row // grid_rows
            cy1 = y0 + (y1 - y0) * (row + 1) // grid_rows
            for column in range(grid_columns):
                cx0 = x0 + (x1 - x0) * column // grid_columns
                cx1 = x0 + (x1 - x0) * (column + 1) // grid_columns
                ref_cell = ref[cy0:cy1, cx0:cx1]
                current_cell = current[cy0:cy1, cx0:cx1]
                cell_union = ref_cell | current_cell
                union_count = int(np.count_nonzero(cell_union))
                if union_count < 20:
                    continue
                cell_intersection = ref_cell & current_cell
                cell_iou = (int(np.count_nonzero(cell_intersection)) /
                            union_count)
                if 1.0 - cell_iou > local_shape_max:
                    local_shape_max = 1.0 - cell_iou
                    local_shape_cell = [column, row]
                    local_shape_changed_pixels = int(np.count_nonzero(
                        ref_cell ^ current_cell))
                ref_surface_cell = reference_surface[cy0:cy1, cx0:cx1]
                current_surface_cell = current_surface[cy0:cy1, cx0:cx1]
                color_cell_union = (ref_surface_cell | current_surface_cell |
                                    cell_union)
                if np.any(color_cell_union):
                    cell_delta = delta[cy0:cy1, cx0:cx1]
                    cell_mae = float(np.mean(cell_delta[color_cell_union]))
                    if cell_mae > local_color_max or local_color_cell is None:
                        local_color_max = cell_mae
                        local_color_cell = [column, row]
    return {
        "reference_silhouette_iou": round(silhouette_iou, 5),
        "reference_foreground_color_mae_0_255": round(color_mae, 3),
        "reference_hue_saturation_histogram_similarity": round(
            histogram_similarity, 5),
        "reference_foreground_overlap_fraction": round(
            intersection / max(1, int(np.count_nonzero(current))), 5),
        "reference_max_local_silhouette_mismatch": round(local_shape_max, 5),
        "reference_max_local_silhouette_cell": local_shape_cell,
        "reference_max_local_silhouette_changed_pixels": local_shape_changed_pixels,
        "reference_max_local_color_mae_0_255": round(local_color_max, 3),
        "reference_max_local_color_cell": local_color_cell,
        "reference_bottom_20pct_silhouette_iou": (
            round(bottom_region_shape_iou, 5)
            if bottom_region_shape_iou is not None else None),
        "reference_bottom_20pct_silhouette_changed_pixels":
            bottom_region_shape_changed_pixels,
        "reference_bottom_20pct_color_mae_0_255": (
            round(bottom_region_color_mae, 3)
            if bottom_region_color_mae is not None else None),
    }


def analyze(paths: list[Path], viewport: tuple[int, int, int, int] | None,
            output: Path, min_saturated_fraction: float | None,
            expect_motion: bool, reference_path: Path | None,
            reference_directory: Path | None,
            min_reference_silhouette_iou: float,
            max_reference_color_mae: float,
            min_reference_histogram_similarity: float,
            max_reference_local_color_mae: float,
            max_reference_local_shape_mismatch: float,
            min_reference_bottom_silhouette_iou: float | None) -> dict:
    images = [read_frame(path, viewport) for path in paths]
    reference_paths = ([reference_directory / path.name for path in paths]
                       if reference_directory else
                       [reference_path for _ in paths] if reference_path else [])
    if any(path is not None and not path.is_file() for path in reference_paths):
        missing = next(path for path in reference_paths
                       if path is not None and not path.is_file())
        raise ValueError(f"matched reference image does not exist: {missing}")
    reference_images = [read_frame(path, viewport) for path in reference_paths]
    dimensions = {(im.shape[1], im.shape[0]) for im in images}
    dimensions.update((im.shape[1], im.shape[0]) for im in reference_images)
    if len(dimensions) != 1:
        raise ValueError("all captures must have the same viewport dimensions")
    results = []
    masks = []
    silhouettes = []
    reference_surface_masks = []
    reference_silhouettes = []
    for ref_path, ref_image in zip(reference_paths, reference_images):
        _, ref_surface, ref_silhouette = frame_metrics(ref_path, ref_image)
        reference_surface_masks.append(ref_surface)
        reference_silhouettes.append(ref_silhouette)
    for index, (path, image) in enumerate(zip(paths, images)):
        metric, mask, silhouette = frame_metrics(path, image)
        if (min_saturated_fraction is not None and
                metric.get("visible_saturated_pixel_fraction", 0) <
                min_saturated_fraction):
            metric["status"] = "FAIL"
            metric["texture_warning"] = (
                "visible color variation is below the expected minimum")
        if reference_images:
            comparison = compare_to_reference(
                reference_images[index], reference_surface_masks[index],
                reference_silhouettes[index], image, mask, silhouette)
            metric.update(comparison)
            metric["reference_file"] = reference_paths[index].name
            if comparison["reference_silhouette_iou"] < min_reference_silhouette_iou:
                metric["status"] = "FAIL"
                metric["reference_shape_warning"] = (
                    "silhouette differs from the matched-pose reference")
            if comparison["reference_foreground_color_mae_0_255"] > max_reference_color_mae:
                metric["status"] = "FAIL"
                metric["reference_texture_warning"] = (
                    "foreground colors differ from the matched-pose reference")
            if (comparison["reference_hue_saturation_histogram_similarity"] <
                    min_reference_histogram_similarity):
                metric["status"] = "FAIL"
                metric["reference_texture_histogram_warning"] = (
                    "foreground hue/saturation distribution differs from the reference")
            if (comparison["reference_max_local_color_mae_0_255"] >
                    max_reference_local_color_mae):
                metric["status"] = "FAIL"
                metric["reference_local_texture_warning"] = (
                    "a local model region differs in color from the reference")
            if (comparison["reference_max_local_silhouette_mismatch"] >
                    max_reference_local_shape_mismatch):
                metric["status"] = "FAIL"
                metric["reference_local_shape_warning"] = (
                    "a local model region differs in silhouette from the reference")
            bottom_iou = comparison[
                "reference_bottom_20pct_silhouette_iou"]
            if (min_reference_bottom_silhouette_iou is not None and
                    bottom_iou is not None and
                    bottom_iou < min_reference_bottom_silhouette_iou):
                metric["status"] = "FAIL"
                metric["reference_bottom_region_warning"] = (
                    "the approximate foot/toe region differs in silhouette "
                    "from the matched-pose reference")
        results.append(metric)
        masks.append(mask)
        silhouettes.append(silhouette)

    transitions = []
    for index in range(1, len(silhouettes)):
        previous, current = (silhouettes[index - 1] > 0,
                             silhouettes[index] > 0)
        union = int(np.count_nonzero(previous | current))
        intersection = int(np.count_nonzero(previous & current))
        iou = intersection / union if union else 1.0
        area_ratio = (np.count_nonzero(current) /
                      max(1, np.count_nonzero(previous)))
        transitions.append({
            "from": paths[index - 1].name,
            "to": paths[index].name,
            "silhouette_iou": round(iou, 5),
            "foreground_area_ratio": round(float(area_ratio), 5),
            "pose_change_observed": bool(iou < 0.995),
            "large_area_change_review": bool(
                area_ratio < 0.65 or area_ratio > 1.54),
        })

    if expect_motion and not any(item["pose_change_observed"] for item in transitions):
        for item in results:
            if item["status"] == "PASS":
                item["status"] = "FAIL"
                item["motion_warning"] = (
                    "no measurable silhouette change across captured frames")

    annotated = []
    for path, image, metric, mask in zip(paths, images, results, silhouettes):
        preview = image.copy()
        if "model_bbox_xyxy" in metric:
            x0, y0, x1, y1 = metric["model_bbox_xyxy"]
            color = (0, 0, 255) if metric["touches_viewport_edge"] else (0, 220, 0)
            cv2.rectangle(preview, (x0, y0), (x1, y1), color, 2)
            bottom_review_y = y0 + int((y1 - y0) * 0.80)
            cv2.line(preview, (x0, bottom_review_y),
                     (x1, bottom_review_y), (0, 180, 255), 1)
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL,
                                           cv2.CHAIN_APPROX_SIMPLE)
            cv2.drawContours(preview, contours, -1, (255, 80, 0), 1)
        annotated.append(preview)
    output.mkdir(parents=True, exist_ok=True)
    columns = min(3, len(annotated))
    thumb_width = 640
    thumb_height = max(1, round(thumb_width * images[0].shape[0] / images[0].shape[1]))
    thumbs = [cv2.resize(im, (thumb_width, thumb_height)) for im in annotated]
    rows = (len(thumbs) + columns - 1) // columns
    canvas = np.zeros((rows * thumb_height, columns * thumb_width, 3), np.uint8)
    for i, thumb in enumerate(thumbs):
        x, y = (i % columns) * thumb_width, (i // columns) * thumb_height
        canvas[y:y + thumb_height, x:x + thumb_width] = thumb
    preview_path = output / "fbx_frame_analysis.png"
    cv2.imwrite(str(preview_path), canvas)
    report = {
        "input_count": len(paths),
        "viewport_size": list(next(iter(dimensions))),
        "reference_image": str(reference_path) if reference_path else None,
        "reference_directory": (str(reference_directory)
                                if reference_directory else None),
        "settings": {
            "min_saturated_fraction": min_saturated_fraction,
            "expect_motion": expect_motion,
            "min_reference_silhouette_iou": min_reference_silhouette_iou,
            "max_reference_color_mae": max_reference_color_mae,
            "min_reference_histogram_similarity": min_reference_histogram_similarity,
            "max_reference_local_color_mae": max_reference_local_color_mae,
            "max_reference_local_shape_mismatch": max_reference_local_shape_mismatch,
            "min_reference_bottom_silhouette_iou":
                min_reference_bottom_silhouette_iou,
        },
        "frame_results": results,
        "transitions": transitions,
        "summary": {
            "frames_with_edge_contact": sum(bool(item.get("touches_viewport_edge")) for item in results),
            "frames_without_detected_model": sum("model_bbox_xyxy" not in item for item in results),
            "frames_failed": sum(item["status"] == "FAIL" for item in results),
            "pose_changes_observed": sum(item["pose_change_observed"] for item in transitions),
            "transitions_requiring_area_review": sum(item["large_area_change_review"] for item in transitions),
            "frames_with_visible_color_variation": sum(
                bool(item.get("visible_saturated_pixel_fraction", 0) >= 0.01)
                for item in results),
        },
        "limitations": [
            "Edge contact is a strong crop warning; non-contact does not prove correct anatomy.",
            "Silhouette change indicates visual motion only; it cannot establish correct skinning.",
            "Saturation indicates visible color variation only; compare against a known-good reference to detect texture regressions.",
            "Reference comparison is valid only for the same model, clip time, camera, viewport, and rendering mode; pose or lighting changes can cause false positives.",
            "The bottom-20-percent comparison is only an approximate foot/toe review region for upright human captures; it does not determine whether a foot points in the anatomically correct direction.",
        ],
        "annotated_preview": str(preview_path),
    }
    (output / "fbx_frame_analysis.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path, help="frame image or directory of captures")
    parser.add_argument("--viewport", type=parse_viewport,
                        help="fixed crop X,Y,W,H for full-window screenshots")
    parser.add_argument("--min-saturated-fraction", type=float,
                        help="fail frames below this visible-color fraction when the fixture is expected to be textured")
    parser.add_argument("--expect-motion", action="store_true",
                        help="fail when silhouettes do not change across the captured sequence")
    reference_group = parser.add_mutually_exclusive_group()
    reference_group.add_argument("--reference-image", type=Path,
                                 help="known-good frame from the same model/clip time/camera/viewport")
    reference_group.add_argument("--reference-dir", type=Path,
                                 help="known-good frames named to match each input frame")
    parser.add_argument("--min-reference-silhouette-iou", type=float, default=0.90,
                        help="fail matched frames below this reference silhouette IoU")
    parser.add_argument("--max-reference-color-mae", type=float, default=20.0,
                        help="fail matched frames above this foreground RGB MAE (0-255)")
    parser.add_argument("--min-reference-histogram-similarity", type=float, default=0.75,
                        help="fail matched frames below this HSV histogram similarity")
    parser.add_argument("--max-reference-local-color-mae", type=float, default=10.0,
                        help="fail if any model grid cell exceeds this foreground BGR MAE")
    parser.add_argument("--max-reference-local-shape-mismatch", type=float, default=0.5,
                        help="fail if any model grid cell exceeds this silhouette mismatch")
    parser.add_argument("--min-reference-bottom-silhouette-iou", type=float,
                        help="optionally fail when the approximate bottom-20%% (foot/toe) silhouette IoU is below this value; intended for upright human captures")
    parser.add_argument("--output", type=Path, default=Path("fbx-frame-analysis"))
    args = parser.parse_args()
    if (args.min_saturated_fraction is not None and
            not 0.0 <= args.min_saturated_fraction <= 1.0):
        parser.error("--min-saturated-fraction must be between 0 and 1")
    if not 0.0 <= args.min_reference_silhouette_iou <= 1.0:
        parser.error("--min-reference-silhouette-iou must be between 0 and 1")
    if not 0.0 <= args.max_reference_color_mae <= 255.0:
        parser.error("--max-reference-color-mae must be between 0 and 255")
    if not 0.0 <= args.min_reference_histogram_similarity <= 1.0:
        parser.error("--min-reference-histogram-similarity must be between 0 and 1")
    if not 0.0 <= args.max_reference_local_color_mae <= 255.0:
        parser.error("--max-reference-local-color-mae must be between 0 and 255")
    if not 0.0 <= args.max_reference_local_shape_mismatch <= 1.0:
        parser.error("--max-reference-local-shape-mismatch must be between 0 and 1")
    if (args.min_reference_bottom_silhouette_iou is not None and
            not 0.0 <= args.min_reference_bottom_silhouette_iou <= 1.0):
        parser.error("--min-reference-bottom-silhouette-iou must be between 0 and 1")
    if args.reference_image is not None and not args.reference_image.is_file():
        parser.error(f"reference image does not exist: {args.reference_image}")
    if args.reference_dir is not None and not args.reference_dir.is_dir():
        parser.error(f"reference directory does not exist: {args.reference_dir}")
    paths = ([args.frames] if args.frames.is_file() else sorted(
        path for path in args.frames.iterdir()
        if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES))
    if not paths:
        parser.error("no supported frame images found")
    try:
        report = analyze(paths, args.viewport, args.output,
                         args.min_saturated_fraction, args.expect_motion,
                         args.reference_image, args.reference_dir,
                         args.min_reference_silhouette_iou,
                         args.max_reference_color_mae,
                         args.min_reference_histogram_similarity,
                         args.max_reference_local_color_mae,
                         args.max_reference_local_shape_mismatch,
                         args.min_reference_bottom_silhouette_iou)
    except (OSError, ValueError) as exc:
        print(f"FBX frame analysis failed: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report["summary"], ensure_ascii=False))
    print(f"Annotated preview: {report['annotated_preview']}")
    return 1 if report["summary"]["frames_failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
