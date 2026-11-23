#!/usr/bin/env python3
"""Generate CPU-only proposal probes for Shader Graph identity boundaries."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path


def canonical_bytes(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha(value: object) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest().upper()


def base_state() -> dict:
    return {
        "document": {
            "schemaId": "artifact.material-graph.document",
            "schemaVersion": 1,
            "nodes": [
                {"nodeId": "color", "operationId": "artifact.constant.color4", "semanticVersion": 1,
                 "parameters": {"value": [0.25, 0.5, 0.75, 1.0]}, "layout": {"x": 10.0, "y": 20.0}},
                {"nodeId": "extra", "operationId": "artifact.constant.color4", "semanticVersion": 1,
                 "parameters": {"value": [0.9, 0.8, 0.7, 1.0]}, "layout": {"x": 10.0, "y": 80.0}},
                {"nodeId": "extra2", "operationId": "artifact.constant.color4", "semanticVersion": 1,
                 "parameters": {"value": [0.1, 0.2, 0.3, 1.0]}, "layout": {"x": 10.0, "y": 120.0}},
                {"nodeId": "math", "operationId": "artifact.math.fract.vector4", "semanticVersion": 1,
                 "parameters": {"clampResult": False}, "layout": {"x": 100.0, "y": 20.0}},
                {"nodeId": "output", "operationId": "artifact.material.output.base_color", "semanticVersion": 1,
                 "parameters": {}, "layout": {"x": 200.0, "y": 20.0}},
            ],
            "links": [
                {"fromNodeId": "color", "fromSocketId": "color", "toNodeId": "math", "toSocketId": "a"},
                {"fromNodeId": "extra", "fromSocketId": "color", "toNodeId": "math", "toSocketId": "b"},
                {"fromNodeId": "math", "fromSocketId": "value", "toNodeId": "output", "toSocketId": "base_color"},
            ],
            "registeredSemanticExtensions": {},
            "opaqueExtensions": {"vendor.note": {"kept": True}},
        },
        "provenance": {"color": {"sourceNodeId": "color", "sourceSocketId": "Color", "sourcePath": "/nodes/0"},
                       "math": {"sourceNodeId": "math", "sourceSocketId": "Value", "sourcePath": "/nodes/3"}},
        "resourceBinding": {"slot": "base_color", "assetId": "asset-17", "revision": 4,
                             "sampler": "linear-wrap", "uvSet": 0, "uvTransform": "identity", "colorInterpretation": "linear"},
        "backend": {"id": "Vulkan", "compiler": "compiler-v1", "options": ["fast-math-off"], "profile": "ps_6_0"},
        "pipeline": {"vertexLayout": "pbr-v1", "renderTarget": "rgba16f", "depth": "d32", "blend": "opaque", "raster": "default", "deviceGeneration": 1},
    }


def projections(state: dict) -> dict[str, str]:
    document = state["document"]
    canonical_document = {
        **{key: value for key, value in document.items() if key not in {"nodes", "links"}},
        "nodes": sorted(document["nodes"], key=lambda item: item["nodeId"].encode("utf-8")),
        "links": sorted(document["links"], key=lambda item: tuple(item[key].encode("utf-8") for key in ("fromNodeId", "fromSocketId", "toNodeId", "toSocketId"))),
    }
    semantic_document = {
        "schemaId": document["schemaId"], "schemaVersion": document["schemaVersion"],
        "nodes": [{key: value for key, value in node.items() if key != "layout" and key != "opaqueExtensions"} for node in canonical_document["nodes"]],
        "links": canonical_document["links"],
        "registeredSemanticExtensions": document["registeredSemanticExtensions"],
    }
    node_by_id = {node["nodeId"]: node for node in canonical_document["nodes"]}
    consumed_inputs = {
        "artifact.constant.color4": set(),
        "artifact.math.fract.vector4": {"a"},
        "artifact.material.output.base_color": {"base_color"},
    }
    incoming = {}
    for edge in canonical_document["links"]:
        incoming.setdefault(edge["toNodeId"], []).append(edge)
    active = set()
    def visit(node_id: str) -> None:
        if node_id in active:
            return
        active.add(node_id)
        operation_id = node_by_id[node_id]["operationId"]
        for edge in incoming.get(node_id, []):
            if edge["toSocketId"] in consumed_inputs[operation_id]:
                visit(edge["fromNodeId"])
    visit("output")
    active_nodes = {node_id: node_by_id[node_id] for node_id in active}
    active_edges = [edge for edge in canonical_document["links"] if edge["fromNodeId"] in active and edge["toNodeId"] in active and edge["toSocketId"] in consumed_inputs[active_nodes[edge["toNodeId"]]["operationId"]]]
    indegree = {node_id: 0 for node_id in active_nodes}
    outgoing = {node_id: [] for node_id in active_nodes}
    for edge in active_edges:
        indegree[edge["toNodeId"]] += 1
        outgoing[edge["fromNodeId"]].append(edge["toNodeId"])
    ready = sorted((node_id for node_id, degree in indegree.items() if degree == 0), key=lambda value: value.encode("utf-8"))
    ordered_nodes = []
    while ready:
        node_id = ready.pop(0)
        ordered_nodes.append(node_by_id[node_id])
        for target in outgoing[node_id]:
            indegree[target] -= 1
            if indegree[target] == 0:
                ready.append(target)
                ready.sort(key=lambda value: value.encode("utf-8"))
    if len(ordered_nodes) != len(active_nodes):
        raise ValueError("active identity probe graph must be acyclic")
    ir_nodes = []
    for node in ordered_nodes:
        ir_nodes.append({key: node[key] for key in ("nodeId", "operationId", "semanticVersion", "parameters")})
    ir = {"nodes": ir_nodes, "typedEdges": active_edges, "outputs": ["base_color"]}
    # The sample emitter uses canonical ordinals, not editor IDs, for shader symbols.
    symbols = {node["nodeId"]: f"n{index:03d}" for index, node in enumerate(ir_nodes)}
    source_lines = []
    for node in ir_nodes:
        symbol = symbols[node["nodeId"]]
        if node["operationId"] == "artifact.constant.color4":
            rgba = ",".join(format(float(value), ".9g") for value in node["parameters"]["value"])
            source_lines.append(f"float4 {symbol} = float4({rgba});")
        elif node["operationId"] == "artifact.math.fract.vector4":
            edge = next(item for item in active_edges if item["toNodeId"] == node["nodeId"] and item["toSocketId"] == "a")
            source_lines.append(f"float4 {symbol} = frac({symbols[edge['fromNodeId']]});")
        elif node["operationId"] == "artifact.material.output.base_color":
            edge = next(item for item in active_edges if item["toNodeId"] == node["nodeId"])
            source_lines.append(f"float4 material_base_color = {symbols[edge['fromNodeId']]};")
    source = "\n".join(source_lines) + "\n"
    document_bytes = canonical_bytes(canonical_document)
    document_hash = hashlib.sha256(canonical_bytes(semantic_document)).hexdigest().upper()
    ir_hash = sha(ir)
    source_hash = hashlib.sha256(source.encode("utf-8")).hexdigest().upper()
    compile_key = sha({"irHash": ir_hash, "sourceHash": source_hash, "backend": state["backend"]})
    binding_key = sha(state["resourceBinding"])
    pipeline_key = sha({"compileKey": compile_key, "pipeline": state["pipeline"]})
    provenance_hash = sha(state["provenance"])
    return {
        "canonicalDocumentBytesHash": hashlib.sha256(document_bytes).hexdigest().upper(),
        "documentSemanticHash": document_hash,
        "typedIrSemanticHash": ir_hash,
        "emittedSourceHash": source_hash,
        "shaderCompileKey": compile_key,
        "resourceBindingKey": binding_key,
        "pipelineKey": pipeline_key,
        "provenanceHash": provenance_hash,
    }


def make_golden() -> dict:
    baseline_state = base_state()
    baseline = projections(baseline_state)
    scenarios = []

    def add(name: str, mutate, expected_changed: list[str], purpose: str) -> None:
        state = copy.deepcopy(baseline_state)
        mutate(state)
        result = projections(state)
        changed = sorted(key for key in baseline if baseline[key] != result[key])
        scenarios.append({"id": name, "purpose": purpose, "expectedChangedKeys": sorted(expected_changed),
                          "actualChangedKeys": changed, "keys": result})

    add("storage-order-permutation", lambda s: (s["document"]["nodes"].reverse(), s["document"]["links"].reverse()), [], "node/link storage order does not change canonical identities")
    add("layout-only-change", lambda s: s["document"]["nodes"][0]["layout"].update({"x": 999.0, "y": -5.0}), ["canonicalDocumentBytesHash"], "layout persists in canonical document bytes but does not invalidate semantic shader keys")
    add("opaque-extension-only-change", lambda s: s["document"]["opaqueExtensions"].update({"vendor.note": {"kept": False}}), ["canonicalDocumentBytesHash"], "opaque unregistered payload round-trips but does not alter shader semantic identity")
    add("provenance-only-change", lambda s: s["provenance"]["color"].update({"sourcePath": "/nodes/8"}), ["provenanceHash"], "debug source mapping changes without changing semantic or generated identities")
    add("resource-revision-change", lambda s: s["resourceBinding"].update({"revision": 5}), ["resourceBindingKey"], "runtime asset revision invalidates binding identity only when no compile specialization is used")
    add("compile-specialization-change", lambda s: s["document"]["nodes"][0]["parameters"].update({"value": [0.5, 0.5, 0.5, 1.0]}), ["canonicalDocumentBytesHash", "documentSemanticHash", "typedIrSemanticHash", "emittedSourceHash", "shaderCompileKey", "pipelineKey"], "compile-time constant changes graph/source/compile/pipeline identity but not runtime resource binding")
    add("backend-compiler-change", lambda s: s["backend"].update({"id": "D3D12", "compiler": "compiler-v2"}), ["shaderCompileKey", "pipelineKey"], "backend/compiler change leaves graph/source/resource binding identities stable")
    add("pipeline-format-change", lambda s: s["pipeline"].update({"renderTarget": "rgba32f"}), ["pipelineKey"], "render-target format changes PSO identity only")
    add("stable-node-id-change", lambda s: s["document"]["nodes"][0].update({"nodeId": "color-renamed"}) or s["document"]["links"][0].update({"fromNodeId": "color-renamed"}) or s["provenance"].update({"color-renamed": s["provenance"].pop("color")}), ["canonicalDocumentBytesHash", "documentSemanticHash", "typedIrSemanticHash", "shaderCompileKey", "pipelineKey", "provenanceHash"], "stable node identity participates in document/IR identity while canonical ordinal source symbols remain unchanged")
    add("unreachable-node-parameter-change", lambda s: next(node for node in s["document"]["nodes"] if node["nodeId"] == "extra2")["parameters"].update({"value": [0.4, 0.3, 0.2, 1.0]}), ["canonicalDocumentBytesHash", "documentSemanticHash"], "unreachable editable node remains in Document identity but not active IR/source/compile identity")
    add("unused-input-link-retarget", lambda s: next(edge for edge in s["document"]["links"] if edge["toSocketId"] == "b").update({"fromNodeId": "extra2"}), ["canonicalDocumentBytesHash", "documentSemanticHash"], "link to unary Math B remains in Document but is outside the operation's consumed-input projection")
    if any(case["actualChangedKeys"] != case["expectedChangedKeys"] for case in scenarios):
        raise ValueError("lowering identity scenario changed-key set differs from its declared contract")
    return {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-lowering-identity-proposal-golden",
        "status": "cpu-projection-model-only-not-core-or-renderer-evidence",
        "baselineKeys": baseline,
        "keyNames": list(baseline),
        "scenarios": scenarios,
        "limitations": ["sample projections are a proposal oracle, not implemented ArtifactCore hashes", "generated source is a tiny CPU model, not current HLSL", "no runtime resource, compiler, PSO, or GPU measurements are included"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("tests/fixtures/shader-graph-rd/lowering_identity_proposal_golden.json"))
    args = parser.parse_args()
    result = make_golden()
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
