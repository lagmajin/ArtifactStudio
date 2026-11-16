#!/usr/bin/env python3
"""Generate a read-only typed projection from one legacy Core material input."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
from typing import Any


def load_prototype(path: Path) -> Any:
    spec = importlib.util.spec_from_file_location("legacy_core_projection_prototype", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load dynamic graph prototype: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    fixture_root = root / "tests/fixtures/shader-graph-rd"
    source_path = fixture_root / "math_add.json"
    registry_path = root / "experiments/ShaderGraphLab/registry.json"
    output_path = root / "experiments/ShaderGraphLab/core_math_add_projection_candidate.json"
    source_json = source_path.read_text(encoding="utf-8")
    prototype = load_prototype(root / "experiments/ShaderGraphLab/dynamic_graph_prototype.py")
    registry = prototype.load_registry(registry_path)
    projection = prototype.project_legacy_core_material_input(
        source_json, registry, target_node_id="surface", target_input_socket="BaseColor")
    compiled = prototype.compile_document_from_data(projection["document"], registry)

    expected_vector4 = [
        f32(0.25 + 0.5), f32(-2.0 + 1.0), f32(3.0 + -4.0), f32(0.5 + 0.25),
    ]
    required_hlsl = (
        "float4 n_input_a_value = float4(0.25f, -2.0f, 3.0f, 0.5f);",
        "float4 n_input_b_value = float4(0.5f, 1.0f, -4.0f, 0.25f);",
        "float4 n_math_result = (n_input_a_value + n_input_b_value);",
    )
    if projection["opaqueSourceJson"] != source_json:
        raise RuntimeError("projection did not preserve the original Core JSON text")
    if any(fragment not in compiled["hlslBody"] for fragment in required_hlsl):
        raise RuntimeError("typed candidate does not match the source-audited Core Add expression")

    nested_source = json.loads(source_json)
    nested_source["nodes"].append({"id": "input_c", "type": "RGB", "params": {
        "r": -0.125, "g": 0.25, "b": 0.5, "a": -0.25,
    }})
    nested_source["nodes"].append({"id": "math_outer", "type": "Math", "params": {
        "op": 0, "clamp": False,
    }})
    nested_source["links"] = [
        link for link in nested_source["links"]
        if not (link["to"] == "surface" and link["in"] == "BaseColor")
    ]
    nested_source["links"].extend((
        {"from": "math", "out": "Value", "to": "math_outer", "in": "A"},
        {"from": "input_c", "out": "Color", "to": "math_outer", "in": "B"},
        {"from": "math_outer", "out": "Value", "to": "surface", "in": "BaseColor"},
    ))
    nested_json = json.dumps(nested_source, ensure_ascii=False, separators=(",", ":"))
    nested_projection = prototype.project_legacy_core_material_input(
        nested_json, registry, target_node_id="surface", target_input_socket="BaseColor")
    nested_compiled = prototype.compile_document_from_data(nested_projection["document"], registry)
    nested_golden = [
        f32(f32(0.25 + 0.5) - 0.125),
        f32(f32(-2.0 + 1.0) + 0.25),
        f32(f32(3.0 + -4.0) + 0.5),
        f32(f32(0.5 + 0.25) - 0.25),
    ]
    if (nested_projection["includedNodeIds"] != ["input_a", "input_b", "math", "input_c", "math_outer"] or
            "float4 n_math_outer_result = (n_math_result + n_input_c_value);" not in nested_compiled["hlslBody"]):
        raise RuntimeError("nested legacy Add chain did not project in deterministic dependency order")

    scalar_source = {
        "version": 1,
        "kind": "artifact-material-graph",
        "nodes": [
            {"id": "scalar", "type": "Value", "params": {"value": 0.375}},
            {"id": "surface", "type": "Principled", "params": {}},
            {"id": "output", "type": "MaterialOutput", "params": {}},
        ],
        "links": [
            {"from": "scalar", "out": "Value", "to": "surface", "in": "BaseColor"},
            {"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"},
        ],
    }
    scalar_json = json.dumps(scalar_source, ensure_ascii=False, separators=(",", ":"))
    scalar_projection = prototype.project_legacy_core_material_input(
        scalar_json, registry, target_node_id="surface", target_input_socket="BaseColor")
    scalar_compiled = prototype.compile_document_from_data(scalar_projection["document"], registry)
    if (scalar_projection["document"]["output"] !=
            {"nodeId": "legacy_BaseColor_replicate", "socketId": "vector"} or
            "float4 n_legacy_BaseColor_replicate_vector = float4(n_scalar_value, n_scalar_value, n_scalar_value, 1.0f);" not in
            scalar_compiled["hlslBody"]):
        raise RuntimeError("legacy scalar-to-BaseColor conversion did not lower explicitly")
    collision_source = json.loads(scalar_json)
    collision_source["nodes"].append({
        "id": "legacy_BaseColor_replicate", "type": "Value", "params": {"value": 1.0},
    })
    try:
        prototype.project_legacy_core_material_input(
            json.dumps(collision_source), registry,
            target_node_id="surface", target_input_socket="BaseColor")
    except prototype.ContractError as error:
        if error.code != "legacy.projection.generated_id.collision":
            raise RuntimeError(f"generated conversion-node collision returned wrong diagnostic: {error.code}")
    else:
        raise RuntimeError("legacy generated conversion-node ID collision was not rejected")

    default_source = {
        "version": 1,
        "kind": "artifact-material-graph",
        "nodes": [
            {"id": "surface", "type": "Principled", "params": {}},
            {"id": "output", "type": "MaterialOutput", "params": {}},
        ],
        "links": [{"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"}],
    }
    default_json = json.dumps(default_source, ensure_ascii=False, separators=(",", ":"))
    default_projection = prototype.project_legacy_core_material_input(
        default_json, registry, target_node_id="surface", target_input_socket="BaseColor")
    default_compiled = prototype.compile_document_from_data(default_projection["document"], registry)
    if (default_projection["includedNodeIds"] or
            default_projection["document"]["nodes"][0]["parameters"].get("value") != [0.8, 0.8, 0.8, 1.0] or
            "float4 n_legacy_BaseColor_default_value = float4(0.800000012f, 0.800000012f, 0.800000012f, 1.0f);" not in
            default_compiled["hlslBody"]):
        raise RuntimeError("unconnected legacy BaseColor default did not project explicitly")
    default_collision_source = json.loads(default_json)
    default_collision_source["nodes"].append({
        "id": "legacy_BaseColor_default", "type": "RGB", "params": {},
    })
    try:
        prototype.project_legacy_core_material_input(
            json.dumps(default_collision_source), registry,
            target_node_id="surface", target_input_socket="BaseColor")
    except prototype.ContractError as error:
        if error.code != "legacy.projection.generated_id.collision":
            raise RuntimeError(f"generated default-node collision returned wrong diagnostic: {error.code}")
    else:
        raise RuntimeError("legacy generated default-node ID collision was not rejected")

    uv_source_path = fixture_root / "texcoord_roughness.json"
    uv_source_json = uv_source_path.read_text(encoding="utf-8")
    uv_projection = prototype.project_legacy_core_material_input(
        uv_source_json, registry, target_node_id="surface",
        target_input_socket="Roughness")
    uv_compiled = prototype.compile_document_from_data(
        uv_projection["document"], registry,
        hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
    if (uv_projection["includedNodeIds"] != ["uv"] or
            uv_compiled["outputType"] != "Float" or
            "float n_uv_value = In.UV.x;" not in uv_compiled["hlslBody"]):
        raise RuntimeError("legacy TexCoord.UV did not project to dynamic UV0.x for Roughness")
    uv_sample_values = (-0.25, 0.0, 0.125, 0.5, 1.0, 1.75)

    unsupported_source = json.loads(source_json)
    math_node = next(node for node in unsupported_source["nodes"] if node["id"] == "math")
    math_node["params"]["op"] = 1
    try:
        prototype.project_legacy_core_material_input(
            json.dumps(unsupported_source), registry,
            target_node_id="surface", target_input_socket="BaseColor")
    except prototype.ContractError as error:
        if error.code != "legacy.node.operation.unsupported":
            raise RuntimeError(f"unsupported operation returned the wrong diagnostic: {error.code}")
    else:
        raise RuntimeError("unsupported legacy Math operation was not rejected")

    extension_source = json.loads(source_json)
    extension_source["vendorExtension"] = {"retain": [1, {"opaque": True}]}
    extension_json = json.dumps(extension_source, ensure_ascii=False, separators=(",", ":"))
    extension_projection = prototype.project_legacy_core_material_input(
        extension_json, registry, target_node_id="surface", target_input_socket="BaseColor")
    if (extension_projection["opaqueSourceJson"] != extension_json or
            extension_projection["document"] != projection["document"]):
        raise RuntimeError("opaque source extension was lost or changed projected semantics")

    candidate = {
        "schemaId": "artifact.shadergraph.legacy-core-read-only-projection-candidate",
        "schemaVersion": 1,
        "status": "python-prototype-source-audited-not-core-runtime",
        "sourceFixture": source_path.relative_to(root).as_posix(),
        "sourceSha256": hashlib.sha256(source_json.encode("utf-8")).hexdigest().upper(),
        "selectedLegacyInput": projection["selectedLegacyInput"],
        "includedNodeIds": projection["includedNodeIds"],
        "excludedNodeIds": projection["excludedNodeIds"],
        "layoutProvenance": projection["layoutProvenance"],
        "document": projection["document"],
        "candidateHlslBodySha256": compiled["hlslBodySha256"],
        "candidateHlslBody": compiled["hlslBody"],
        "cpuVector4AddGolden": expected_vector4,
        "negativeProbe": {
            "unsupportedMathOperation": "legacy.node.operation.unsupported",
            "opaqueExtensionPreservedVerbatim": True,
            "nestedAddIncludedNodeIds": nested_projection["includedNodeIds"],
            "generatedConversionNodeCollision": "legacy.projection.generated_id.collision",
            "generatedDefaultNodeCollision": "legacy.projection.generated_id.collision",
        },
        "nestedAddProjection": {
            "sourceSha256": hashlib.sha256(nested_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": nested_projection["includedNodeIds"],
            "document": nested_projection["document"],
            "candidateHlslBodySha256": nested_compiled["hlslBodySha256"],
            "candidateHlslBody": nested_compiled["hlslBody"],
            "cpuVector4AddGolden": nested_golden,
        },
        "scalarBaseColorConversionProbe": {
            "sourceSha256": hashlib.sha256(scalar_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": scalar_projection["includedNodeIds"],
            "document": scalar_projection["document"],
            "candidateHlslBodySha256": scalar_compiled["hlslBodySha256"],
            "candidateHlslBody": scalar_compiled["hlslBody"],
            "cpuVector4Golden": [0.375, 0.375, 0.375, 1.0],
            "conversionOperationId": "artifact.convert.scalar.replicate.vector4.rgb_alpha_one",
        },
        "unconnectedBaseColorDefaultProbe": {
            "sourceSha256": hashlib.sha256(default_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": default_projection["includedNodeIds"],
            "excludedNodeIds": default_projection["excludedNodeIds"],
            "document": default_projection["document"],
            "candidateHlslBodySha256": default_compiled["hlslBodySha256"],
            "candidateHlslBody": default_compiled["hlslBody"],
            "cpuVector4Golden": [0.8, 0.8, 0.8, 1.0],
        },
        "dynamicRoughnessUvProjection": {
            "sourceFixture": uv_source_path.relative_to(root).as_posix(),
            "sourceSha256": hashlib.sha256(uv_source_json.encode("utf-8")).hexdigest().upper(),
            "selectedLegacyInput": uv_projection["selectedLegacyInput"],
            "includedNodeIds": uv_projection["includedNodeIds"],
            "excludedNodeIds": uv_projection["excludedNodeIds"],
            "layoutProvenance": uv_projection["layoutProvenance"],
            "document": uv_projection["document"],
            "candidateHlslBodySha256": uv_compiled["hlslBodySha256"],
            "candidateHlslBody": uv_compiled["hlslBody"],
            "outputType": uv_compiled["outputType"],
            "contextBindings": uv_compiled["contextBindings"],
            "coreSourceAdapter": uv_projection["sourceAdapter"],
            "numericCases": [
                {"uv0X": value, "expectedBinary32": f32(value)}
                for value in uv_sample_values
            ],
        },
        "limitations": [
            "read-only projection of the explicitly selected Principled BaseColor or direct UV-driven Roughness input only",
            "supports linked RGB constants, nested unclamped Math Add nodes, and a direct scalar Value-to-BaseColor explicit conversion",
            "does not infer or perform any other legacy implicit numeric conversion",
            "unconnected Principled BaseColor is represented as an explicit candidate constant matching the Core source default",
            "Roughness accepts only direct TexCoord.UV and projects the source-audited Vector2.x conversion to UV0.x",
            "Principled/MaterialOutput remain outside the projected document",
            "source JSON text is retained by the adapter result and is never rewritten",
            "no Core API, project persistence, shader compiler, renderer, or GPU path is invoked",
        ],
    }
    output_path.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"wrote {output_path.relative_to(root).as_posix()}")
    print(f"sourceSha256={candidate['sourceSha256']}")
    print(f"candidateHlslBodySha256={candidate['candidateHlslBodySha256']}")
    print(f"cpuVector4AddGolden={expected_vector4}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
