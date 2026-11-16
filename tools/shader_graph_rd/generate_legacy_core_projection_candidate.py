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

    base_color_math_variants = {
        "add": {
            "legacyOperation": 0,
            "operationId": "artifact.rd.math.add.vector4.float32",
            "sourceSha256": hashlib.sha256(source_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": projection["includedNodeIds"],
            "document": projection["document"],
            "candidateHlslBodySha256": compiled["hlslBodySha256"],
            "candidateHlslBody": compiled["hlslBody"],
            "cpuVector4Golden": expected_vector4,
        },
    }
    for variant_id, legacy_op, operation_id, operator in (
        ("subtract", 1, "artifact.rd.math.subtract.vector4.float32", "-"),
        ("multiply", 2, "artifact.rd.math.multiply.vector4.float32", "*"),
        ("dividePositiveFloor", 3,
         "artifact.rd.math.divide.vector4.positive_floor.float32", "/"),
    ):
        variant_source = json.loads(source_json)
        next(node for node in variant_source["nodes"]
             if node["id"] == "math")["params"]["op"] = legacy_op
        variant_json = json.dumps(variant_source, ensure_ascii=False,
                                  separators=(",", ":"))
        variant_projection = prototype.project_legacy_core_material_input(
            variant_json, registry, target_node_id="surface",
            target_input_socket="BaseColor")
        variant_compiled = prototype.compile_document_from_data(
            variant_projection["document"], registry)
        expression = (
            "float4 n_math_result = (n_input_a_value / max(n_input_b_value, "
            "float4(1e-5f,1e-5f,1e-5f,1e-5f)));"
            if legacy_op == 3 else
            f"float4 n_math_result = (n_input_a_value {operator} n_input_b_value);"
        )
        if (variant_projection["document"]["nodes"][2]["operationId"] != operation_id or
                expression not in variant_compiled["hlslBody"]):
            raise RuntimeError(f"legacy BaseColor {variant_id} did not preserve typed Math semantics")
        if legacy_op == 1:
            expected = [f32(a - b) for a, b in zip(
                (0.25, -2.0, 3.0, 0.5), (0.5, 1.0, -4.0, 0.25))]
        elif legacy_op == 2:
            expected = [f32(a * b) for a, b in zip(
                (0.25, -2.0, 3.0, 0.5), (0.5, 1.0, -4.0, 0.25))]
        else:
            expected = [f32(a / f32(max(b, 1.0e-5))) for a, b in zip(
                (0.25, -2.0, 3.0, 0.5), (0.5, 1.0, -4.0, 0.25))]
        base_color_math_variants[variant_id] = {
            "legacyOperation": legacy_op,
            "operationId": operation_id,
            "sourceSha256": hashlib.sha256(variant_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": variant_projection["includedNodeIds"],
            "document": variant_projection["document"],
            "candidateHlslBodySha256": variant_compiled["hlslBodySha256"],
            "candidateHlslBody": variant_compiled["hlslBody"],
            "cpuVector4Golden": expected,
        }

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

    uv_source_path = fixture_root / "texcoord_math_roughness.json"
    uv_source_json = uv_source_path.read_text(encoding="utf-8")
    uv_projection = prototype.project_legacy_core_material_input(
        uv_source_json, registry, target_node_id="surface",
        target_input_socket="Roughness")
    uv_compiled = prototype.compile_document_from_data(
        uv_projection["document"], registry,
        hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
    expected_uv_math_hlsl = (
        "float n_bias_value = 0.125f;\n"
        "float n_gain_value = 0.5f;\n"
        "float n_uv_value = In.UV.x;\n"
        "float n_scale_result = (n_uv_value * n_gain_value);\n"
        "float n_roughness_result = (n_scale_result + n_bias_value);\n"
        "return n_roughness_result;\n")
    if (uv_projection["includedNodeIds"] != ["uv", "gain", "scale", "bias", "roughness"] or
            uv_compiled["outputType"] != "Float" or
            uv_compiled["hlslBody"] != expected_uv_math_hlsl):
        raise RuntimeError("legacy UV-driven Math chain did not project to dynamic Roughness")
    reordered_roughness_source = json.loads(uv_source_json)
    reordered_roughness_source["nodes"].reverse()
    reordered_roughness_source["links"].reverse()
    reordered_roughness_json = json.dumps(
        reordered_roughness_source, ensure_ascii=False, separators=(",", ":"))
    reordered_roughness_projection = prototype.project_legacy_core_material_input(
        reordered_roughness_json, registry,
        target_node_id="surface", target_input_socket="Roughness")
    reordered_roughness_compiled = prototype.compile_document_from_data(
        reordered_roughness_projection["document"], registry,
        hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
    if (reordered_roughness_projection["document"] != uv_projection["document"] or
            reordered_roughness_compiled["hlslBody"] != uv_compiled["hlslBody"] or
            reordered_roughness_projection["opaqueSourceJson"] != reordered_roughness_json):
        raise RuntimeError("legacy Roughness projection changed with source node/link storage order")
    uv_sample_values = (-0.25, 0.0, 0.125, 0.5, 1.0, 1.75)

    roughness_math_variants = {}
    for variant_id, legacy_op, gain, operation_id, expression in (
        ("subtract", 1, 0.5, "artifact.rd.math.subtract.float32",
         "float n_scale_result = (n_uv_value - n_gain_value);"),
        ("dividePositiveFloor", 3, -0.5,
         "artifact.rd.math.divide.positive_floor.float32",
         "float n_scale_result = (n_uv_value / max(n_gain_value, 1e-5f));"),
    ):
        variant_source = json.loads(uv_source_json)
        next(node for node in variant_source["nodes"]
             if node["id"] == "scale")["params"]["op"] = legacy_op
        next(node for node in variant_source["nodes"]
             if node["id"] == "gain")["params"]["value"] = gain
        variant_json = json.dumps(variant_source, ensure_ascii=False,
                                  separators=(",", ":"))
        variant_projection = prototype.project_legacy_core_material_input(
            variant_json, registry, target_node_id="surface",
            target_input_socket="Roughness")
        variant_compiled = prototype.compile_document_from_data(
            variant_projection["document"], registry,
            hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
        if (variant_projection["document"]["nodes"][2]["operationId"] != operation_id or
                expression not in variant_compiled["hlslBody"]):
            raise RuntimeError(f"legacy Roughness {variant_id} did not retain exact typed Math semantics")
        expected_values = []
        for value in uv_sample_values:
            uv = f32(value)
            if variant_id == "subtract":
                intermediate = f32(uv - f32(gain))
            else:
                intermediate = f32(uv / f32(max(gain, 1.0e-5)))
            expected_values.append(f32(intermediate + f32(0.125)))
        roughness_math_variants[variant_id] = {
            "legacyOperation": legacy_op,
            "operationId": operation_id,
            "sourceSha256": hashlib.sha256(variant_json.encode("utf-8")).hexdigest().upper(),
            "includedNodeIds": variant_projection["includedNodeIds"],
            "document": variant_projection["document"],
            "candidateHlslBodySha256": variant_compiled["hlslBodySha256"],
            "candidateHlslBody": variant_compiled["hlslBody"],
            "gain": gain,
            "numericCases": [
                {"uv0X": value, "expectedBinary32": expected}
                for value, expected in zip(uv_sample_values, expected_values)
            ],
        }

    roughness_rejection_cases = {}
    unsupported_roughness_source = json.loads(uv_source_json)
    next(node for node in unsupported_roughness_source["nodes"]
         if node["id"] == "scale")["params"]["op"] = 4
    roughness_rejection_cases["unsupportedRoughnessMathOperation"] = (
        unsupported_roughness_source, "legacy.node.operation.unsupported")
    clamped_roughness_source = json.loads(uv_source_json)
    next(node for node in clamped_roughness_source["nodes"]
         if node["id"] == "scale")["params"]["clamp"] = True
    roughness_rejection_cases["clampedRoughnessMath"] = (
        clamped_roughness_source, "legacy.node.operation.unsupported")
    duplicate_roughness_source = json.loads(uv_source_json)
    duplicate_roughness_source["links"].append(
        {"from": "bias", "out": "Value", "to": "scale", "in": "A"})
    roughness_rejection_cases["duplicateRoughnessMathWriter"] = (
        duplicate_roughness_source, "legacy.link.cardinality.invalid")
    cyclic_roughness_source = json.loads(uv_source_json)
    cyclic_roughness_source["links"] = [
        link for link in cyclic_roughness_source["links"]
        if not (link["to"] == "scale" and link["in"] == "A")]
    cyclic_roughness_source["links"].append(
        {"from": "roughness", "out": "Value", "to": "scale", "in": "A"})
    roughness_rejection_cases["roughnessMathCycle"] = (
        cyclic_roughness_source, "legacy.graph.cycle")
    for case_id, (invalid_source, expected_code) in roughness_rejection_cases.items():
        try:
            prototype.project_legacy_core_material_input(
                json.dumps(invalid_source), registry,
                target_node_id="surface", target_input_socket="Roughness")
        except prototype.ContractError as error:
            if error.code != expected_code:
                raise RuntimeError(f"{case_id} returned {error.code}, expected {expected_code}")
        else:
            raise RuntimeError(f"{case_id} was not rejected")

    unsupported_source = json.loads(source_json)
    math_node = next(node for node in unsupported_source["nodes"] if node["id"] == "math")
    math_node["params"]["op"] = 4
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
        "baseColorMathVariants": base_color_math_variants,
        "candidateHlslBodySha256": compiled["hlslBodySha256"],
        "candidateHlslBody": compiled["hlslBody"],
        "cpuVector4AddGolden": expected_vector4,
        "negativeProbe": {
            "unsupportedMathOperation": "legacy.node.operation.unsupported",
            "opaqueExtensionPreservedVerbatim": True,
            "nestedAddIncludedNodeIds": nested_projection["includedNodeIds"],
            "generatedConversionNodeCollision": "legacy.projection.generated_id.collision",
            "generatedDefaultNodeCollision": "legacy.projection.generated_id.collision",
            **{case_id: code for case_id, (_, code) in roughness_rejection_cases.items()},
            "roughnessNodeLinkOrderInvariant": True,
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
            "mathOperationVariants": roughness_math_variants,
            "candidateHlslBodySha256": uv_compiled["hlslBodySha256"],
            "candidateHlslBody": uv_compiled["hlslBody"],
            "outputType": uv_compiled["outputType"],
            "contextBindings": uv_compiled["contextBindings"],
            "coreSourceAdapter": uv_projection["sourceAdapter"],
            "graphFormula": "f32(f32(uv0.x * 0.5) + 0.125)",
            "numericCases": [
                {"uv0X": value,
                 "expectedBinary32": f32(f32(f32(value) * f32(0.5)) + f32(0.125))}
                for value in uv_sample_values
            ],
        },
        "limitations": [
            "read-only projection of the explicitly selected Principled BaseColor input or bounded UV-driven Roughness scalar subgraph only",
            "supports linked RGB constants, nested unclamped Math Add nodes, direct scalar Value-to-BaseColor conversion, and selected BaseColor/Roughness arithmetic projection lanes",
            "does not infer or perform any other legacy implicit numeric conversion",
            "unconnected Principled BaseColor is represented as an explicit candidate constant matching the Core source default",
            "selected BaseColor and Roughness Math nodes map Add/Subtract/Multiply/Divide into the matching typed operation; Divide preserves the Core positive denominator floor max(b, 1e-5); clamped Math is rejected",
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
