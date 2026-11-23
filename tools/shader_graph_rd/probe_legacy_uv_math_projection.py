#!/usr/bin/env python3
"""Compile and numerically probe a dynamic UV0.x arithmetic graph projection."""

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


def load_module(name: str, path: Path) -> Any:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run(command: list[str]) -> dict[str, Any]:
    result = subprocess.run(command, capture_output=True, text=True)
    return {"exitCode": result.returncode, "stdout": result.stdout,
            "stderr": result.stderr}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def compile_dynamic(source: str, stem: str, frontend: str, output_dir: Path,
                    args: Any, frontend_helpers: Any,
                    compiler_helpers: Any) -> dict[str, Any]:
    extension = ".frag" if frontend == "glsl" else ".hlsl"
    source_path = output_dir / f"{stem}{extension}"
    spirv_path = output_dir / f"{stem}_{frontend}.spv"
    optimized_path = output_dir / f"{stem}_{frontend}_opt.spv"
    source_path.write_text(source, encoding="utf-8", newline="\n")
    if frontend == "glsl":
        compile_result = run([str(args.glslang), "-V", "--target-env", "vulkan1.2",
                              "-S", "frag", "-o", str(spirv_path), str(source_path)])
    else:
        compile_result = run([str(args.dxc), "-spirv", "-T", "ps_6_0", "-E", "main",
                              "-fspv-target-env=vulkan1.2", "-Fo", str(spirv_path),
                              str(source_path)])
    validation = (run([str(args.spirv_val), "--target-env", "vulkan1.2", str(spirv_path)])
                  if compile_result["exitCode"] == 0 else {"exitCode": None})
    optimize = (run([str(args.spirv_opt), "-O", str(spirv_path), "-o", str(optimized_path)])
                if validation["exitCode"] == 0 else {"exitCode": None})
    optimized_validation = (run([str(args.spirv_val), "--target-env", "vulkan1.2",
                                 str(optimized_path)]) if optimize["exitCode"] == 0
                            else {"exitCode": None})
    disassembly = (run([str(args.spirv_dis), str(optimized_path), "-o", "-"])
                   if optimized_validation["exitCode"] == 0 else
                   {"exitCode": None, "stdout": ""})
    interface = None
    output_value = None
    error = None
    if disassembly["exitCode"] == 0:
        interface = frontend_helpers.parse_spirv_interface(disassembly["stdout"])
        try:
            output_value = compiler_helpers.extract_output_scalar(disassembly["stdout"])
        except RuntimeError as exception:
            error = str(exception)
    passed = (compile_result["exitCode"] == 0 and validation["exitCode"] == 0 and
              optimize["exitCode"] == 0 and optimized_validation["exitCode"] == 0 and
              disassembly["exitCode"] == 0)
    return {"compile": compile_result, "validation": validation,
            "optimize": optimize, "optimizedValidation": optimized_validation,
            "interface": interface, "output": output_value,
            "outputError": error, "status": "passed" if passed else "failed"}


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=root / "temp" / "shader-graph-core-projection-compile")
    for option, tool in (("glslang", "glslangValidator"), ("dxc", "dxc"),
                         ("spirv-val", "spirv-val"), ("spirv-dis", "spirv-dis"),
                         ("spirv-opt", "spirv-opt")):
        parser.add_argument(f"--{option}", type=Path,
                            default=Path(shutil.which(tool) or tool))
    args = parser.parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    prototype = load_module("legacy_uv_math_graph_prototype",
        root / "experiments/ShaderGraphLab/dynamic_graph_prototype.py")
    compiler_helpers = load_module("legacy_uv_math_compile_helpers",
        root / "tools/shader_graph_rd/compile_legacy_core_projection_candidates.py")
    frontends = load_module("legacy_uv_math_frontend_helpers",
        root / "tools/shader_graph_rd/compile_glsl_vulkan_candidates.py")
    numeric = load_module("legacy_uv_math_numeric_helpers",
        root / "tools/shader_graph_rd/probe_vulkan_ramp_numeric_parity.py")
    registry = prototype.load_registry(root / "experiments/ShaderGraphLab/registry.json")
    fixture_path = root / "experiments/ShaderGraphLab/dynamic_uv_roughness_math.json"
    fixture_bytes = fixture_path.read_bytes()
    document = json.loads(fixture_bytes.decode("utf-8"))
    compiled = prototype.compile_document_from_data(document, registry,
        hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
    expected_hlsl = (
        "float n_bias_value = 0.125f;\n"
        "float n_gain_value = 0.5f;\n"
        "float n_uv_value = In.UV.x;\n"
        "float n_scale_result = (n_uv_value * n_gain_value);\n"
        "float n_roughness_result = (n_scale_result + n_bias_value);\n"
        "return n_roughness_result;\n")
    if compiled["hlslBody"] != expected_hlsl:
        raise RuntimeError("dynamic UV arithmetic graph lowered to unexpected HLSL")
    glsl_source, interface = prototype.emit_glsl_from_typed_ir(
        compiled["typedIR"], compiled["parameterBlock"],
        context_bindings={("artifact.material.surface.uv0", "x"): "artifactHostUV.x"})
    glsl_source = glsl_source.replace("#version 450\n",
        "#version 450\nlayout(location = 0) in vec2 artifactHostUV;\n", 1)
    hlsl_source = frontends.build_hlsl_fragment(compiled, interface)
    dynamic_glsl = compile_dynamic(glsl_source, "uv_math_dynamic", "glsl", output_dir,
                                   args, frontends, compiler_helpers)
    dynamic_hlsl = compile_dynamic(hlsl_source, "uv_math_dynamic", "hlsl", output_dir,
                                   args, frontends, compiler_helpers)
    dynamic_interface_match = (dynamic_glsl["interface"] is not None and
                               dynamic_glsl["interface"] == dynamic_hlsl["interface"])

    samples = []
    for index, uv_x in enumerate((-0.25, 0.0, 0.125, 0.5, 1.0, 1.75)):
        literal = format(uv_x, ".9g")
        if "." not in literal and "e" not in literal.lower():
            literal += ".0"
        glsl_case = compile_dynamic(glsl_source.replace("artifactHostUV.x", literal),
            f"uv_math_sample_{index:02d}", "glsl", output_dir, args,
            frontends, compiler_helpers)
        hlsl_case = compile_dynamic(hlsl_source.replace("In.UV.x", literal + "f"),
            f"uv_math_sample_{index:02d}", "hlsl", output_dir, args,
            frontends, compiler_helpers)
        expected = numeric.f32(numeric.f32(uv_x * 0.5) + 0.125)
        exact_match = (glsl_case["output"] is not None and hlsl_case["output"] is not None and
            numeric.f32_bits(glsl_case["output"]) == numeric.f32_bits(expected) and
            numeric.f32_bits(hlsl_case["output"]) == numeric.f32_bits(expected))
        samples.append({"uv0X": uv_x, "expectedBinary32": expected,
            "glslOutput": glsl_case["output"], "hlslOutput": hlsl_case["output"],
            "glslStatus": glsl_case["status"], "hlslStatus": hlsl_case["status"],
            "exactBinary32GoldenMatch": exact_match,
            "status": "passed" if glsl_case["status"] == hlsl_case["status"] ==
                "passed" and exact_match else "failed"})
        print(f"UV0.x={uv_x}: {'PASS' if samples[-1]['status'] == 'passed' else 'FAIL'}")

    passed = (compiled["outputType"] == "Float" and dynamic_interface_match and
        dynamic_glsl["status"] == dynamic_hlsl["status"] == "passed" and
        "In.UV.x" in hlsl_source and "artifactHostUV.x" in glsl_source and
        all(sample["status"] == "passed" for sample in samples))
    result = {"schemaId": "artifact.shadergraph.dynamic-uv-math-compiler-evidence",
        "schemaVersion": 1, "status": "passed" if passed else "failed",
        "targetEnvironment": "vulkan1.2", "sourceFixture": fixture_path.relative_to(root).as_posix(),
        "sourceSha256": sha256(fixture_bytes), "outputType": compiled["outputType"],
        "semanticHashSha256": compiled["semanticHashSha256"],
        "hlslBodySha256": compiled["hlslBodySha256"],
        "hlslBody": compiled["hlslBody"], "contextBindings": compiled["contextBindings"],
        "dynamicInputRetained": "In.UV.x" in hlsl_source and "artifactHostUV.x" in glsl_source,
        "dynamicInputInterfaceMatch": dynamic_interface_match,
        "dynamicGlSl": {k:v for k,v in dynamic_glsl.items() if k != "interface"},
        "dynamicHlsl": {k:v for k,v in dynamic_hlsl.items() if k != "interface"},
        "numericSamples": samples,
        "limitations": ["No GPU execution", "No ArtifactCore renderer integration",
                        "No Diligent binding or project persistence verification"]}
    summary_path = output_dir / "dynamic-uv-math-summary.json"
    summary_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                            encoding="utf-8", newline="\n")
    print(f"status={result['status']} samples={len(samples)} summary={summary_path}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
