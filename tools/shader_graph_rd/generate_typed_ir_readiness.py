#!/usr/bin/env python3
"""Report whether selected legacy fixtures can form a complete typed IR."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct


FIXTURES = (
    "principled_default.json",
    "surface_numeric_semantics.json",
    "channel_probe_basecolor.json",
    "skipped_type_mismatch_link_negative.json",
    "duplicate_input_negative.json",
)
ROOT_BOUNDARY_KINDS = {"MaterialOutput"}
LEGACY_SOCKET_MAPS = {
    "Principled": {
        "inputs": {
            "base_color": "BaseColor", "metallic": "Metallic", "roughness": "Roughness",
            "emission_color": "EmissionColor", "emission_strength": "EmissionStrength",
            "alpha": "Alpha", "normal": "Normal", "occlusion": "Occlusion",
        },
        "outputs": {"surface": "BSDF"},
        "inputTypes": {"BaseColor": "Vector4", "Metallic": "Float", "Roughness": "Float", "EmissionColor": "Vector4", "EmissionStrength": "Float", "Alpha": "Float", "Normal": "Vector3", "Occlusion": "Float"},
        "outputTypes": {"BSDF": "Shader"},
    },
    "Value": {"inputs": {}, "outputs": {"value": "Value"}, "inputTypes": {}, "outputTypes": {"Value": "Float"}},
    "RGB": {"inputs": {}, "outputs": {"color": "Color"}, "inputTypes": {}, "outputTypes": {"Color": "Vector4"}},
    "TexCoord": {"inputs": {}, "outputs": {"uv": "UV"}, "inputTypes": {}, "outputTypes": {"UV": "Vector2"}},
    "ImageTexture": {"inputs": {"uv": "UV"}, "outputs": {"color": "Color", "alpha": "Alpha"}, "inputTypes": {"UV": "Vector2"}, "outputTypes": {"Color": "Vector4", "Alpha": "Float"}},
}
LEGACY_BOUNDARY_TYPES = {"MaterialOutput": {"inputs": {"Surface": "Shader"}, "outputs": {}}}
LEGACY_DEFAULT_VALUES = {
    "artifact.surface.principled.fields": {
        "base_color": [0.8, 0.8, 0.8, 1.0], "metallic": 0.0, "roughness": 0.5,
        "emission_color": [0.0, 0.0, 0.0, 1.0], "emission_strength": 1.0,
        "alpha": 1.0, "normal": {"hostContext": "normalSample", "coordinateSpace": "unresolved"},
        "occlusion": {"hostContext": "occlusionSample"},
    },
    "artifact.math.add.vector4": {"a": [0.5, 0.5, 0.5, 0.5], "b": [0.0, 0.0, 0.0, 0.0]},
    "artifact.texture.material_slot_sample.color4": {"uv": {"hostContext": "In.UV", "uvSetAndTransform": "unresolved"}},
}


def binary32(value: object) -> object:
    if isinstance(value, bool):
        return value
    if isinstance(value, float):
        if not (float("-inf") < value < float("inf")):
            raise ValueError("non-finite numeric value")
        rounded = struct.unpack("<f", struct.pack("<f", value))[0]
        return 0.0 if rounded == 0.0 else rounded
    if isinstance(value, list):
        return [binary32(item) for item in value]
    if isinstance(value, dict):
        return {key: binary32(item) for key, item in value.items()}
    return value


def type_adapter(legacy_type: str | None, candidate_type: str | None) -> str:
    if legacy_type == candidate_type:
        return "identity"
    if (legacy_type, candidate_type) == ("Vector4", "Color4"):
        return "component-wise identity Vector4-to-Color4 semantic relabel; no swizzle or conversion"
    if (legacy_type, candidate_type) == ("Shader", "SurfaceFields"):
        return "renderer-specific Shader bundle to SurfaceFields candidate adapter; field parity remains unverified"
    return "unresolved-type-adapter"


def build_candidate_projection(candidates: list[dict], root_boundary: dict, records: dict) -> tuple[dict | None, list[str]]:
    by_node = {candidate["nodeId"]: candidate for candidate in candidates}
    operation_records = {candidate["operationId"]: records[candidate["operationId"]] for candidate in candidates}
    errors = []
    dependencies: dict[str, set[str]] = {node_id: set() for node_id in by_node}
    projected = {}
    for node_id, candidate in by_node.items():
        record = operation_records[candidate["operationId"]]
        typed_inputs = []
        for declaration, binding_info in zip(record.get("inputs", []), candidate.get("inputs", [])):
            binding = binding_info.get("binding", {})
            state = binding.get("status")
            if state == "linked-source-identified-pin-types-recorded":
                source_id = binding.get("sourceNodeId")
                source_candidate = by_node.get(source_id)
                source_kind = source_candidate.get("legacyKind") if source_candidate else None
                socket_map = LEGACY_SOCKET_MAPS.get(source_kind, {}).get("outputs", {})
                output_id = next((key for key, label in socket_map.items() if label == binding.get("sourceOutputSocket")), None)
                source_record = operation_records.get(source_candidate.get("operationId")) if source_candidate else None
                source_output = next((item for item in source_record.get("outputs", []) if item.get("outputId") == output_id), None) if source_record else None
                if not source_output:
                    errors.append(f"{node_id}.{declaration['inputId']}: linked source output has no candidate record")
                    continue
                if source_output.get("valueType") != declaration.get("valueType"):
                    errors.append(f"{node_id}.{declaration['inputId']}: candidate edge type mismatch {source_output.get('valueType')} -> {declaration.get('valueType')}")
                    continue
                source_legacy_type = binding.get("sourceOutputLegacyPinType")
                target_legacy_type = binding_info.get("legacyPinType")
                target_adapter = binding_info.get("typeAdapter")
                source_adapter = next((item.get("typeAdapter") for item in source_candidate.get("outputs", []) if item.get("outputId") == output_id), None)
                if (not source_legacy_type or source_legacy_type != target_legacy_type or
                        source_adapter == "unresolved-type-adapter" or target_adapter == "unresolved-type-adapter"):
                    errors.append(f"{node_id}.{declaration['inputId']}: legacy pin type or adapter is unresolved ({source_legacy_type} -> {target_legacy_type})")
                    continue
                dependencies[node_id].add(source_id)
                typed_inputs.append({"inputId": declaration["inputId"], "valueType": declaration["valueType"], "legacyPinType": target_legacy_type, "typeAdapter": target_adapter, "binding": {"kind": "edge", "sourceNodeId": source_id, "sourceOutputId": output_id, "sourceLegacyPinType": source_legacy_type}})
            elif state == "unconnected-candidate-default":
                value = binding.get("candidateValue")
                if value is None:
                    errors.append(f"{node_id}.{declaration['inputId']}: candidate default is unresolved")
                    continue
                if binding_info.get("typeAdapter") == "unresolved-type-adapter":
                    errors.append(f"{node_id}.{declaration['inputId']}: legacy pin type adapter is unresolved")
                    continue
                typed_inputs.append({"inputId": declaration["inputId"], "valueType": declaration["valueType"], "legacyPinType": binding_info.get("legacyPinType"), "typeAdapter": binding_info.get("typeAdapter"), "binding": {"kind": "candidate-default", "value": binary32(value)}})
            else:
                errors.append(f"{node_id}.{declaration['inputId']}: unsupported binding state {state!r}")
        params = []
        for parameter in candidate.get("parameters", []):
            if parameter.get("resolved") is False:
                errors.append(f"{node_id}.{parameter.get('parameterId')}: legacy parameter value is unresolved")
                continue
            params.append({"parameterId": parameter["parameterId"], "value": binary32(parameter.get("value"))})
        resource_dependencies = []
        for dependency in record.get("resourceDependencies", []):
            symbolic_value = next((item.get("value") for item in params if item.get("parameterId") == dependency.get("resourceKey")), None)
            resource_dependencies.append({
                "resourceKey": dependency.get("resourceKey"),
                "symbolicValue": symbolic_value,
                "status": "symbolic-slot-only-asset-identity-revision-and-sampler-unresolved",
            })
        projected[node_id] = {
            "nodeId": node_id,
            "operationId": candidate["operationId"],
            "semanticVersion": record["semanticVersion"],
            "inputs": typed_inputs,
            "outputs": [{"outputId": item["outputId"], "valueType": item["valueType"], "legacySocket": item["legacySocket"], "legacyPinType": item["legacyPinType"], "typeAdapter": item["typeAdapter"], "stableSocketId": item["stableSocketId"]} for item in candidate.get("outputs", [])],
            "parameters": params,
            "resourceDependencies": resource_dependencies,
        }
    root_links = root_boundary.get("surfaceInputLinks", [])
    if len(root_links) != 1:
        errors.append("selected MaterialOutput.Surface must have exactly one source for candidate projection")
        root = None
    else:
        link = root_links[0]
        source = by_node.get(link.get("sourceNodeId"))
        source_kind = source.get("legacyKind") if source else None
        socket_map = LEGACY_SOCKET_MAPS.get(source_kind, {}).get("outputs", {})
        output_id = next((key for key, label in socket_map.items() if label == link.get("sourceOutputSocket")), None)
        source_record = operation_records.get(source.get("operationId")) if source else None
        output = next((item for item in source_record.get("outputs", []) if item.get("outputId") == output_id), None) if source_record else None
        if not output or output.get("valueType") != "SurfaceFields":
            errors.append("selected MaterialOutput.Surface source does not resolve to candidate SurfaceFields")
            root = None
        else:
            source_legacy_type = link.get("sourceOutputLegacyPinType")
            input_legacy_type = link.get("inputLegacyPinType")
            if source_legacy_type != input_legacy_type or link.get("typeAdapter") == "unresolved-type-adapter":
                errors.append("MaterialOutput.Surface legacy pin type or candidate boundary adapter is unresolved")
                root = None
            else:
                root = {"nodeId": source["nodeId"], "outputId": output_id, "valueType": "SurfaceFields", "sourceLegacyPinType": source_legacy_type, "boundaryInputLegacyPinType": input_legacy_type, "typeAdapter": link.get("typeAdapter")}
    ordered_ids = []
    remaining = {node_id: set(values) for node_id, values in dependencies.items()}
    while remaining:
        ready = sorted((node_id for node_id, deps in remaining.items() if not deps), key=lambda value: value.encode("utf-8"))
        if not ready:
            errors.append("candidate typed operation dependencies contain a cycle")
            break
        for node_id in ready:
            ordered_ids.append(node_id)
            del remaining[node_id]
        for deps in remaining.values():
            deps.difference_update(ready)
    if errors or root is None:
        return None, errors
    candidate_ir = {
        "kind": "artifact-typed-ir-projection-candidate",
        "status": "proposal-fixture-projection-not-core-ir",
        "operations": [projected[node_id] for node_id in ordered_ids],
        "root": root,
    }
    canonical = json.dumps(candidate_ir, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    candidate_ir["candidateSerializationSha256"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest().upper()
    return candidate_ir, []


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def operation_for_node(node: dict, families: dict, parameter_mapping: dict) -> tuple[str | None, str]:
    kind = node.get("type")
    family = families.get(kind)
    if not isinstance(family, dict):
        return None, "legacy node family has no declared coverage record"
    operation_ids = family.get("operationIds", [])
    if not operation_ids:
        return None, "node family has no typed operation record"
    if kind == "Math":
        enum_map = parameter_mapping.get("Math", {}).get("op", {}).get("values", {})
        ordinal = str(node.get("params", {}).get("op", 0))
        operation_name = enum_map.get(ordinal)
        if operation_name == "add":
            return "artifact.math.add.vector4", "exact representative operation record"
        return None, f"Math operation {operation_name or ordinal!r} has no exact typed operation record"
    if kind == "MixRGB":
        enum_map = parameter_mapping.get("MixRGB", {}).get("blend", {}).get("values", {})
        ordinal = str(node.get("params", {}).get("blend", 0))
        operation_name = enum_map.get(ordinal)
        if operation_name == "overlay":
            return "artifact.color.mix_rgb.overlay", "exact representative operation record"
        return None, f"MixRGB mode {operation_name or ordinal!r} has no exact typed operation record"
    if len(operation_ids) == 1:
        return operation_ids[0], "representative typed operation record"
    return None, "node family has multiple records but no deterministic legacy variant mapping"


def active_nodes(payload: dict, output_node: dict) -> tuple[list[dict], list[str]]:
    nodes = {node["id"]: node for node in payload.get("nodes", []) if isinstance(node, dict) and isinstance(node.get("id"), str)}
    incoming: dict[str, list[str]] = {}
    for link in payload.get("links", []):
        if isinstance(link, dict) and isinstance(link.get("from"), str) and isinstance(link.get("to"), str):
            incoming.setdefault(link["to"], []).append(link["from"])
    visited: set[str] = set()
    visiting: set[str] = set()
    cycles: set[str] = set()

    def visit(node_id: str) -> None:
        if node_id in visiting:
            cycles.add(node_id)
            return
        if node_id in visited or node_id not in nodes:
            return
        visiting.add(node_id)
        for source_id in incoming.get(node_id, []):
            visit(source_id)
        visiting.remove(node_id)
        visited.add(node_id)

    visit(output_node["id"])
    return [nodes[node_id] for node_id in sorted(visited, key=lambda item: item.encode("utf-8"))], sorted(cycles)


def project_candidate(node: dict, operation_id: str, classification: str, payload: dict, records: dict, stable_socket_ids: dict) -> dict:
    kind = node.get("type")
    mapping = LEGACY_SOCKET_MAPS.get(kind, {"inputs": {}, "outputs": {}})
    record = records[operation_id]
    nodes_by_id = {candidate.get("id"): candidate for candidate in payload.get("nodes", []) if isinstance(candidate, dict)}
    incoming = [link for link in payload.get("links", []) if link.get("to") == node["id"]]
    inputs = []
    for declaration in record.get("inputs", []):
        input_id = declaration["inputId"]
        legacy_socket = mapping["inputs"].get(input_id)
        matches = [link for link in incoming if link.get("in") == legacy_socket]
        if len(matches) == 1:
            link = matches[0]
            binding = {
                "status": "linked-source-identified-pin-types-recorded",
                "sourceNodeId": link.get("from"),
                "sourceOutputSocket": link.get("out"),
                "sourceOutputLegacyPinType": LEGACY_SOCKET_MAPS.get(nodes_by_id.get(link.get("from"), {}).get("type"), {}).get("outputTypes", {}).get(link.get("out")),
            }
            source_node = nodes_by_id.get(link.get("from"), {})
            source_label = f"{source_node.get('type')}.{link.get('out')}"
            binding["sourceOutputStableSocketId"] = stable_socket_ids.get(source_label)
        elif len(matches) > 1:
            binding = {"status": "ambiguous-multiple-source-links", "linkCount": len(matches)}
        else:
            binding = {
                "status": "unconnected-candidate-default",
                "candidateValue": LEGACY_DEFAULT_VALUES.get(operation_id, {}).get(input_id),
                "declaredPolicy": declaration.get("defaultPolicy"),
            }
        legacy_type = mapping.get("inputTypes", {}).get(legacy_socket)
        candidate_type = declaration.get("valueType")
        adapter = type_adapter(legacy_type, candidate_type)
        inputs.append({"inputId": input_id, "legacySocket": legacy_socket, "legacyPinType": legacy_type, "valueType": candidate_type, "typeAdapter": adapter, "stableSocketId": stable_socket_ids.get(f"{kind}.{legacy_socket}"), "binding": binding})
    parameters = []
    params = node.get("params", {})
    if operation_id == "artifact.constant.scalar.float32":
        parameters = [{"parameterId": "value", "value": params.get("value", 0.0), "source": "node-owned parameter or constructor default"}]
    elif operation_id == "artifact.constant.color4.float32":
        parameters = [{"parameterId": "rgba", "value": [params.get(key, default) for key, default in (("r", 0.5), ("g", 0.5), ("b", 0.5), ("a", 1.0))], "source": "node-owned channel parameters or constructor defaults"}]
    elif operation_id == "artifact.texture.material_slot_sample.color4":
        slot = str(params.get("slot", 0))
        slot_values = {"0": "base_color", "1": "opacity", "2": "emission", "3": "metallic_roughness", "4": "normal", "5": "occlusion"}
        parameters = [{"parameterId": "material_slot", "value": slot_values.get(slot), "legacyOrdinal": params.get("slot", 0), "source": "legacy enum mapping", "resolved": slot in slot_values}]
    return {
        "nodeId": node["id"], "legacyKind": kind, "operationId": operation_id,
        "classification": classification, "inputs": inputs,
        "outputs": [{"outputId": item["outputId"], "valueType": item["valueType"], "legacySocket": mapping["outputs"].get(item["outputId"]), "legacyPinType": mapping.get("outputTypes", {}).get(mapping["outputs"].get(item["outputId"])), "typeAdapter": type_adapter(mapping.get("outputTypes", {}).get(mapping["outputs"].get(item["outputId"])), item["valueType"]), "stableSocketId": stable_socket_ids.get(f"{kind}.{mapping['outputs'].get(item['outputId'])}")} for item in record.get("outputs", [])],
        "parameters": parameters,
        "legacyParameters": node.get("params", {}),
    }


def project_fixture_payload(payload: dict, families: dict, records: dict, parameter_mapping: dict, stable_socket_ids: dict) -> dict:
    outputs = [node for node in payload.get("nodes", []) if isinstance(node, dict) and node.get("type") == "MaterialOutput"]
    if len(outputs) != 1:
        return {"status": "output-selection-invalid", "diagnosticCodes": ["output.ambiguous" if outputs else "output.missing"], "activeNodeIds": [], "candidateOperations": [], "candidateTypedIrProjection": None, "projectionDiagnostics": [], "unsupportedNodes": [], "cycleNodeIds": []}
    reachable, cycles = active_nodes(payload, outputs[0])
    candidates = []
    unsupported = []
    for node in reachable:
        if node.get("type") in ROOT_BOUNDARY_KINDS:
            continue
        operation_id, reason = operation_for_node(node, families, parameter_mapping)
        if operation_id is None:
            unsupported.append({"nodeId": node["id"], "legacyKind": node.get("type"), "reason": reason})
        else:
            candidates.append(project_candidate(node, operation_id, reason, payload, records, stable_socket_ids))
    root_boundary = {
        "nodeId": outputs[0]["id"], "legacyKind": outputs[0].get("type"),
        "classification": "selected-output-boundary-not-a-typed-operation",
        "surfaceInputLinks": [
            {"sourceNodeId": link.get("from"), "sourceOutputSocket": link.get("out"), "sourceOutputStableSocketId": stable_socket_ids.get(f"{next((node.get('type') for node in payload.get('nodes', []) if node.get('id') == link.get('from')), '')}.{link.get('out')}"), "inputStableSocketId": stable_socket_ids.get(f"MaterialOutput.{link.get('in')}"), "sourceOutputLegacyPinType": LEGACY_SOCKET_MAPS.get(next((node.get('type') for node in payload.get('nodes', []) if node.get('id') == link.get('from')), ''), {}).get("outputTypes", {}).get(link.get("out")), "inputLegacyPinType": LEGACY_BOUNDARY_TYPES["MaterialOutput"]["inputs"].get(link.get("in")), "typeAdapter": type_adapter(LEGACY_SOCKET_MAPS.get(next((node.get('type') for node in payload.get('nodes', []) if node.get('id') == link.get('from')), ''), {}).get("outputTypes", {}).get(link.get("out")), "SurfaceFields"), "status": "identified-pin-types-recorded"}
            for link in payload.get("links", []) if link.get("to") == outputs[0]["id"] and link.get("in") == "Surface"
        ],
    }
    projection, projection_errors = build_candidate_projection(candidates, root_boundary, records)
    diagnostics = []
    if cycles:
        diagnostics.append("graph.cycle")
    if unsupported:
        diagnostics.append("lowering.operation.unsupported")
    if projection_errors:
        diagnostics.append("lowering.projection.candidate-invalid")
    return {
        "status": "blocked-no-partial-ir" if diagnostics else "candidate-projection-unverified",
        "rootBoundary": root_boundary,
        "diagnosticCodes": diagnostics,
        "activeNodeIds": [node["id"] for node in reachable],
        "candidateOperations": candidates,
        "candidateTypedIrProjection": projection,
        "projectionDiagnostics": projection_errors,
        "unsupportedNodes": unsupported,
        "cycleNodeIds": cycles,
    }


def build_report(root: Path) -> dict:
    fixture_root = root / "tests/fixtures/shader-graph-rd"
    manifest = read_json(fixture_root / "evaluation_manifest.json")
    schema = manifest["coreTypedOperationSchemaProposal"]
    families = schema["legacyNodeCoverage"]["families"]
    records = {record["operationId"]: record for record in schema["exampleOperationRecords"]}
    parameter_mapping = manifest["coreDocumentSchemaProposal"]["legacyParameterMapping"]
    stable_socket_ids = manifest["coreDocumentSchemaProposal"].get("socketIdentityProposal", {}).get("legacyLabelToStableId", {})
    explicit_conversion = build_explicit_conversion_report(root, manifest, records)
    vector_truncation = build_vector_truncation_report(root, manifest, records)
    reports = []
    for fixture_name in FIXTURES:
        fixture_path = fixture_root / fixture_name
        payload = read_json(fixture_path)
        projected = project_fixture_payload(payload, families, records, parameter_mapping, stable_socket_ids)
        stability_cases = []
        candidate_projection = projected.get("candidateTypedIrProjection")
        if candidate_projection is not None:
            variants = []
            reversed_storage = json.loads(json.dumps(payload))
            reversed_storage["nodes"] = list(reversed(reversed_storage.get("nodes", [])))
            reversed_storage["links"] = list(reversed(reversed_storage.get("links", [])))
            variants.append(("reverse-node-and-link-storage-order", reversed_storage, True))
            layout_only = json.loads(json.dumps(payload))
            for index, node in enumerate(layout_only.get("nodes", [])):
                node["x"] = 97.0 + index
                node["y"] = -31.0 - index
            variants.append(("change-layout-only", layout_only, True))
            renamed_id = candidate_projection["root"]["nodeId"]
            renamed = json.loads(json.dumps(payload))
            for node in renamed.get("nodes", []):
                if node.get("id") == renamed_id:
                    node["id"] = renamed_id + "_identity_probe"
            for link in renamed.get("links", []):
                if link.get("from") == renamed_id:
                    link["from"] = renamed_id + "_identity_probe"
                if link.get("to") == renamed_id:
                    link["to"] = renamed_id + "_identity_probe"
            variants.append(("change-stable-node-id", renamed, False))
            for case_id, variant, expected_match in variants:
                variant_projection = project_fixture_payload(variant, families, records, parameter_mapping, stable_socket_ids).get("candidateTypedIrProjection")
                stability_cases.append({"id": case_id, "expectedMatchesBaseline": expected_match, "matchesBaseline": variant_projection == candidate_projection})
        if stability_cases and not all(case["matchesBaseline"] == case["expectedMatchesBaseline"] for case in stability_cases):
            projected["status"] = "blocked-no-partial-ir"
            projected["diagnosticCodes"].append("lowering.projection.order-dependent")
            projected["candidateTypedIrProjection"] = None
            projected["projectionDiagnostics"].append("candidate projection changed under storage-order or layout-only permutation")
        reports.append({
            "fixture": fixture_name,
            "fixtureSha256": hashlib.sha256(fixture_path.read_bytes()).hexdigest().upper(),
            **projected,
            "stabilityCases": stability_cases,
            "irProduced": False,
        })
    return {
        "schemaVersion": 1,
        "kind": "artifact-typed-ir-fixture-readiness-report",
        "status": "fixture-driven-candidate-projection-created-no-core-ir",
        "fixtures": reports,
        "explicitConversionExpression": explicit_conversion,
        "vectorTruncationExpression": vector_truncation,
        "limitations": [
            "uses legacy fixture topology and proposed operation coverage only",
        "MaterialOutput selects the graph root and is treated as a boundary; it does not require a typed operation record",
            "checks selected legacy PinType fragments against source and candidate record types; does not validate the complete Document schema or numeric lowering",
            "input links are matched by legacy display socket labels; stable socket IDs and conversion semantics are unresolved",
            "stable socket IDs are included only where the proposal manifest defines a legacy-label mapping; null means no mapping is proposed",
            "unconnected values are candidate source defaults and host-context inputs remain unresolved",
            "candidate parameter values are source-fixture projections; binary32 canonicalization and parameter validation are not performed",
            "candidateOperations are not a partial IR and must not be compiled or hashed as one",
            "candidateTypedIrProjection is a fixture-driven schema projection only; its serialization digest is not the Core semantic hash",
            "symbolic material texture slots do not resolve asset identity, revision, sampler, color interpretation, or alpha association",
            "no Core implementation, HLSL, shader compiler, renderer, or GPU evidence is included",
        ],
    }


def build_explicit_conversion_report(root: Path, manifest: dict, records: dict) -> dict:
    fixture_root = root / "tests/fixtures/shader-graph-rd"
    expression_path = fixture_root / "typed_ir_explicit_conversion_expression.json"
    source_fixture_path = fixture_root / "skipped_type_mismatch_link_negative.json"
    expression = read_json(expression_path)
    source_fixture = read_json(source_fixture_path)
    schema = manifest["coreTypedOperationSchemaProposal"]
    conversion_id = schema["explicitConversionExpression"]["operationId"]
    matrix = manifest["coreDocumentSchemaProposal"]["socketIdentityProposal"]["legacyConversionMatrix"]
    conversion_contract = next((item for item in matrix if item.get("operationId") == conversion_id), {})
    diagnostics = []
    operations = []
    node_by_id = {item.get("nodeId"): item for item in expression.get("operations", [])}
    for item in expression.get("operations", []):
        record = records.get(item.get("operationId"), {})
        if not record or item.get("semanticVersion") != record.get("semanticVersion"):
            diagnostics.append(f"{item.get('nodeId')}: operation record or semantic version is missing")
            continue
        projected_inputs = []
        declarations = {pin.get("inputId"): pin for pin in record.get("inputs", [])}
        for edge in item.get("inputs", []):
            declaration = declarations.get(edge.get("inputId"), {})
            source_node = node_by_id.get(edge.get("sourceNodeId"), {})
            source_record = records.get(source_node.get("operationId"), {})
            source_output = next((pin for pin in source_record.get("outputs", []) if pin.get("outputId") == edge.get("sourceOutputId")), {})
            if (not declaration or not source_output or
                    edge.get("valueType") != declaration.get("valueType") or
                    source_output.get("valueType") != declaration.get("valueType")):
                diagnostics.append(f"{item.get('nodeId')}.{edge.get('inputId')}: explicit conversion expression has an invalid typed edge")
                continue
            projected_inputs.append({"inputId": edge["inputId"], "valueType": declaration["valueType"], "binding": {"kind": "edge", "sourceNodeId": edge["sourceNodeId"], "sourceOutputId": edge["sourceOutputId"]}})
        operations.append({
            "nodeId": item.get("nodeId"), "operationId": item.get("operationId"),
            "semanticVersion": item.get("semanticVersion"), "inputs": projected_inputs,
            "outputs": record.get("outputs", []),
            "parameters": [{"parameterId": value["parameterId"], "value": binary32(value.get("value"))} for value in item.get("parameters", [])],
        })
    ids = [item.get("nodeId") for item in operations]
    operation_index = {node_id: index for index, node_id in enumerate(ids)}
    for item in operations:
        for edge in item.get("inputs", []):
            source_id = edge.get("binding", {}).get("sourceNodeId")
            if source_id not in operation_index or operation_index[source_id] >= operation_index.get(item.get("nodeId"), -1):
                diagnostics.append(f"{item.get('nodeId')}.{edge.get('inputId')}: operation order is not source-before-consumer")
    root_record = records.get(node_by_id.get(expression.get("root", {}).get("nodeId"), {}).get("operationId"), {})
    root_output = next((item for item in root_record.get("outputs", []) if item.get("outputId") == expression.get("root", {}).get("outputId")), {})
    if root_output.get("valueType") != expression.get("root", {}).get("valueType"):
        diagnostics.append("expression root output does not match its declared candidate type")
    if ids != [item.get("nodeId") for item in expression.get("operations", [])]:
        diagnostics.append("explicit conversion expression operation order is invalid")
    computed = None
    scalar = node_by_id.get("scalar", {})
    conversion = node_by_id.get("explicit_convert", {})
    scalar_value = next((item.get("value") for item in scalar.get("parameters", []) if item.get("parameterId") == "value"), None)
    conversion_inputs = conversion.get("inputs", [])
    if (conversion_inputs != [{"inputId": "value", "sourceNodeId": "scalar", "sourceOutputId": "value", "valueType": "Float"}] or
            scalar_value != conversion_contract.get("numericProbeInput")):
        diagnostics.append("explicit conversion fixture must connect the declared scalar probe to the conversion input")
    if conversion.get("operationId") == conversion_id and conversion_contract.get("from") == "Float" and conversion_contract.get("to") == "Vector4" and isinstance(scalar_value, (int, float)):
        value = binary32(float(scalar_value))
        computed = [value, value, value, 1.0]
    else:
        diagnostics.append("candidate CPU evaluator does not recognize the declared conversion contract")
    if computed != expression.get("expectedOutput"):
        diagnostics.append("explicit conversion candidate output differs from its fixture expectation")
    legacy_links = source_fixture.get("links", [])
    if not any(link.get("from") == "scalar" and link.get("to") == "surface" and link.get("in") == "BaseColor" for link in legacy_links):
        diagnostics.append("source negative fixture no longer contains the implicit Float-to-BaseColor edge")
    candidate = None
    if not diagnostics:
        candidate = {
            "kind": "artifact-typed-operation-expression-candidate",
            "status": "explicit-conversion-proposal-not-core-ir",
            "operations": operations,
            "root": expression["root"],
        }
        canonical = json.dumps(candidate, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        candidate["candidateSerializationSha256"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest().upper()
    return {
        "fixture": expression_path.relative_to(root).as_posix(),
        "fixtureSha256": hashlib.sha256(expression_path.read_bytes()).hexdigest().upper(),
        "sourceLegacyFixture": source_fixture_path.name,
        "sourceLegacyFixtureSha256": hashlib.sha256(source_fixture_path.read_bytes()).hexdigest().upper(),
        "operationId": conversion_id,
        "from": conversion_contract.get("from"), "to": conversion_contract.get("to"),
        "computedOutput": computed, "expectedOutput": expression.get("expectedOutput"),
        "status": "explicit-conversion-candidate-valid-no-core-ir" if candidate else "blocked-no-partial-expression",
        "candidateExpression": candidate,
        "diagnostics": diagnostics,
        "irProduced": False,
    }


def build_vector_truncation_report(root: Path, manifest: dict, records: dict) -> dict:
    fixture_root = root / "tests/fixtures/shader-graph-rd"
    expression_path = fixture_root / "typed_ir_vector4_to_vector3_expression.json"
    expression = read_json(expression_path)
    schema = manifest["coreTypedOperationSchemaProposal"]
    conversion_id = schema["vectorTruncationExpression"]["operationId"]
    matrix = manifest["coreDocumentSchemaProposal"]["socketIdentityProposal"]["legacyConversionMatrix"]
    contract = next((item for item in matrix if item.get("operationId") == conversion_id), {})
    diagnostics = []
    nodes = {item.get("nodeId"): item for item in expression.get("operations", [])}
    if len(expression.get("operations", [])) != 2:
        diagnostics.append("vector truncation expression must contain source and conversion operations")
    conversion = nodes.get("explicit_convert", {})
    source = nodes.get("vector", {})
    source_value = next((item.get("value") for item in source.get("parameters", []) if item.get("parameterId") == "value"), None)
    if (source.get("operationId") != "artifact.constant.vector4.float32" or
            conversion.get("operationId") != conversion_id or
            conversion.get("inputs") != [{"inputId": "value", "sourceNodeId": "vector", "sourceOutputId": "value", "valueType": "Vector4"}] or
            source_value != contract.get("numericProbeInput") or
            contract.get("from") != "Vector4" or contract.get("to") != "Vector3"):
        diagnostics.append("vector truncation expression must match the declared Vector4-to-Vector3 probe and typed edge")
    projection = None
    if not diagnostics:
        computed = [binary32(float(component)) for component in source_value[:3]]
        if computed != expression.get("expectedOutput") or computed != contract.get("expectedSemanticValue"):
            diagnostics.append("Vector4-to-Vector3 candidate output differs from the declared truncation result")
        else:
            projection = {
                "kind": "artifact-typed-operation-expression-candidate",
                "status": "explicit-conversion-proposal-not-core-ir",
                "operations": [
                    {"nodeId": "vector", "operationId": source["operationId"], "semanticVersion": source["semanticVersion"], "parameters": [{"parameterId": "value", "value": [binary32(float(value)) for value in source_value]}], "inputs": [], "outputs": records[source["operationId"]]["outputs"]},
                    {"nodeId": "explicit_convert", "operationId": conversion_id, "semanticVersion": conversion["semanticVersion"], "parameters": [], "inputs": [{"inputId": "value", "valueType": "Vector4", "binding": {"kind": "edge", "sourceNodeId": "vector", "sourceOutputId": "value"}}], "outputs": records[conversion_id]["outputs"]},
                ],
                "root": expression["root"],
            }
            canonical = json.dumps(projection, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
            projection["candidateSerializationSha256"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest().upper()
    return {
        "fixture": expression_path.relative_to(root).as_posix(),
        "fixtureSha256": hashlib.sha256(expression_path.read_bytes()).hexdigest().upper(),
        "operationId": conversion_id,
        "from": contract.get("from"), "to": contract.get("to"),
        "computedOutput": ([binary32(float(component)) for component in source_value[:3]] if isinstance(source_value, list) and len(source_value) == 4 else None),
        "expectedOutput": expression.get("expectedOutput"),
        "status": "explicit-conversion-candidate-valid-no-core-ir" if projection else "blocked-no-partial-expression",
        "candidateExpression": projection,
        "diagnostics": diagnostics,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/typed_ir_projection_readiness.json"))
    args = parser.parse_args()
    result = build_report(args.project_root.resolve())
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
