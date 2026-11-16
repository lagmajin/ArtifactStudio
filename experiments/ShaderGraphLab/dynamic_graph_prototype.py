#!/usr/bin/env python3
"""Source-only prototype for data-driven shader node registries and graphs.

The registry is declarative. Executable behavior is selected only from the
closed BUILTIN_IMPLEMENTATIONS table below; a registry file cannot inject HLSL
or Python code.
"""

from __future__ import annotations

import argparse
import copy
import heapq
import hashlib
import json
import math
import os
import re
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable


VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION = {
    "vector4.mix.saturated_factor": "mix",
    "vector4.mixrgb.add.saturated_factor": "add",
    "vector4.mixrgb.multiply.saturated_factor": "multiply",
    "vector4.mixrgb.screen.saturated_factor": "screen",
    "vector4.mixrgb.overlay.saturated_factor": "overlay",
    "vector4.mixrgb.darken.saturated_factor": "darken",
    "vector4.mixrgb.lighten.saturated_factor": "lighten",
    "vector4.mixrgb.difference.saturated_factor": "difference",
}
TEXTURE_SAMPLE_SLOTS = {
    "texture.sample.base_color": ("BaseColor", "g_BaseColorTexture"),
    "texture.sample.opacity": ("Opacity", "g_OpacityTexture"),
    "texture.sample.emission": ("Emission", "g_EmissionTexture"),
    "texture.sample.metallic_roughness": ("MetallicRoughness", "g_MetallicRoughnessTexture"),
    "texture.sample.normal": ("Normal", "g_NormalTexture"),
    "texture.sample.occlusion": ("Occlusion", "g_OcclusionTexture"),
}
GENERIC_TEXTURE_SAMPLE = "texture.sample.image2d"
MULTI_OUTPUT_IMPLEMENTATIONS = {"vector4.separate_channels", "vector4.ramp.linear_stops",
                               *TEXTURE_SAMPLE_SLOTS, GENERIC_TEXTURE_SAMPLE}

BUILTIN_IMPLEMENTATIONS = {
    "float.constant", "vector3.constant", "vector4.constant", "float.add", "float.subtract", "float.multiply",
    "float.divide.positive_floor_1e5", "vector3.add", "vector3.subtract", "vector3.multiply",
    "float.uv0.x", "vector2.uv0", "vector2.mapping.scale_offset",
    "vector4.add", "vector4.subtract", "vector4.multiply", "vector4.divide.positive_floor_1e5",
    "vector4.invert.linear_factor",
    "float.clamp.sorted_bounds",
    *VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION,
    "float.replicate.rgb_alpha_one", "vector4.extract_x",
    "vector4.separate_channels",
    "vector4.ramp.linear_stops", *TEXTURE_SAMPLE_SLOTS, GENERIC_TEXTURE_SAMPLE,
}
MAX_JSON_NESTING = 256
MAX_JSON_FILE_BYTES = 16 * 1024 * 1024
MAX_GRAPH_NODES = 4096
MAX_GRAPH_LINKS = 16384
MAX_GRAPH_RESOURCES = 4096
MIN_RAMP_STOPS = 2
MAX_RAMP_STOPS = 32
SOURCE_CONTEXTS = {
    "float.uv0.x": {
        "contextId": "artifact.material.surface.uv0",
        "component": "x",
        "valueType": "Float",
    },
    "vector2.uv0": {
        "contextId": "artifact.material.surface.uv0",
        "component": "xy",
        "valueType": "Vector2",
    },
}
HLSL_SOURCE_CONTEXT_BINDINGS = {
    ("artifact.material.surface.uv0", "x"): "(In.UV).x",
    ("artifact.material.surface.uv0", "xy"): "In.UV",
}
FORBIDDEN_CODE_FIELDS = {"hlsl", "source", "code", "expression", "python", "shadersource"}
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")
ASSET_UUID = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")


class DuplicateJsonKey(ValueError):
    def __init__(self, key: str) -> None:
        super().__init__(key)
        self.key = key


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateJsonKey(key)
        result[key] = value
    return result


def _reject_nonstandard_constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value} is forbidden")


def parse_json_text(text: str) -> Any:
    if not isinstance(text, str) or len(text) > MAX_JSON_FILE_BYTES:
        raise ContractError("JSON text exceeds the configured size limit",
                            code="json.size.invalid", path="/")
    try:
        return json.loads(text, object_pairs_hook=_object_without_duplicate_keys,
                          parse_constant=_reject_nonstandard_constant)
    except DuplicateJsonKey as error:
        raise ContractError(f"duplicate JSON object key {error.key!r}", code="json.key.duplicate",
                            path="/" + _pointer_segment(error.key)) from error
    except RecursionError as error:
        raise ContractError("JSON nesting exceeds the parser's safe depth", code="json.value.depth",
                            path="/") from error
    except (json.JSONDecodeError, ValueError) as error:
        raise ContractError(f"invalid strict JSON: {error}", code="json.parse.invalid", path="/") from error


def _validate_json_value(value: Any, path: str = "/") -> None:
    """Validate JSON-compatible data without recursing on Python's call stack."""
    active_containers: set[int] = set()
    pending: list[tuple[str, Any, str, int]] = [("visit", value, path, 0)]
    while pending:
        action, current, current_path, depth = pending.pop()
        if action == "leave":
            active_containers.remove(id(current))
            continue
        if current is None or isinstance(current, (str, bool, int)):
            continue
        if isinstance(current, float):
            if not math.isfinite(current):
                raise ContractError("JSON numbers must be finite", code="json.number.non_finite", path=current_path)
            continue
        if isinstance(current, (list, dict)):
            if depth >= MAX_JSON_NESTING:
                raise ContractError(f"JSON nesting exceeds {MAX_JSON_NESTING} levels",
                                    code="json.value.depth", path=current_path)
            identity = id(current)
            if identity in active_containers:
                raise ContractError("cyclic references are not JSON data", code="json.value.cycle", path=current_path)
            active_containers.add(identity)
            pending.append(("leave", current, current_path, depth))
            if isinstance(current, list):
                for index in range(len(current) - 1, -1, -1):
                    pending.append(("visit", current[index], f"{current_path.rstrip('/')}/{index}", depth + 1))
            else:
                keys = list(current)
                for key in reversed(keys):
                    if not isinstance(key, str):
                        raise ContractError("JSON object keys must be strings", code="json.key.invalid", path=current_path)
                    pending.append(("visit", current[key], f"{current_path.rstrip('/')}/{_pointer_segment(key)}", depth + 1))
            continue
        raise ContractError(f"value of type {type(current).__name__} is not JSON data",
                            code="json.value.invalid", path=current_path)


def validate_json_value(value: Any) -> None:
    """Public boundary check for already parsed JSON-compatible values."""
    _validate_json_value(value)


def canonical_json_text(value: Any) -> str:
    _validate_json_value(value)
    try:
        return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False)
    except RecursionError as error:
        raise ContractError("JSON nesting exceeds serializer depth", code="json.value.depth", path="/") from error
    except (TypeError, ValueError) as error:
        raise ContractError(f"value cannot be encoded as strict JSON: {error}", code="json.value.invalid", path="/") from error


def serialize_document(document: Any) -> str:
    return canonical_json_text(document) + "\n"


def _read_json_bytes(path: Path) -> bytes:
    with path.open("rb") as source:
        raw = source.read(MAX_JSON_FILE_BYTES + 1)
    if len(raw) > MAX_JSON_FILE_BYTES:
        raise ContractError(f"JSON file exceeds {MAX_JSON_FILE_BYTES} bytes",
                            code="json.size.invalid", path="/")
    return raw


def _read_json_text(path: Path) -> str:
    return _read_json_bytes(path).decode("utf-8")


def load_document_snapshot(path: Path) -> tuple[dict[str, Any], str]:
    """Load a graph and hash the exact file bytes used to parse it."""
    try:
        raw = _read_json_bytes(path)
        document = parse_json_text(raw.decode("utf-8"))
    except (OSError, UnicodeError) as error:
        raise ContractError(f"cannot read document {path}: {error}", code="document.read.failed", path="/") from error
    _validate_json_value(document)
    if not isinstance(document, dict):
        raise ContractError("document root must be an object", code="document.invalid", path="/")
    return document, hashlib.sha256(raw).hexdigest().upper()


def load_document(path: Path) -> dict[str, Any]:
    """Load strict JSON without discarding unknown graph fields."""
    return load_document_snapshot(path)[0]


def save_document(path: Path, document: Any, *, expected_existing_sha256: str | None = None) -> str:
    """Atomically save canonical document JSON while retaining opaque fields."""
    encoded = serialize_document(document)
    try:
        encoded_bytes = encoded.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ContractError("serialized document is not valid UTF-8",
                            code="document.encoding.invalid", path="/") from error
    if len(encoded_bytes) > MAX_JSON_FILE_BYTES:
        raise ContractError(f"serialized document exceeds {MAX_JSON_FILE_BYTES} bytes",
                            code="document.size.invalid", path="/")
    def verify_existing_file() -> None:
        if expected_existing_sha256 is None:
            return
        try:
            current_hash = hashlib.sha256(_read_json_bytes(path)).hexdigest().upper()
        except (OSError, ContractError) as error:
            raise ContractError("document changed or disappeared since it was loaded; refusing to recreate it",
                                code="document.save.conflict", path="/documentPath") from error
        if current_hash != expected_existing_sha256:
            raise ContractError("document changed on disk since it was loaded; save was not applied",
                                code="document.save.conflict", path="/documentPath")

    verify_existing_file()
    temporary_path = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent,
                                         prefix=f".{path.name}.", suffix=".tmp", delete=False) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(encoded_bytes)
            temporary.flush()
            os.fsync(temporary.fileno())
        verify_existing_file()
        os.replace(temporary_path, path)
    except ContractError:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass
        raise
    except OSError as error:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass
        raise ContractError(f"cannot save document {path}: {error}", code="document.write.failed", path="/") from error
    return hashlib.sha256(encoded_bytes).hexdigest().upper()


class ContractError(ValueError):
    def __init__(
        self,
        message: str,
        *,
        code: str,
        path: str,
    ) -> None:
        super().__init__(message)
        self.code = code
        self.path = path
        self.severity = "error"

    def to_json(self) -> dict[str, str]:
        return {
            "code": self.code,
            "severity": self.severity,
            "path": self.path,
            "message": str(self),
        }


def graph_collection_limit_issues(nodes: list[Any], links: list[Any]) -> list[dict[str, str]]:
    """Return bounded graph-shape diagnostics before indexing node/link records."""
    issues = []
    if len(nodes) > MAX_GRAPH_NODES:
        issues.append({
            "code": "graph.nodes.limit_exceeded", "severity": "error", "path": "/nodes",
            "message": f"document has {len(nodes)} nodes; prototype limit is {MAX_GRAPH_NODES}",
        })
    if len(links) > MAX_GRAPH_LINKS:
        issues.append({
            "code": "graph.links.limit_exceeded", "severity": "error", "path": "/links",
            "message": f"document has {len(links)} links; prototype limit is {MAX_GRAPH_LINKS}",
        })
    return issues


def validate_parameter_block_contracts(parameter_blocks: Any) -> None:
    """Validate logical parameter blocks and reject duplicate stable IDs."""
    if not isinstance(parameter_blocks, list):
        raise ContractError("parameter blocks must be an array", code="shader.binding.contract.invalid",
                            path="/parameterBlocks")
    required_block_fields = {"blockName", "schemaId", "schemaVersion", "bindingId", "resourceKind",
                             "physicalSlot", "physicalSlotStatus", "layoutContract", "byteSize", "fields"}
    seen_binding_ids: set[str] = set()
    seen_block_names: set[str] = set()
    for index, block in enumerate(parameter_blocks):
        path = f"/parameterBlocks/{index}"
        if not isinstance(block, dict) or set(block) != required_block_fields:
            raise ContractError("parameter block shape is invalid",
                                code="shader.binding.contract.invalid", path=path)
        binding_id = block.get("bindingId")
        block_name = block.get("blockName")
        if (not isinstance(binding_id, str) or not binding_id or
                not isinstance(block_name, str) or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", block_name) is None):
            raise ContractError("logical binding identity is invalid",
                                code="shader.binding.contract.invalid", path=path)
        if binding_id in seen_binding_ids:
            raise ContractError("logical parameter binding ID is duplicated",
                                code="shader.binding.logical_id.duplicate", path=f"{path}/bindingId")
        if block_name in seen_block_names:
            raise ContractError("parameter block name is duplicated",
                                code="shader.binding.block_name.duplicate", path=f"{path}/blockName")
        seen_binding_ids.add(binding_id)
        seen_block_names.add(block_name)
        if (block.get("schemaId") != "artifact.shadergraph.parameter-block" or
                isinstance(block.get("schemaVersion"), bool) or block.get("schemaVersion") != 1 or
                block.get("bindingId") != "artifact.shadergraph.parameters" or
                block.get("resourceKind") != "constantBuffer" or
                block.get("physicalSlot") is not None or
                block.get("physicalSlotStatus") != "requires-compiler-reflection" or
                block.get("layoutContract") != "hlsl-cbuffer-packoffset-16byte-v1" or
                isinstance(block.get("byteSize"), bool) or not isinstance(block.get("byteSize"), int) or
                block.get("byteSize") <= 0 or block.get("byteSize") % 16 != 0 or
                not isinstance(block.get("fields"), list)):
            raise ContractError("parameter block does not match the supported schema contract",
                                code="shader.binding.schema.unsupported", path=path)


def resolve_reflected_parameter_block(parameter_block: Any,
                                      reflected_resources: Any) -> dict[str, Any]:
    """Resolve a logical block ID to a compiler-assigned slot fail-closed."""
    if not isinstance(reflected_resources, list):
        raise ContractError("reflection resources have an invalid shape",
                            code="shader.binding.reflection.invalid", path="/reflection/resources")
    validate_parameter_block_contracts([parameter_block])
    names: set[str] = set()
    slots: dict[tuple[str, str], str] = {}
    matches = []
    for index, resource in enumerate(reflected_resources):
        path = f"/reflection/resources/{index}"
        if not isinstance(resource, dict) or set(resource) != {"name", "resourceKind", "physicalSlot"}:
            raise ContractError("reflected resource shape is invalid",
                                code="shader.binding.reflection.invalid", path=path)
        name, resource_kind, physical_slot = (resource["name"], resource["resourceKind"],
                                               resource["physicalSlot"])
        slot_prefix = {"constantBuffer": "b", "texture": "t",
                       "unorderedAccess": "u", "sampler": "s"}.get(resource_kind)
        if (not isinstance(name, str) or not name or name in names or
                slot_prefix is None or not isinstance(physical_slot, str) or
                re.fullmatch(slot_prefix + r"[0-9]+", physical_slot) is None):
            raise ContractError("reflected resource identity or slot is invalid",
                                code="shader.binding.reflection.invalid", path=path)
        names.add(name)
        slot_key = (resource_kind, physical_slot)
        if slot_key in slots:
            raise ContractError("compiler reflection assigned a duplicate resource slot",
                                code="shader.binding.slot_collision", path=path)
        slots[slot_key] = name
        if name == parameter_block["blockName"]:
            if resource_kind != parameter_block["resourceKind"]:
                raise ContractError("compiler reflection changed the parameter block resource kind",
                                    code="shader.binding.reflection.kind_mismatch", path=path)
            matches.append(physical_slot)
    if len(matches) != 1:
        code = "shader.binding.reflection.missing" if not matches else "shader.binding.reflection.duplicate"
        raise ContractError("parameter block must resolve to exactly one reflected resource",
                            code=code, path="/reflection/resources")
    resolved = dict(parameter_block)
    resolved["physicalSlot"] = matches[0]
    resolved["physicalSlotStatus"] = "compiler-reflected"
    return resolved


RESIDENT_GENERIC_PARAMETER_LANE_COUNT = 16
RESIDENT_GENERIC_PARAMETER_LANE_BYTES = 4


def assign_resident_generic_parameter_slots(parameter_bindings: Any) -> list[dict[str, Any]]:
    """Map graph parameters to the existing sixteen-scalar resident ABI slots."""
    if not isinstance(parameter_bindings, list):
        raise ContractError("parameter bindings must be an array",
                            code="shader.binding.resident_contract.invalid", path="/parameterBindings")
    seen_ids: set[str] = set()
    seen_symbols: set[str] = set()
    result = []
    next_slot = 0
    for index, binding in enumerate(parameter_bindings):
        path = f"/parameterBindings/{index}"
        if not isinstance(binding, dict):
            raise ContractError("parameter binding record is invalid",
                                code="shader.binding.resident_contract.invalid", path=path)
        parameter_id = binding.get("parameterId")
        value_type = binding.get("valueType")
        symbol = binding.get("hlslSymbol")
        if not isinstance(parameter_id, str) or not IDENTIFIER.fullmatch(parameter_id):
            raise ContractError("resident parameter binding is invalid or duplicated",
                                code="shader.binding.resident_contract.invalid", path=path)
        if parameter_id in seen_ids:
            raise ContractError("resident parameter ID is duplicated",
                                code="shader.binding.logical_id.duplicate", path=path)
        expected_symbol = "P_" + re.sub(r"[^A-Za-z0-9_]", "_", parameter_id)
        if (not isinstance(value_type, str) or value_type not in {"Float32", "Vector3F32", "Vector4F32"} or
                symbol != expected_symbol or binding.get("blockBindingId") != "artifact.shadergraph.parameters" or
                not isinstance(binding.get("bindings"), list) or not binding["bindings"] or
                symbol in seen_symbols):
            raise ContractError("resident parameter binding is invalid or duplicated",
                                code="shader.binding.resident_contract.invalid", path=path)
        slot_count = {"Float32": 1, "Vector3F32": 3, "Vector4F32": 4}[value_type]
        if next_slot + slot_count > RESIDENT_GENERIC_PARAMETER_LANE_COUNT:
            raise ContractError("resident generic ABI supports at most sixteen scalar lanes",
                            code="shader.binding.resident_slot_overflow", path="/parameterBindings")
        seen_ids.add(parameter_id)
        seen_symbols.add(symbol)
        slot_indices = list(range(next_slot, next_slot + slot_count))
        abi_names = [f"g_P{slot}" for slot in slot_indices]
        hlsl_expression = (abi_names[0] if slot_count == 1 else
                           f"float{slot_count}(" + ", ".join(abi_names) + ")")
        result.append({
            "parameterId": parameter_id,
            "valueType": value_type,
            "hlslSymbol": symbol,
            "bindingId": binding.get("blockBindingId"),
            "slotIndex": next_slot,
            "slotCount": slot_count,
            "slotIndices": slot_indices,
            "abiNames": abi_names,
            "hlslExpression": hlsl_expression,
            "bindings": binding.get("bindings"),
        })
        next_slot += slot_count
    return result


def pack_resident_generic_parameter_values(compile_result: Any) -> dict[str, Any]:
    """Pack canonical graph values into the current sixteen-float resident ABI."""
    if not isinstance(compile_result, dict) or compile_result.get("status") != "candidate-lowered-no-shader-runtime":
        raise ContractError("compile result is not a supported candidate",
                            code="shader.binding.resident_contract.invalid", path="/compileResult")
    slots = assign_resident_generic_parameter_slots(compile_result.get("parameterBindings"))
    values = compile_result.get("parameterValues")
    if not isinstance(values, list):
        raise ContractError("parameter values must be an array",
                            code="shader.binding.parameter_values.invalid", path="/parameterValues")
    by_id: dict[str, dict[str, Any]] = {}
    for index, record in enumerate(values):
        path = f"/parameterValues/{index}"
        if (not isinstance(record, dict) or set(record) != {"parameterId", "valueType", "value"} or
                not isinstance(record.get("parameterId"), str) or record["parameterId"] in by_id):
            raise ContractError("parameter value record is invalid or duplicated",
                                code="shader.binding.parameter_values.invalid", path=path)
        by_id[record["parameterId"]] = record
    if set(by_id) != {slot["parameterId"] for slot in slots}:
        raise ContractError("parameter values do not match parameter bindings",
                            code="shader.binding.parameter_values.invalid", path="/parameterValues")
    canonical_values = []
    binding_by_id = {slot["parameterId"]: slot for slot in slots}
    lanes = [0.0] * RESIDENT_GENERIC_PARAMETER_LANE_COUNT
    lane_owners: list[str | None] = [None] * RESIDENT_GENERIC_PARAMETER_LANE_COUNT
    for parameter_id in sorted(by_id):
        record = by_id[parameter_id]
        slot = binding_by_id.get(parameter_id)
        if slot is None:
            raise ContractError("parameter value has no resident ABI binding",
                                code="shader.binding.parameter_values.invalid",
                                path=f"/parameterValues/{parameter_id}")
        if record["valueType"] != slot["valueType"]:
            raise ContractError("parameter value type does not match its binding",
                                code="shader.binding.parameter_type.mismatch",
                                path=f"/parameterValues/{parameter_id}/valueType")
        value = _canonical_graph_parameter_value(
            record["valueType"], record["value"], f"/parameterValues/{parameter_id}/value")
        components = [value] if isinstance(value, float) else value
        if len(components) != slot["slotCount"]:
            raise ContractError("parameter component count does not match its resident ABI slots",
                                code="shader.binding.parameter_values.invalid",
                                path=f"/parameterValues/{parameter_id}/value")
        for lane, component in zip(slot["slotIndices"], components):
            if lane_owners[lane] is not None:
                raise ContractError("resident parameter lanes overlap",
                                    code="shader.binding.resident_contract.invalid", path="/parameterBindings")
            lanes[lane] = component
            lane_owners[lane] = parameter_id
        canonical_values.append({"parameterId": parameter_id, "valueType": record["valueType"], "value": value})
    for index, binding in enumerate(compile_result.get("parameterBindings")):
        parameter_id = binding.get("parameterId") if isinstance(binding, dict) else None
        if index and isinstance(parameter_id, str) and parameter_id <= compile_result["parameterBindings"][index - 1].get("parameterId", ""):
            raise ContractError("parameter bindings must use stable parameter-ID order",
                                code="shader.binding.parameter_order.invalid",
                                path=f"/parameterBindings/{index}/parameterId")
    canonical = json.dumps(canonical_values, sort_keys=True, separators=(",", ":"),
                           ensure_ascii=False).encode("utf-8")
    value_hash = hashlib.sha256(canonical).hexdigest().upper()
    if value_hash != compile_result.get("parameterValuesHashSha256"):
        raise ContractError("parameter values do not match the compile result hash",
                            code="shader.binding.parameter_values.hash_mismatch",
                            path="/parameterValuesHashSha256")
    return {"targetAbi": "artifact.resident-generic-params.scalar16.v1",
            "laneCount": RESIDENT_GENERIC_PARAMETER_LANE_COUNT,
            "parameterBytes": RESIDENT_GENERIC_PARAMETER_LANE_COUNT * RESIDENT_GENERIC_PARAMETER_LANE_BYTES,
            "lanes": lanes, "laneOwners": lane_owners,
            "activeLaneCount": sum(owner is not None for owner in lane_owners),
            "parameterValuesHashSha256": value_hash}


def adapt_to_resident_generic_abi(compile_result: Any) -> dict[str, Any]:
    """Lower the candidate block onto ResidentGenericParams g_P0..g_P15."""
    if not isinstance(compile_result, dict) or compile_result.get("status") != "candidate-lowered-no-shader-runtime":
        raise ContractError("compile result is not a supported candidate",
                            code="shader.binding.resident_contract.invalid", path="/compileResult")
    bindings = compile_result.get("parameterBindings")
    slots = assign_resident_generic_parameter_slots(bindings)
    packed_values = pack_resident_generic_parameter_values(compile_result)
    candidate_hlsl = compile_result.get("hlslBody")
    if not isinstance(candidate_hlsl, str):
        raise ContractError("candidate HLSL body is missing",
                            code="shader.binding.resident_contract.invalid", path="/hlslBody")
    block = compile_result.get("parameterBlock")
    body = candidate_hlsl
    if slots:
        if not isinstance(block, dict) or not isinstance(block.get("blockName"), str):
            raise ContractError("candidate parameter block metadata is missing",
                                code="shader.binding.resident_contract.invalid", path="/parameterBlock")
        prefix = f"cbuffer {block['blockName']} {{\n"
        if not body.startswith(prefix):
            raise ContractError("candidate HLSL parameter block does not match its metadata",
                                code="shader.binding.resident_contract.invalid", path="/hlslBody")
        _, separator, body = body.partition("};\n")
        if not separator:
            raise ContractError("candidate HLSL parameter block is incomplete",
                                code="shader.binding.resident_contract.invalid", path="/hlslBody")
    for slot in slots:
        body, substitutions = re.subn(r"\b" + re.escape(slot["hlslSymbol"]) + r"\b",
                                      slot["hlslExpression"], body)
        if substitutions == 0:
            raise ContractError("candidate HLSL does not reference a declared graph parameter",
                                code="shader.binding.resident_symbol.unused",
                                path=f"/parameterBindings/{slot['slotIndex']}/hlslSymbol")
    adapter_structure = {
        "targetAbi": "artifact.resident-generic-params.scalar16.v1",
        "parameterBindings": [
            {key: slot[key] for key in ("parameterId", "valueType", "bindingId",
                                        "slotIndex", "slotCount", "slotIndices", "abiNames",
                                        "hlslExpression", "bindings")}
            for slot in slots
        ],
        "hlslBody": body,
    }
    structure_bytes = json.dumps(adapter_structure, sort_keys=True, separators=(",", ":"),
                                 ensure_ascii=False).encode("utf-8")
    return {
        "status": "candidate-lowered-no-shader-runtime",
        "targetAbi": "artifact.resident-generic-params.scalar16.v1",
        "semanticHashSha256": compile_result.get("semanticHashSha256"),
        "shaderStructureHashSha256": hashlib.sha256(structure_bytes).hexdigest().upper(),
        "parameterValuesHashSha256": compile_result.get("parameterValuesHashSha256"),
        "packedParameterValues": packed_values,
        "parameterBindings": slots,
        "hlslBody": body,
        "limitations": [
            "g_P0..g_P15 are supplied as scalar floats by the Artifact resident prelude and are not declared in this body",
            "this adapter is source-only; no shader compiler, renderer, or runtime binding is invoked",
        ],
    }


def _pointer_segment(value: str) -> str:
    return value.replace("~", "~0").replace("/", "~1")


@dataclass(frozen=True)
class Socket:
    socket_id: str
    value_type: str
    required: bool = False
    default: Any = None


@dataclass(frozen=True)
class Parameter:
    parameter_id: str
    value_type: str
    default: Any


@dataclass(frozen=True)
class Operation:
    operation_id: str
    semantic_version: int
    display_name: str
    category: str
    implementation_id: str
    inputs: tuple[Socket, ...]
    outputs: tuple[Socket, ...]
    parameters: tuple[Parameter, ...]


@dataclass(frozen=True)
class DocumentCommand:
    label: str
    before_payload: str
    after_payload: str
    before_hash: str
    after_hash: str


def _document_snapshot(document: Any) -> tuple[str, str]:
    detached = preserve_document(document)
    payload = canonical_json_text(detached)
    return payload, hashlib.sha256(payload.encode("utf-8")).hexdigest().upper()


def create_document_command(label: str, before: Any, after: Any) -> DocumentCommand:
    if not isinstance(label, str) or not label.strip():
        raise ContractError("command label must be non-empty", code="command.label.invalid", path="/label")
    before_payload, before_hash = _document_snapshot(before)
    after_payload, after_hash = _document_snapshot(after)
    if len(before_payload) > MAX_JSON_FILE_BYTES or len(after_payload) > MAX_JSON_FILE_BYTES:
        raise ContractError("undo snapshot exceeds the configured JSON size limit",
                            code="command.history.snapshot_size_exceeded", path="/history")
    if before_hash == after_hash:
        raise ContractError("command has no document change", code="command.noop", path="/")
    return DocumentCommand(label.strip(), before_payload, after_payload, before_hash, after_hash)


def _validated_command_snapshots(command: Any) -> tuple[dict[str, Any], dict[str, Any]]:
    if not isinstance(command, DocumentCommand):
        raise ContractError("history entry must be a DocumentCommand",
                            code="command.invalid", path="/command")
    snapshots = []
    for direction, payload, expected_hash in (
            ("before", command.before_payload, command.before_hash),
            ("after", command.after_payload, command.after_hash)):
        if not isinstance(payload, str) or not isinstance(expected_hash, str):
            raise ContractError("command payload and hash must be strings",
                                code="command.snapshot.invalid", path=f"/{direction}")
        try:
            document = parse_json_text(payload)
        except ContractError as error:
            raise ContractError("command snapshot is not valid strict JSON",
                                code="command.snapshot.invalid", path=f"/{direction}") from error
        if not isinstance(document, dict) or canonical_json_text(document) != payload:
            raise ContractError("command snapshot is not a canonical document object",
                                code="command.snapshot.invalid", path=f"/{direction}")
        actual_hash = hashlib.sha256(payload.encode("utf-8")).hexdigest().upper()
        if expected_hash != actual_hash:
            raise ContractError("command snapshot hash does not match its payload",
                                code="command.snapshot.hash_mismatch", path=f"/{direction}Hash")
        snapshots.append(document)
    return snapshots[0], snapshots[1]


def apply_document_command(command: DocumentCommand, current: Any, *, direction: str) -> dict[str, Any]:
    if direction not in {"undo", "redo"}:
        raise ContractError("direction must be undo or redo", code="command.direction.invalid", path="/direction")
    before, after = _validated_command_snapshots(command)
    _, current_hash = _document_snapshot(current)
    expected_hash = command.after_hash if direction == "undo" else command.before_hash
    if current_hash != expected_hash:
        raise ContractError("document no longer matches command history state", code="command.history.stale", path="/")
    return before if direction == "undo" else after


def apply_document_batch(document: Any, edits: list[Callable[[dict[str, Any]], dict[str, Any]]]) -> dict[str, Any]:
    """Apply several document edits atomically to a detached working copy."""
    if not isinstance(edits, list) or not edits:
        raise ContractError("edit batch must be a non-empty list", code="command.batch.invalid", path="/edits")
    working = preserve_document(document)
    for index, edit in enumerate(edits):
        if not callable(edit):
            raise ContractError("batch entry must be callable", code="command.batch.entry.invalid", path=f"/edits/{index}")
        candidate = edit(working)
        if not isinstance(candidate, dict):
            raise ContractError("edit must return a document object", code="command.batch.result.invalid", path=f"/edits/{index}")
        working = preserve_document(candidate)
    return working


def create_document_transaction(label: str, document: Any,
                                edits: list[Callable[[dict[str, Any]], dict[str, Any]]]) -> DocumentCommand:
    after = apply_document_batch(document, edits)
    return create_document_command(label, document, after)


def compose_document_commands(label: str, commands: list[DocumentCommand]) -> DocumentCommand:
    """Group adjacent history entries into one undo unit."""
    if not isinstance(commands, list) or not commands:
        raise ContractError("transaction requires at least one command", code="command.transaction.invalid", path="/commands")
    snapshots = [_validated_command_snapshots(command) for command in commands]
    for index, (left, right) in enumerate(zip(commands, commands[1:])):
        if left.after_hash != right.before_hash:
            raise ContractError("commands are not a contiguous history sequence", code="command.transaction.discontinuous", path=f"/commands/{index + 1}")
    return create_document_command(label, snapshots[0][0], snapshots[-1][1])


def _reject_code_payload(value: Any, path: tuple[str, ...] = ()) -> None:
    pending: list[tuple[Any, tuple[str, ...]]] = [(value, path)]
    while pending:
        current, current_path = pending.pop()
        if isinstance(current, dict):
            for key, child in current.items():
                child_path = (*current_path, str(key))
                if isinstance(key, str) and key.lower() in FORBIDDEN_CODE_FIELDS:
                    project_asset_source = (
                        key.lower() == "source" and len(child_path) >= 3 and
                        child_path[-3].lower() == "resources" and
                        child_path[-2].isdigit())
                    if project_asset_source:
                        pending.append((child, child_path))
                        continue
                    json_path = "/" + "/".join(_pointer_segment(segment) for segment in child_path)
                    raise ContractError(
                        "executable/source payload field is forbidden",
                        code="registry.payload.executable_forbidden",
                        path=json_path,
                    )
                pending.append((child, child_path))
        elif isinstance(current, list):
            pending.extend((child, (*current_path, str(index)))
                           for index, child in enumerate(current))


def reject_code_payload(value: Any) -> None:
    _reject_code_payload(value)


def _load_json(path: Path) -> Any:
    try:
        value = parse_json_text(_read_json_text(path))
        _validate_json_value(value)
        return value
    except (OSError, UnicodeError) as error:
        raise ContractError(
            f"cannot read {path}: {error}",
            code="json.parse.invalid",
            path="/",
        ) from error


def _f32(value: Any, label: str, *, path: str = "/", code: str = "value.float32.invalid") -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ContractError(f"{label} must be a JSON number", code=code, path=path)
    try:
        number = float(value)
    except OverflowError as error:
        raise ContractError(f"{label} is outside finite binary32", code=code, path=path) from error
    if not math.isfinite(number):
        raise ContractError(f"{label} must be finite", code=code, path=path)
    try:
        result = struct.unpack("<f", struct.pack("<f", number))[0]
    except OverflowError as error:
        raise ContractError(f"{label} is outside finite binary32", code=code, path=path) from error
    if not math.isfinite(result):
        raise ContractError(f"{label} is outside finite binary32", code=code, path=path)
    return 0.0 if result == 0.0 else result


def _canonical_ramp_stops(value: Any, path: str, *, code: str) -> list[dict[str, Any]]:
    if (not isinstance(value, list) or
            not MIN_RAMP_STOPS <= len(value) <= MAX_RAMP_STOPS):
        raise ContractError(
            f"ramp stops must contain {MIN_RAMP_STOPS} to {MAX_RAMP_STOPS} entries",
            code=code, path=path)
    stops = []
    for index, stop in enumerate(value):
        stop_path = f"{path}/{index}"
        if not isinstance(stop, dict) or set(stop) != {"position", "color"}:
            raise ContractError("ramp stop must contain position and color",
                                code=code, path=stop_path)
        position = _f32(stop.get("position"), "ramp stop position",
                        path=f"{stop_path}/position", code=code)
        color = stop.get("color")
        if not isinstance(color, list) or len(color) != 4:
            raise ContractError("ramp stop color must contain four Float32 lanes",
                                code=code, path=f"{stop_path}/color")
        stops.append({
            "position": position,
            "color": [_f32(component, f"ramp stop color[{lane}]",
                            path=f"{stop_path}/color/{lane}", code=code)
                      for lane, component in enumerate(color)],
        })
    return stops


def load_registry(path: Path) -> dict[tuple[str, int], Operation]:
    document = _load_json(path)
    _reject_code_payload(document)
    if (not isinstance(document, dict) or isinstance(document.get("schemaVersion"), bool) or
            not isinstance(document.get("schemaVersion"), int) or document.get("schemaVersion") != 1):
        raise ContractError("registry schemaVersion must be 1", code="schema.version.unsupported", path="/schemaVersion")
    if document.get("kind") != "artifact-shader-graph-rd-operation-registry":
        raise ContractError("registry kind is unsupported", code="document.kind.unsupported", path="/kind")
    if set(document) != {"schemaVersion", "kind", "operations"}:
        unknown = sorted(set(document) - {"schemaVersion", "kind", "operations"})
        raise ContractError("registry contains an unknown top-level field", code="schema.field.unknown", path="/" + _pointer_segment(str(unknown[0])) if unknown else "/")
    raw_operations = document.get("operations")
    if not isinstance(raw_operations, list):
        raise ContractError("registry operations must be an array", code="schema.collection.required", path="/operations")

    registry: dict[tuple[str, int], Operation] = {}
    for index, raw in enumerate(raw_operations):
        path_label = f"registry.operations[{index}]"
        path = f"/operations/{index}"
        if not isinstance(raw, dict):
            raise ContractError(f"{path_label} must be an object", code="registry.operation.invalid", path=path)
        if set(raw) != {"operationId", "semanticVersion", "displayName", "category", "implementationId", "inputs", "outputs", "parameters"}:
            raise ContractError(f"{path_label} has missing or unknown fields", code="registry.operation.shape.invalid", path=path)
        operation_id = raw.get("operationId")
        implementation_id = raw.get("implementationId")
        version = raw.get("semanticVersion")
        if not isinstance(operation_id, str) or not IDENTIFIER.fullmatch(operation_id):
            raise ContractError(f"{path_label}.operationId is invalid", code="operation.id.invalid", path=f"{path}/operationId")
        if isinstance(version, bool) or not isinstance(version, int) or version < 1:
            raise ContractError(f"{path_label}.semanticVersion must be a positive integer", code="operation.version.invalid", path=f"{path}/semanticVersion")
        registry_key = (operation_id, version)
        if registry_key in registry:
            raise ContractError(f"duplicate operation registration {operation_id}@{version}", code="operation.registration.duplicate", path=path)
        if not isinstance(implementation_id, str) or implementation_id not in BUILTIN_IMPLEMENTATIONS:
            raise ContractError(f"{path_label}.implementationId is not implemented by this prototype", code="operation.implementation.unsupported", path=f"{path}/implementationId")
        display_name, category = raw.get("displayName"), raw.get("category")
        if not isinstance(display_name, str) or not display_name.strip():
            raise ContractError(f"{path_label}.displayName must be a non-empty string", code="operation.display_name.invalid", path=f"{path}/displayName")
        if not isinstance(category, str) or not category.strip():
            raise ContractError(f"{path_label}.category must be a non-empty string", code="operation.category.invalid", path=f"{path}/category")

        def parse_sockets(key: str, is_input: bool) -> tuple[Socket, ...]:
            values = raw.get(key)
            if not isinstance(values, list):
                raise ContractError(f"{path_label}.{key} must be an array", code="schema.collection.required", path=f"{path}/{key}")
            parsed = []
            seen: set[str] = set()
            for socket_index, socket in enumerate(values):
                socket_path = f"{path_label}.{key}[{socket_index}]"
                socket_pointer = f"{path}/{key}/{socket_index}"
                if not isinstance(socket, dict):
                    raise ContractError(f"{socket_path} must be an object", code="registry.socket.invalid", path=socket_pointer)
                allowed_socket_fields = {"socketId", "valueType", "required", "default"} if is_input else {"socketId", "valueType"}
                if set(socket) - allowed_socket_fields or not {"socketId", "valueType"} <= set(socket):
                    raise ContractError(f"{socket_path} has missing or unknown fields", code="registry.socket.shape.invalid", path=socket_pointer)
                socket_id = socket.get("socketId")
                value_type = socket.get("valueType")
                if not isinstance(socket_id, str) or not IDENTIFIER.fullmatch(socket_id):
                    raise ContractError(f"{socket_path}.socketId is invalid", code="socket.id.invalid", path=f"{socket_pointer}/socketId")
                if socket_id in seen:
                    raise ContractError(f"duplicate socketId {socket_id} in {path_label}.{key}", code="socket.id.duplicate", path=f"{socket_pointer}/socketId")
                seen.add(socket_id)
                if not isinstance(value_type, str) or value_type not in {"Float", "Vector2", "Vector3", "Vector4"}:
                    raise ContractError(f"{socket_path}.valueType is unsupported by this prototype", code="socket.type.unsupported", path=f"{socket_pointer}/valueType")
                required = socket.get("required", False) if is_input else False
                if not isinstance(required, bool):
                    raise ContractError(f"{socket_path}.required must be boolean", code="socket.required.invalid", path=f"{socket_pointer}/required")
                default = None
                if is_input and not required:
                    if "default" not in socket:
                        raise ContractError(f"{socket_path} optional input requires a typed default", code="socket.default.required", path=f"{socket_pointer}/default")
                    default_path = f"{socket_pointer}/default"
                    if value_type == "Float":
                        default = _f32(socket["default"], f"{socket_path}.default",
                                      path=default_path, code="socket.default.invalid")
                    elif value_type == "Vector4":
                        raw_default = socket["default"]
                        if not isinstance(raw_default, list) or len(raw_default) != 4:
                            raise ContractError(f"{socket_path}.default must contain four Float32 lanes", code="socket.default.invalid", path=default_path)
                        default = [_f32(value, f"{socket_path}.default[{lane}]",
                                        path=f"{default_path}/{lane}", code="socket.default.invalid")
                                   for lane, value in enumerate(raw_default)]
                    elif value_type == "Vector3":
                        raw_default = socket["default"]
                        if not isinstance(raw_default, list) or len(raw_default) != 3:
                            raise ContractError(f"{socket_path}.default must contain three Float32 lanes", code="socket.default.invalid", path=default_path)
                        default = [_f32(value, f"{socket_path}.default[{lane}]",
                                        path=f"{default_path}/{lane}", code="socket.default.invalid")
                                   for lane, value in enumerate(raw_default)]
                    else:
                        raise ContractError(f"{socket_path}.default has no Vector2 canonical representation",
                                            code="socket.default.type.unsupported", path=default_path)
                elif is_input and "default" in socket:
                    raise ContractError(f"{socket_path} required input cannot declare an implicit default", code="socket.default.unused", path=f"{socket_pointer}/default")
                parsed.append(Socket(socket_id, value_type, required, default))
            return tuple(parsed)

        raw_parameters = raw.get("parameters")
        if not isinstance(raw_parameters, list):
            raise ContractError(f"{path_label}.parameters must be an array", code="schema.collection.required", path=f"{path}/parameters")
        parameters = []
        parameter_ids: set[str] = set()
        for parameter_index, parameter in enumerate(raw_parameters):
            parameter_path = f"{path_label}.parameters[{parameter_index}]"
            parameter_pointer = f"{path}/parameters/{parameter_index}"
            if not isinstance(parameter, dict):
                raise ContractError(f"{parameter_path} must be an object", code="registry.parameter.invalid", path=parameter_pointer)
            if set(parameter) - {"parameterId", "valueType", "default"} or not {"parameterId", "valueType"} <= set(parameter):
                raise ContractError(f"{parameter_path} has missing or unknown fields", code="registry.parameter.shape.invalid", path=parameter_pointer)
            parameter_id = parameter.get("parameterId")
            if not isinstance(parameter_id, str) or not IDENTIFIER.fullmatch(parameter_id):
                raise ContractError(f"{parameter_path}.parameterId is invalid", code="parameter.id.invalid", path=f"{parameter_pointer}/parameterId")
            if parameter_id in parameter_ids:
                raise ContractError(f"duplicate parameterId {parameter_id} in {path_label}", code="parameter.id.duplicate", path=f"{parameter_pointer}/parameterId")
            parameter_ids.add(parameter_id)
            parameter_type = parameter.get("valueType")
            if not isinstance(parameter_type, str) or parameter_type not in {"Float32", "Vector3F32", "Vector4F32", "RampStops"}:
                raise ContractError(f"{parameter_path}.valueType is unsupported", code="parameter.type.unsupported", path=f"{parameter_pointer}/valueType")
            default = parameter.get("default")
            default_path = f"/operations/{index}/parameters/{parameter_index}/default"
            if default is None:
                canonical_default = None
            elif parameter_type == "Float32":
                canonical_default = _f32(default, f"{parameter_path}.default", path=default_path,
                                          code="registry.parameter.default.invalid")
            elif parameter_type in {"Vector3F32", "Vector4F32"}:
                component_count = 3 if parameter_type == "Vector3F32" else 4
                if not isinstance(default, list) or len(default) != component_count:
                    raise ContractError(f"{parameter_path}.default must contain {component_count} Float32 values",
                                        code="registry.parameter.default.invalid", path=default_path)
                canonical_default = tuple(_f32(value, f"{parameter_path}.default[{component}]",
                                                path=f"{default_path}/{component}",
                                                code="registry.parameter.default.invalid")
                                          for component, value in enumerate(default))
            else:
                canonical_default = _canonical_ramp_stops(
                    default, default_path, code="registry.parameter.default.invalid")
            parameters.append(Parameter(parameter_id, parameter_type, canonical_default))

        operation = Operation(
            operation_id=operation_id,
            semantic_version=version,
            display_name=display_name,
            category=category,
            implementation_id=implementation_id,
            inputs=parse_sockets("inputs", True),
            outputs=parse_sockets("outputs", False),
            parameters=tuple(parameters),
        )
        if len(operation.outputs) != 1 and implementation_id not in MULTI_OUTPUT_IMPLEMENTATIONS:
            raise ContractError(f"{path_label} must declare exactly one output", code="operation.output.shape.invalid", path=f"{path}/outputs")
        if implementation_id == "float.uv0.x" and (
                operation.inputs or len(operation.outputs) != 1 or
                operation.outputs[0].socket_id != "value" or
                operation.outputs[0].value_type != "Float" or operation.parameters):
            raise ContractError(f"{path_label} float.uv0.x requires no inputs or parameters and one Float value output", code="operation.signature.invalid", path=path)
        if implementation_id == "vector2.uv0" and (
                operation.inputs or len(operation.outputs) != 1 or
                operation.outputs[0].socket_id != "value" or
                operation.outputs[0].value_type != "Vector2" or operation.parameters):
            raise ContractError(f"{path_label} vector2.uv0 requires no inputs or parameters and one Vector2 value output", code="operation.signature.invalid", path=path)
        if implementation_id == "vector2.mapping.scale_offset" and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("vector", "Vector2", True)] or
                [(item.socket_id, item.value_type) for item in operation.outputs] !=
                [("vector", "Vector2")] or
                [(item.parameter_id, item.value_type) for item in operation.parameters] !=
                [("scaleX", "Float32"), ("scaleY", "Float32"),
                 ("offsetX", "Float32"), ("offsetY", "Float32")]):
            raise ContractError(f"{path_label} vector2.mapping.scale_offset requires one Vector2 input/output and four Float32 parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "float.constant" and (
                operation.inputs or operation.outputs[0].value_type != "Float" or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "value" or
                operation.parameters[0].value_type != "Float32"):
            raise ContractError(f"{path_label} float.constant requires zero inputs and one value parameter", code="operation.signature.invalid", path=path)
        if implementation_id == "vector3.constant" and (
                operation.inputs or operation.outputs[0].value_type != "Vector3" or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "value" or
                operation.parameters[0].value_type != "Vector3F32"):
            raise ContractError(f"{path_label} vector3.constant requires zero inputs, one Vector3 output, and one Vector3F32 value parameter", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.constant" and (
                operation.inputs or operation.outputs[0].value_type != "Vector4" or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "value" or
                operation.parameters[0].value_type != "Vector4F32"):
            raise ContractError(f"{path_label} vector4.constant requires zero inputs, one Vector4 output, and one Vector4F32 value parameter", code="operation.signature.invalid", path=path)
        if implementation_id in {"float.add", "float.subtract", "float.multiply", "float.divide.positive_floor_1e5", "vector3.add", "vector3.subtract", "vector3.multiply", "vector4.add", "vector4.subtract", "vector4.multiply", "vector4.divide.positive_floor_1e5"} and (
                (operation.inputs[0].value_type if len(operation.inputs) > 0 else None) !=
                ("Float" if implementation_id.startswith("float.") else "Vector3" if implementation_id.startswith("vector3.") else "Vector4") or
                (operation.inputs[1].value_type if len(operation.inputs) > 1 else None) !=
                ("Float" if implementation_id.startswith("float.") else "Vector3" if implementation_id.startswith("vector3.") else "Vector4") or
                [item.socket_id for item in operation.inputs] != ["a", "b"] or
                not operation.inputs[0].required or operation.inputs[0].default is not None or
                (operation.inputs[1].required and operation.inputs[1].default is not None) or
                (not operation.inputs[1].required and operation.inputs[1].default is None) or
                (implementation_id not in {"float.add", "vector3.add", "vector4.add"} and not operation.inputs[1].required) or
                operation.outputs[0].socket_id != "result" or
                operation.outputs[0].value_type != ("Float" if implementation_id.startswith("float.") else "Vector3" if implementation_id.startswith("vector3.") else "Vector4") or
                operation.parameters):
            raise ContractError(f"{path_label} {implementation_id} requires two matching typed inputs a/b, a matching result, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.invert.linear_factor" and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("color", "Vector4", True), ("factor", "Float", True)] or
                operation.outputs[0].socket_id != "result" or
                operation.outputs[0].value_type != "Vector4" or
                operation.parameters):
            raise ContractError(f"{path_label} vector4.invert.linear_factor requires Vector4 color and Float factor inputs, a Vector4 result, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "float.clamp.sorted_bounds" and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("value", "Float", True), ("minimum", "Float", True), ("maximum", "Float", True)] or
                operation.outputs[0].socket_id != "result" or
                operation.outputs[0].value_type != "Float" or
                operation.parameters):
            raise ContractError(f"{path_label} float.clamp.sorted_bounds requires Float value/minimum/maximum inputs, a Float result, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id in VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("factor", "Float", True), ("color1", "Vector4", True), ("color2", "Vector4", True)] or
                operation.outputs[0].socket_id != "result" or
                operation.outputs[0].value_type != "Vector4" or
                operation.parameters):
            raise ContractError(f"{path_label} vector4.mix.saturated_factor requires Float factor, Vector4 color1/color2, a Vector4 result, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "float.replicate.rgb_alpha_one" and (
                len(operation.inputs) != 1 or operation.inputs[0].socket_id != "value" or
                operation.inputs[0].value_type != "Float" or not operation.inputs[0].required or
                operation.outputs[0].socket_id != "vector" or operation.outputs[0].value_type != "Vector4" or
                operation.parameters):
            raise ContractError(f"{path_label} float.replicate.rgb_alpha_one requires Float value input, Vector4 vector output, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.extract_x" and (
                len(operation.inputs) != 1 or operation.inputs[0].socket_id != "vector" or
                operation.inputs[0].value_type != "Vector4" or not operation.inputs[0].required or
                operation.outputs[0].socket_id != "value" or operation.outputs[0].value_type != "Float" or
                operation.parameters):
            raise ContractError(f"{path_label} vector4.extract_x requires Vector4 vector input, Float value output, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.separate_channels" and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("vector", "Vector4", True)] or
                [(item.socket_id, item.value_type) for item in operation.outputs] !=
                [("x", "Float"), ("y", "Float"), ("z", "Float"), ("w", "Float")] or
                operation.parameters):
            raise ContractError(f"{path_label} vector4.separate_channels requires one Vector4 input, Float x/y/z/w outputs, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.ramp.linear_stops" and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("factor", "Float", True)] or
                [(item.socket_id, item.value_type) for item in operation.outputs] !=
                [("color", "Vector4"), ("alpha", "Float")] or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "stops" or
                operation.parameters[0].value_type != "RampStops" or
                operation.parameters[0].default is None):
            raise ContractError(f"{path_label} vector4.ramp.linear_stops requires a Float factor input, RampStops parameter, Vector4 color and Float alpha outputs", code="operation.signature.invalid", path=path)
        if implementation_id in (set(TEXTURE_SAMPLE_SLOTS) | {GENERIC_TEXTURE_SAMPLE}) and (
                [(item.socket_id, item.value_type, item.required) for item in operation.inputs] !=
                [("uv", "Vector2", True)] or
                [(item.socket_id, item.value_type) for item in operation.outputs] !=
                [("color", "Vector4"), ("alpha", "Float")] or operation.parameters):
            raise ContractError(f"{path_label} {implementation_id} requires a Vector2 UV input, raw Vector4 color and Float alpha outputs, and no parameters", code="operation.signature.invalid", path=path)
        if implementation_id == "float.divide.positive_floor_1e5" and operation.outputs[0].value_type != "Float":
            raise ContractError(f"{path_label} float.divide.positive_floor_1e5 requires Float input and output sockets", code="operation.signature.invalid", path=path)
        registry[registry_key] = operation
    return registry


