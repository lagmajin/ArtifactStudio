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
    resource_projection = module.project_resource_requirements(document, asset_catalog)
    dynamic_document, node_resource_references = module.adapt_document_to_dynamic_registry(
        document, resource_projection)
    dynamic_bindings = module.build_dynamic_resolved_bindings(
        document, resource_projection, source_states)
    dynamic_candidate = dynamic_module.compile_document_from_data(
        dynamic_document, dynamic_module.load_registry(root / dynamic_contract["registryFixture"]),
        node_resource_references=node_resource_references,
        resolved_resource_bindings=dynamic_bindings)
    baseline_projection = {key: dynamic_candidate[key] for key in (
        "resourceBindingStatus", "resolvedResourceBindings", "unresolvedResourceKeys",
        "resourceBindingKeySha256")}
    candidate["dynamicResolverBindingAdapter"] = {
        "status": "generic-node-document-adapted-and-lowered-by-dynamic-prototype",
        "dynamicDocument": dynamic_document,
        "nodeResourceReferences": node_resource_references,
        "projection": baseline_projection,
        "compilerCandidate": dynamic_candidate,
        "adapterPolicy": {
            "samplerKey": "SHA-256 of canonical authored sampler descriptor, prefixed sampler.",
            "uv": "UV remains an explicit graph link; this fixture maps the UV0 source to set 0 and identity transform only as resolver compatibility metadata.",
            "scope": "The translated graph uses the closed dynamic registry and HLSL candidate lowering. Schema-v4 persistence, editor authoring, shader compilation, and GPU binding remain outside this adapter."
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
    contract["dynamicRegistrySha256"] = hashlib.sha256(
        (root / dynamic_contract["registryFixture"]).read_bytes()).hexdigest().upper()
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                             encoding="utf-8", newline="\n")
    print(f"Updated {contract['candidateFixture']} ({contract['candidateSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
