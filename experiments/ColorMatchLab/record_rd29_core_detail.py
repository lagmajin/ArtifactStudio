"""Record exploratory spatial-detail metrics for saved RD27 Core PNG outputs."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
RD27 = ROOT / "captures" / "rd27_core_natural_scene"
RD18 = ROOT / "captures" / "rd18_lamp_white_roi"
OUTPUT = ROOT / "captures" / "rd29_core_detail"
METHODS = ("Reinhard", "MeanStddev", "Histogram")
PAD = 18


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_srgb(path: Path) -> tuple[np.ndarray, np.ndarray]:
    with Image.open(path) as image:
        rgb8 = np.asarray(image.convert("RGB"), dtype=np.uint8)
    encoded = rgb8.astype(np.float64) / 255.0
    linear = np.where(encoded <= 0.04045,
                      encoded / 12.92,
                      ((encoded + 0.055) / 1.055) ** 2.4)
    y = (0.2126 * linear[..., 0] +
         0.7152 * linear[..., 1] +
         0.0722 * linear[..., 2])
    return rgb8, y


def gradients(y: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    gx = np.zeros_like(y)
    gy = np.zeros_like(y)
    gx[:, 1:-1] = 0.5 * (y[:, 2:] - y[:, :-2])
    gy[1:-1, :] = 0.5 * (y[2:, :] - y[:-2, :])
    magnitude = np.hypot(gx, gy)
    return gx, gy, magnitude


def region_masks() -> tuple[dict[str, np.ndarray], dict[str, dict[str, object]]]:
    with Image.open(RD27 / "source.png") as image:
        width, height = image.size
    masks: dict[str, np.ndarray] = {"whole_frame": np.ones((height, width), dtype=bool)}
    provenance: dict[str, dict[str, object]] = {"whole_frame": {"kind": "all pixels"}}
    for name in ("left_lampshade", "right_lampshade"):
        mask_path = RD18 / f"{name}_mask.png"
        with Image.open(mask_path) as image:
            mask_image = image.convert("L")
            mask = np.asarray(mask_image, dtype=np.uint8) > 0
            bbox = mask_image.getbbox()
        if mask.shape != (height, width) or bbox is None:
            raise RuntimeError(f"Invalid RD18 mask: {mask_path}")
        masks[name] = mask
        provenance[name] = {"kind": "existing source-reviewed binary mask",
                            "path": str(mask_path.relative_to(ROOT)),
                            "sha256": sha(mask_path),
                            "pixel_count": int(mask.sum())}
        left, top, right, bottom = bbox
        x0, y0 = max(0, left - PAD), max(0, top - PAD)
        x1, y1 = min(width, right + PAD), min(height, bottom + PAD)
        context = np.zeros((height, width), dtype=bool)
        context[y0:y1, x0:x1] = True
        context_name = f"{name}_context_{PAD}px"
        masks[context_name] = context
        provenance[context_name] = {"kind": "rectangle derived from existing mask bbox",
                                    "source_mask": str(mask_path.relative_to(ROOT)),
                                    "padding_pixels": PAD,
                                    "xyxy": [x0, y0, x1, y1],
                                    "pixel_count": int(context.sum())}
    return masks, provenance


def main() -> None:
    source_path = RD27 / "source.png"
    source_rgb, source_y = read_srgb(source_path)
    source_gx, source_gy, source_mag = gradients(source_y)
    masks, region_provenance = region_masks()
    rows: list[dict[str, object]] = []
    output_provenance: dict[str, dict[str, str]] = {}

    for method in METHODS:
        path = RD27 / f"{method}_i1.0.png"
        rgb8, y = read_srgb(path)
        if rgb8.shape != source_rgb.shape:
            raise RuntimeError(f"RD29 input dimensions differ: {path}")
        gx, gy, magnitude = gradients(y)
        output_provenance[method] = {"path": str(path.relative_to(ROOT)), "sha256": sha(path)}
        for region, mask in masks.items():
            valid = mask.copy()
            valid[[0, -1], :] = False
            valid[:, [0, -1]] = False
            src_m = source_mag[valid]
            out_m = magnitude[valid]
            positive_src_m = src_m[src_m > 0.0]
            if positive_src_m.size == 0:
                raise RuntimeError(f"No positive source gradients in region {region}")
            threshold = float(np.percentile(positive_src_m, 90.0))
            edges = valid & (source_mag >= threshold) & (source_mag > 0.0)
            src_norm = np.hypot(source_gx[edges], source_gy[edges])
            out_norm = np.hypot(gx[edges], gy[edges])
            denominator = np.maximum(src_norm * out_norm, 1.0e-15)
            cosine = np.clip((source_gx[edges] * gx[edges] +
                              source_gy[edges] * gy[edges]) / denominator, -1.0, 1.0)
            angle = np.degrees(np.arccos(cosine))
            src_endpoint = np.any((source_rgb == 0) | (source_rgb == 255), axis=2)[valid]
            out_endpoint = np.any((rgb8 == 0) | (rgb8 == 255), axis=2)[valid]
            rows.append({
                "method": method,
                "intensity": 1.0,
                "region": region,
                "pixels": int(valid.sum()),
                "source_gradient_mean": float(src_m.mean()),
                "source_gradient_median": float(np.median(src_m)),
                "source_gradient_p95": float(np.percentile(src_m, 95.0)),
                "output_gradient_mean": float(out_m.mean()),
                "output_gradient_median": float(np.median(out_m)),
                "output_gradient_p95": float(np.percentile(out_m, 95.0)),
                "output_source_gradient_mean_ratio": float(out_m.mean() / max(float(src_m.mean()), 1.0e-15)),
                "output_source_gradient_p95_ratio": float(np.percentile(out_m, 95.0) / max(float(np.percentile(src_m, 95.0)), 1.0e-15)),
                "source_edge_threshold_p90": threshold,
                "source_edge_pixels": int(edges.sum()),
                "edge_orientation_mean_cosine": float(cosine.mean()),
                "edge_orientation_median_degrees": float(np.median(angle)),
                "edge_orientation_p95_degrees": float(np.percentile(angle, 95.0)),
                "source_endpoint_pixel_fraction": float(src_endpoint.mean()),
                "output_endpoint_pixel_fraction": float(out_endpoint.mean()),
            })

    OUTPUT.mkdir(parents=True, exist_ok=True)
    metrics_path = OUTPUT / "detail_metrics.csv"
    with metrics_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    manifest = {
        "rd": "rd29-rd27-actual-core-detail-response",
        "status": "retrospective descriptive diagnostic; no acceptance gate",
        "input_pair": {
            "source": str(source_path.relative_to(ROOT)),
            "source_sha256": sha(source_path),
            "source_reference_role": "Middlebury RAW-derived source; no ground-truth role in detail comparison",
            "reference_not_used_for_detail_scoring": True,
            "dimensions": list(source_rgb.shape[1::-1]),
        },
        "conditions": {
            "methods": list(METHODS), "intensity": 1.0,
            "source_and_outputs": output_provenance,
            "regions": region_provenance,
            "display_encoding_assumption": "PNG RGB values interpreted as sRGB, decoded with IEC 61966-2-1 transfer curve",
            "luminance": "linear Rec.709 Y = 0.2126 R + 0.7152 G + 0.0722 B",
            "gradient": "centered two-pixel finite differences on linear Y; image border excluded",
            "edge_orientation": "source-only positive regional gradient magnitudes; threshold is their p90; zero-gradient pixels are excluded; report mean cosine and median/p95 angle between source/output gradient vectors",
            "endpoint_occupancy": "fraction of pixels where any encoded RGB channel equals 0 or 255 in the saved 8-bit PNG; endpoint occupancy alone does not establish that clipping occurred",
            "prior_review": "RD27 candidate outputs had already been inspected; RD29 is retrospective and not blind",
        },
        "metrics_csv": metrics_path.name,
        "metrics_sha256": sha(metrics_path),
        "implementation": {"script": str(Path(__file__).relative_to(ROOT)),
                            "script_sha256": sha(Path(__file__))},
        "limitations": [
            "uses saved 8-bit PNGs, not retained Core float buffers",
            "only the whole frame and small lampshade/context ROIs are analyzed; no skin or fine-texture reference ROI",
            "gradient changes mix color/tone remapping and detail visibility; this is not a perceptual quality score",
            "camera JPEG is not a colorimetric or detail ground truth and is not used for detail scoring",
            "single scene, strength 1.0, one existing unblinded visual review; no thresholds or method ranking",
            "does not evaluate banding at other bit depths, app/GPU route, or independent review",
        ],
        "acceptance": False,
        "rows": rows,
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"rows": len(rows), "metrics_sha256": sha(metrics_path),
                      "manifest_sha256": sha(manifest_path)}, indent=2))


if __name__ == "__main__":
    main()
