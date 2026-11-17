"""Compute baseline image statistics for the Content-Aware Fill R&D.

Baseline means "inpaint not applied": source.npy is the output under test and
ground_truth.npy is the synthetic clean plate. No threshold is declared here;
numbers are descriptive inputs for the R1 acceptance discussion.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from build_inpaint_fixtures import srgb_oetf

ANALYZER_VERSION = "0.1.0"
DIAGNOSTIC_ABSOLUTE_LEVELS = (0.01, 0.1, 0.5)


def percentile(values: np.ndarray, q: float) -> float:
    if values.size == 0:
        return 0.0
    return float(np.percentile(values, q))


def region_metrics(output: np.ndarray, reference: np.ndarray, region: np.ndarray) -> dict:
    rgb_out = output[..., :3]
    rgb_ref = reference[..., :3]
    a_out = output[..., 3]
    a_ref = reference[..., 3]

    m = region
    diff = np.abs(rgb_out[m] - rgb_ref[m])
    finite = np.isfinite(diff)
    diff = diff[finite]

    def distribution(values: np.ndarray) -> dict:
        if values.size == 0:
            return {"min": 0.0, "mean": 0.0, "median": 0.0, "p99": 0.0, "max": 0.0}
        return {
            "min": float(np.min(values)),
            "mean": float(np.mean(values)),
            "median": float(np.median(values)),
            "p99": percentile(values, 99.0),
            "max": float(np.max(values)),
        }

    result = {
        "pixels": int(np.count_nonzero(m)),
        "non_finite_rgb": int(np.count_nonzero(~np.isfinite(rgb_out[m]))),
        "non_finite_alpha": int(np.count_nonzero(~np.isfinite(a_out[m]))),
        "rgb_abs_diff": {
            "mae": float(np.mean(diff)) if diff.size else 0.0,
            "rmse": float(np.sqrt(np.mean(np.square(diff)))) if diff.size else 0.0,
            "max": float(np.max(diff)) if diff.size else 0.0,
            "p99": percentile(diff, 99.0),
        },
        "alpha_abs_diff": {
            "mae": float(np.mean(np.abs(a_out[m] - a_ref[m]))) if np.any(m) else 0.0,
            "max": float(np.max(np.abs(a_out[m] - a_ref[m]))) if np.any(m) else 0.0,
        },
        "exceed_rate": {
            str(level): float(np.mean(diff > level)) if diff.size else 0.0
            for level in DIAGNOSTIC_ABSOLUTE_LEVELS
        },
        "output_distribution": distribution(rgb_out[m].astype(np.float64).ravel()),
        "reference_distribution": distribution(rgb_ref[m].astype(np.float64).ravel()),
    }
    return result


def build_regions(mask: np.ndarray, ground_truth: np.ndarray) -> dict[str, np.ndarray]:
    h, w = mask.shape
    filled = mask > 0
    kernel = np.ones((5, 5), dtype=np.uint8)
    dilated = cv2.dilate(mask, kernel, iterations=1) > 0
    eroded = cv2.erode(mask, kernel, iterations=1) > 0
    boundary = dilated & ~eroded

    gray = np.mean(ground_truth[..., :3], axis=2)
    gray_sq = cv2.blur(gray * gray, (7, 7))
    local_var = np.maximum(gray_sq - cv2.blur(gray, (7, 7)) ** 2, 0.0)
    flat = (local_var < 0.01) & ~dilated

    hdr = np.max(ground_truth[..., :3], axis=2) > 1.0

    return {
        "full": np.ones((h, w), dtype=bool),
        "mask_in": filled,
        "mask_out": ~filled,
        "boundary_band": boundary,
        "flat": flat,
        "hdr": hdr,
    }


def write_contact_sheet(path: Path, source: np.ndarray, ground_truth: np.ndarray) -> None:
    diff = np.abs(source[..., :3] - ground_truth[..., :3])
    diff_rgba = np.empty_like(source)
    diff_rgba[..., :3] = diff
    diff_rgba[..., 3] = 1.0

    panels = [source, ground_truth, diff_rgba]
    images = []
    for gain in (1.0, 1.0, 4.0):
        rgb = srgb_oetf(np.clip(panels[len(images)][..., :3] * gain, 0.0, 1.0))
        image = np.round(rgb * 255.0).astype(np.uint8)
        images.append(image)

    separator = np.full((images[0].shape[0], 4, 3), 24, dtype=np.uint8)
    sheet = np.concatenate([images[0], separator, images[1], separator, images[2]], axis=1)
    if not cv2.imwrite(str(path), sheet):
        raise RuntimeError(f"failed to write {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixtures",
        type=Path,
        default=Path("experiments/InpaintLab/fixtures/inpaint-rd1"),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("experiments/InpaintLab/captures/baseline-rd1"),
    )
    args = parser.parse_args()

    manifest_path = args.fixtures / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    args.output.mkdir(parents=True, exist_ok=True)

    fixture_results = []
    for entry in manifest["fixtures"]:
        folder = args.fixtures / entry["id"]
        source = np.load(folder / "source.npy")
        mask = np.load(folder / "mask.npy")
        ground_truth = np.load(folder / "ground_truth.npy")
        regions = build_regions(mask, ground_truth)

        result = {
            "id": entry["id"],
            "purpose": entry["purpose"],
            "regions": {
                name: region_metrics(source, ground_truth, region)
                for name, region in regions.items()
                if np.any(region)
            },
        }
        fixture_results.append(result)

        if not entry["id"].startswith("sequence_"):
            write_contact_sheet(args.output / f"{entry['id']}-contact.png", source, ground_truth)

    sequence_ids = manifest["groups"]["sequence_moving_object"]
    sequence_frames = []
    for fixture_id in sequence_ids:
        folder = args.fixtures / fixture_id
        sequence_frames.append({
            "id": fixture_id,
            "source": np.load(folder / "source.npy"),
            "mask": np.load(folder / "mask.npy"),
        })

    temporal = []
    for previous, current in zip(sequence_frames, sequence_frames[1:]):
        static = (previous["mask"] == 0) & (current["mask"] == 0)
        diff = np.abs(
            previous["source"][..., :3][static].astype(np.float64)
            - current["source"][..., :3][static].astype(np.float64)
        )
        temporal.append({
            "pair": [previous["id"], current["id"]],
            "static_pixels": int(np.count_nonzero(static)),
            "mae": float(np.mean(diff)),
            "max": float(np.max(diff)),
        })

    report = {
        "analyzer_version": ANALYZER_VERSION,
        "fixture_bundle": manifest["bundle"],
        "fixture_manifest_sha256": None,
        "meaning": "Baseline = inpaint not applied. Descriptive only; no pass/fail threshold declared.",
        "diagnostic_absolute_levels": list(DIAGNOSTIC_ABSOLUTE_LEVELS),
        "environment": manifest["environment"],
        "generator": manifest["generator_version"],
        "seed": manifest["seed"],
        "fixtures": fixture_results,
        "temporal_static_background": temporal,
    }
    report["fixture_manifest_sha256"] = hashlib.sha256(manifest_path.read_bytes()).hexdigest()

    report_path = args.output / "baseline-metrics.json"
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {report_path} ({len(fixture_results)} fixtures, {len(temporal)} frame pairs)")


if __name__ == "__main__":
    main()