def _lookup_operation(registry: dict[tuple[str, int], Operation],
                      operation_id: Any, semantic_version: Any) -> Operation | None:
    """Look up serialized identity fields without hashing malformed JSON values."""
    if (not isinstance(operation_id, str) or isinstance(semantic_version, bool) or
            not isinstance(semantic_version, int)):
        return None
    return registry.get((operation_id, semantic_version))


def operation_contract_hash(operation: Operation) -> str:
    """Hash executable/type/default semantics, excluding presentation metadata."""
    contract = {
        "operationId": operation.operation_id,
        "semanticVersion": operation.semantic_version,
        "implementationId": operation.implementation_id,
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required,
                    **({"default": item.default} if item.default is not None else {})}
                   for item in operation.inputs],
        "outputs": [{"socketId": item.socket_id, "valueType": item.value_type} for item in operation.outputs],
        "parameters": [{"parameterId": item.parameter_id, "valueType": item.value_type, "default": item.default} for item in operation.parameters],
    }
    encoded = json.dumps(contract, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest().upper()


def _operation_semantics_without_version(operation: Operation) -> str:
    payload = {
        "operationId": operation.operation_id,
        "implementationId": operation.implementation_id,
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required,
                    **({"default": item.default} if item.default is not None else {})}
                   for item in operation.inputs],
        "outputs": [{"socketId": item.socket_id, "valueType": item.value_type} for item in operation.outputs],
        "parameters": [{"parameterId": item.parameter_id, "valueType": item.value_type, "default": item.default} for item in operation.parameters],
    }
    return json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False)


def registry_contract_lock(registry: dict[tuple[str, int], Operation]) -> dict[str, str]:
    return {f"{operation_id}@{version}": operation_contract_hash(operation)
            for (operation_id, version), operation in sorted(registry.items())}


def build_palette_model(registry: dict[tuple[str, int], Operation]) -> list[dict[str, Any]]:
    """Build deterministic UI metadata without using labels as identity."""
    operations = sorted(registry.values(), key=lambda item: (item.category.casefold(), item.display_name.casefold(), item.operation_id, item.semantic_version))
    return [{
        "entryId": f"{operation.operation_id}@{operation.semantic_version}",
        "operationId": operation.operation_id,
        "semanticVersion": operation.semantic_version,
        "label": operation.display_name,
        "category": operation.category,
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required,
                    **({"default": item.default} if item.default is not None else {})}
                   for item in operation.inputs],
        "outputs": [{"socketId": item.socket_id, "valueType": item.value_type} for item in operation.outputs],
        "parameters": [{"parameterId": item.parameter_id, "valueType": item.value_type, "default": item.default} for item in operation.parameters],
    } for operation in operations]


