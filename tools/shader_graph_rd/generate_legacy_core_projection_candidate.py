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
        },
        "limitations": [
            "read-only projection of the explicitly selected Principled BaseColor input only",
            "supports only linked RGB constants feeding both inputs of unclamped Math Add",
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
