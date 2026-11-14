#!/usr/bin/env python3
"""Validate the schema-v4 generic Image Texture resource-reference proposal.

This isolated prototype projects active document references into typed resource
requirements. It does not compile shaders, load images, or create GPU objects.
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import re
from pathlib import Path
from typing import Any


UUID = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")
MAX_NODES = 4096
MAX_LINKS = 16384
MAX_RESOURCES = 4096


class ContractError(ValueError):
    def __init__(self, message: str, code: str, path: str) -> None:
        super().__init__(message)
        self.code = code
        self.path = path

    def to_json(self) -> dict[str, str]:
        return {"code": self.code, "severity": "error", "path": self.path,
                "message": str(self)}


def _require(condition: bool, message: str, code: str, path: str) -> None:
    if not condition:
        raise ContractError(message, code, path)


def project_resource_requirements(document: Any, asset_catalog: Any) -> dict[str, Any]:
    """Validate the closed UV0/Image2D subset and project active resources."""
    _require(isinstance(document, dict), "document must be an object",
             "document.invalid", "")
    _require(document.get("schemaId") == "artifact.shadergraph.document" and
             document.get("schemaVersion") == 4 and
             document.get("kind") == "artifact-shader-graph-rd-document" and
             set(document) == {"schemaId", "schemaVersion", "kind", "nodes", "links",
                               "resources", "output"},
             "document schema identity is unsupported", "schema.version.unsupported",
             "/schemaVersion")
    _require(isinstance(asset_catalog, dict), "asset catalog must map Asset UUID to type",
             "resource.catalog.invalid", "/assetCatalog")
    canonical_catalog: dict[str, str] = {}
    for asset_id, asset_type in asset_catalog.items():
        _require(isinstance(asset_id, str) and UUID.fullmatch(asset_id),
                 "asset catalog key must be a canonical UUID string",
                 "resource.catalog.asset_id.invalid", "/assetCatalog")
        _require(isinstance(asset_type, str) and bool(asset_type),
                 "asset catalog type must be a non-empty string",
                 "resource.catalog.asset_type.invalid", f"/assetCatalog/{asset_id}")
        normalized_id = asset_id.lower()
        _require(normalized_id not in canonical_catalog,
                 "asset catalog contains duplicate UUIDs", "resource.catalog.asset_id.duplicate",
                 "/assetCatalog")
        canonical_catalog[normalized_id] = asset_type

    nodes_raw = document.get("nodes")
    resources_raw = document.get("resources")
    links_raw = document.get("links")
    _require(isinstance(nodes_raw, list), "nodes must be an array",
             "schema.collection.required", "/nodes")
    _require(isinstance(resources_raw, list), "resources must be an array",
             "schema.collection.required", "/resources")
    _require(isinstance(links_raw, list), "links must be an array",
             "schema.collection.required", "/links")
    _require(len(nodes_raw) <= MAX_NODES, "document exceeds the node limit",
             "graph.node.limit.exceeded", "/nodes")
    _require(len(links_raw) <= MAX_LINKS, "document exceeds the link limit",
             "graph.link.limit.exceeded", "/links")
    _require(len(resources_raw) <= MAX_RESOURCES, "document exceeds the resource limit",
             "graph.resource.limit.exceeded", "/resources")

    resources: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(resources_raw):
        path = f"/resources/{index}"
        _require(isinstance(item, dict) and set(item) == {"resourceId", "resourceKind", "source"},
                 "resource entry shape is invalid", "resource.shape.invalid", path)
        resource_id = item["resourceId"]
        _require(isinstance(resource_id, str) and IDENTIFIER.fullmatch(resource_id),
                 "resourceId is invalid", "resource.id.invalid", f"{path}/resourceId")
        _require(resource_id not in resources, "resourceId is duplicated",
                 "resource.id.duplicate", f"{path}/resourceId")
        _require(item["resourceKind"] == "sampledTexture2D", "resource kind is unsupported",
                 "resource.kind.unsupported", f"{path}/resourceKind")
        source = item["source"]
        _require(isinstance(source, dict) and set(source) == {"kind", "assetId"} and
                 source.get("kind") == "projectAsset", "resource source is invalid",
                 "resource.source.invalid", f"{path}/source")
        asset_id = source["assetId"]
        _require(isinstance(asset_id, str) and UUID.fullmatch(asset_id),
                 "assetId must be a canonical UUID string", "resource.asset_id.invalid",
                 f"{path}/source/assetId")
        resources[resource_id] = {"resourceId": resource_id, "assetId": asset_id.lower(),
                                  "resourceKind": item["resourceKind"]}

    nodes: dict[str, dict[str, Any]] = {}
    node_indices: dict[str, int] = {}
    output_types: dict[tuple[str, str], str] = {}
    input_types: dict[tuple[str, str], str] = {}
    for index, node in enumerate(nodes_raw):
        path = f"/nodes/{index}"
        _require(isinstance(node, dict), "node must be an object", "node.invalid", path)
        node_id = node.get("nodeId")
        _require(isinstance(node_id, str) and IDENTIFIER.fullmatch(node_id),
                 "nodeId is invalid", "node.id.invalid", f"{path}/nodeId")
        _require(node_id not in nodes, "nodeId is duplicated", "node.id.duplicate",
                 f"{path}/nodeId")
        operation = node.get("operationId")
        version = node.get("semanticVersion")
        _require(isinstance(version, int) and not isinstance(version, bool) and version == 1,
                 "operation semantic version is unsupported", "node.operation.version.unsupported",
                 f"{path}/semanticVersion")
        _require(isinstance(node.get("parameters"), dict), "parameters must be an object",
                 "node.parameters.invalid", f"{path}/parameters")
        layout = node.get("layout")
        _require(isinstance(layout, dict) and set(layout) == {"x", "y"} and
                 all(isinstance(layout[axis], (int, float)) and not isinstance(layout[axis], bool) and
                     math.isfinite(layout[axis])
                     for axis in ("x", "y")), "layout must contain finite numeric x and y",
                 "node.layout.invalid", f"{path}/layout")

        if operation == "artifact.input.texcoord.uv0.vector2":
            _require(set(node) == {"nodeId", "operationId", "semanticVersion", "parameters", "layout"} and
                     node["parameters"] == {}, "UV0 node shape is invalid", "node.shape.invalid", path)
            output_types[(node_id, "value")] = "Vector2"
        elif operation == "artifact.texture.sample.image2d":
            _require(set(node) == {"nodeId", "operationId", "semanticVersion", "parameters",
                                   "resourceReferences", "layout"},
                     "Image Texture node shape is invalid", "node.shape.invalid", path)
            references = node["resourceReferences"]
            _require(isinstance(references, dict) and set(references) == {"image"} and
                     isinstance(references.get("image"), str),
                     "Image Texture node requires one image resource reference",
                     "resource.reference.invalid", f"{path}/resourceReferences")
            resource_id = references["image"]
            _require(resource_id in resources, "node references a missing resourceId",
                     "resource.reference.missing", f"{path}/resourceReferences/image")
            input_types[(node_id, "uv")] = "Vector2"
            output_types[(node_id, "color")] = "Vector4"
            output_types[(node_id, "alpha")] = "Float"
        else:
            raise ContractError("operation is unsupported by the generic resource prototype",
                                "node.operation.unsupported", f"{path}/operationId")
        nodes[node_id] = node
        node_indices[node_id] = index

    writers: dict[tuple[str, str], tuple[str, str]] = {}
    incoming: dict[str, set[str]] = {node_id: set() for node_id in nodes}
    outgoing: dict[str, set[str]] = {node_id: set() for node_id in nodes}
    for index, link in enumerate(links_raw):
        path = f"/links/{index}"
        _require(isinstance(link, dict) and set(link) == {"from", "to"},
                 "link shape is invalid", "link.shape.invalid", path)
        source, target = link["from"], link["to"]
        _require(isinstance(source, dict) and set(source) == {"nodeId", "socketId"} and
                 isinstance(target, dict) and set(target) == {"nodeId", "socketId"},
                 "link endpoints are invalid", "link.endpoint.invalid", path)
        _require(all(isinstance(value, str) for value in
                     (source["nodeId"], source["socketId"], target["nodeId"], target["socketId"])),
                 "link endpoint nodeId and socketId must be strings",
                 "link.endpoint.invalid", path)
        source_key = (source["nodeId"], source["socketId"])
        target_key = (target["nodeId"], target["socketId"])
        _require(source_key in output_types, "link source socket is unavailable",
                 "link.socket.missing", f"{path}/from")
        _require(target_key in input_types, "link target socket is unavailable",
                 "link.socket.missing", f"{path}/to")
        _require(output_types[source_key] == input_types[target_key], "link socket types differ",
                 "link.type.incompatible", path)
        _require(target_key not in writers, "input socket has multiple writers",
                 "link.input.multiple_writers", f"{path}/to")
        writers[target_key] = source_key
        incoming[target_key[0]].add(source_key[0])
        outgoing[source_key[0]].add(target_key[0])

    indegree = {node_id: len(incoming[node_id]) for node_id in nodes}
    ready = [node_id for node_id, count in indegree.items() if count == 0]
    heapq.heapify(ready)
    visited_count = 0
    while ready:
        current = heapq.heappop(ready)
        visited_count += 1
        for successor in sorted(outgoing[current]):
            indegree[successor] -= 1
            if indegree[successor] == 0:
                heapq.heappush(ready, successor)
    _require(visited_count == len(nodes), "document contains a cycle",
             "graph.cycle", "/links")

    for node_id, node in nodes.items():
        if node["operationId"] == "artifact.texture.sample.image2d":
            _require((node_id, "uv") in writers, "Image Texture uv input is unconnected",
                     "node.input.required_unconnected",
                     f"/nodes/{node_indices[node_id]}/inputs/uv")

    selected_output = document.get("output")
    _require(isinstance(selected_output, dict) and set(selected_output) == {"nodeId", "socketId"},
             "output must identify one output socket", "output.invalid", "/output")
    _require(isinstance(selected_output["nodeId"], str) and
             isinstance(selected_output["socketId"], str),
             "output nodeId and socketId must be strings", "output.invalid", "/output")
    output_key = (selected_output["nodeId"], selected_output["socketId"])
    _require(output_key in output_types, "output socket is unavailable",
             "output.socket.missing", "/output")

    active: set[str] = set()
    pending = [output_key[0]]
    while pending:
        current = pending.pop()
        if current in active:
            continue
        active.add(current)
        pending.extend(sorted(incoming[current] - active))

    requirements = []
    asset_resolutions = []
    unresolved = []
    for node_id in sorted(active):
        node = nodes[node_id]
        if node["operationId"] != "artifact.texture.sample.image2d":
            continue
        resource_id = node["resourceReferences"]["image"]
        resource = resources[resource_id]
        resource_key = f"artifact.shadergraph.image.{node_id}"
        catalog_type = canonical_catalog.get(resource["assetId"])
        requirements.append({"resourceKey": resource_key, "resourceId": resource_id,
                             "nodeId": node_id,
                             "resourceKind": resource["resourceKind"]})
        asset_resolutions.append({"resourceKey": resource_key, "resourceId": resource_id,
                                  "assetId": resource["assetId"], "assetType": catalog_type})
        if catalog_type is None:
            unresolved.append(resource_key)
        elif catalog_type != "Image":
            raise ContractError("sampledTexture2D requires an Image asset",
                                "resource.binding.asset_type.incompatible",
                                f"/resources/{resource_id}/source/assetId")

    projection = {
        "documentStatus": "valid",
        "selectedOutput": {"nodeId": output_key[0], "socketId": output_key[1],
                           "valueType": output_types[output_key]},
        "activeNodeIds": sorted(active),
        "resourceRequirements": requirements,
        "assetResolutions": asset_resolutions,
        "assetResolutionStatus": "unresolved" if unresolved else
                                 ("resolved" if requirements else "not-required"),
        "unresolvedResourceKeys": unresolved,
        "resourceBindingStatus": "not-projected",
        "resourceBindingKeySha256": None,
        "runtimeStatus": "synthetic asset catalog lookup only; source revisions, samplers, UV/color binding, asset loading, and GPU sampling are not projected",
    }
    return projection


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--document", type=Path, required=True)
    parser.add_argument("--asset-catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.document.read_text(encoding="utf-8"))
    asset_catalog = json.loads(args.asset_catalog.read_text(encoding="utf-8"))
    try:
        result = project_resource_requirements(document, asset_catalog)
    except ContractError as error:
        result = {"documentStatus": "rejected", "diagnostics": [error.to_json()]}
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8", newline="\n")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
