#!/usr/bin/env python3
"""Compare compiled Vulkan GLSL/HLSL Ramp outputs with the binary32 CPU oracle."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
import hashlib
import importlib.util
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def load_module(path: Path, name: str) -> Any:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load Python module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        detail = (result.stdout + result.stderr).strip()
        raise RuntimeError(f"command failed ({result.returncode}): {command!r}\n{detail}")
    return result


def output_vector(disassembly: str) -> list[float]:
    location_zero: set[str] = set()
    output_variables: set[str] = set()
    composite_constants: dict[str, list[str]] = {}
    scalar_constants: dict[str, float] = {}
    stored_value: str | None = None

    for line in disassembly.splitlines():
        match = re.match(r"\s*OpDecorate\s+(%\S+)\s+Location\s+0\b", line)
        if match:
            location_zero.add(match.group(1))
            continue
        match = re.match(r"\s*(%\S+)\s*=\s*OpVariable\s+%\S+\s+Output\b", line)
        if match:
            output_variables.add(match.group(1))
            continue
        match = re.match(r"\s*(%\S+)\s*=\s*OpConstant\s+%\S+\s+([^\s]+)", line)
        if match:
            scalar_constants[match.group(1)] = float(match.group(2))
            continue
        match = re.match(r"\s*(%\S+)\s*=\s*OpConstantComposite\s+%\S+\s+(.+)", line)
        if match:
            composite_constants[match.group(1)] = match.group(2).split()
            continue
        match = re.match(r"\s*OpStore\s+(%\S+)\s+(%\S+)", line)
        if match and match.group(1) in output_variables | location_zero:
            stored_value = match.group(2)

    if stored_value is None or stored_value not in composite_constants:
        raise RuntimeError("optimized SPIR-V output is not a constant vector store")
    components = composite_constants[stored_value]
    if len(components) != 4 or any(item not in scalar_constants for item in components):
        raise RuntimeError("optimized SPIR-V output is not a four-lane float constant")
    return [scalar_constants[item] for item in components]


def f32_bits(value: float) -> bytes:
    return struct.pack("<f", value)


def f32_ulp_distance(left: float, right: float) -> int:
    def ordered(value: float) -> int:
        bits = struct.unpack("<I", f32_bits(value))[0]
        return (~bits & 0xFFFFFFFF) if bits & 0x80000000 else bits | 0x80000000
    return abs(ordered(left) - ordered(right))


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=root / "temp" / "shader-graph-glsl450-compile" /
                        "ramp-numeric-parity.json")
    parser.add_argument("--glslang", type=Path,
                        default=Path(shutil.which("glslangValidator") or "glslangValidator"))
    parser.add_argument("--dxc", type=Path,
                        default=Path(shutil.which("dxc") or "dxc"))
    parser.add_argument("--spirv-opt", type=Path,
                        default=Path(shutil.which("spirv-opt") or "spirv-opt"))
    parser.add_argument("--spirv-val", type=Path,
                        default=Path(shutil.which("spirv-val") or "spirv-val"))
    parser.add_argument("--spirv-dis", type=Path,
                        default=Path(shutil.which("spirv-dis") or "spirv-dis"))
    parser.add_argument("--keep-artifacts", action="store_true",
                        help="keep generated sources and SPIR-V files beside the report")
    args = parser.parse_args()

    document_path = root / "experiments" / "ShaderGraphLab" / "dynamic_vector4_ramp.json"
    registry_path = root / "experiments" / "ShaderGraphLab" / "registry.json"
    golden_path = root / "tests" / "fixtures" / "shader-graph-rd" / "dynamic_ramp_formula_golden.json"
    document_bytes = document_path.read_bytes()
    golden = json.loads(golden_path.read_text(encoding="utf-8"))
    if golden.get("sourceDocumentSha256") != sha256(document_bytes):
        raise RuntimeError("Ramp golden does not match the current source document")

    prototype = load_module(root / "experiments" / "ShaderGraphLab" /
                            "dynamic_graph_prototype.py", "shader_graph_ramp_numeric_probe")
    registry = prototype.load_registry(registry_path)
    compiled = prototype.compile_document_from_data(json.loads(document_bytes), registry)
    cases: list[dict[str, Any]] = []

    if args.keep_artifacts:
        scratch_path = args.output.parent / "ramp-numeric-work"
        scratch_path.mkdir(parents=True, exist_ok=True)
        scratch_context = nullcontext(str(scratch_path))
    else:
        scratch_context = tempfile.TemporaryDirectory(prefix="shader-graph-ramp-numeric-")
    with scratch_context as scratch:
        scratch_path = Path(scratch)
        for index, expected in enumerate(golden["cases"]):
            factor = float(expected["factor"])
            literal = format(factor, ".17g")
            hlsl_binding = f"(float2({literal}, 0.0f)).x"
            glsl_binding = f"(vec2({literal}, 0.0f)).x"
            context_id = "artifact.material.surface.uv0"
            context_component = "x"

            hlsl_body, _ = prototype.emit_hlsl_from_typed_ir(
                compiled["typedIR"], compiled["parameterBlock"],
                context_bindings={(context_id, context_component): hlsl_binding})
            glsl_source, glsl_interface = prototype.emit_glsl_from_typed_ir(
                compiled["typedIR"], compiled["parameterBlock"],
                context_bindings={(context_id, context_component): glsl_binding})
            hlsl_source = f"float4 main() : SV_Target0 {{\n{hlsl_body}}}\n"

            stem = f"case-{index}"
            glsl_path = scratch_path / f"{stem}.frag"
            hlsl_path = scratch_path / f"{stem}.hlsl"
            glsl_spv = scratch_path / f"{stem}-glsl.spv"
            hlsl_spv = scratch_path / f"{stem}-hlsl.spv"
            glsl_opt = scratch_path / f"{stem}-glsl-opt.spv"
            hlsl_opt = scratch_path / f"{stem}-hlsl-opt.spv"
            glsl_path.write_text(glsl_source, encoding="utf-8", newline="\n")
            hlsl_path.write_text(hlsl_source, encoding="utf-8", newline="\n")

            run([str(args.glslang), "-V", "--target-env", "vulkan1.2", "-S", "frag",
                 "-o", str(glsl_spv), str(glsl_path)])
            run([str(args.dxc), "-spirv", "-T", "ps_6_0", "-E", "main",
                 "-fspv-target-env=vulkan1.2", "-Fo", str(hlsl_spv), str(hlsl_path)])
            run([str(args.spirv_opt), "-O", str(glsl_spv), "-o", str(glsl_opt)])
            run([str(args.spirv_opt), "-O", str(hlsl_spv), "-o", str(hlsl_opt)])
            run([str(args.spirv_val), "--target-env", "vulkan1.2", str(glsl_opt)])
            run([str(args.spirv_val), "--target-env", "vulkan1.2", str(hlsl_opt)])
            glsl_dis = run([str(args.spirv_dis), str(glsl_opt), "-o", "-"]).stdout
            hlsl_dis = run([str(args.spirv_dis), str(hlsl_opt), "-o", "-"]).stdout
            glsl_value = output_vector(glsl_dis)
            hlsl_value = output_vector(hlsl_dis)
            expected_value = [float(component) for component in expected["color"]]
            glsl_matches_hlsl = all(f32_bits(a) == f32_bits(b)
                                    for a, b in zip(glsl_value, hlsl_value))
            glsl_matches_golden = all(f32_bits(a) == f32_bits(b)
                                      for a, b in zip(glsl_value, expected_value))
            hlsl_matches_golden = all(f32_bits(a) == f32_bits(b)
                                      for a, b in zip(hlsl_value, expected_value))
            glsl_ulp = [f32_ulp_distance(a, b)
                        for a, b in zip(glsl_value, expected_value)]
            hlsl_ulp = [f32_ulp_distance(a, b)
                        for a, b in zip(hlsl_value, expected_value)]
            cases.append({
                "factor": factor,
                "expectedRGBA32F": expected_value,
                "glslRGBA32F": glsl_value,
                "hlslRGBA32F": hlsl_value,
                "glslHlslExactBinary32": glsl_matches_hlsl,
                "glslGoldenExactBinary32": glsl_matches_golden,
                "hlslGoldenExactBinary32": hlsl_matches_golden,
                "glslGoldenUlpDistanceRGBA": glsl_ulp,
                "hlslGoldenUlpDistanceRGBA": hlsl_ulp,
                "maximumGoldenUlpDistance": max(glsl_ulp + hlsl_ulp),
                "glslSourceSha256": sha256(glsl_path.read_bytes()),
                "hlslSourceSha256": sha256(hlsl_path.read_bytes()),
                "glslOptimizedSpirvSha256": sha256(glsl_opt.read_bytes()),
                "hlslOptimizedSpirvSha256": sha256(hlsl_opt.read_bytes()),
                "glslBackendInterface": glsl_interface,
            })

    passed = bool(cases) and all(
        item["glslHlslExactBinary32"] and item["glslGoldenExactBinary32"] and
        item["hlslGoldenExactBinary32"] for item in cases)
    report = {
        "schemaId": "artifact.shadergraph.vulkan-ramp-numeric-parity-evidence",
        "schemaVersion": 1,
        "status": "passed" if passed else "failed",
        "claim": "exact binary32 compiled-output equality for constant-folded SPIR-V probes",
        "sourceDocument": document_path.relative_to(root).as_posix(),
        "sourceDocumentSha256": sha256(document_bytes),
        "goldenFixture": golden_path.relative_to(root).as_posix(),
        "goldenSha256": sha256(golden_path.read_bytes()),
        "algorithm": golden.get("algorithm"),
        "targetEnvironment": "vulkan1.2",
        "optimization": "spirv-opt -O applied independently to each compiler output before reading constant output",
        "glslangValidator": str(args.glslang),
        "dxc": str(args.dxc),
        "spirvTools": {"optimizer": str(args.spirv_opt), "validator": str(args.spirv_val),
                        "disassembler": str(args.spirv_dis)},
        "gpuExecution": False,
        "limitations": [
            "This is compiler plus SPIR-V constant-fold evidence, not GPU runtime execution.",
            "Strict comparison is exact binary32; no tolerance was applied to the CPU formula oracle.",
            "It covers the five pre-registered Ramp factors only.",
            "It does not establish Diligent integration, rendered image parity, color-space behavior, or Blender parity."
        ],
        "caseCount": len(cases),
        "glslHlslExactBinary32Count": sum(item["glslHlslExactBinary32"] for item in cases),
        "glslGoldenExactBinary32Count": sum(item["glslGoldenExactBinary32"] for item in cases),
        "hlslGoldenExactBinary32Count": sum(item["hlslGoldenExactBinary32"] for item in cases),
        "maximumGoldenUlpDistance": max(
            item["maximumGoldenUlpDistance"] for item in cases) if cases else None,
        "cases": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"status={report['status']} cases={len(cases)} "
          f"glsl-hlsl={report['glslHlslExactBinary32Count']}/{len(cases)} "
          f"glsl-golden={report['glslGoldenExactBinary32Count']}/{len(cases)} "
          f"hlsl-golden={report['hlslGoldenExactBinary32Count']}/{len(cases)} "
          f"max-golden-ulp={report['maximumGoldenUlpDistance']}")
    print(f"evidence={args.output}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
