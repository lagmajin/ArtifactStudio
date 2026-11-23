from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd16_float_range_sensitivity"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
sys.path.insert(0, str(ROOT))
import run_rd1_source_mirror as mirror  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture() -> tuple[np.ndarray, dict[str, np.ndarray], list[float]]:
    levels = [-0.25, -0.05, 0.0, 0.01, 0.04, 0.18, 0.5, 0.9, 1.0, 1.05, 1.2, 2.0, 4.0]
    colors = []
    labels = []
    for level in levels:
        # Three channel permutations expose channel-specific behavior while
        # preserving equal sample counts for every fixed level.
        colors.extend(([level, level*0.7, level*0.4],
                       [level*0.4, level, level*0.7],
                       [level*0.7, level*0.4, level]))
        labels.extend([level]*3)
    base = np.asarray(colors, dtype=np.float64)
    labels_arr = np.asarray(labels)
    masks = {
        "underflow": labels_arr < 0,
        "sdr": (labels_arr >= 0) & (labels_arr <= 1),
        "highlight": labels_arr > 1,
        "all": np.ones(labels_arr.shape, dtype=bool),
    }
    return base, masks, levels


def metric(reference: np.ndarray, output: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
    values = output[mask]
    delta = np.abs(values-reference[mask])
    return {
        "pixels": int(mask.sum()),
        "rgb_mae": float(delta.mean()),
        "rgb_p95": float(np.percentile(delta, 95)),
        "rgb_max": float(delta.max()),
        "negative_channel_ratio": float(np.mean(values < 0)),
        "in_0_1_channel_ratio": float(np.mean((values >= 0) & (values <= 1))),
        "above_1_channel_ratio": float(np.mean(values > 1)),
        "non_finite_values": int(values.size-np.count_nonzero(np.isfinite(values))),
    }


def main() -> None:
    core_hash = sha(CORE_SOURCE)
    expected = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
    if core_hash != expected:
        raise RuntimeError(f"Core source changed; re-audit before updating pin: {core_hash}")
    CAPTURE.mkdir(parents=True, exist_ok=True)
    target, masks, levels = fixture()
    scale = np.array([0.96, 1.025, 0.985])
    offset = np.array([0.015, -0.01, 0.008])
    source = target*scale + offset
    methods = {"Reinhard": mirror.apply_reinhard, "MeanStddev": mirror.apply_mean_std,
               "Histogram": mirror.apply_histogram}
    results = []
    for name, fn in methods.items():
        for intensity in (0.25, 0.5, 1.0):
            output = fn(source, target, intensity)
            np.save(CAPTURE/f"{name}_i{intensity:.2f}.npy", output.astype(np.float32), allow_pickle=False)
            results.append({"method": name, "intensity": intensity,
                            "regions": {region: metric(target, output, mask) for region,mask in masks.items()}})
    files = {p.name: sha(p) for p in sorted(CAPTURE.iterdir()) if p.is_file() and p.name != "manifest.json"}
    manifest = {
        "rd": "rd16-float-range-sensitivity",
        "status": "completed synthetic equation-mirror range probe; not Core execution or quality acceptance",
        "fixture": {"dimensions": [len(target), 1], "levels": levels, "channel_permutations_per_level": 3,
                    "generation": "fixed finite float triplets; source is target*[0.96,1.025,0.985]+[0.015,-0.01,0.008], without clipping",
                    "alpha": "not represented"},
        "conditions": {"range_groups": {"underflow": "level < 0", "sdr": "0 <= level <= 1", "highlight": "level > 1"},
                       "methods": list(methods), "intensities": [0.25,0.5,1.0],
                       "metrics": "unclipped numeric RGB errors and output channel range ratios; Lab not applied", "gate": "none; descriptive only"},
        "source_contract": {"source_sha256": core_hash,
                            "inspected_behavior": {"Reinhard": "clamps RGB input to [0,1] before Lab and clamps reconstructed linear RGB to [0,1] before gamma transfer",
                                                   "MeanStddev": "computes per-channel statistics on raw float values and does not clamp mapped output",
                                                   "Histogram": "converts value*255 to int, clamps bin to [0,255], maps bins to [0,1]"},
                            "mirror": "independent Python equations imported from run_rd1_source_mirror.py; no C++ differential"},
        "unverified": ["compiled Core differential", "real linear HDR image behavior and display transform", "NaN/Inf and extreme values", "descriptor use", "alpha/hidden RGB", "visual review", "GPU/product route", "performance"],
        "results": results, "files": files,
        "runner_sha256": sha(Path(__file__).resolve()),
        "equation_mirror_script_sha256": sha(Path(__file__).with_name("run_rd1_source_mirror.py")),
    }
    (CAPTURE/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n", encoding="utf-8", newline="\n")
    compact = [{"method":r["method"],"intensity":r["intensity"],
                "regions":{k:{"mae":v["rgb_mae"],"negative":v["negative_channel_ratio"],"above1":v["above_1_channel_ratio"]}
                           for k,v in r["regions"].items()}} for r in results]
    print(json.dumps({"capture":str(CAPTURE),"results":compact},indent=2))


if __name__ == "__main__":
    main()
