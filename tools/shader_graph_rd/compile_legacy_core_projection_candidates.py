#!/usr/bin/env python3
"""Compile read-only legacy Core graph projections through both Vulkan frontends."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


CASES = (
    ("document", "base_color_add"),
    ("nestedAddProjection", "nested_add"),
    ("scalarBaseColorConversionProbe", "scalar_conversion"),
    ("unconnectedBaseColorDefaultProbe", "base_color_default"),
)


def load_module(name: str, path: Path) -> Any:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def run(command: list[str]) -> dict[str, Any]:
    result = subprocess.run(command, capture_output=True, text=True)
    return {"exitCode": result.returncode, "stdout": result.stdout,
            "stderr": result.stderr}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=root / "temp" / "shader-graph-core-projection-compile")
    parser.add_argument("--glslang", type=Path,
                        default=Path(shutil.which("glslangValidator") or "glslangValidator"))
    parser.add_argument("--dxc", type=Path,
                        default=Path(shutil.which("dxc") or "dxc"))
    parser.add_argument("--spirv-val", type=Path,
                        default=Path(shutil.which("spirv-val") or "spirv-val"))
    parser.add_argument("--spirv-dis", type=Path,
                        default=Path(shutil.which("spirv-dis") or "spirv-dis"))
    parser.add_argument("--spirv-opt", type=Path,
                        default=Path(shutil.which("spirv-opt") or "spirv-opt"))
    args = parser.parse_args()

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    compiler = load_module(
        "shader_graph_legacy_projection_compile_helpers",
        root / "tools" / "shader_graph_rd" / "compile_glsl_vulkan_candidates.py")
    numeric_probe = load_module(
        "shader_graph_legacy_projection_numeric_helpers",
        root / "tools" / "shader_graph_rd" / "probe_vulkan_ramp_numeric_parity.py")
    prototype = load_module(
        "shader_graph_legacy_projection_prototype",
        root / "experiments" / "ShaderGraphLab" / "dynamic_graph_prototype.py")
    registry = prototype.load_registry(root / "experiments" / "ShaderGraphLab" / "registry.json")
    candidate_path = root / "experiments" / "ShaderGraphLab" / "core_math_add_projection_candidate.json"
    candidate_bytes = candidate_path.read_bytes()
    candidate = json.loads(candidate_bytes.decode("utf-8"))
    records = []

    for key, label in CASES:
        projection = candidate if key == "document" else candidate[key]
        document = projection["document"]
        compiled = prototype.compile_document_from_data(document, registry)
        context_bindings = {}
        for binding in compiled["contextBindings"]:
            if binding["contextId"] != "artifact.material.surface.uv0":
                raise RuntimeError(f"unsupported projected context: {binding['contextId']}")
            context_bindings[(binding["contextId"], binding["component"])] = (
                "artifactHostUV" if binding["component"] == "xy"
                else f"artifactHostUV.{binding['component']}"
            )
        glsl_source, interface = prototype.emit_glsl_from_typed_ir(
            compiled["typedIR"], compiled["parameterBlock"],
            context_bindings=context_bindings)
        if context_bindings:
            glsl_source = glsl_source.replace(
                "#version 450\n",
                "#version 450\nlayout(location = 0) in vec2 artifactHostUV;\n", 1)
        hlsl_source = compiler.build_hlsl_fragment(compiled, interface)

        glsl_path = output_dir / f"{label}.frag"
        hlsl_path = output_dir / f"{label}.hlsl"
        glsl_spirv = output_dir / f"{label}.spv"
        hlsl_spirv = output_dir / f"{label}_hlsl.spv"
        glsl_optimized = output_dir / f"{label}_opt.spv"
        hlsl_optimized = output_dir / f"{label}_hlsl_opt.spv"
        glsl_bytes = glsl_source.encode("utf-8")
        hlsl_bytes = hlsl_source.encode("utf-8")
        glsl_path.write_bytes(glsl_bytes)
        hlsl_path.write_bytes(hlsl_bytes)

        glsl_compile = run([str(args.glslang), "-V", "--target-env", "vulkan1.2",
                            "-S", "frag", "-o", str(glsl_spirv), str(glsl_path)])
        hlsl_compile = run([str(args.dxc), "-spirv", "-T", "ps_6_0", "-E", "main",
                            "-fspv-target-env=vulkan1.2", "-Fo", str(hlsl_spirv),
                            str(hlsl_path)])
        glsl_validate = (run([str(args.spirv_val), "--target-env", "vulkan1.2",
                              str(glsl_spirv)]) if glsl_compile["exitCode"] == 0
                         else {"exitCode": None, "stdout": "", "stderr": ""})
        hlsl_validate = (run([str(args.spirv_val), "--target-env", "vulkan1.2",
                              str(hlsl_spirv)]) if hlsl_compile["exitCode"] == 0
                         else {"exitCode": None, "stdout": "", "stderr": ""})
        glsl_optimize = (run([str(args.spirv_opt), "-O", str(glsl_spirv),
                              "-o", str(glsl_optimized)]) if glsl_validate["exitCode"] == 0
                        else {"exitCode": None, "stdout": "", "stderr": ""})
        hlsl_optimize = (run([str(args.spirv_opt), "-O", str(hlsl_spirv),
                              "-o", str(hlsl_optimized)]) if hlsl_validate["exitCode"] == 0
                        else {"exitCode": None, "stdout": "", "stderr": ""})
        glsl_optimized_validate = (run([str(args.spirv_val), "--target-env", "vulkan1.2",
                                        str(glsl_optimized)]) if glsl_optimize["exitCode"] == 0
                                   else {"exitCode": None, "stdout": "", "stderr": ""})
        hlsl_optimized_validate = (run([str(args.spirv_val), "--target-env", "vulkan1.2",
                                        str(hlsl_optimized)]) if hlsl_optimize["exitCode"] == 0
                                   else {"exitCode": None, "stdout": "", "stderr": ""})

        interface_match = False
        output_match = False
        glsl_output = hlsl_output = expected_output = None
        output_error = None
        glsl_disassembly = hlsl_disassembly = None
        if glsl_optimized_validate["exitCode"] == 0 and hlsl_optimized_validate["exitCode"] == 0:
            glsl_disassembly = run([str(args.spirv_dis), str(glsl_optimized), "-o", "-"])
            hlsl_disassembly = run([str(args.spirv_dis), str(hlsl_optimized), "-o", "-"])
            if glsl_disassembly["exitCode"] == 0 and hlsl_disassembly["exitCode"] == 0:
                glsl_interface = compiler.parse_spirv_interface(glsl_disassembly["stdout"])
                hlsl_interface = compiler.parse_spirv_interface(hlsl_disassembly["stdout"])
                interface_match = glsl_interface == hlsl_interface
                try:
                    glsl_output = numeric_probe.output_vector(glsl_disassembly["stdout"])
                    hlsl_output = numeric_probe.output_vector(hlsl_disassembly["stdout"])
                    expected_output = (projection.get("cpuVector4AddGolden") or
                                       projection.get("cpuVector4Golden"))
                    if expected_output is None or len(expected_output) != 4:
                        raise RuntimeError("projection has no four-lane CPU golden")
                    output_match = all(
                        numeric_probe.f32_bits(actual) == numeric_probe.f32_bits(float(expected))
                        for actual, expected in zip(glsl_output, expected_output)
                    ) and all(
                        numeric_probe.f32_bits(actual) == numeric_probe.f32_bits(float(expected))
                        for actual, expected in zip(hlsl_output, expected_output)
                    )
                except (RuntimeError, ValueError, KeyError) as error:
                    output_error = str(error)

        passed = (glsl_compile["exitCode"] == 0 and hlsl_compile["exitCode"] == 0 and
                  glsl_validate["exitCode"] == 0 and hlsl_validate["exitCode"] == 0 and
                  glsl_optimized_validate["exitCode"] == 0 and
                  hlsl_optimized_validate["exitCode"] == 0 and interface_match and output_match)
        records.append({
            "caseId": label,
            "projectionSha256": sha256(json.dumps(
                projection, sort_keys=True, separators=(",", ":")
            ).encode("utf-8")),
            "documentSha256": sha256(json.dumps(
                document, sort_keys=True, separators=(",", ":")
            ).encode("utf-8")),
            "outputType": compiled["outputType"],
            "glslSha256": sha256(glsl_bytes),
            "hlslSha256": sha256(hlsl_bytes),
            "glslSpirvSha256": sha256(glsl_spirv.read_bytes()) if glsl_spirv.is_file() else None,
            "hlslSpirvSha256": sha256(hlsl_spirv.read_bytes()) if hlsl_spirv.is_file() else None,
            "glslCompile": glsl_compile,
            "hlslCompile": hlsl_compile,
            "glslValidation": glsl_validate,
            "hlslValidation": hlsl_validate,
            "glslOptimize": glsl_optimize,
            "hlslOptimize": hlsl_optimize,
            "glslOptimizedValidation": glsl_optimized_validate,
            "hlslOptimizedValidation": hlsl_optimized_validate,
            "interfaceMatch": interface_match,
            "expectedOutputRGBA32F": expected_output,
            "glslOutputRGBA32F": glsl_output,
            "hlslOutputRGBA32F": hlsl_output,
            "exactBinary32GoldenMatch": output_match,
            "outputReadbackError": output_error,
            "gpuExecution": False,
            "status": "passed" if passed else "failed",
        })
        print(f"{label}: {'PASS' if passed else 'FAIL'}")

    result = {
        "schemaId": "artifact.shadergraph.legacy-core-projection-compiler-evidence",
        "schemaVersion": 1,
        "status": "passed" if all(item["status"] == "passed" for item in records) else "failed",
        "targetEnvironment": "vulkan1.2",
        "sourceCandidate": str(candidate_path.relative_to(root)).replace("\\", "/"),
        "sourceCandidateSha256": sha256(candidate_bytes),
        "candidateCount": len(records),
        "passedCount": sum(item["status"] == "passed" for item in records),
        "limitations": [
            "No GPU execution or image response comparison",
            "No ArtifactCore loader or renderer integration",
            "SPIR-V interface agreement does not establish Diligent binding compatibility",
        ],
        "candidates": records,
    }
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8", newline="\n")
    print(f"status={result['status']} candidates={result['candidateCount']} "
          f"passed={result['passedCount']} summary={summary_path}")
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
