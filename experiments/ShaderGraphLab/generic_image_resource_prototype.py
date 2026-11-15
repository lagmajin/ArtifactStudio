#!/usr/bin/env python3
"""Validate and adapt the schema-v4 generic Image Texture resource proposal.

This prototype projects active resources and translates the closed fixture into
the data-driven registry compiler's editable node document plus external host
bindings. It emits only a source-level HLSL candidate; it does not invoke a
shader compiler, load images, or create GPU objects.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import heapq
import json
import math
import re
from pathlib import Path
from typing import Any


UUID = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")
FILTERS = {"nearest", "linear"}
MIP_FILTERS = {"none", "nearest", "linear"}
ADDRESS_MODES = {"clamp", "repeat", "mirror"}
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


def _pointer_segment(value: str) -> str:
    return value.replace("~", "~0").replace("/", "~1")


def _canonical_sampler(raw: Any, path: str) -> dict[str, Any]:
    fields = {"minFilter", "magFilter", "mipFilter", "addressU", "addressV", "maxAnisotropy"}
    _require(isinstance(raw, dict) and set(raw) == fields,
             "sampler descriptor has missing or unknown fields", "sampler.shape.invalid", path)
    for field in ("minFilter", "magFilter"):
        _require(isinstance(raw[field], str) and raw[field] in FILTERS,
                 f"{field} is unsupported", "sampler.filter.invalid",
                 f"{path}/{field}")
    _require(isinstance(raw["mipFilter"], str) and raw["mipFilter"] in MIP_FILTERS,
             "mipFilter is unsupported",
             "sampler.filter.invalid", f"{path}/mipFilter")
    for field in ("addressU", "addressV"):
        _require(isinstance(raw[field], str) and raw[field] in ADDRESS_MODES,
                 f"{field} is unsupported",
                 "sampler.address.invalid", f"{path}/{field}")
    anisotropy = raw["maxAnisotropy"]
    _require(isinstance(anisotropy, int) and not isinstance(anisotropy, bool) and
             1 <= anisotropy <= 16, "maxAnisotropy must be an integer from 1 to 16",
             "sampler.anisotropy.invalid", f"{path}/maxAnisotropy")
    return {field: raw[field] for field in sorted(fields)}


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
    required_inputs: set[tuple[str, str]] = set()
    sampling_by_node: dict[str, dict[str, Any]] = {}
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
                                   "resourceReferences", "sampler", "colorInterpretation", "layout"},
                     "Image Texture node shape is invalid", "node.shape.invalid", path)
            references = node["resourceReferences"]
            _require(isinstance(references, dict) and set(references) == {"image"} and
                     isinstance(references.get("image"), str),
                     "Image Texture node requires one image resource reference",
                     "resource.reference.invalid", f"{path}/resourceReferences")
            resource_id = references["image"]
            _require(resource_id in resources, "node references a missing resourceId",
                     "resource.reference.missing", f"{path}/resourceReferences/image")
            sampler = _canonical_sampler(node["sampler"], f"{path}/sampler")
            color_interpretation = node["colorInterpretation"]
            _require(isinstance(color_interpretation, str) and
                     color_interpretation in {"linear", "srgb"},
                     "colorInterpretation must be linear or srgb",
                     "resource.color_interpretation.invalid", f"{path}/colorInterpretation")
            sampling_by_node[node_id] = {"sampler": sampler,
                                         "colorInterpretation": color_interpretation}
            input_types[(node_id, "uv")] = "Vector2"
            required_inputs.add((node_id, "uv"))
            output_types[(node_id, "color")] = "Vector4"
            output_types[(node_id, "alpha")] = "Float"
        elif operation == "artifact.rd.math.add.vector4.float32":
            _require(set(node) == {"nodeId", "operationId", "semanticVersion", "parameters", "layout"} and
                     node["parameters"] == {}, "Vector4 Add node shape is invalid",
                     "node.shape.invalid", path)
            input_types[(node_id, "a")] = "Vector4"
            input_types[(node_id, "b")] = "Vector4"
            required_inputs.update({(node_id, "a"), (node_id, "b")})
            output_types[(node_id, "result")] = "Vector4"
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

    for node_id, socket_id in sorted(required_inputs):
        _require((node_id, socket_id) in writers, f"required input {node_id}.{socket_id} is unconnected",
                 "node.input.required_unconnected",
                 f"/nodes/{node_indices[node_id]}/inputs/{socket_id}")

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

    requirements_json = json.dumps(requirements, sort_keys=True, separators=(",", ":"),
                                   ensure_ascii=False).encode("utf-8")
    projection = {
        "documentStatus": "valid",
        "selectedOutput": {"nodeId": output_key[0], "socketId": output_key[1],
                           "valueType": output_types[output_key]},
        "activeNodeIds": sorted(active),
        "resourceRequirements": requirements,
        "resourceRequirementIdentitySha256": hashlib.sha256(requirements_json).hexdigest().upper(),
        "assetResolutions": asset_resolutions,
        "assetResolutionStatus": "unresolved" if unresolved else
                                 ("resolved" if requirements else "not-required"),
        "unresolvedResourceKeys": unresolved,
        "resourceBindingStatus": "not-projected",
        "resourceBindingKeySha256": None,
        "runtimeStatus": "synthetic asset catalog lookup only; source revisions, samplers, UV/color binding, asset loading, and GPU sampling are not projected",
    }
    return projection


def project_resource_bindings(document: Any, resource_projection: Any,
                              asset_source_states: Any) -> dict[str, Any]:
    """Resolve source revisions and authored sampler/color state into a binding key."""
    _require(isinstance(resource_projection, dict) and
             resource_projection.get("documentStatus") == "valid" and
             isinstance(resource_projection.get("resourceRequirements"), list) and
             isinstance(resource_projection.get("assetResolutions"), list),
             "resource projection is invalid", "resource.projection.invalid",
             "/resourceProjection")
    _require(isinstance(asset_source_states, dict),
             "asset source states must map Asset UUID to source revision",
             "resource.source_states.invalid", "/assetSourceStates")

    canonical_states: dict[str, dict[str, Any]] = {}
    for asset_id, state in asset_source_states.items():
        _require(isinstance(asset_id, str) and UUID.fullmatch(asset_id),
                 "asset source state key must be a canonical UUID string",
                 "resource.source_state.asset_id.invalid", "/assetSourceStates")
        normalized_id = asset_id.lower()
        _require(normalized_id not in canonical_states,
                 "asset source states contain duplicate UUIDs",
                 "resource.source_state.asset_id.duplicate", "/assetSourceStates")
        _require(isinstance(state, dict) and set(state) == {"sourceRevision"},
                 "asset source state shape is invalid", "resource.source_state.shape.invalid",
                 f"/assetSourceStates/{asset_id}")
        revision = state["sourceRevision"]
        _require(isinstance(revision, int) and not isinstance(revision, bool) and revision > 0,
                 "sourceRevision must be a positive integer",
                 "resource.source_revision.invalid",
                 f"/assetSourceStates/{asset_id}/sourceRevision")
        canonical_states[normalized_id] = {"sourceRevision": revision}

    resolutions = {item["resourceKey"]: item
                   for item in resource_projection["assetResolutions"]}
    node_by_id = {node["nodeId"]: node for node in document["nodes"]}
    resolved = []
    unresolved = []
    for requirement in resource_projection["resourceRequirements"]:
        resource_key = requirement["resourceKey"]
        resolution = resolutions.get(resource_key)
        if resolution is None or resolution.get("assetType") is None:
            unresolved.append(resource_key)
            continue
        _require(resolution.get("assetType") == "Image",
                 "sampledTexture2D requires an Image asset",
                 "resource.binding.asset_type.incompatible",
                 f"/assetResolutions/{_pointer_segment(resource_key)}/assetType")
        source_state = canonical_states.get(resolution.get("assetId"))
        if source_state is None:
            unresolved.append(resource_key)
            continue
        node = node_by_id.get(requirement.get("nodeId"))
        _require(isinstance(node, dict), "resource requirement node is missing",
                 "resource.requirement.node_missing", f"/resourceRequirements/{_pointer_segment(resource_key)}")
        sampling = {
            "sampler": _canonical_sampler(node.get("sampler"),
                                          f"/nodes/{_pointer_segment(node['nodeId'])}/sampler"),
            "colorInterpretation": node.get("colorInterpretation"),
        }
        _require(isinstance(sampling["colorInterpretation"], str) and
                 sampling["colorInterpretation"] in {"linear", "srgb"},
                 "colorInterpretation must be linear or srgb",
                 "resource.color_interpretation.invalid",
                 f"/nodes/{_pointer_segment(node['nodeId'])}/colorInterpretation")
        resolved.append({
            "resourceKey": resource_key,
            "resourceId": requirement["resourceId"],
            "nodeId": requirement["nodeId"],
            "assetId": resolution["assetId"],
            "assetType": resolution["assetType"],
            "sourceRevision": source_state["sourceRevision"],
            **sampling,
        })

    if unresolved:
        return {"resourceBindingStatus": "unresolved" if not resolved else "incomplete",
                "resolvedResourceBindings": resolved,
                "unresolvedResourceKeys": sorted(unresolved),
                "resourceBindingKeySha256": None,
                "bindingIdentityFields": ["resourceKey", "resourceId", "assetId", "assetType",
                                          "sourceRevision", "sampler", "colorInterpretation"],
                "runtimeStatus": "logical binding identity only; GPU objects and backend generation are not represented"}
    if not resolved:
        return {"resourceBindingStatus": "not-required", "resolvedResourceBindings": [],
                "unresolvedResourceKeys": [], "resourceBindingKeySha256": None,
                "bindingIdentityFields": ["resourceKey", "resourceId", "assetId", "assetType",
                                          "sourceRevision", "sampler", "colorInterpretation"],
                "runtimeStatus": "no active image requirements"}
    resolved.sort(key=lambda item: item["resourceKey"])
    canonical = json.dumps(resolved, sort_keys=True, separators=(",", ":"),
                           ensure_ascii=False, allow_nan=False).encode("utf-8")
    return {"resourceBindingStatus": "resolved",
            "resolvedResourceBindings": resolved,
            "unresolvedResourceKeys": [],
            "resourceBindingKeySha256": hashlib.sha256(canonical).hexdigest().upper(),
            "bindingIdentityFields": ["resourceKey", "resourceId", "assetId", "assetType",
                                      "sourceRevision", "sampler", "colorInterpretation"],
            "runtimeStatus": "logical binding identity only; GPU objects and backend generation are not represented"}


def adapt_document_to_dynamic_registry(document: dict[str, Any],
                                       resource_projection: dict[str, Any]) -> dict[str, Any]:
    """Translate schema-v4 nodes into a graph with persisted per-node references."""
    operation_map = {
        "artifact.input.texcoord.uv0.vector2": "artifact.input.texcoord.uv0.vector2",
        "artifact.texture.sample.image2d": "artifact.texture.sample.image2d",
        "artifact.rd.math.add.vector4.float32": "artifact.rd.math.add.vector4.float32",
    }
    requirements_by_node = {item["nodeId"]: item
                            for item in resource_projection["resourceRequirements"]}
    dynamic_nodes = []
    for node in document["nodes"]:
        operation_id = node["operationId"]
        _require(operation_id in operation_map,
                 "operation has no dynamic registry adapter",
                 "adapter.operation.unsupported",
                 f"/nodes/{_pointer_segment(node['nodeId'])}/operationId")
        dynamic_operation = operation_map[operation_id]
        dynamic_node = {
            "nodeId": node["nodeId"],
            "operationId": dynamic_operation,
            "semanticVersion": node["semanticVersion"],
            "parameters": {},
            "layout": copy.deepcopy(node["layout"]),
        }
        if operation_id == "artifact.texture.sample.image2d":
            requirement = requirements_by_node.get(node["nodeId"])
            resource_id = node["resourceReferences"]["image"]
            resource_key = (requirement["resourceKey"] if isinstance(requirement, dict)
                            else f"artifact.shadergraph.image.{node['nodeId']}")
            _require(not isinstance(requirement, dict) or
                     requirement.get("resourceId") == resource_id,
                     "active image requirement differs from its document reference",
                     "adapter.resource_requirement.mismatch",
                     f"/nodes/{_pointer_segment(node['nodeId'])}/resourceReferences/image")
            dynamic_node["resourceReference"] = {
                "resourceKey": resource_key,
                "resourceId": resource_id,
                "sampler": copy.deepcopy(node["sampler"]),
                "colorInterpretation": node["colorInterpretation"],
            }
        dynamic_nodes.append(dynamic_node)
    dynamic_document = {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-rd-document",
        "nodes": dynamic_nodes,
        "links": copy.deepcopy(document["links"]),
        "output": copy.deepcopy(document["output"]),
    }
    return dynamic_document


def build_dynamic_resolved_bindings(document: dict[str, Any],
                                    resource_projection: dict[str, Any],
                                    asset_source_states: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Map the generic authored image state into the dynamic resolver's host-binding shape."""
    logical_projection = project_resource_bindings(document, resource_projection,
                                                    asset_source_states)
    resolved = {item["resourceKey"]: item
                for item in logical_projection["resolvedResourceBindings"]}
    asset_resolutions = {item["resourceKey"]: item
                         for item in resource_projection["assetResolutions"]}
    nodes = {item["nodeId"]: item for item in document["nodes"]}
    result = {}
    for requirement in resource_projection["resourceRequirements"]:
        resource_key = requirement["resourceKey"]
        logical = resolved.get(resource_key)
        resolution = asset_resolutions.get(resource_key)
        if logical is None or resolution is None:
            continue
        sampler_bytes = json.dumps(logical["sampler"], sort_keys=True,
                                   separators=(",", ":"), ensure_ascii=False,
                                   allow_nan=False).encode("utf-8")
        node = nodes[requirement["nodeId"]]
        result[resource_key] = {
            "assetId": logical["assetId"],
            "assetType": logical["assetType"],
            "sourceRevision": logical["sourceRevision"],
            "samplerKey": "sampler." + hashlib.sha256(sampler_bytes).hexdigest().lower(),
            "uvSet": 0,
            "uvTransform": {"scaleU": 1.0, "scaleV": 1.0,
                            "rotationDegrees": 0.0, "offsetU": 0.0,
                            "offsetV": 0.0},
            "colorInterpretation": node["colorInterpretation"],
        }
    return result


