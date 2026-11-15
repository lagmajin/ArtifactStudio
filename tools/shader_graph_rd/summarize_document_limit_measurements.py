#!/usr/bin/env python3
"""Summarize shader-graph limit measurements without selecting production limits.

Input is JSON Lines produced by an external measurement runner plus the matching
generated corpus. This tool performs deterministic aggregation and boundary
contract checks only; it does not run Core or infer resource thresholds.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Iterable


GROUP_FIELDS = (
    "runId", "caseId", "phase", "appRevision", "buildAndCompiler", "platform",
    "hardware", "instrumentation", "effectiveLimits",
)
PERCENTILE_FIELDS = (
    "parseNs", "structuralValidationNs", "canonicalizationNs", "loweringNs",
    "peakLiveBytes", "allocationCount", "allocatedBytes", "largestAllocationBytes",
    "stackHighWaterBytes",
)
MEASUREMENT_STAGES = (
    "configuration", "before-parse", "parse", "before-index", "before-adjacency",
    "validation", "depth-check", "canonicalization", "lowering", "accepted",
)
REQUIRED_CONTEXT_FIELDS = (
    "appRevision", "buildAndCompiler", "platform", "hardware", "instrumentation",
    "effectiveLimits",
)
REQUIRED_NON_EMPTY_FIELDS = (
    "runId", "appRevision", "buildAndCompiler", "platform", "hardware", "instrumentation",
)
DIMENSION_FIELDS = ("inputUtf8Bytes", "documentNodes", "documentLinks", "observedTraversalDepth")


def _canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(",", ":"))


def _nearest_rank(values: list[int], percentile: float) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    return ordered[max(0, math.ceil(percentile * len(ordered)) - 1)]


def _read_records(path: Path) -> tuple[list[dict[str, Any]], list[str]]:
    records: list[dict[str, Any]] = []
    errors: list[str] = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        return [], [f"cannot read measurement JSONL: {error}"]
    for line_number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as error:
            errors.append(f"line {line_number}: invalid JSON: {error.msg}")
            continue
        if not isinstance(value, dict):
            errors.append(f"line {line_number}: each measurement record must be a JSON object")
            continue
        records.append(value)
    return records, errors


def _load_corpus(path: Path) -> tuple[dict[str, dict[str, Any]], list[str]]:
    try:
        corpus = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return {}, [f"cannot read boundary corpus JSON: {error}"]
    if not isinstance(corpus, dict) or corpus.get("kind") != "artifact-shader-graph-document-limit-corpus":
        return {}, ["boundary corpus has an unsupported kind"]
    cases = corpus.get("cases")
    if not isinstance(cases, list):
        return {}, ["boundary corpus cases must be an array"]
    by_id: dict[str, dict[str, Any]] = {}
    errors: list[str] = []
    for case in cases:
        if not isinstance(case, dict) or not isinstance(case.get("caseId"), str):
            errors.append("boundary corpus contains a case without a string caseId")
            continue
        case_id = case["caseId"]
        if case_id in by_id:
            errors.append(f"boundary corpus repeats caseId {case_id}")
        by_id[case_id] = case
    return by_id, errors


def _validate_summary_schema(corpus: dict[str, Any]) -> list[str]:
    schema = corpus.get("measurementSummarySchema")
    if not isinstance(schema, dict) or schema.get("version") != 1:
        return ["corpus must declare measurementSummarySchema version 1"]
    errors: list[str] = []
    if schema.get("groupBy") != list(GROUP_FIELDS):
        errors.append("corpus summary groupBy does not match this reducer")
    boundary_schema = schema.get("boundaryDisposition", {})
    case_identity_rule = boundary_schema.get("caseIdentity", "") if isinstance(boundary_schema, dict) else ""
    if not isinstance(case_identity_rule, str) or "must equal the referenced generated corpus case" not in case_identity_rule:
        errors.append("corpus boundary schema does not pin measured case identity fields")
    percentile_schema = schema.get("percentiles", {})
    if not isinstance(percentile_schema, dict):
        return errors + ["corpus percentiles must be an object"]
    if percentile_schema.get("fields") != list(PERCENTILE_FIELDS):
        errors.append("corpus percentile fields do not match this reducer")
    if percentile_schema.get("values") != ["median", "p95", "p99"]:
        errors.append("corpus percentile names do not match this reducer")
    method = percentile_schema.get("method", "")
    if not isinstance(method, str) or "nearest-rank" not in method:
        errors.append("corpus percentile method is not nearest-rank")
    required = schema.get("requiredSummaryFields", [])
    summary_fields = {
        "caseId", "case", "phase", "context", "observedSampleCount", "fieldSummaries",
        "acceptCount", "rejectCount", "destinationPreservationUnknownCount",
        "contractViolationCount", "decisionStatus",
    }
    if not isinstance(required, list) or not all(isinstance(field, str) for field in required) or not summary_fields <= set(required):
        errors.append("corpus requiredSummaryFields omits reducer output fields")
    if not isinstance(schema.get("decisionRules"), str) or "contract violation takes precedence" not in schema["decisionRules"]:
        errors.append("corpus decisionRules do not declare the reducer status precedence")
    return errors


def _row_issues(record: dict[str, Any], required_fields: Iterable[str]) -> list[str]:
    issues: list[str] = []
    for field in required_fields:
        if field not in record:
            issues.append(f"missing required field {field}")
    for field in REQUIRED_CONTEXT_FIELDS:
        if field in record and (record[field] is None or record[field] == ""):
            issues.append(f"context field {field} must be present and non-empty")
    for field in REQUIRED_NON_EMPTY_FIELDS:
        if field in record and (not isinstance(record[field], str) or not record[field]):
            issues.append(f"{field} must be a non-empty string")
    limits = record.get("effectiveLimits")
    if limits is not None and (not isinstance(limits, dict) or any(type(value) is not int or value < 1 for value in limits.values())):
        issues.append("effectiveLimits must map each supplied limit to a positive integer")
    for field in DIMENSION_FIELDS:
        value = record.get(field)
        if value is not None and (type(value) is not int or value < 0):
            issues.append(f"{field} must be a non-negative integer or null")
    if record.get("phase") not in ("cold-parse", "warm-validation"):
        issues.append("phase must be cold-parse or warm-validation")
    sample_index = record.get("sampleIndex")
    if type(sample_index) is not int or sample_index < 0:
        issues.append("sampleIndex must be a non-negative integer")
    for field in PERCENTILE_FIELDS:
        value = record.get(field)
        if value is not None and (type(value) is not int or value < 0):
            issues.append(f"{field} must be a non-negative integer or null")
    stage = record.get("rejectionStage")
    if stage not in (*MEASUREMENT_STAGES, "unknown"):
        issues.append("rejectionStage is outside the declared stage vocabulary")
    if not isinstance(record.get("resultStatus"), str) or not record.get("resultStatus"):
        issues.append("resultStatus must be a non-empty string")
    if stage == "accepted" and record.get("destinationUnchanged") not in (None, True):
        issues.append("accepted sample cannot report destinationUnchanged=false")
    destination_unchanged = record.get("destinationUnchanged")
    if destination_unchanged is not None and type(destination_unchanged) is not bool:
        issues.append("destinationUnchanged must be true, false, or null")
    return issues


def _boundary_issues(record: dict[str, Any], case: dict[str, Any]) -> list[str]:
    issues: list[str] = []
    if record.get("effectiveLimits") != case.get("caseLimits"):
        issues.append("effectiveLimits do not match the generated corpus case")
    for record_field, case_field in (
        ("inputUtf8Bytes", "serializedUtf8Bytes"),
        ("documentNodes", "documentNodes"),
        ("documentLinks", "documentLinks"),
    ):
        if record.get(record_field) != case.get(case_field):
            issues.append(f"{record_field} does not match the generated corpus case")
    observed_stage = record.get("rejectionStage")
    disposition = case.get("expectedDisposition")
    if disposition == "accept-if-semantically-valid":
        if observed_stage != "accepted":
            issues.append("within-limit case was not accepted")
        return issues
    if not isinstance(disposition, str) or not disposition.startswith("reject-"):
        issues.append("case has no supported expectedDisposition")
        return issues
    if observed_stage in ("accepted", "unknown") or observed_stage not in MEASUREMENT_STAGES:
        issues.append("limit-plus-one case was not rejected at a known stage")
        return issues
    if record.get("destinationUnchanged") is False:
        issues.append("rejected limit-boundary sample changed its destination")

    required_max_stage = {
        "reject-before-parse": "before-parse",
        "reject-before-index-allocation": "before-index",
        "reject-before-adjacency-allocation": "before-adjacency",
        "reject-before-recursive-or-unbounded-traversal": "depth-check",
    }.get(disposition)
    if required_max_stage is None:
        issues.append(f"unsupported expectedDisposition {disposition}")
    elif MEASUREMENT_STAGES.index(observed_stage) > MEASUREMENT_STAGES.index(required_max_stage):
        issues.append(f"rejected at {observed_stage}, later than required maximum stage {required_max_stage}")
    return issues


def summarize(records: list[dict[str, Any]], cases: dict[str, dict[str, Any]], required_fields: list[str], minimum_samples: int = 30) -> dict[str, Any]:
    groups: dict[tuple[str, ...], list[dict[str, Any]]] = {}
    global_issues: list[str] = []
    for row_number, record in enumerate(records, 1):
        row_issues = _row_issues(record, required_fields)
        case_id = record.get("caseId")
        case = cases.get(case_id) if isinstance(case_id, str) else None
        if case is None:
            row_issues.append(f"caseId {case_id!r} is absent from the supplied corpus")
        else:
            row_issues.extend(_boundary_issues(record, case))
        if row_issues:
            global_issues.extend(f"record {row_number}: {issue}" for issue in row_issues)
        if any(field not in record for field in GROUP_FIELDS):
            continue
        key = tuple(_canonical(record[field]) for field in GROUP_FIELDS)
        groups.setdefault(key, []).append(record)

    summaries: list[dict[str, Any]] = []
    for key, rows in sorted(groups.items(), key=lambda item: item[0]):
        context = {field: rows[0][field] for field in GROUP_FIELDS}
        case_id = context["caseId"]
        case = cases.get(case_id, {}) if isinstance(case_id, str) else {}
        violations: list[str] = []
        for row in rows:
            violations.extend(_row_issues(row, required_fields))
        indices: list[int] = []
        for row in rows:
            index = row.get("sampleIndex")
            if type(index) is int and index >= 0:
                indices.append(index)
        if len(indices) != len(set(indices)):
            violations.append("sampleIndex values must be unique within a case/phase/context group")
        if indices and sorted(indices) != list(range(len(indices))):
            violations.append("sampleIndex values must be contiguous from zero within a group")
        for row in rows:
            violations.extend(_boundary_issues(row, case))
        field_summaries: dict[str, Any] = {}
        for field in PERCENTILE_FIELDS:
            values = [row[field] for row in rows if type(row.get(field)) is int and row[field] >= 0]
            field_summaries[field] = {
                "sampleCount": len(values),
                "median": _nearest_rank(values, 0.50),
                "p95": _nearest_rank(values, 0.95),
                "p99": _nearest_rank(values, 0.99),
            }
        accepted = sum(row.get("rejectionStage") == "accepted" for row in rows)
        rejected = sum(row.get("rejectionStage") in MEASUREMENT_STAGES[:-1] for row in rows)
        preservation_unknown = sum(
            row.get("rejectionStage") in MEASUREMENT_STAGES[:-1] and row.get("destinationUnchanged") is None
            for row in rows
        )
        observed_count = len(rows)
        all_metric_values_missing = all(item["sampleCount"] == 0 for item in field_summaries.values())
        if violations:
            decision = "contract-violation"
        elif observed_count < minimum_samples:
            decision = "insufficient-samples"
        elif all_metric_values_missing or preservation_unknown:
            decision = "incomplete-instrumentation"
        else:
            decision = "evidence-only-no-production-limit"
        summaries.append({
            "caseId": context["caseId"],
            "case": {
                field: case.get(field)
                for field in ("limitField", "boundary", "limitValue", "measuredDimensionValue", "payloadSha256", "expectedDisposition")
            },
            "phase": context["phase"],
            "context": {"runId": context["runId"], **{field: context[field] for field in REQUIRED_CONTEXT_FIELDS}},
            "observedSampleCount": observed_count,
            "fieldSummaries": field_summaries,
            "acceptCount": accepted,
            "rejectCount": rejected,
            "destinationPreservationUnknownCount": preservation_unknown,
            "contractViolationCount": len(violations),
            "contractViolations": sorted(set(violations)),
            "decisionStatus": decision,
        })
    return {
        "kind": "artifact-shader-graph-document-limit-measurement-summary",
        "schemaVersion": 1,
        "minimumTimedSamples": minimum_samples,
        "groupBy": list(GROUP_FIELDS),
        "summaryCount": len(summaries),
        "globalIssueCount": len(global_issues),
        "globalIssues": global_issues,
        "summaries": summaries,
        "groupContractViolationCount": sum(item["contractViolationCount"] for item in summaries),
        "productionLimitsSelected": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--records", required=True, type=Path, help="raw measurement records in JSON Lines format")
    parser.add_argument("--corpus", required=True, type=Path, help="matching generated boundary corpus JSON")
    parser.add_argument("--output", type=Path, help="summary JSON output (defaults to stdout)")
    args = parser.parse_args()

    records, read_errors = _read_records(args.records)
    if not records and not read_errors:
        read_errors.append("measurement JSONL contains no records")
    cases, corpus_errors = _load_corpus(args.corpus)
    required_fields: list[str] = []
    minimum_samples = 30
    if not corpus_errors:
        # The corpus is generated from the same record contract that defines the summary.
        corpus_doc = json.loads(args.corpus.read_text(encoding="utf-8"))
        corpus_errors.extend(_validate_summary_schema(corpus_doc))
        required_fields = corpus_doc.get("measurementRecordFields", [])
        summary_schema = corpus_doc.get("measurementSummarySchema", {})
        minimum_samples = summary_schema.get("minimumTimedSamples", 30) if isinstance(summary_schema, dict) else 30
        if type(minimum_samples) is not int or minimum_samples < 1:
            corpus_errors.append("measurement summary schema minimumTimedSamples must be a positive integer")
            minimum_samples = 30
        if not isinstance(required_fields, list) or not all(isinstance(field, str) for field in required_fields):
            corpus_errors.append("corpus measurementRecordFields must be a string array")
            required_fields = []
    result = summarize(records, cases, required_fields, minimum_samples)
    result["globalIssues"] = read_errors + corpus_errors + result["globalIssues"]
    result["globalIssueCount"] = len(result["globalIssues"])
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
    else:
        print(rendered, end="")
    has_contract_violations = result["globalIssueCount"] > 0 or result["groupContractViolationCount"] > 0
    return 2 if read_errors or corpus_errors else (1 if has_contract_violations else 0)


if __name__ == "__main__":
    raise SystemExit(main())
