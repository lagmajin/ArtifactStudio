#!/usr/bin/env python3
"""Generate and validate the persisted dynamic shader-graph GLSL candidates."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def load_prototype(path: Path) -> Any:
    spec = importlib.util.spec_from_file_location("shader_graph_glsl_vulkan_candidates", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load shader graph prototype: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def discover_pairs(root: Path) -> list[tuple[Path, Path]]:
    lab = root / "experiments" / "ShaderGraphLab"
    pairs = []
    for candidate in sorted(lab.glob("*_candidate.json")):
        document = candidate.with_name(candidate.name[:-len("_candidate.json")] + ".json")
        if document.is_file():
            pairs.append((document, candidate))
    fixture_candidate = root / "tests" / "fixtures" / "shader-graph-rd" / "dynamic_add_lowering_candidate.json"
    fixture_document = lab / "dynamic_add.json"
    if fixture_candidate.is_file() and fixture_document.is_file():
        pairs.append((fixture_document, fixture_candidate))
    return pairs


def build_hlsl_fragment(compiled: dict[str, Any], interface: dict[str, Any]) -> str:
    body = compiled["hlslBody"]
    cbuffer_start = body.find("cbuffer ArtifactGraphParameters {")
    cbuffer = ""
    if cbuffer_start >= 0:
        cbuffer_end = body.find("};", cbuffer_start)
        if cbuffer_end < 0:
            raise RuntimeError("HLSL parameter block is not closed")
        cbuffer_end += 2
        cbuffer = body[cbuffer_start:cbuffer_end].replace(
            "cbuffer ArtifactGraphParameters {",
            "cbuffer ArtifactGraphParameters : register(b0, space1) {",
            1,
        )
        body = body[:cbuffer_start] + body[cbuffer_end:]
    resources = []
    for item in interface["resourceBindings"]:
        resources.extend([
            f"Texture2D {item['textureSymbol']} : register(t{item['textureBinding']}, space1);",
            f"SamplerState {item['samplerSymbol']} : register(s{item['samplerBinding']}, space1);",
        ])
    output_type = {"Float": "float", "Vector2": "float2",
                   "Vector3": "float3", "Vector4": "float4"}.get(compiled["outputType"])
    if output_type is None:
        raise RuntimeError(f"unsupported HLSL fragment output type: {compiled['outputType']}")
    return "\n".join([
        cbuffer,
        *resources,
        "struct ArtifactGraphMaterialInput { float2 UV; };",
        f"{output_type} main(float2 artifactHostUV : TEXCOORD0) : SV_Target0 {{",
        "    ArtifactGraphMaterialInput In;",
        "    In.UV = artifactHostUV;",
        body.rstrip(),
        "}",
        "",
    ])


def parse_spirv_interface(disassembly: str) -> dict[str, Any]:
    names: dict[str, str] = {}
    descriptor_sets: dict[str, int] = {}
    descriptor_bindings: dict[str, int] = {}
    locations: dict[str, int] = {}
    member_names: dict[tuple[str, int], str] = {}
    member_offsets: dict[tuple[str, int], int] = {}
    types: dict[str, tuple[Any, ...]] = {}
    variables: list[tuple[str, str, str]] = []
    for line in disassembly.splitlines():
        match = re.match(r'\s*OpName\s+(%\S+)\s+"([^"]*)"', line)
        if match:
            names[match.group(1)] = match.group(2)
        match = re.match(r"\s*OpDecorate\s+(%\S+)\s+DescriptorSet\s+(\d+)", line)
        if match:
            descriptor_sets[match.group(1)] = int(match.group(2))
        match = re.match(r"\s*OpDecorate\s+(%\S+)\s+Binding\s+(\d+)", line)
        if match:
            descriptor_bindings[match.group(1)] = int(match.group(2))
        match = re.match(r"\s*OpDecorate\s+(%\S+)\s+Location\s+(\d+)", line)
        if match:
            locations[match.group(1)] = int(match.group(2))
        match = re.match(r'\s*OpMemberName\s+(%\S+)\s+(\d+)\s+"([^"]*)"', line)
        if match:
            member_names[(match.group(1), int(match.group(2)))] = match.group(3)
        match = re.match(r"\s*OpMemberDecorate\s+(%\S+)\s+(\d+)\s+Offset\s+(\d+)", line)
        if match:
            member_offsets[(match.group(1), int(match.group(2)))] = int(match.group(3))
        match = re.match(r"\s*(%\S+)\s*=\s*OpTypeFloat\s+(\d+)", line)
        if match:
            types[match.group(1)] = ("float", int(match.group(2)))
        match = re.match(r"\s*(%\S+)\s*=\s*OpTypeInt\s+(\d+)\s+(\d+)", line)
        if match:
            types[match.group(1)] = ("int", int(match.group(2)), int(match.group(3)))
        match = re.match(r"\s*(%\S+)\s*=\s*OpTypeVector\s+(%\S+)\s+(\d+)", line)
        if match:
            types[match.group(1)] = ("vector", match.group(2), int(match.group(3)))
        match = re.match(r"\s*(%\S+)\s*=\s*OpTypePointer\s+(\S+)\s+(%\S+)", line)
        if match:
            types[match.group(1)] = ("pointer", match.group(2), match.group(3))
        match = re.match(r"\s*(%\S+)\s*=\s*OpVariable\s+(%\S+)\s+(Input|Output)", line)
        if match:
            variables.append((match.group(1), match.group(2), match.group(3)))

    def type_shape(type_id: str, active: set[str] | None = None) -> str:
        if active is None:
            active = set()
        if type_id in active:
            return "recursive"
        active.add(type_id)
        shape = types.get(type_id)
        if shape is None:
            return "opaque"
        if shape[0] == "float":
            return f"float{shape[1]}"
        if shape[0] == "int":
            return f"int{shape[1]}s{shape[2]}"
        if shape[0] == "vector":
            return f"vec{shape[2]}<{type_shape(shape[1], active)}>"
        if shape[0] == "pointer":
            return type_shape(shape[2], active)
        return str(shape[0])

    descriptors = sorted(
        (names.get(identifier, identifier).lower(), descriptor_sets[identifier], descriptor_bindings[identifier])
        for identifier in descriptor_sets.keys() & descriptor_bindings.keys())
    offsets = sorted(
        (member_names[key], value) for key, value in member_offsets.items()
        if key in member_names)
    stage_locations = sorted(
        (storage, locations[identifier], type_shape(pointer_type))
        for identifier, pointer_type, storage in variables if identifier in locations)
    return {"descriptors": descriptors, "memberOffsets": offsets,
            "stageLocations": stage_locations}


def compile_candidates(root: Path, output_dir: Path, target_env: str,
                       glslang: Path, dxc: Path, spirv_val: Path,
                       spirv_dis: Path) -> dict[str, Any]:
    prototype_path = root / "experiments" / "ShaderGraphLab" / "dynamic_graph_prototype.py"
    registry_path = root / "experiments" / "ShaderGraphLab" / "registry.json"
    prototype = load_prototype(prototype_path)
    registry = prototype.load_registry(registry_path)
    output_dir.mkdir(parents=True, exist_ok=True)
    records = []

    for index, (document_path, candidate_path) in enumerate(discover_pairs(root)):
        document = json.loads(document_path.read_text(encoding="utf-8"))
        compiled = prototype.compile_document_from_data(document, registry)
        context_bindings = {}
        for item in compiled["contextBindings"]:
            if item["contextId"] != "artifact.material.surface.uv0":
                raise RuntimeError(f"unsupported host context in candidate {document_path.name}: {item['contextId']}")
            context_bindings[(item["contextId"], item["component"])] = (
                "artifactHostUV" if item["component"] == "xy" else f"artifactHostUV.{item['component']}"
            )
        source, interface = prototype.emit_glsl_from_typed_ir(
            compiled["typedIR"], compiled["parameterBlock"], context_bindings=context_bindings)
        if context_bindings:
            source = source.replace(
                "#version 450\n",
                "#version 450\nlayout(location = 0) in vec2 artifactHostUV;\n",
                1,
            )

        stem = f"{index:02d}_{document_path.stem}"
        source_path = output_dir / f"{stem}.frag"
        spirv_path = output_dir / f"{stem}.spv"
        hlsl_path = output_dir / f"{stem}.hlsl"
        hlsl_spirv_path = output_dir / f"{stem}_hlsl.spv"
        source_bytes = source.encode("utf-8")
        source_path.write_bytes(source_bytes)
        hlsl_source = build_hlsl_fragment(compiled, interface)
        hlsl_bytes = hlsl_source.encode("utf-8")
        hlsl_path.write_bytes(hlsl_bytes)
        compile_command = [str(glslang), "-V", "--target-env", target_env,
                           "-S", "frag", "-o", str(spirv_path), str(source_path)]
        compile_result = subprocess.run(compile_command, capture_output=True, text=True)
        spirv_result = None
        spirv_was_written = compile_result.returncode == 0 and spirv_path.is_file()
        if spirv_was_written:
            spirv_result = subprocess.run(
                [str(spirv_val), "--target-env", target_env, str(spirv_path)],
                capture_output=True,
                text=True,
            )
        dxc_command = [str(dxc), "-spirv", "-T", "ps_6_0", "-E", "main",
                       f"-fspv-target-env={target_env}", "-Fo", str(hlsl_spirv_path),
                       str(hlsl_path)]
        dxc_result = subprocess.run(dxc_command, capture_output=True, text=True)
        hlsl_spirv_was_written = dxc_result.returncode == 0 and hlsl_spirv_path.is_file()
        hlsl_spirv_result = None
        if hlsl_spirv_was_written:
            hlsl_spirv_result = subprocess.run(
                [str(spirv_val), "--target-env", target_env, str(hlsl_spirv_path)],
                capture_output=True,
                text=True,
            )
        glsl_disassembly = None
        hlsl_disassembly = None
        interface_parity = None
        if spirv_was_written and hlsl_spirv_was_written:
            glsl_dis_run = subprocess.run([str(spirv_dis), str(spirv_path)],
                                          capture_output=True, text=True)
            hlsl_dis_run = subprocess.run([str(spirv_dis), str(hlsl_spirv_path)],
                                          capture_output=True, text=True)
            if glsl_dis_run.returncode == 0 and hlsl_dis_run.returncode == 0:
                glsl_disassembly = glsl_dis_run.stdout
                hlsl_disassembly = hlsl_dis_run.stdout
                glsl_interface = parse_spirv_interface(glsl_disassembly)
                hlsl_interface = parse_spirv_interface(hlsl_disassembly)
                interface_parity = {
                    "descriptorsMatch": glsl_interface["descriptors"] == hlsl_interface["descriptors"],
                    "parameterOffsetsMatch": glsl_interface["memberOffsets"] == hlsl_interface["memberOffsets"],
                    "stageLocationsMatch": glsl_interface["stageLocations"] == hlsl_interface["stageLocations"],
                    "glsl": glsl_interface,
                    "hlsl": hlsl_interface,
                    "spirvDisExitCodes": [glsl_dis_run.returncode, hlsl_dis_run.returncode],
                }
        record = {
            "document": document_path.relative_to(root).as_posix(),
            "candidate": candidate_path.relative_to(root).as_posix(),
            "documentSha256": sha256(document_path.read_bytes()),
            "sourceFile": source_path.name,
            "sourceSha256": sha256(source_bytes),
            "hlslSourceFile": hlsl_path.name,
            "hlslSourceSha256": sha256(hlsl_bytes),
            "spirvFile": spirv_path.name if spirv_was_written else None,
            "spirvSha256": sha256(spirv_path.read_bytes()) if spirv_was_written else None,
            "hlslSpirvFile": hlsl_spirv_path.name if hlsl_spirv_was_written else None,
            "hlslSpirvSha256": sha256(hlsl_spirv_path.read_bytes()) if hlsl_spirv_was_written else None,
            "backendInterface": interface,
            "glslangCommand": compile_command,
            "glslangExitCode": compile_result.returncode,
            "glslangOutput": (compile_result.stdout + compile_result.stderr).strip(),
            "spirvValExitCode": spirv_result.returncode if spirv_result else None,
            "spirvValOutput": ((spirv_result.stdout + spirv_result.stderr).strip()
                               if spirv_result else None),
            "dxcCommand": dxc_command,
            "dxcExitCode": dxc_result.returncode,
            "dxcOutput": (dxc_result.stdout + dxc_result.stderr).strip(),
            "hlslSpirvValExitCode": hlsl_spirv_result.returncode if hlsl_spirv_result else None,
            "hlslSpirvValOutput": ((hlsl_spirv_result.stdout + hlsl_spirv_result.stderr).strip()
                                   if hlsl_spirv_result else None),
            "spirvInterfaceParity": interface_parity,
        }
        records.append(record)

    return {
        "schemaId": "artifact.shadergraph.glsl-compile-evidence",
        "schemaVersion": 1,
        "status": "passed" if records and all(
            item["glslangExitCode"] == 0 and item["spirvValExitCode"] == 0 and
            item["dxcExitCode"] == 0 and item["hlslSpirvValExitCode"] == 0 and
            item["spirvInterfaceParity"] is not None and
            item["spirvInterfaceParity"]["descriptorsMatch"] and
            item["spirvInterfaceParity"]["parameterOffsetsMatch"] and
            item["spirvInterfaceParity"]["stageLocationsMatch"]
            for item in records) else "failed",
        "targetEnvironment": target_env,
        "glslangValidator": str(glslang),
        "dxc": str(dxc),
        "spirvValidator": str(spirv_val),
        "spirvDisassembler": str(spirv_dis),
        "hostAdapter": "GLSL injects vec2 artifactHostUV at location 0; HLSL fragment wrapper maps TEXCOORD0 to In.UV. Both wrappers supply host context only.",
        "limitations": ["No Diligent application integration", "No GPU execution",
                        "Successful compilation does not establish cross-backend numeric or image parity",
                        "Descriptor binding proposal is not checked against renderer layouts"],
        "candidateCount": len(records),
        "passedCompileCount": sum(item["glslangExitCode"] == 0 for item in records),
        "passedSpirvValidationCount": sum(item["spirvValExitCode"] == 0 for item in records),
        "passedHlslCompileCount": sum(item["dxcExitCode"] == 0 for item in records),
        "passedHlslSpirvValidationCount": sum(item["hlslSpirvValExitCode"] == 0 for item in records),
        "passedSpirvInterfaceParityCount": sum(
            all(item["spirvInterfaceParity"][key] for key in
                ("descriptorsMatch", "parameterOffsetsMatch", "stageLocationsMatch"))
            if item["spirvInterfaceParity"] else False for item in records),
        "artifacts": records,
    }


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=root / "temp" / "shader-graph-glsl450-compile")
    parser.add_argument("--target-env", choices=("vulkan1.1", "vulkan1.2", "vulkan1.3"),
                        default="vulkan1.2")
    parser.add_argument("--glslang", type=Path,
                        default=Path(shutil.which("glslangValidator") or "glslangValidator"))
    parser.add_argument("--dxc", type=Path,
                        default=Path(shutil.which("dxc") or "dxc"))
    parser.add_argument("--spirv-val", type=Path,
                        default=Path(shutil.which("spirv-val") or "spirv-val"))
    parser.add_argument("--spirv-dis", type=Path,
                        default=Path(shutil.which("spirv-dis") or "spirv-dis"))
    args = parser.parse_args()
    summary = compile_candidates(root, args.output_dir.resolve(), args.target_env,
                                 args.glslang.resolve(), args.dxc.resolve(),
                                 args.spirv_val.resolve(), args.spirv_dis.resolve())
    summary_path = args.output_dir.resolve() / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8", newline="\n")
    print(f"status={summary['status']} candidates={summary['candidateCount']} "
          f"glslang={summary['passedCompileCount']} "
          f"dxc={summary['passedHlslCompileCount']} "
          f"spirv-val={summary['passedSpirvValidationCount']}/"
          f"{summary['passedHlslSpirvValidationCount']} "
          f"interface={summary['passedSpirvInterfaceParityCount']} summary={summary_path}")
    return 0 if summary["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