def instantiate_node(operation: Operation, node_id: str, *, x: float = 0.0, y: float = 0.0) -> dict[str, Any]:
    """Create editable node state from a registered operation's defaults."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("nodeId is invalid", code="node.id.invalid", path="/nodeId")
    parameters = {
        item.parameter_id: (list(item.default) if isinstance(item.default, tuple)
                            else copy.deepcopy(item.default))
        for item in operation.parameters if item.default is not None
    }
    node = {
        "nodeId": node_id,
        "operationId": operation.operation_id,
        "semanticVersion": operation.semantic_version,
        "parameters": parameters,
        "layout": {
            "x": _f32(x, "layout.x", path="/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(y, "layout.y", path="/layout/y", code="node.layout.coordinate.invalid"),
        },
    }
    if operation.implementation_id == GENERIC_TEXTURE_SAMPLE:
        node["resourceReference"] = None
    return node


def _canonical_image_resource_reference(raw: Any, node_id: str, node_index: int) -> dict[str, Any]:
    path = f"/nodes/{node_index}/resourceReference"
    if (not isinstance(raw, dict) or
            set(raw) != {"resourceKey", "resourceId", "sampler", "colorInterpretation"}):
        raise ContractError("Image Texture resourceReference must contain resourceKey, resourceId, sampler, and colorInterpretation",
                            code="resource.reference.invalid", path=path)
    resource_key, resource_id = raw.get("resourceKey"), raw.get("resourceId")
    if (not isinstance(resource_key, str) or not IDENTIFIER.fullmatch(resource_key) or
            not isinstance(resource_id, str) or not IDENTIFIER.fullmatch(resource_id)):
        raise ContractError("resourceKey and resourceId must be valid identifiers",
                            code="resource.reference.invalid", path=path)
    sampler = raw.get("sampler")
    sampler_fields = {"minFilter", "magFilter", "mipFilter", "addressU", "addressV", "maxAnisotropy"}
    if not isinstance(sampler, dict) or set(sampler) != sampler_fields:
        raise ContractError("sampler descriptor has an invalid shape",
                            code="resource.sampler.invalid", path=f"{path}/sampler")
    enums = {
        "minFilter": {"nearest", "linear"}, "magFilter": {"nearest", "linear"},
        "mipFilter": {"none", "nearest", "linear"},
        "addressU": {"repeat", "clamp", "mirror"},
        "addressV": {"repeat", "clamp", "mirror"},
    }
    for field, accepted in enums.items():
        if not isinstance(sampler.get(field), str) or sampler[field] not in accepted:
            raise ContractError(f"sampler {field} is unsupported",
                                code="resource.sampler.invalid", path=f"{path}/sampler/{field}")
    anisotropy = sampler.get("maxAnisotropy")
    if isinstance(anisotropy, bool) or not isinstance(anisotropy, int) or not 1 <= anisotropy <= 16:
        raise ContractError("sampler maxAnisotropy must be an integer from 1 through 16",
                            code="resource.sampler.invalid", path=f"{path}/sampler/maxAnisotropy")
    color = raw.get("colorInterpretation")
    if not isinstance(color, str) or color not in {"linear", "srgb"}:
        raise ContractError("colorInterpretation is unsupported",
                            code="resource.color_interpretation.invalid",
                            path=f"{path}/colorInterpretation")
    return {"resourceKey": resource_key, "resourceId": resource_id,
            "sampler": {field: sampler[field] for field in sorted(sampler_fields)},
            "colorInterpretation": color}


def _canonical_graph_resources(raw: Any, *, path: str = "/resources") -> tuple[list[dict[str, Any]], dict[str, dict[str, Any]]]:
    if not isinstance(raw, list):
        raise ContractError("resources must be an array",
                            code="schema.collection.required", path=path)
    if len(raw) > MAX_GRAPH_RESOURCES:
        raise ContractError("document exceeds the resource limit",
                            code="graph.resources.limit_exceeded", path=path)
    records = []
    by_id: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(raw):
        item_path = f"{path}/{index}"
        if not isinstance(item, dict) or set(item) != {"resourceId", "resourceKind", "source"}:
            raise ContractError("resource entry has an invalid shape",
                                code="resource.shape.invalid", path=item_path)
        resource_id = item.get("resourceId")
        if not isinstance(resource_id, str) or not IDENTIFIER.fullmatch(resource_id):
            raise ContractError("resourceId is invalid", code="resource.id.invalid",
                                path=f"{item_path}/resourceId")
        if resource_id in by_id:
            raise ContractError("resourceId is duplicated", code="resource.id.duplicate",
                                path=f"{item_path}/resourceId")
        if item.get("resourceKind") != "sampledTexture2D":
            raise ContractError("resourceKind is unsupported", code="resource.kind.unsupported",
                                path=f"{item_path}/resourceKind")
        source = item.get("source")
        if (not isinstance(source, dict) or set(source) != {"kind", "assetId"} or
                source.get("kind") != "projectAsset"):
            raise ContractError("resource source is invalid", code="resource.source.invalid",
                                path=f"{item_path}/source")
        asset_id = source.get("assetId")
        if not isinstance(asset_id, str) or not ASSET_UUID.fullmatch(asset_id):
            raise ContractError("assetId must be a canonical UUID string",
                                code="resource.asset_id.invalid",
                                path=f"{item_path}/source/assetId")
        record = {"resourceId": resource_id, "resourceKind": "sampledTexture2D",
                  "source": {"kind": "projectAsset", "assetId": asset_id.lower()}}
        records.append(record)
        by_id[resource_id] = record
    return records, by_id


def _canonical_socket_default_value(value: Any, definition: Socket, node_id: str,
                                    path: str) -> float | list[float]:
    if definition.value_type == "Float":
        return _f32(value, f"node {node_id}.{definition.socket_id}",
                    path=path, code="socket.default.invalid")
    if definition.value_type == "Vector4":
        if not isinstance(value, (list, tuple)) or len(value) != 4:
            raise ContractError(f"node {node_id}.{definition.socket_id} default must contain four Float32 lanes",
                                code="socket.default.invalid", path=path)
        return [
            _f32(lane, f"node {node_id}.{definition.socket_id}[{index}]",
                 path=f"{path}/{index}", code="socket.default.invalid")
            for index, lane in enumerate(value)
        ]
    if definition.value_type == "Vector3":
        if not isinstance(value, (list, tuple)) or len(value) != 3:
            raise ContractError(f"node {node_id}.{definition.socket_id} default must contain three Float32 lanes",
                                code="socket.default.invalid", path=path)
        return [
            _f32(lane, f"node {node_id}.{definition.socket_id}[{index}]",
                 path=f"{path}/{index}", code="socket.default.invalid")
            for index, lane in enumerate(value)
        ]
    if definition.value_type == "Vector2":
        raise ContractError(f"node {node_id}.{definition.socket_id} has no Vector2 default contract",
                            code="socket.default.type.unsupported", path=path)
    raise ContractError(f"node {node_id}.{definition.socket_id} has an unsupported default type",
                        code="socket.default.type.unsupported", path=path)


def _canonical_socket_defaults(raw: Any, operation: Operation, node_id: str,
                               node_index: int, *, require_complete: bool = False) -> dict[str, float | list[float]]:
    if not isinstance(raw, dict):
        raise ContractError(f"node {node_id} socketDefaults must be an object",
                            code="node.socket_defaults.invalid",
                            path=f"/nodes/{node_index}/socketDefaults")
    optional = {item.socket_id: item for item in operation.inputs if not item.required}
    unknown = set(raw) - set(optional)
    if unknown:
        socket_id = sorted(unknown)[0]
        raise ContractError(f"node {node_id} has an unknown or required socket default",
                            code="node.socket_default.unknown",
                            path=f"/nodes/{node_index}/socketDefaults/{_pointer_segment(str(socket_id))}")
    if require_complete and set(raw) != set(optional):
        missing = sorted(set(optional) - set(raw))
        raise ContractError("schema version 3 requires a stored default for every optional input",
                            code="node.socket_default.required",
                            path=f"/nodes/{node_index}/socketDefaults/{_pointer_segment(missing[0])}" if missing else f"/nodes/{node_index}/socketDefaults")
    result: dict[str, float | list[float]] = {}
    for socket_id, definition in optional.items():
        path = f"/nodes/{node_index}/socketDefaults/{_pointer_segment(socket_id)}"
        value = raw.get(socket_id, definition.default)
        if value is None:
            raise ContractError(f"node {node_id}.{socket_id} has no registered default",
                                code="socket.default.required", path=path)
        result[socket_id] = _canonical_socket_default_value(value, definition, node_id, path)
    return result


def _upgrade_document_to_socket_defaults(document: dict[str, Any],
                                         registry: dict[tuple[str, int], Operation]) -> None:
    """Promote an editable document and snapshot every optional input default."""
    if document.get("schemaVersion") == 3:
        return
    version = document.get("schemaVersion")
    if (document.get("kind") != "artifact-shader-graph-rd-document" or
            isinstance(version, bool) or not isinstance(version, int) or version not in (1, 2)):
        raise ContractError("document kind or schemaVersion is unsupported",
                            code="schema.version.unsupported", path="/schemaVersion")
    if version == 1 and "graphParameters" in document:
        raise ContractError("graphParameters require document schema version 2",
                            code="schema.field.version_mismatch", path="/graphParameters")
    if version == 2 and not isinstance(document.get("graphParameters"), list):
        raise ContractError("schema version 2 requires a graphParameters array",
                            code="schema.collection.required", path="/graphParameters")
    nodes = document.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            continue
        operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
        if operation is None:
            continue
        node["socketDefaults"] = {
            item.socket_id: copy.deepcopy(item.default)
            for item in operation.inputs if not item.required
        }
    document["schemaVersion"] = 3
    if version == 1:
        document["graphParameters"] = []


def add_node(document: Any, registry: dict[tuple[str, int], Operation],
             operation_id: str, semantic_version: int, node_id: str, *,
             x: float = 0.0, y: float = 0.0) -> dict[str, Any]:
    """Append one registry-backed node without changing links or active output."""
    edited = preserve_document(document)
    if (not isinstance(edited, dict) or isinstance(edited.get("schemaVersion"), bool) or
            not isinstance(edited.get("schemaVersion"), int) or edited.get("schemaVersion") not in (1, 2, 3)):
        raise ContractError("document schemaVersion is unsupported", code="schema.version.unsupported", path="/schemaVersion")
    if edited.get("kind") != "artifact-shader-graph-rd-document":
        raise ContractError("document kind is unsupported", code="document.kind.unsupported", path="/kind")
    nodes, links = edited.get("nodes"), edited.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays", code="schema.collection.required", path="/nodes")
    if any(isinstance(node, dict) and node.get("nodeId") == node_id for node in nodes):
        raise ContractError(f"duplicate nodeId {node_id}", code="node.id.duplicate", path="/nodes")
    operation = _lookup_operation(registry, operation_id, semantic_version)
    if operation is None:
        raise ContractError(f"operation {operation_id}@{semantic_version} is unavailable",
                            code="node.operation.version.unsupported", path="/operationId")
    nodes.append(instantiate_node(operation, node_id, x=x, y=y))
    if operation.implementation_id == GENERIC_TEXTURE_SAMPLE and "resources" not in edited:
        edited["resources"] = []
    if any(not item.required for item in operation.inputs):
        _upgrade_document_to_socket_defaults(edited, registry)
    return edited


def replace_node_operation(document: Any, registry: dict[tuple[str, int], Operation],
                           node_id: str, operation_id: str, semantic_version: int) -> dict[str, Any]:
    """Replace a node only when its serialized pins/parameters remain compatible."""
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError("node is missing or ambiguous", code="node.id.not_unique_or_missing", path="/nodes")
    index, node = matches[0]
    source = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    target = _lookup_operation(registry, operation_id, semantic_version)
    if source is None:
        raise ContractError("current operation is unavailable", code="node.operation.version.unsupported",
                            path=f"/nodes/{index}/semanticVersion")
    if target is None:
        raise ContractError("replacement operation is unavailable", code="node.operation.version.unsupported",
                            path=f"/nodes/{index}/operationId")
    if source.operation_id == target.operation_id and source.semantic_version == target.semantic_version:
        raise ContractError("replacement operation is unchanged", code="node.operation.noop",
                            path=f"/nodes/{index}/operationId")
    if (source.inputs != target.inputs or source.outputs != target.outputs or
            [(item.parameter_id, item.value_type) for item in source.parameters] !=
            [(item.parameter_id, item.value_type) for item in target.parameters]):
        raise ContractError("replacement must preserve pin IDs/types/requiredness and parameter IDs/types",
                            code="node.operation.incompatible", path=f"/nodes/{index}/operationId")
    raw_parameters = node.get("parameters", {})
    if not isinstance(raw_parameters, dict):
        raise ContractError("node parameters must be an object", code="node.parameters.invalid",
                            path=f"/nodes/{index}/parameters")
    source_parameters = {item.parameter_id: item for item in source.parameters}
    for target_parameter in target.parameters:
        source_parameter = source_parameters[target_parameter.parameter_id]
        if (target_parameter.parameter_id not in raw_parameters and
                source_parameter.default != target_parameter.default):
            raise ContractError("replacement changes an unmaterialized parameter default",
                                code="node.operation.incompatible",
                                path=f"/nodes/{index}/parameters/{_pointer_segment(target_parameter.parameter_id)}")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    _canonical_parameters(node.get("parameters", {}), target, node_id, index)
    source_key = f"{source.operation_id}@{source.semantic_version}"
    target_key = f"{target.operation_id}@{target.semantic_version}"
    node["operationId"] = target.operation_id
    node["semanticVersion"] = target.semantic_version
    if target.implementation_id != GENERIC_TEXTURE_SAMPLE:
        node.pop("resourceReference", None)
    if "registryContractLock" in edited:
        lock = edited["registryContractLock"]
        still_referenced = any(isinstance(item, dict) and
                               f"{item.get('operationId')}@{item.get('semanticVersion')}" == source_key
                               for item in nodes)
        if not still_referenced:
            lock.pop(source_key, None)
        lock[target_key] = operation_contract_hash(target)
    return edited


@dataclass
class _ConnectionState:
    nodes: dict[str, dict[str, Any]]
    operations: dict[str, Operation | None]
    outputs: dict[tuple[str, str], Socket]
    inputs: dict[tuple[str, str], Socket]
    adjacency: dict[str, set[str]]
    writers: set[tuple[str, str]]
    topological_rank: dict[str, int] | None = None


def _topological_ranks(adjacency: dict[str, set[str]]) -> dict[str, int] | None:
    indegree = {node_id: 0 for node_id in adjacency}
    for successors in adjacency.values():
        for successor in successors:
            indegree[successor] += 1
    ready = [node_id for node_id, count in indegree.items() if count == 0]
    heapq.heapify(ready)
    ranks: dict[str, int] = {}
    while ready:
        node_id = heapq.heappop(ready)
        ranks[node_id] = len(ranks)
        for successor in sorted(adjacency[node_id]):
            indegree[successor] -= 1
            if indegree[successor] == 0:
                heapq.heappush(ready, successor)
    return ranks if len(ranks) == len(adjacency) else None


def _connection_validation_state(document: dict[str, Any],
                                 registry: dict[tuple[str, int], Operation], *,
                                 optimize_batch_cycles: bool = False) -> _ConnectionState:
    """Index the existing graph once for one connection or an atomic batch."""
    nodes = document.get("nodes")
    links = document.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays",
                            code="schema.collection.required", path="/links")
    by_id: dict[str, dict[str, Any]] = {}
    for index, node in enumerate(nodes):
        if not isinstance(node, dict) or not isinstance(node.get("nodeId"), str):
            raise ContractError("existing node has no string nodeId", code="node.id.invalid", path=f"/nodes/{index}/nodeId")
        node_id = node["nodeId"]
        if node_id in by_id:
            raise ContractError(f"duplicate nodeId {node_id}", code="node.id.duplicate", path=f"/nodes/{index}/nodeId")
        by_id[node_id] = node
    operations = {node_id: _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
                  for node_id, node in by_id.items()}
    output_sockets: dict[tuple[str, str], Socket] = {}
    input_sockets: dict[tuple[str, str], Socket] = {}
    for node_id, operation in operations.items():
        if operation is not None:
            output_sockets.update({(node_id, socket.socket_id): socket for socket in operation.outputs})
            input_sockets.update({(node_id, socket.socket_id): socket for socket in operation.inputs})
    adjacency: dict[str, set[str]] = {node_id: set() for node_id in by_id}
    writers: set[tuple[str, str]] = set()
    for link in links:
        if not isinstance(link, dict) or not isinstance(link.get("from"), dict) or not isinstance(link.get("to"), dict):
            raise ContractError("existing link is malformed", code="link.invalid", path="/links")
        from_id, to_id = link["from"].get("nodeId"), link["to"].get("nodeId")
        from_socket, to_socket = link["from"].get("socketId"), link["to"].get("socketId")
        if not all(isinstance(value, str) for value in (from_id, to_id, from_socket, to_socket)):
            raise ContractError("existing link endpoints require string node/socket IDs", code="link.endpoint.required", path="/links")
        if (to_id, to_socket) in writers:
            raise ContractError("existing input has multiple writers", code="link.input.multiple_writers", path="/links")
        writers.add((to_id, to_socket))
        if from_id in adjacency and to_id in adjacency:
            adjacency[from_id].add(to_id)
        # Keep opaque edges involving unknown operations untouched; the compiler
        # reports them until their definitions become available.
        if operations.get(from_id) is not None and operations.get(to_id) is not None:
            output = output_sockets.get((from_id, from_socket))
            input_socket = input_sockets.get((to_id, to_socket))
            if output is None or input_socket is None:
                raise ContractError("existing link references an unknown socket", code="link.socket.missing_or_direction_invalid", path="/links")
            if output.value_type != input_socket.value_type:
                raise ContractError("existing link types do not match", code="link.type.incompatible", path="/links")

    return _ConnectionState(
        by_id, operations, output_sockets, input_sockets, adjacency, writers,
        _topological_ranks(adjacency) if optimize_batch_cycles else None)


def _connect_with_state(edited: dict[str, Any], state: _ConnectionState,
        source_node_id: str, source_socket_id: str,
        target_node_id: str, target_socket_id: str) -> None:
    """Validate and append one edge while updating reusable batch indexes."""
    if not all(isinstance(value, str) for value in
               (source_node_id, source_socket_id, target_node_id, target_socket_id)):
        raise ContractError("connection endpoints require string node/socket IDs",
                            code="link.endpoint.required", path="/links")
    if source_node_id not in state.nodes:
        raise ContractError(f"node {source_node_id} does not exist", code="link.node.missing", path="/links")
    if target_node_id not in state.nodes:
        raise ContractError(f"node {target_node_id} does not exist", code="link.node.missing", path="/links")
    if state.operations.get(source_node_id) is None:
        raise ContractError(f"operation for node {source_node_id} is unavailable",
                            code="node.operation.version.unsupported", path="/nodes")
    if state.operations.get(target_node_id) is None:
        raise ContractError(f"operation for node {target_node_id} is unavailable",
                            code="node.operation.version.unsupported", path="/nodes")
    source_spec = state.outputs.get((source_node_id, source_socket_id))
    target_spec = state.inputs.get((target_node_id, target_socket_id))
    if source_spec is None:
        raise ContractError(f"socket {source_socket_id} is unavailable in this direction",
                            code="link.socket.missing_or_direction_invalid", path="/links/from/socketId")
    if target_spec is None:
        raise ContractError(f"socket {target_socket_id} is unavailable in this direction",
                            code="link.socket.missing_or_direction_invalid", path="/links/to/socketId")
    if source_spec.value_type != target_spec.value_type:
        raise ContractError(f"link type mismatch {source_spec.value_type} -> {target_spec.value_type}",
                            code="link.type.incompatible", path="/links")
    target_key = (target_node_id, target_socket_id)
    if target_key in state.writers:
        raise ContractError("input already has a writer", code="link.input.multiple_writers", path="/links")
    ranks = state.topological_rank
    needs_cycle_search = (ranks is None or
                          ranks[source_node_id] >= ranks[target_node_id])
    if needs_cycle_search:
        pending, visited = [target_node_id], set()
        while pending:
            current = pending.pop()
            if current == source_node_id:
                raise ContractError("link would create a cycle", code="graph.cycle", path="/links")
            if current not in visited:
                visited.add(current)
                pending.extend(state.adjacency.get(current, ()))
    edited["links"].append({"from": {"nodeId": source_node_id, "socketId": source_socket_id},
                            "to": {"nodeId": target_node_id, "socketId": target_socket_id}})
    state.writers.add(target_key)
    state.adjacency[source_node_id].add(target_node_id)
    if ranks is not None and needs_cycle_search:
        state.topological_rank = _topological_ranks(state.adjacency)


def connect_nodes(document: Any, registry: dict[tuple[str, int], Operation],
                  source_node_id: str, source_socket_id: str,
                  target_node_id: str, target_socket_id: str) -> dict[str, Any]:
    """Atomically add a typed single-writer edge and reject cycles."""
    edited = preserve_document(document)
    state = _connection_validation_state(edited, registry)
    _connect_with_state(edited, state, source_node_id, source_socket_id,
                        target_node_id, target_socket_id)
    return edited


def rewire_input(document: Any, registry: dict[tuple[str, int], Operation],
                 source_node_id: str, source_socket_id: str,
                 target_node_id: str, target_socket_id: str) -> dict[str, Any]:
    """Replace one existing input edge atomically, validating the new graph."""
    links = document.get("links") if isinstance(document, dict) else None
    if not isinstance(links, list):
        raise ContractError("document links must be an array", code="schema.collection.required", path="/links")
    matches = [index for index, link in enumerate(links)
               if isinstance(link, dict) and isinstance(link.get("to"), dict)
               and link["to"].get("nodeId") == target_node_id
               and link["to"].get("socketId") == target_socket_id]
    if len(matches) != 1:
        raise ContractError("rewiring requires exactly one existing input edge",
                            code="link.input.not_unique_or_missing", path="/links")
    current = links[matches[0]]
    current_source = current.get("from") if isinstance(current, dict) else None
    if (isinstance(current_source, dict) and current_source.get("nodeId") == source_node_id
            and current_source.get("socketId") == source_socket_id):
        return preserve_document(document)
    edited = preserve_document(document)
    del edited["links"][matches[0]]
    state = _connection_validation_state(edited, registry)
    _connect_with_state(edited, state, source_node_id, source_socket_id,
                        target_node_id, target_socket_id)
    return edited


def connect_nodes_batch(document: Any, registry: dict[tuple[str, int], Operation],
                        connections: list[tuple[str, str, str, str]]) -> dict[str, Any]:
    """Apply several typed links atomically; failure leaves the input untouched."""
    if not isinstance(connections, list) or not connections:
        raise ContractError("connections must be a non-empty list", code="link.batch.invalid", path="/connections")
    working = preserve_document(document)
    state = _connection_validation_state(
        working, registry, optimize_batch_cycles=len(connections) > 1)
    for index, connection in enumerate(connections):
        if (not isinstance(connection, (tuple, list)) or len(connection) != 4 or
                not all(isinstance(value, str) for value in connection)):
            raise ContractError("connection must contain source node/socket and target node/socket IDs",
                                code="link.batch.entry.invalid", path=f"/connections/{index}")
        _connect_with_state(working, state, *connection)
    return working


def disconnect_nodes(document: Any, source_node_id: str, source_socket_id: str,
                     target_node_id: str, target_socket_id: str) -> dict[str, Any]:
    """Remove exactly one addressed edge while preserving every other link."""
    edited = preserve_document(document)
    links = edited.get("links")
    if not isinstance(links, list):
        raise ContractError("document links must be an array", code="schema.collection.required", path="/links")
    matches = [index for index, link in enumerate(links)
               if isinstance(link, dict) and isinstance(link.get("from"), dict) and isinstance(link.get("to"), dict)
               and link["from"].get("nodeId") == source_node_id and link["from"].get("socketId") == source_socket_id
               and link["to"].get("nodeId") == target_node_id and link["to"].get("socketId") == target_socket_id]
    if len(matches) != 1:
        raise ContractError("expected exactly one matching link", code="link.not_unique_or_missing", path="/links")
    del links[matches[0]]
    return edited


def disconnect_nodes_batch(document: Any,
                           connections: list[tuple[str, str, str, str]]) -> dict[str, Any]:
    """Remove several exact links only after every requested edge is found once."""
    if not isinstance(connections, list) or not connections:
        raise ContractError("connections must be a non-empty list", code="link.batch.invalid", path="/connections")
    links = document.get("links") if isinstance(document, dict) else None
    if not isinstance(links, list):
        raise ContractError("document links must be an array", code="schema.collection.required", path="/links")
    indices_by_edge: dict[tuple[str, str, str, str], list[int]] = {}
    for link_index, link in enumerate(links):
        if not isinstance(link, dict) or not isinstance(link.get("from"), dict) or not isinstance(link.get("to"), dict):
            continue
        edge = (link["from"].get("nodeId"), link["from"].get("socketId"),
                link["to"].get("nodeId"), link["to"].get("socketId"))
        if all(isinstance(value, str) for value in edge):
            indices_by_edge.setdefault(edge, []).append(link_index)
    matched_indices: set[int] = set()
    for index, connection in enumerate(connections):
        if (not isinstance(connection, (tuple, list)) or len(connection) != 4 or
                not all(isinstance(value, str) for value in connection)):
            raise ContractError("connection must contain source node/socket and target node/socket IDs",
                                code="link.batch.entry.invalid", path=f"/connections/{index}")
        matches = indices_by_edge.get(tuple(connection), [])
        if len(matches) != 1 or matches[0] in matched_indices:
            raise ContractError("each requested link must match one distinct existing edge",
                                code="link.not_unique_or_missing", path=f"/connections/{index}")
        matched_indices.add(matches[0])
    edited = preserve_document(document)
    edited["links"] = [link for index, link in enumerate(edited["links"]) if index not in matched_indices]
    return edited


def disconnect_link_at(document: Any, link_index: int) -> dict[str, Any]:
    """Remove exactly one link by its current document position for repair UI use."""
    if isinstance(link_index, bool) or not isinstance(link_index, int):
        raise ContractError("link index must be an integer", code="link.index.invalid", path="/linkIndex")
    links = document.get("links") if isinstance(document, dict) else None
    if not isinstance(links, list):
        raise ContractError("document links must be an array", code="schema.collection.required", path="/links")
    if link_index < 0 or link_index >= len(links):
        raise ContractError("link index is outside the current document", code="link.index.missing", path="/linkIndex")
    edited = preserve_document(document)
    del edited["links"][link_index]
    return edited


def set_graph_output(document: Any, registry: dict[tuple[str, int], Operation],
                     node_id: str, socket_id: str) -> dict[str, Any]:
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError("output node is missing or ambiguous", code="output.node.missing", path="/output/nodeId")
    index, node = matches[0]
    operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    if operation is None:
        raise ContractError("output operation is unavailable", code="node.operation.version.unsupported", path="/output/nodeId")
    if not any(item.socket_id == socket_id for item in operation.outputs):
        raise ContractError("output socket is unavailable", code="output.socket.missing", path="/output/socketId")
    edited["output"] = {"nodeId": node_id, "socketId": socket_id}
    return edited


def add_node_with_connections(document: Any, registry: dict[tuple[str, int], Operation],
                              operation_id: str, semantic_version: int, node_id: str,
                              connections: list[tuple[str, str, str, str]],
                              output_socket_id: str, *, x: float = 0.0, y: float = 0.0) -> dict[str, Any]:
    """Build a complete node edit on detached data and select its output last."""
    if not isinstance(connections, list):
        raise ContractError("connections must be a list", code="link.batch.invalid", path="/connections")
    edited = add_node(document, registry, operation_id, semantic_version, node_id, x=x, y=y)
    if connections:
        edited = connect_nodes_batch(edited, registry, connections)
    return set_graph_output(edited, registry, node_id, output_socket_id)


def delete_node(document: Any, registry: dict[tuple[str, int], Operation], node_id: str,
                *, replacement_output: tuple[str, str] | None = None) -> dict[str, Any]:
    """Remove one node and all incident links; output removal must be explicit."""
    edited = preserve_document(document)
    nodes, links = edited.get("nodes"), edited.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays", code="schema.collection.required", path="/nodes")
    matches = [index for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError("node is missing or ambiguous", code="node.id.not_unique_or_missing", path="/nodes")
    selected_output = edited.get("output")
    removes_output = isinstance(selected_output, dict) and selected_output.get("nodeId") == node_id
    if removes_output and replacement_output is None:
        raise ContractError("deleting the selected output requires an explicit replacement", code="output.replacement.required", path="/output")
    if replacement_output is not None and not removes_output:
        raise ContractError("replacement output is only valid when deleting the selected output", code="output.replacement.unexpected", path="/output")
    if replacement_output is not None:
        if replacement_output[0] == node_id:
            raise ContractError("replacement output cannot be the deleted node", code="output.replacement.invalid", path="/output/nodeId")
        edited = set_graph_output(edited, registry, replacement_output[0], replacement_output[1])
    del edited["nodes"][matches[0]]
    edited["links"] = [link for link in edited["links"]
                        if not (isinstance(link, dict) and
                                ((isinstance(link.get("from"), dict) and link["from"].get("nodeId") == node_id) or
                                 (isinstance(link.get("to"), dict) and link["to"].get("nodeId") == node_id)))]
    if isinstance(edited.get("graphParameters"), list):
        retained_parameters = []
        for parameter in edited["graphParameters"]:
            if not isinstance(parameter, dict) or not isinstance(parameter.get("bindings"), list):
                retained_parameters.append(parameter)
                continue
            well_formed = (
                set(parameter) == {"parameterId", "label", "valueType", "value", "bindings"} and
                bool(parameter["bindings"]) and
                all(isinstance(binding, dict) and set(binding) == {"nodeId", "parameterId"} and
                    isinstance(binding.get("nodeId"), str) and isinstance(binding.get("parameterId"), str)
                    for binding in parameter["bindings"])
            )
            parameter["bindings"] = [binding for binding in parameter["bindings"]
                                     if not (isinstance(binding, dict) and binding.get("nodeId") == node_id)]
            if parameter["bindings"] or not well_formed:
                retained_parameters.append(parameter)
        edited["graphParameters"] = retained_parameters
    return edited


def delete_nodes_batch(document: Any, registry: dict[tuple[str, int], Operation],
                       node_ids: list[str], *,
                       replacement_output: tuple[str, str] | None = None) -> dict[str, Any]:
    """Delete distinct nodes atomically, selecting a surviving output first."""
    if not isinstance(node_ids, list) or not node_ids:
        raise ContractError("node IDs must be a non-empty list",
                            code="node.delete.batch.invalid", path="/nodeIds")
    if any(not isinstance(node_id, str) for node_id in node_ids):
        raise ContractError("every node ID must be a string",
                            code="node.delete.batch.invalid", path="/nodeIds")
    if len(set(node_ids)) != len(node_ids):
        raise ContractError("node IDs must be unique",
                            code="node.delete.batch.duplicate", path="/nodeIds")
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    links = edited.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays",
                            code="schema.collection.required", path="/nodes" if not isinstance(nodes, list) else "/links")
    for index, node_id in enumerate(node_ids):
        matches = [node for node in nodes if isinstance(node, dict) and node.get("nodeId") == node_id]
        if len(matches) != 1:
            raise ContractError(f"node {node_id} is missing or ambiguous",
                                code="node.id.not_unique_or_missing", path=f"/nodeIds/{index}")
    node_id_set = set(node_ids)
    selected_output = edited.get("output")
    output_node_id = selected_output.get("nodeId") if isinstance(selected_output, dict) else None
    deletes_output = isinstance(output_node_id, str) and output_node_id in node_id_set
    if deletes_output and replacement_output is None:
        raise ContractError("deleting the selected output requires an explicit replacement",
                            code="output.replacement.required", path="/output")
    if replacement_output is not None:
        if (not isinstance(replacement_output, (tuple, list)) or len(replacement_output) != 2 or
                not all(isinstance(value, str) for value in replacement_output)):
            raise ContractError("replacement output must be a node/socket pair",
                                code="output.replacement.invalid", path="/output")
        if not deletes_output:
            raise ContractError("replacement output is only valid when deleting the selected output",
                                code="output.replacement.unexpected", path="/output")
        if replacement_output[0] in node_id_set:
            raise ContractError("replacement output must survive the deletion",
                                code="output.replacement.invalid", path="/output/nodeId")
        edited = set_graph_output(edited, registry, replacement_output[0], replacement_output[1])
    edited["nodes"] = [node for node in nodes
                       if not (isinstance(node, dict) and isinstance(node.get("nodeId"), str) and
                               node["nodeId"] in node_id_set)]
    edited["links"] = [link for link in links
                       if not (isinstance(link, dict) and
                               ((isinstance(link.get("from"), dict) and
                                 isinstance(link["from"].get("nodeId"), str) and
                                 link["from"]["nodeId"] in node_id_set) or
                                (isinstance(link.get("to"), dict) and
                                 isinstance(link["to"].get("nodeId"), str) and
                                 link["to"]["nodeId"] in node_id_set)))]
    if isinstance(edited.get("graphParameters"), list):
        retained_parameters = []
        for parameter in edited["graphParameters"]:
            if not isinstance(parameter, dict) or not isinstance(parameter.get("bindings"), list):
                retained_parameters.append(parameter)
                continue
            well_formed = (
                set(parameter) == {"parameterId", "label", "valueType", "value", "bindings"} and
                bool(parameter["bindings"]) and
                all(isinstance(binding, dict) and set(binding) == {"nodeId", "parameterId"} and
                    isinstance(binding.get("nodeId"), str) and isinstance(binding.get("parameterId"), str)
                    for binding in parameter["bindings"])
            )
            parameter["bindings"] = [binding for binding in parameter["bindings"]
                                     if not (isinstance(binding, dict) and
                                             isinstance(binding.get("nodeId"), str) and
                                             binding["nodeId"] in node_id_set)]
            if parameter["bindings"] or not well_formed:
                retained_parameters.append(parameter)
        edited["graphParameters"] = retained_parameters
    return edited


def duplicate_nodes_batch(document: Any, registry: dict[tuple[str, int], Operation],
                          duplicates: list[dict[str, str]], *,
                          offset_x: float = 32.0, offset_y: float = 32.0) -> dict[str, Any]:
    """Duplicate known nodes, shared parameter bindings, and internal edges atomically."""
    if not isinstance(duplicates, list) or not duplicates:
        raise ContractError("duplicates must be a non-empty list",
                            code="node.duplicate.batch.invalid", path="/duplicates")
    x_offset = _f32(offset_x, "duplicate offset x", path="/offsetX", code="node.layout.coordinate.invalid")
    y_offset = _f32(offset_y, "duplicate offset y", path="/offsetY", code="node.layout.coordinate.invalid")
    edited = preserve_document(document)
    nodes, links = edited.get("nodes"), edited.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays",
                            code="schema.collection.required", path="/nodes")
    source_to_clone: dict[str, str] = {}
    clone_ids: set[str] = set()
    existing_ids: set[str] = set()
    node_matches_by_id: dict[str, list[tuple[int, dict[str, Any]]]] = {}
    for node_index, node in enumerate(nodes):
        if isinstance(node, dict) and isinstance(node.get("nodeId"), str):
            node_id = node["nodeId"]
            existing_ids.add(node_id)
            node_matches_by_id.setdefault(node_id, []).append((node_index, node))
    originals: dict[str, dict[str, Any]] = {}
    operations: dict[str, Operation] = {}
    source_indices: dict[str, int] = {}
    for index, entry in enumerate(duplicates):
        path = f"/duplicates/{index}"
        if not isinstance(entry, dict) or set(entry) != {"sourceNodeId", "nodeId"}:
            raise ContractError("duplicate entry must contain sourceNodeId and nodeId",
                                code="node.duplicate.entry.invalid", path=path)
        source_id, clone_id = entry.get("sourceNodeId"), entry.get("nodeId")
        if not isinstance(source_id, str) or not isinstance(clone_id, str) or not IDENTIFIER.fullmatch(clone_id):
            raise ContractError("duplicate source and node IDs must be strings with a valid new ID",
                                code="node.duplicate.entry.invalid", path=path)
        if source_id in source_to_clone or clone_id in clone_ids:
            raise ContractError("duplicate source and destination IDs must be unique",
                                code="node.duplicate.batch.duplicate", path=path)
        if clone_id in existing_ids:
            raise ContractError(f"node ID {clone_id} already exists",
                                code="node.id.duplicate", path=f"{path}/nodeId")
        matches = node_matches_by_id.get(source_id, [])
        if len(matches) != 1:
            raise ContractError(f"source node {source_id} is missing or ambiguous",
                                code="node.id.not_unique_or_missing", path=f"{path}/sourceNodeId")
        source_index, source = matches[0]
        operation = _lookup_operation(registry, source.get("operationId"), source.get("semanticVersion"))
        if operation is None:
            raise ContractError(f"source node {source_id} has an unavailable operation",
                                code="node.operation.version.unsupported", path=f"{path}/sourceNodeId")
        source_to_clone[source_id] = clone_id
        clone_ids.add(clone_id)
        originals[source_id] = source
        operations[source_id] = operation
        source_indices[source_id] = source_index

    clones = []
    for source_id, clone_id in source_to_clone.items():
        source = originals[source_id]
        operation = operations[source_id]
        source_index = source_indices[source_id]
        canonical_parameters = _canonical_parameters(source.get("parameters", {}), operation, source_id, source_index)
        layout = source.get("layout")
        if not isinstance(layout, dict):
            raise ContractError("source node layout must be an object",
                                code="node.layout.invalid", path=f"/nodes/{source_index}/layout")
        clone = json.loads(canonical_json_text(source))
        clone["nodeId"] = clone_id
        if isinstance(clone.get("resourceReference"), dict):
            clone["resourceReference"]["resourceKey"] = f"artifact.shadergraph.image.{clone_id}"
        clone["parameters"] = canonical_parameters
        clone["layout"] = {
            "x": _f32(_f32(layout.get("x"), "source layout x", path=f"/nodes/{source_index}/layout/x", code="node.layout.coordinate.invalid") + x_offset,
                      "duplicate layout x", path=f"/nodes/{source_index}/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(_f32(layout.get("y"), "source layout y", path=f"/nodes/{source_index}/layout/y", code="node.layout.coordinate.invalid") + y_offset,
                      "duplicate layout y", path=f"/nodes/{source_index}/layout/y", code="node.layout.coordinate.invalid"),
        }
        clones.append(clone)
    edited["nodes"].extend(clones)

    if edited.get("schemaVersion") in (2, 3):
        graph_parameters = edited.get("graphParameters")
        if isinstance(graph_parameters, list):
            copied_binding_keys: set[tuple[str, str]] = set()
            for parameter in graph_parameters:
                if not isinstance(parameter, dict) or not isinstance(parameter.get("bindings"), list):
                    continue
                new_bindings = []
                for binding in parameter["bindings"]:
                    if not isinstance(binding, dict):
                        continue
                    source_id, parameter_id = binding.get("nodeId"), binding.get("parameterId")
                    if not isinstance(source_id, str) or source_id not in source_to_clone:
                        continue
                    if (set(binding) != {"nodeId", "parameterId"} or
                            not isinstance(parameter_id, str)):
                        continue
                    binding_key = (source_id, parameter_id)
                    if binding_key in copied_binding_keys:
                        continue
                    operation = operations[source_id]
                    definition = next((item for item in operation.parameters if item.parameter_id == parameter_id), None)
                    if definition is None or parameter.get("valueType") != definition.value_type:
                        continue
                    copied_binding_keys.add(binding_key)
                    new_bindings.append({"nodeId": source_to_clone[source_id], "parameterId": parameter_id})
                parameter["bindings"].extend(new_bindings)

    cloned_links = []
    for link_index, link in enumerate(links):
        if not isinstance(link, dict) or not isinstance(link.get("from"), dict) or not isinstance(link.get("to"), dict):
            continue
        source_endpoint, target_endpoint = link["from"], link["to"]
        source_id, target_id = source_endpoint.get("nodeId"), target_endpoint.get("nodeId")
        if (not isinstance(source_id, str) or not isinstance(target_id, str) or
                source_id not in source_to_clone or target_id not in source_to_clone):
            continue
        cloned_links.append((source_to_clone[source_id], source_endpoint.get("socketId"),
                             source_to_clone[target_id], target_endpoint.get("socketId")))
    if cloned_links:
        edited = connect_nodes_batch(edited, registry, cloned_links)
    return edited


def copy_nodes_subgraph(document: Any, node_ids: list[str],
                       registry: dict[tuple[str, int], Operation] | None = None) -> dict[str, Any]:
    """Create a portable JSON clipboard envelope for a selected subgraph."""
    if not isinstance(node_ids, list) or not node_ids or any(not isinstance(item, str) for item in node_ids):
        raise ContractError("node IDs must be a non-empty string list",
                            code="clipboard.selection.invalid", path="/nodeIds")
    if len(set(node_ids)) != len(node_ids):
        raise ContractError("node IDs must be unique", code="clipboard.selection.duplicate", path="/nodeIds")
    preserved = preserve_document(document)
    nodes, links = preserved.get("nodes"), preserved.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays",
                            code="schema.collection.required", path="/nodes")
    node_matches_by_id: dict[str, list[dict[str, Any]]] = {}
    for node in nodes:
        if isinstance(node, dict) and isinstance(node.get("nodeId"), str):
            node_matches_by_id.setdefault(node["nodeId"], []).append(node)
    selected = {}
    for index, node_id in enumerate(node_ids):
        matches = node_matches_by_id.get(node_id, [])
        if len(matches) != 1:
            raise ContractError(f"node {node_id} is missing or ambiguous",
                                code="node.id.not_unique_or_missing", path=f"/nodeIds/{index}")
        selected[node_id] = matches[0]
    internal_links = []
    for link in links:
        if not isinstance(link, dict) or not isinstance(link.get("from"), dict) or not isinstance(link.get("to"), dict):
            continue
        source_id, target_id = link["from"].get("nodeId"), link["to"].get("nodeId")
        if isinstance(source_id, str) and isinstance(target_id, str) and source_id in selected and target_id in selected:
            internal_links.append(link)
    graph_parameters = []
    if preserved.get("schemaVersion") in (2, 3):
        raw_parameters = preserved.get("graphParameters")
        if isinstance(raw_parameters, list):
            for index, parameter in enumerate(raw_parameters):
                required_fields = {"parameterId", "label", "valueType", "value", "bindings"}
                if not isinstance(parameter, dict) or not isinstance(parameter.get("bindings"), list):
                    continue
                relevant_bindings = [
                    (binding_index, binding)
                    for binding_index, binding in enumerate(parameter["bindings"])
                if isinstance(binding, dict) and isinstance(binding.get("nodeId"), str) and
                binding["nodeId"] in selected
                ]
                if not relevant_bindings:
                    continue
                if set(parameter) != required_fields:
                    raise ContractError("graph parameter shape is invalid",
                                        code="graph.parameter.shape.invalid", path=f"/graphParameters/{index}")
                bindings = []
                for binding_index, binding in relevant_bindings:
                    if (set(binding) != {"nodeId", "parameterId"} or
                            not isinstance(binding.get("nodeId"), str) or
                            not isinstance(binding.get("parameterId"), str)):
                        raise ContractError("graph parameter binding shape is invalid",
                                            code="graph.parameter.binding.invalid", path=f"/graphParameters/{index}/bindings/{binding_index}")
                    bindings.append(binding)
                if bindings:
                    if (not isinstance(parameter.get("parameterId"), str) or
                            not IDENTIFIER.fullmatch(parameter["parameterId"]) or
                            not isinstance(parameter.get("label"), str) or not parameter["label"].strip()):
                        raise ContractError("bound graph parameter metadata is invalid",
                                            code="graph.parameter.shape.invalid", path=f"/graphParameters/{index}")
                    _canonical_graph_parameter_value(parameter.get("valueType"), parameter.get("value"),
                                                     f"/graphParameters/{index}/value")
                    copied = json.loads(canonical_json_text(parameter))
                    copied["bindings"] = bindings
                    graph_parameters.append(copied)
    registry_lock = {}
    raw_lock = preserved.get("registryContractLock")
    if "registryContractLock" in preserved and not isinstance(raw_lock, dict):
        raise ContractError("registry contract lock must be an object",
                            code="registry.lock.invalid", path="/registryContractLock")
    if isinstance(raw_lock, dict):
        used_keys = {f"{node.get('operationId')}@{node.get('semanticVersion')}"
                     for node in selected.values() if isinstance(node.get("operationId"), str)}
        registry_lock = {key: value for key, value in raw_lock.items() if key in used_keys}
    if registry is not None:
        current_lock = registry_contract_lock(registry)
        for node in selected.values():
            operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
            if operation is None:
                raise ContractError("selected node operation is unavailable; a portable clipboard contract cannot be produced",
                                    code="clipboard.operation.unavailable", path="/nodes")
            key = f"{operation.operation_id}@{operation.semantic_version}"
            saved_hash = registry_lock.get(key)
            if saved_hash is not None and saved_hash != current_lock[key]:
                raise ContractError(f"source operation contract differs: {key}",
                                    code="registry.operation.contract_changed",
                                    path=f"/registryContractLock/{_pointer_segment(key)}")
            registry_lock[key] = current_lock[key]
    layouts = {node_id: selected[node_id].get("layout") for node_id in node_ids}
    if any(not isinstance(layout, dict) for layout in layouts.values()):
        raise ContractError("selected node layout must be an object",
                            code="node.layout.invalid", path="/nodes")
    coordinates = {
        node_id: (
            _f32(layout.get("x"), "node layout x", path=f"/nodes/{node_id}/layout/x", code="node.layout.coordinate.invalid"),
            _f32(layout.get("y"), "node layout y", path=f"/nodes/{node_id}/layout/y", code="node.layout.coordinate.invalid"),
        )
        for node_id, layout in layouts.items()
    }
    origin_x = min(point[0] for point in coordinates.values())
    origin_y = min(point[1] for point in coordinates.values())
    max_x = max(point[0] for point in coordinates.values())
    max_y = max(point[1] for point in coordinates.values())
    copied_nodes = []
    for node_id in node_ids:
        copied = json.loads(canonical_json_text(selected[node_id]))
        x, y = coordinates[node_id]
        copied["layout"] = {"x": x - origin_x, "y": y - origin_y}
        copied_nodes.append(copied)
    carries_image_nodes = any(
        isinstance(node, dict) and
        node.get("operationId") == "artifact.texture.sample.image2d"
        for node in copied_nodes)
    copied_resources = []
    if carries_image_nodes:
        raw_resources = preserved.get("resources", [])
        _, source_resources_by_id = _canonical_graph_resources(raw_resources)
        selected_resource_ids = {
            node["resourceReference"].get("resourceId")
            for node in copied_nodes
            if isinstance(node.get("resourceReference"), dict)
        }
        copied_resources = [source_resources_by_id[resource_id]
                            for resource_id in sorted(selected_resource_ids)
                            if resource_id in source_resources_by_id]
    payload = {
        "kind": "artifact-shader-graph-rd-subgraph",
        "schemaVersion": (4 if carries_image_nodes else
                          3 if any("socketDefaults" in node for node in copied_nodes) else 2),
        "nodes": copied_nodes,
        "links": internal_links,
        "graphParameters": graph_parameters,
        "registryContractLock": registry_lock,
        "layoutBounds": {"width": max_x - origin_x, "height": max_y - origin_y},
    }
    if carries_image_nodes:
        payload["resources"] = copied_resources
    _reject_code_payload(payload)
    return payload


def paste_nodes_subgraph(document: Any, registry: dict[tuple[str, int], Operation],
                         clipboard: Any, new_node_ids: dict[str, str], *,
                         offset_x: float = 32.0, offset_y: float = 32.0) -> dict[str, Any]:
    """Paste a clipboard subgraph with fresh node/parameter IDs and internal links."""
    _validate_json_value(clipboard)
    if (not isinstance(clipboard, dict) or
            clipboard.get("kind") != "artifact-shader-graph-rd-subgraph" or
            isinstance(clipboard.get("schemaVersion"), bool) or
            clipboard.get("schemaVersion") not in (1, 2, 3, 4)):
        raise ContractError("clipboard payload kind or schema is unsupported",
                            code="clipboard.schema.unsupported", path="/")
    clipboard_version = clipboard["schemaVersion"]
    expected_fields = {"kind", "schemaVersion", "nodes", "links", "graphParameters", "registryContractLock"}
    if clipboard_version in (2, 3, 4):
        expected_fields.add("layoutBounds")
    if clipboard_version == 4:
        expected_fields.add("resources")
    if (set(clipboard) != expected_fields or
            clipboard.get("kind") != "artifact-shader-graph-rd-subgraph" or
            clipboard_version not in (1, 2, 3, 4)):
        raise ContractError("clipboard payload kind or schema is unsupported",
                            code="clipboard.schema.unsupported", path="/")
    _reject_code_payload(clipboard)
    source_nodes, source_links = clipboard.get("nodes"), clipboard.get("links")
    if not isinstance(source_nodes, list) or not source_nodes or not isinstance(source_links, list):
        raise ContractError("clipboard nodes and links must be arrays",
                            code="clipboard.shape.invalid", path="/")
    bounds = clipboard.get("layoutBounds")
    if clipboard_version in (2, 3, 4):
        if not isinstance(bounds, dict) or set(bounds) != {"width", "height"}:
            raise ContractError("clipboard layout bounds are invalid",
                                code="clipboard.layout_bounds.invalid", path="/layoutBounds")
        bound_width = _f32(bounds.get("width"), "clipboard layout width", path="/layoutBounds/width",
                           code="clipboard.layout_bounds.invalid")
        bound_height = _f32(bounds.get("height"), "clipboard layout height", path="/layoutBounds/height",
                            code="clipboard.layout_bounds.invalid")
        if bound_width < 0 or bound_height < 0:
            raise ContractError("clipboard layout bounds must be non-negative",
                                code="clipboard.layout_bounds.invalid", path="/layoutBounds")
    else:
        bound_width = bound_height = 0.0
    if not isinstance(new_node_ids, dict) or not new_node_ids:
        raise ContractError("new node ID map is required",
                            code="clipboard.node_map.invalid", path="/newNodeIds")
    source_ids = [node.get("nodeId") for node in source_nodes if isinstance(node, dict)]
    if len(source_ids) != len(source_nodes) or any(not isinstance(item, str) for item in source_ids) or len(set(source_ids)) != len(source_ids):
        raise ContractError("clipboard node IDs must be unique strings",
                            code="clipboard.node_id.invalid", path="/nodes")
    if set(new_node_ids) != set(source_ids) or any(not isinstance(value, str) or not IDENTIFIER.fullmatch(value) for value in new_node_ids.values()):
        raise ContractError("new node ID map must cover every clipboard node with valid IDs",
                            code="clipboard.node_map.invalid", path="/newNodeIds")
    if len(set(new_node_ids.values())) != len(new_node_ids):
        raise ContractError("new node IDs must be unique",
                            code="clipboard.node_map.duplicate", path="/newNodeIds")
    x_offset = _f32(offset_x, "paste anchor x", path="/offsetX", code="node.layout.coordinate.invalid")
    y_offset = _f32(offset_y, "paste anchor y", path="/offsetY", code="node.layout.coordinate.invalid")
    edited = preserve_document(document)
    if (isinstance(edited.get("schemaVersion"), bool) or not isinstance(edited.get("schemaVersion"), int) or
            edited.get("schemaVersion") not in (1, 2, 3)):
        raise ContractError("target document schemaVersion is unsupported",
                            code="schema.version.unsupported", path="/schemaVersion")
    if edited.get("kind") != "artifact-shader-graph-rd-document":
        raise ContractError("target document kind is unsupported",
                            code="document.kind.unsupported", path="/kind")
    target_nodes = edited.get("nodes")
    target_links = edited.get("links")
    if not isinstance(target_nodes, list) or not isinstance(target_links, list):
        raise ContractError("target document nodes and links must be arrays",
                            code="schema.collection.required", path="/nodes")
    existing_ids = {node.get("nodeId") for node in target_nodes if isinstance(node, dict) and isinstance(node.get("nodeId"), str)}
    collision = next((node_id for node_id in new_node_ids.values() if node_id in existing_ids), None)
    if collision is not None:
        raise ContractError(f"node ID {collision} already exists",
                            code="node.id.duplicate", path="/newNodeIds")
    if clipboard_version in (3, 4) and edited.get("schemaVersion") != 3 and any(
            isinstance(node, dict) and "socketDefaults" in node for node in source_nodes):
        _upgrade_document_to_socket_defaults(edited, registry)
    if clipboard_version == 4:
        incoming_resources, incoming_resources_by_id = _canonical_graph_resources(
            clipboard.get("resources"), path="/resources")
        target_resources, target_resources_by_id = _canonical_graph_resources(
            edited.get("resources", []))
        for resource_id, resource in incoming_resources_by_id.items():
            existing = target_resources_by_id.get(resource_id)
            if existing is not None and existing != resource:
                raise ContractError("clipboard resource ID conflicts with the target document",
                                    code="resource.reference.resource.conflict",
                                    path=f"/resources/{_pointer_segment(resource_id)}")
            if existing is None:
                target_resources.append(resource)
        edited["resources"] = target_resources
    clones = []
    layout_points = []
    for index, source in enumerate(source_nodes):
        clone = json.loads(canonical_json_text(source))
        layout = clone.get("layout")
        if not isinstance(layout, dict):
            raise ContractError("clipboard node layout must be an object",
                                code="node.layout.invalid", path=f"/nodes/{index}/layout")
        clone["nodeId"] = new_node_ids[source["nodeId"]]
        if isinstance(clone.get("resourceReference"), dict):
            clone["resourceReference"]["resourceKey"] = (
                f"artifact.shadergraph.image.{clone['nodeId']}")
        source_x = _f32(layout.get("x"), "clipboard layout x", path=f"/nodes/{index}/layout/x", code="node.layout.coordinate.invalid")
        source_y = _f32(layout.get("y"), "clipboard layout y", path=f"/nodes/{index}/layout/y", code="node.layout.coordinate.invalid")
        if clipboard_version in (2, 3, 4) and (source_x < 0 or source_y < 0 or source_x > bound_width or source_y > bound_height):
            raise ContractError("clipboard node layout exceeds its bounds",
                                code="clipboard.layout_bounds.invalid", path=f"/nodes/{index}/layout")
        layout_points.append((source_x, source_y))
        clone["layout"] = {
            "x": _f32(source_x + x_offset,
                      "paste layout x", path=f"/nodes/{index}/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(source_y + y_offset,
                      "paste layout y", path=f"/nodes/{index}/layout/y", code="node.layout.coordinate.invalid"),
        }
        if edited.get("schemaVersion") == 3:
            operation = _lookup_operation(registry, clone.get("operationId"), clone.get("semanticVersion"))
            if operation is not None:
                clone["socketDefaults"] = _canonical_socket_defaults(
                    clone.get("socketDefaults", {}), operation, clone["nodeId"], index)
        clones.append(clone)
    if clipboard_version in (2, 3, 4) and (max(point[0] for point in layout_points) != bound_width or
                                   max(point[1] for point in layout_points) != bound_height or
                                   min(point[0] for point in layout_points) != 0 or
                                   min(point[1] for point in layout_points) != 0):
        raise ContractError("clipboard layout bounds do not match its nodes",
                            code="clipboard.layout_bounds.invalid", path="/layoutBounds")
    edited["nodes"].extend(clones)
    cloned_links = []
    cloned_connections = []
    for index, link in enumerate(source_links):
        if not isinstance(link, dict) or not isinstance(link.get("from"), dict) or not isinstance(link.get("to"), dict):
            raise ContractError("clipboard link endpoints are invalid",
                                code="link.invalid", path=f"/links/{index}")
        source_id, target_id = link["from"].get("nodeId"), link["to"].get("nodeId")
        source_socket, target_socket = link["from"].get("socketId"), link["to"].get("socketId")
        if (not isinstance(source_id, str) or not isinstance(target_id, str) or
                source_id not in new_node_ids or target_id not in new_node_ids):
            raise ContractError("clipboard links must stay within the copied subgraph",
                                code="clipboard.link.external", path=f"/links/{index}")
        if not isinstance(source_socket, str) or not isinstance(target_socket, str):
            raise ContractError("clipboard link socket IDs must be strings",
                                code="link.endpoint.required", path=f"/links/{index}")
        cloned_link = json.loads(canonical_json_text(link))
        cloned_link["from"]["nodeId"] = new_node_ids[source_id]
        cloned_link["to"]["nodeId"] = new_node_ids[target_id]
        cloned_links.append(cloned_link)
        cloned_connections.append((new_node_ids[source_id], source_socket,
                                   new_node_ids[target_id], target_socket))
    if cloned_connections:
        # Validate the clipboard subgraph in isolation so malformed target links
        # remain untouched while imported links still obey the graph contract.
        link_validation_document = {"nodes": clones, "links": []}
        connect_nodes_batch(link_validation_document, registry, cloned_connections)
    edited["links"].extend(cloned_links)

    source_parameters = clipboard.get("graphParameters", [])
    if not isinstance(source_parameters, list):
        raise ContractError("clipboard graph parameters must be an array",
                            code="clipboard.shape.invalid", path="/graphParameters")
    if source_parameters:
        if edited.get("schemaVersion") == 1:
            edited["schemaVersion"] = 2
            edited["graphParameters"] = []
        target_parameters = edited.get("graphParameters")
        if not isinstance(target_parameters, list):
            raise ContractError("target graphParameters must be an array",
                                code="schema.collection.required", path="/graphParameters")
        used_parameter_ids = {item.get("parameterId") for item in target_parameters
                              if isinstance(item, dict) and isinstance(item.get("parameterId"), str)}
        source_parameter_ids: set[str] = set()
        source_bindings: set[tuple[str, str]] = set()
        clones_by_id = {clone["nodeId"]: (index, clone) for index, clone in enumerate(clones)}
        for index, raw_parameter in enumerate(source_parameters):
            if (not isinstance(raw_parameter, dict) or
                    set(raw_parameter) != {"parameterId", "label", "valueType", "value", "bindings"} or
                    not isinstance(raw_parameter.get("parameterId"), str) or
                    not IDENTIFIER.fullmatch(raw_parameter["parameterId"]) or
                    not isinstance(raw_parameter.get("label"), str) or not raw_parameter["label"].strip() or
                    not isinstance(raw_parameter.get("bindings"), list) or not raw_parameter["bindings"]):
                raise ContractError("clipboard graph parameter record is invalid",
                                    code="graph.parameter.shape.invalid", path=f"/graphParameters/{index}")
            source_parameter_id = raw_parameter["parameterId"]
            if source_parameter_id in source_parameter_ids:
                raise ContractError("clipboard graph parameter ID is duplicated",
                                    code="graph.parameter.id.duplicate", path=f"/graphParameters/{index}/parameterId")
            source_parameter_ids.add(source_parameter_id)
            canonical_value = _canonical_graph_parameter_value(
                raw_parameter.get("valueType"), raw_parameter.get("value"),
                f"/graphParameters/{index}/value")
            copied_parameter = json.loads(canonical_json_text(raw_parameter))
            base_id = copied_parameter["parameterId"] + "_paste"
            parameter_id = base_id
            suffix = 2
            while parameter_id in used_parameter_ids:
                parameter_id = f"{base_id}_{suffix}"
                suffix += 1
            used_parameter_ids.add(parameter_id)
            copied_parameter["parameterId"] = parameter_id
            for binding_index, binding in enumerate(copied_parameter["bindings"]):
                if (not isinstance(binding, dict) or set(binding) != {"nodeId", "parameterId"} or
                        not isinstance(binding.get("nodeId"), str) or
                        binding.get("nodeId") not in new_node_ids or
                        not isinstance(binding.get("parameterId"), str)):
                    raise ContractError("clipboard graph parameter binding is invalid",
                                        code="graph.parameter.binding.invalid", path=f"/graphParameters/{index}/bindings/{binding_index}")
                source_binding_key = (binding["nodeId"], binding["parameterId"])
                if source_binding_key in source_bindings:
                    raise ContractError("clipboard node parameter has duplicate graph bindings",
                                        code="graph.parameter.binding.duplicate",
                                        path=f"/graphParameters/{index}/bindings/{binding_index}")
                source_bindings.add(source_binding_key)
                clone_index, clone = clones_by_id[new_node_ids[binding["nodeId"]]]
                operation = _lookup_operation(registry, clone.get("operationId"), clone.get("semanticVersion"))
                definition = (next((item for item in operation.parameters
                                    if item.parameter_id == binding["parameterId"]), None)
                              if operation is not None else None)
                if definition is None or definition.value_type != raw_parameter["valueType"]:
                    raise ContractError("clipboard graph parameter binding is incompatible",
                                        code="graph.parameter.type.incompatible",
                                        path=f"/graphParameters/{index}/bindings/{binding_index}/parameterId")
                node_values = _canonical_parameters(clone.get("parameters", {}), operation,
                                                     clone["nodeId"], clone_index)
                if node_values[binding["parameterId"]] != canonical_value:
                    raise ContractError("clipboard graph parameter value differs from its node value",
                                        code="graph.parameter.binding.value_mismatch",
                                        path=f"/graphParameters/{index}/bindings/{binding_index}")
                binding["nodeId"] = new_node_ids[binding["nodeId"]]
            target_parameters.append(copied_parameter)

    source_lock = clipboard.get("registryContractLock", {})
    if not isinstance(source_lock, dict):
        raise ContractError("clipboard registry contract lock must be an object",
                            code="registry.lock.invalid", path="/registryContractLock")
    required_lock_keys = set()
    for index, source in enumerate(source_nodes):
        operation_id = source.get("operationId")
        semantic_version = source.get("semanticVersion")
        if (not isinstance(operation_id, str) or isinstance(semantic_version, bool) or
                not isinstance(semantic_version, int) or semantic_version < 1):
            raise ContractError("clipboard node operation identity is invalid",
                                code="node.operation.invalid", path=f"/nodes/{index}")
        required_lock_keys.add(f"{operation_id}@{semantic_version}")
    if set(source_lock) != required_lock_keys:
        raise ContractError("clipboard registry locks must exactly cover copied node operations",
                            code="registry.lock.coverage.invalid", path="/registryContractLock")
    if source_lock:
        target_lock = edited.get("registryContractLock")
        if target_lock is None:
            target_lock = {}
            edited["registryContractLock"] = target_lock
        if not isinstance(target_lock, dict):
            raise ContractError("target registry contract lock must be an object",
                                code="registry.lock.invalid", path="/registryContractLock")
        current_lock = registry_contract_lock(registry)
        for key, value in source_lock.items():
            if key not in current_lock or current_lock[key] != value:
                raise ContractError(f"clipboard operation contract is unavailable or changed: {key}",
                                    code="registry.operation.contract_changed", path=f"/registryContractLock/{_pointer_segment(str(key))}")
            if key in target_lock and target_lock[key] != value:
                raise ContractError(f"target operation contract differs: {key}",
                                    code="registry.operation.contract_changed", path=f"/registryContractLock/{_pointer_segment(str(key))}")
            target_lock[key] = value
    return edited


def compare_registry_contracts(saved_lock: Any, registry: dict[tuple[str, int], Operation]) -> list[dict[str, str]]:
    if not isinstance(saved_lock, dict):
        raise ContractError("saved registry contract lock must be an object", code="registry.lock.invalid", path="/registryContractLock")
    current = registry_contract_lock(registry)
    diagnostics = []
    for key, saved_hash in sorted(saved_lock.items()):
        if key not in current:
            diagnostics.append(ContractError(f"saved operation {key} is no longer registered", code="registry.operation.removed", path=f"/registryContractLock/{_pointer_segment(str(key))}").to_json())
        elif saved_hash != current[key]:
            diagnostics.append(ContractError(f"operation contract changed without a semantic version change: {key}", code="registry.operation.contract_changed", path=f"/registryContractLock/{_pointer_segment(str(key))}").to_json())
    return diagnostics


def seal_document(document: Any, registry: dict[tuple[str, int], Operation]) -> dict[str, Any]:
    """Attach semantic contracts for every currently available node operation."""
    sealed = preserve_document(document)
    nodes = sealed.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    keys = {(node.get("operationId"), node.get("semanticVersion")) for node in nodes if isinstance(node, dict)}
    lock = registry_contract_lock(registry)
    sealed["registryContractLock"] = {f"{operation_id}@{version}": lock[f"{operation_id}@{version}"]
                                      for operation_id, version in sorted(keys)
                                      if (operation_id, version) in registry and f"{operation_id}@{version}" in lock}
    return sealed


def migrate_document_versions(document: Any, registry: dict[tuple[str, int], Operation],
                              migrations: dict[str, str]) -> dict[str, Any]:
    """Apply explicit identity-preserving version migrations; never auto-upgrade."""
    migrated = preserve_document(document)
    compatibility = compare_registry_contracts(migrated.get("registryContractLock"), registry)
    if compatibility:
        issue = compatibility[0]
        raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    nodes = migrated.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            continue
        source_key = f"{node.get('operationId')}@{node.get('semanticVersion')}"
        target_key = migrations.get(source_key)
        if target_key is None:
            continue
        try:
            operation_id, version_text = target_key.rsplit("@", 1)
            target_version = int(version_text)
        except (ValueError, AttributeError) as error:
            raise ContractError("migration target must be operationId@positiveVersion", code="registry.migration.target.invalid", path=f"/nodes/{index}/semanticVersion") from error
        source = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
        target = registry.get((operation_id, target_version))
        if source is None or target is None:
            raise ContractError("migration requires both registered source and target operations", code="registry.migration.operation.missing", path=f"/nodes/{index}/semanticVersion")
        if source.operation_id != target.operation_id or _operation_semantics_without_version(source) != _operation_semantics_without_version(target):
            raise ContractError("automatic migration is limited to identity-preserving version changes", code="registry.migration.semantic_change_requires_transform", path=f"/nodes/{index}/semanticVersion")
        node["semanticVersion"] = target_version
    migrated.pop("registryContractLock", None)
    return seal_document(migrated, registry)


def _canonical_parameters(raw: Any, operation: Operation, node_id: str, node_index: int) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise ContractError(f"node {node_id} parameters must be an object", code="node.parameters.invalid", path=f"/nodes/{node_index}/parameters")
    definitions = {parameter.parameter_id: parameter for parameter in operation.parameters}
    unknown = set(raw) - set(definitions)
    if unknown:
        parameter_id = sorted(unknown)[0]
        raise ContractError(f"node {node_id} has unknown parameters: {sorted(unknown)}", code="node.parameter.unknown", path=f"/nodes/{node_index}/parameters/{_pointer_segment(str(parameter_id))}")
    result = {}
    for parameter_id, definition in definitions.items():
        if parameter_id in raw:
            value = raw[parameter_id]
        elif definition.default is not None:
            value = definition.default
        else:
            raise ContractError(f"node {node_id} is missing required parameter {parameter_id}", code="node.parameter.required_missing", path=f"/nodes/{node_index}/parameters/{_pointer_segment(parameter_id)}")
        parameter_path = f"/nodes/{node_index}/parameters/{_pointer_segment(parameter_id)}"
        if definition.value_type == "Float32":
            result[parameter_id] = _f32(
                value,
                f"node {node_id}.{parameter_id}",
                path=parameter_path,
                code="parameter.float32.invalid",
            )
        elif definition.value_type == "Vector4F32":
            if not isinstance(value, (list, tuple)) or len(value) != 4:
                raise ContractError(f"node {node_id}.{parameter_id} must contain four Float32 values",
                                    code="parameter.vector4.invalid", path=parameter_path)
            result[parameter_id] = [
                _f32(component, f"node {node_id}.{parameter_id}[{index}]",
                     path=f"{parameter_path}/{index}", code="parameter.float32.invalid")
                for index, component in enumerate(value)
            ]
        elif definition.value_type == "Vector3F32":
            if not isinstance(value, (list, tuple)) or len(value) != 3:
                raise ContractError(f"node {node_id}.{parameter_id} must contain three Float32 values",
                                    code="parameter.vector3.invalid", path=parameter_path)
            result[parameter_id] = [
                _f32(component, f"node {node_id}.{parameter_id}[{index}]",
                     path=f"{parameter_path}/{index}", code="parameter.float32.invalid")
                for index, component in enumerate(value)
            ]
        elif definition.value_type == "RampStops":
            result[parameter_id] = _canonical_ramp_stops(
                value, parameter_path, code="parameter.ramp_stops.invalid")
        else:
            raise ContractError(f"node {node_id}.{parameter_id} has an unsupported parameter type",
                                code="parameter.type.unsupported", path=parameter_path)
    return result


def _canonical_graph_parameter_value(value_type: str, value: Any, path: str) -> float | list[float]:
    if value_type == "Float32":
        return _f32(value, "graph parameter", path=path, code="graph.parameter.value.invalid")
    if value_type in {"Vector3F32", "Vector4F32"}:
        lane_count = 3 if value_type == "Vector3F32" else 4
        if not isinstance(value, (list, tuple)) or len(value) != lane_count:
            raise ContractError(f"{value_type[:-3]} graph parameter requires {lane_count} Float32 values",
                                code="graph.parameter.value.invalid", path=path)
        return [_f32(component, f"graph parameter[{index}]", path=f"{path}/{index}",
                      code="graph.parameter.value.invalid") for index, component in enumerate(value)]
    raise ContractError("graph parameter valueType is unsupported",
                        code="graph.parameter.type.invalid", path=path)


def _apply_graph_parameters(document: dict[str, Any], nodes: dict[str, dict[str, Any]],
                            node_indices: dict[str, int]) -> list[dict[str, Any]]:
    schema_version = document.get("schemaVersion")
    for node in nodes.values():
        node["graphParameterBindings"] = {}
    if schema_version == 1:
        if "graphParameters" in document:
            raise ContractError("graphParameters require document schema version 2",
                                code="schema.field.version_mismatch", path="/graphParameters")
        return []
    raw_parameters = document.get("graphParameters")
    if not isinstance(raw_parameters, list):
        raise ContractError("schema version 2 requires a graphParameters array",
                            code="schema.collection.required", path="/graphParameters")
    seen_ids: set[str] = set()
    seen_bindings: set[tuple[str, str]] = set()
    result: list[dict[str, Any]] = []
    for index, raw in enumerate(raw_parameters):
        path = f"/graphParameters/{index}"
        required = {"parameterId", "label", "valueType", "value", "bindings"}
        if not isinstance(raw, dict) or set(raw) != required:
            unknown = sorted(set(raw) - required) if isinstance(raw, dict) else []
            field_path = f"{path}/{_pointer_segment(str(unknown[0]))}" if unknown else path
            raise ContractError("graph parameter has missing or unknown fields",
                                code="graph.parameter.shape.invalid", path=field_path)
        parameter_id, label, value_type = raw["parameterId"], raw["label"], raw["valueType"]
        if not isinstance(parameter_id, str) or not IDENTIFIER.fullmatch(parameter_id):
            raise ContractError("graph parameter ID is invalid", code="graph.parameter.id.invalid",
                                path=f"{path}/parameterId")
        if parameter_id in seen_ids:
            raise ContractError("graph parameter ID is duplicated", code="graph.parameter.id.duplicate",
                                path=f"{path}/parameterId")
        seen_ids.add(parameter_id)
        if not isinstance(label, str) or not label.strip():
            raise ContractError("graph parameter label is empty", code="graph.parameter.label.invalid",
                                path=f"{path}/label")
        if not isinstance(value_type, str) or value_type not in {"Float32", "Vector3F32", "Vector4F32"}:
            raise ContractError("graph parameter type is unsupported", code="graph.parameter.type.invalid",
                                path=f"{path}/valueType")
        value = _canonical_graph_parameter_value(value_type, raw["value"], f"{path}/value")
        bindings = raw["bindings"]
        if not isinstance(bindings, list) or not bindings:
            raise ContractError("graph parameter requires at least one binding",
                                code="graph.parameter.binding.required", path=f"{path}/bindings")
        canonical_bindings = []
        for binding_index, binding in enumerate(bindings):
            binding_path = f"{path}/bindings/{binding_index}"
            if not isinstance(binding, dict) or set(binding) != {"nodeId", "parameterId"}:
                raise ContractError("graph parameter binding shape is invalid",
                                    code="graph.parameter.binding.invalid", path=binding_path)
            node_id, node_parameter_id = binding["nodeId"], binding["parameterId"]
            if not isinstance(node_id, str) or node_id not in nodes:
                raise ContractError("graph parameter binding node is missing",
                                    code="graph.parameter.binding.node_missing", path=f"{binding_path}/nodeId")
            if not isinstance(node_parameter_id, str):
                raise ContractError("bound node parameter ID must be a string",
                                    code="graph.parameter.binding.invalid",
                                    path=f"{binding_path}/parameterId")
            binding_key = (node_id, node_parameter_id)
            if binding_key in seen_bindings:
                raise ContractError("node parameter has multiple graph parameter bindings",
                                    code="graph.parameter.binding.duplicate", path=binding_path)
            operation = nodes[node_id]["operation"]
            definition = next((item for item in operation.parameters
                               if item.parameter_id == node_parameter_id), None)
            if definition is None:
                raise ContractError("bound node parameter is unavailable",
                                    code="graph.parameter.binding.parameter_missing",
                                    path=f"{binding_path}/parameterId")
            if definition.value_type != value_type:
                raise ContractError("graph and node parameter types differ",
                                    code="graph.parameter.type.incompatible",
                                    path=f"{binding_path}/parameterId")
            current_value = nodes[node_id]["parameters"][node_parameter_id]
            if current_value != value:
                raise ContractError("bound node parameter value differs from its graph parameter",
                                    code="graph.parameter.binding.value_mismatch",
                                    path=f"/nodes/{node_indices[node_id]}/parameters/{_pointer_segment(node_parameter_id)}")
            seen_bindings.add(binding_key)
            nodes[node_id]["graphParameterBindings"][node_parameter_id] = parameter_id
            nodes[node_id]["parameters"][node_parameter_id] = list(value) if isinstance(value, list) else value
            canonical_bindings.append({"nodeId": node_id, "parameterId": node_parameter_id})
        result.append({"parameterId": parameter_id, "valueType": value_type, "value": value,
                       "bindings": canonical_bindings})
    return result


def expose_node_parameter(document: Any, registry: dict[tuple[str, int], Operation],
                          node_id: str, node_parameter_id: str, graph_parameter_id: str,
                          display_name: str) -> dict[str, Any]:
    """Expose a node value or bind another same-typed node value to it."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("node ID is invalid", code="node.id.invalid", path="/nodeId")
    if not isinstance(node_parameter_id, str) or not IDENTIFIER.fullmatch(node_parameter_id):
        raise ContractError("node parameter ID is invalid", code="node.parameter.id.invalid",
                            path="/parameterId")
    if not isinstance(graph_parameter_id, str) or not IDENTIFIER.fullmatch(graph_parameter_id):
        raise ContractError("graph parameter ID is invalid", code="graph.parameter.id.invalid", path="/graphParameters/parameterId")
    if not isinstance(display_name, str) or not display_name.strip():
        raise ContractError("graph parameter label must not be empty", code="graph.parameter.label.invalid", path="/graphParameters/label")
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    nodes_by_id: dict[str, list[tuple[int, dict[str, Any]]]] = {}
    for index, candidate in enumerate(nodes):
        if isinstance(candidate, dict) and isinstance(candidate.get("nodeId"), str):
            nodes_by_id.setdefault(candidate["nodeId"], []).append((index, candidate))
    matches = nodes_by_id.get(node_id, []) if isinstance(node_id, str) else []
    if len(matches) != 1:
        raise ContractError("node is missing or ambiguous", code="node.id.not_unique_or_missing", path="/nodes")
    node_index, node = matches[0]
    operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    if operation is None:
        raise ContractError("node operation is unavailable", code="node.operation.version.unsupported",
                            path=f"/nodes/{node_index}/semanticVersion")
    parameter = next((item for item in operation.parameters if item.parameter_id == node_parameter_id), None)
    if parameter is None:
        raise ContractError("node parameter is unavailable", code="node.parameter.unknown",
                            path=f"/nodes/{node_index}/parameters/{_pointer_segment(node_parameter_id)}")
    if parameter.value_type not in {"Float32", "Vector3F32", "Vector4F32"}:
        raise ContractError("node parameter type has no graph-parameter packing contract",
                            code="graph.parameter.type.unsupported",
                            path=f"/nodes/{node_index}/parameters/{_pointer_segment(node_parameter_id)}")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    if edited.get("schemaVersion") == 1:
        edited["schemaVersion"] = 2
        edited["graphParameters"] = []
    if edited.get("schemaVersion") not in (2, 3) or not isinstance(edited.get("graphParameters"), list):
        raise ContractError("graph parameter exposure requires document schema version 2",
                            code="schema.version.unsupported", path="/schemaVersion")
    graph_parameters = edited["graphParameters"]
    target_matches = [item for item in graph_parameters
                      if isinstance(item, dict) and item.get("parameterId") == graph_parameter_id]
    if len(target_matches) > 1:
        raise ContractError("graph parameter ID is ambiguous", code="graph.parameter.id.duplicate",
                            path=f"/graphParameters/{_pointer_segment(graph_parameter_id)}")
    target = target_matches[0] if target_matches else None
    target_value = None
    if target is not None and (
            set(target) != {"parameterId", "label", "valueType", "value", "bindings"} or
            not isinstance(target.get("bindings"), list)):
        raise ContractError("target graph parameter shape is invalid",
                            code="graph.parameter.shape.invalid",
                            path=f"/graphParameters/{_pointer_segment(graph_parameter_id)}")
    if target is not None:
        if not isinstance(target.get("label"), str) or not target["label"].strip():
            raise ContractError("graph parameter label must not be empty",
                                code="graph.parameter.label.invalid",
                                path=f"/graphParameters/{_pointer_segment(graph_parameter_id)}/label")
        target_value = _canonical_graph_parameter_value(
            target.get("valueType"), target.get("value"),
            f"/graphParameters/{_pointer_segment(graph_parameter_id)}/value")
        existing_bindings: set[tuple[str, str]] = set()
        for binding_index, existing_binding in enumerate(target["bindings"]):
            if (not isinstance(existing_binding, dict) or
                    set(existing_binding) != {"nodeId", "parameterId"} or
                    not isinstance(existing_binding.get("nodeId"), str) or
                    not isinstance(existing_binding.get("parameterId"), str)):
                raise ContractError("target graph parameter binding shape is invalid",
                                    code="graph.parameter.binding.invalid",
                                    path=f"/graphParameters/{_pointer_segment(graph_parameter_id)}/bindings/{binding_index}")
            binding_key = (existing_binding["nodeId"], existing_binding["parameterId"])
            binding_path = f"/graphParameters/{_pointer_segment(graph_parameter_id)}/bindings/{binding_index}"
            if binding_key in existing_bindings:
                raise ContractError("node parameter has multiple graph parameter bindings",
                                    code="graph.parameter.binding.duplicate", path=binding_path)
            existing_bindings.add(binding_key)
            bound_nodes = nodes_by_id.get(binding_key[0], [])
            if len(bound_nodes) != 1:
                raise ContractError("graph parameter binding node is missing or ambiguous",
                                    code="graph.parameter.binding.node_missing", path=f"{binding_path}/nodeId")
            bound_index, bound_node = bound_nodes[0]
            bound_operation = _lookup_operation(registry, bound_node.get("operationId"),
                                                bound_node.get("semanticVersion"))
            if bound_operation is None:
                raise ContractError("graph parameter binding operation is unavailable",
                                    code="node.operation.version.unsupported", path=f"{binding_path}/nodeId")
            bound_definition = next((item for item in bound_operation.parameters
                                     if item.parameter_id == binding_key[1]), None)
            if bound_definition is None:
                raise ContractError("graph parameter binding parameter is unavailable",
                                    code="graph.parameter.binding.parameter_missing",
                                    path=f"{binding_path}/parameterId")
            if bound_definition.value_type != target.get("valueType"):
                raise ContractError("graph parameter and existing binding types differ",
                                    code="graph.parameter.type.incompatible",
                                    path=f"{binding_path}/parameterId")
            bound_values = _canonical_parameters(bound_node.get("parameters", {}), bound_operation,
                                                 binding_key[0], bound_index)
            if bound_values.get(binding_key[1]) != target_value:
                raise ContractError("graph parameter value differs from an existing binding",
                                    code="graph.parameter.binding.value_mismatch",
                                    path=f"/nodes/{bound_index}/parameters/{_pointer_segment(binding_key[1])}")
    binding = {"nodeId": node_id, "parameterId": node_parameter_id}
    for graph_index, item in enumerate(graph_parameters):
        if not isinstance(item, dict) or not isinstance(item.get("bindings"), list):
            continue
        if any(isinstance(existing, dict) and existing.get("nodeId") == node_id and
               existing.get("parameterId") == node_parameter_id for existing in item["bindings"]):
            raise ContractError("node parameter is already exposed", code="graph.parameter.binding.duplicate",
                                path=f"/graphParameters/{graph_index}/bindings")
    if target is not None:
        if target.get("valueType") != parameter.value_type:
            raise ContractError("shared graph parameter type does not match node parameter",
                                code="graph.parameter.type.incompatible",
                                path=f"/graphParameters/{graph_parameter_id}/valueType")
        _canonical_parameters(node.get("parameters", {}), operation, node_id, node_index)
        node.setdefault("parameters", {})[node_parameter_id] = (
            list(target_value) if isinstance(target_value, list) else target_value)
        target["bindings"].append(binding)
    else:
        current = _canonical_parameters(node.get("parameters", {}), operation, node_id, node_index)
        value = current[node_parameter_id]
        node.setdefault("parameters", {})[node_parameter_id] = list(value) if isinstance(value, list) else value
        graph_parameters.append({
            "parameterId": graph_parameter_id,
            "label": display_name.strip(),
            "valueType": parameter.value_type,
            "value": list(value) if isinstance(value, list) else value,
            "bindings": [binding],
        })
    return edited


