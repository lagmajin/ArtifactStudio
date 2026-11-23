"""Localization study (rd2): does seam removal avoid detail, or only avoid contiguity?

rd1 showed the preregistered P5 (max contiguous removal run) failed for every lane and
could not distinguish intended background compression from destructive squeezing. This
study measures removal localization against content energy instead: 16x16 blocks are
ranked by source gradient energy, and compression / energy retention are reported for the
top and bottom quartiles.

Gates P7-P9 are pre-registered in EVALUATION_PLAN.md (rd2). The study also runs the
synthetic `wires` fixture, a known failure case, to check that the metrics can actually
detect it.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

import photo_study as ps
import retarget_study as rs

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd2"
BLOCK = 16
LANES = ["crop_center", "seam_backward", "seam_forward"]
SYNTHETIC = ["wires", "portrait", "facade"]


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_lane(work: np.ndarray, lane: str, target_w: int):
    rows, width = work.shape[:2]
    if lane == "crop_center":
        left = (width - target_w) // 2
        kept = np.zeros(width, bool)
        kept[left:left + target_w] = True
        out = work[:, kept]
        source_x = np.broadcast_to(np.nonzero(kept)[0], (rows, target_w)).copy()
        return out, source_x
    if lane == "seam_backward":
        out, source_x, _ = ps.carve(work, target_w, "backward")
        return out, source_x
    out, source_x, _ = ps.carve(work, target_w, "forward")
    return out, source_x


def block_metrics(work: np.ndarray, alive: np.ndarray):
    """Compression and energy retention per 16x16 block, split by source energy quartile."""
    magnitude = ps.structure_masks(work)[0]
    rows, width = work.shape[:2]
    n_rows = rows // BLOCK
    n_cols = width // BLOCK

    energy = np.zeros((n_rows, n_cols))
    compression = np.zeros((n_rows, n_cols))
    retained = np.zeros((n_rows, n_cols))
    for by in range(n_rows):
        for bx in range(n_cols):
            ys = slice(by * BLOCK, (by + 1) * BLOCK)
            xs = slice(bx * BLOCK, (bx + 1) * BLOCK)
            block = magnitude[ys, xs]
            live = alive[ys, xs]
            energy[by, bx] = float(block.sum())
            compression[by, bx] = 1.0 - float(live.mean())
            retained[by, bx] = float(block[live].sum())

    energy_flat = energy.reshape(-1)
    compression_flat = compression.reshape(-1)
    retained_flat = retained.reshape(-1)
    q1_cut = np.percentile(energy_flat, 25.0)
    q4_cut = np.percentile(energy_flat, 75.0)
    low = energy_flat <= q1_cut
    high = energy_flat >= q4_cut

    def safe_mean(values: np.ndarray) -> float:
        return float(values.mean()) if values.size else float("nan")

    q1_compression = safe_mean(compression_flat[low])
    q4_compression = safe_mean(compression_flat[high])
    q4_energy_total = float(energy_flat[high].sum())
    return {
        "blocks": int(energy_flat.size),
        "q4_compression_mean": round(q4_compression, 4),
        "q4_compression_p95": round(float(np.percentile(compression_flat[high], 95.0)), 4),
        "q1_compression_mean": round(q1_compression, 4),
        "selectivity": (
            round(q1_compression / q4_compression, 3)
            if q4_compression > 1e-9 else None
        ),
        "q4_energy_retention": (
            round(float(retained_flat[high].sum()) / q4_energy_total, 4)
            if q4_energy_total > 0 else None
        ),
        "q4_removed_energy_share": (
            round(1.0 - float(retained_flat[high].sum()) / q4_energy_total, 4)
            if q4_energy_total > 0 else None
        ),
    }


def evaluate(name: str, work: np.ndarray, kind: str) -> dict:
    rows, width = work.shape[:2]
    target_w = rows
    result = {"kind": kind, "source_size": [int(width), int(rows)], "lanes": {}}
    for lane in LANES:
        out, source_x = run_lane(work, lane, target_w)
        alive = ps.alive_map(source_x, width)
        entry = block_metrics(work, alive)
        entry["output_size"] = [int(out.shape[1]), int(out.shape[0])]
        result["lanes"][lane] = entry
    png = OUT / f"{name}_localization.png"
    rs.write_png(png, np.concatenate([run_lane(work, lane, target_w)[0] for lane in LANES], axis=1))
    result["contact_sheet_sha256"] = sha256_file(png)
    return result


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "rd": "rd2",
        "study": "removal localization vs content energy",
        "block_size": BLOCK,
        "quartiles": "per-image source gradient energy: Q1 = bottom 25%, Q4 = top 25%",
        "generator_sha256": sha256_file(Path(__file__)),
        "gates": {
            "P7_q4_compression_p95_max": 0.25,
            "P8_q4_energy_retention_min": 0.90,
            "P9_selectivity_min": 3.0,
        },
        "fixtures": {},
        "notes": [
            "P5 (max contiguous removal run) is reported in rd0/rd1 as history and is not used for pass/fail here.",
            "stretch is excluded: it removes no pixels, so localization does not apply.",
            "The synthetic wires fixture is included as a known-failure check for the new metrics.",
            "No visual review was performed.",
        ],
    }

    fixtures = rs.build_fixtures()
    for name in SYNTHETIC:
        work = fixtures[name]["image"]
        report["fixtures"][f"synthetic_{name}"] = evaluate(f"synthetic_{name}", work, "synthetic")

    for plate in ps.PLATES:
        work, info = ps.load_plate(plate)
        entry = evaluate(plate["name"], work, "photo")
        entry["source_path"] = plate["path"]
        entry["source_sha256"] = plate["sha256"]
        entry["license"] = plate["license"]
        entry["provenance"] = plate["provenance"]
        report["fixtures"][plate["name"]] = entry

    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")

    for name, entry in report["fixtures"].items():
        print(f"== {name} ({entry['kind']})")
        for lane, m in entry["lanes"].items():
            print("  %-14s q4_comp=%.3f p95=%.3f q1_comp=%.3f sel=%s q4_ret=%s"
                  % (lane, m["q4_compression_mean"], m["q4_compression_p95"],
                     m["q1_compression_mean"], m["selectivity"], m["q4_energy_retention"]))


if __name__ == "__main__":
    main()
