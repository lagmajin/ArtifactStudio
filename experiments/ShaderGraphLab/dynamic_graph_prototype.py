#!/usr/bin/env python3
"""Source-only prototype for data-driven shader node registries and graphs.

The registry is declarative. Executable behavior is selected only from the
closed BUILTIN_IMPLEMENTATIONS table below; a registry file cannot inject HLSL
or Python code.
"""

from __future__ import annotations

import argparse
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


BUILTIN_IMPLEMENTATIONS = {
    "float.constant", "vector4.constant", "float.add", "float.subtract", "float.multiply",
    "vector4.add", "vector4.multiply",
    "float.replicate.rgb_alpha_one", "vector4.extract_x",
}
MAX_JSON_NESTING = 256
MAX_JSON_FILE_BYTES = 16 * 1024 * 1024
FORBIDDEN_CODE_FIELDS = {"hlsl", "source", "code", "expression", "python", "shadersource"}
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")


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
        if (not isinstance(value_type, str) or value_type not in {"Float32", "Vector4F32"} or
                symbol != expected_symbol or binding.get("blockBindingId") != "artifact.shadergraph.parameters" or
                not isinstance(binding.get("bindings"), list) or not binding["bindings"] or
                symbol in seen_symbols):
            raise ContractError("resident parameter binding is invalid or duplicated",
                                code="shader.binding.resident_contract.invalid", path=path)
        slot_count = 1 if value_type == "Float32" else 4
        if next_slot + slot_count > RESIDENT_GENERIC_PARAMETER_LANE_COUNT:
            raise ContractError("resident generic ABI supports at most sixteen scalar lanes",
                            code="shader.binding.resident_slot_overflow", path="/parameterBindings")
        seen_ids.add(parameter_id)
        seen_symbols.add(symbol)
        slot_indices = list(range(next_slot, next_slot + slot_count))
        abi_names = [f"g_P{slot}" for slot in slot_indices]
        hlsl_expression = abi_names[0] if slot_count == 1 else "float4(" + ", ".join(abi_names) + ")"
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