def build_candidate(document: Any, asset_catalog: dict[str, Any],
                    asset_source_states: dict[str, Any]) -> dict[str, Any]:
    """Build reproducible graph, relink, and runtime binding identity projections."""
    candidate = project_resource_requirements(document, asset_catalog)
    candidate["runtimeBindingProjection"] = project_resource_bindings(
        document, candidate, asset_source_states)

    relinked_document = copy.deepcopy(document)
    previous_asset_id = relinked_document["resources"][0]["source"]["assetId"]
    replacement_asset_id = "22222222-2222-4222-8222-222222222222"
    relinked_document["resources"][0]["source"]["assetId"] = replacement_asset_id
    relinked_catalog = {asset_id: asset_type for asset_id, asset_type in asset_catalog.items()
                        if asset_id.lower() != previous_asset_id.lower()}
    relinked_catalog[replacement_asset_id] = "Image"
    relinked_candidate = project_resource_requirements(relinked_document, relinked_catalog)
    relinked_source_states = {
        asset_id: state for asset_id, state in asset_source_states.items()
        if asset_id.lower() != previous_asset_id.lower()}
    relinked_source_states[replacement_asset_id] = {"sourceRevision": 1}
    relinked_binding = project_resource_bindings(
        relinked_document, relinked_candidate, relinked_source_states)

    revision_changed_states = copy.deepcopy(asset_source_states)
    revision_asset_id = "33333333-3333-4333-8333-333333333333"
    revision_changed_states[revision_asset_id]["sourceRevision"] += 1
    revision_changed_binding = project_resource_bindings(
        document, candidate, revision_changed_states)

    sampler_changed_document = copy.deepcopy(document)
    sampler_changed_document["nodes"][1]["sampler"]["minFilter"] = "nearest"
    sampler_changed_projection = project_resource_requirements(
        sampler_changed_document, asset_catalog)
    sampler_changed_binding = project_resource_bindings(
        sampler_changed_document, sampler_changed_projection, asset_source_states)

    color_changed_document = copy.deepcopy(document)
    color_changed_document["nodes"][1]["colorInterpretation"] = "linear"
    color_changed_projection = project_resource_requirements(
        color_changed_document, asset_catalog)
    color_changed_binding = project_resource_bindings(
        color_changed_document, color_changed_projection, asset_source_states)
    missing_source_binding = project_resource_bindings(document, candidate, {})

    candidate["assetRelinkProjection"] = {
        "previousAssetId": previous_asset_id,
        "replacementAssetId": replacement_asset_id,
        "previousResolution": candidate["assetResolutions"],
        "replacementResolution": relinked_candidate["assetResolutions"],
        "resourceRequirementsUnchanged": candidate["resourceRequirements"] ==
                                         relinked_candidate["resourceRequirements"],
        "resourceRequirementIdentityUnchanged": candidate["resourceRequirementIdentitySha256"] ==
                                                 relinked_candidate["resourceRequirementIdentitySha256"],
        "resourceBindingKeyChanged": candidate["runtimeBindingProjection"]["resourceBindingKeySha256"] !=
                                     relinked_binding["resourceBindingKeySha256"],
        "replacementBindingProjection": relinked_binding,
        "shaderCodeIdentity": "not-measured-no-shader-code-is-generated",
    }
    candidate["bindingIdentityMutationProbes"] = {
        "sourceRevisionChangesBindingKey": candidate["runtimeBindingProjection"]["resourceBindingKeySha256"] !=
                                            revision_changed_binding["resourceBindingKeySha256"],
        "samplerChangePreservesSymbolicRequirementButChangesBindingKey":
            candidate["resourceRequirementIdentitySha256"] ==
            sampler_changed_projection["resourceRequirementIdentitySha256"] and
            candidate["runtimeBindingProjection"]["resourceBindingKeySha256"] !=
            sampler_changed_binding["resourceBindingKeySha256"],
        "colorInterpretationChangePreservesSymbolicRequirementButChangesBindingKey":
            candidate["resourceRequirementIdentitySha256"] ==
            color_changed_projection["resourceRequirementIdentitySha256"] and
            candidate["runtimeBindingProjection"]["resourceBindingKeySha256"] !=
            color_changed_binding["resourceBindingKeySha256"],
        "missingSourceRevisionProducesNoBindingKey":
            missing_source_binding["resourceBindingStatus"] == "unresolved" and
            missing_source_binding["resourceBindingKeySha256"] is None,
        "UVCoordinatesRemainGraphInputs": all(
            {"from": {"nodeId": "uv", "socketId": "value"},
             "to": {"nodeId": node_id, "socketId": "uv"}} in document["links"]
            for node_id in ("image_sample", "roughness_sample")) and
            all("uvSet" not in item and "uvTransform" not in item
                for item in candidate["resourceRequirements"]),
    }
    return candidate


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
