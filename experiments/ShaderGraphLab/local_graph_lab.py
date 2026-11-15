#!/usr/bin/env python3
"""Run the dependency-free local browser UI for ShaderGraphLab."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import sys
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse

import dynamic_graph_prototype as graph


MAX_REQUEST_BYTES = 1_048_576
MAX_RESPONSE_BYTES = 64 * 1024 * 1024
HISTORY_MERGE_WINDOW_SECONDS = 0.8
MAX_HISTORY_COMMANDS = 100
MAX_HISTORY_PAYLOAD_BYTES = 32 * 1024 * 1024
MAX_ASSET_CATALOG_BYTES = 16 * 1024 * 1024
MAX_ASSET_CATALOG_RECORDS = 16384
MAX_ASSET_SOURCE_STATES_BYTES = 4 * 1024 * 1024
ASSET_TYPE_IMAGE = 1
ACTION_FIELDS = {
    "undo": (set(), set()),
    "redo": (set(), set()),
    "save": (set(), set()),
    "reloadDocument": (set(), set()),
    "add": ({"operationId", "semanticVersion", "nodeId"}, {"x", "y"}),
    "connect": ({"fromNode", "fromSocket", "toNode", "toSocket"}, set()),
    "rewire": ({"fromNode", "fromSocket", "toNode", "toSocket"}, set()),
    "disconnect": ({"fromNode", "fromSocket", "toNode", "toSocket"}, set()),
    "disconnectLink": ({"linkIndex", "expectedDocumentHash"}, set()),
    "parameter": ({"nodeId", "values"}, set()),
    "imageResource": ({"nodeId", "resourceId", "assetId"}, {"sampler", "colorInterpretation"}),
    "clearImageResource": ({"nodeId"}, set()),
    "socketDefault": ({"nodeId", "socketId", "value"}, set()),
    "expose": ({"nodeId", "parameterId", "graphParameterId", "label"}, set()),
    "graphParameter": ({"graphParameterId", "value"}, set()),
    "unexpose": ({"nodeId", "parameterId"}, set()),
    "layout": ({"nodeId", "values"}, set()),
    "layoutBatch": ({"edits"}, set()),
    "arrange": (set(), {"nodeIds"}),
    "layoutSelection": ({"nodeIds", "mode"}, set()),
    "output": ({"nodeId", "socketId"}, set()),
    "replace": ({"nodeId", "operationId", "semanticVersion"}, set()),
    "delete": ({"nodeId"}, {"replacementOutput"}),
    "deleteBatch": ({"nodeIds"}, {"replacementOutput"}),
    "duplicateBatch": ({"duplicates"}, set()),
    "copySubgraph": ({"nodeIds"}, set()),
    "pasteSubgraph": ({"newNodeIds"}, {"clipboard", "offsetX", "offsetY"}),
}


def load_asset_catalog_snapshot(path: Path) -> tuple[list[dict[str, str]], str]:
    """Read image choices from AssetDatabase::toJson() without retaining paths."""
    try:
        with path.open("rb") as snapshot_file:
            payload = snapshot_file.read(MAX_ASSET_CATALOG_BYTES + 1)
    except OSError as error:
        raise graph.ContractError(f"cannot read AssetDatabase snapshot: {error}",
                                  code="asset.catalog.read.failed", path="/assetCatalog") from error
    if len(payload) > MAX_ASSET_CATALOG_BYTES:
        raise graph.ContractError("AssetDatabase snapshot exceeds the size limit",
                                  code="asset.catalog.size.invalid", path="/assetCatalog")
    try:
        records = json.loads(payload.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise graph.ContractError("AssetDatabase snapshot is not valid UTF-8 JSON",
                                  code="asset.catalog.json.invalid", path="/assetCatalog") from error
    if not isinstance(records, list) or len(records) > MAX_ASSET_CATALOG_RECORDS:
        raise graph.ContractError("AssetDatabase snapshot must be a bounded array",
                                  code="asset.catalog.shape.invalid", path="/assetCatalog")

    seen_ids: set[str] = set()
    images: list[dict[str, str]] = []
    for index, record in enumerate(records):
        record_path = f"/assetCatalog/{index}"
        if not isinstance(record, dict):
            raise graph.ContractError("AssetDatabase record must be an object",
                                      code="asset.catalog.record.invalid", path=record_path)
        asset_id = record.get("id")
        if isinstance(asset_id, str) and asset_id.startswith("{") and asset_id.endswith("}"):
            asset_id = asset_id[1:-1]
        if not isinstance(asset_id, str) or not graph.ASSET_UUID.fullmatch(asset_id):
            raise graph.ContractError("AssetDatabase record has an invalid UUID",
                                      code="asset.catalog.asset_id.invalid", path=f"{record_path}/id")
        asset_id = asset_id.lower()
        if asset_id in seen_ids:
            raise graph.ContractError("AssetDatabase snapshot contains duplicate UUIDs",
                                      code="asset.catalog.asset_id.duplicate", path=f"{record_path}/id")
        seen_ids.add(asset_id)
        asset_type = record.get("type")
        if isinstance(asset_type, bool) or not isinstance(asset_type, int) or not 0 <= asset_type <= 7:
            raise graph.ContractError("AssetDatabase record has an unsupported type value",
                                      code="asset.catalog.asset_type.invalid", path=f"{record_path}/type")
        if asset_type != ASSET_TYPE_IMAGE:
            continue
        name = record.get("name")
        if not isinstance(name, str) or not name.strip():
            raise graph.ContractError("Image asset must have a display name",
                                      code="asset.catalog.asset_name.invalid", path=f"{record_path}/name")
        images.append({"assetId": asset_id, "name": name.strip(), "assetType": "Image"})
    images.sort(key=lambda item: (item["name"].casefold(), item["assetId"]))
    return images, hashlib.sha256(payload).hexdigest().upper()


def load_asset_source_states(path: Path) -> tuple[dict[str, dict[str, int]], str]:
    """Read the host-exported AssetManager sourceVersion map."""
    try:
        with path.open("rb") as snapshot_file:
            payload = snapshot_file.read(MAX_ASSET_SOURCE_STATES_BYTES + 1)
    except OSError as error:
        raise graph.ContractError(f"cannot read AssetManager source state: {error}",
                                  code="asset.source_states.read.failed",
                                  path="/assetSourceStates") from error
    if len(payload) > MAX_ASSET_SOURCE_STATES_BYTES:
        raise graph.ContractError("AssetManager source state exceeds the size limit",
                                  code="asset.source_states.size.invalid",
                                  path="/assetSourceStates")
    try:
        raw_states = json.loads(payload.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise graph.ContractError("AssetManager source state is not valid UTF-8 JSON",
                                  code="asset.source_states.json.invalid",
                                  path="/assetSourceStates") from error
    if not isinstance(raw_states, dict) or len(raw_states) > MAX_ASSET_CATALOG_RECORDS:
        raise graph.ContractError("AssetManager source state must be a bounded object",
                                  code="asset.source_states.shape.invalid",
                                  path="/assetSourceStates")
    states: dict[str, dict[str, int]] = {}
    for raw_id, state in raw_states.items():
        asset_id = raw_id[1:-1] if raw_id.startswith("{") and raw_id.endswith("}") else raw_id
        if not graph.ASSET_UUID.fullmatch(asset_id):
            raise graph.ContractError("source state has an invalid Asset UUID",
                                      code="asset.source_state.asset_id.invalid",
                                      path=f"/assetSourceStates/{graph._pointer_segment(raw_id)}")
        asset_id = asset_id.lower()
        if asset_id in states:
            raise graph.ContractError("source state contains duplicate normalized Asset UUIDs",
                                      code="asset.source_state.asset_id.duplicate",
                                      path=f"/assetSourceStates/{graph._pointer_segment(raw_id)}")
        if (not isinstance(state, dict) or set(state) != {"sourceRevision"} or
                isinstance(state.get("sourceRevision"), bool) or
                not isinstance(state.get("sourceRevision"), int) or state["sourceRevision"] < 1):
            raise graph.ContractError("sourceRevision must be a positive integer",
                                      code="asset.source_state.revision.invalid",
                                      path=f"/assetSourceStates/{graph._pointer_segment(raw_id)}/sourceRevision")
        states[asset_id] = {"sourceRevision": state["sourceRevision"]}
    return states, hashlib.sha256(payload).hexdigest().upper()


@dataclass
class Session:
    registry: dict[tuple[str, int], graph.Operation]
    document: dict[str, Any]
    document_path: Path | None
    history: list[graph.DocumentCommand]
    asset_catalog: list[dict[str, str]] | None = None
    asset_catalog_sha256: str | None = None
    asset_source_states: dict[str, dict[str, int]] | None = None
    asset_source_states_sha256: str | None = None
    cursor: int = 0
    last_history_action_key: tuple[str, str] | None = None
    last_history_action_time: float = 0.0
    clipboard: dict[str, Any] | None = None
    history_truncated: bool = False
    history_payload_bytes: int = 0
    saved_document_hash: str = ""
    current_document_hash: str = ""
    saved_file_hash: str | None = None
    save_conflict: bool = False

    def __post_init__(self) -> None:
        current_hash = graph._document_snapshot(self.document)[1]
        self.current_document_hash = current_hash
        if not self.saved_document_hash:
            self.saved_document_hash = current_hash

    def state(self) -> dict[str, Any]:
        resolved_bindings = self._resolved_resource_bindings()
        validation = graph.validate_document_for_editing(
            self.document, self.registry, resolved_resource_bindings=resolved_bindings)
        try:
            compilation = graph.compile_document_from_data(
                self.document, self.registry, resolved_resource_bindings=resolved_bindings)
        except graph.ContractError as error:
            compilation = {"status": "unavailable", "diagnostics": [error.to_json()]}
        return {
            "document": graph.preserve_document(self.document),
            "palette": graph.build_palette_model(self.registry),
            "assetCatalog": self.asset_catalog or [],
            "assetCatalogStatus": "snapshot" if self.asset_catalog is not None else "not-provided",
            "assetCatalogSha256": self.asset_catalog_sha256,
            "assetSourceStatesStatus": "snapshot" if self.asset_source_states is not None else "not-provided",
            "assetSourceStatesSha256": self.asset_source_states_sha256,
            "validation": validation,
            "compilation": compilation,
            "history": {
                "canUndo": self.cursor > 0,
                "canRedo": self.cursor < len(self.history),
                "commandCount": len(self.history),
                "payloadBytes": self.history_payload_bytes,
                "truncated": self.history_truncated,
            },
            "documentPath": str(self.document_path) if self.document_path else None,
            "documentHash": self.current_document_hash,
            "isDirty": self.current_document_hash != self.saved_document_hash,
            "saveConflict": self.save_conflict,
            "clipboard": self.clipboard,
        }

    def _resolved_resource_bindings(self) -> dict[str, dict[str, Any]]:
        if self.asset_catalog is None or self.asset_source_states is None:
            return {}
        try:
            candidate = graph.compile_document_from_data(self.document, self.registry)
        except graph.ContractError:
            return {}
        if not candidate.get("resourceRequirements"):
            return {}
        nodes = self.document.get("nodes")
        links = self.document.get("links")
        resources = self.document.get("resources")
        if not isinstance(nodes, list) or not isinstance(links, list) or not isinstance(resources, list):
            return {}
        nodes_by_id = {item.get("nodeId"): item for item in nodes
                       if isinstance(item, dict) and isinstance(item.get("nodeId"), str)}
        resources_by_id = {item.get("resourceId"): item for item in resources
                            if isinstance(item, dict) and isinstance(item.get("resourceId"), str)}
        assets_by_id = {item["assetId"]: item for item in self.asset_catalog}
        bindings: dict[str, dict[str, Any]] = {}

        def uv_set_for(node_id: str, visited: set[str]) -> int | None:
            if node_id in visited:
                return None
            visited.add(node_id)
            node = nodes_by_id.get(node_id)
            if node is None:
                return None
            operation = graph._lookup_operation(
                self.registry, node.get("operationId"), node.get("semanticVersion"))
            if operation is None:
                return None
            if operation.implementation_id == "vector2.uv0":
                return 0
            if operation.implementation_id != "vector2.mapping.scale_offset":
                return None
            incoming = [link for link in links if isinstance(link, dict) and
                        isinstance(link.get("to"), dict) and
                        link["to"].get("nodeId") == node_id and
                        link["to"].get("socketId") == "vector"]
            if len(incoming) != 1 or not isinstance(incoming[0].get("from"), dict):
                return None
            source_id = incoming[0]["from"].get("nodeId")
            return uv_set_for(source_id, visited) if isinstance(source_id, str) else None

        for requirement in candidate["resourceRequirements"]:
            resource_key = requirement.get("resourceKey")
            node = nodes_by_id.get(requirement.get("nodeId"))
            reference = node.get("resourceReference") if isinstance(node, dict) else None
            if (not isinstance(resource_key, str) or not isinstance(reference, dict) or
                    reference.get("resourceKey") != resource_key):
                continue
            resource = resources_by_id.get(reference.get("resourceId"))
            source = resource.get("source") if isinstance(resource, dict) else None
            raw_asset_id = source.get("assetId") if isinstance(source, dict) else None
            asset_id = raw_asset_id.lower() if isinstance(raw_asset_id, str) else None
            asset = assets_by_id.get(asset_id) if asset_id is not None else None
            source_state = self.asset_source_states.get(asset_id) if asset_id is not None else None
            sample_node_id = requirement.get("nodeId")
            uv_links = [link for link in links if isinstance(link, dict) and
                        isinstance(link.get("to"), dict) and
                        link["to"].get("nodeId") == sample_node_id and
                        link["to"].get("socketId") == "uv"]
            uv_source = uv_links[0].get("from") if len(uv_links) == 1 else None
            uv_source_id = uv_source.get("nodeId") if isinstance(uv_source, dict) else None
            sample_uv = uv_set_for(uv_source_id, set()) if isinstance(uv_source_id, str) else None
            if (asset is None or source_state is None or sample_uv is None or
                    asset["assetType"] != "Image"):
                continue
            sampler_bytes = json.dumps(reference.get("sampler"), sort_keys=True,
                                       separators=(",", ":"), ensure_ascii=False,
                                       allow_nan=False).encode("utf-8")
            bindings[resource_key] = {
                "assetId": asset_id,
                "assetType": "Image",
                "sourceRevision": source_state["sourceRevision"],
                "samplerKey": "sampler." + hashlib.sha256(sampler_bytes).hexdigest().lower(),
                "uvSet": sample_uv,
                "uvTransform": {"scaleU": 1.0, "scaleV": 1.0,
                                "rotationDegrees": 0.0, "offsetU": 0.0, "offsetV": 0.0},
                "colorInterpretation": reference.get("colorInterpretation"),
            }
        return bindings

    def compare_disk(self, node_id: str | None = None) -> dict[str, Any]:
        if self.document_path is None:
            raise graph.ContractError("disk comparison requires a --document session",
                                      code="document.compare.not_available", path="/documentPath")
        if node_id is not None and (not isinstance(node_id, str) or not node_id or len(node_id) > 256):
            raise graph.ContractError("node comparison ID is invalid",
                                      code="document.compare.node_id.invalid", path="/nodeId")
        disk_document, disk_file_hash = graph.load_document_snapshot(self.document_path)
        local = graph.preserve_document(self.document)
        if node_id is None and disk_file_hash == self.saved_file_hash:
            self.save_conflict = False
        if node_id is not None:
            def node_records(document: dict[str, Any]) -> list[Any]:
                nodes = document.get("nodes")
                return [node for node in nodes if isinstance(node, dict) and node.get("nodeId") == node_id] if isinstance(nodes, list) else []
            return {"ok": True, "nodeId": node_id, "saveConflict": self.save_conflict,
                    "localMatches": node_records(local), "diskMatches": node_records(disk_document)}
        fields = sorted(set(local) | set(disk_document))
        changed_sections = [key for key in fields
                            if key not in local or key not in disk_document or
                            graph.canonical_json_text(local[key]) != graph.canonical_json_text(disk_document[key])]

        def record_changes(field: str, identity: str) -> dict[str, Any]:
            def index_records(document: dict[str, Any]) -> tuple[dict[str, str], int]:
                records = document.get(field)
                if not isinstance(records, list):
                    return {}, 1 if field in document else 0
                indexed: dict[str, str] = {}
                invalid = 0
                for record in records:
                    record_id = record.get(identity) if isinstance(record, dict) else None
                    if not isinstance(record_id, str):
                        invalid += 1
                    else:
                        if record_id in indexed:
                            invalid += 1
                        indexed[record_id] = graph.canonical_json_text(record)
                return indexed, invalid
            local_records, local_invalid = index_records(local)
            disk_records, disk_invalid = index_records(disk_document)
            local_ids, disk_ids = set(local_records), set(disk_records)
            added = sorted(disk_ids - local_ids)
            removed = sorted(local_ids - disk_ids)
            changed = sorted(node_id for node_id in local_ids & disk_ids
                             if local_records[node_id] != disk_records[node_id])
            all_ids = added + removed + changed
            return {"addedCount": len(added), "removedCount": len(removed),
                    "changedCount": len(changed), "invalidRecordCount": local_invalid + disk_invalid,
                    "sampleIds": all_ids[:100], "sampleTruncated": len(all_ids) > 100}

        def link_counts(document: dict[str, Any]) -> Counter[str]:
            links = document.get("links")
            if not isinstance(links, list):
                return Counter()
            return Counter(graph.canonical_json_text(link) for link in links)

        local_links, disk_links = link_counts(local), link_counts(disk_document)
        added_links = sum((disk_links - local_links).values())
        removed_links = sum((local_links - disk_links).values())
        local_output, disk_output = local.get("output"), disk_document.get("output")
        return {
            "ok": True,
            "saveConflict": self.save_conflict,
            "diskMatchesLoadedVersion": disk_file_hash == self.saved_file_hash,
            "localDocumentHash": self.current_document_hash,
            "diskFileHash": disk_file_hash,
            "changedSections": changed_sections,
            "nodes": record_changes("nodes", "nodeId"),
            "graphParameters": record_changes("graphParameters", "parameterId"),
            "links": {"addedCount": added_links, "removedCount": removed_links,
                      "localCount": sum(local_links.values()), "diskCount": sum(disk_links.values())},
            "outputChanged": ("output" not in local or "output" not in disk_document or
                              graph.canonical_json_text(local_output) != graph.canonical_json_text(disk_output)),
        }

    def apply(self, action: dict[str, Any]) -> None:
        if not isinstance(action, dict) or not isinstance(action.get("type"), str):
            raise graph.ContractError("action must contain a type", code="ui.action.invalid", path="/type")
        graph.validate_json_value(action)
        kind = action["type"]
        fields = ACTION_FIELDS.get(kind)
        if fields is None:
            raise graph.ContractError(f"unsupported UI action {kind}", code="ui.action.unsupported", path="/type")
        required, optional = fields
        unknown = sorted(set(action) - {"type"} - required - optional)
        if unknown:
            raise graph.ContractError("action contains an unknown field", code="ui.action.field.unknown",
                                      path=f"/{graph._pointer_segment(unknown[0])}")
        missing = sorted(required - set(action))
        if missing:
            raise graph.ContractError("action is missing a required field", code="ui.action.field.required",
                                      path=f"/{graph._pointer_segment(missing[0])}")
        if kind == "copySubgraph":
            self.clipboard = graph.copy_nodes_subgraph(self.document, action["nodeIds"], self.registry)
            return
        before = self.document
        if kind == "undo":
            if self.cursor == 0:
                raise graph.ContractError("nothing to undo", code="command.history.empty", path="/history")
            restored = graph.apply_document_command(self.history[self.cursor - 1], before, direction="undo")
            self.document = restored
            self.current_document_hash = self.history[self.cursor - 1].before_hash
            self.cursor -= 1
            self.last_history_action_key = None
            return
        if kind == "redo":
            if self.cursor >= len(self.history):
                raise graph.ContractError("nothing to redo", code="command.history.empty", path="/history")
            restored = graph.apply_document_command(self.history[self.cursor], before, direction="redo")
            self.document = restored
            self.current_document_hash = self.history[self.cursor].after_hash
            self.cursor += 1
            self.last_history_action_key = None
            return
        if kind == "save":
            if self.document_path is None:
                raise graph.ContractError("start with --document to enable save", code="document.path.required", path="/documentPath")
            try:
                self.saved_file_hash = graph.save_document(
                    self.document_path, self.document,
                    expected_existing_sha256=self.saved_file_hash)
            except graph.ContractError as error:
                if error.code == "document.save.conflict":
                    self.save_conflict = True
                raise
            self.saved_document_hash = self.current_document_hash
            self.save_conflict = False
            self.last_history_action_key = None
            return
        if kind == "reloadDocument":
            if self.document_path is None:
                raise graph.ContractError("reload requires a --document session",
                                          code="document.path.required", path="/documentPath")
            if not self.save_conflict:
                raise graph.ContractError("reload is only available after a save conflict",
                                          code="document.reload.not_required", path="/saveConflict")
            restored, file_hash = graph.load_document_snapshot(self.document_path)
            _, document_hash = graph._document_snapshot(restored)
            self.document = restored
            self.current_document_hash = document_hash
            self.saved_document_hash = document_hash
            self.saved_file_hash = file_hash
            self.history = []
            self.cursor = 0
            self.history_payload_bytes = 0
            self.history_truncated = False
            self.clipboard = None
            self.last_history_action_key = None
            self.save_conflict = False
            return
        if kind == "add":
            after = graph.add_node(before, self.registry, action.get("operationId"), action.get("semanticVersion"),
                                  action.get("nodeId"), x=action.get("x", 48), y=action.get("y", 48))
        elif kind == "connect":
            after = graph.connect_nodes(before, self.registry, action.get("fromNode"), action.get("fromSocket"),
                                        action.get("toNode"), action.get("toSocket"))
        elif kind == "rewire":
            after = graph.rewire_input(before, self.registry, action.get("fromNode"), action.get("fromSocket"),
                                       action.get("toNode"), action.get("toSocket"))
        elif kind == "disconnect":
            after = graph.disconnect_nodes(before, action.get("fromNode"), action.get("fromSocket"),
                                           action.get("toNode"), action.get("toSocket"))
        elif kind == "disconnectLink":
            if action.get("expectedDocumentHash") != self.current_document_hash:
                raise graph.ContractError("document changed since this link was displayed; refresh before disconnecting",
                                           code="link.document.stale", path="/expectedDocumentHash")
            after = graph.disconnect_link_at(before, action.get("linkIndex"))
        elif kind == "parameter":
            after = graph.edit_node_parameters(before, self.registry, action.get("nodeId"), action.get("values"))
        elif kind == "imageResource":
            node_id = action.get("nodeId")
            asset_id = action["assetId"]
            if (self.asset_catalog is not None and
                    (not isinstance(asset_id, str) or
                     not any(item["assetId"] == asset_id.lower() for item in self.asset_catalog))):
                raise graph.ContractError("Asset UUID is not an Image in the supplied snapshot",
                                          code="resource.asset.catalog.unavailable", path="/assetId")
            nodes = before.get("nodes")
            if not isinstance(nodes, list):
                raise graph.ContractError("document nodes must be an array",
                                          code="schema.collection.required", path="/nodes")
            node = next((item for item in nodes if isinstance(item, dict) and
                         item.get("nodeId") == node_id), None)
            previous = node.get("resourceReference") if isinstance(node, dict) else None
            reference = dict(previous) if isinstance(previous, dict) else {
                "resourceKey": f"artifact.shadergraph.image.{node_id}",
                "sampler": {
                    "minFilter": "linear", "magFilter": "linear", "mipFilter": "linear",
                    "addressU": "repeat", "addressV": "repeat", "maxAnisotropy": 1,
                },
                "colorInterpretation": "srgb",
            }
            reference["resourceId"] = action["resourceId"]
            if "sampler" in action:
                reference["sampler"] = action["sampler"]
            if "colorInterpretation" in action:
                reference["colorInterpretation"] = action["colorInterpretation"]
            after = graph.set_graph_resource_asset(before, action["resourceId"], asset_id)
            after = graph.edit_node_resource_reference(after, self.registry, node_id, reference)
        elif kind == "clearImageResource":
            after = graph.edit_node_resource_reference(before, self.registry, action.get("nodeId"), None)
        elif kind == "socketDefault":
            after = graph.edit_node_socket_default(before, self.registry, action.get("nodeId"),
                                                   action.get("socketId"), action.get("value"))
        elif kind == "expose":
            after = graph.expose_node_parameter(before, self.registry, action.get("nodeId"),
                                                action.get("parameterId"), action.get("graphParameterId"),
                                                action.get("label"))
        elif kind == "graphParameter":
            after = graph.edit_graph_parameter(before, self.registry,
                                               action.get("graphParameterId"), action.get("value"))
        elif kind == "unexpose":
            after = graph.unexpose_node_parameter(before, self.registry,
                                                  action.get("nodeId"), action.get("parameterId"))
        elif kind == "layout":
            after = graph.edit_node_layout(before, action.get("nodeId"), action.get("values"))
        elif kind == "layoutBatch":
            after = graph.edit_node_layout_batch(before, action.get("edits"))
        elif kind == "arrange":
            after = graph.arrange_nodes_by_flow(before, action.get("nodeIds"))
        elif kind == "layoutSelection":
            after = graph.layout_node_selection(before, action.get("nodeIds"), action.get("mode"))
        elif kind == "output":
            after = graph.set_graph_output(before, self.registry, action.get("nodeId"), action.get("socketId"))
        elif kind == "replace":
            after = graph.replace_node_operation(before, self.registry, action.get("nodeId"),
                                                 action.get("operationId"), action.get("semanticVersion"))
        elif kind == "delete":
            replacement = action.get("replacementOutput")
            if replacement is not None and (not isinstance(replacement, list) or len(replacement) != 2 or
                                            not all(isinstance(value, str) for value in replacement)):
                raise graph.ContractError("replacementOutput must be a [nodeId, socketId] pair",
                                          code="ui.action.field.invalid", path="/replacementOutput")
            after = graph.delete_node(before, self.registry, action.get("nodeId"),
                                      replacement_output=tuple(replacement) if replacement else None)
        elif kind == "deleteBatch":
            replacement = action.get("replacementOutput")
            if replacement is not None and (not isinstance(replacement, list) or len(replacement) != 2 or
                                            not all(isinstance(value, str) for value in replacement)):
                raise graph.ContractError("replacementOutput must be a [nodeId, socketId] pair",
                                          code="ui.action.field.invalid", path="/replacementOutput")
            after = graph.delete_nodes_batch(before, self.registry, action.get("nodeIds"),
                                             replacement_output=tuple(replacement) if replacement else None)
        elif kind == "duplicateBatch":
            after = graph.duplicate_nodes_batch(before, self.registry, action.get("duplicates"))
        elif kind == "pasteSubgraph":
            clipboard = action.get("clipboard") if action.get("clipboard") is not None else self.clipboard
            after = graph.paste_nodes_subgraph(before, self.registry, clipboard, action["newNodeIds"],
                                               offset_x=action.get("offsetX", 32), offset_y=action.get("offsetY", 32))
        else:
            raise graph.ContractError(f"unsupported UI action {kind}", code="ui.action.unsupported", path="/type")
        if after == before:
            self.last_history_action_key = None
            return
        before_nodes, after_nodes = before.get("nodes"), after.get("nodes")
        before_links, after_links = before.get("links"), after.get("links")
        for collection_name, old_items, new_items, limit, code in (
            ("nodes", before_nodes, after_nodes, graph.MAX_GRAPH_NODES, "graph.nodes.limit_exceeded"),
            ("links", before_links, after_links, graph.MAX_GRAPH_LINKS, "graph.links.limit_exceeded"),
        ):
            if (isinstance(old_items, list) and isinstance(new_items, list) and
                    len(new_items) > limit and len(new_items) > len(old_items)):
                raise graph.ContractError(
                    f"edit would grow {collection_name} beyond the prototype limit of {limit}",
                    code=code, path=f"/{collection_name}")
        command = graph.create_document_command(kind.replace("_", " ").title(), before, after)
        action_key = (kind, action.get("graphParameterId")) if kind == "graphParameter" and isinstance(action.get("graphParameterId"), str) else None
        now = time.monotonic()
        updated_history = self.history[:self.cursor]
        updated_cursor = self.cursor
        if (action_key is not None and action_key == self.last_history_action_key and
                self.cursor > 0 and self.cursor == len(self.history) and
                now - self.last_history_action_time <= HISTORY_MERGE_WINDOW_SECONDS):
            updated_history[-1] = graph.compose_document_commands(
                f"Edit Graph Parameter {action_key[1]}", [self.history[-1], command])
        else:
            updated_history.append(command)
            updated_cursor += 1
        payload_bytes = sum(len(item.before_payload.encode("utf-8")) +
                            len(item.after_payload.encode("utf-8")) for item in updated_history)
        if (len(updated_history) > MAX_HISTORY_COMMANDS or
                payload_bytes > MAX_HISTORY_PAYLOAD_BYTES):
            newest = updated_history[-1]
            newest_bytes = len(newest.before_payload.encode("utf-8")) + len(newest.after_payload.encode("utf-8"))
            if newest_bytes > MAX_HISTORY_PAYLOAD_BYTES:
                raise graph.ContractError("edit exceeds the bounded undo history payload limit",
                                          code="command.history.capacity_exceeded", path="/history")
        history_truncated = self.history_truncated
        while len(updated_history) > MAX_HISTORY_COMMANDS or payload_bytes > MAX_HISTORY_PAYLOAD_BYTES:
            discarded = updated_history.pop(0)
            payload_bytes -= len(discarded.before_payload.encode("utf-8")) + len(discarded.after_payload.encode("utf-8"))
            updated_cursor -= 1
            history_truncated = True
        self.history = updated_history
        self.cursor = updated_cursor
        self.history_truncated = history_truncated
        self.history_payload_bytes = payload_bytes
        self.last_history_action_key = action_key
        self.last_history_action_time = now
        self.document = after
        self.current_document_hash = command.after_hash


def make_handler(session: Session, page: bytes):
    class Handler(BaseHTTPRequestHandler):
        server_version = "ShaderGraphLab/0.1"

        def _write(self, status: int, payload: bytes, content_type: str) -> None:
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(payload)

        def _write_json(self, status: int, payload: dict[str, Any]) -> None:
            encoder = json.JSONEncoder(ensure_ascii=False, separators=(",", ":"))
            encoded_size = 0
            try:
                for chunk in encoder.iterencode(payload):
                    encoded_size += len(chunk.encode("utf-8"))
                    if encoded_size > MAX_RESPONSE_BYTES:
                        break
            except UnicodeEncodeError:
                encoded_size = MAX_RESPONSE_BYTES + 1
            if encoded_size > MAX_RESPONSE_BYTES:
                status = 413
                body = json.dumps({"ok": False, "diagnostics": [{
                    "code": "ui.response.size.invalid", "severity": "error", "path": "/",
                    "message": "state response is not UTF-8 encodable or exceeds the configured size limit",
                }]}, separators=(",", ":")).encode("utf-8")
            else:
                body = "".join(encoder.iterencode(payload)).encode("utf-8")
            self._write(status, body, "application/json; charset=utf-8")

        def do_GET(self) -> None:
            route = urlparse(self.path).path
            if route == "/":
                self._write(200, page, "text/html; charset=utf-8")
            elif route == "/api/state":
                self._write_json(200, session.state())
            elif route == "/api/compare":
                try:
                    node_ids = parse_qs(urlparse(self.path).query).get("nodeId", [])
                    if len(node_ids) > 1:
                        raise graph.ContractError("only one node ID may be compared",
                                                  code="document.compare.node_id.invalid", path="/nodeId")
                    self._write_json(200, session.compare_disk(node_ids[0] if node_ids else None))
                except graph.ContractError as error:
                    self._write_json(409, {"ok": False, "diagnostics": [error.to_json()]})
            else:
                self._write(404, b"not found", "text/plain; charset=utf-8")

        def do_POST(self) -> None:
            if urlparse(self.path).path != "/api/action":
                self._write(404, b"not found", "text/plain; charset=utf-8")
                return
            try:
                length = int(self.headers.get("Content-Length", "-1"))
                if length < 0 or length > MAX_REQUEST_BYTES:
                    raise graph.ContractError("request body size is invalid", code="ui.request.size.invalid", path="/")
                raw = self.rfile.read(length).decode("utf-8")
                action = graph.parse_json_text(raw)
                session.apply(action)
                response = {"ok": True, **session.state()}
                status = 200
            except graph.ContractError as error:
                response = {"ok": False, "diagnostics": [error.to_json()], **session.state()}
                status = 400
            except (UnicodeError, ValueError, TypeError) as error:
                diagnostic = {"code": "ui.request.invalid", "severity": "error", "path": "/",
                              "message": str(error)}
                response = {"ok": False, "diagnostics": [diagnostic], **session.state()}
                status = 400
            self._write_json(status, response)

        def log_message(self, format: str, *args: Any) -> None:
            print(f"[ShaderGraphLab] {self.address_string()} {format % args}", file=sys.stderr)

    return Handler


def main() -> int:
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--document", type=Path, help="graph JSON to edit and save")
    parser.add_argument("--registry", type=Path, default=root / "registry.json")
    parser.add_argument("--asset-catalog", type=Path,
                        help="optional AssetDatabase::toJson() snapshot for Image Texture selection")
    parser.add_argument("--asset-source-states", type=Path,
                        help="optional AssetManager sourceVersion map keyed by Asset UUID")
    parser.add_argument("--host", default="127.0.0.1", choices=("127.0.0.1",))
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    registry = graph.load_registry(args.registry)
    if args.asset_catalog:
        try:
            asset_catalog, asset_catalog_sha256 = load_asset_catalog_snapshot(args.asset_catalog.resolve())
        except graph.ContractError as error:
            parser.error(f"{error.code}: {error.message}")
    else:
        asset_catalog, asset_catalog_sha256 = None, None
    if args.asset_source_states:
        if asset_catalog is None:
            parser.error("--asset-source-states requires --asset-catalog")
        try:
            asset_source_states, asset_source_states_sha256 = load_asset_source_states(
                args.asset_source_states.resolve())
        except graph.ContractError as error:
            parser.error(f"{error.code}: {error.message}")
    else:
        asset_source_states, asset_source_states_sha256 = None, None
    document_path = args.document.resolve() if args.document else None
    initial_path = document_path or (root / "dynamic_add.json")
    document, document_file_hash = graph.load_document_snapshot(initial_path)
    page = (root / "local_graph_lab.html").read_bytes()
    session = Session(registry, document, document_path, [], asset_catalog=asset_catalog,
                      asset_catalog_sha256=asset_catalog_sha256,
                      asset_source_states=asset_source_states,
                      asset_source_states_sha256=asset_source_states_sha256,
                      saved_file_hash=document_file_hash)
    server = HTTPServer((args.host, args.port), make_handler(session, page))
    print(f"ShaderGraphLab is available at http://{args.host}:{args.port}/")
    print("Local-only R&D UI; generated HLSL is a candidate string, not a compiled shader.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
