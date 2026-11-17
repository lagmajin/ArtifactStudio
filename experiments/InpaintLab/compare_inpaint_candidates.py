"""Compare OpenCV inpaint candidates against the frozen R1 baseline gates.

OpenCV 4.13 cv2.inpaint accepts 8-bit 3-channel or 32-bit float 1-channel.
It does not accept float32 RGBA/RGB, so candidates are split into:
  - u8: linear -> sRGB OETF -> 8-bit RGB -> inpaint -> sRGB EOTF -> linear
  - f32: per-channel float32 linear inpaint (HDR preserved; channels processed independently)

Alpha is copied from the source in all candidates. The expected alpha behavior
inside the mask is still an open contract question (see R&D plan section 12).
No build, test, or Artifact runtime is involved.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from analyze_inpaint_baseline import build_regions, region_metrics
from build_inpaint_fixtures import srgb_oetf

COMPARATOR_VERSION = "0.1.0"
GATE_TOLERANCE = 1e-6
CANDIDATES = (
    {"id": "telea_u8_r3", "method": "telea", "domain": "u8", "radius": 3},
    {"id": "ns_u8_r3", "method": "ns", "domain": "u8", "radius": 3},
    {"id": "ns_f32_r1", "method": "ns", "domain": "f32", "radius": 1},
    {"id": "ns_f32_r3", "method": "ns", "domain": "f32", "radius": 3},
    {"id": "ns_f32_r5", "method": "ns", "domain": "f32", "radius": 5},
    {"id": "ns_f32_r7", "method": "ns", "domain": "f32", "radius": 7},
    {"id": "telea_f32_r3", "method": "telea", "domain": "f32", "radius": 3},
)
LINE_ROI = (slice(126, 130), slice(96, 161))


def srgb_eotf(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(
        x <= 0.04045,
        x / 12.92,
        np.power((x + 0.055) / 1.055, 2.4),
    )


def linear_to_u8(rgb: np.ndarray) -> np.ndarray:
    return np.round(np.clip(srgb_oetf(rgb), 0.0, 1.0) * 255.0).astype(np.uint8)


def u8_to_linear(image: np.ndarray) -> np.ndarray:
    return srgb_eotf(image.astype(np.float32) / 255.0).astype(np.float32)


def run_candidate(source: np.ndarray, mask: np.ndarray, spec: dict) -> np.ndarray:
    flag = cv2.INPAINT_TELEA if spec["method"] == "telea" else cv2.INPAINT_NS
    rgb = source[..., :3]
    radius = int(spec["radius"])

    if spec["domain"] == "u8":
        filled = cv2.inpaint(linear_to_u8(rgb), mask, radius, flag)
        out_rgb = u8_to_linear(filled)
    else:
        out_rgb = np.empty_like(rgb)
        for channel in range(3):
            out_rgb[..., channel] = cv2.inpaint(
                np.ascontiguousarray(rgb[..., channel]), mask, radius, flag
            )

    out = np.empty_like(source)
    out[..., :3] = out_rgb
    out[..., 3] = source[..., 3]
    return out


def roi_mae(output: np.ndarray, reference: np.ndarray, roi: tuple) -> float:
    diff = np.abs(output[roi][..., :3] - reference[roi][..., :3])
    return float(np.mean(diff)) if diff.size else 0.0


def region_mae(metrics: dict, region_name: str) -> float:
    return float(metrics["regions"][region_name]["rgb_abs_diff"]["mae"])


def write_contact_sheet(path: Path, source: np.ndarray, result: np.ndarray, reference: np.ndarray) -> None:
    def to_display(rgba: np.ndarray) -> np.ndarray:
        rgb = srgb_oetf(np.clip(rgba[..., :3], 0.0, 1.0))
        return np.round(rgb * 255.0).astype(np.uint8)

    separator = np.full((source.shape[0], 4, 3), 24, dtype=np.uint8)
    sheet = np.concatenate(
        [
            to_display(source),
            separator,
            to_display(result),
            separator,
            to_display(reference),
        ],
        axis=1,
    )
    if not cv2.imwrite(str(path), sheet):
        raise RuntimeError(f"failed to write {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, default=Path("experiments/InpaintLab/fixtures/inpaint-rd1"))
    parser.add_argument("--baseline", type=Path, default=Path("experiments/InpaintLab/captures/baseline-rd1/baseline-metrics.json"))
    parser.add_argument("--output", type=Path, default=Path("experiments/InpaintLab/captures/candidates-rd2"))
    args = parser.parse_args()

    fixture_manifest = json.loads((args.fixtures / "manifest.json").read_text(encoding="utf-8"))
    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    baseline_by_id = {entry["id"]: entry for entry in baseline["fixtures"]}
    sequence_ids = fixture_manifest["groups"]["sequence_moving_object"]
    args.output.mkdir(parents=True, exist_ok=True)

    frames: dict[str, dict[str, np.ndarray]] = {}
    for fixture_id in sequence_ids:
        folder = args.fixtures / fixture_id
        frames[fixture_id] = {
            "source": np.load(folder / "source.npy"),
            "mask": np.load(folder / "mask.npy"),
            "reference": np.load(folder / "ground_truth.npy"),
        }

    report = {
        "comparator_version": COMPARATOR_VERSION,
        "fixture_bundle": fixture_manifest["bundle"],
        "baseline_metrics_sha256": hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
        "gate_tolerance": GATE_TOLERANCE,
        "gates": {
            "G1": "mask_out RGB/alpha MAE and max <= 1e-6",
            "G2": "output non-finite RGB/alpha == 0",
            "G3": "sequence static-background frame MAE <= 1e-6",
            "G4": "mask_in MAE < baseline mask_in MAE for each fixture",
            "G5": "boundary_band MAE < baseline boundary_band MAE for each fixture",
            "G6": "thin_line line ROI MAE < baseline, plus visual contact-sheet review",
        },
        "candidates": [],
    }

    for spec in CANDIDATES:
        candidate_folder = args.output / spec["id"]
        candidate_folder.mkdir(parents=True, exist_ok=True)
        entry = {"id": spec["id"], "spec": spec, "fixtures": [], "gates": {}}
        gate_failures: dict[str, list[str]] = {f"G{i}": [] for i in range(1, 7)}
        gate_unverified: dict[str, list[str]] = {f"G{i}": [] for i in range(1, 7)}

        for fixture in fixture_manifest["fixtures"]:
            fixture_id = fixture["id"]
            folder = args.fixtures / fixture_id
            source = np.load(folder / "source.npy")
            mask = np.load(folder / "mask.npy")
            reference = np.load(folder / "ground_truth.npy")
            result = run_candidate(source, mask, spec)

            regions = build_regions(mask, reference)
            metrics = region_metrics(result, reference, regions["full"])
            per_region = {
                name: region_metrics(result, reference, region)
                for name, region in regions.items()
                if np.any(region)
            }

            baseline_entry = baseline_by_id[fixture_id]
            mask_out = per_region["mask_out"]
            mask_out_ok = (
                mask_out["rgb_abs_diff"]["mae"] <= GATE_TOLERANCE
                and mask_out["rgb_abs_diff"]["max"] <= GATE_TOLERANCE
                and mask_out["alpha_abs_diff"]["mae"] <= GATE_TOLERANCE
                and mask_out["alpha_abs_diff"]["max"] <= GATE_TOLERANCE
            )
            finite_ok = metrics["non_finite_rgb"] == 0 and metrics["non_finite_alpha"] == 0
            mask_in_ok = (
                per_region["mask_in"]["rgb_abs_diff"]["mae"]
                < region_mae(baseline_entry, "mask_in")
            )
            boundary_ok = (
                per_region["boundary_band"]["rgb_abs_diff"]["mae"]
                < region_mae(baseline_entry, "boundary_band")
            )

            if not mask_out_ok:
                gate_failures["G1"].append(fixture_id)
            if not finite_ok:
                gate_failures["G2"].append(fixture_id)
            if not mask_in_ok:
                gate_failures["G4"].append(fixture_id)
            if not boundary_ok:
                gate_failures["G5"].append(fixture_id)

            fixture_result = {
                "id": fixture_id,
                "regions": per_region,
                "non_finite": {
                    "rgb": metrics["non_finite_rgb"],
                    "alpha": metrics["non_finite_alpha"],
                },
                "gates": {
                    "G1_mask_out_unchanged": mask_out_ok,
                    "G2_finite": finite_ok,
                    "G4_mask_in_improved": mask_in_ok,
                    "G5_boundary_improved": boundary_ok,
                },
                "baseline_mask_in_mae": region_mae(baseline_entry, "mask_in"),
                "baseline_boundary_mae": region_mae(baseline_entry, "boundary_band"),
            }

            if fixture_id == "thin_line":
                baseline_roi = roi_mae(source, reference, LINE_ROI)
                candidate_roi = roi_mae(result, reference, LINE_ROI)
                line_ok = candidate_roi < baseline_roi
                fixture_result["line_roi"] = {
                    "baseline_mae": baseline_roi,
                    "candidate_mae": candidate_roi,
                    "improved": line_ok,
                }
                if not line_ok:
                    gate_failures["G6"].append(fixture_id)

            if not fixture_id.startswith("sequence_"):
                write_contact_sheet(
                    candidate_folder / f"{fixture_id}-contact.png",
                    source,
                    result,
                    reference,
                )

            entry["fixtures"].append(fixture_result)

        results_by_id = {item["id"]: item for item in entry["fixtures"]}
        temporal = []
        for previous_id, current_id in zip(sequence_ids, sequence_ids[1:]):
            previous = frames[previous_id]
            current = frames[current_id]
            previous_result = run_candidate(previous["source"], previous["mask"], spec)
            current_result = run_candidate(current["source"], current["mask"], spec)
            static = (previous["mask"] == 0) & (current["mask"] == 0)
            diff = np.abs(
                previous_result[..., :3][static].astype(np.float64)
                - current_result[..., :3][static].astype(np.float64)
            )
            pair = {
                "pair": [previous_id, current_id],
                "mae": float(np.mean(diff)),
                "max": float(np.max(diff)),
            }
            temporal.append(pair)
            if pair["mae"] > GATE_TOLERANCE:
                gate_failures["G3"].append(previous_id)
        entry["temporal_static_background"] = temporal

        gate_unverified["G6"].append("visual contact-sheet review not yet recorded")
        for gate, failures in gate_failures.items():
            entry["gates"][gate] = {
                "status": "fail" if failures else ("unverified" if gate_unverified[gate] else "pass"),
                "failures": failures,
                "unverified": gate_unverified[gate],
            }

        report["candidates"].append(entry)
        summary = ", ".join(
            f"{gate}={entry['gates'][gate]['status']}" for gate in entry["gates"]
        )
        print(f"{spec['id']}: {summary}")

    report_path = args.output / "candidate-metrics.json"
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {report_path}")


if __name__ == "__main__":
    main()
