#!/usr/bin/env python3
"""Audit the proposed typed operation catalog without claiming runtime support."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


def build_report(root: Path) -> dict:
    manifest_path = root / "tests/fixtures/shader-graph-rd/evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    schema = manifest["coreTypedOperationSchemaProposal"]
    records = schema["exampleOperationRecords"]
    families = schema["legacyNodeCoverage"]["families"]
    legacy_pins = manifest["coreDocumentSchemaProposal"]["legacySocketTypeCharacterization"]
    stable_socket_ids = manifest["coreDocumentSchemaProposal"]["socketIdentityProposal"]["legacyLabelToStableId"]
    source_path = root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp"
    source = source_path.read_text(encoding="utf-8")
    constructors = {
        "Color": "ColorNode", "Output": "OutputNode", "Add": "AddNode",
        "Value": "ValueNode", "RGB": "RGBNode", "TexCoord": "TexCoordNode",
        "Geometry": "GeometryNode", "Fresnel": "FresnelNode", "LayerWeight": "LayerWeightNode",
        "Blackbody": "BlackbodyNode", "ImageTexture": "ImageTextureNode", "Noise": "NoiseTextureNode",
        "Voronoi": "VoronoiTextureNode", "MixRGB": "MixRGBNode", "ColorRamp": "ColorRampNode",
        "Math": "MathNode", "Principled": "PrincipledBSDFNode", "MixShader": "MixShaderNode",
        "MaterialOutput": "MaterialOutputNode", "NormalMap": "NormalMapNode", "Bump": "BumpNode",
        "Invert": "InvertNode", "Gamma": "GammaNode", "HueSatVal": "HueSatValNode",
        "Luminance": "LuminanceNode", "MapRange": "MapRangeNode", "Clamp": "ClampNode",
    }
    source_pins_by_family: dict[str, dict[str, list[dict[str, str]]]] = {}
    for family_name, constructor in constructors.items():
        match = re.search(
            rf"{constructor}::{constructor}\([^)]*\)\s*\{{(.*?)(?=\n\s*\}})",
            source, re.DOTALL,
        )
        pins = {"inputs": [], "outputs": []}
        if match:
            for call, direction in (("addInput", "inputs"), ("addOutput", "outputs")):
                for pin_match in re.finditer(rf'{call}\("([^"]+)",\s*PinType::(\w+)\)', match.group(1)):
                    pins[direction].append({"label": pin_match.group(1), "valueType": pin_match.group(2)})
        source_pins_by_family[family_name] = pins
    promotion_waves = schema["operationPromotionWaves"]
    family_ids = {operation_id for family in families.values() for operation_id in family["operationIds"]}
    records_by_id = {record["operationId"]: record for record in records}
    conversion_rows = manifest["coreDocumentSchemaProposal"]["socketIdentityProposal"]["legacyConversionMatrix"]
    conversion_ids = {item["operationId"] for item in conversion_rows}
    entries = []
    for record in sorted(records, key=lambda item: (item["operationId"], item["semanticVersion"])):
        record_bytes = json.dumps(record, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        entries.append({
            "operationId": record["operationId"],
            "semanticVersion": record["semanticVersion"],
            "registryKey": [record["operationId"], record["semanticVersion"]],
            "inputIds": [item["inputId"] for item in record["inputs"]],
            "outputIds": [item["outputId"] for item in record["outputs"]],
            "parameterIds": [item["parameterId"] for item in record["parameters"]],
            "familyCoverageReference": record["operationId"] in family_ids,
            "explicitConversionReference": record["operationId"] in conversion_ids,
            "resourceDependencyCount": len(record["resourceDependencies"]),
            "semanticHashFields": record["semanticHashFields"],
            "evidenceStatus": record["evidenceStatus"],
            "sourceRecordSha256": hashlib.sha256(record_bytes.encode("utf-8")).hexdigest().upper(),
        })
    family_pin_checks = []
    operation_socket_binding_checks = []
    operation_socket_binding_errors = []
    for family_name, family in families.items():
        observed = legacy_pins.get(family_name, {})
        declared_inputs = family.get("legacyInputTypes", observed.get("inputs", {}))
        declared_outputs = family.get("legacyOutputTypes", observed.get("outputs", {}))
        input_rows = []
        output_rows = []
        for direction, declared, actual, target_rows in (
            ("input", declared_inputs, observed.get("inputs", {}), input_rows),
            ("output", declared_outputs, observed.get("outputs", {}), output_rows),
        ):
            for label, value_type in sorted(declared.items()):
                legacy_type = actual.get(label)
                stable_id = stable_socket_ids.get(f"{family_name}.{label}")
                constructor = constructors.get(family_name)
                source_type = None
                source_pin_present = False
                if constructor:
                    match = re.search(
                        rf"{constructor}::{constructor}\([^)]*\)\s*\{{(.*?)(?=\n\s*\}})",
                        source, re.DOTALL,
                    )
                    if match:
                        call = "addInput" if direction == "input" else "addOutput"
                        pin_match = re.search(rf'{call}\("{re.escape(label)}",\s*PinType::(\w+)\)', match.group(1))
                        if pin_match:
                            source_pin_present = True
                            source_type = {"Float": "Float", "Vector2": "Vector2", "Vector3": "Vector3", "Vector4": "Vector4", "Shader": "Shader"}.get(pin_match.group(1), pin_match.group(1))
                target_rows.append({
                    "label": label,
                    "direction": direction,
                    "declaredType": value_type,
                    "legacyObservedType": legacy_type,
                    "sourceObservedType": source_type,
                    "stableSocketId": stable_id,
                    "typeMatches": legacy_type == value_type,
                    "sourceMatches": source_pin_present and source_type == value_type,
                    "stableIdPresent": bool(stable_id),
                })
        family_pin_checks.append({
            "legacyNodeKind": family_name,
            "coverage": family.get("coverage"),
            "inputPins": input_rows,
            "outputPins": output_rows,
            "sourcePins": source_pins_by_family.get(family_name, {"inputs": [], "outputs": []}),
            "uncharacterizedSourcePins": [
                {"direction": direction, **pin}
                for direction in ("inputs", "outputs")
                for pin in source_pins_by_family.get(family_name, {}).get(direction, [])
                if pin["label"] not in (declared_inputs if direction == "inputs" else declared_outputs)
            ],
            "completeDeclaredPinCoverage": bool(input_rows or output_rows) and
                all(item["typeMatches"] and item["sourceMatches"] and item["stableIdPresent"] for item in input_rows + output_rows) and
                not any(pin["label"] not in (declared_inputs if direction == "inputs" else declared_outputs)
                        for direction, pins in source_pins_by_family.get(family_name, {}).items() for pin in pins),
            "runtimeSupportClaim": False,
        })
        for operation_id in family.get("operationIds", []):
            record = records_by_id.get(operation_id, {})
            mapping = family.get("operationSocketMappings", {}).get(operation_id, {})
            operation_rows = []
            for direction, record_key, declared_key in (
                ("input", "inputs", declared_inputs),
                ("output", "outputs", declared_outputs),
            ):
                mapped_by_id = {item.get("operationSocketId"): item for item in mapping.get(record_key, [])}
                for socket in record.get(record_key, []):
                    operation_socket_id = socket.get("inputId" if direction == "input" else "outputId")
                    mapped = mapped_by_id.get(operation_socket_id, {})
                    label = mapped.get("legacySocketLabel")
                    legacy_type = declared_key.get(label)
                    stable_id = stable_socket_ids.get(f"{family_name}.{label}") if label else None
                    adapter = mapped.get("adapter")
                    operation_type = socket.get("valueType")
                    expected_adapter = (
                        "identity" if legacy_type == operation_type else
                        "Vector4-to-Color4-component-identity" if (legacy_type, operation_type) == ("Vector4", "Color4") else
                        "Shader-to-SurfaceFields-renderer-adapter-unverified" if (legacy_type, operation_type) == ("Shader", "SurfaceFields") else
                        None
                    )
                    row = {
                        "direction": direction,
                        "operationSocketId": operation_socket_id,
                        "operationValueType": operation_type,
                        "legacySocketLabel": label,
                        "legacyValueType": legacy_type,
                        "stableSocketId": stable_id,
                        "adapter": adapter,
                        "valid": bool(label and stable_id and expected_adapter == adapter),
                    }
                    operation_rows.append(row)
                    if not row["valid"]:
                        operation_socket_binding_errors.append({"legacyNodeKind": family_name, "operationId": operation_id, **row})
                if set(mapped_by_id) != {socket.get("inputId" if direction == "input" else "outputId") for socket in record.get(record_key, [])}:
                    operation_socket_binding_errors.append({"legacyNodeKind": family_name, "operationId": operation_id, "direction": direction, "error": "mapping IDs do not exactly cover operation record sockets"})
            operation_socket_binding_checks.append({
                "legacyNodeKind": family_name,
                "operationId": operation_id,
                "bindings": operation_rows,
                "runtimeSupportClaim": False,
            })
    keys = [tuple(item["registryKey"]) for item in entries]
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-operation-registry-audit",
        "status": "manifest-derived-proposal-audit-no-core-registry",
        "manifest": manifest_path.relative_to(root).as_posix(),
        "manifestSha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest().upper(),
        "operationCount": len(entries),
        "uniqueRegistryKeyCount": len(set(keys)),
        "duplicateRegistryKeys": [list(key) for key in sorted({key for key in keys if keys.count(key) > 1})],
        "operations": entries,
        "coverage": {
            "legacyNodeFamilies": {name: item["operationIds"] for name, item in families.items()},
            "explicitConversions": sorted(conversion_ids),
            "recordsOutsideLegacyFamilyCoverage": sorted(item["operationId"] for item in entries if not item["familyCoverageReference"]),
        },
        "promotionWaves": promotion_waves,
        "waveCoverage": [
            {
                "wave": wave["wave"],
                "name": wave["name"],
                "operationIds": wave["operationIds"],
                "unknownOperationIds": sorted(set(wave["operationIds"]) - {item["operationId"] for item in entries}),
                "status": wave["status"],
            }
            for wave in promotion_waves
        ],
        "legacyPinContractAudit": {
            "status": "manifest-cross-check-only-no-runtime-registry",
            "families": family_pin_checks,
            "operationSocketBindings": operation_socket_binding_checks,
            "operationSocketBindingErrors": operation_socket_binding_errors,
            "sourceFamilyInventory": sorted(constructors),
            "allSourcePinsByFamily": source_pins_by_family,
            "sourceFamiliesMissingCoverageRows": sorted(set(constructors) - set(families)),
            "familiesWithDeclaredPins": sum(bool(item["inputPins"] or item["outputPins"]) for item in family_pin_checks),
            "familiesWithSourcePins": sum(bool(item["sourcePins"]["inputs"] or item["sourcePins"]["outputs"]) for item in family_pin_checks),
            "pinCount": sum(len(item["inputPins"]) + len(item["outputPins"]) for item in family_pin_checks),
            "mismatchedOrUnidentifiedPins": [
                {"legacyNodeKind": item["legacyNodeKind"], "direction": pin["direction"], "label": pin["label"], "declaredType": pin["declaredType"], "legacyObservedType": pin["legacyObservedType"], "stableSocketId": pin["stableSocketId"]}
                for item in family_pin_checks for pin in item["inputPins"] + item["outputPins"]
                if not pin["typeMatches"] or not pin["sourceMatches"] or not pin["stableIdPresent"]
            ],
            "sourcePath": source_path.relative_to(root).as_posix(),
            "sourceSha256": hashlib.sha256(source_path.read_bytes()).hexdigest().upper(),
            "sourceAuditScope": "constructor addInput/addOutput declarations for manifest-characterized families only",
        },
        "limitations": [
            "records are proposal metadata and do not prove a runtime registry or lowering implementation",
            "sourceRecordSha256 is an audit digest, not a Core semantic hash",
            "resource dependencies describe symbolic requirements and do not resolve assets or backend bindings",
            "evidenceStatus does not include executed CPU contract tests, shader compilation, GPU output, or backend parity",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/operation_registry_audit.json"))
    args = parser.parse_args()
    root = args.project_root.resolve()
    report = build_report(root)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
