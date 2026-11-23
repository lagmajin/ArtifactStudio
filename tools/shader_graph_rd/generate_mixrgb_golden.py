#!/usr/bin/env python3
"""Generate finite CPU formula probes for the legacy MixRGB lowering."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


MODES = ("mix", "add", "multiply", "screen", "overlay", "darken", "lighten", "difference")
COLOR1 = (0.25, 0.75, 0.5, 0.25)
COLOR2 = (0.75, 0.25, 0.5, 0.75)
FACTOR = 0.4


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def blend(mode: str, a: float, b: float) -> float:
    a, b = f32(a), f32(b)
    if mode == "mix": return b
    if mode == "add": return f32(a + b)
    if mode == "multiply": return f32(a * b)
    if mode == "screen": return f32(1.0 - f32(f32(1.0 - a) * f32(1.0 - b)))
    if mode == "overlay":
        low = f32(f32(2.0 * a) * b)
        high = f32(1.0 - f32(f32(2.0 * f32(1.0 - a)) * f32(1.0 - b)))
        return high if a >= 0.5 else low
    if mode == "darken": return min(a, b)
    if mode == "lighten": return max(a, b)
    if mode == "difference": return f32(abs(f32(a - b)))
    raise ValueError(f"unknown MixRGB mode: {mode}")


def evaluate(mode: str, color1: tuple[float, ...], color2: tuple[float, ...], factor: float,
             clamp_factor: bool, clamp_result: bool) -> list[float]:
    f = f32(factor)
    if clamp_factor:
        f = min(1.0, max(0.0, f))
    rgb = []
    for a, b in zip(color1[:3], color2[:3]):
        mixed = f32(f32(a) + f32(f * f32(blend(mode, a, b) - f32(a))))
        rgb.append(min(1.0, max(0.0, mixed)) if clamp_result else mixed)
    alpha = f32(f32(color1[3]) + f32(f * f32(f32(color2[3]) - f32(color1[3]))))
    return rgb + [alpha]


def make_golden(fixture_root: Path, source: Path) -> dict:
    cases = []
    for mode in MODES:
        path = fixture_root / f"mixrgb_{mode}.json"
        cases.append({
            "mode": mode,
            "fixture": path.name,
            "fixtureSha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
            "inputs": {"Color1": list(COLOR1), "Color2": list(COLOR2), "Fac": FACTOR,
                       "clampFactor": False, "clampResult": False},
            "candidateRgbaBinary32": evaluate(mode, COLOR1, COLOR2, FACTOR, False, False),
        })
    clamp_probes = [
        {"id": "factor-unclamped-negative", "mode": "add", "factor": -0.25, "clampFactor": False},
        {"id": "factor-clamped-negative", "mode": "add", "factor": -0.25, "clampFactor": True},
        {"id": "factor-unclamped-above-one", "mode": "add", "factor": 1.25, "clampFactor": False},
        {"id": "factor-clamped-above-one", "mode": "add", "factor": 1.25, "clampFactor": True},
    ]
    edge_a, edge_b = (0.8, 0.2, 0.5, 0.25), (0.8, 0.2, 0.5, 1.5)
    for item in clamp_probes:
        item["inputs"] = {"Color1": list(edge_a), "Color2": list(edge_b), "clampResult": False}
        item["candidateRgbaBinary32"] = evaluate(item["mode"], edge_a, edge_b, item["factor"], item["clampFactor"], False)
    for enabled in (False, True):
        clamp_probes.append({
            "id": f"result-rgb-only-clamp-{str(enabled).lower()}", "mode": "add", "factor": 1.0,
            "clampFactor": False, "inputs": {"Color1": list(edge_a), "Color2": list(edge_b), "clampResult": enabled},
            "candidateRgbaBinary32": evaluate("add", edge_a, edge_b, 1.0, False, enabled),
        })
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-mixrgb-formula-golden",
        "status": "candidate-cpu-formula-only-not-hlsl-runtime-or-backend-evidence",
        "sourcePath": "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp",
        "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest().upper(),
        "arithmetic": "candidate IEEE-754 binary32 with explicit round points; HLSL lerp/intrinsic lowering and fast-math may differ",
        "alphaRule": "candidate linear interpolation independent of RGB blend; clampResult does not clamp alpha",
        "cases": cases,
        "clampBoundaryCases": clamp_probes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-root", type=Path, default=Path("tests/fixtures/shader-graph-rd"))
    parser.add_argument("--source", type=Path, default=Path("ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp"))
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/mixrgb_formula_golden.json"))
    args = parser.parse_args()
    result = make_golden(args.fixture_root, args.source)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
