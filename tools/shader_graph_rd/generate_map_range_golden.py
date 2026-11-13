#!/usr/bin/env python3
"""Generate CPU binary32 candidates for the legacy MapRange lowering."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def f32(value: float) -> float:
    value = struct.unpack("<f", struct.pack("<f", value))[0]
    if not math.isfinite(value):
        raise ValueError("MapRange candidate input/output must be finite binary32")
    return value


def evaluate(value: float, from_min: float, from_max: float, to_min: float,
             to_max: float, steps: float, interpolation: str, clamp_range: bool) -> float:
    value, from_min, from_max, to_min, to_max, steps = map(
        f32, (value, from_min, from_max, to_min, to_max, steps))
    span = f32(from_max - from_min)
    if abs(span) < 1.0e-5:
        span = 1.0
    t = f32(f32(value - from_min) / span)
    if interpolation == "stepped":
        count = max(f32(math.floor(steps)), 1.0)
        t = f32(f32(math.floor(f32(min(max(t, 0.0), 1.0) * count))) / count)
    elif interpolation == "smoothstep":
        t = f32(f32(f32(t * t) * f32(3.0 - f32(2.0 * t))))
    elif interpolation == "smootherstep":
        t2 = f32(t * t)
        t3 = f32(t2 * t)
        inner = f32(f32(t * f32(f32(t * 6.0) - 15.0)) + 10.0)
        t = f32(t3 * inner)
    elif interpolation != "linear":
        raise ValueError(f"unknown interpolation {interpolation}")
    result = f32(to_min + f32(t * f32(to_max - to_min)))
    if clamp_range:
        result = f32(min(max(result, min(to_min, to_max)), max(to_min, to_max)))
    return result


def build_golden() -> dict:
    defaults = dict(from_min=0.0, from_max=1.0, to_min=0.0, to_max=1.0, steps=4.0)
    probes = [
        ("linear-midpoint", 0.5, "linear", False, defaults),
        ("linear-extrapolates-below", -0.5, "linear", False, defaults),
        ("linear-clamps-output", -0.5, "linear", True, defaults),
        ("smoothstep-midpoint", 0.5, "smoothstep", False, defaults),
        ("smoothstep-does-not-clamp-input-t", -0.5, "smoothstep", False, defaults),
        ("smootherstep-midpoint", 0.5, "smootherstep", False, defaults),
        ("smootherstep-does-not-clamp-input-t", 1.5, "smootherstep", False, defaults),
        ("stepped-quarter", 0.25, "stepped", False, defaults),
        ("stepped-saturates-t-above-range", 1.5, "stepped", False, defaults),
        ("stepped-floor-steps-below-one", 0.5, "stepped", False, {**defaults, "steps": 0.25}),
        ("zero-span-fallback-one", 2.0, "linear", False, {**defaults, "from_min": 1.0, "from_max": 1.0}),
        ("reversed-source-span", 0.25, "linear", False, {**defaults, "from_min": 1.0, "from_max": 0.0}),
        ("reversed-target-range-clamp", 0.25, "linear", True, {**defaults, "to_min": 2.0, "to_max": -2.0}),
    ]
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-map-range-golden",
        "status": "source-derived-cpu-formula-candidate-no-renderer-runtime",
        "legacyNode": "MapRange",
        "legacyInputTypes": {"Value": "Float", "FromMin": "Float", "FromMax": "Float", "ToMin": "Float", "ToMax": "Float", "Steps": "Float"},
        "legacyOutputType": "Float",
        "formula": "span=fmax-fmin; if abs(span)<1e-5 then span=1; t=(value-fmin)/span; shape t by interpolation; map=tmin+t*(tmax-tmin); optionally clamp result to sorted target bounds",
        "cases": [
            {"caseId": name, "value": value, "interpolation": interp, "clampRange": clamp,
             "parameters": params,
             "expected": evaluate(value, params["from_min"], params["from_max"], params["to_min"], params["to_max"], params["steps"], interp, clamp)}
            for name, value, interp, clamp, params in probes
        ],
        "limitations": [
            "SmoothStep and SmootherStep use the raw t without saturation; this preserves current source behavior, including extrapolation outside [0,1]",
            "Stepped alone saturates t and floors Steps to an integer count with a minimum of one",
            "zero source span is replaced with positive one, including a negative near-zero span",
            "CPU binary32 sequencing does not establish HLSL compiler contraction, GPU precision, or backend parity",
            "source-derived Artifact behavior does not establish Blender compatibility",
            "legacy MapRange exposes one Steps pin even when interpolation does not consume it; no Core operation or runtime implementation is added",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/map_range_formula_golden.json"))
    args = parser.parse_args()
    target = args.project_root.resolve() / args.output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build_golden(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
