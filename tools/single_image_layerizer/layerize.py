#!/usr/bin/env python3
"""Standalone still-image layerization prototype.

The prototype deliberately does not import ArtifactStudio or Qt.  It turns one
image plus an optional mask/bounding box into a small, inspectable layer pack:
RGBA cutouts, masks, a background plate, a preview, and a versioned manifest.

The segmentation backends are intentionally small and explicit:
* --mask uses a user-supplied grayscale/alpha mask and is deterministic.
* --bbox creates a deterministic rectangular candidate.
* --auto uses OpenCV GrabCut and optionally accepts --bbox as its seed.

This keeps the file contract testable before a model or the application is
chosen as the production integration point.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

try:
    from PIL import Image, ImageChops, ImageFilter
except ImportError as exc:  # pragma: no cover - depends on the local environment
    raise SystemExit("Pillow is required: python -m pip install Pillow") from exc


SCHEMA = "artifact.single-image-layerization/v1"
RESAMPLE = getattr(Image, "Resampling", Image).LANCZOS


class LayerizerError(RuntimeError):
    """User-facing processing error."""


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a standalone still-image layerization pack."
    )
    parser.add_argument("input", type=Path, help="Input still image")
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output directory for the layer pack",
    )
    source_group = parser.add_mutually_exclusive_group()
    source_group.add_argument(
        "--mask",
        type=Path,
        help="Grayscale/alpha mask; non-zero pixels become the foreground",
    )
    source_group.add_argument(
        "--bbox",
        metavar="X,Y,W,H",
        help="Foreground seed rectangle in source pixels",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="Use OpenCV GrabCut with an inset or --bbox seed",
    )
    parser.add_argument(
        "--background",
        choices=("transparent", "original", "inpaint"),
        default="inpaint",
        help="Background plate strategy (default: inpaint)",
    )
    parser.add_argument(
        "--split-components",
        action="store_true",
        help="Split disconnected mask components into separate layers",
    )
    parser.add_argument(
        "--max-components",
        type=int,
        default=8,
        help="Maximum number of output components (default: 8)",
    )
    parser.add_argument(
        "--min-area-px",
        type=int,
        default=64,
        help="Discard components smaller than this area (default: 64)",
    )
    parser.add_argument(
        "--padding",
        type=int,
        default=0,
        help="Extra source pixels around each cutout bounds",
    )
    parser.add_argument(
        "--feather",
        type=float,
        default=0.0,
        help="Gaussian mask feather radius in source pixels",
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=1,
        help="Mask threshold from 0 to 255 (default: 1)",
    )
    parser.add_argument(
        "--name-prefix",
        default="Layer",
        help="Prefix for generated layer names (default: Layer)",
    )
    parser.add_argument(
        "--inpaint-radius",
        type=float,
        default=3.0,
        help="OpenCV inpaint radius in pixels (default: 3)",
    )
    args = parser.parse_args(argv)
    if args.mask is None and args.bbox is None and not args.auto:
        parser.error("one of --mask, --bbox, or --auto is required")
    if args.mask is not None and args.auto:
        parser.error("--mask cannot be combined with --auto")
    return args


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_bbox(value: Optional[str], width: int, height: int) -> Tuple[int, int, int, int]:
    if value is None:
        margin_x = max(1, int(width * 0.05))
        margin_y = max(1, int(height * 0.05))
        return (
            margin_x,
            margin_y,
            max(1, width - margin_x * 2),
            max(1, height - margin_y * 2),
        )

    try:
        parts = [int(part.strip()) for part in value.split(",")]
    except ValueError as exc:
        raise LayerizerError("--bbox must be X,Y,W,H using integer pixels") from exc
    if len(parts) != 4:
        raise LayerizerError("--bbox must be X,Y,W,H using integer pixels")

    x, y, box_width, box_height = parts
    if box_width <= 0 or box_height <= 0:
        raise LayerizerError("--bbox width and height must be positive")
    x0 = max(0, min(x, width - 1))
    y0 = max(0, min(y, height - 1))
    x1 = max(x0 + 1, min(width, x + box_width))
    y1 = max(y0 + 1, min(height, y + box_height))
    return x0, y0, x1 - x0, y1 - y0


def load_mask(path: Path, size: Tuple[int, int]) -> Image.Image:
    with Image.open(path) as loaded:
        if loaded.mode in ("RGBA", "LA"):
            mask = loaded.getchannel("A")
        else:
            mask = loaded.convert("L")
        if mask.size != size:
            mask = mask.resize(size, RESAMPLE)
        return mask.copy()


def rectangle_mask(size: Tuple[int, int], bbox: Tuple[int, int, int, int]) -> Image.Image:
    mask = Image.new("L", size, 0)
    x, y, width, height = bbox
    mask.paste(255, (x, y, x + width, y + height))
    return mask


def grabcut_mask(
    source: Image.Image, bbox: Tuple[int, int, int, int]
) -> Image.Image:
    try:
        import cv2
        import numpy as np
    except ImportError as exc:
        raise LayerizerError(
            "--auto requires OpenCV and NumPy; use --mask or --bbox for a dependency-light run"
        ) from exc

    rgb = np.asarray(source.convert("RGB"))
    x, y, width, height = bbox
    grabcut_mask_array = np.zeros(rgb.shape[:2], dtype=np.uint8)
    background_model = np.zeros((1, 65), dtype=np.float64)
    foreground_model = np.zeros((1, 65), dtype=np.float64)
    cv2.grabCut(
        rgb,
        grabcut_mask_array,
        (x, y, width, height),
        background_model,
        foreground_model,
        5,
        cv2.GC_INIT_WITH_RECT,
    )
    foreground = np.where(
        (grabcut_mask_array == cv2.GC_FGD)
        | (grabcut_mask_array == cv2.GC_PR_FGD),
        255,
        0,
    ).astype(np.uint8)
    return Image.fromarray(foreground, mode="L")


def threshold_mask(mask: Image.Image, threshold: int) -> Image.Image:
    if not 0 <= threshold <= 255:
        raise LayerizerError("--threshold must be between 0 and 255")
    return mask.point(lambda value: 255 if value >= threshold else 0, mode="L")


def connected_components(
    mask: Image.Image,
    split: bool,
    min_area: int,
    max_components: int,
) -> Tuple[List[Image.Image], List[str]]:
    if min_area < 1:
        raise LayerizerError("--min-area-px must be positive")
    if max_components < 1:
        raise LayerizerError("--max-components must be positive")

    warnings: List[str] = []
    if not split:
        return [mask], warnings

    try:
        import cv2
        import numpy as np
    except ImportError:
        warnings.append(
            "OpenCV/NumPy unavailable; disconnected components were kept as one candidate"
        )
        return [mask], warnings

    source = np.asarray(mask, dtype=np.uint8)
    labels_count, labels, stats, _ = cv2.connectedComponentsWithStats(source, 8)
    components: List[Tuple[int, Image.Image]] = []
    for label in range(1, labels_count):
        area = int(stats[label, cv2.CC_STAT_AREA])
        if area < min_area:
            continue
        component = np.where(labels == label, 255, 0).astype(np.uint8)
        components.append((area, Image.fromarray(component, mode="L")))
    components.sort(key=lambda item: item[0], reverse=True)
    if len(components) > max_components:
        warnings.append(
            f"{len(components) - max_components} small candidate(s) were omitted by --max-components"
        )
    return [component for _, component in components[:max_components]], warnings


def apply_feather(mask: Image.Image, radius: float) -> Image.Image:
    if radius <= 0:
        return mask
    return mask.filter(ImageFilter.GaussianBlur(radius=radius))


def expand_bounds(
    bounds: Tuple[int, int, int, int], size: Tuple[int, int], padding: int
) -> Tuple[int, int, int, int]:
    x, y, width, height = bounds
    image_width, image_height = size
    if padding < 0:
        raise LayerizerError("--padding cannot be negative")
    x0 = max(0, x - padding)
    y0 = max(0, y - padding)
    x1 = min(image_width, x + width + padding)
    y1 = min(image_height, y + height + padding)
    return x0, y0, max(1, x1 - x0), max(1, y1 - y0)


def cutout(source: Image.Image, mask: Image.Image, bounds: Tuple[int, int, int, int]) -> Image.Image:
    alpha = ImageChops.multiply(source.getchannel("A"), mask)
    result = source.copy()
    result.putalpha(alpha)
    x, y, width, height = bounds
    return result.crop((x, y, x + width, y + height))


def transparent_background(source: Image.Image, union_mask: Image.Image) -> Image.Image:
    result = source.copy()
    alpha = ImageChops.multiply(
        source.getchannel("A"), ImageChops.invert(union_mask)
    )
    result.putalpha(alpha)
    return result


def inpaint_background(
    source: Image.Image, union_mask: Image.Image, radius: float
) -> Image.Image:
    try:
        import cv2
        import numpy as np
    except ImportError as exc:
        raise LayerizerError(
            "--background inpaint requires OpenCV and NumPy; use transparent or original"
        ) from exc

    rgb = np.asarray(source.convert("RGB"))
    inpaint_mask = np.asarray(union_mask, dtype=np.uint8)
    bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
    repaired = cv2.inpaint(bgr, inpaint_mask, max(1.0, radius), cv2.INPAINT_TELEA)
    repaired_rgb = cv2.cvtColor(repaired, cv2.COLOR_BGR2RGB)
    result = Image.fromarray(repaired_rgb, mode="RGB").convert("RGBA")
    result.putalpha(source.getchannel("A"))
    return result


def relative_file(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def write_layer_pack(args: argparse.Namespace) -> Dict[str, Any]:
    input_path = args.input.expanduser().resolve()
    output_dir = args.output.expanduser().resolve()
    if not input_path.is_file():
        raise LayerizerError(f"Input image does not exist: {input_path}")
    if input_path == output_dir or output_dir.is_relative_to(input_path):
        raise LayerizerError("Output directory must not be the input image path")

    output_dir.mkdir(parents=True, exist_ok=True)
    layers_dir = output_dir / "layers"
    masks_dir = output_dir / "masks"
    layers_dir.mkdir(exist_ok=True)
    masks_dir.mkdir(exist_ok=True)

    with Image.open(input_path) as loaded:
        source = loaded.convert("RGBA")
    width, height = source.size
    source_hash = sha256_file(input_path)

    if args.mask is not None:
        mask_path = args.mask.expanduser().resolve()
        if not mask_path.is_file():
            raise LayerizerError(f"Mask image does not exist: {mask_path}")
        mask = load_mask(mask_path, source.size)
        method = "provided_mask"
        seed_bbox = None
    elif args.auto:
        seed_bbox = parse_bbox(args.bbox, width, height)
        mask = grabcut_mask(source, seed_bbox)
        method = "grabcut"
    elif args.bbox is not None:
        seed_bbox = parse_bbox(args.bbox, width, height)
        mask = rectangle_mask(source.size, seed_bbox)
        method = "bbox"
    else:
        raise LayerizerError("one of --mask, --bbox, or --auto is required")

    mask = threshold_mask(mask, args.threshold)
    mask = apply_feather(mask, args.feather)
    binary_mask = threshold_mask(mask, 1)
    components, warnings = connected_components(
        binary_mask,
        args.split_components,
        args.min_area_px,
        args.max_components,
    )
    if not components:
        raise LayerizerError("No foreground component was found in the supplied mask")

    settings = {
        "segmentation_method": method,
        "background": args.background,
        "split_components": bool(args.split_components),
        "max_components": args.max_components,
        "min_area_px": args.min_area_px,
        "padding": args.padding,
        "feather": args.feather,
        "threshold": args.threshold,
        "inpaint_radius": args.inpaint_radius,
    }
    result_id = hashlib.sha256(
        (source_hash + json.dumps(settings, sort_keys=True)).encode("utf-8")
    ).hexdigest()[:16]

    candidate_records: List[Dict[str, Any]] = []
    union_mask = Image.new("L", source.size, 0)
    generated_layers: List[Tuple[Tuple[int, int, int, int], Image.Image]] = []
    for index, component_mask in enumerate(components):
        component_bounds = component_mask.getbbox()
        if component_bounds is None:
            continue
        x0, y0, x1, y1 = component_bounds
        bounds = expand_bounds((x0, y0, x1 - x0, y1 - y0), source.size, args.padding)
        layer_mask = apply_feather(component_mask, args.feather)
        layer_name = f"{args.name_prefix}_{index + 1:02d}"
        mask_file = masks_dir / f"{layer_name}.png"
        layer_file = layers_dir / f"{layer_name}.png"
        layer_mask.save(mask_file)
        layer_image = cutout(source, layer_mask, bounds)
        layer_image.save(layer_file)
        union_mask = ImageChops.lighter(union_mask, layer_mask)
        generated_layers.append((bounds, layer_image))

        area = sum(1 for value in component_mask.getdata() if value > 0)
        bounds_area = max(1, (x1 - x0) * (y1 - y0))
        candidate_records.append(
            {
                "id": f"candidate-{index + 1:02d}",
                "name": layer_name,
                "ordinal": index,
                "bounds": {
                    "x": bounds[0],
                    "y": bounds[1],
                    "width": bounds[2],
                    "height": bounds[3],
                },
                "area_pixels": area,
                "coverage": round(area / bounds_area, 6),
                "confidence": None,
                "confidence_source": "not_available",
                "files": {
                    "rgba": relative_file(layer_file, output_dir),
                    "mask": relative_file(mask_file, output_dir),
                },
                "provenance": {
                    "pixels": "source_visible_pixels",
                    "mask": method,
                    "hidden_pixels": "not_recovered",
                },
            }
        )

    if args.background == "transparent":
        background = transparent_background(source, union_mask)
        background_provenance = "source_visible_pixels_only"
        inferred_regions = False
    elif args.background == "original":
        background = source.copy()
        background_provenance = "source_reference_with_foreground"
        inferred_regions = False
    else:
        background = inpaint_background(source, union_mask, args.inpaint_radius)
        background_provenance = "inferred_pixels"
        inferred_regions = True

    background_file = output_dir / "background.png"
    preview_file = output_dir / "preview.png"
    background.save(background_file)
    preview = background.copy()
    for bounds, layer_image in generated_layers:
        preview.alpha_composite(layer_image, dest=(bounds[0], bounds[1]))
    preview.save(preview_file)

    generated_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    manifest: Dict[str, Any] = {
        "schema": SCHEMA,
        "result_identity": result_id,
        "generated_at_utc": generated_at,
        "source": {
            "path": str(input_path),
            "sha256": source_hash,
            "width": width,
            "height": height,
            "mode": "RGBA",
        },
        "settings": settings,
        "seed": {
            "bbox": (
                {
                    "x": seed_bbox[0],
                    "y": seed_bbox[1],
                    "width": seed_bbox[2],
                    "height": seed_bbox[3],
                }
                if seed_bbox is not None
                else None
            )
        },
        "candidates": candidate_records,
        "background": {
            "mode": args.background,
            "file": relative_file(background_file, output_dir),
            "inferred_regions": inferred_regions,
            "provenance": background_provenance,
        },
        "preview_file": relative_file(preview_file, output_dir),
        "warnings": [
            "Hidden pixels cannot be recovered from a single source image.",
            *warnings,
        ],
    }
    manifest_file = output_dir / "manifest.json"
    manifest_file.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return manifest


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        manifest = write_layer_pack(args)
    except LayerizerError as exc:
        print(f"layerizer: error: {exc}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"layerizer: file error: {exc}", file=sys.stderr)
        return 2

    print(f"Created {len(manifest['candidates'])} candidate layer(s)")
    print(f"Manifest: {Path(args.output).resolve() / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
