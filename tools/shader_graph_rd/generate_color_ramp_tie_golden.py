#!/usr/bin/env python3
"""Generate binary32 formula goldens for the proposed ColorRamp tie contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


FIXTURES = (
    "ramp_duplicate_stops_forward.json",
    "ramp_duplicate_stops_reversed.json",
)
FACTORS = (0.0, 0.4999, 0.5, 0.5001, 1.0)


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def evaluate(stops: list[dict], factor: float) -> list[float]:
    ordered = sorted(enumerate(stops), key=lambda item: (f32(item[1]["pos"]), item[0]))
    t = f32(factor)
    if not ordered:
        raise ValueError("ColorRamp needs at least one stop")
    t = f32(min(f32(ordered[-1][1]["pos"]), max(f32(ordered[0][1]["pos"]), t)))

    # At an exact tied coordinate, choose the first authored stop in that group.
    # Values on either side interpolate over the adjacent distinct-position span.
    for index, (pos, stop) in enumerate((f32(item[1]["pos"]), item[1]) for item in ordered):
        if t == pos:
            if index > 0 and f32(ordered[index - 1][1]["pos"]) == pos:
                continue
            return [f32(stop[channel]) for channel in ("r", "g", "b", "a")]

    left = ordered[0][1]
    right = ordered[-1][1]
    for index in range(1, len(ordered)):
        p0 = f32(ordered[index - 1][1]["pos"])
        p1 = f32(ordered[index][1]["pos"])
        if p0 < t < p1:
            left = ordered[index - 1][1]
            right = ordered[index][1]
            break
    p0, p1 = f32(left["pos"]), f32(right["pos"])
    amount = f32(f32(t - p0) / f32(p1 - p0))
    result = []
    for channel in ("r", "g", "b", "a"):
        a, b = f32(left[channel]), f32(right[channel])
        delta = f32(b - a)
        result.append(f32(a + f32(delta * amount)))
    return result


def make_golden(fixture_root: Path) -> dict:
    cases = []
    for name in FIXTURES:
        payload = json.loads((fixture_root / name).read_text(encoding="utf-8"))
        ramp = next(node for node in payload["nodes"] if node["id"] == "ramp")
        stops = ramp["params"]["stops"]
        cases.append({
            "fixture": name,
            "fixtureSha256": hashlib.sha256((fixture_root / name).read_bytes()).hexdigest().upper(),
            "samples": [
                {"factor": factor, "linearRgbaBinary32": evaluate(stops, factor)}
                for factor in FACTORS
            ],
        })
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-ramp-formula-golden",
        "status": "candidate-core-math-only-not-runtime-evidence",
        "arithmetic": "IEEE-754 binary32 round-to-nearest at each declared arithmetic operation; no fused multiply-add",
        "factorDomain": "clamp to the minimum and maximum finite stop positions after stable sorting",
        "interpolation": "linear",
        "stopOrdering": "stable ascending (position, authored array index)",
        "exactTieRule": "first authored stop at the exact tied position; interpolate adjacent distinct-position intervals on either side",
        "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture-root", type=Path, default=Path("tests/fixtures/shader-graph-rd"))
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/ramp_duplicate_stops_formula_golden.json"))
    args = parser.parse_args()
    golden = make_golden(args.fixture_root)
    args.output.write_text(json.dumps(golden, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
