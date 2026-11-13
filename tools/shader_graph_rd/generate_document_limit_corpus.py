#!/usr/bin/env python3
"""Generate deterministic graph documents for caller-supplied limit sweeps.

This is a data generator only. It does not invoke Core, build, tests, or runtime
measurements. Generated numeric limits are supplied by the experiment operator
and are never promoted to production defaults by this tool.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


def _graph(node_count: int, link_count: int) -> dict[str, Any]:
    """Build a valid, Output-reachable unary Math chain of the requested size."""
    if node_count < 2 or link_count != node_count - 1:
        raise ValueError("reachable-chain corpus requires nodes >= 2 and links == nodes - 1")
    nodes: list[dict[str, Any]] = [
        {"id": "math_000000", "type": "Math", "params": {"operation": 0}}
    ]
    links: list[dict[str, str]] = []
    for index in range(1, node_count - 1):
        node_id = f"math_{index:06d}"
        nodes.append({"id": node_id, "type": "Math", "params": {"operation": 0}})
        links.append({"from": f"math_{index - 1:06d}", "out": "Value", "to": node_id, "in": "A"})
    nodes.append({"id": "output", "type": "MaterialOutput", "params": {}})
    links.append({"from": f"math_{node_count - 2:06d}", "out": "Value", "to": "output", "in": "Surface"})
    return {"version": 1, "kind": "artifact-material-graph", "nodes": nodes, "links": links}


def _count_boundary_document(node_count: int, link_count: int) -> dict[str, Any]:
    """Build a valid, Output-reachable unary Math chain with exact counts."""
    if node_count < 3 or link_count != node_count - 1:
        raise ValueError("reachable count corpus requires nodes >= 3 and links == nodes - 1")
    math_count = node_count - 2
    return _depth_boundary_document(math_count)


def _depth_boundary_document(math_count: int) -> dict[str, Any]:
    """Build a Math chain feeding BaseColor of a valid Principled surface."""
    if math_count < 1:
        raise ValueError("depth boundary requires at least one Math operation")
    nodes = [
        {"id": f"math_{index:06d}", "type": "Math", "params": {"operation": 0}}
        for index in range(math_count)
    ]
    nodes.extend([
        {"id": "surface", "type": "Principled", "params": {}},
        {"id": "output", "type": "MaterialOutput", "params": {}},
    ])
    links = [
        {"from": f"math_{index:06d}", "out": "Value", "to": f"math_{index + 1:06d}", "in": "A"}
        for index in range(math_count - 1)
    ]
    links.extend([
        {"from": f"math_{math_count - 1:06d}", "out": "Value", "to": "surface", "in": "Metallic"},
        {"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"},
    ])
    return {"version": 1, "kind": "artifact-material-graph", "nodes": nodes, "links": links}


def _wide_boundary_document(node_count: int, link_count: int) -> dict[str, Any]:
    """Add disconnected valid Value records around a small reachable chain."""
    if node_count < 3 or link_count < 1 or link_count > node_count - 2:
        raise ValueError("wide boundary requires nodes >= 3 and links in [1, nodes-2]")
    # A connected chain gives unambiguous single-writer links. Remaining nodes
    # are valid disconnected constants, isolating document/index size from IR reachability.
    math_count = link_count - 1
    document = _depth_boundary_document(max(1, math_count))
    for index in range(node_count - len(document["nodes"])):
        document["nodes"].append({"id": f"wide_unused_{index:06d}", "type": "Value", "params": {"value": 0.0}})
    if link_count == 1:
        document = {
            "version": 1,
            "kind": "artifact-material-graph",
            "nodes": [
                {"id": "surface", "type": "Principled", "params": {}},
                {"id": "output", "type": "MaterialOutput", "params": {}},
            ] + [{"id": f"wide_unused_{index:06d}", "type": "Value", "params": {"value": 0.0}} for index in range(node_count - 2)],
            "links": [{"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"}],
        }
    return document


def _canonical_bytes(document: dict[str, Any]) -> bytes:
    return (json.dumps(document, ensure_ascii=False, separators=(",", ":"), sort_keys=True) + "\n").encode("utf-8")


def _candidate_math_depth(document: dict[str, Any]) -> int:
    by_id = {node["id"]: node for node in document["nodes"]}
    incoming = {node_id: [] for node_id in by_id}
    for link in document["links"]:
        incoming[link["to"]].append(link["from"])
    output_ids = [node["id"] for node in document["nodes"] if node["type"] == "MaterialOutput"]
    if len(output_ids) != 1:
        return 0
    best: dict[str, int] = {}
    visiting: set[str] = set()
    def visit(node_id: str) -> int:
        if node_id in best:
            return best[node_id]
        if node_id in visiting:
            raise ValueError("cycle in generated graph")
        visiting.add(node_id)
        own = int(by_id[node_id]["type"] == "Math")
        depth = own + max((visit(parent) for parent in incoming[node_id]), default=0)
        visiting.remove(node_id)
        best[node_id] = depth
        return depth
    return visit(output_ids[0])


def _byte_boundary_document(target_bytes: int) -> tuple[dict[str, Any], bytes]:
    """Pad an opaque string extension until serialized UTF-8 length is exact."""
    base = _graph(2, 1)
    node = base["nodes"][0]
    node["opaqueExtensions"] = {"rdPadding": ""}
    base_size = len(_canonical_bytes(base))
    # JSON-escaped ASCII padding contributes exactly one byte per character.
    padding_length = target_bytes - base_size
    if padding_length < 0:
        raise ValueError(f"maxJsonBytes boundary {target_bytes} is smaller than minimum valid graph size {base_size}")
    node["opaqueExtensions"]["rdPadding"] = "x" * padding_length
    payload = _canonical_bytes(base)
    if len(payload) != target_bytes:
        raise AssertionError(f"byte-boundary generator drift: expected {target_bytes}, got {len(payload)}")
    return base, payload


def build_case_set(limits: dict[str, int]) -> dict[str, Any]:
    required = ("maxJsonBytes", "maxNodes", "maxLinks", "maxTraversalDepth")
    if set(limits) != set(required) or any(type(limits[key]) is not int or limits[key] < 1 for key in required):
        raise ValueError("all four DocumentLimits values must be positive integers")
    cases: list[dict[str, Any]] = []
    for field in required:
        limit = limits[field]
        # Isolate one dimension at a time. Orthogonal limits are explicit
        # controls, raised above this case's payload requirements.
        for delta, label in ((-1, "limit-minus-one"), (0, "exact-limit"), (1, "limit-plus-one")):
            value = limit + delta
            case_limits = dict(limits)
            case_limits[field] = limit
            if field == "maxJsonBytes":
                document, payload = _byte_boundary_document(value)
                expected = "accept-if-semantically-valid" if delta <= 0 else "reject-before-parse"
                observation = "UTF-8 input byte boundary; include extension payload bytes in measured count"
                node_count = len(document["nodes"])
                link_count = len(document["links"])
                traversal_depth = _candidate_math_depth(document)
                payload_sha256 = hashlib.sha256(payload).hexdigest().upper()
            else:
                if field == "maxTraversalDepth":
                    math_count = value
                    node_count = math_count + 2
                    link_count = math_count + 1
                    document = _depth_boundary_document(math_count)
                    traversal_depth = math_count
                    case_limits["maxNodes"] = max(case_limits["maxNodes"], node_count)
                    case_limits["maxLinks"] = max(case_limits["maxLinks"], link_count)
                else:
                    if field == "maxNodes":
                        node_count = value
                        link_count = max(1, min(4, node_count - 2))
                        document = _wide_boundary_document(node_count, link_count)
                        case_limits["maxLinks"] = max(case_limits["maxLinks"], len(document["links"]))
                        case_limits["maxTraversalDepth"] = max(case_limits["maxTraversalDepth"], _candidate_math_depth(document))
                        case_limits["maxNodes"] = limit
                    elif field == "maxLinks":
                        node_count = value + 1
                        link_count = value
                        case_limits["maxNodes"] = max(case_limits["maxNodes"], node_count)
                        document = _count_boundary_document(node_count, link_count)
                        case_limits["maxTraversalDepth"] = max(case_limits["maxTraversalDepth"], node_count)
                payload = _canonical_bytes(document)
                expected = "accept-if-semantically-valid" if delta <= 0 else {
                    "maxNodes": "reject-before-index-allocation",
                    "maxLinks": "reject-before-adjacency-allocation",
                    "maxTraversalDepth": "reject-before-recursive-or-unbounded-traversal",
                }[field]
                observation = "valid output-reachable unary Math chain; implementation must report its own depth definition"
                payload_sha256 = hashlib.sha256(payload).hexdigest().upper()
                if field != "maxTraversalDepth":
                    traversal_depth = _candidate_math_depth(document)
                else:
                    case_limits["maxTraversalDepth"] = limit
            if field != "maxJsonBytes":
                case_limits["maxJsonBytes"] = max(case_limits["maxJsonBytes"], len(payload))
            cases.append({
                "limitField": field,
                "boundary": label,
                "limitValue": limit,
                "caseLimits": case_limits,
                "measuredDimensionValue": value,
                "documentNodes": len(document["nodes"]) if document is not None else 0,
                "documentLinks": len(document["links"]) if document is not None else 0,
                "serializedUtf8Bytes": len(payload),
                "traversalDepthCandidate": traversal_depth,
                "limitControlPolicy": "target dimension fixed at the requested limit; all orthogonal resource dimensions are raised above payload requirements",
                "depthDefinition": "candidate Math-chain operation count; implementation under test must report its own depth metric",
                "expectedDisposition": expected,
                "payloadSha256": payload_sha256,
                "payload": document,
                "observation": observation,
            })
    return {
        "kind": "artifact-shader-graph-document-limit-corpus",
        "schemaVersion": 1,
        "status": "generated-inputs-only-not-core-runtime-evidence",
        "limitsAreExperimentalInputsNotProductionDefaults": True,
        "limits": limits,
        "cases": cases,
        "measurementRecordFields": [
            "runId", "caseId", "appRevision", "buildAndCompiler", "platform", "instrumentation",
            "parseValidationCanonicalizationLoweringNs", "peakLiveBytes", "allocationCount",
            "allocatedBytes", "largestAllocationBytes", "stackHighWaterBytes", "rejectionStage",
            "destinationUnchanged", "resultStatus", "notes",
        ],
    }


def validate_case_set(corpus: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    cases = corpus.get("cases", [])
    expected_fields = {"maxJsonBytes", "maxNodes", "maxLinks", "maxTraversalDepth"}
    if len(cases) != 12 or {case.get("limitField") for case in cases} != expected_fields:
        errors.append("corpus must contain three boundary cases for each DocumentLimits field")
    for case in cases:
        field = case.get("limitField")
        label = case.get("boundary")
        delta = {"limit-minus-one": -1, "exact-limit": 0, "limit-plus-one": 1}.get(label)
        if delta is None or case.get("measuredDimensionValue") != case.get("limitValue", 0) + delta:
            errors.append(f"{field}/{label}: measured dimension must equal limit plus its declared boundary offset")
            continue
        case_limits = case.get("caseLimits", {})
        if case_limits.get(field) != case.get("limitValue"):
            errors.append(f"{field}/{label}: target limit must remain exact while orthogonal limits are controls")
        if any(type(value) is not int or value < 1 for value in case_limits.values()):
            errors.append(f"{field}/{label}: every effective case limit must be a positive integer")
        if "all orthogonal resource dimensions are raised" not in case.get("limitControlPolicy", ""):
            errors.append(f"{field}/{label}: explicit orthogonal-limit control policy is missing")
        payload = case.get("payload")
        if not isinstance(payload, dict):
            errors.append(f"{field}/{label}: graph payload must be present")
            continue
        encoded = _canonical_bytes(payload)
        if len(encoded) != case.get("serializedUtf8Bytes"):
            errors.append(f"{field}/{label}: UTF-8 byte count does not match payload")
        if hashlib.sha256(encoded).hexdigest().upper() != case.get("payloadSha256"):
            errors.append(f"{field}/{label}: payload SHA-256 does not match")
        nodes, links = payload.get("nodes", []), payload.get("links", [])
        if len(nodes) != case.get("documentNodes") or len(links) != case.get("documentLinks"):
            errors.append(f"{field}/{label}: graph counts do not match payload")
        node_ids = [node.get("id") for node in nodes]
        if len(node_ids) != len(set(node_ids)):
            errors.append(f"{field}/{label}: node IDs must be unique")
        link_edges = [(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in links]
        input_keys = [(link.get("to"), link.get("in")) for link in links]
        if len(link_edges) != len(set(link_edges)) or len(input_keys) != len(set(input_keys)):
            errors.append(f"{field}/{label}: links must be unique and each input single-writer")
        ids = set(node_ids)
        if any(source not in ids or target not in ids for source, _, target, _ in link_edges):
            errors.append(f"{field}/{label}: every link endpoint must exist")
        adjacency = {node_id: [] for node_id in node_ids}
        for source, _, target, _ in link_edges:
            adjacency[source].append(target)
        visiting: set[str] = set()
        visited: set[str] = set()
        def visit(node_id: str) -> bool:
            if node_id in visiting:
                return False
            if node_id in visited:
                return True
            visiting.add(node_id)
            acyclic = all(visit(target) for target in adjacency[node_id])
            visiting.remove(node_id)
            visited.add(node_id)
            return acyclic
        if not all(visit(node_id) for node_id in node_ids):
            errors.append(f"{field}/{label}: generated graph must be acyclic")
        if field == "maxJsonBytes" and (case.get("serializedUtf8Bytes") != case.get("measuredDimensionValue") or not nodes[0].get("opaqueExtensions")):
            errors.append(f"{field}/{label}: exact byte boundary must include opaque extension bytes")
        for other_field, observed in (("maxNodes", len(nodes)), ("maxLinks", len(links)), ("maxTraversalDepth", case.get("traversalDepthCandidate", 0))):
            if other_field != field and observed > case_limits.get(other_field, 0):
                errors.append(f"{field}/{label}: orthogonal {other_field} control is below the generated document requirement")
        if field != "maxJsonBytes" and case.get("serializedUtf8Bytes", 0) > case_limits.get("maxJsonBytes", 0):
            errors.append(f"{field}/{label}: orthogonal maxJsonBytes control is below the generated payload size")
        if field == "maxNodes" and len(nodes) != case.get("measuredDimensionValue"):
            errors.append(f"{field}/{label}: node boundary does not match generated node count")
        if field == "maxLinks" and len(links) != case.get("measuredDimensionValue"):
            errors.append(f"{field}/{label}: link boundary does not match generated link count")
        if field == "maxTraversalDepth" and case.get("traversalDepthCandidate") != case.get("measuredDimensionValue"):
            errors.append(f"{field}/{label}: traversal-depth boundary does not match candidate Math operation count")
        if case.get("traversalDepthCandidate") != _candidate_math_depth(payload):
            errors.append(f"{field}/{label}: candidate depth must match output-reachable Math depth")
        if field == "maxTraversalDepth" and case.get("traversalDepthCandidate") != case.get("measuredDimensionValue"):
            errors.append(f"{field}/{label}: candidate Math-chain depth does not match requested depth boundary")
        expected_rejection = {
            "maxJsonBytes": "reject-before-parse",
            "maxNodes": "reject-before-index-allocation",
            "maxLinks": "reject-before-adjacency-allocation",
            "maxTraversalDepth": "reject-before-recursive-or-unbounded-traversal",
        }.get(field)
        if delta == 1 and case.get("expectedDisposition") != expected_rejection:
            errors.append(f"{field}/{label}: limit-plus-one must declare early rejection")
        if delta <= 0 and case.get("expectedDisposition") != "accept-if-semantically-valid":
            errors.append(f"{field}/{label}: within-limit cases must declare conditional acceptance")
    if corpus.get("limitsAreExperimentalInputsNotProductionDefaults") is not True:
        errors.append("corpus must explicitly keep supplied limits experimental")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--max-json-bytes", type=int, required=True)
    parser.add_argument("--max-nodes", type=int, required=True)
    parser.add_argument("--max-links", type=int, required=True)
    parser.add_argument("--max-traversal-depth", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    limits = {
        "maxJsonBytes": args.max_json_bytes,
        "maxNodes": args.max_nodes,
        "maxLinks": args.max_links,
        "maxTraversalDepth": args.max_traversal_depth,
    }
    result = build_case_set(limits)
    errors = validate_case_set(result)
    if errors:
        raise SystemExit("\n".join(errors))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {len(result['cases'])} parameterized boundary cases to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
