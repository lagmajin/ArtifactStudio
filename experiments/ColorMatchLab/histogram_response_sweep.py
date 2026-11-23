"""Sweep one synthetic neutral patch through the inspected Histogram mirror."""
from __future__ import annotations

import csv
import hashlib
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import apply_histogram, sha, CORE_SOURCE, EXPECTED_CORE_SHA256
from white_axis_study import encode_srgb, linear_to_lab

RD0 = ROOT / "captures" / "rd0"
OUT = ROOT / "captures" / "rd3_histogram_sweep"
LEVELS = np.linspace(0.84, 0.98, 141)
BOX = (384, 218, 492, 304)
SCALE = np.array([1.055, 0.995, 0.87])


def histogram_mapping_diagnostic(src_encoded: np.ndarray, ref_encoded: np.ndarray,
                                 patch_rgb: np.ndarray) -> dict:
    result = {}
    # The arrays in this offline study use semantic RGB order. AutoColorMatcher's
    # raw-buffer indices are BGRA, but its channel-wise mappings are separable;
    # report the semantic RGB channel values here.
    for index, channel in enumerate((0, 1, 2)):
        source = src_encoded[..., channel]
        reference = ref_encoded[..., channel]
        src_bins = np.clip((source * 255).astype(np.int32), 0, 255)
        ref_bins = np.clip((reference * 255).astype(np.int32), 0, 255)
        src_hist = np.bincount(src_bins.ravel(), minlength=256)
        ref_hist = np.bincount(ref_bins.ravel(), minlength=256)
        src_cdf = np.cumsum(src_hist, dtype=np.float64) / src_bins.size
        ref_cdf = np.cumsum(ref_hist, dtype=np.float64) / ref_bins.size
        source_bin = int(np.clip(patch_rgb[index] * 255, 0, 255))
        source_cdf = float(src_cdf[source_bin])
        distances = np.abs(ref_cdf - source_cdf)
        mapped_bin = int(np.argmin(distances))
        result["RGB"[index]] = {
            "source_patch_bin": source_bin,
            "source_patch_cdf": source_cdf,
            "mapped_reference_bin": mapped_bin,
            "mapped_reference_encoded": mapped_bin / 255.0,
            "nearest_cdf_distance": float(distances[mapped_bin]),
            "reference_bin_cdf": float(ref_cdf[mapped_bin]),
        }
    return result


def main() -> None:
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("Core source changed; re-audit equation mirror")
    source_base = np.load(RD0 / "synthetic_source.npy", allow_pickle=False).astype(np.float64)
    reference_base = np.load(RD0 / "synthetic_reference.npy", allow_pickle=False).astype(np.float64)
    x0, y0, x1, y1 = BOX
    rows = []

    for level in LEVELS:
        source, reference = source_base.copy(), reference_base.copy()
        source_rgb = np.clip(level * SCALE, 0.0, 0.985)
        source[y0:y1, x0:x1] = source_rgb
        reference[y0:y1, x0:x1] = level
        source_encoded, reference_encoded = encode_srgb(source), encode_srgb(reference)
        output = apply_histogram(source_encoded, reference_encoded, 1.0)
        display = np.clip(output, 0, 1)
        output_linear = np.where(display <= 0.04045, display / 12.92,
                                 ((display + 0.055) / 1.055) ** 2.4)
        patch_target = np.full((y1-y0, x1-x0, 3), level)
        patch_output = output_linear[y0:y1, x0:x1]
        delta_e = np.linalg.norm(linear_to_lab(patch_output) - linear_to_lab(patch_target), axis=-1)
        row = {
            "target_linear": float(level),
            "target_srgb": float(encode_srgb(np.array(level))),
            "source_patch_encoded": encode_srgb(source_rgb).tolist(),
            "source_patch_bins": (encode_srgb(source_rgb) * 255).astype(np.int32).tolist(),
            "reference_patch_encoded": reference_encoded[y0, x0].tolist(),
            "reference_patch_bins": (reference_encoded[y0, x0] * 255).astype(np.int32).tolist(),
            "mapping": histogram_mapping_diagnostic(source_encoded, reference_encoded,
                                                    encode_srgb(source_rgb)),
            "output_linear_mean": patch_output.mean(axis=(0, 1)).tolist(),
            "mean_deltaE76": float(delta_e.mean()),
            "p95_deltaE76": float(np.percentile(delta_e, 95)),
            "mean_linear_luminance_error": float(
                (patch_output @ np.array([0.2126, 0.7152, 0.0722])).mean() - level
            ),
        }
        rows.append(row)

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "response.json").write_text(json.dumps(rows, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    with (OUT / "response.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("target_linear", "target_srgb",
            "source_patch_encoded", "source_patch_bins", "reference_patch_encoded",
            "reference_patch_bins", "output_linear_mean", "mean_deltaE76",
            "p95_deltaE76", "mean_linear_luminance_error"))
        writer.writeheader()
        for row in rows:
            writer.writerow({k: json.dumps(row[k]) if isinstance(row[k], (list, dict)) else row[k]
                             for k in writer.fieldnames})

    delta_e = np.array([row["mean_deltaE76"] for row in rows])
    changes = np.abs(np.diff(delta_e))
    largest = np.argsort(changes)[-8:][::-1]
    summary = {
        "rd": "rd3-histogram-neutral-level-sweep",
        "status": "synthetic source-mirror diagnostic only; no quality gate",
        "target_levels_linear": [float(LEVELS[0]), float(LEVELS[-1]), len(LEVELS)],
        "patch_xyxy": list(BOX),
        "metric": "mean/p95 CIE Lab D65 2-degree DeltaE76 after sRGB decode; mean linear Rec.709 luminance error",
        "largest_adjacent_deltaE_changes": [{
            "from": float(LEVELS[i]), "to": float(LEVELS[i+1]),
            "deltaE_before": float(delta_e[i]), "deltaE_after": float(delta_e[i+1]),
            "absolute_change": float(changes[i]),
            "mapping_before": rows[i]["mapping"], "mapping_after": rows[i+1]["mapping"],
        } for i in largest],
        "deltaE_min": float(delta_e.min()), "deltaE_max": float(delta_e.max()),
        "monotonic_nonincreasing": bool(np.all(np.diff(delta_e) <= 1e-12)),
        "source_hashes": {"core": sha(CORE_SOURCE), "runner": sha(ROOT / "run_rd1_source_mirror.py"),
            "sweep_script": sha(Path(__file__)), "rd0_source": sha(RD0 / "synthetic_source.npy"),
            "rd0_reference": sha(RD0 / "synthetic_reference.npy")},
        "limitations": ["equation mirror only; no C++ parity", "single synthetic chart distribution",
            "no natural-white ROI or visual gate", "patch hard-edge and global histogram coupling"],
    }
    summary["files"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in OUT.iterdir() if p.is_file() and p.name != "manifest.json"}
    (OUT / "manifest.json").write_text(json.dumps(summary, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