@dataclass(frozen=True)
class Parameter:
    parameter_id: str
    value_type: str
    default: float | tuple[float, float, float, float] | None


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
                allowed_socket_fields = {"socketId", "valueType", "required"} if is_input else {"socketId", "valueType"}
                if set(socket) - allowed_socket_fields or not {"socketId", "valueType"} <= set(socket):
                    raise ContractError(f"{socket_path} has missing or unknown fields", code="registry.socket.shape.invalid", path=socket_pointer)
                socket_id = socket.get("socketId")
                value_type = socket.get("valueType")
                if not isinstance(socket_id, str) or not IDENTIFIER.fullmatch(socket_id):
                    raise ContractError(f"{socket_path}.socketId is invalid", code="socket.id.invalid", path=f"{socket_pointer}/socketId")
                if socket_id in seen:
                    raise ContractError(f"duplicate socketId {socket_id} in {path_label}.{key}", code="socket.id.duplicate", path=f"{socket_pointer}/socketId")
                seen.add(socket_id)
                if not isinstance(value_type, str) or value_type not in {"Float", "Vector4"}:
                    raise ContractError(f"{socket_path}.valueType is unsupported by this prototype", code="socket.type.unsupported", path=f"{socket_pointer}/valueType")
                required = socket.get("required", False) if is_input else False
                if not isinstance(required, bool):
                    raise ContractError(f"{socket_path}.required must be boolean", code="socket.required.invalid", path=f"{socket_pointer}/required")
                parsed.append(Socket(socket_id, value_type, required))
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
            if not isinstance(parameter_type, str) or parameter_type not in {"Float32", "Vector4F32"}:
                raise ContractError(f"{parameter_path}.valueType is unsupported", code="parameter.type.unsupported", path=f"{parameter_pointer}/valueType")
            default = parameter.get("default")
            default_path = f"/operations/{index}/parameters/{parameter_index}/default"
            if default is None:
                canonical_default = None
            elif parameter_type == "Float32":
                canonical_default = _f32(default, f"{parameter_path}.default", path=default_path,
                                          code="registry.parameter.default.invalid")
            else:
                if not isinstance(default, list) or len(default) != 4:
                    raise ContractError(f"{parameter_path}.default must contain four Float32 values",
                                        code="registry.parameter.default.invalid", path=default_path)
                canonical_default = tuple(_f32(value, f"{parameter_path}.default[{component}]",
                                                path=f"{default_path}/{component}",
                                                code="registry.parameter.default.invalid")
                                          for component, value in enumerate(default))
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
        if len(operation.outputs) != 1:
            raise ContractError(f"{path_label} must declare exactly one output", code="operation.output.shape.invalid", path=f"{path}/outputs")
        if implementation_id == "float.constant" and (
                operation.inputs or operation.outputs[0].value_type != "Float" or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "value" or
                operation.parameters[0].value_type != "Float32"):
            raise ContractError(f"{path_label} float.constant requires zero inputs and one value parameter", code="operation.signature.invalid", path=path)
        if implementation_id == "vector4.constant" and (
                operation.inputs or operation.outputs[0].value_type != "Vector4" or
                len(operation.parameters) != 1 or operation.parameters[0].parameter_id != "value" or
                operation.parameters[0].value_type != "Vector4F32"):
            raise ContractError(f"{path_label} vector4.constant requires zero inputs, one Vector4 output, and one Vector4F32 value parameter", code="operation.signature.invalid", path=path)
        if implementation_id in {"float.add", "float.subtract", "float.multiply", "vector4.add", "vector4.multiply"} and (
                (operation.inputs[0].value_type if len(operation.inputs) > 0 else None) !=
                ("Float" if implementation_id.startswith("float.") else "Vector4") or
                (operation.inputs[1].value_type if len(operation.inputs) > 1 else None) !=
                ("Float" if implementation_id.startswith("float.") else "Vector4") or
                [item.socket_id for item in operation.inputs] != ["a", "b"] or
                not all(item.required for item in operation.inputs) or
                operation.outputs[0].socket_id != "result" or
                operation.outputs[0].value_type != ("Float" if implementation_id.startswith("float.") else "Vector4") or
                operation.parameters):
            raise ContractError(f"{path_label} {implementation_id} requires two matching typed inputs a/b, a matching result, and no parameters", code="operation.signature.invalid", path=path)
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
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required} for item in operation.inputs],
        "outputs": [{"socketId": item.socket_id, "valueType": item.value_type} for item in operation.outputs],
        "parameters": [{"parameterId": item.parameter_id, "valueType": item.value_type, "default": item.default} for item in operation.parameters],
    }
    encoded = json.dumps(contract, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest().upper()


def _operation_semantics_without_version(operation: Operation) -> str:
    payload = {
        "operationId": operation.operation_id,
        "implementationId": operation.implementation_id,
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required} for item in operation.inputs],
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
        "inputs": [{"socketId": item.socket_id, "valueType": item.value_type, "required": item.required} for item in operation.inputs],
        "outputs": [{"socketId": item.socket_id, "valueType": item.value_type} for item in operation.outputs],
        "parameters": [{"parameterId": item.parameter_id, "valueType": item.value_type, "default": item.default} for item in operation.parameters],
    } for operation in operations]


