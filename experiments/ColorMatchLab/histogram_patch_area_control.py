"""Compare neutral ROI area with fixed versus changing global histograms."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from histogram_response_sweep import histogram_mapping_diagnostic
from run_rd1_source_mirror import apply_histogram, sha, CORE_SOURCE, EXPECTED_CORE_SHA256
from white_axis_study import encode_srgb, linear_to_lab

RD0 = ROOT / "captures" / "rd0"
OUT = ROOT / "captures" / "rd4_histogram_area_control"
SIZES = (32, 48, 64, 90, 128)
LEVELS = (0.891, 0.892)
WIDTH, HEIGHT = 512, 320
SCALE = np.array([1.055, 0.995, 0.87])


def sha256_array(array: np.ndarray) -> str:
    return hashlib.sha256(np.ascontiguousarray(array).tobytes()).hexdigest()


def evaluate(source: np.ndarray, reference: np.ndarray, level: float,
             box: tuple[int, int, int, int], arm: str) -> dict:
    x0, y0, x1, y1 = box
    source_encoded, reference_encoded = encode_srgb(source), encode_srgb(reference)
    output = apply_histogram(source_encoded, reference_encoded, 1.0)
    display = np.clip(output, 0, 1)
    linear = np.where(display <= 0.04045, display / 12.92,
                      ((display + 0.055) / 1.055) ** 2.4)
    roi = linear[y0:y1, x0:x1]
    target = np.full_like(roi, level)
    de = np.linalg.norm(linear_to_lab(roi) - linear_to_lab(target), axis=-1)
    src_roi = encode_srgb(source[y0:y1, x0:x1].mean(axis=(0, 1)))
    mapping = histogram_mapping_diagnostic(source_encoded, reference_encoded, src_roi)
    return {
        "arm": arm, "target_linear": level, "box_xyxy": list(box),
        "patch_area": (x1-x0)*(y1-y0), "patch_area_ratio": ((x1-x0)*(y1-y0))/(WIDTH*HEIGHT),
        "mean_deltaE76": float(de.mean()), "p95_deltaE76": float(np.percentile(de, 95)),
        "output_linear_mean": roi.mean(axis=(0, 1)).tolist(), "blue_mapping": mapping["B"],
        "source_histogram_hashes": [sha256_array(np.histogram(
            source_encoded[..., c], bins=256, range=(0, 1))[0]) for c in range(3)],
        "reference_histogram_hashes": [sha256_array(np.histogram(
            reference_encoded[..., c], bins=256, range=(0, 1))[0]) for c in range(3)],
    }


def main() -> None:
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("Core source changed; re-audit equation mirror")
    base_source = np.load(RD0 / "synthetic_source.npy", allow_pickle=False).astype(np.float64)
    base_reference = np.load(RD0 / "synthetic_reference.npy", allow_pickle=False).astype(np.float64)
    if base_source.shape != (HEIGHT, WIDTH, 3) or base_reference.shape != base_source.shape:
        raise RuntimeError("Unexpected fixed chart shape")

    rows = []
    rng = np.random.default_rng(20261116)
    for level in LEVELS:
        source_probe = np.clip(level * SCALE, 0, 0.985)
        target_probe = np.full(3, level)
        max_count = SIZES[-1] ** 2

        # Build a fixed multiset: reserve exactly max_count paired pixels for
        # the probe, then spatially rearrange the same multiset for each ROI.
        base_src_flat = base_source.reshape(-1, 3).copy()
        base_ref_flat = base_reference.reshape(-1, 3).copy()
        base_src_flat[:max_count] = source_probe
        base_ref_flat[:max_count] = target_probe
        background_indices = np.arange(max_count, WIDTH * HEIGHT)

        for size in SIZES:
            area = size * size
            box = (32, 32, 32 + size, 32 + size)

            # Fixed-histogram arm: exactly the same source/reference pixel
            # multisets are spatially reassigned; only contiguous ROI area moves.
            fixed_src = np.empty_like(base_src_flat)
            fixed_ref = np.empty_like(base_ref_flat)
            fixed_bg_order = rng.permutation(background_indices)
            x0, y0, x1, y1 = box
            patch_positions = np.array([y * WIDTH + x for y in range(y0, y1)
                                        for x in range(x0, x1)])
            patch_mask = np.zeros(WIDTH * HEIGHT, dtype=bool)
            patch_mask[patch_positions] = True
            other_positions = np.flatnonzero(~patch_mask)
            fixed_src[patch_positions] = source_probe
            fixed_ref[patch_positions] = target_probe
            other_probe_count = max_count - area
            rest_probe_positions = rng.choice(other_positions, size=other_probe_count, replace=False)
            fixed_src[rest_probe_positions] = source_probe
            fixed_ref[rest_probe_positions] = target_probe
            bg_positions = np.setdiff1d(other_positions, rest_probe_positions, assume_unique=True)
            fixed_src[bg_positions] = base_src_flat[fixed_bg_order]
            fixed_ref[bg_positions] = base_ref_flat[fixed_bg_order]
            fixed_source = fixed_src.reshape(HEIGHT, WIDTH, 3)
            fixed_reference = fixed_ref.reshape(HEIGHT, WIDTH, 3)
            fixed_row = evaluate(fixed_source, fixed_reference, level, box, "fixed_global_histogram")
            fixed_row["source_float_multiset_hash"] = sha256_array(np.sort(fixed_src, axis=0))
            fixed_row["reference_float_multiset_hash"] = sha256_array(np.sort(fixed_ref, axis=0))
            rows.append(fixed_row)

            # Variable-histogram arm: keep the background field fixed and grow
            # the patch by replacing more background pixels with probe values.
            variable_source, variable_reference = base_source.copy(), base_reference.copy()
            x0, y0, x1, y1 = box
            variable_source[y0:y1, x0:x1] = source_probe
            variable_reference[y0:y1, x0:x1] = target_probe
            rows.append(evaluate(variable_source, variable_reference, level, box,
                                 "patch_replaces_background"))

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "results.json").write_text(json.dumps(rows, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    summary = {
        "rd": "rd4-histogram-patch-area-control",
        "status": "synthetic equation-mirror diagnostic only; no quality gate",
        "levels_linear": list(LEVELS), "square_patch_sizes": list(SIZES),
        "fixed_histogram_arm": "same paired pixel multisets rearranged spatially for each ROI size",
        "variable_histogram_arm": "patch grows by replacing the unchanged base chart",
        "metric": "mean/p95 CIE Lab D65 2-degree DeltaE76 after sRGB decode; intensity 1.0",
        "source_hashes": {"core": sha(CORE_SOURCE), "runner": sha(ROOT / "run_rd1_source_mirror.py"),
            "sweep_script": sha(ROOT / "histogram_response_sweep.py"),
            "control_script": sha(Path(__file__)), "rd0_source": sha(RD0 / "synthetic_source.npy"),
            "rd0_reference": sha(RD0 / "synthetic_reference.npy")},
        "limitations": ["no compiled C++ differential", "single base chart and cast", "no natural white or skin ground truth",
            "spatial rearrangement in fixed histogram arm", "hard patch boundaries", "no human visual assessment"],
    }
    summary["files"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in OUT.iterdir() if p.is_file() and p.name != "manifest.json"}
    (OUT / "manifest.json").write_text(json.dumps(summary, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