def edit_graph_parameter(document: Any, registry: dict[tuple[str, int], Operation],
                         graph_parameter_id: str, value: Any) -> dict[str, Any]:
    """Set one exposed value atomically; all bound node inputs resolve from it."""
    if not isinstance(graph_parameter_id, str) or not IDENTIFIER.fullmatch(graph_parameter_id):
        raise ContractError("graph parameter ID is invalid", code="graph.parameter.id.invalid",
                            path="/graphParameters/parameterId")
    edited = preserve_document(document)
    parameters = edited.get("graphParameters")
    if edited.get("schemaVersion") not in (2, 3) or not isinstance(parameters, list):
        raise ContractError("document has no graph parameters", code="graph.parameter.missing", path="/graphParameters")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    matches = [(index, item) for index, item in enumerate(parameters)
               if isinstance(item, dict) and item.get("parameterId") == graph_parameter_id]
    if len(matches) != 1:
        raise ContractError("graph parameter is missing or ambiguous", code="graph.parameter.missing",
                            path=f"/graphParameters/{_pointer_segment(graph_parameter_id)}")
    index, parameter = matches[0]
    value = _canonical_graph_parameter_value(
        parameter.get("valueType"), value, f"/graphParameters/{index}/value")
    bindings = parameter.get("bindings")
    if not isinstance(bindings, list) or not bindings:
        raise ContractError("graph parameter requires bindings", code="graph.parameter.binding.required",
                            path=f"/graphParameters/{index}/bindings")
    raw_nodes = edited.get("nodes")
    if not isinstance(raw_nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    node_records_by_id: dict[str, list[dict[str, Any]]] = {}
    for node in raw_nodes:
        if isinstance(node, dict) and isinstance(node.get("nodeId"), str):
            node_records_by_id.setdefault(node["nodeId"], []).append(node)
    binding_locations: dict[tuple[str, str], set[int]] = {}
    for graph_index, graph_parameter in enumerate(parameters):
        if not isinstance(graph_parameter, dict) or not isinstance(graph_parameter.get("bindings"), list):
            continue
        for candidate in graph_parameter["bindings"]:
            if (isinstance(candidate, dict) and isinstance(candidate.get("nodeId"), str) and
                    isinstance(candidate.get("parameterId"), str)):
                binding_locations.setdefault((candidate["nodeId"], candidate["parameterId"]), set()).add(graph_index)
    seen_bindings: set[tuple[str, str]] = set()
    for binding_index, binding in enumerate(bindings):
        binding_path = f"/graphParameters/{index}/bindings/{binding_index}"
        if not isinstance(binding, dict) or set(binding) != {"nodeId", "parameterId"}:
            raise ContractError("graph parameter binding shape is invalid",
                                code="graph.parameter.binding.invalid", path=binding_path)
        if not isinstance(binding.get("nodeId"), str) or not isinstance(binding.get("parameterId"), str):
            raise ContractError("graph parameter binding IDs must be strings",
                                code="graph.parameter.binding.invalid", path=binding_path)
        binding_key = (binding["nodeId"], binding["parameterId"])
        if binding_key in seen_bindings:
            raise ContractError("node parameter has multiple graph parameter bindings",
                                code="graph.parameter.binding.duplicate", path=binding_path)
        seen_bindings.add(binding_key)
        if any(graph_index != index for graph_index in binding_locations.get(binding_key, set())):
            raise ContractError("node parameter has multiple graph parameter bindings",
                                code="graph.parameter.binding.duplicate", path=binding_path)
        node_matches = node_records_by_id.get(binding["nodeId"], [])
        if len(node_matches) != 1:
            raise ContractError("graph parameter binding node is missing",
                                code="graph.parameter.binding.node_missing", path=f"{binding_path}/nodeId")
        node_parameters = node_matches[0].get("parameters")
        if not isinstance(node_parameters, dict):
            raise ContractError("bound node parameters must be an object",
                                code="node.parameters.invalid", path=f"{binding_path}/nodeId")
        operation = _lookup_operation(registry, node_matches[0].get("operationId"), node_matches[0].get("semanticVersion"))
        if operation is None:
            raise ContractError("bound node operation is unavailable",
                                code="node.operation.version.unsupported", path=f"{binding_path}/nodeId")
        definition = next((item for item in operation.parameters
                           if item.parameter_id == binding["parameterId"]), None)
        if definition is None:
            raise ContractError("bound node parameter is unavailable",
                                code="graph.parameter.binding.parameter_missing",
                                path=f"{binding_path}/parameterId")
        if definition.value_type != parameter.get("valueType"):
            raise ContractError("graph and node parameter types differ",
                                code="graph.parameter.type.incompatible",
                                path=f"{binding_path}/parameterId")
        node_parameters[binding["parameterId"]] = list(value) if isinstance(value, list) else value
    parameter["value"] = value
    return edited


def unexpose_node_parameter(document: Any, registry: dict[tuple[str, int], Operation],
                            node_id: str, node_parameter_id: str) -> dict[str, Any]:
    """Detach one node parameter while preserving its current shared value."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("node ID is invalid", code="node.id.invalid", path="/nodeId")
    if not isinstance(node_parameter_id, str) or not IDENTIFIER.fullmatch(node_parameter_id):
        raise ContractError("node parameter ID is invalid", code="node.parameter.id.invalid",
                            path="/parameterId")
    edited = preserve_document(document)
    parameters = edited.get("graphParameters")
    if edited.get("schemaVersion") not in (2, 3) or not isinstance(parameters, list):
        raise ContractError("document has no graph parameters", code="graph.parameter.missing", path="/graphParameters")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    binding_matches = []
    for graph_index, graph_parameter in enumerate(parameters):
        if not isinstance(graph_parameter, dict) or not isinstance(graph_parameter.get("bindings"), list):
            continue
        for binding_index, binding in enumerate(graph_parameter["bindings"]):
            if not isinstance(binding, dict):
                continue
            if binding.get("nodeId") == node_id and binding.get("parameterId") == node_parameter_id:
                if set(binding) != {"nodeId", "parameterId"}:
                    raise ContractError("target graph parameter binding shape is invalid",
                                        code="graph.parameter.binding.invalid",
                                        path=f"/graphParameters/{graph_index}/bindings/{binding_index}")
                binding_matches.append((graph_index, binding_index, graph_parameter))
    if len(binding_matches) != 1:
        raise ContractError("node parameter is not bound to exactly one graph parameter",
                            code="graph.parameter.binding.not_unique",
                            path=f"/graphParameters/{_pointer_segment(str(node_id))}/{_pointer_segment(str(node_parameter_id))}")
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    node_matches = [(index, node) for index, node in enumerate(nodes)
                    if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(node_matches) != 1:
        raise ContractError("node is missing or ambiguous", code="node.id.not_unique_or_missing", path="/nodes")
    node_index, node = node_matches[0]
    operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    if operation is None:
        raise ContractError("node operation is unavailable", code="node.operation.version.unsupported",
                            path=f"/nodes/{node_index}/semanticVersion")
    definition = next((item for item in operation.parameters if item.parameter_id == node_parameter_id), None)
    if definition is None:
        raise ContractError("node parameter is unavailable", code="node.parameter.unknown",
                            path=f"/nodes/{node_index}/parameters/{_pointer_segment(node_parameter_id)}")
    graph_index, binding_index, graph_parameter = binding_matches[0]
    if definition.value_type != graph_parameter.get("valueType"):
        raise ContractError("graph and node parameter types differ",
                            code="graph.parameter.type.incompatible",
                            path=f"/graphParameters/{graph_index}/valueType")
    value = _canonical_graph_parameter_value(
        graph_parameter.get("valueType"), graph_parameter.get("value"),
        f"/graphParameters/{graph_index}/value")
    node_parameters = node.get("parameters")
    if not isinstance(node_parameters, dict):
        raise ContractError("node parameters must be an object", code="node.parameters.invalid",
                            path=f"/nodes/{node_index}/parameters")
    node_parameters[node_parameter_id] = list(value) if isinstance(value, list) else value
    well_formed_parameter = (
        set(graph_parameter) == {"parameterId", "label", "valueType", "value", "bindings"} and
        isinstance(graph_parameter.get("parameterId"), str) and
        bool(IDENTIFIER.fullmatch(graph_parameter["parameterId"])) and
        isinstance(graph_parameter.get("label"), str) and bool(graph_parameter["label"].strip()) and
        graph_parameter.get("valueType") in {"Float32", "Vector3F32", "Vector4F32"} and
        all(isinstance(binding, dict) and set(binding) == {"nodeId", "parameterId"} and
            isinstance(binding.get("nodeId"), str) and isinstance(binding.get("parameterId"), str)
            for binding in graph_parameter["bindings"])
    )
    if well_formed_parameter:
        try:
            _canonical_graph_parameter_value(
                graph_parameter["valueType"], graph_parameter.get("value"),
                f"/graphParameters/{graph_index}/value")
        except ContractError:
            well_formed_parameter = False
    graph_parameter["bindings"].pop(binding_index)
    if not graph_parameter["bindings"] and well_formed_parameter:
        parameters.pop(graph_index)
    return edited


def edit_node_parameters(document: Any, registry: dict[tuple[str, int], Operation],
                         node_id: str, updates: dict[str, Any]) -> dict[str, Any]:
    """Validate all parameter edits before atomically replacing node values."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("node ID is invalid", code="node.id.invalid", path="/nodeId")
    if not isinstance(updates, dict) or not updates:
        raise ContractError("parameter updates must be a non-empty object", code="node.parameters.invalid", path="/parameters")
    if any(not isinstance(parameter_id, str) or not IDENTIFIER.fullmatch(parameter_id)
           for parameter_id in updates):
        raise ContractError("node parameter ID is invalid", code="node.parameter.id.invalid",
                            path="/parameters")
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError(f"expected one node named {node_id}", code="node.id.not_unique_or_missing", path="/nodes")
    index, node = matches[0]
    operation_id, version = node.get("operationId"), node.get("semanticVersion")
    operation = _lookup_operation(registry, operation_id, version)
    if operation is None:
        raise ContractError(f"operation for node {node_id} is unavailable", code="node.operation.version.unsupported", path=f"/nodes/{index}/semanticVersion")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    exposed_parameters = edited.get("graphParameters", [])
    if not isinstance(exposed_parameters, list):
        exposed_parameters = []
    for graph_index, exposed in enumerate(exposed_parameters):
        if not isinstance(exposed, dict) or not isinstance(exposed.get("bindings"), list):
            continue
        parameter_id = next((binding.get("parameterId") for binding in exposed["bindings"]
                             if isinstance(binding, dict) and binding.get("nodeId") == node_id and
                             isinstance(binding.get("parameterId"), str) and
                             binding["parameterId"] in updates), None)
        if parameter_id is not None:
            raise ContractError("edit the exposed graph parameter instead",
                                code="node.parameter.exposed",
                                path=f"/graphParameters/{graph_index}/bindings/{_pointer_segment(parameter_id)}")
    current = node.get("parameters", {})
    if not isinstance(current, dict):
        raise ContractError("node parameters must be an object", code="node.parameters.invalid", path=f"/nodes/{index}/parameters")
    candidate = dict(current)
    candidate.update(updates)
    canonical = _canonical_parameters(candidate, operation, node_id, index)
    node["parameters"] = canonical
    return edited


def edit_node_resource_reference(document: Any, registry: dict[tuple[str, int], Operation],
                                 node_id: str, resource_reference: Any) -> dict[str, Any]:
    """Atomically set or clear an Image Texture node's authored resource reference."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("node ID is invalid", code="node.id.invalid", path="/nodeId")
    edited = preserve_document(document)
    nodes = edited.get("nodes") if isinstance(edited, dict) else None
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array",
                            code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError("node is missing or ambiguous",
                            code="node.id.not_unique_or_missing", path="/nodes")
    index, node = matches[0]
    operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    if operation is None:
        raise ContractError("node operation is unavailable",
                            code="node.operation.version.unsupported",
                            path=f"/nodes/{index}/semanticVersion")
    if operation.implementation_id != GENERIC_TEXTURE_SAMPLE:
        raise ContractError("resourceReference is only supported by Image Texture nodes",
                            code="resource.reference.operation.unsupported",
                            path=f"/nodes/{index}/resourceReference")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    if resource_reference is None:
        node["resourceReference"] = None
        return edited
    canonical = _canonical_image_resource_reference(resource_reference, node_id, index)
    expected_key = f"artifact.shadergraph.image.{node_id}"
    if canonical["resourceKey"] != expected_key:
        raise ContractError("resourceKey must be node-scoped from nodeId",
                            code="resource.reference.key.invalid",
                            path=f"/nodes/{index}/resourceReference/resourceKey")
    _, resources_by_id = _canonical_graph_resources(edited.get("resources", []))
    if canonical["resourceId"] not in resources_by_id:
        raise ContractError("resourceId is missing from the document resource table",
                            code="resource.reference.resource.missing",
                            path=f"/nodes/{index}/resourceReference/resourceId")
    node["resourceReference"] = canonical
    return edited


def set_graph_resource_asset(document: Any, resource_id: str, asset_id: str) -> dict[str, Any]:
    """Create or relink one stable graph resource to a project Asset UUID."""
    if not isinstance(resource_id, str) or not IDENTIFIER.fullmatch(resource_id):
        raise ContractError("resourceId is invalid", code="resource.id.invalid", path="/resourceId")
    if not isinstance(asset_id, str) or not ASSET_UUID.fullmatch(asset_id):
        raise ContractError("assetId must be a canonical UUID string",
                            code="resource.asset_id.invalid", path="/assetId")
    edited = preserve_document(document)
    if not isinstance(edited, dict) or edited.get("kind") != "artifact-shader-graph-rd-document":
        raise ContractError("document kind is unsupported", code="document.kind.unsupported", path="/kind")
    records, by_id = _canonical_graph_resources(edited.get("resources", []))
    normalized_asset_id = asset_id.lower()
    existing = by_id.get(resource_id)
    if existing is None:
        records.append({"resourceId": resource_id, "resourceKind": "sampledTexture2D",
                        "source": {"kind": "projectAsset", "assetId": normalized_asset_id}})
    else:
        existing["source"]["assetId"] = normalized_asset_id
    edited["resources"] = records
    return edited


def edit_node_socket_default(document: Any, registry: dict[tuple[str, int], Operation],
                             node_id: str, socket_id: str, value: Any) -> dict[str, Any]:
    """Edit one node-owned optional input default, preserving it in schema v3."""
    if (not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id) or
            not isinstance(socket_id, str) or not IDENTIFIER.fullmatch(socket_id)):
        raise ContractError("node or socket ID is invalid", code="node.socket_default.id.invalid",
                            path="/socketDefaults")
    edited = preserve_document(document)
    if (not isinstance(edited, dict) or edited.get("kind") != "artifact-shader-graph-rd-document" or
            isinstance(edited.get("schemaVersion"), bool) or
            edited.get("schemaVersion") not in (1, 2, 3)):
        raise ContractError("document kind or schemaVersion is unsupported",
                            code="schema.version.unsupported", path="/schemaVersion")
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError("node is missing or ambiguous", code="node.id.not_unique_or_missing", path="/nodes")
    index, node = matches[0]
    operation = _lookup_operation(registry, node.get("operationId"), node.get("semanticVersion"))
    if operation is None:
        raise ContractError("node operation is unavailable", code="node.operation.version.unsupported",
                            path=f"/nodes/{index}/semanticVersion")
    definition = next((item for item in operation.inputs if item.socket_id == socket_id), None)
    if definition is None or definition.required:
        raise ContractError("only declared optional input sockets have editable defaults",
                            code="node.socket_default.not_optional",
                            path=f"/nodes/{index}/socketDefaults/{_pointer_segment(socket_id)}")
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    _upgrade_document_to_socket_defaults(edited, registry)
    raw_defaults = node.get("socketDefaults", {})
    canonical = _canonical_socket_defaults(raw_defaults, operation, node_id, index)
    canonical[socket_id] = _canonical_socket_default_value(
        value, definition, node_id,
        f"/nodes/{index}/socketDefaults/{_pointer_segment(socket_id)}")
    node["socketDefaults"] = canonical
    return edited


def _endpoint(raw: Any, label: str, path: str) -> tuple[str, str]:
    if not isinstance(raw, dict):
        raise ContractError(f"{label} must be an object", code="link.endpoint.invalid", path=path)
    node_id, socket_id = raw.get("nodeId"), raw.get("socketId")
    if not isinstance(node_id, str) or not isinstance(socket_id, str):
        raise ContractError(f"{label} requires nodeId and socketId", code="link.endpoint.required", path=path)
    return node_id, socket_id


def _hlsl_float(value: float) -> str:
    text = format(value, ".9g")
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return text + "f"


def compile_document(document_path: Path, registry: dict[tuple[str, int], Operation]) -> dict[str, Any]:
    return compile_document_from_data(_load_json(document_path), registry)


def preserve_document(document: Any) -> dict[str, Any]:
    """Return a detached editing copy without interpreting or dropping extensions.

    Compilation remains strict; this persistence-facing probe demonstrates that
    an editor can retain an unknown node/version while reporting it separately.
    """
    if not isinstance(document, dict):
        raise ContractError("document must be an object", code="document.invalid", path="/")
    return parse_json_text(canonical_json_text(document))


def edit_node_layout(document: Any, node_id: str, updates: dict[str, Any]) -> dict[str, Any]:
    """Apply validated x/y layout updates without changing graph semantics."""
    if not isinstance(updates, dict) or not updates or set(updates) - {"x", "y"}:
        raise ContractError("layout updates must contain x and/or y only", code="node.layout.invalid", path="/layout")
    edited = preserve_document(document)
    nodes = edited.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array", code="schema.collection.required", path="/nodes")
    matches = [(index, node) for index, node in enumerate(nodes)
               if isinstance(node, dict) and node.get("nodeId") == node_id]
    if len(matches) != 1:
        raise ContractError(f"expected one node named {node_id}", code="node.id.not_unique_or_missing", path="/nodes")
    index, node = matches[0]
    layout = node.get("layout")
    if not isinstance(layout, dict) or set(layout) != {"x", "y"}:
        raise ContractError("node layout must contain x and y", code="node.layout.invalid", path="/layout")
    for coordinate, value in updates.items():
        layout[coordinate] = _f32(value, f"layout.{coordinate}", path=f"/nodes/{index}/layout/{coordinate}", code="node.layout.coordinate.invalid")
    return edited


def edit_node_layout_batch(document: Any,
                           edits: list[tuple[str, dict[str, Any]]]) -> dict[str, Any]:
    """Apply distinct node layout edits atomically through the single-node validator."""
    if not isinstance(edits, list) or not edits:
        raise ContractError("layout edits must be a non-empty list", code="node.layout.batch.invalid", path="/edits")
    working = preserve_document(document)
    nodes = working.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array",
                            code="schema.collection.required", path="/nodes")
    indices_by_id: dict[str, list[int]] = {}
    for index, node in enumerate(nodes):
        if isinstance(node, dict) and isinstance(node.get("nodeId"), str):
            indices_by_id.setdefault(node["nodeId"], []).append(index)
    seen_node_ids: set[str] = set()
    for index, edit in enumerate(edits):
        if not isinstance(edit, (tuple, list)) or len(edit) != 2 or not isinstance(edit[0], str) or not isinstance(edit[1], dict):
            raise ContractError("layout edit must contain a node ID and coordinate object",
                                code="node.layout.batch.entry.invalid", path=f"/edits/{index}")
        node_id, updates = edit
        if not updates or set(updates) - {"x", "y"}:
            raise ContractError("layout updates must contain x and/or y only",
                                code="node.layout.invalid", path=f"/edits/{index}/1")
        if node_id in seen_node_ids:
            raise ContractError(f"node {node_id} appears more than once in layout batch",
                                code="node.layout.batch.duplicate", path=f"/edits/{index}")
        seen_node_ids.add(node_id)
        matches = indices_by_id.get(node_id, [])
        if len(matches) != 1:
            raise ContractError(f"expected one node named {node_id}",
                                code="node.id.not_unique_or_missing", path="/nodes")
        node_index = matches[0]
        layout = nodes[node_index].get("layout")
        if not isinstance(layout, dict) or set(layout) != {"x", "y"}:
            raise ContractError("node layout must contain x and y",
                                code="node.layout.invalid", path=f"/nodes/{node_index}/layout")
        for coordinate, value in updates.items():
            layout[coordinate] = _f32(value, f"layout.{coordinate}",
                                      path=f"/edits/{index}/1/{coordinate}",
                                      code="node.layout.coordinate.invalid")
    return working


def arrange_nodes_by_flow(document: Any, node_ids: list[str] | None = None, *,
                          column_spacing: float = 320.0,
                          row_spacing: float = 220.0) -> dict[str, Any]:
    """Lay out selected nodes by graph flow while retaining a stable top-left anchor."""
    preserved = preserve_document(document)
    nodes, links = preserved.get("nodes"), preserved.get("links")
    if not isinstance(nodes, list) or not isinstance(links, list):
        raise ContractError("document nodes and links must be arrays",
                            code="schema.collection.required", path="/nodes")
    if node_ids is None:
        selected_ids = [node.get("nodeId") for node in nodes
                        if isinstance(node, dict) and isinstance(node.get("nodeId"), str)]
        if not selected_ids:
            return preserved
    else:
        if not isinstance(node_ids, list) or not node_ids or any(not isinstance(item, str) for item in node_ids):
            raise ContractError("node IDs must be a non-empty string list",
                                code="node.layout.selection.invalid", path="/nodeIds")
        selected_ids = list(node_ids)
    if not selected_ids or len(set(selected_ids)) != len(selected_ids):
        raise ContractError("arrange selection must contain unique node IDs",
                            code="node.layout.selection.invalid", path="/nodeIds")
    selected_id_set = set(selected_ids)
    by_id: dict[str, dict[str, Any]] = {}
    for index, node in enumerate(nodes):
        if (isinstance(node, dict) and isinstance(node.get("nodeId"), str) and
                node["nodeId"] in selected_id_set):
            node_id = node["nodeId"]
            if node_id in by_id:
                raise ContractError(f"node {node_id} is ambiguous",
                                    code="node.id.duplicate", path=f"/nodes/{index}/nodeId")
            by_id[node_id] = node
    missing = next((node_id for node_id in selected_ids if node_id not in by_id), None)
    if missing is not None:
        raise ContractError(f"node {missing} is missing", code="node.id.not_unique_or_missing", path="/nodeIds")
    x_gap = _f32(column_spacing, "column spacing", path="/columnSpacing", code="node.layout.coordinate.invalid")
    y_gap = _f32(row_spacing, "row spacing", path="/rowSpacing", code="node.layout.coordinate.invalid")
    if x_gap <= 0 or y_gap <= 0:
        raise ContractError("arrange spacing must be positive",
                            code="node.layout.spacing.invalid", path="/")
    coordinates: dict[str, tuple[float, float]] = {}
    for node_id, node in by_id.items():
        layout = node.get("layout")
        if not isinstance(layout, dict):
            raise ContractError("node layout must be an object",
                                code="node.layout.invalid", path=f"/nodes/{node_id}/layout")
        coordinates[node_id] = (
            _f32(layout.get("x"), "layout x", path=f"/nodes/{node_id}/layout/x", code="node.layout.coordinate.invalid"),
            _f32(layout.get("y"), "layout y", path=f"/nodes/{node_id}/layout/y", code="node.layout.coordinate.invalid"),
        )
    adjacency = {node_id: set() for node_id in selected_ids}
    indegree = {node_id: 0 for node_id in selected_ids}
    external_positions: dict[str, list[float]] = {}
    for node in nodes:
        if not isinstance(node, dict) or not isinstance(node.get("nodeId"), str):
            continue
        layout = node.get("layout")
        if not isinstance(layout, dict):
            continue
        try:
            position = [_f32(layout.get("x"), "external layout x"),
                        _f32(layout.get("y"), "external layout y")]
        except ContractError:
            continue
        external_positions.setdefault(node["nodeId"], []).append(position[0])
    incoming_constraints: list[tuple[str, float]] = []
    outgoing_constraints: list[tuple[str, float]] = []
    for link in links:
        if not isinstance(link, dict):
            continue
        try:
            source_id, _ = _endpoint(link.get("from"), "link.from", "/links/from")
            target_id, _ = _endpoint(link.get("to"), "link.to", "/links/to")
        except ContractError:
            continue
        if source_id in adjacency and target_id in adjacency and target_id not in adjacency[source_id]:
            adjacency[source_id].add(target_id)
            indegree[target_id] += 1
        elif source_id in adjacency and target_id not in adjacency:
            positions = external_positions.get(target_id, [])
            if len(positions) == 1:
                outgoing_constraints.append((source_id, positions[0]))
        elif target_id in adjacency and source_id not in adjacency:
            positions = external_positions.get(source_id, [])
            if len(positions) == 1:
                incoming_constraints.append((target_id, positions[0]))
    layers = {node_id: 0 for node_id in selected_ids}
    ready = [node_id for node_id in selected_ids if indegree[node_id] == 0]
    heapq.heapify(ready)
    arranged = 0
    while ready:
        node_id = heapq.heappop(ready)
        arranged += 1
        for target_id in sorted(adjacency[node_id]):
            layers[target_id] = max(layers[target_id], layers[node_id] + 1)
            indegree[target_id] -= 1
            if indegree[target_id] == 0:
                heapq.heappush(ready, target_id)
    if arranged != len(selected_ids):
        last_layer = max((layers[node_id] for node_id in selected_ids if indegree[node_id] == 0), default=-1) + 1
        for node_id in selected_ids:
            if indegree[node_id] > 0:
                layers[node_id] = last_layer
    columns: dict[int, list[str]] = {}
    for node_id, layer in layers.items():
        columns.setdefault(layer, []).append(node_id)
    origin_x = max(0.0, min(point[0] for point in coordinates.values()))
    origin_y = max(0.0, min(point[1] for point in coordinates.values()))
    minimum_origin_x = 0.0
    maximum_origin_x: float | None = None
    for node_id, external_x in incoming_constraints:
        minimum_origin_x = max(minimum_origin_x, external_x + 24.0 - layers[node_id] * x_gap)
    for node_id, external_x in outgoing_constraints:
        candidate_maximum = external_x - 24.0 - layers[node_id] * x_gap
        maximum_origin_x = (candidate_maximum if maximum_origin_x is None
                            else min(maximum_origin_x, candidate_maximum))
    if maximum_origin_x is None or minimum_origin_x <= maximum_origin_x:
        origin_x = max(origin_x, minimum_origin_x)
        if maximum_origin_x is not None:
            origin_x = min(origin_x, maximum_origin_x)
    edits = []
    for layer, column in sorted(columns.items()):
        column.sort(key=lambda node_id: (coordinates[node_id][1], coordinates[node_id][0], node_id))
        for row, node_id in enumerate(column):
            edits.append((node_id, {"x": origin_x + layer * x_gap,
                                    "y": origin_y + row * y_gap}))
    return edit_node_layout_batch(preserved, edits)


def layout_node_selection(document: Any, node_ids: list[str], mode: str) -> dict[str, Any]:
    """Align or evenly distribute selected node top-left coordinates in one edit."""
    if not isinstance(mode, str) or mode not in {"align_x", "align_y", "distribute_x", "distribute_y"}:
        raise ContractError("layout mode is unsupported", code="node.layout.mode.unsupported", path="/mode")
    if not isinstance(node_ids, list) or any(not isinstance(item, str) for item in node_ids):
        raise ContractError("node IDs must be a string list", code="node.layout.selection.invalid", path="/nodeIds")
    minimum_count = 3 if mode.startswith("distribute_") else 2
    if len(node_ids) < minimum_count or len(set(node_ids)) != len(node_ids):
        raise ContractError(f"layout mode requires at least {minimum_count} distinct nodes",
                            code="node.layout.selection.invalid", path="/nodeIds")
    preserved = preserve_document(document)
    nodes = preserved.get("nodes")
    if not isinstance(nodes, list):
        raise ContractError("document nodes must be an array",
                            code="schema.collection.required", path="/nodes")
    matches: dict[str, list[tuple[int, dict[str, Any]]]] = {node_id: [] for node_id in node_ids}
    for index, node in enumerate(nodes):
        if isinstance(node, dict) and isinstance(node.get("nodeId"), str) and node["nodeId"] in matches:
            matches[node["nodeId"]].append((index, node))
    for node_id, entries in matches.items():
        if len(entries) != 1:
            raise ContractError(f"node {node_id} is missing or ambiguous",
                                code="node.id.not_unique_or_missing", path="/nodeIds")
    coordinate = mode[-1]
    positions = []
    for node_id in node_ids:
        index, node = matches[node_id][0]
        layout = node.get("layout")
        if not isinstance(layout, dict) or set(layout) != {"x", "y"}:
            raise ContractError("node layout must contain x and y",
                                code="node.layout.invalid", path=f"/nodes/{index}/layout")
        value = _f32(layout[coordinate], f"layout.{coordinate}",
                     path=f"/nodes/{index}/layout/{coordinate}",
                     code="node.layout.coordinate.invalid")
        positions.append((node_id, value, _f32(layout["y" if coordinate == "x" else "x"],
                                                "other layout coordinate",
                                                path=f"/nodes/{index}/layout",
                                                code="node.layout.coordinate.invalid")))
    edits = []
    if mode.startswith("align_"):
        aligned_value = min(item[1] for item in positions)
        edits = [(node_id, {coordinate: aligned_value}) for node_id, _, _ in positions]
    else:
        ordered = sorted(positions, key=lambda item: (item[1], item[2], item[0]))
        first, last = ordered[0][1], ordered[-1][1]
        divisor = len(ordered) - 1
        edits = [(node_id, {coordinate: first + (last - first) * index / divisor})
                 for index, (node_id, _, _) in enumerate(ordered)]
    return edit_node_layout_batch(preserved, edits)


def edit_known_node(document: Any, node_id: str, *, parameter_updates: dict[str, Any] | None = None,
                    layout_updates: dict[str, Any] | None = None) -> dict[str, Any]:
    """Compatibility wrapper; parameter edits must use the registry-aware API."""
    if parameter_updates is not None:
        raise ContractError("use edit_node_parameters with the operation registry", code="node.parameters.registry_required", path="/parameters")
    if layout_updates is None:
        raise ContractError("layout updates are required", code="node.layout.invalid", path="/layout")
    return edit_node_layout(document, node_id, layout_updates)


def diagnose_unavailable_operations(document: Any, registry: dict[tuple[str, int], Operation]) -> list[dict[str, str]]:
    preserved = preserve_document(document)
    diagnostics = []
    nodes = preserved.get("nodes")
    if not isinstance(nodes, list):
        return diagnostics
    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            continue
        operation_id, version = node.get("operationId"), node.get("semanticVersion")
        if isinstance(operation_id, str) and isinstance(version, int) and not isinstance(version, bool):
            if (operation_id, version) not in registry:
                diagnostics.append(ContractError(
                    f"operation {operation_id}@{version} is unavailable; node payload is preserved",
                    code="node.operation.version.unsupported",
                    path=f"/nodes/{index}/semanticVersion",
                ).to_json())
    return diagnostics


def validate_document_for_editing(
    document: Any,
    registry: dict[tuple[str, int], Operation],
    *,
    resolved_resource_bindings: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Aggregate graph diagnostics; missing required inputs remain edit-time warnings."""
    diagnostics: list[dict[str, str]] = []
    compile_result = None
    repairable = False

    def report(code: str, path: str, message: str, severity: str = "error") -> None:
        diagnostics.append({"code": code, "severity": severity, "path": path, "message": message})

    try:
        preserved = preserve_document(document)
        _reject_code_payload(preserved)
    except ContractError as error:
        diagnostics.append(error.to_json())
        return {"status": "invalid", "compilationAvailable": False, "diagnostics": diagnostics, "semanticHashSha256": None}

    if not isinstance(preserved, dict):
        report("document.invalid", "/", "document must be an object")
    else:
        schema_version = preserved.get("schemaVersion")
        if isinstance(schema_version, bool) or not isinstance(schema_version, int) or schema_version not in (1, 2, 3):
            report("schema.version.unsupported", "/schemaVersion", "document schemaVersion is unsupported; source document remains editable for repair", severity="warning")
            repairable = True
        if preserved.get("kind") != "artifact-shader-graph-rd-document":
            report("document.kind.unsupported", "/kind", "document kind is unsupported; source document remains editable for repair", severity="warning")
            repairable = True
        allowed = {"schemaVersion", "kind", "nodes", "links", "output",
                   "registryContractLock", "resources"}
        required = {"schemaVersion", "kind", "nodes", "links", "output"}
        if schema_version in (2, 3):
            allowed.add("graphParameters")
            required.add("graphParameters")
        for key in sorted(set(preserved) - allowed):
            report("schema.field.unknown", f"/{_pointer_segment(str(key))}", f"unknown document field {key}; retained for repair", severity="warning")
            repairable = True
        for key in sorted(required - set(preserved)):
            report("schema.field.required", f"/{_pointer_segment(key)}", f"required document field {key} is missing")
        if "registryContractLock" in preserved:
            try:
                for issue in compare_registry_contracts(preserved["registryContractLock"], registry):
                    issue["severity"] = "warning"
                    issue["message"] += "; saved document is retained for repair"
                    diagnostics.append(issue)
                    repairable = True
            except ContractError as error:
                diagnostics.append(error.to_json())

    if not isinstance(preserved, dict):
        diagnostics.sort(key=lambda item: (0 if item["severity"] == "error" else 1, item["path"], item["code"]))
        return {"status": "invalid", "compilationAvailable": False, "diagnostics": diagnostics, "semanticHashSha256": None}

    raw_nodes, raw_links = preserved.get("nodes"), preserved.get("links")
    if not isinstance(raw_nodes, list) or not isinstance(raw_links, list):
        if not isinstance(raw_nodes, list):
            report("schema.collection.required", "/nodes", "document nodes must be an array")
        if not isinstance(raw_links, list):
            report("schema.collection.required", "/links", "document links must be an array")
        diagnostics.sort(key=lambda item: (0 if item["severity"] == "error" else 1, item["path"], item["code"]))
        return {"status": "invalid", "repairable": repairable, "compilationAvailable": False,
                "diagnostics": diagnostics, "semanticHashSha256": None}
    diagnostics.extend(graph_collection_limit_issues(raw_nodes, raw_links))
    if any(item["severity"] == "error" for item in diagnostics):
        diagnostics.sort(key=lambda item: (0 if item["severity"] == "error" else 1, item["path"], item["code"]))
        return {"status": "invalid", "repairable": repairable, "compilationAvailable": False,
                "diagnostics": diagnostics, "semanticHashSha256": None}
    if "resources" in preserved:
        try:
            _canonical_graph_resources(preserved["resources"])
        except ContractError as error:
            diagnostics.append(error.to_json())
            diagnostics.sort(key=lambda item: (0 if item["severity"] == "error" else 1, item["path"], item["code"]))
            return {"status": "invalid", "repairable": repairable, "compilationAvailable": False,
                    "diagnostics": diagnostics, "semanticHashSha256": None}

    node_by_id: dict[str, tuple[int, Operation, dict[str, Any]]] = {}
    canonical_node_parameters: dict[str, dict[str, float | list[float]]] = {}
    canonical_node_socket_defaults: dict[str, dict[str, float | list[float]]] = {}
    all_node_ids: set[str] = set()
    sanitized: dict[str, str] = {}
    for index, node in enumerate(raw_nodes):
        path = f"/nodes/{index}"
        if not isinstance(node, dict):
            report("node.invalid", path, "node must be an object")
            continue
        allowed_node = {"nodeId", "operationId", "semanticVersion", "parameters", "layout"}
        if preserved.get("schemaVersion") == 3:
            allowed_node.add("socketDefaults")
        if node.get("operationId") == "artifact.texture.sample.image2d":
            allowed_node.add("resourceReference")
        for key in sorted(set(node) - allowed_node):
            report("node.field.unknown", f"{path}/{_pointer_segment(str(key))}", f"unknown node field {key}; retained for repair", severity="warning")
            repairable = True
        required_node_fields = set(allowed_node)
        if node.get("operationId") == "artifact.texture.sample.image2d":
            required_node_fields.discard("resourceReference")
        for key in sorted(required_node_fields - set(node)):
            report("node.field.required", f"{path}/{key}", f"required node field {key} is missing")
        node_id, operation_id, version = node.get("nodeId"), node.get("operationId"), node.get("semanticVersion")
        if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
            report("node.id.invalid", f"{path}/nodeId", "nodeId is invalid")
            continue
        if node_id in all_node_ids:
            report("node.id.duplicate", f"{path}/nodeId", f"duplicate nodeId {node_id}")
            continue
        all_node_ids.add(node_id)
        if not isinstance(operation_id, str):
            report("node.operation.unsupported", f"{path}/operationId", "operationId must be a string")
            continue
        if isinstance(version, bool) or not isinstance(version, int):
            report("node.operation.version.invalid", f"{path}/semanticVersion", "semanticVersion must be an integer")
            continue
        operation = registry.get((operation_id, version))
        if operation is None:
            report("node.operation.version.unsupported", f"{path}/semanticVersion", f"operation {operation_id}@{version} is unavailable; node payload is retained for repair", severity="warning")
            repairable = True
            continue
        if preserved.get("schemaVersion") == 3 and "socketDefaults" not in node:
            report("node.field.required", f"{path}/socketDefaults", "schema version 3 requires socketDefaults")
        if operation.implementation_id == GENERIC_TEXTURE_SAMPLE:
            if node.get("resourceReference") is None:
                report("resource.reference.required", f"{path}/resourceReference",
                       "Image Texture resource must be selected before compilation",
                       severity="warning")
                repairable = True
            else:
                try:
                    _canonical_image_resource_reference(node.get("resourceReference"), node_id, index)
                except ContractError as error:
                    diagnostics.append(error.to_json())
        symbol = "n_" + re.sub(r"[^A-Za-z0-9_]", "_", node_id)
        if symbol in sanitized:
            report("node.identifier.backend_collision", f"{path}/nodeId", f"node ID collides with {sanitized[symbol]!r} after backend sanitization")
        sanitized[symbol] = node_id
        layout = node.get("layout")
        if not isinstance(layout, dict):
            report("node.layout.invalid", f"{path}/layout", "layout must be an object")
        else:
            for coordinate in ("x", "y"):
                if coordinate not in layout:
                    report("node.layout.invalid", f"{path}/layout/{coordinate}", f"layout {coordinate} is required")
                else:
                    try:
                        _f32(layout[coordinate], f"{path}/layout/{coordinate}")
                    except ContractError as error:
                        report(error.code, f"{path}/layout/{coordinate}", str(error))
        try:
            canonical_node_parameters[node_id] = _canonical_parameters(
                node.get("parameters", {}), operation, node_id, index)
        except ContractError as error:
            diagnostics.append(error.to_json())
        try:
            canonical_node_socket_defaults[node_id] = _canonical_socket_defaults(
                node.get("socketDefaults", {}), operation, node_id, index,
                require_complete=preserved.get("schemaVersion") == 3)
        except ContractError as error:
            diagnostics.append(error.to_json())
        node_by_id[node_id] = (index, operation, node)

    if preserved.get("schemaVersion") in (2, 3):
        raw_graph_parameters = preserved.get("graphParameters")
        if "graphParameters" in preserved and not isinstance(raw_graph_parameters, list):
            report("schema.collection.required", "/graphParameters",
                   "schema version 2 requires a graphParameters array")
        elif isinstance(raw_graph_parameters, list):
            graph_parameter_ids: set[str] = set()
            graph_parameter_bindings: set[tuple[str, str]] = set()
            for graph_index, graph_parameter in enumerate(raw_graph_parameters):
                path = f"/graphParameters/{graph_index}"
                required_fields = {"parameterId", "label", "valueType", "value", "bindings"}
                if not isinstance(graph_parameter, dict) or set(graph_parameter) != required_fields:
                    unknown = sorted(set(graph_parameter) - required_fields) if isinstance(graph_parameter, dict) else []
                    field_path = f"{path}/{_pointer_segment(str(unknown[0]))}" if unknown else path
                    report("graph.parameter.shape.invalid", field_path,
                           "graph parameter has missing or unknown fields")
                    continue
                parameter_id = graph_parameter["parameterId"]
                if not isinstance(parameter_id, str) or not IDENTIFIER.fullmatch(parameter_id):
                    report("graph.parameter.id.invalid", f"{path}/parameterId", "graph parameter ID is invalid")
                elif parameter_id in graph_parameter_ids:
                    report("graph.parameter.id.duplicate", f"{path}/parameterId", "graph parameter ID is duplicated")
                else:
                    graph_parameter_ids.add(parameter_id)
                if not isinstance(graph_parameter["label"], str) or not graph_parameter["label"].strip():
                    report("graph.parameter.label.invalid", f"{path}/label", "graph parameter label is empty")
                value_type = graph_parameter["valueType"]
                value_type_valid = isinstance(value_type, str) and value_type in {"Float32", "Vector3F32", "Vector4F32"}
                if not value_type_valid:
                    report("graph.parameter.type.invalid", f"{path}/valueType", "graph parameter type is unsupported")
                    canonical_value = None
                else:
                    try:
                        canonical_value = _canonical_graph_parameter_value(
                            value_type, graph_parameter["value"], f"{path}/value")
                    except ContractError as error:
                        diagnostics.append(error.to_json())
                        canonical_value = None
                bindings = graph_parameter["bindings"]
                if not isinstance(bindings, list) or not bindings:
                    report("graph.parameter.binding.required", f"{path}/bindings",
                           "graph parameter requires at least one binding")
                    continue
                for binding_index, binding in enumerate(bindings):
                    binding_path = f"{path}/bindings/{binding_index}"
                    if (not isinstance(binding, dict) or set(binding) != {"nodeId", "parameterId"} or
                            not isinstance(binding.get("nodeId"), str) or
                            not isinstance(binding.get("parameterId"), str)):
                        report("graph.parameter.binding.invalid", binding_path,
                               "graph parameter binding shape is invalid")
                        continue
                    node_id, node_parameter_id = binding["nodeId"], binding["parameterId"]
                    binding_key = (node_id, node_parameter_id)
                    if binding_key in graph_parameter_bindings:
                        report("graph.parameter.binding.duplicate", binding_path,
                               "node parameter has multiple graph parameter bindings")
                    graph_parameter_bindings.add(binding_key)
                    node_data = node_by_id.get(node_id)
                    if node_data is None:
                        if node_id in all_node_ids:
                            report("graph.parameter.binding.operation_unavailable", f"{binding_path}/nodeId",
                                   "bound node operation is unavailable; source document remains editable for repair",
                                   severity="warning")
                            repairable = True
                        else:
                            report("graph.parameter.binding.node_missing", f"{binding_path}/nodeId",
                                   "graph parameter binding node is missing")
                        continue
                    node_index, operation, _ = node_data
                    definition = next((item for item in operation.parameters
                                       if item.parameter_id == node_parameter_id), None)
                    if definition is None:
                        report("graph.parameter.binding.parameter_missing", f"{binding_path}/parameterId",
                               "bound node parameter is unavailable")
                        continue
                    if value_type_valid and definition.value_type != value_type:
                        report("graph.parameter.type.incompatible", f"{binding_path}/parameterId",
                               "graph and node parameter types differ")
                        continue
                    if not value_type_valid or canonical_value is None:
                        continue
                    node_values = canonical_node_parameters.get(node_id)
                    if node_values is None:
                        continue
                    if node_values.get(node_parameter_id) != canonical_value:
                        report("graph.parameter.binding.value_mismatch",
                               f"/nodes/{node_index}/parameters/{_pointer_segment(node_parameter_id)}",
                               "bound node parameter value differs from its graph parameter")

    writers: dict[tuple[str, str], int] = {}
    incoming: set[tuple[str, str]] = set()
    adjacency: dict[str, set[str]] = {node_id: set() for node_id in node_by_id}
    for index, link in enumerate(raw_links):
        path = f"/links/{index}"
        if not isinstance(link, dict):
            report("link.shape.invalid", path, "link must be an object")
            continue
        for key in sorted(set(link) - {"from", "to"}):
            report("link.field.unknown", f"{path}/{_pointer_segment(str(key))}", f"unknown link field {key}; retained for repair", severity="warning")
            repairable = True
        if not {"from", "to"} <= set(link):
            report("link.shape.invalid", path, "link must contain from and to endpoints")
            continue
        try:
            source_id, source_socket = _endpoint(link.get("from"), "link.from", f"{path}/from")
            target_id, target_socket = _endpoint(link.get("to"), "link.to", f"{path}/to")
        except ContractError as error:
            diagnostics.append(error.to_json())
            continue
        source_data, target_data = node_by_id.get(source_id), node_by_id.get(target_id)
        if source_id not in all_node_ids or target_id not in all_node_ids:
            report("link.node.missing", path, "link references a missing node")
            continue
        if source_data is None or target_data is None:
            report("link.operation.unavailable", path, "link touches a node whose operation is unavailable; link payload is retained for repair", severity="warning")
            repairable = True
            continue
        source_op, target_op = source_data[1], target_data[1]
        source_def = next((item for item in source_op.outputs if item.socket_id == source_socket), None)
        target_def = next((item for item in target_op.inputs if item.socket_id == target_socket), None)
        if source_def is None or target_def is None:
            report("link.socket.missing_or_direction_invalid", path, "link references a missing or wrong-direction socket")
            continue
        if source_def.value_type != target_def.value_type:
            report("link.type.incompatible", path, f"link type mismatch {source_def.value_type} -> {target_def.value_type}")
        target_key = (target_id, target_socket)
        if target_key in writers:
            report("link.input.multiple_writers", path, "input has multiple writers")
        writers[target_key] = index
        incoming.add(target_key)
        adjacency[source_id].add(target_id)

    for node_id, (node_index, operation, _) in node_by_id.items():
        for socket in operation.inputs:
            if socket.required and (node_id, socket.socket_id) not in incoming:
                report("node.input.required_unconnected", f"/nodes/{node_index}/inputs/{_pointer_segment(socket.socket_id)}",
                       f"required input {node_id}.{socket.socket_id} is not connected yet", severity="warning")

    output = preserved.get("output")
    output_data = None
    if not isinstance(output, dict) or set(output) != {"nodeId", "socketId"}:
        report("output.invalid", "/output", "output must contain nodeId and socketId")
    elif not isinstance(output.get("nodeId"), str) or not isinstance(output.get("socketId"), str):
        report("output.invalid", "/output", "output nodeId and socketId must be strings")
    else:
        output_data = node_by_id.get(output["nodeId"])
        if output_data is None:
            if output["nodeId"] in all_node_ids:
                report("output.operation.unavailable", "/output/nodeId",
                       "output operation is unavailable; source document remains editable for repair", severity="warning")
                repairable = True
            else:
                report("output.node.missing", "/output/nodeId", "output references a missing or invalid node")
        elif not any(item.socket_id == output["socketId"] for item in output_data[1].outputs):
            report("output.socket.missing", "/output/socketId", "output references a missing output socket")

    indegree = {node_id: 0 for node_id in adjacency}
    for successors in adjacency.values():
        for successor in successors:
            indegree[successor] += 1
    ready = [node_id for node_id, count in indegree.items() if count == 0]
    visited_count = 0
    while ready:
        current = ready.pop()
        visited_count += 1
        for successor in adjacency[current]:
            indegree[successor] -= 1
            if indegree[successor] == 0:
                ready.append(successor)
    if visited_count != len(indegree):
        report("graph.cycle", "/links", "document contains a cycle")

    if not repairable and not any(item["severity"] == "error" for item in diagnostics):
        try:
            compile_result = compile_document_from_data(
                preserved, registry,
                resolved_resource_bindings=resolved_resource_bindings)
            for warning in compile_result.get("warnings", []):
                node_id = warning.removeprefix("node ").removesuffix(" is unreachable from the selected output")
                report("graph.node.unreachable", f"/nodes/{_pointer_segment(node_id)}", warning, severity="warning")
            for resource_key in compile_result.get("unresolvedResourceKeys", []):
                report("resource.binding.unresolved",
                       f"/resourceBindings/{_pointer_segment(resource_key)}",
                       "project Asset or runtime source is unavailable; the graph is retained for repair",
                       severity="warning")
                repairable = True
        except ContractError as error:
            if error.code != "node.input.required_unconnected":
                diagnostics.append(error.to_json())

    diagnostics.sort(key=lambda item: (0 if item["severity"] == "error" else 1, item["path"], item["code"]))
    has_errors = any(item["severity"] == "error" for item in diagnostics)
    has_warnings = any(item["severity"] == "warning" for item in diagnostics)
    return {
        "status": "invalid" if has_errors else "repairable" if repairable else "incomplete" if has_warnings else "valid",
        "repairable": repairable,
        "compilationAvailable": compile_result is not None and not has_errors and not repairable,
        "diagnostics": diagnostics,
        "semanticHashSha256": compile_result.get("semanticHashSha256") if compile_result is not None and not has_errors and not repairable else None,
    }


def _project_resolved_resource_bindings(
    requirements: list[dict[str, Any]],
    resolved_bindings: dict[str, dict[str, Any]] | None,
) -> dict[str, Any]:
    """Keep resolved project asset state outside shader and typed-IR identities."""
    if not requirements:
        if resolved_bindings not in (None, {}):
            raise ContractError("resource bindings were supplied but the active graph has no resource requirements",
                                code="resource.binding.unrequired", path="/resourceBindings")
        return {"resourceBindingStatus": "not-required", "resolvedResourceBindings": [],
                "unresolvedResourceKeys": [], "resourceBindingKeySha256": None}
    if resolved_bindings is None:
        resolved_bindings = {}
    if not isinstance(resolved_bindings, dict):
        raise ContractError("resolved resource bindings must be an object keyed by resourceKey",
                            code="resource.binding.invalid", path="/resourceBindings")
    required_keys = {item["resourceKey"] for item in requirements}
    extras = sorted(set(resolved_bindings) - required_keys)
    if extras:
        raise ContractError(f"resource binding {extras[0]!r} is not required by the active graph",
                            code="resource.binding.unrequired", path=f"/resourceBindings/{_pointer_segment(extras[0])}")
    canonical_records = []
    unresolved = []
    uuid_pattern = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
    for requirement in requirements:
        resource_key = requirement["resourceKey"]
        raw = resolved_bindings.get(resource_key)
        path = f"/resourceBindings/{_pointer_segment(resource_key)}"
        if raw is None:
            unresolved.append(resource_key)
            continue
        required_fields = {"assetId", "assetType", "sourceRevision", "samplerKey", "uvSet",
                           "uvTransform", "colorInterpretation"}
        if not isinstance(raw, dict) or set(raw) != required_fields:
            unknown = sorted(set(raw) - required_fields) if isinstance(raw, dict) else []
            field_path = f"{path}/{_pointer_segment(unknown[0])}" if unknown else path
            raise ContractError("resolved resource binding has missing or unknown fields",
                                code="resource.binding.shape.invalid", path=field_path)
        asset_id = raw["assetId"]
        if not isinstance(asset_id, str) or not uuid_pattern.fullmatch(asset_id):
            raise ContractError("assetId must be a canonical UUID string",
                                code="resource.binding.asset_id.invalid", path=f"{path}/assetId")
        asset_type = raw["assetType"]
        if asset_type != "Image":
            raise ContractError("sampledTexture2D requires an Image asset",
                                code="resource.binding.asset_type.incompatible", path=f"{path}/assetType")
        source_revision = raw["sourceRevision"]
        if isinstance(source_revision, bool) or not isinstance(source_revision, int) or source_revision < 1:
            raise ContractError("sourceRevision must be a positive integer",
                                code="resource.binding.revision.invalid", path=f"{path}/sourceRevision")
        sampler_key = raw["samplerKey"]
        if not isinstance(sampler_key, str) or not IDENTIFIER.fullmatch(sampler_key):
            raise ContractError("samplerKey must be a stable non-empty identifier",
                                code="resource.binding.sampler.invalid", path=f"{path}/samplerKey")
        uv_set = raw["uvSet"]
        if isinstance(uv_set, bool) or not isinstance(uv_set, int) or uv_set not in (0, 1):
            raise ContractError("uvSet must be 0 or 1",
                                code="resource.binding.uv_set.invalid", path=f"{path}/uvSet")
        raw_transform = raw["uvTransform"]
        transform_fields = {"scaleU", "scaleV", "rotationDegrees", "offsetU", "offsetV"}
        if not isinstance(raw_transform, dict) or set(raw_transform) != transform_fields:
            unknown = sorted(set(raw_transform) - transform_fields) if isinstance(raw_transform, dict) else []
            field_path = f"{path}/uvTransform/{_pointer_segment(unknown[0])}" if unknown else f"{path}/uvTransform"
            raise ContractError("uvTransform must define finite scale, rotation, and offset values",
                                code="resource.binding.uv_transform.invalid", path=field_path)
        uv_transform = {
            key: _f32(value, f"resource binding {resource_key} {key}",
                      path=f"{path}/uvTransform/{key}",
                      code="resource.binding.uv_transform.invalid")
            for key, value in sorted(raw_transform.items())
        }
        color_interpretation = raw["colorInterpretation"]
        if not isinstance(color_interpretation, str) or color_interpretation not in {"linear", "srgb"}:
            raise ContractError("colorInterpretation must be linear or srgb",
                                code="resource.binding.color_interpretation.invalid",
                                path=f"{path}/colorInterpretation")
        canonical_records.append({
            "resourceKey": resource_key,
            "resourceKind": requirement["resourceKind"],
            "assetId": asset_id.lower(),
            "assetType": asset_type,
            "sourceRevision": source_revision,
            "samplerKey": sampler_key,
            "uvSet": uv_set,
            "uvTransform": uv_transform,
            "colorInterpretation": color_interpretation,
        })
    if unresolved:
        return {"resourceBindingStatus": "unresolved" if not canonical_records else "incomplete",
                "resolvedResourceBindings": canonical_records,
                "unresolvedResourceKeys": unresolved,
                "resourceBindingKeySha256": None}
    encoded = json.dumps(canonical_records, sort_keys=True, separators=(",", ":"),
                         ensure_ascii=False, allow_nan=False).encode("utf-8")
    return {"resourceBindingStatus": "resolved",
            "resolvedResourceBindings": canonical_records,
            "unresolvedResourceKeys": [],
            "resourceBindingKeySha256": hashlib.sha256(encoded).hexdigest().upper()}


def emit_hlsl_from_typed_ir(
    typed_ir: dict[str, Any],
    parameter_block: dict[str, Any] | None,
    *,
    context_bindings: dict[tuple[str, str], str] | None = None,
) -> tuple[str, list[dict[str, Any]]]:
    """Emit the prototype HLSL body from the typed IR and explicit host bindings."""
    if (not isinstance(typed_ir, dict) or
            typed_ir.get("schemaId") != "artifact.shadergraph.typed-ir" or
            typed_ir.get("schemaVersion") != 1 or
            not isinstance(typed_ir.get("operations"), list) or
            not isinstance(typed_ir.get("output"), dict)):
        raise ContractError("typed IR schema is unsupported", code="shader.typed_ir.invalid", path="/typedIR")
    bindings = {} if context_bindings is None else context_bindings
    if not isinstance(bindings, dict):
        raise ContractError("context bindings must be an object", code="shader.context.binding.invalid", path="/contextBindings")

    operations = typed_ir["operations"]
    by_node = {item.get("nodeId"): item for item in operations if isinstance(item, dict)}
    if len(by_node) != len(operations) or None in by_node:
        raise ContractError("typed IR operation IDs must be unique", code="shader.typed_ir.operation.invalid", path="/typedIR/operations")
    expressions: dict[tuple[str, str], str] = {}
    lines: list[str] = []
    context_records: dict[tuple[str, str], dict[str, Any]] = {}

    def node_symbol(node_id: str) -> str:
        return "n_" + re.sub(r"[^A-Za-z0-9_]", "_", node_id)

    def input_expression(operation: dict[str, Any], socket_id: str) -> str:
        item = next((value for value in operation.get("inputs", [])
                     if value.get("socketId") == socket_id), None)
        if item is None:
            raise ContractError("typed IR input socket is missing", code="shader.typed_ir.input.missing",
                                path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}")
        producer = item.get("producer")
        if producer is not None:
            key = (producer.get("nodeId"), producer.get("socketId"))
            if key not in expressions:
                raise ContractError("typed IR producer has not been emitted", code="shader.typed_ir.producer.invalid",
                                    path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}/producer")
            return expressions[key]
        value = item.get("defaultValue")
        if item.get("required") or value is None:
            raise ContractError("typed IR required input has no operand", code="shader.typed_ir.input.missing",
                                path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}")
        value_type = item.get("valueType")
        if value_type == "Float":
            return _hlsl_float(value)
        if value_type in {"Vector2", "Vector3", "Vector4"}:
            lane_count = {"Vector2": 2, "Vector3": 3, "Vector4": 4}[value_type]
            hlsl_type = {"Vector2": "float2", "Vector3": "float3", "Vector4": "float4"}[value_type]
            if not isinstance(value, list) or len(value) != lane_count:
                raise ContractError("typed IR vector default has invalid lanes", code="shader.typed_ir.default.invalid",
                                    path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}/defaultValue")
            return hlsl_type + "(" + ", ".join(_hlsl_float(lane) for lane in value) + ")"
        raise ContractError("typed IR default type is unsupported", code="shader.typed_ir.default.invalid",
                            path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}/defaultValue")

    if parameter_block is not None:
        if not isinstance(parameter_block, dict) or not isinstance(parameter_block.get("fields"), list):
            raise ContractError("parameter block layout is invalid", code="shader.parameter_block.invalid", path="/parameterBlock")
        lines.append("cbuffer ArtifactGraphParameters {")
        for field in parameter_block["fields"]:
            value_type = field.get("valueType")
            hlsl_type = {"Float32": "float", "Vector3F32": "float3", "Vector4F32": "float4"}.get(value_type)
            if hlsl_type is None:
                raise ContractError("parameter block field type is unsupported", code="shader.parameter_block.invalid", path="/parameterBlock/fields")
            lines.append(f"    {hlsl_type} {field['hlslSymbol']} : packoffset({field['hlslPackOffset']});")
        lines.append("};")

    for operation in operations:
        node_id = operation["nodeId"]
        implementation_id = operation["implementationId"]
        symbol = node_symbol(node_id)
        parameters = {item["parameterId"]: item for item in operation.get("parameters", [])}
        outputs = operation.get("outputs", [])
        if not outputs:
            raise ContractError("typed IR operation has no outputs", code="shader.typed_ir.output.missing",
                                path=f"/typedIR/operations/{node_id}/outputs")

        if "sourceContext" in operation:
            context = operation["sourceContext"]
            context_key = (context.get("contextId"), context.get("component"))
            expression = bindings.get(context_key)
            if not isinstance(expression, str) or not expression.strip():
                raise ContractError(f"source context {context_key} has no backend binding",
                                    code="shader.context.binding.unsupported", path=f"/typedIR/operations/{node_id}/sourceContext")
            record = context_records.setdefault(context_key, {
                "contextId": context_key[0], "component": context_key[1],
                "valueType": context["valueType"], "hlslExpression": expression, "nodeIds": [],
            })
            record["nodeIds"].append(node_id)
            if len(outputs) != 1:
                raise ContractError("source context typed IR must have one output", code="shader.typed_ir.output.invalid",
                                    path=f"/typedIR/operations/{node_id}/outputs")
            hlsl_type = {"Float": "float", "Vector2": "float2"}.get(outputs[0].get("valueType"))
            if hlsl_type is None:
                raise ContractError("source context output type is unsupported", code="shader.typed_ir.output.invalid",
                                    path=f"/typedIR/operations/{node_id}/outputs/0/valueType")
            variable = f"{symbol}_{re.sub(r'[^A-Za-z0-9_]', '_', outputs[0]['socketId'])}"
            lines.append(f"{hlsl_type} {variable} = {expression};")
            expression_by_output = {outputs[0]["socketId"]: variable}
        elif implementation_id in {"float.constant", "vector3.constant", "vector4.constant"}:
            parameter = parameters.get("value")
            if parameter is None:
                raise ContractError("constant typed IR parameter is missing", code="shader.typed_ir.parameter.missing",
                                    path=f"/typedIR/operations/{node_id}/parameters/value")
            graph_parameter_id = parameter.get("graphParameterId")
            value = parameter.get("value")
            if graph_parameter_id is not None:
                expression = "P_" + re.sub(r"[^A-Za-z0-9_]", "_", graph_parameter_id)
            elif implementation_id == "float.constant":
                expression = _hlsl_float(value)
            else:
                hlsl_type = "float3" if implementation_id == "vector3.constant" else "float4"
                expected = 3 if implementation_id == "vector3.constant" else 4
                if not isinstance(value, list) or len(value) != expected:
                    raise ContractError("constant typed IR parameter has invalid lanes", code="shader.typed_ir.parameter.invalid",
                                        path=f"/typedIR/operations/{node_id}/parameters/value")
                expression = hlsl_type + "(" + ", ".join(_hlsl_float(lane) for lane in value) + ")"
            hlsl_type = {"Float": "float", "Vector3": "float3", "Vector4": "float4"}.get(outputs[0].get("valueType"))
            if hlsl_type is None or len(outputs) != 1:
                raise ContractError("constant typed IR output is unsupported", code="shader.typed_ir.output.invalid",
                                    path=f"/typedIR/operations/{node_id}/outputs")
            variable = f"{symbol}_{re.sub(r'[^A-Za-z0-9_]', '_', outputs[0]['socketId'])}"
            lines.append(f"{hlsl_type} {variable} = {expression};")
            expression_by_output = {outputs[0]["socketId"]: variable}
        elif implementation_id in TEXTURE_SAMPLE_SLOTS or implementation_id == GENERIC_TEXTURE_SAMPLE:
            dependency = next((item for item in operation.get("resourceDependencies", [])
                               if item.get("resourceKind") == "sampledTexture2D"), None)
            if dependency is None:
                raise ContractError("texture typed IR resource is missing", code="shader.typed_ir.resource.missing",
                                    path=f"/typedIR/operations/{node_id}/resourceDependencies")
            uv = input_expression(operation, "uv")
            color_symbol, alpha_symbol = f"{symbol}_color", f"{symbol}_alpha"
            lines.extend([
                f"float4 {color_symbol} = {dependency['shaderSymbol']}.Sample({dependency['samplerSymbol']}, {uv});",
                f"float {alpha_symbol} = {color_symbol}.a;",
            ])
            expression_by_output = {"color": color_symbol, "alpha": alpha_symbol}
        elif implementation_id == "vector4.ramp.linear_stops":
            factor = input_expression(operation, "factor")
            parameter = parameters.get("stops")
            stops = parameter.get("value") if parameter else None
            if not isinstance(stops, list) or not stops:
                raise ContractError("ramp typed IR stops are missing", code="shader.typed_ir.parameter.invalid",
                                    path=f"/typedIR/operations/{node_id}/parameters/stops")
            ordered = sorted(enumerate(stops), key=lambda item: (item[1]["position"], item[0]))
            lines.append(f"float {symbol}_factor = saturate({factor});")
            for stop_index, (_, stop) in enumerate(ordered):
                lines.append(f"float {symbol}_stop_{stop_index} = saturate({_hlsl_float(stop['position'])});")
                lines.append(f"float4 {symbol}_stop_color_{stop_index} = float4({', '.join(_hlsl_float(lane) for lane in stop['color'])});")
            lines.append(f"float4 {symbol}_color = {symbol}_stop_color_0;")
            for stop_index in range(1, len(ordered)):
                previous_position, current_position = f"{symbol}_stop_{stop_index - 1}", f"{symbol}_stop_{stop_index}"
                previous_color, current_color = f"{symbol}_stop_color_{stop_index - 1}", f"{symbol}_stop_color_{stop_index}"
                local_t = f"{symbol}_t_{stop_index}"
                lines.extend([
                    f"float {local_t} = saturate(({symbol}_factor - {previous_position}) / max({current_position} - {previous_position}, 1e-5f));",
                    f"if ({symbol}_factor >= {previous_position} && {symbol}_factor < {current_position}) {symbol}_color = lerp({previous_color}, {current_color}, {local_t});",
                ])
            lines.append(f"if ({symbol}_factor >= {symbol}_stop_{len(ordered) - 1}) {symbol}_color = {symbol}_stop_color_{len(ordered) - 1};")
            lines.append(f"float {symbol}_alpha = {symbol}_color.a;")
            expression_by_output = {"color": f"{symbol}_color", "alpha": f"{symbol}_alpha"}
        elif implementation_id == "vector4.separate_channels":
            vector = input_expression(operation, "vector")
            expression_by_output = {}
            for output in outputs:
                socket_id = output["socketId"]
                variable = f"{symbol}_{re.sub(r'[^A-Za-z0-9_]', '_', socket_id)}"
                lines.append(f"float {variable} = ({vector}).{socket_id};")
                expression_by_output[socket_id] = variable
        else:
            if implementation_id == "vector2.mapping.scale_offset":
                value = input_expression(operation, "vector")
                by_id = {key: item.get("value") for key, item in parameters.items()}
                scale = f"float2({_hlsl_float(by_id['scaleX'])}, {_hlsl_float(by_id['scaleY'])})"
                offset = f"float2({_hlsl_float(by_id['offsetX'])}, {_hlsl_float(by_id['offsetY'])})"
                expression = f"(({value} * {scale}) + {offset})"
            elif implementation_id == "float.replicate.rgb_alpha_one":
                value = input_expression(operation, operation["inputs"][0]["socketId"])
                expression = f"float4({value}, {value}, {value}, 1.0f)"
            elif implementation_id == "vector4.extract_x":
                expression = f"({input_expression(operation, operation['inputs'][0]['socketId'])}).x"
            elif implementation_id == "vector4.invert.linear_factor":
                color = input_expression(operation, "color")
                factor = input_expression(operation, "factor")
                expression = f"lerp({color}, (float4(1.0f, 1.0f, 1.0f, 1.0f) - {color}), saturate({factor}))"
            elif implementation_id == "float.clamp.sorted_bounds":
                value, minimum, maximum = (input_expression(operation, key) for key in ("value", "minimum", "maximum"))
                expression = f"clamp({value}, min({minimum}, {maximum}), max({minimum}, {maximum}))"
            elif implementation_id in VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION:
                factor = input_expression(operation, "factor")
                color1, color2 = input_expression(operation, "color1"), input_expression(operation, "color2")
                color1_rgb, color2_rgb = f"({color1}).rgb", f"({color2}).rgb"
                mode = VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION[implementation_id]
                blend = {
                    "mix": color2_rgb,
                    "add": f"({color1_rgb} + {color2_rgb})",
                    "multiply": f"({color1_rgb} * {color2_rgb})",
                    "screen": f"(1.0f - (1.0f - {color1_rgb}) * (1.0f - {color2_rgb}))",
                    "overlay": (f"lerp(2.0f * {color1_rgb} * {color2_rgb}, "
                                f"1.0f - 2.0f * (1.0f - {color1_rgb}) * (1.0f - {color2_rgb}), "
                                f"step(0.5f, {color1_rgb}))"),
                    "darken": f"min({color1_rgb}, {color2_rgb})",
                    "lighten": f"max({color1_rgb}, {color2_rgb})",
                    "difference": f"abs({color1_rgb} - {color2_rgb})",
                }[mode]
                expression = (f"float4(lerp({color1_rgb}, {blend}, saturate({factor})), "
                              f"lerp(({color1}).a, ({color2}).a, saturate({factor})))")
            else:
                a, b = input_expression(operation, operation["inputs"][0]["socketId"]), input_expression(operation, operation["inputs"][1]["socketId"])
                if implementation_id == "float.divide.positive_floor_1e5":
                    expression = f"({a} / max({b}, 1e-5f))"
                elif implementation_id == "vector4.divide.positive_floor_1e5":
                    expression = f"({a} / max({b}, float4(1e-5f,1e-5f,1e-5f,1e-5f)))"
                else:
                    operator = {
                        "float.add": "+", "float.subtract": "-", "float.multiply": "*",
                        "vector3.add": "+", "vector3.subtract": "-", "vector3.multiply": "*",
                        "vector4.add": "+", "vector4.subtract": "-", "vector4.multiply": "*",
                    }.get(implementation_id)
                    if operator is None:
                        raise ContractError(f"typed IR implementation is unsupported: {implementation_id}",
                                            code="lowering.operation.unsupported", path=f"/typedIR/operations/{node_id}/implementationId")
                    expression = f"({a} {operator} {b})"
            if len(outputs) != 1:
                raise ContractError("typed IR operation output arity is unsupported", code="shader.typed_ir.output.invalid",
                                    path=f"/typedIR/operations/{node_id}/outputs")
            hlsl_type = {"Float": "float", "Vector2": "float2", "Vector3": "float3", "Vector4": "float4"}.get(outputs[0].get("valueType"))
            if hlsl_type is None:
                raise ContractError("typed IR output type is unsupported", code="shader.typed_ir.output.invalid",
                                    path=f"/typedIR/operations/{node_id}/outputs/0/valueType")
            output_symbol = re.sub(r"[^A-Za-z0-9_]", "_", outputs[0]["socketId"])
            variable = f"{symbol}_{output_symbol}"
            lines.append(f"{hlsl_type} {variable} = {expression};")
            expression_by_output = {outputs[0]["socketId"]: variable}

        for socket_id, expression in expression_by_output.items():
            expressions[(node_id, socket_id)] = expression

    for record in context_records.values():
        record["nodeIds"].sort()
    output = typed_ir["output"]
    root_key = (output.get("nodeId"), output.get("socketId"))
    if root_key not in expressions:
        raise ContractError("typed IR selected output is unavailable", code="shader.typed_ir.output.invalid", path="/typedIR/output")
    return "\n".join(lines) + f"\nreturn {expressions[root_key]};\n", [context_records[key] for key in sorted(context_records)]


def emit_glsl_from_typed_ir(
    typed_ir: dict[str, Any],
    parameter_block: dict[str, Any] | None,
    *,
    context_bindings: dict[tuple[str, str], str] | None = None,
) -> tuple[str, dict[str, Any]]:
    """Emit a standalone Vulkan GLSL 4.5 fragment candidate from the shared typed IR."""
    if (not isinstance(typed_ir, dict) or
            typed_ir.get("schemaId") != "artifact.shadergraph.typed-ir" or
            typed_ir.get("schemaVersion") != 1 or
            not isinstance(typed_ir.get("operations"), list) or
            not isinstance(typed_ir.get("output"), dict)):
        raise ContractError("typed IR schema is unsupported", code="shader.typed_ir.invalid", path="/typedIR")
    bindings = {} if context_bindings is None else context_bindings
    if not isinstance(bindings, dict):
        raise ContractError("context bindings must be an object", code="shader.context.binding.invalid", path="/contextBindings")
    glsl_types = {"Float": "float", "Vector2": "vec2", "Vector3": "vec3", "Vector4": "vec4"}
    operations = typed_ir["operations"]
    by_node = {item.get("nodeId"): item for item in operations if isinstance(item, dict)}
    if len(by_node) != len(operations) or None in by_node:
        raise ContractError("typed IR operation IDs must be unique", code="shader.typed_ir.operation.invalid", path="/typedIR/operations")

    if parameter_block is not None and not isinstance(parameter_block, dict):
        raise ContractError("parameter block layout is invalid", code="shader.parameter_block.invalid", path="/parameterBlock")
    fields = [] if parameter_block is None else parameter_block.get("fields")
    if not isinstance(fields, list):
        raise ContractError("parameter block layout is invalid", code="shader.parameter_block.invalid", path="/parameterBlock")
    parameter_expressions = {}
    global_lines = ["#version 450"]
    if fields:
        global_lines.append("layout(std140, set = 1, binding = 0) uniform ArtifactGraphParameters {")
        for field in fields:
            value_type = {"Float32": "float", "Vector3F32": "vec3", "Vector4F32": "vec4"}.get(field.get("valueType"))
            if value_type is None or not isinstance(field.get("byteOffset"), int):
                raise ContractError("parameter block field is unsupported", code="shader.parameter_block.invalid", path="/parameterBlock/fields")
            symbol = field.get("hlslSymbol")
            if not isinstance(symbol, str) or not IDENTIFIER.fullmatch(symbol):
                raise ContractError("parameter block symbol is invalid", code="shader.parameter_block.invalid", path="/parameterBlock/fields")
            global_lines.append(f"    layout(offset = {field['byteOffset']}) {value_type} {symbol};")
            parameter_expressions[symbol] = f"artifactGraphParameters.{symbol}"
        global_lines.append("} artifactGraphParameters;")

    resource_dependencies = {}
    for operation in operations:
        for dependency in operation.get("resourceDependencies", []):
            if dependency.get("resourceKind") == "sampledTexture2D":
                key = dependency.get("resourceKey")
                if not isinstance(key, str) or not key:
                    raise ContractError("typed IR resource key is invalid", code="shader.typed_ir.resource.invalid", path=f"/typedIR/operations/{operation.get('nodeId')}/resourceDependencies")
                resource_dependencies[key] = dependency
    resource_bindings = []
    for index, key in enumerate(sorted(resource_dependencies)):
        dependency = resource_dependencies[key]
        texture_symbol, sampler_symbol = dependency.get("shaderSymbol"), dependency.get("samplerSymbol")
        if not all(isinstance(item, str) and IDENTIFIER.fullmatch(item) for item in (texture_symbol, sampler_symbol)):
            raise ContractError("typed IR resource symbols are invalid", code="shader.typed_ir.resource.invalid", path=f"/typedIR/resources/{_pointer_segment(key)}")
        texture_binding = 1 + index * 2
        sampler_binding = texture_binding + 1
        global_lines.extend([
            f"layout(set = 1, binding = {texture_binding}) uniform texture2D {texture_symbol};",
            f"layout(set = 1, binding = {sampler_binding}) uniform sampler {sampler_symbol};",
        ])
        resource_bindings.append({"resourceKey": key, "descriptorSet": 1,
                                  "textureSymbol": texture_symbol, "textureBinding": texture_binding,
                                  "samplerSymbol": sampler_symbol, "samplerBinding": sampler_binding})

    output = typed_ir["output"]
    root_operation = by_node.get(output.get("nodeId"))
    root_output = next((item for item in root_operation.get("outputs", [])
                        if item.get("socketId") == output.get("socketId")), None) if root_operation else None
    output_type = glsl_types.get(root_output.get("valueType")) if root_output else None
    if output_type is None:
        raise ContractError("typed IR selected output is unavailable", code="shader.typed_ir.output.invalid", path="/typedIR/output")
    global_lines.append(f"layout(location = 0) out {output_type} artifactGraphOutput;")

    body = ["void main() {"]
    expressions: dict[tuple[str, str], str] = {}
    context_records = {}

    def node_symbol(node_id: str) -> str:
        return "n_" + re.sub(r"[^A-Za-z0-9_]", "_", node_id)

    def input_expression(operation: dict[str, Any], socket_id: str) -> str:
        item = next((value for value in operation.get("inputs", []) if value.get("socketId") == socket_id), None)
        if item is None:
            raise ContractError("typed IR input socket is missing", code="shader.typed_ir.input.missing", path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}")
        producer = item.get("producer")
        if producer is not None:
            key = (producer.get("nodeId"), producer.get("socketId"))
            if key not in expressions:
                raise ContractError("typed IR producer has not been emitted", code="shader.typed_ir.producer.invalid", path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}/producer")
            return expressions[key]
        value_type, value = item.get("valueType"), item.get("defaultValue")
        if item.get("required") or value is None:
            raise ContractError("typed IR required input has no operand", code="shader.typed_ir.input.missing", path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}")
        if value_type == "Float":
            return _hlsl_float(value)
        glsl_type = glsl_types.get(value_type)
        lane_count = {"Vector2": 2, "Vector3": 3, "Vector4": 4}.get(value_type)
        if glsl_type is None or not isinstance(value, list) or len(value) != lane_count:
            raise ContractError("typed IR vector default has invalid lanes", code="shader.typed_ir.default.invalid", path=f"/typedIR/operations/{operation.get('nodeId')}/inputs/{socket_id}/defaultValue")
        return glsl_type + "(" + ", ".join(_hlsl_float(lane) for lane in value) + ")"

    def output_name(operation: dict[str, Any], output_socket: str) -> str:
        return f"{node_symbol(operation['nodeId'])}_{re.sub(r'[^A-Za-z0-9_]', '_', output_socket)}"

    for operation in operations:
        node_id, implementation_id = operation["nodeId"], operation["implementationId"]
        parameters = {item["parameterId"]: item for item in operation.get("parameters", [])}
        outputs = operation.get("outputs", [])
        symbol = node_symbol(node_id)
        result_by_output = {}
        if "sourceContext" in operation:
            context = operation["sourceContext"]
            context_key = (context.get("contextId"), context.get("component"))
            expression = bindings.get(context_key)
            if not isinstance(expression, str) or not expression.strip() or len(outputs) != 1:
                raise ContractError("typed IR source context has no GLSL binding", code="shader.context.binding.unsupported", path=f"/typedIR/operations/{node_id}/sourceContext")
            record = context_records.setdefault(context_key, {"contextId": context_key[0], "component": context_key[1], "valueType": context.get("valueType"), "glslExpression": expression, "nodeIds": []})
            record["nodeIds"].append(node_id)
            name = output_name(operation, outputs[0]["socketId"])
            body.append(f"    {glsl_types[outputs[0]['valueType']]} {name} = {expression};")
            result_by_output[outputs[0]["socketId"]] = name
        elif implementation_id in {"float.constant", "vector3.constant", "vector4.constant"}:
            parameter = parameters.get("value")
            if parameter is None or len(outputs) != 1:
                raise ContractError("typed IR constant parameter is missing", code="shader.typed_ir.parameter.missing", path=f"/typedIR/operations/{node_id}/parameters/value")
            graph_parameter_id = parameter.get("graphParameterId")
            if graph_parameter_id is not None:
                match = next((field for field in fields if field.get("parameterId") == graph_parameter_id), None)
                expression = parameter_expressions.get(match.get("hlslSymbol")) if match else None
                if expression is None:
                    raise ContractError("graph parameter is absent from the backend layout", code="shader.parameter_block.binding.missing", path=f"/typedIR/operations/{node_id}/parameters/value")
            elif implementation_id == "float.constant":
                expression = _hlsl_float(parameter.get("value"))
            else:
                expected = 3 if implementation_id == "vector3.constant" else 4
                value = parameter.get("value")
                if not isinstance(value, list) or len(value) != expected:
                    raise ContractError("typed IR constant lanes are invalid", code="shader.typed_ir.parameter.invalid", path=f"/typedIR/operations/{node_id}/parameters/value")
                expression = glsl_types[outputs[0]["valueType"]] + "(" + ", ".join(_hlsl_float(lane) for lane in value) + ")"
            name = output_name(operation, outputs[0]["socketId"])
            body.append(f"    {glsl_types[outputs[0]['valueType']]} {name} = {expression};")
            result_by_output[outputs[0]["socketId"]] = name
        elif implementation_id in TEXTURE_SAMPLE_SLOTS or implementation_id == GENERIC_TEXTURE_SAMPLE:
            dependency = next((item for item in operation.get("resourceDependencies", []) if item.get("resourceKind") == "sampledTexture2D"), None)
            if dependency is None:
                raise ContractError("typed IR texture dependency is missing", code="shader.typed_ir.resource.missing", path=f"/typedIR/operations/{node_id}/resourceDependencies")
            uv = input_expression(operation, "uv")
            color_name, alpha_name = f"{symbol}_color", f"{symbol}_alpha"
            body.extend([f"    vec4 {color_name} = texture(sampler2D({dependency['shaderSymbol']}, {dependency['samplerSymbol']}), {uv});", f"    float {alpha_name} = {color_name}.a;"])
            result_by_output = {"color": color_name, "alpha": alpha_name}
        elif implementation_id == "vector4.separate_channels":
            value = input_expression(operation, "vector")
            for item in outputs:
                name = output_name(operation, item["socketId"])
                body.append(f"    float {name} = ({value}).{item['socketId']};")
                result_by_output[item["socketId"]] = name
        else:
            if implementation_id == "vector2.mapping.scale_offset":
                value = input_expression(operation, "vector")
                by_id = {key: item.get("value") for key, item in parameters.items()}
                scale = f"vec2({_hlsl_float(by_id['scaleX'])}, {_hlsl_float(by_id['scaleY'])})"
                offset = f"vec2({_hlsl_float(by_id['offsetX'])}, {_hlsl_float(by_id['offsetY'])})"
                expression = f"(({value} * {scale}) + {offset})"
            elif implementation_id == "float.replicate.rgb_alpha_one":
                value = input_expression(operation, operation["inputs"][0]["socketId"])
                expression = f"vec4({value}, {value}, {value}, 1.0)"
            elif implementation_id == "vector4.extract_x":
                expression = f"({input_expression(operation, operation['inputs'][0]['socketId'])}).x"
            elif implementation_id == "vector4.invert.linear_factor":
                color, factor = input_expression(operation, "color"), input_expression(operation, "factor")
                expression = f"mix({color}, (vec4(1.0, 1.0, 1.0, 1.0) - {color}), clamp({factor}, 0.0, 1.0))"
            elif implementation_id == "float.clamp.sorted_bounds":
                value, minimum, maximum = (input_expression(operation, key) for key in ("value", "minimum", "maximum"))
                expression = f"clamp({value}, min({minimum}, {maximum}), max({minimum}, {maximum}))"
            elif implementation_id in VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION:
                factor = input_expression(operation, "factor")
                color1, color2 = input_expression(operation, "color1"), input_expression(operation, "color2")
                c1, c2 = f"({color1}).rgb", f"({color2}).rgb"
                mode = VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION[implementation_id]
                blend = {"mix": c2, "add": f"({c1} + {c2})", "multiply": f"({c1} * {c2})", "screen": f"(1.0 - (1.0 - {c1}) * (1.0 - {c2}))", "overlay": f"mix(2.0 * {c1} * {c2}, 1.0 - 2.0 * (1.0 - {c1}) * (1.0 - {c2}), step(vec3(0.5), {c1}))", "darken": f"min({c1}, {c2})", "lighten": f"max({c1}, {c2})", "difference": f"abs({c1} - {c2})"}[mode]
                amount = f"clamp({factor}, 0.0, 1.0)"
                expression = f"vec4(mix({c1}, {blend}, {amount}), mix(({color1}).a, ({color2}).a, {amount}))"
            elif implementation_id == "vector4.ramp.linear_stops":
                factor = input_expression(operation, "factor")
                stop_parameter = parameters.get("stops")
                stops = stop_parameter.get("value") if stop_parameter else None
                if not isinstance(stops, list) or not stops:
                    raise ContractError("typed IR ramp stops are missing", code="shader.typed_ir.parameter.invalid", path=f"/typedIR/operations/{node_id}/parameters/stops")
                ordered = sorted(enumerate(stops), key=lambda item: (item[1]["position"], item[0]))
                body.append(f"    float {symbol}_factor = clamp({factor}, 0.0, 1.0);")
                for index, (_, stop) in enumerate(ordered):
                    body.append(f"    float {symbol}_stop_{index} = clamp({_hlsl_float(stop['position'])}, 0.0, 1.0);")
                    body.append(f"    vec4 {symbol}_stop_color_{index} = vec4({', '.join(_hlsl_float(lane) for lane in stop['color'])});")
                body.append(f"    vec4 {symbol}_color = {symbol}_stop_color_0;")
                for index in range(1, len(ordered)):
                    prev, current = f"{symbol}_stop_{index - 1}", f"{symbol}_stop_{index}"
                    body.append(f"    float {symbol}_t_{index} = clamp(({symbol}_factor - {prev}) / max({current} - {prev}, 1e-5), 0.0, 1.0);")
                    body.append(f"    if ({symbol}_factor >= {prev} && {symbol}_factor < {current}) {symbol}_color = mix({symbol}_stop_color_{index - 1}, {symbol}_stop_color_{index}, {symbol}_t_{index});")
                body.append(f"    if ({symbol}_factor >= {symbol}_stop_{len(ordered) - 1}) {symbol}_color = {symbol}_stop_color_{len(ordered) - 1};")
                alpha_name = f"{symbol}_alpha"
                body.append(f"    float {alpha_name} = {symbol}_color.a;")
                result_by_output = {"color": f"{symbol}_color", "alpha": alpha_name}
                for socket_id, expression in result_by_output.items():
                    expressions[(node_id, socket_id)] = expression
                continue
            else:
                a, b = (input_expression(operation, operation["inputs"][index]["socketId"]) for index in range(2))
                if implementation_id in {"float.divide.positive_floor_1e5", "vector4.divide.positive_floor_1e5"}:
                    epsilon = "1e-5" if implementation_id.startswith("float.") else "vec4(1e-5, 1e-5, 1e-5, 1e-5)"
                    expression = f"({a} / max({b}, {epsilon}))"
                else:
                    operator = {"float.add": "+", "float.subtract": "-", "float.multiply": "*", "vector3.add": "+", "vector3.subtract": "-", "vector3.multiply": "*", "vector4.add": "+", "vector4.subtract": "-", "vector4.multiply": "*"}.get(implementation_id)
                    if operator is None:
                        raise ContractError(f"GLSL typed IR implementation is unsupported: {implementation_id}", code="lowering.operation.unsupported", path=f"/typedIR/operations/{node_id}/implementationId")
                    expression = f"({a} {operator} {b})"
            if len(outputs) != 1 or outputs[0].get("valueType") not in glsl_types:
                raise ContractError("typed IR operation output is unsupported", code="shader.typed_ir.output.invalid", path=f"/typedIR/operations/{node_id}/outputs")
            name = output_name(operation, outputs[0]["socketId"])
            body.append(f"    {glsl_types[outputs[0]['valueType']]} {name} = {expression};")
            result_by_output[outputs[0]["socketId"]] = name
        expressions.update({(node_id, socket_id): expression for socket_id, expression in result_by_output.items()})

    root_key = (output.get("nodeId"), output.get("socketId"))
    if root_key not in expressions:
        raise ContractError("typed IR selected output is unavailable", code="shader.typed_ir.output.invalid", path="/typedIR/output")
    body.extend([f"    artifactGraphOutput = {expressions[root_key]};", "}"])
    for record in context_records.values():
        record["nodeIds"].sort()
    source = "\n".join(global_lines + body) + "\n"
    return source, {"backendId": "vulkan-glsl450-verbatim",
                    "parameterBlockBinding": {"descriptorSet": 1, "binding": 0} if fields else None,
                    "resourceBindings": resource_bindings,
                    "contextBindings": [context_records[key] for key in sorted(context_records)]}


def compile_document_from_data(
    document: Any,
    registry: dict[tuple[str, int], Operation],
    *,
    hlsl_context_bindings: dict[tuple[str, str], str] | None = None,
    resolved_resource_bindings: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    if hlsl_context_bindings is None:
        context_binding_expressions = HLSL_SOURCE_CONTEXT_BINDINGS
    else:
        if not isinstance(hlsl_context_bindings, dict):
            raise ContractError("host HLSL context bindings must be an object",
                                code="shader.context.binding.invalid", path="/contextBindings")
        context_binding_expressions = {}
        for key, expression in hlsl_context_bindings.items():
            if (not isinstance(key, tuple) or len(key) != 2 or
                    not all(isinstance(part, str) and part for part in key) or
                    not isinstance(expression, str) or not expression.strip()):
                raise ContractError("host HLSL context binding is invalid",
                                    code="shader.context.binding.invalid", path="/contextBindings")
            context_binding_expressions[key] = expression
    _reject_code_payload(document)
    if (not isinstance(document, dict) or isinstance(document.get("schemaVersion"), bool) or
            not isinstance(document.get("schemaVersion"), int) or document.get("schemaVersion") not in (1, 2, 3)):
        raise ContractError("document schemaVersion must be 1, 2, or 3", code="schema.version.unsupported", path="/schemaVersion")
    if document.get("kind") != "artifact-shader-graph-rd-document":
        raise ContractError("document kind is unsupported", code="document.kind.unsupported", path="/kind")
    allowed_document_fields = {"schemaVersion", "kind", "nodes", "links", "output",
                               "registryContractLock", "resources"}
    if document["schemaVersion"] in (2, 3):
        allowed_document_fields.add("graphParameters")
        if "graphParameters" not in document:
            raise ContractError("schema version 2 requires graphParameters", code="schema.field.required", path="/graphParameters")
    if set(document) - allowed_document_fields or not {"schemaVersion", "kind", "nodes", "links", "output"} <= set(document):
        unknown = sorted(set(document) - allowed_document_fields)
        raise ContractError("document contains an unknown top-level field", code="schema.field.unknown", path="/" + _pointer_segment(str(unknown[0])) if unknown else "/")
    if "registryContractLock" in document:
        compatibility = compare_registry_contracts(document["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    raw_nodes, raw_links = document.get("nodes"), document.get("links")
    if not isinstance(raw_nodes, list) or not isinstance(raw_links, list):
        raise ContractError("document nodes and links must be arrays", code="schema.collection.required", path="/nodes" if not isinstance(raw_nodes, list) else "/links")
    graph_limit_issues = graph_collection_limit_issues(raw_nodes, raw_links)
    if graph_limit_issues:
        issue = graph_limit_issues[0]
        raise ContractError(issue["message"], code=issue["code"], path=issue["path"])

    nodes: dict[str, dict[str, Any]] = {}
    node_indices: dict[str, int] = {}
    sanitized: dict[str, str] = {}
    for index, raw in enumerate(raw_nodes):
        label = f"document.nodes[{index}]"
        if not isinstance(raw, dict):
            raise ContractError(f"{label} must be an object", code="node.invalid", path=f"/nodes/{index}")
        node_fields = {"nodeId", "operationId", "semanticVersion", "parameters", "layout"}
        if document["schemaVersion"] == 3:
            node_fields.add("socketDefaults")
        if raw.get("operationId") == "artifact.texture.sample.image2d":
            node_fields.add("resourceReference")
        if set(raw) != node_fields:
            unknown = sorted(set(raw) - node_fields)
            path = f"/nodes/{index}/{_pointer_segment(str(unknown[0]))}" if unknown else f"/nodes/{index}"
            raise ContractError(f"{label} has missing or unknown fields", code="node.shape.invalid", path=path)
        layout = raw.get("layout")
        if not isinstance(layout, dict) or set(layout) != {"x", "y"}:
            unknown_layout = sorted(set(layout) - {"x", "y"}) if isinstance(layout, dict) else []
            path = f"/nodes/{index}/layout/{_pointer_segment(str(unknown_layout[0]))}" if unknown_layout else f"/nodes/{index}/layout"
            raise ContractError(f"{label}.layout must contain only x and y", code="node.layout.invalid", path=path)
        _f32(layout["x"], f"{label}.layout.x", path=f"/nodes/{index}/layout/x", code="node.layout.coordinate.invalid")
        _f32(layout["y"], f"{label}.layout.y", path=f"/nodes/{index}/layout/y", code="node.layout.coordinate.invalid")
        node_id, operation_id = raw.get("nodeId"), raw.get("operationId")
        version = raw.get("semanticVersion")
        if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
            raise ContractError(f"{label}.nodeId is invalid", code="node.id.invalid", path=f"/nodes/{index}/nodeId")
        if node_id in nodes:
            raise ContractError(f"duplicate nodeId {node_id}", code="node.id.duplicate", path=f"/nodes/{index}/nodeId")
        if not isinstance(operation_id, str):
            raise ContractError(f"node {node_id} uses unsupported operation {operation_id!r}", code="node.operation.unsupported", path=f"/nodes/{index}/operationId")
        if isinstance(version, bool) or not isinstance(version, int):
            raise ContractError(f"node {node_id} semanticVersion must be an integer", code="node.operation.version.invalid", path=f"/nodes/{index}/semanticVersion")
        operation = registry.get((operation_id, version))
        if operation is None:
            raise ContractError(f"node {node_id} uses unsupported operation/version {operation_id}@{version}", code="node.operation.version.unsupported", path=f"/nodes/{index}/semanticVersion")
        symbol = "n_" + re.sub(r"[^A-Za-z0-9_]", "_", node_id)
        if symbol in sanitized:
            raise ContractError(f"node IDs {sanitized[symbol]!r} and {node_id!r} collide after HLSL sanitization", code="node.identifier.backend_collision", path=f"/nodes/{index}/nodeId")
        sanitized[symbol] = node_id
        parameters = _canonical_parameters(raw.get("parameters", {}), operation, node_id, index)
        socket_defaults = _canonical_socket_defaults(
            raw.get("socketDefaults", {}), operation, node_id, index,
            require_complete=document.get("schemaVersion") == 3)
        nodes[node_id] = {"operation": operation, "parameters": parameters,
                          "socketDefaults": socket_defaults, "raw": raw, "symbol": symbol}
        node_indices[node_id] = index

    generic_image_node_ids = {
        node_id for node_id, data in nodes.items()
        if data["operation"].implementation_id == GENERIC_TEXTURE_SAMPLE
    }
    canonical_node_resource_references: dict[str, dict[str, Any]] = {}
    seen_resource_keys: set[str] = set()
    for node_id in sorted(generic_image_node_ids):
        node_index = node_indices[node_id]
        reference = _canonical_image_resource_reference(
            nodes[node_id]["raw"].get("resourceReference"), node_id, node_index)
        expected_resource_key = f"artifact.shadergraph.image.{node_id}"
        if reference["resourceKey"] != expected_resource_key:
            raise ContractError("resourceKey must be node-scoped from nodeId",
                                code="resource.reference.key.invalid",
                                path=f"/nodes/{node_index}/resourceReference/resourceKey")
        if reference["resourceKey"] in seen_resource_keys:
            raise ContractError("resourceKey is duplicated",
                                code="resource.reference.key.duplicate",
                                path=f"/nodes/{node_index}/resourceReference/resourceKey")
        seen_resource_keys.add(reference["resourceKey"])
        canonical_node_resource_references[node_id] = reference

    _, resources_by_id = _canonical_graph_resources(
        document.get("resources", []), path="/resources")
    if generic_image_node_ids and "resources" not in document:
        raise ContractError("generic Image Texture nodes require a document resource table",
                            code="schema.field.required", path="/resources")
    for node_id, reference in canonical_node_resource_references.items():
        if reference["resourceId"] not in resources_by_id:
            node_index = node_indices[node_id]
            raise ContractError("resourceId is missing from the document resource table",
                                code="resource.reference.resource.missing",
                                path=f"/nodes/{node_index}/resourceReference/resourceId")

    graph_parameters = _apply_graph_parameters(document, nodes, node_indices)

    links: list[tuple[str, str, str, str]] = []
    writers: dict[tuple[str, str], tuple[str, str]] = {}
    outgoing: dict[str, set[str]] = {node_id: set() for node_id in nodes}
    incoming: dict[str, set[str]] = {node_id: set() for node_id in nodes}
    for index, raw_link in enumerate(raw_links):
        label = f"document.links[{index}]"
        if not isinstance(raw_link, dict):
            raise ContractError(f"{label} must be an object", code="link.invalid", path=f"/links/{index}")
        if set(raw_link) != {"from", "to"}:
            unknown = sorted(set(raw_link) - {"from", "to"})
            path = f"/links/{index}/{_pointer_segment(str(unknown[0]))}" if unknown else f"/links/{index}"
            raise ContractError(f"{label} has missing or unknown fields", code="link.shape.invalid", path=path)
        source_id, source_socket = _endpoint(raw_link.get("from"), f"{label}.from", f"/links/{index}/from")
        target_id, target_socket = _endpoint(raw_link.get("to"), f"{label}.to", f"/links/{index}/to")
        if source_id not in nodes or target_id not in nodes:
            raise ContractError(f"{label} references a missing node", code="link.node.missing", path=f"/links/{index}/{'from' if source_id not in nodes else 'to'}/nodeId")
        source_op = nodes[source_id]["operation"]
        target_op = nodes[target_id]["operation"]
        source_def = next((item for item in source_op.outputs if item.socket_id == source_socket), None)
        target_def = next((item for item in target_op.inputs if item.socket_id == target_socket), None)
        if source_def is None or target_def is None:
            raise ContractError(f"{label} references a missing or wrong-direction socket", code="link.socket.missing_or_direction_invalid", path=f"/links/{index}/{'from' if source_def is None else 'to'}/socketId")
        if source_def.value_type != target_def.value_type:
            raise ContractError(f"{label} type mismatch {source_def.value_type} -> {target_def.value_type}", code="link.type.incompatible", path=f"/links/{index}")
        target_key = (target_id, target_socket)
        if target_key in writers:
            raise ContractError(f"input {target_id}.{target_socket} has multiple writers", code="link.input.multiple_writers", path=f"/links/{index}/to")
        writers[target_key] = (source_id, source_socket)
        links.append((source_id, source_socket, target_id, target_socket))
        outgoing[source_id].add(target_id)
        incoming[target_id].add(source_id)

    output = document.get("output")
    if not isinstance(output, dict) or set(output) != {"nodeId", "socketId"}:
        unknown = sorted(set(output) - {"nodeId", "socketId"}) if isinstance(output, dict) else []
        path = f"/output/{_pointer_segment(str(unknown[0]))}" if unknown else "/output"
        raise ContractError("document.output must contain only nodeId and socketId", code="output.invalid", path=path)
    output_node, output_socket = _endpoint(output, "document.output", "/output")
    if output_node not in nodes:
        raise ContractError("document.output references a missing node", code="output.node.missing", path="/output/nodeId")
    output_def = next((item for item in nodes[output_node]["operation"].outputs if item.socket_id == output_socket), None)
    if output_def is None:
        raise ContractError("document.output references a missing output socket", code="output.socket.missing", path="/output/socketId")

    for node_id, data in nodes.items():
        operation = data["operation"]
        for socket in operation.inputs:
            if socket.required and (node_id, socket.socket_id) not in writers:
                node_index = node_indices[node_id]
                raise ContractError(
                    f"required input {node_id}.{socket.socket_id} is unconnected",
                    code="node.input.required_unconnected",
                    path=f"/nodes/{node_index}/inputs/{_pointer_segment(socket.socket_id)}",
                )

    # Validate every node/link, including unreachable subgraphs, before pruning.
    indegree = {node_id: len(incoming[node_id]) for node_id in nodes}
    ready = [node_id for node_id, count in indegree.items() if count == 0]
    heapq.heapify(ready)
    topological = []
    while ready:
        current = heapq.heappop(ready)
        topological.append(current)
        for successor in sorted(outgoing[current]):
            indegree[successor] -= 1
            if indegree[successor] == 0:
                heapq.heappush(ready, successor)
    if len(topological) != len(nodes):
        raise ContractError("document contains a cycle", code="graph.cycle", path="/links")

    active: set[str] = set()
    pending = [output_node]
    while pending:
        current = pending.pop()
        if current in active:
            continue
        active.add(current)
        pending.extend(incoming[current])
    active_order = [node_id for node_id in topological if node_id in active]
    warnings = [f"node {node_id} is unreachable from the selected output" for node_id in sorted(set(nodes) - active)]

    active_graph_parameters = []
    for parameter in graph_parameters:
        active_bindings = [binding for binding in parameter["bindings"] if binding["nodeId"] in active]
        if active_bindings:
            symbol = "P_" + re.sub(r"[^A-Za-z0-9_]", "_", parameter["parameterId"])
            active_graph_parameters.append({
                "parameterId": parameter["parameterId"],
                "valueType": parameter["valueType"],
                "value": parameter["value"],
                "hlslSymbol": symbol,
                "bindings": sorted(active_bindings, key=lambda item: (item["nodeId"], item["parameterId"])),
            })
    parameter_symbols = {}
    for parameter in active_graph_parameters:
        if parameter["hlslSymbol"] in parameter_symbols:
            raise ContractError("graph parameter IDs collide after HLSL sanitization",
                                code="graph.parameter.identifier_collision", path="/graphParameters")
        parameter_symbols[parameter["hlslSymbol"]] = parameter["parameterId"]
    parameter_fields = []
    parameter_cursor = 0
    for parameter in sorted(active_graph_parameters, key=lambda item: item["parameterId"]):
        value_type = parameter["valueType"]
        # Reserve a complete 16-byte register for vectors, including Vector3.
        field_size = 16 if value_type in {"Vector3F32", "Vector4F32"} else 4
        if field_size == 16:
            parameter_cursor = (parameter_cursor + 15) // 16 * 16
        byte_offset = parameter_cursor
        parameter_cursor += field_size
        parameter_fields.append({
            "parameterId": parameter["parameterId"],
            "valueType": value_type,
            "hlslSymbol": parameter["hlslSymbol"],
            "byteOffset": byte_offset,
            "byteSize": field_size,
            "registerIndex": byte_offset // 16,
            "registerComponentOffset": (byte_offset % 16) // 4,
            "hlslPackOffset": f"c{byte_offset // 16}.{('x', 'y', 'z', 'w')[(byte_offset % 16) // 4]}",
        })
    parameter_block = None
    parameter_block_binding_id = "artifact.shadergraph.parameters"
    parameter_block_schema_id = "artifact.shadergraph.parameter-block"
    parameter_block_schema_version = 1
    if parameter_fields:
        parameter_block = {
            "blockName": "ArtifactGraphParameters",
            "schemaId": parameter_block_schema_id,
            "schemaVersion": parameter_block_schema_version,
            "bindingId": parameter_block_binding_id,
            "resourceKind": "constantBuffer",
            "physicalSlot": None,
            "physicalSlotStatus": "requires-compiler-reflection",
            "layoutContract": "hlsl-cbuffer-packoffset-16byte-v1",
            "byteSize": (parameter_cursor + 15) // 16 * 16,
            "fields": parameter_fields,
        }
    typed_ir_operations: list[dict[str, Any]] = []
    for node_id in active_order:
        data = nodes[node_id]
        operation = data["operation"]
        implementation_id = operation.implementation_id
        symbol = data["symbol"]
        ir_inputs = []
        for socket in operation.inputs:
            producer = writers.get((node_id, socket.socket_id))
            ir_inputs.append({
                "socketId": socket.socket_id,
                "valueType": socket.value_type,
                "required": socket.required,
                "defaultValue": data["socketDefaults"].get(socket.socket_id) if producer is None else None,
                "producer": (None if producer is None else {
                    "nodeId": producer[0],
                    "socketId": producer[1],
                }),
            })
        ir_parameters = []
        for parameter in operation.parameters:
            graph_parameter_id = data["graphParameterBindings"].get(parameter.parameter_id)
            ir_parameters.append({
                "parameterId": parameter.parameter_id,
                "valueType": parameter.value_type,
                "value": (None if graph_parameter_id is not None else
                          data["parameters"][parameter.parameter_id]),
                "graphParameterId": graph_parameter_id,
            })
        ir_operation = {
            "nodeId": node_id,
            "operationId": operation.operation_id,
            "semanticVersion": operation.semantic_version,
            "implementationId": implementation_id,
            "inputs": ir_inputs,
            "outputs": [
                {"socketId": socket.socket_id, "valueType": socket.value_type}
                for socket in operation.outputs
            ],
            "parameters": ir_parameters,
        }
        if implementation_id in TEXTURE_SAMPLE_SLOTS:
            slot_name, shader_symbol = TEXTURE_SAMPLE_SLOTS[implementation_id]
            ir_operation["resourceDependencies"] = [{
                "resourceKey": f"artifact.material.texture.{implementation_id.split('texture.sample.', 1)[1]}",
                "resourceKind": "sampledTexture2D",
                "slot": slot_name,
                "shaderSymbol": shader_symbol,
                "samplerSymbol": "g_BaseColorSampler",
                "samplerContract": "legacy-shared-sampler-uncharacterized",
                "uvContract": "explicit-Vector2-input-no-implicit-transform",
                "sampleContract": "raw-float4-no-implicit-swizzle-or-color-decode",
            }]
        elif implementation_id == GENERIC_TEXTURE_SAMPLE:
            reference = canonical_node_resource_references[node_id]
            ir_operation["resourceDependencies"] = [{
                "resourceKey": reference["resourceKey"],
                "resourceId": reference["resourceId"],
                "nodeId": node_id,
                "resourceKind": "sampledTexture2D",
                "shaderSymbol": f"g_GraphTexture_{symbol[2:]}",
                "samplerSymbol": f"g_GraphSampler_{symbol[2:]}",
                "samplerContract": "resolved-per-node-binding",
                "uvContract": "explicit-Vector2-input-no-implicit-transform",
                "sampleContract": "raw-float4-no-implicit-swizzle-or-color-decode",
            }]
        if implementation_id in SOURCE_CONTEXTS:
            context = SOURCE_CONTEXTS[implementation_id]
            ir_operation["sourceContext"] = {
                "contextId": context["contextId"],
                "component": context["component"],
                "valueType": context["valueType"],
            }
        typed_ir_operations.append(ir_operation)
    active_node_records = []
    for node_id in sorted(active):
        data = nodes[node_id]
        operation = nodes[node_id]["operation"]
        record = {
            "nodeId": node_id,
            "operationId": operation.operation_id,
            "semanticVersion": operation.semantic_version,
            "implementationId": operation.implementation_id,
            "parameters": nodes[node_id]["parameters"],
        }
        input_defaults = {socket.socket_id: data["socketDefaults"][socket.socket_id]
                          for socket in operation.inputs
                          if socket.socket_id in data["socketDefaults"] and
                          (node_id, socket.socket_id) not in writers}
        if input_defaults:
            record["inputDefaults"] = input_defaults
        if node_id in canonical_node_resource_references:
            reference = canonical_node_resource_references[node_id]
            record["resourceReference"] = {
                "resourceKey": reference["resourceKey"],
                "resourceId": reference["resourceId"],
            }
        active_node_records.append(record)
    semantic_projection = {
        "schemaVersion": document["schemaVersion"],
        "output": {"nodeId": output_node, "socketId": output_socket, "valueType": output_def.value_type},
        "nodes": active_node_records,
        "links": [
            {"from": {"nodeId": source_id, "socketId": source_socket},
             "to": {"nodeId": target_id, "socketId": target_socket}}
            for source_id, source_socket, target_id, target_socket in sorted(links)
            if source_id in active and target_id in active
        ],
    }
    if active_graph_parameters:
        semantic_projection["graphParameters"] = [
            {"parameterId": item["parameterId"], "valueType": item["valueType"],
             "value": item["value"], "bindings": item["bindings"]}
            for item in sorted(active_graph_parameters, key=lambda item: item["parameterId"])
        ]
    canonical = json.dumps(semantic_projection, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    typed_ir = {
        "schemaId": "artifact.shadergraph.typed-ir",
        "schemaVersion": 1,
        "output": {
            "nodeId": output_node,
            "socketId": output_socket,
            "valueType": output_def.value_type,
        },
        "operations": typed_ir_operations,
        "graphParameters": [
            {
                "parameterId": item["parameterId"],
                "valueType": item["valueType"],
                "value": item["value"],
                "bindings": item["bindings"],
            }
            for item in sorted(active_graph_parameters, key=lambda item: item["parameterId"])
        ],
    }
    hlsl, emitted_context_bindings = emit_hlsl_from_typed_ir(
        typed_ir,
        parameter_block,
        context_bindings=context_binding_expressions,
    )
    context_bindings_by_key = {
        (binding["contextId"], binding["component"]): binding
        for binding in emitted_context_bindings
    }
    resource_requirements_by_key = {}
    for operation in typed_ir_operations:
        for dependency in operation.get("resourceDependencies", []):
            resource_requirements_by_key[dependency["resourceKey"]] = dependency
    resource_requirements = [resource_requirements_by_key[key]
                             for key in sorted(resource_requirements_by_key)]
    if isinstance(resolved_resource_bindings, dict):
        references_by_key = {
            reference["resourceKey"]: reference
            for reference in canonical_node_resource_references.values()
        }
        for resource_key, reference in references_by_key.items():
            binding = resolved_resource_bindings.get(resource_key)
            if not isinstance(binding, dict):
                continue
            sampler_bytes = json.dumps(reference["sampler"], sort_keys=True,
                                       separators=(",", ":"), ensure_ascii=False,
                                       allow_nan=False).encode("utf-8")
            expected_sampler_key = "sampler." + hashlib.sha256(sampler_bytes).hexdigest().lower()
            resource_record = resources_by_id.get(reference["resourceId"])
            if (binding.get("samplerKey") != expected_sampler_key or
                    binding.get("colorInterpretation") != reference["colorInterpretation"]):
                raise ContractError("resolved sampler/color state differs from the node resourceReference",
                                    code="resource.binding.authored_state.mismatch",
                                    path=f"/resourceBindings/{_pointer_segment(resource_key)}")
            if (resource_record is not None and
                    isinstance(binding.get("assetId"), str) and
                    binding["assetId"].lower() != resource_record["source"]["assetId"]):
                raise ContractError("resolved Asset UUID differs from the document resource table",
                                    code="resource.binding.asset_reference.mismatch",
                                    path=f"/resourceBindings/{_pointer_segment(resource_key)}/assetId")
    resource_binding_projection = _project_resolved_resource_bindings(
        resource_requirements, resolved_resource_bindings)
    typed_ir_canonical = json.dumps(
        typed_ir, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    structure_projection = json.loads(json.dumps(semantic_projection))
    for node in structure_projection["nodes"]:
        source_node = nodes[node["nodeId"]]
        for parameter_id in source_node["graphParameterBindings"]:
            if parameter_id in node["parameters"]:
                node["parameters"][parameter_id] = {"graphParameter": source_node["graphParameterBindings"][parameter_id]}
    if active_graph_parameters:
        structure_projection["graphParameters"] = [
            {"parameterId": item["parameterId"], "valueType": item["valueType"],
             "bindings": item["bindings"]}
            for item in sorted(active_graph_parameters, key=lambda item: item["parameterId"])
        ]
        structure_projection["parameterBlockContract"] = {
            "schemaId": parameter_block_schema_id,
            "schemaVersion": parameter_block_schema_version,
            "bindingId": parameter_block_binding_id,
            "resourceKind": "constantBuffer",
            "layoutContract": "hlsl-cbuffer-packoffset-16byte-v1",
        }
    if context_bindings_by_key:
        structure_projection["sourceContextInputs"] = [
            {key: binding[key] for key in ("contextId", "component", "valueType", "nodeIds")}
            for _, binding in sorted(context_bindings_by_key.items())
        ]
    structure_canonical = json.dumps(structure_projection, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    value_projection = [{"parameterId": item["parameterId"], "valueType": item["valueType"], "value": item["value"]}
                        for item in sorted(active_graph_parameters, key=lambda item: item["parameterId"])]
    value_canonical = json.dumps(value_projection, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    parameter_fields_by_id = {field["parameterId"]: field for field in parameter_fields}
    parameter_bindings = []
    for item in sorted(active_graph_parameters, key=lambda item: item["parameterId"]):
        field = parameter_fields_by_id[item["parameterId"]]
        parameter_bindings.append({
            "parameterId": item["parameterId"], "valueType": item["valueType"],
            "hlslSymbol": item["hlslSymbol"],
            "blockBindingId": parameter_block_binding_id,
            "byteOffset": field["byteOffset"],
            "byteSize": field["byteSize"],
            "registerIndex": field["registerIndex"],
            "registerComponentOffset": field["registerComponentOffset"],
            "hlslPackOffset": field["hlslPackOffset"],
            "bindings": item["bindings"],
        })
    return {
        "status": "candidate-lowered-no-shader-runtime",
        "semanticHashSha256": hashlib.sha256(canonical).hexdigest().upper(),
        "typedIRSemanticHashSha256": hashlib.sha256(typed_ir_canonical).hexdigest().upper(),
        "typedIR": typed_ir,
        "shaderStructureHashSha256": hashlib.sha256(structure_canonical).hexdigest().upper(),
        "hlslBodySha256": hashlib.sha256(hlsl.encode("utf-8")).hexdigest().upper(),
        "parameterValuesHashSha256": hashlib.sha256(value_canonical).hexdigest().upper(),
        "parameterValues": value_projection,
        "parameterBindings": parameter_bindings,
        "parameterBlock": parameter_block,
        "resourceRequirements": resource_requirements,
        **resource_binding_projection,
        "contextBindings": [context_bindings_by_key[key] for key in sorted(context_bindings_by_key)],
        "topologicalNodeIds": active_order,
        "outputType": output_def.value_type,
        "hlslBody": hlsl,
        "warnings": warnings,
        "limitations": [
            f"prototype supports Float/Vector2/Vector3/Vector4 sockets, Float/Vector3/Vector4 socket defaults, node-owned Vector3F32 values, Float/Vector3/Vector4 graph parameters, and {len(BUILTIN_IMPLEMENTATIONS)} closed implementation IDs only",
            "registry metadata may select existing implementations but cannot supply executable code or HLSL",
            "typed IR materializes active Float/Vector3/Vector4 socket defaults and symbolic material-slot texture dependencies; runtime resource identity, sampler state, color interpretation, and backend semantics remain incomplete",
            "generated HLSL is a deterministic candidate string; no shader compiler/backend/runtime is invoked",
            "diagnostic codes and document paths are golden-checked for eleven representative rejection classes; other structured diagnostics need broader fixture coverage",
            "this is an R&D prototype, not ArtifactCore API or Blender compatibility evidence",
        ],
    }


def project_legacy_core_material_input(
    source_json: str,
    registry: dict[tuple[str, int], Operation],
    *,
    target_node_id: str,
    target_input_socket: str,
) -> dict[str, Any]:
    """Project one supported legacy material input without rewriting its source."""
    if not isinstance(source_json, str):
        raise ContractError("legacy source must be the original JSON string",
                            code="legacy.document.invalid", path="/")
    try:
        source = json.loads(source_json)
    except (json.JSONDecodeError, RecursionError) as error:
        raise ContractError("legacy material graph JSON is invalid",
                            code="legacy.document.invalid", path="/") from error
    if not isinstance(source, dict):
        raise ContractError("legacy material graph must be an object",
                            code="legacy.document.invalid", path="/")
    if (isinstance(source.get("version"), bool) or source.get("version") != 1 or
            source.get("kind") not in (None, "artifact-material-graph")):
        raise ContractError("legacy material graph identity is unsupported",
                            code="legacy.document.unsupported", path="/version")
    source_nodes, source_links = source.get("nodes"), source.get("links")
    if not isinstance(source_nodes, list) or not isinstance(source_links, list):
        raise ContractError("legacy nodes and links arrays are required",
                            code="legacy.document.invalid", path="/")

    node_by_id: dict[str, dict[str, Any]] = {}
    for index, node in enumerate(source_nodes):
        if not isinstance(node, dict):
            raise ContractError("legacy node must be an object",
                                code="legacy.node.invalid", path=f"/nodes/{index}")
        node_id, node_type = node.get("id"), node.get("type")
        if not isinstance(node_id, str) or not node_id or not isinstance(node_type, str):
            raise ContractError("legacy node id and type are required",
                                code="legacy.node.invalid", path=f"/nodes/{index}")
        if node_id in node_by_id:
            raise ContractError("legacy node ids must be unique",
                                code="legacy.node.id.duplicate", path=f"/nodes/{index}/id")
        node_by_id[node_id] = node

    incoming: dict[tuple[str, str], list[tuple[str, str]]] = {}
    for index, link in enumerate(source_links):
        if not isinstance(link, dict):
            raise ContractError("legacy link must be an object",
                                code="legacy.link.invalid", path=f"/links/{index}")
        endpoints = (link.get("from"), link.get("out"), link.get("to"), link.get("in"))
        if not all(isinstance(item, str) and item for item in endpoints):
            raise ContractError("legacy link endpoints must be non-empty strings",
                                code="legacy.link.invalid", path=f"/links/{index}")
        source_id, source_socket, target_id, target_socket = endpoints
        incoming.setdefault((target_id, target_socket), []).append((source_id, source_socket))

    target = node_by_id.get(target_node_id)
    if target is None or target.get("type") != "Principled" or target_input_socket != "BaseColor":
        raise ContractError("projection target must be a Principled BaseColor input",
                            code="legacy.projection.target.unsupported", path="/target")
    target_sources = incoming.get((target_node_id, target_input_socket), [])
    if len(target_sources) != 1:
        raise ContractError("selected legacy material input must have exactly one source",
                            code="legacy.link.cardinality.invalid",
                            path=f"/nodes/{target_node_id}/inputs/{target_input_socket}")
    root_id, root_socket = target_sources[0]
    dependency_socket: dict[str, str] = {root_id: root_socket}

    def input_sockets(node_id: str) -> tuple[str, ...]:
        node = node_by_id.get(node_id)
        socket = dependency_socket[node_id]
        if node is None:
            raise ContractError("legacy link references a missing node",
                                code="legacy.link.endpoint.missing", path="/links")
        if node.get("type") == "RGB" and socket == "Color":
            return ()
        if node.get("type") == "Math" and socket == "Value":
            params = node.get("params", {})
            if not isinstance(params, dict):
                raise ContractError("legacy node params must be an object",
                                    code="legacy.node.parameters.invalid",
                                    path=f"/nodes/{node_id}/params")
            operation = params.get("op", 0)
            if isinstance(operation, bool) or operation != 0:
                raise ContractError("only legacy Math Add is supported by this projection",
                                    code="legacy.node.operation.unsupported",
                                    path=f"/nodes/{node_id}/params/op")
            clamp_result = params.get("clamp", False)
            if not isinstance(clamp_result, bool) or clamp_result:
                raise ContractError("clamped legacy Math Add has no matching typed operation",
                                    code="legacy.node.operation.unsupported",
                                    path=f"/nodes/{node_id}/params/clamp")
            return ("A", "B")
        raise ContractError("legacy node/socket is outside the supported projection lane",
                            code="legacy.node.operation.unsupported",
                            path=f"/nodes/{node_id}")

    ordered_ids: list[str] = []
    visited: set[str] = set()
    visiting: set[str] = {root_id}
    stack: list[tuple[str, int, tuple[str, ...]]] = [(root_id, 0, input_sockets(root_id))]
    while stack:
        node_id, input_index, sockets = stack[-1]
        if input_index == len(sockets):
            stack.pop()
            visiting.remove(node_id)
            visited.add(node_id)
            ordered_ids.append(node_id)
            continue
        socket = sockets[input_index]
        stack[-1] = (node_id, input_index + 1, sockets)
        sources = incoming.get((node_id, socket), [])
        if len(sources) != 1:
            raise ContractError("legacy Math Add inputs must each have exactly one source",
                                code="legacy.link.cardinality.invalid",
                                path=f"/nodes/{node_id}/inputs/{socket}")
        dependency_id, dependency_output = sources[0]
        dependency = node_by_id.get(dependency_id)
        if dependency is None:
            raise ContractError("legacy link references a missing node",
                                code="legacy.link.endpoint.missing", path="/links")
        supported_source = (
            (dependency.get("type") == "RGB" and dependency_output == "Color") or
            (dependency.get("type") == "Math" and dependency_output == "Value")
        )
        if not supported_source:
            raise ContractError("legacy Math Add inputs must come from RGB Color or Math Value sockets",
                                code="legacy.link.type.incompatible",
                                path=f"/nodes/{node_id}/inputs/{socket}")
        dependency_socket[dependency_id] = dependency_output
        if dependency_id in visiting:
            raise ContractError("legacy projection subgraph contains a cycle",
                                code="legacy.graph.cycle", path=f"/nodes/{dependency_id}")
        if dependency_id not in visited:
            visiting.add(dependency_id)
            stack.append((dependency_id, 0, input_sockets(dependency_id)))

    depth_by_id: dict[str, int] = {}
    for node_id in ordered_ids:
        dependencies = [source_id for socket in input_sockets(node_id)
                       for source_id, _ in incoming[(node_id, socket)]]
        depth_by_id[node_id] = 1 + max(
            (depth_by_id[dependency_id] for dependency_id in dependencies),
            default=-1)
    ids_by_depth: dict[int, list[str]] = {}
    for node_id in ordered_ids:
        ids_by_depth.setdefault(depth_by_id[node_id], []).append(node_id)

    projected_nodes: list[dict[str, Any]] = []
    projected_links: list[dict[str, dict[str, str]]] = []
    layout_provenance: dict[str, str] = {}
    for node_id in ordered_ids:
        source_node = node_by_id[node_id]
        params = source_node.get("params", {})
        if not isinstance(params, dict):
            raise ContractError("legacy node params must be an object",
                                code="legacy.node.parameters.invalid",
                                path=f"/nodes/{node_id}/params")
        layout = {}
        if "x" in source_node and "y" in source_node:
            layout = {
                "x": _f32(source_node["x"], "legacy layout x",
                          path=f"/nodes/{node_id}/x", code="legacy.layout.invalid"),
                "y": _f32(source_node["y"], "legacy layout y",
                          path=f"/nodes/{node_id}/y", code="legacy.layout.invalid"),
            }
            layout_provenance[node_id] = "legacy"
        else:
            layout = {
                "x": float(depth_by_id[node_id] * 220),
                "y": float(sorted(ids_by_depth[depth_by_id[node_id]]).index(node_id) * 120),
            }
            layout_provenance[node_id] = "deterministic-rd-canvas-projection"
        if source_node["type"] == "RGB":
            values = [
                _f32(params.get(channel, default), f"legacy RGB {channel}",
                     path=f"/nodes/{node_id}/params/{channel}", code="legacy.parameter.invalid")
                for channel, default in (("r", 0.5), ("g", 0.5),
                                         ("b", 0.5), ("a", 1.0))
            ]
            operation_id = "artifact.rd.constant.vector4.float32"
            semantic_parameters = {"value": values}
        else:
            operation_id = "artifact.rd.math.add.vector4.float32"
            semantic_parameters = {}
            for legacy_socket, typed_socket in (("A", "a"), ("B", "b")):
                source_id, source_socket = incoming[(node_id, legacy_socket)][0]
                projected_links.append({
                    "from": {"nodeId": source_id,
                             "socketId": "value" if source_socket == "Color" else "result"},
                    "to": {"nodeId": node_id, "socketId": typed_socket},
                })
        projected_node: dict[str, Any] = {
            "nodeId": node_id,
            "operationId": operation_id,
            "semanticVersion": 1,
            "parameters": semantic_parameters,
        }
        if layout:
            projected_node["layout"] = layout
        projected_nodes.append(projected_node)

    projected_document = {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-rd-document",
        "nodes": projected_nodes,
        "links": projected_links,
        "output": {"nodeId": root_id, "socketId": "result"},
    }
    compile_document_from_data(projected_document, registry)
    return {
        "status": "read-only-projection",
        "sourceSha256": hashlib.sha256(source_json.encode("utf-8")).hexdigest().upper(),
        "opaqueSourceJson": source_json,
        "selectedLegacyInput": {"nodeId": target_node_id,
                                 "socketId": target_input_socket},
        "includedNodeIds": ordered_ids,
        "excludedNodeIds": [node_id for node_id in node_by_id if node_id not in visited],
        "layoutProvenance": layout_provenance,
        "document": projected_document,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("document", nargs="?", type=Path, default=Path(__file__).with_name("dynamic_add.json"))
    parser.add_argument("--registry", type=Path, default=Path(__file__).with_name("registry.json"))
    parser.add_argument("--output", type=Path, help="write JSON output to this path instead of stdout")
    args = parser.parse_args()
    try:
        result = compile_document(args.document, load_registry(args.registry))
    except ContractError as error:
        print(json.dumps({
            "status": "rejected",
            "diagnostics": [error.to_json()],
        }, ensure_ascii=False, indent=2), file=sys.stderr)
        return 1
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8", newline="\n")
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
