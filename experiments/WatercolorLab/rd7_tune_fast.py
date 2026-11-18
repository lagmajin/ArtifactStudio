"""RD7 parameter sweep for the fast shared-weight filter.

Tunes the v5 gate parameters against v3 joint-bilateral output on existing
opaque fixtures. This is model design tuning on descriptive metrics; no
quality gate is set and no visual ranking is produced.

Objective (lexicographic, recorded explicitly): minimize max RGB delta vs v3,
then mean absolute RGB delta vs v3. Runtime is recorded but not used to pick
the winner, because all candidates in the sweep share the same O(h*w) cost.
"""

from __future__ import annotations

import argparse
import itertools
import json
import platform
import time
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import save_output, sha256
from rd1_candidates import ALL_CANDIDATES, CANDIDATE_SEED_BASE
from watercolor_reference_v3 import apply_candidate_v3
from watercolor_reference_v5 import apply_candidate_v5

TUNE_FIXTURES = ("color_ramp_small", "thin_lines_small", "flat_highlight_small")
TUNE_CANDIDATES = ("only_bleed", "wc_a")
PARAM_GRID = {
    "k": (3, 5, 7, 11),
    "sigma_space": (1.0, 2.0, 3.0, 5.0),
    "sigma_color": (0.08, 0.15, 0.25, 0.40, 0.60),
}


def run(output_rd0: Path, output_rd7: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd7.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd7 / "tuning_candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd7 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    cases = []
    for key in TUNE_FIXTURES:
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][key]["npy"])
        for cand in TUNE_CANDIDATES:
            spec = ALL_CANDIDATES[cand]
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand)
            cases.append({
                "fixture": key,
                "candidate": cand,
                "rgba": rgba,
                "spec": spec,
                "seed": seed,
                "v3": apply_candidate_v3(rgba, spec, seed, descriptor="straight")[..., :3],
            })

    results = []
    keys = list(PARAM_GRID)
    for values in itertools.product(*(PARAM_GRID[k] for k in keys)):
        params = dict(zip(keys, values))
        max_delta = 0.0
        mae_sum = 0.0
        mae_weight = 0
        elapsed = 0.0
        for case in cases:
            t0 = time.perf_counter()
            out = apply_candidate_v5(case["rgba"], case["spec"], case["seed"],
                                     descriptor="straight", fast_params=params)[..., :3]
            elapsed += time.perf_counter() - t0
            d = np.abs(out - case["v3"])
            max_delta = max(max_delta, float(d.max()))
            mae_sum += float(d.mean())
            mae_weight += 1
        results.append({
            "params": params,
            "max_rgb_delta_vs_v3": max_delta,
            "mean_mae_vs_v3": mae_sum / mae_weight,
            "seconds_sum": float(elapsed),
        })

    results.sort(key=lambda r: (r["max_rgb_delta_vs_v3"], r["mean_mae_vs_v3"]))
    best = results[0]

    # Verify the tuned params keep descriptor consistency on the alpha fixture.
    alpha_entry = fm["fixtures"]["alpha_boundary_small"]
    straight = np.load(output_rd0 / "fixtures" / alpha_entry["npy"])
    premul = np.load(output_rd0 / "fixtures" / alpha_entry["premultiplied"]["npy"])
    alpha = straight[..., 3:4]
    valid = alpha[..., 0] > 0.01
    descriptor_check = {}
    for cand in TUNE_CANDIDATES:
        spec = ALL_CANDIDATES[cand]
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand)
        s = apply_candidate_v5(straight, spec, seed, descriptor="straight", fast_params=best["params"])
        p = apply_candidate_v5(premul, spec, seed, descriptor="premultiplied", fast_params=best["params"])
        recovered = p[..., :3] / np.maximum(p[..., 3:4], 1e-6)
        descriptor_check[cand] = float(np.max(np.abs(recovered - s[..., :3])[valid]))

    outputs = {}
    for case in cases:
        stem = f"tuned_{case['fixture']}_{case['candidate']}"
        out = apply_candidate_v5(case["rgba"], case["spec"], case["seed"],
                                 descriptor="straight", fast_params=best["params"])
        outputs[stem] = save_output(cand_dir / stem, out)

    lines = [
        "# RD7 tuning sweep (blank conclusion)",
        "",
        "Objective: minimize max RGB delta vs v3, then mean absolute delta. Runtime recorded, not scored.",
        "No quality gate is set; visual ranking is not produced.",
        "",
        "| rank | k | sigma_space | sigma_color | max delta vs v3 | mean MAE vs v3 | seconds (sum over cases) |",
        "|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for i, r in enumerate(results[:20], 1):
        p = r["params"]
        lines.append(
            f"| {i} | {p['k']} | {p['sigma_space']} | {p['sigma_color']} | "
            f"{r['max_rgb_delta_vs_v3']:.4g} | {r['mean_mae_vs_v3']:.6f} | {r['seconds_sum']:.4f} |"
        )
    lines.extend([
        "",
        "## Tuned descriptor check (alpha > 0.01)",
        "",
        "| candidate | max recovered-straight delta |",
        "|---|---:|",
    ] + [f"| {c} | {v:.3g} |" for c, v in descriptor_check.items()])
    table_path = review_dir / "RD7_TUNING_TABLE.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    data = {
        "lab": "WatercolorLab",
        "rd": "rd7",
        "stage": "fast_filter_tuning",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "objective": "lexicographic: max RGB delta vs v3, then mean absolute delta; runtime recorded only",
        "tune_fixtures": TUNE_FIXTURES,
        "tune_candidates": TUNE_CANDIDATES,
        "param_grid": PARAM_GRID,
        "combinations": len(results),
        "best": best,
        "descriptor_check": descriptor_check,
        "outputs": outputs,
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
    }
    data_path = output_rd7 / "tuning_rd7.json"
    data_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd7",
        "stage": "fast_filter_tuning",
        "date": "2026-11-17",
        "evidence": {
            "data": {"file": data_path.name, "sha256": sha256(data_path.read_bytes())},
            "table": {"file": f"review/{table_path.name}", "sha256": sha256(table_path.read_bytes())},
        },
        "gates": "unset",
    }
    manifest_path = output_rd7 / "manifest_rd7_tuning.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"best params: {best['params']} max_delta={best['max_rgb_delta_vs_v3']:.4g} "
          f"mae={best['mean_mae_vs_v3']:.6f}")
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd7")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    run(rd0, Path(args.output))


if __name__ == "__main__":
    main()
