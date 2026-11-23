#!/usr/bin/env python3
"""Generate binary32 source-formula probes for Shader Graph Math operations."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


CASES = {
    "add": ([0.25, -2.0, 3.0, 0.5], [0.5, 1.0, -4.0, 0.25], None),
    "subtract": ([0.25, -2.0, 3.0, 0.5], [0.5, 1.0, -4.0, 0.25], None),
    "multiply": ([0.25, -2.0, 3.0, 0.5], [0.5, 1.0, -4.0, 0.25], None),
    "divide": ([1.0, 1.0, 1.0, 1.0], [0.0, -2.0e-5, 2.0e-5, -2.0], None),
    "mix": ([0.0, 0.0, 0.0, 0.0], [1.0, 2.0, -1.0, 0.5], 0.75),
    "sine": ([0.0, 0.5, -1.0, 3.1415927], None, None),
    "cosine": ([0.0, 0.5, -1.0, 3.1415927], None, None),
    "power": ([-2.0, -0.0, 0.5, 2.0], [3.0, 0.5, -1.0, 3.0], None),
    "minimum": ([0.25, -2.0, 3.0, 0.5], [0.5, 1.0, -4.0, 0.25], None),
    "maximum": ([0.25, -2.0, 3.0, 0.5], [0.5, 1.0, -4.0, 0.25], None),
    "less_than": ([0.25, 2.0, 3.0, 0.5], [0.5, 1.0, 3.0, 0.25], None),
    "greater_than": ([0.25, 2.0, 3.0, 0.5], [0.5, 1.0, 3.0, 0.25], None),
    "modulo": ([-5.5, 5.5, 2.0, -2.0], [2.0, 2.0, 0.0, -3.0], None),
    "absolute": ([-0.0, -2.5, 0.0, 3.0], None, None),
    "floor": ([-1.2, -0.0, 0.0, 1.8], None, None),
    "ceil": ([-1.2, -0.0, 0.0, 1.8], None, None),
    "fract": ([-1.2, -0.0, 0.0, 1.8], None, None),
}


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def evaluate(operation: str, a: list[float], b: list[float] | None, fac: float | None) -> list[float] | None:
    if operation in {"sine", "cosine", "mix"}:
        return None
    result: list[float] = []
    for index, raw_a in enumerate(a):
        av = f32(raw_a)
        bv = f32(b[index]) if b is not None else 0.0
        if operation == "add": value = f32(av + bv)
        elif operation == "subtract": value = f32(av - bv)
        elif operation == "multiply": value = f32(av * bv)
        elif operation == "divide": value = f32(av / max(bv, f32(1.0e-5)))
        elif operation == "mix":
            amount = f32(min(1.0, max(0.0, f32(fac))))
            value = f32(av + f32(f32(bv - av) * amount))
        elif operation == "power": value = f32(math.pow(max(av, 0.0), bv))
        elif operation == "minimum": value = min(av, bv)
        elif operation == "maximum": value = max(av, bv)
        elif operation == "less_than": value = 1.0 if av < bv else 0.0
        elif operation == "greater_than": value = 1.0 if av > bv else 0.0
        elif operation == "modulo":
            divisor = max(bv, f32(1.0e-5))
            value = f32(av - f32(divisor * f32(math.floor(f32(av / divisor)))))
        elif operation == "absolute": value = abs(av)
        elif operation == "floor": value = float(math.floor(av))
        elif operation == "ceil": value = float(math.ceil(av))
        elif operation == "fract": value = f32(math.copysign(av - math.floor(av), av))
        else: raise ValueError(f"unknown Math operation: {operation}")
        result.append(f32(value))
    return result


def make_golden(source: Path, fixture_root: Path) -> dict:
    source_bytes = source.read_bytes()
    cases = []
    for operation, (a, b, fac) in CASES.items():
        fixture_path = fixture_root / f"math_{operation}.json"
        fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
        cases.append({
            "operation": operation,
            "fixture": fixture_path.name,
            "fixtureSha256": hashlib.sha256(fixture_path.read_bytes()).hexdigest().upper(),
            "inputs": {"A": a, **({"B": b} if b is not None else {}), **({"Fac": fac} if fac is not None else {})},
            "expectedBinary32": evaluate(operation, a, b, fac),
            "oracleStatus": (
                "cpu-binary32-formula" if operation not in {"sine", "cosine", "mix"}
                else "source-expression-pinned-no-cross-libm-or-lerp-rounding-golden"
            ),
        })
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-math-formula-golden",
        "status": "candidate-core-math-only-not-runtime-or-cross-backend-evidence",
        "sourcePath": "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp",
        "sourceSha256": hashlib.sha256(source_bytes).hexdigest().upper(),
        "arithmetic": "binary32 round-to-nearest after each declared operation; no fused multiply-add; compiler fast-math behavior not characterized",
        "clampResult": False,
        "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp"))
    parser.add_argument("--fixture-root", type=Path, default=Path("tests/fixtures/shader-graph-rd"))
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/math_operation_formula_golden.json"))
    args = parser.parse_args()
    golden = make_golden(args.source, args.fixture_root)
    args.output.write_text(json.dumps(golden, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
