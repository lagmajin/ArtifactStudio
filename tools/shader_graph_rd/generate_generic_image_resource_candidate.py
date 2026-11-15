#!/usr/bin/env python3
"""Regenerate the generic Image Texture reference candidate and manifest pins."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import sys


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    manifest_path = root / "tests/fixtures/shader-graph-rd/evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"][
        "genericImageResourceReferencePrototype"]
    dynamic_contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"]
    prototype_path = root / contract["prototype"]
    spec = importlib.util.spec_from_file_location("generic_image_resource_prototype", prototype_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load generic image resource prototype")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)

    dynamic_path = root / dynamic_contract["prototype"]
    dynamic_spec = importlib.util.spec_from_file_location(
        "shader_graph_dynamic_binding_resolver", dynamic_path)
    if dynamic_spec is None or dynamic_spec.loader is None:
        raise RuntimeError("cannot load dynamic graph binding resolver")
    dynamic_module = importlib.util.module_from_spec(dynamic_spec)
    sys.modules[dynamic_spec.name] = dynamic_module
    dynamic_spec.loader.exec_module(dynamic_module)

    document_path = root / contract["documentFixture"]
    catalog_path = root / contract["assetCatalogFixture"]
    source_states_path = root / contract["assetSourceStatesFixture"]
    candidate_path = root / contract["candidateFixture"]
    document = json.loads(document_path.read_text(encoding="utf-8"))
    asset_catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    source_states = json.loads(source_states_path.read_text(encoding="utf-8"))
    candidate = module.build_candidate(document, asset_catalog, source_states)

    def dynamic_binding_projection(doc, catalog, states):
        requirements_projection = module.project_resource_requirements(doc, catalog)
        resolutions = {item["resourceKey"]: item
                       for item in requirements_projection["assetResolutions"]}
        nodes = {item["nodeId"]: item for item in doc["nodes"]}
        normalized_states = {key.lower(): value for key, value in states.items()}
        bindings = {}
        for requirement in requirements_projection["resourceRequirements"]:
            resolution = resolutions.get(requirement["resourceKey"])
            if resolution is None or resolution["assetType"] is None:
                continue
            source = normalized_states.get(resolution["assetId"].lower())
            if source is None:
                continue
            node = nodes[requirement["nodeId"]]
            sampler_bytes = json.dumps(node["sampler"], sort_keys=True,
                                       separators=(",", ":"),
                                       ensure_ascii=False).encode("utf-8")
            sampler_key = "sampler." + hashlib.sha256(sampler_bytes).hexdigest().lower()
            bindings[requirement["resourceKey"]] = {
                "assetId": resolution["assetId"],
                "assetType": resolution["assetType"],
                "sourceRevision": source["sourceRevision"],
                "samplerKey": sampler_key,
                "uvSet": 0,
                "uvTransform": {"scaleU": 1.0, "scaleV": 1.0,
                                "rotationDegrees": 0.0, "offsetU": 0.0,
                                "offsetV": 0.0},
                "colorInterpretation": node["colorInterpretation"],
            }
        return dynamic_module._project_resolved_resource_bindings(
            requirements_projection["resourceRequirements"], bindings)

    baseline_projection = dynamic_binding_projection(document, asset_catalog, source_states)
    candidate["dynamicResolverBindingAdapter"] = {
        "status": "projected-through-dynamic-compiler-binding-resolver-only",
        "projection": baseline_projection,
        "adapterPolicy": {
            "samplerKey": "SHA-256 of canonical authored sampler descriptor, prefixed sampler.",
            "uv": "UV remains an explicit graph link; this fixture maps the UV0 source to set 0 and identity transform only as resolver compatibility metadata.",
            "scope": "Exercises the existing dynamic compiler resource-binding resolver; does not feed generic nodes through registry validation, typed IR, HLSL lowering, or a GPU backend."
        }
    }
    candidate_path.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n",
                              encoding="utf-8", newline="\n")

    for key, path in (("documentSha256", document_path),
                      ("assetCatalogSha256", catalog_path),
                      ("assetSourceStatesSha256", source_states_path),
                      ("prototypeSha256", prototype_path),
                      ("candidateSha256", candidate_path)):
        contract[key] = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    contract["dynamicResolverPrototypeSha256"] = hashlib.sha256(dynamic_path.read_bytes()).hexdigest().upper()
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                             encoding="utf-8", newline="\n")
    print(f"Updated {contract['candidateFixture']} ({contract['candidateSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
