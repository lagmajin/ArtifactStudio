#!/usr/bin/env python3
"""Generate CPU-only golden cases for the current Vector4 Luminance lowering."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path


def f32(value: float) -> float:
    result = struct.unpack("<f", struct.pack("<f", value))[0]
    if not math.isfinite(result):
        raise ValueError("candidate input or result is outside finite binary32")
    return result


def evaluate(rgba: list[float]) -> float:
    if len(rgba) != 4:
        raise ValueError("Luminance probe requires four Vector4 components")
    rgb = [f32(value) for value in rgba[:3]]
    products = [f32(channel * coefficient) for channel, coefficient in zip(rgb, (0.2126, 0.7152, 0.0722))]
    return f32(f32(products[0] + products[1]) + products[2])


def build_golden() -> dict:
    probes = [
        ("primary-red-alpha-zero", [1.0, 0.0, 0.0, 0.0]),
        ("primary-green-alpha-zero", [0.0, 1.0, 0.0, 0.0]),
        ("primary-blue-alpha-zero", [0.0, 0.0, 1.0, 0.0]),
        ("mixed-rgb-alpha-one", [0.25, 0.5, 0.75, 1.0]),
        ("same-rgb-alpha-half", [0.25, 0.5, 0.75, 0.5]),
        ("negative-and-over-one-rgb", [-0.5, 1.5, 2.0, -2.0]),
    ]
    cases = []
    for case_id, rgba in probes:
        cases.append({
            "caseId": case_id,
            "inputVector4": rgba,
            "expectedFloat32": evaluate(rgba),
            "alphaIgnored": True,
            "roundingModel": "binary32 component conversion, multiply terms, then left-associated additions; CPU candidate only",
        })
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-luminance-formula-golden",
        "status": "source-derived-cpu-formula-candidate-no-renderer-runtime",
        "operationId": "artifact.vector4.weighted_luminance.legacy_v1",
        "legacyInputType": "Vector4",
        "legacyOutputType": "Float",
        "formula": "r*0.2126 + g*0.7152 + b*0.0722; ignore alpha; no clamp or transfer conversion",
        "cases": cases,
        "limitations": [
            "coefficients are pinned to current Artifact source and do not establish Blender parity",
            "input channels are treated as raw numeric Vector4 components; linear/sRGB interpretation is not selected",
            "CPU binary32 evaluation does not establish HLSL compiler contraction, GPU precision, or backend parity",
            "no Core operation registry or lowering is implemented",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/luminance_formula_golden.json"))
    args = parser.parse_args()
    result = build_golden()
    target = args.project_root.resolve() / args.output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
