#!/usr/bin/env python3
"""Generate finite binary32 constant canonicalization candidate goldens."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


def canonical_binary32(value: float) -> tuple[float, str]:
    if not math.isfinite(value):
        raise ValueError("constant component must be finite binary64 input")
    packed = struct.pack(">f", value)
    result = struct.unpack(">f", packed)[0]
    if not math.isfinite(result):
        raise ValueError("constant component is outside finite binary32 range")
    if result == 0.0:
        packed = bytes.fromhex("00000000")
        result = 0.0
    return result, packed.hex().upper()


def vector_record(kind: str, values: list[float], value_type: str) -> dict:
    canonical = [canonical_binary32(value) for value in values]
    return {
        "semanticKind": kind,
        "valueType": value_type,
        "components": [value for value, _ in canonical],
        "componentBytesBigEndian": [encoded for _, encoded in canonical],
        "canonicalSemanticBytesHex": "".join(encoded for _, encoded in canonical),
        "status": "candidate-canonicalization-no-Core-codec",
    }


def build_golden(root: Path) -> dict:
    cases = [
        {"caseId": "scalar-positive-zero", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": 0.0, "expected": 0.0, "bytes": "00000000"},
        {"caseId": "scalar-negative-zero-normalizes", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": -0.0, "expected": 0.0, "bytes": "00000000"},
        {"caseId": "scalar-one-third-rounds-to-binary32", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": 1.0 / 3.0, "expected": 0.3333333432674408, "bytes": "3EAAAAAB"},
        {"caseId": "scalar-smallest-positive-subnormal", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": float.fromhex("0x1p-149"), "expected": float.fromhex("0x1p-149"), "bytes": "00000001"},
        {"caseId": "scalar-half-subnormal-rounds-to-zero", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": float.fromhex("0x1p-150"), "expected": 0.0, "bytes": "00000000"},
        {"caseId": "scalar-largest-finite-binary32", "operationId": "artifact.constant.scalar.float32", "valueType": "Float", "input": struct.unpack(">f", bytes.fromhex("7F7FFFFF"))[0], "expected": struct.unpack(">f", bytes.fromhex("7F7FFFFF"))[0], "bytes": "7F7FFFFF"},
    ]
    evaluated = []
    for case in cases:
        actual, encoded = canonical_binary32(case["input"])
        if actual != case["expected"] or encoded != case["bytes"]:
            raise ValueError(f"oracle expectation mismatch: {case['caseId']}")
        evaluated.append({**case, "actual": actual, "actualBytes": encoded, "matchesExpected": True})
    color = vector_record("color", [0.25, -0.5, 1.25, 0.75], "Color4")
    vector = vector_record("vector", [0.25, -0.5, 1.25, 0.75], "Vector4")
    if color["canonicalSemanticBytesHex"] != vector["canonicalSemanticBytesHex"]:
        raise ValueError("component-wise numeric payload should match for the identity-relabeled probe")
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-constant-value-golden",
        "status": "candidate-binary32-canonicalization-no-Core-runtime",
        "rounding": "IEEE-754 binary32 round-to-nearest ties-to-even as provided by the host conversion oracle",
        "byteOrder": "big-endian IEEE-754 binary32 per component",
        "signedZero": "normalize negative zero to positive zero before semantic serialization",
        "acceptedDomain": "finite source number whose rounded binary32 result remains finite",
        "rejectedInputs": ["NaN", "+Inf", "-Inf", "finite source number that overflows to binary32 infinity"],
        "scalarOperationId": "artifact.constant.scalar.float32",
        "colorOperationId": "artifact.constant.color4.float32",
        "rawVectorOperationId": "artifact.constant.vector4.float32",
        "scalarCases": evaluated,
        "typedTupleCases": {
            "color": color,
            "rawVector": vector,
            "sameNumericBytesDoNotImplySameSemanticType": color["valueType"] != vector["valueType"],
        },
        "rejectionCases": [
            {"caseId": "nan-rejected", "input": "NaN", "expectedError": "constant component must be finite binary64 input"},
            {"caseId": "positive-infinity-rejected", "input": "+Inf", "expectedError": "constant component must be finite binary64 input"},
            {"caseId": "binary32-overflow-rejected", "input": 3.4028236e38, "expectedError": "constant component is outside finite binary32 range"},
        ],
        "limitations": [
            "Python IEEE-754 conversion is a candidate oracle; it does not execute the Core codec",
            "Color4 transfer function and alpha association remain unspecified",
            "equal component bytes do not make Color4 and Vector4 interchangeable semantic types",
            "no shader compiler, GPU output, renderer parity, or project round-trip evidence is included",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/constant_value_canonicalization_golden.json"))
    args = parser.parse_args()
    root = args.project_root.resolve()
    result = build_golden(root)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
