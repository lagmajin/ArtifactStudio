#!/usr/bin/env python3
"""Generate CPU-only goldens for the source Vector4 Invert operation."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def f32(value: float) -> float:
    result = struct.unpack("<f", struct.pack("<f", value))[0]
    if not math.isfinite(result):
        raise ValueError("Invert candidate input or output is outside finite binary32")
    return result


def evaluate(color: list[float], factor: float) -> list[float]:
    if len(color) != 4:
        raise ValueError("Invert requires a four-component Vector4")
    fac = min(max(f32(factor), 0.0), 1.0)
    output = []
    for component in color:
        value = f32(component)
        inverted = f32(1.0 - value)
        delta = f32(inverted - value)
        output.append(f32(value + f32(fac * delta)))
    return output


def build_golden() -> dict:
    probes = [
        ("factor-zero", [0.2, 0.4, 0.6, 0.8], 0.0),
        ("factor-half", [0.2, 0.4, 0.6, 0.8], 0.5),
        ("factor-one", [0.2, 0.4, 0.6, 0.8], 1.0),
        ("factor-below-zero-saturates", [0.2, 0.4, 0.6, 0.8], -1.0),
        ("factor-above-one-saturates", [0.2, 0.4, 0.6, 0.8], 2.0),
        ("raw-vector-components-outside-unit-range", [-0.5, 1.5, 2.0, -1.0], 1.0),
        ("legacy-unconnected-color-default", [0.0, 0.0, 0.0, 1.0], 1.0),
    ]
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-vector4-invert-golden",
        "status": "source-derived-cpu-formula-candidate-no-renderer-runtime",
        "operationId": "artifact.vector4.invert.linear_factor",
        "legacyInputType": "Vector4",
        "legacyOutputType": "Vector4",
        "formula": "lerp(color, 1-color, saturate(factor)); apply to all four components, with no color-space conversion or result clamp",
        "cases": [
            {"caseId": name, "color": color, "factor": factor,
             "expectedVector4": evaluate(color, factor),
             "factorSaturated": min(max(f32(factor), 0.0), 1.0)}
            for name, color, factor in probes
        ],
        "limitations": [
            "Vector4 is treated as a raw numeric tuple; this operation does not claim Color4 transfer or alpha-association semantics",
            "CPU binary32 candidate does not establish HLSL compiler contraction, GPU precision, or backend parity",
            "source-derived Artifact behavior is not Blender parity",
            "no Core operation registry or lowering is implemented",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/vector4_invert_formula_golden.json"))
    args = parser.parse_args()
    target = args.project_root.resolve() / args.output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(build_golden(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
