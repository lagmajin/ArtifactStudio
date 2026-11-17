"""Separate clamp / conversion-order / 8-bit-quantization contributions (R2).

Stepwise model using the chains from analyze_precision_chain.py:
  reference -> clamped      : HDR clamp only
  clamped   -> naive_float  : premultiplied sRGB order error (no 8-bit quant)
  naive_float -> naive8     : 8-bit quantization added

MAEs are not strictly additive; the residual is reported as a diagnostic, not
as an error budget. Gates were frozen in the R&D plan before this comparison.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from analyze_precision_chain import (
    BACKGROUNDS,
    banding_stats,
    build_chains,
    describe_diff,
)

ALPHA_EDGE_GATE = 0.01
TRANSPARENT_GATE = 1.0e-3


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path,
                        default=Path("experiments/PrecisionLab/fixtures/precision-rd1"))
    parser.add_argument("--baseline", type=Path,
                        default=Path("experiments/PrecisionLab/captures/chain-rd1/chain-metrics.json"))
    parser.add_argument("--output", type=Path,
                        default=Path("experiments/PrecisionLab/captures/contributions-rd2"))
    args = parser.parse_args()

    manifest_path = args.fixtures / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    args.output.mkdir(parents=True, exist_ok=True)

    report = {
        "fixture_bundle": manifest["bundle"],
        "baseline_metrics_sha256": hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
        "gates": {
            "G1": f"correct8 alpha-edge MAE <= {ALPHA_EDGE_GATE}",
            "G2": "non-finite values == 0",
            "G3": f"correct8 transparent-region max diff <= {TRANSPARENT_GATE}",
            "G4": "HDR clip rate recorded (no pass/fail)",
            "G5": "banding levels and max step recorded",
            "G6": "visual contact-sheet review recorded in R1 (user judgment pending)",
        },
        "fixtures": [],
        "gate_results": {},
    }
    gate_failures = {"G1": [], "G2": [], "G3": []}

    for entry in manifest["fixtures"]:
        folder = args.fixtures / entry["id"]
        rgba = np.load(folder / "source.npy")
        alpha = rgba[..., 3]
        linear = rgba[..., :3]
        regions = {
            "full": np.ones(alpha.shape, dtype=bool),
            "alpha_edge": (alpha > 0.0) & (alpha < 1.0),
            "transparent": alpha == 0.0,
            "hdr": np.max(linear, axis=2) > 1.0,
        }

        fixture_result = {
            "id": entry["id"],
            "clip_rate": float(np.mean(linear > 1.0)),
            "non_finite": int(np.count_nonzero(~np.isfinite(rgba))),
            "backgrounds": {},
        }
        if entry["id"] == "banding_ramp":
            fixture_result["banding"] = banding_stats(rgba)

        for bg_name, bg_rgb in BACKGROUNDS.items():
            chains = build_chains(rgba, bg_rgb)
            reference = chains["reference"]
            steps = {
                "clamp": chains["clamped"],
                "order": chains["naive_float"],
                "quantization": chains["naive8"],
                "correct_order_8bit": chains["correct8"],
            }
            prev = reference
            step_images = {}
            for name in ("clamp", "order", "quantization"):
                step_images[name] = np.abs(steps[name] - prev)
                prev = steps[name]

            fixture_result["backgrounds"][bg_name] = {
                region_name: {
                    "clamp_mae": describe_diff(step_images["clamp"][region])["mae"],
                    "order_mae": describe_diff(step_images["order"][region])["mae"],
                    "quantization_mae": describe_diff(step_images["quantization"][region])["mae"],
                    "total_mae": describe_diff(np.abs(chains["naive8"][region] - reference[region]))["mae"],
                    "correct8_mae": describe_diff(np.abs(chains["correct8"][region] - reference[region]))["mae"],
                    "correct8_max": describe_diff(np.abs(chains["correct8"][region] - reference[region]))["max"],
                }
                for region_name, region in regions.items()
                if np.any(region)
            }

        gray = fixture_result["backgrounds"]["gray18"]
        if "alpha_edge" in gray and gray["alpha_edge"]["correct8_mae"] > ALPHA_EDGE_GATE:
            gate_failures["G1"].append(entry["id"])
        if "transparent" in gray and gray["transparent"]["correct8_max"] > TRANSPARENT_GATE:
            gate_failures["G3"].append(entry["id"])

        if fixture_result["non_finite"] != 0:
            gate_failures["G2"].append(entry["id"])
        report["fixtures"].append(fixture_result)

    for gate, failures in gate_failures.items():
        report["gate_results"][gate] = {
            "status": "fail" if failures else "pass",
            "failures": failures,
        }
    report["gate_results"]["G4"] = {"status": "unverified", "note": "record only"}
    report["gate_results"]["G5"] = {"status": "unverified", "note": "record only"}
    report["gate_results"]["G6"] = {"status": "unverified", "note": "user judgment pending"}

    report_path = args.output / "contributions.json"
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    for fixture in report["fixtures"]:
        gray = fixture["backgrounds"]["gray18"]
        summary = ", ".join(
            f"{region}:clamp={stats['clamp_mae']:.5f}/order={stats['order_mae']:.5f}/quant={stats['quantization_mae']:.5f}"
            for region, stats in gray.items()
        )
        print(f"{fixture['id']}: {summary}")
    print(f"gates: " + ", ".join(
        f"{gate}={value['status']}" for gate, value in report["gate_results"].items()
    ))
    print(f"Wrote {report_path}")


if __name__ == "__main__":
    main()
