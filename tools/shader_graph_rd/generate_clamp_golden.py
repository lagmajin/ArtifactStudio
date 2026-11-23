#!/usr/bin/env python3
"""Generate CPU-only goldens for the current finite-scalar Clamp lowering."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def f32(value: float) -> float:
    result = struct.unpack("<f", struct.pack("<f", value))[0]
    if not math.isfinite(result):
        raise ValueError("Clamp candidate input or result is outside finite binary32")
    return result


def evaluate(value: float, minimum: float, maximum: float) -> float:
    v, a, b = f32(value), f32(minimum), f32(maximum)
    low, high = f32(min(a, b)), f32(max(a, b))
    return f32(min(max(v, low), high))


def build_golden() -> dict:
    probes = [
        ("inside-range", 0.5, 0.0, 1.0),
        ("below-range", -0.5, 0.0, 1.0),
        ("above-range", 1.5, 0.0, 1.0),
        ("reversed-bounds-low", 0.1, 0.8, 0.2),
        ("reversed-bounds-high", 0.9, 0.8, 0.2),
        ("equal-bounds", 0.2, 0.5, 0.5),
        ("negative-domain", -2.0, -1.0, 1.0),
    ]
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-clamp-formula-golden",
        "status": "source-derived-cpu-formula-candidate-no-renderer-runtime",
        "operationId": "artifact.math.clamp.sorted_bounds",
        "legacyInputType": "Float",
        "legacyOutputType": "Float",
        "formula": "clamp(value, min(minimum, maximum), max(minimum, maximum))",
        "cases": [
            {"caseId": name, "value": value, "minimum": minimum, "maximum": maximum,
             "expectedFloat32": evaluate(value, minimum, maximum)}
            for name, value, minimum, maximum in probes
        ],
        "limitations": [
            "candidate formula is pinned to current Artifact source; Blender parity is not claimed",
            "only finite binary32 inputs are modeled; non-finite handling belongs to document validation",
            "CPU binary32 evaluation does not establish HLSL compiler contraction, GPU precision, or backend parity",
            "no Core operation registry or lowering is implemented",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/clamp_formula_golden.json"))
    args = parser.parse_args()
    target = args.project_root.resolve() / args.output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build_golden(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
