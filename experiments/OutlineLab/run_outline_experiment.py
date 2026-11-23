#!/usr/bin/env python3
"""Run Phase 1 outline reference experiments.

The experiment is deliberately offline and deterministic. It loads the generated
.npy fixtures, runs the reference model, and writes review images plus a JSON
manifest. It does not invoke ArtifactStudio or modify product code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, replace
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image

import outline_metrics as metrics
import outline_reference as ref


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "generated"


def load_fixture(index: dict[str, Any], name: str, alpha_mode: str) -> dict[str, Any]:
    matches = [
        item for item in index["fixtures"]
        if item["name"] == name and item["alpha_mode"] == alpha_mode
    ]
    if not matches:
        raise KeyError(f"fixture not found: {name} / {alpha_mode}")
    if len(matches) > 1:
        raise ValueError(f"duplicate fixture entries: {name} / {alpha_mode}")
    return matches[0]


def load_npy(path: Path) -> np.ndarray:
    return np.load(path).astype(np.float32)


def sha256_array(a: np.ndarray) -> str:
    return hashlib.sha256(np.ascontiguousarray(a).tobytes()).hexdigest()


def to_png(path: Path, rgb: np.ndarray) -> None:
    rgb = np.clip(rgb, 0.0, 1.0)
    srgb = np.where(rgb <= 0.0031308, rgb * 12.92, 1.055 * np.power(rgb, 1.0 / 2.4) - 0.055)
    data = np.clip(srgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(data, mode="RGB").save(path)


def gray_png(path: Path, field: np.ndarray) -> None:
    to_png(path, np.repeat(np.clip(field, 0.0, 1.0)[..., None], 3, axis=2))


def case_metrics(line: np.ndarray, truth: np.ndarray, width_px: float) -> dict[str, Any]:
    widths = metrics.line_widths(line, truth)
    position = metrics.position_error(line, truth, width_px=width_px)
    mask = line >= 0.5
    return {
        "line_sha256": sha256_array(line),
        "line_coverage": float(np.mean(mask)),
        "line_mean": float(np.mean(line)),
        "line_true_p50": float(np.percentile(line[mask], 50) if np.any(mask) else 0.0),
        "line_true_p90": float(np.percentile(line[mask], 90) if np.any(mask) else 0.0),
        "width_50": widths["width_50"],
        "width_90": widths["width_90"],
        "line_peak": widths["peak"],
        "position_error": position,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["alpha-depth"], default="alpha-depth")
    parser.add_argument("--width", type=float, default=2.0)
    parser.add_argument("--softness", type=float, default=0.75)
    parser.add_argument("--alpha-detail-weight", type=float, default=0.0)
    parser.add_argument("--output", default=str(ROOT / "captures" / "outline-rd1"))
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    index_path = ASSETS / "index.json"
    if not index_path.exists():
        raise SystemExit(
            "fixtures not found; run generate_fixtures.py first "
            f"(expected {index_path})"
        )
    index = json.loads(index_path.read_text(encoding="utf-8"))
    settings = ref.OutlineSettings(
        width_px=args.width,
        softness=args.softness,
        alpha_detail_weight=args.alpha_detail_weight,
        include_depth=True,
    )
    # Always evaluate the detail channel as a named variant so a report can be
    # compared without re-running the script with a different default.
    detail_settings = replace(settings, alpha_detail_weight=1.0)

    report: dict[str, Any] = {
        "lab": "OutlineLab",
        "created": datetime.now(timezone.utc).isoformat(),
        "mode": args.mode,
        "settings": asdict(settings),
        "fixtures": [],
        "limitations": [
            "Python reference model only; not an ArtifactStudio or ArtifactCore implementation.",
            "Depth is synthetic and not metric; no normal, color, temporal, GPU, or product path is evaluated.",
            "PNG review images are display-encoded; measurements use linear float arrays.",
            "Widths are sampled along boundary normals at 0.25px steps and quantized by that step.",
        ],
        "metric_definitions": {
            "width_50": "extent along the truth-boundary normal where the line stays >=50% of its local peak; p10/p50/p90 over sampled points",
            "width_90": "same as width_50 but at >=90% of the local peak",
            "line_to_truth": "distance from detected line pixels (line>=0.5) to the nearest truth boundary pixel",
            "truth_to_line": "distance from truth boundary pixels to the nearest detected line pixel",
            "unwanted_line_rate": "fraction of detected line pixels farther than requested_width_px + 2 from the truth boundary",
        },
    }

    fixtures = (
        ("alpha-shapes", "straight"),
        ("alpha-shapes", "premultiplied"),
        ("depth-planes", "straight"),
    )
    for name, alpha_mode in fixtures:
        item = load_fixture(index, name, alpha_mode)
        fixture_path = ROOT / item["preview"].replace(".preview.png", ".npy")
        if not fixture_path.exists():
            raise SystemExit(
                f"fixture data not found: {fixture_path}; run generate_fixtures.py first"
            )
        rgba = load_npy(fixture_path)
        depth = None
        if item.get("has_depth"):
            depth_path = fixture_path.with_name(f"{item['name']}.depth.npy")
            if not depth_path.exists():
                raise SystemExit(
                    f"depth data not found: {depth_path}; run generate_fixtures.py first"
                )
            depth = load_npy(depth_path)

        result = ref.extract(rgba, depth, settings)

        # The depth fixture isolates depth edges; alpha fixtures isolate alpha.
        if item.get("truth_depth"):
            truth_path = ROOT / item["truth_depth"]
            truth_kind = "depth"
        else:
            truth_path = ROOT / item["truth_alpha"]
            truth_kind = "alpha"
        if not truth_path.exists():
            raise SystemExit(
                f"truth data not found: {truth_path}; run generate_fixtures.py first"
            )
        truth = np.load(truth_path)
        primary = case_metrics(result["line"], truth, settings.width_px)
        detail_result = ref.extract(rgba, depth, detail_settings)
        detail_variant = case_metrics(detail_result["line"], truth, settings.width_px)

        case_dir = out_dir / f"{name}-{alpha_mode}"
        case_dir.mkdir(parents=True, exist_ok=True)
        rgb = rgba[..., :3]
        alpha = rgba[..., 3]
        is_premultiplied = alpha_mode == "premultiplied"
        to_png(case_dir / "source.png", ref.composite(rgb, alpha, "black", premultiplied=is_premultiplied))
        gray_png(case_dir / "silhouette.png", result["silhouette"])
        gray_png(case_dir / "alpha-detail.png", result["alpha_detail"])
        if result["depth_discontinuity"] is not None:
            gray_png(case_dir / "depth-discontinuity.png", result["depth_discontinuity"])
        gray_png(case_dir / "line.png", result["line"])
        gray_png(case_dir / "line-with-detail.png", detail_result["line"])
        for bg in ("black", "gray", "white"):
            to_png(case_dir / f"composite-{bg}.png", ref.composite(rgb, alpha, bg, premultiplied=is_premultiplied))

        report["fixtures"].append({
            "name": item["name"],
            "alpha_mode": item["alpha_mode"],
            "width": item["width"],
            "height": item["height"],
            "source_sha256": item["sha256"],
            "depth_applied": depth is not None,
            "truth": truth_kind,
            "requested_width_px": float(settings.width_px),
            "primary": primary,
            "detail_variant": detail_variant,
            "line_width_measured": True,
            "limitations": report["limitations"],
        })

    report_path = out_dir / "outline-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())