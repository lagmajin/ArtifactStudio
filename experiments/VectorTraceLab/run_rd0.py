"""R0 / rd0 runner: fixture generation, identity check, baseline measurement, evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import statistics
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from generate_fixtures import (SEED, SS, build_all, render_fixture,
                               review_png, sha256, write_fixtures)
from vector_reference import alpha_threshold_trace, evaluate, posterize_components

METHODS = {
    "alpha_threshold_trace": lambda fx, src: alpha_threshold_trace(fx, src),
    "posterize_components": lambda fx, src: posterize_components(fx, src),
}

# Finalized from the rd0 baseline before any rd1 candidate comparison.
# Rationale is recorded in EVALUATION_PLAN.md section 5.
GATES = {
    "fixture_identity_max_abs_diff": {"op": "<=", "value": 0.0},
    "region_count_abs_error": {"op": "<=", "value": 1},
    "iou_mean": {"op": ">=", "value": 0.90},
    "iou_min": {"op": ">=", "value": 0.70},
    "boundary_chamfer_p95_px": {"op": "<=", "value": 3.0},
    "holes_recovered_fraction": {"op": ">=", "value": 1.0},
    "thin_recall": {"op": ">=", "value": 0.80},
    "color_de76_mean": {"op": "<=", "value": 3.0},
    "alpha_edge_error_shift1_p95_codes": {"op": "<=", "value": 60.0},
    "resolution_normalized_chamfer_change": {"op": "<=", "value": 0.25},
}


def _load_npz(path):
    with np.load(path) as data:
        return {k: data[k] for k in data.files}


def _review_grid(out_dir: Path, results: dict):
    """Create a compact contact sheet: one row per fixture, source + methods."""
    rows = []
    for fx_name in results:
        images = [("source", Image.fromarray(results[fx_name]["source_review"]))]
        for method in METHODS:
            p = out_dir / f"{fx_name}__{method}__review.png"
            if p.exists():
                images.append((method, Image.open(p).convert("RGB")))
        scaled = []
        for label, im in images:
            if im.width > 512:
                im = im.resize((512, int(im.height * 512 / im.width)))
            scaled.append((label, im))
        h = max(im.height for _, im in scaled) + 18
        w = sum(im.width for _, im in scaled)
        row = Image.new("RGB", (w, h), (32, 32, 32))
        draw = ImageDraw.Draw(row)
        x = 0
        for label, im in scaled:
            row.paste(im, (x, 18))
            draw.text((x + 4, 3), label, fill=(230, 230, 230))
            x += im.width
        rows.append(row)
    if not rows:
        return
    W = max(r.width for r in rows)
    H = sum(r.height for r in rows)
    sheet = Image.new("RGB", (W, H), (16, 16, 16))
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.height
    sheet.save(out_dir / "contact.png")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="captures/rd0")
    ap.add_argument("--fixtures", default=None,
                    help="fixture dir (default: <out>/fixtures); generated if missing")
    args = ap.parse_args()

    out = Path(args.out)
    fx_dir = Path(args.fixtures) if args.fixtures else out / "fixtures"
    out.mkdir(parents=True, exist_ok=True)
    fixture_meta = write_fixtures(fx_dir) if not (fx_dir / "logo_badge__source.npz").exists() else None

    fixtures = build_all()
    results = {}
    all_metrics = {}
    identity = {}

    for fx in fixtures:
        src = _load_npz(fx_dir / f"{fx.name}__source.npz")["rgba"]
        gt = _load_npz(fx_dir / f"{fx.name}__gt.npz")
        results[fx.name] = {"source_review": review_png(src)}

        # identity check: deterministic regeneration
        regen, _, _, _ = render_fixture(fx)
        identity[fx.name] = float(np.abs(regen - src).max())

        all_metrics[fx.name] = {}
        for method_name, runner in METHODS.items():
            times = []
            traced = None
            for _ in range(3):
                t0 = time.perf_counter()
                traced = runner(fx, src)
                times.append(time.perf_counter() - t0)
            m = evaluate(fx, src, gt, traced, method_name)
            m["wall_time_ms_median"] = statistics.median(times) * 1000.0
            all_metrics[fx.name][method_name] = m

            out_rgba, _ = __import__("vector_reference").traced_to_arrays(
                traced, fx.w, fx.h)
            Image.fromarray(review_png(out_rgba)).save(
                out / f"{fx.name}__{method_name}__review.png")
            print(f"{fx.name:22s} {method_name:22s} "
                  f"IoU={m['iou_mean']} chamfer={m['boundary_chamfer_p95']} "
                  f"holes={m['holes_recovered']}/{m['holes_gt']}")

    _review_grid(out, results)

    metrics = {
        "schema_version": 1,
        "lab": "VectorTraceLab",
        "rd": "rd0",
        "seed": SEED,
        "supersample": SS,
        "identity_max_abs_diff": identity,
        "fixtures": all_metrics,
    }
    (out / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")

    gates = {
        "status": "final (fixed from rd0 baseline before rd1)",
        "note": ("Gate values were finalized from the rd0 baseline recorded in "
                 "EVALUATION_PLAN.md section 5. Do not loosen after seeing candidates."),
        "gates": GATES,
        "rd0_baseline": {
            "alpha_threshold_trace": {
                k: [all_metrics[n]["alpha_threshold_trace"].get(k)
                    for n in all_metrics]
                for k in ("region_count", "iou_mean", "iou_min",
                          "boundary_chamfer_p95", "holes_recovered",
                          "thin_recall", "color_de76_mean",
                          "alpha_edge_error_shift1_p95_codes",
                          "wall_time_ms_median")
            },
            "posterize_components": {
                k: [all_metrics[n]["posterize_components"].get(k)
                    for n in all_metrics]
                for k in ("region_count", "iou_mean", "iou_min",
                          "boundary_chamfer_p95", "holes_recovered",
                          "thin_recall", "color_de76_mean",
                          "alpha_edge_error_shift1_p95_codes",
                          "wall_time_ms_median")
            },
        },
    }
    (out / "gates.json").write_text(json.dumps(gates, indent=2), encoding="utf-8")

    manifest = {
        "schema_version": 1,
        "lab": "VectorTraceLab",
        "rd": "rd0",
        "created": datetime.now(timezone.utc).isoformat(),
        "status": "R0 baseline measured; provisional gates; no human visual review",
        "operator": "AI agent (no independent human visual review performed)",
        "environment": {
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "numpy": np.__version__,
            "opencv": __import__("cv2").__version__,
        },
        "seed": SEED,
        "supersample": SS,
        "fixture_generation": fixture_meta or {"reused_existing": str(fx_dir)},
        "methods": list(METHODS),
        "outputs": {
            "metrics": "metrics.json",
            "gates": "gates.json",
            "contact": "contact.png",
        },
        "hashes": {
            "metrics.json": sha256(out / "metrics.json"),
            "gates.json": sha256(out / "gates.json"),
            "contact.png": sha256(out / "contact.png"),
        },
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    digest = hashlib.sha256((out / "manifest.json").read_bytes()).hexdigest()
    (out / "manifest.sha256").write_text(digest + "\n", encoding="utf-8")
    print(f"R0 evidence written to {out}")


if __name__ == "__main__":
    main()
