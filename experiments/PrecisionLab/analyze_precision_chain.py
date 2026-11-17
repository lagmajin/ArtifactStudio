"""Compare linear/sRGB/premultiplied conversion chains against a linear reference.

Models the chains described in docs/analysis/IMAGE_BUFFER_PRECISION_AUDIT_2026-08-13.md.
This is a source-equation study only: no Artifact code, GPU dispatch, or runtime
capture is involved, and no pass/fail threshold is declared here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np

from build_precision_fixtures import srgb_oetf

ANALYZER_VERSION = "0.1.0"
BACKGROUNDS = {
    "black": (0.0, 0.0, 0.0),
    "gray18": (0.18, 0.18, 0.18),
    "white": (1.0, 1.0, 1.0),
}


def srgb_eotf(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(
        x <= 0.04045,
        x / 12.92,
        np.power((x + 0.055) / 1.055, 2.4),
    )


def quantize8(x: np.ndarray) -> np.ndarray:
    return np.round(np.clip(x, 0.0, 1.0) * 255.0) / 255.0


def describe_diff(diff: np.ndarray) -> dict:
    if diff.size == 0:
        return {"mae": 0.0, "p99": 0.0, "max": 0.0, "exceed_1e3": 0.0}
    return {
        "mae": float(np.mean(diff)),
        "p99": float(np.percentile(diff, 99.0)),
        "max": float(np.max(diff)),
        "exceed_1e3": float(np.mean(diff > 1.0e-3)),
    }


def build_chains(rgba: np.ndarray, bg_rgb: tuple[float, float, float]) -> dict[str, np.ndarray]:
    linear = rgba[..., :3]
    alpha = rgba[..., 3:4]
    bg = np.asarray(bg_rgb, dtype=np.float32)[None, None, :]

    encoded = srgb_oetf(linear)
    premul_encoded = encoded * alpha
    premul_encoded8 = quantize8(premul_encoded)
    alpha8 = quantize8(alpha)

    reference = linear * alpha + bg * (1.0 - alpha)

    naive8 = srgb_eotf(premul_encoded8) + bg * (1.0 - alpha8)
    guard = np.maximum(alpha8, 1.0e-4)
    correct8 = srgb_eotf(premul_encoded8 / guard) * alpha8 + bg * (1.0 - alpha8)
    naive_float = srgb_eotf(premul_encoded) + bg * (1.0 - alpha)
    clamped = np.clip(linear, 0.0, 1.0) * alpha + bg * (1.0 - alpha)

    return {
        "reference": reference,
        "naive8": naive8,
        "correct8": correct8,
        "naive_float": naive_float,
        "clamped": clamped,
    }


def banding_stats(rgba: np.ndarray) -> dict:
    encoded8 = quantize8(srgb_oetf(rgba[..., :3]))
    row = encoded8[rgba.shape[0] // 2, :, 0]
    levels = np.unique(np.round(row * 255.0).astype(np.int32))
    steps = np.abs(np.diff(row.astype(np.float64)))
    return {
        "distinct_levels": int(levels.size),
        "max_adjacent_step_encoded": float(np.max(steps)) if steps.size else 0.0,
    }


def write_contact_sheet(path: Path, chains: dict[str, np.ndarray]) -> None:
    panels = [chains[name] for name in ("reference", "naive8", "correct8")]

    def to_display(rgb: np.ndarray) -> np.ndarray:
        return np.round(srgb_oetf(rgb) * 255.0).astype(np.uint8)

    images = [to_display(panel) for panel in panels]
    separator = np.full((images[0].shape[0], 4, 3), 24, dtype=np.uint8)
    sheet = np.concatenate(
        [images[0], separator, images[1], separator, images[2]], axis=1
    )
    if not cv2.imwrite(str(path), sheet):
        raise RuntimeError(f"failed to write {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path,
                        default=Path("experiments/PrecisionLab/fixtures/precision-rd1"))
    parser.add_argument("--output", type=Path,
                        default=Path("experiments/PrecisionLab/captures/chain-rd1"))
    args = parser.parse_args()

    manifest_path = args.fixtures / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    args.output.mkdir(parents=True, exist_ok=True)

    report = {
        "analyzer_version": ANALYZER_VERSION,
        "fixture_bundle": manifest["bundle"],
        "fixture_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "environment": manifest["environment"],
        "meaning": "Source-equation comparison only. No artifact runtime, no pass/fail threshold.",
        "fixtures": [],
    }

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
            "purpose": entry["purpose"],
            "backgrounds": {},
            "clip_rate": float(np.mean(linear > 1.0)),
            "non_finite": int(np.count_nonzero(~np.isfinite(rgba))),
        }
        if entry["id"] == "banding_ramp":
            fixture_result["banding"] = banding_stats(rgba)

        for bg_name, bg_rgb in BACKGROUNDS.items():
            chains = build_chains(rgba, bg_rgb)
            fixture_result["backgrounds"][bg_name] = {
                chain_name: {
                    region_name: describe_diff(
                        np.abs(chains[chain_name][region] - chains["reference"][region])
                    )
                    for region_name, region in regions.items()
                    if np.any(region)
                }
                for chain_name in ("naive8", "correct8", "naive_float", "clamped")
            }

        report["fixtures"].append(fixture_result)
        gray_chains = build_chains(rgba, BACKGROUNDS["gray18"])
        write_contact_sheet(args.output / f"{entry['id']}-contact.png", gray_chains)

    report_path = args.output / "chain-metrics.json"
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Wrote {report_path} ({len(report['fixtures'])} fixtures)")


if __name__ == "__main__":
    main()
