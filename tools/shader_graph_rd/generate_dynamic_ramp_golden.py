#!/usr/bin/env python3
"""Generate a CPU binary32 formula reference for the dynamic Ramp fixture."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct


FACTORS = (0.0, 0.4999, 0.5, 0.5001, 1.0)
SPAN_FLOOR = 1e-5


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def lerp_f32(left: float, right: float, factor: float) -> float:
    difference = f32(right - left)
    product = f32(difference * factor)
    return f32(left + product)


def evaluate(stops: list[dict[str, object]], factor: float) -> tuple[list[float], dict[str, int | str]]:
    canonical = [
        (index, f32(float(stop["position"])), [f32(float(lane)) for lane in stop["color"]])
        for index, stop in enumerate(stops)
    ]
    ordered = sorted(canonical, key=lambda item: (min(max(item[1], 0.0), 1.0), item[0]))
    ordered = [(index, min(max(position, 0.0), 1.0), color)
               for index, position, color in ordered]
    value = min(max(f32(factor), 0.0), 1.0)
    if value < ordered[0][1]:
        return list(ordered[0][2]), {"kind": "before-first", "authoredIndex": ordered[0][0]}
    for left, right in zip(ordered, ordered[1:]):
        if value >= left[1] and value < right[1]:
            span = max(f32(right[1] - left[1]), f32(SPAN_FLOOR))
            local = min(max(f32(f32(value - left[1]) / span), 0.0), 1.0)
            result = [lerp_f32(a, b, local) for a, b in zip(left[2], right[2])]
            return result, {"kind": "segment", "leftAuthoredIndex": left[0],
                            "rightAuthoredIndex": right[0]}
    return list(ordered[-1][2]), {"kind": "at-or-after-last", "authoredIndex": ordered[-1][0]}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    manifest_path = root / "tests/fixtures/shader-graph-rd/evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"]
    document_path = root / contract["vector4RampDocumentFixture"]
    document_bytes = document_path.read_bytes()
    document = json.loads(document_bytes.decode("utf-8"))
    ramp = next(node for node in document["nodes"] if node["nodeId"] == "ramp")
    stops = ramp["parameters"]["stops"]
    cases = []
    for factor in FACTORS:
        color, selection = evaluate(stops, factor)
        cases.append({"factor": f32(factor), "color": color, "alpha": color[3],
                      "selection": selection})
    golden = {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-dynamic-ramp-binary32-formula-golden",
        "algorithm": "artifact.vector4.ramp.linear_stops.1",
        "evidence": "independent-cpu-binary32-formula-only-no-hlsl-compiler-or-gpu-capture",
        "sourceDocument": contract["vector4RampDocumentFixture"],
        "sourceDocumentSha256": hashlib.sha256(document_bytes).hexdigest().upper(),
        "stopCount": len(stops),
        "stopPositionPolicy": "clamp-to-0..1-then-stable-sort-by-position-and-authored-index",
        "exactDuplicatePositionPolicy": "last-authored-stop-is-the-left-endpoint-at-the-exact-boundary",
        "segmentFormula": "binary32-lerp-with-half-open-segment-and-positive-1e-5-span-floor",
        "factors": list(FACTORS),
        "cases": cases,
        "limitations": [
            "binary32 CPU rounding does not model compiler reassociation or GPU execution",
            "does not establish rendered color-space, alpha-association, backend, or Blender parity",
        ],
    }
    output_path = root / "tests/fixtures/shader-graph-rd/dynamic_ramp_formula_golden.json"
    output_bytes = (json.dumps(golden, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    output_path.write_bytes(output_bytes)
    contract["vector4RampFormulaGoldenFixture"] = output_path.relative_to(root).as_posix()
    contract["vector4RampFormulaGoldenSha256"] = hashlib.sha256(output_bytes).hexdigest().upper()
    manifest_path.write_bytes((json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    print(f"Updated {contract['vector4RampFormulaGoldenFixture']} ({contract['vector4RampFormulaGoldenSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