def instantiate_node(operation: Operation, node_id: str, *, x: float = 0.0, y: float = 0.0) -> dict[str, Any]:
    """Create editable node state from a registered operation's defaults."""
    if not isinstance(node_id, str) or not IDENTIFIER.fullmatch(node_id):
        raise ContractError("nodeId is invalid", code="node.id.invalid", path="/nodeId")
    parameters = {item.parameter_id: list(item.default) if isinstance(item.default, tuple) else item.default
                  for item in operation.parameters if item.default is not None}
    return {
        "nodeId": node_id,
        "operationId": operation.operation_id,
        "semanticVersion": operation.semantic_version,
        "parameters": parameters,
        "layout": {
            "x": _f32(x, "layout.x", path="/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(y, "layout.y", path="/layout/y", code="node.layout.coordinate.invalid"),
        },
    }


def add_node(document: Any, registry: dict[tuple[str, int], Operation],
             operation_id: str, semantic_version: int, node_id: str, *,
             x: float = 0.0, y: float = 0.0) -> dict[str, Any]:
    """Append one registry-backed node without changing links or active output."""
    edited = preserve_document(document)
    if (not isinstance(edited, dict) or isinstance(edited.get("schemaVersion"), bool) or
            not isinstance(edited.get("schemaVersion"), int) or edited.get("schemaVersion") not in (1, 2)):
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
        clone["parameters"] = canonical_parameters
        clone["layout"] = {
            "x": _f32(_f32(layout.get("x"), "source layout x", path=f"/nodes/{source_index}/layout/x", code="node.layout.coordinate.invalid") + x_offset,
                      "duplicate layout x", path=f"/nodes/{source_index}/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(_f32(layout.get("y"), "source layout y", path=f"/nodes/{source_index}/layout/y", code="node.layout.coordinate.invalid") + y_offset,
                      "duplicate layout y", path=f"/nodes/{source_index}/layout/y", code="node.layout.coordinate.invalid"),
        }
        clones.append(clone)
    edited["nodes"].extend(clones)

    if edited.get("schemaVersion") == 2:
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
    if preserved.get("schemaVersion") == 2:
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
    payload = {
        "kind": "artifact-shader-graph-rd-subgraph",
        "schemaVersion": 2,
        "nodes": copied_nodes,
        "links": internal_links,
        "graphParameters": graph_parameters,
        "registryContractLock": registry_lock,
        "layoutBounds": {"width": max_x - origin_x, "height": max_y - origin_y},
    }
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
            clipboard.get("schemaVersion") not in (1, 2)):
        raise ContractError("clipboard payload kind or schema is unsupported",
                            code="clipboard.schema.unsupported", path="/")
    clipboard_version = clipboard["schemaVersion"]
    expected_fields = {"kind", "schemaVersion", "nodes", "links", "graphParameters", "registryContractLock"}
    if clipboard_version == 2:
        expected_fields.add("layoutBounds")
    if (set(clipboard) != expected_fields or
            clipboard.get("kind") != "artifact-shader-graph-rd-subgraph" or
            clipboard_version not in (1, 2)):
        raise ContractError("clipboard payload kind or schema is unsupported",
                            code="clipboard.schema.unsupported", path="/")
    _reject_code_payload(clipboard)
    source_nodes, source_links = clipboard.get("nodes"), clipboard.get("links")
    if not isinstance(source_nodes, list) or not source_nodes or not isinstance(source_links, list):
        raise ContractError("clipboard nodes and links must be arrays",
                            code="clipboard.shape.invalid", path="/")
    bounds = clipboard.get("layoutBounds")
    if clipboard_version == 2:
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
            edited.get("schemaVersion") not in (1, 2)):
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
    clones = []
    layout_points = []
    for index, source in enumerate(source_nodes):
        clone = json.loads(canonical_json_text(source))
        layout = clone.get("layout")
        if not isinstance(layout, dict):
            raise ContractError("clipboard node layout must be an object",
                                code="node.layout.invalid", path=f"/nodes/{index}/layout")
        clone["nodeId"] = new_node_ids[source["nodeId"]]
        source_x = _f32(layout.get("x"), "clipboard layout x", path=f"/nodes/{index}/layout/x", code="node.layout.coordinate.invalid")
        source_y = _f32(layout.get("y"), "clipboard layout y", path=f"/nodes/{index}/layout/y", code="node.layout.coordinate.invalid")
        if clipboard_version == 2 and (source_x < 0 or source_y < 0 or source_x > bound_width or source_y > bound_height):
            raise ContractError("clipboard node layout exceeds its bounds",
                                code="clipboard.layout_bounds.invalid", path=f"/nodes/{index}/layout")
        layout_points.append((source_x, source_y))
        clone["layout"] = {
            "x": _f32(source_x + x_offset,
                      "paste layout x", path=f"/nodes/{index}/layout/x", code="node.layout.coordinate.invalid"),
            "y": _f32(source_y + y_offset,
                      "paste layout y", path=f"/nodes/{index}/layout/y", code="node.layout.coordinate.invalid"),
        }
        clones.append(clone)
    if clipboard_version == 2 and (max(point[0] for point in layout_points) != bound_width or
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


def _canonical_parameters(raw: Any, operation: Operation, node_id: str, node_index: int) -> dict[str, float]:
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
        else:
            if not isinstance(value, (list, tuple)) or len(value) != 4:
                raise ContractError(f"node {node_id}.{parameter_id} must contain four Float32 values",
                                    code="parameter.vector4.invalid", path=parameter_path)
            result[parameter_id] = [
                _f32(component, f"node {node_id}.{parameter_id}[{index}]",
                     path=f"{parameter_path}/{index}", code="parameter.float32.invalid")
                for index, component in enumerate(value)
            ]
    return result


def _canonical_graph_parameter_value(value_type: str, value: Any, path: str) -> float | list[float]:
    if value_type == "Float32":
        return _f32(value, "graph parameter", path=path, code="graph.parameter.value.invalid")
    if value_type == "Vector4F32":
        if not isinstance(value, (list, tuple)) or len(value) != 4:
            raise ContractError("Vector4 graph parameter requires four Float32 values",
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
        if not isinstance(value_type, str) or value_type not in {"Float32", "Vector4F32"}:
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
    if "registryContractLock" in edited:
        compatibility = compare_registry_contracts(edited["registryContractLock"], registry)
        if compatibility:
            issue = compatibility[0]
            raise ContractError(issue["message"], code=issue["code"], path=issue["path"])
    if edited.get("schemaVersion") == 1:
        edited["schemaVersion"] = 2
        edited["graphParameters"] = []
    if edited.get("schemaVersion") != 2 or not isinstance(edited.get("graphParameters"), list):
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
    if edited.get("schemaVersion") != 2 or not isinstance(parameters, list):
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
    if edited.get("schemaVersion") != 2 or not isinstance(parameters, list):
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
        graph_parameter.get("valueType") in {"Float32", "Vector4F32"} and
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


def validate_document_for_editing(document: Any, registry: dict[tuple[str, int], Operation]) -> dict[str, Any]:
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
        if isinstance(schema_version, bool) or not isinstance(schema_version, int) or schema_version not in (1, 2):
            report("schema.version.unsupported", "/schemaVersion", "document schemaVersion is unsupported; source document remains editable for repair", severity="warning")
            repairable = True
        if preserved.get("kind") != "artifact-shader-graph-rd-document":
            report("document.kind.unsupported", "/kind", "document kind is unsupported; source document remains editable for repair", severity="warning")
            repairable = True
        allowed = {"schemaVersion", "kind", "nodes", "links", "output", "registryContractLock"}
        required = {"schemaVersion", "kind", "nodes", "links", "output"}
        if schema_version == 2:
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

    node_by_id: dict[str, tuple[int, Operation, dict[str, Any]]] = {}
    canonical_node_parameters: dict[str, dict[str, float | list[float]]] = {}
    all_node_ids: set[str] = set()
    sanitized: dict[str, str] = {}
    for index, node in enumerate(raw_nodes):
        path = f"/nodes/{index}"
        if not isinstance(node, dict):
            report("node.invalid", path, "node must be an object")
            continue
        allowed_node = {"nodeId", "operationId", "semanticVersion", "parameters", "layout"}
        for key in sorted(set(node) - allowed_node):
            report("node.field.unknown", f"{path}/{_pointer_segment(str(key))}", f"unknown node field {key}; retained for repair", severity="warning")
            repairable = True
        for key in sorted(allowed_node - set(node)):
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
        node_by_id[node_id] = (index, operation, node)

    if preserved.get("schemaVersion") == 2:
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
                value_type_valid = isinstance(value_type, str) and value_type in {"Float32", "Vector4F32"}
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
            compile_result = compile_document_from_data(preserved, registry)
            for warning in compile_result.get("warnings", []):
                node_id = warning.removeprefix("node ").removesuffix(" is unreachable from the selected output")
                report("graph.node.unreachable", f"/nodes/{_pointer_segment(node_id)}", warning, severity="warning")
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


def compile_document_from_data(document: Any, registry: dict[tuple[str, int], Operation]) -> dict[str, Any]:
    _reject_code_payload(document)
    if (not isinstance(document, dict) or isinstance(document.get("schemaVersion"), bool) or
            not isinstance(document.get("schemaVersion"), int) or document.get("schemaVersion") not in (1, 2)):
        raise ContractError("document schemaVersion must be 1 or 2", code="schema.version.unsupported", path="/schemaVersion")
    if document.get("kind") != "artifact-shader-graph-rd-document":
        raise ContractError("document kind is unsupported", code="document.kind.unsupported", path="/kind")
    allowed_document_fields = {"schemaVersion", "kind", "nodes", "links", "output", "registryContractLock"}
    if document["schemaVersion"] == 2:
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

    nodes: dict[str, dict[str, Any]] = {}
    node_indices: dict[str, int] = {}
    sanitized: dict[str, str] = {}
    for index, raw in enumerate(raw_nodes):
        label = f"document.nodes[{index}]"
        if not isinstance(raw, dict):
            raise ContractError(f"{label} must be an object", code="node.invalid", path=f"/nodes/{index}")
        if set(raw) != {"nodeId", "operationId", "semanticVersion", "parameters", "layout"}:
            unknown = sorted(set(raw) - {"nodeId", "operationId", "semanticVersion", "parameters", "layout"})
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
        nodes[node_id] = {"operation": operation, "parameters": parameters, "raw": raw, "symbol": symbol}
        node_indices[node_id] = index

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
        field_size = 16 if value_type == "Vector4F32" else 4
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
    expressions: dict[tuple[str, str], str] = {}
    hlsl_lines = []
    if parameter_block is not None:
        hlsl_lines.append("cbuffer ArtifactGraphParameters {")
        hlsl_lines.extend(
            f"    {'float4' if field['valueType'] == 'Vector4F32' else 'float'} {field['hlslSymbol']} : packoffset({field['hlslPackOffset']});"
            for field in parameter_fields
        )
        hlsl_lines.append("};")
    for node_id in active_order:
        data = nodes[node_id]
        operation = data["operation"]
        implementation_id = operation.implementation_id
        symbol = data["symbol"]
        output_id = operation.outputs[0].socket_id
        if implementation_id == "float.constant":
            parameter_id = operation.parameters[0].parameter_id
            graph_parameter_id = data["graphParameterBindings"].get(parameter_id)
            expression = ("P_" + re.sub(r"[^A-Za-z0-9_]", "_", graph_parameter_id)
                          if graph_parameter_id is not None else
                          _hlsl_float(data["parameters"][parameter_id]))
        elif implementation_id == "vector4.constant":
            parameter_id = operation.parameters[0].parameter_id
            graph_parameter_id = data["graphParameterBindings"].get(parameter_id)
            if graph_parameter_id is not None:
                expression = "P_" + re.sub(r"[^A-Za-z0-9_]", "_", graph_parameter_id)
            else:
                values = data["parameters"][parameter_id]
                expression = "float4(" + ", ".join(_hlsl_float(value) for value in values) + ")"
        elif implementation_id == "float.replicate.rgb_alpha_one":
            source = writers[(node_id, operation.inputs[0].socket_id)]
            value = expressions[source]
            expression = f"float4({value}, {value}, {value}, 1.0f)"
        elif implementation_id == "vector4.extract_x":
            source = writers[(node_id, operation.inputs[0].socket_id)]
            expression = f"({expressions[source]}).x"
        else:
            source_a = writers[(node_id, operation.inputs[0].socket_id)]
            source_b = writers[(node_id, operation.inputs[1].socket_id)]
            a = expressions[source_a]
            b = expressions[source_b]
            operator = {
                "float.add": "+", "float.subtract": "-", "float.multiply": "*",
                "vector4.add": "+", "vector4.multiply": "*",
            }[implementation_id]
            expression = f"({a} {operator} {b})"
        output_symbol = re.sub(r"[^A-Za-z0-9_]", "_", output_id)
        variable = f"{symbol}_{output_symbol}"
        output_type = operation.outputs[0].value_type
        hlsl_type = "float4" if output_type == "Vector4" else "float"
        hlsl_lines.append(f"{hlsl_type} {variable} = {expression};")
        expressions[(node_id, output_id)] = variable

    root_expression = expressions[(output_node, output_socket)]
    hlsl = "\n".join(hlsl_lines) + f"\nreturn {root_expression};\n"
    semantic_projection = {
        "schemaVersion": document["schemaVersion"],
        "output": {"nodeId": output_node, "socketId": output_socket, "valueType": output_def.value_type},
        "nodes": [
            {"nodeId": node_id,
             "operationId": nodes[node_id]["operation"].operation_id,
             "semanticVersion": nodes[node_id]["operation"].semantic_version,
             "implementationId": nodes[node_id]["operation"].implementation_id,
             "parameters": nodes[node_id]["parameters"]}
            for node_id in sorted(active)
        ],
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
        "shaderStructureHashSha256": hashlib.sha256(structure_canonical).hexdigest().upper(),
        "parameterValuesHashSha256": hashlib.sha256(value_canonical).hexdigest().upper(),
        "parameterValues": value_projection,
        "parameterBindings": parameter_bindings,
        "parameterBlock": parameter_block,
        "topologicalNodeIds": active_order,
        "outputType": output_def.value_type,
        "hlslBody": hlsl,
        "warnings": warnings,
        "limitations": [
            "prototype supports Float/Vector4 sockets and nine closed implementation IDs only",
            "registry metadata may select existing implementations but cannot supply executable code or HLSL",
            "generated HLSL is a deterministic candidate string; no shader compiler/backend/runtime is invoked",
            "diagnostic codes and document paths are golden-checked for eleven representative rejection classes; other structured diagnostics need broader fixture coverage",
            "this is an R&D prototype, not ArtifactCore API or Blender compatibility evidence",
        ],
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
