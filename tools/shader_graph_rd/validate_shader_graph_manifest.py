#!/usr/bin/env python3
"""Validate Shader Graph R&D fixtures, assets, and evidence-manifest consistency."""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import inspect
import json
import math
from dataclasses import replace
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib


def decode_rgba8(data: bytes, width: int, height: int) -> bytes:
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("invalid PNG signature")
    offset = 8
    compressed = bytearray()
    image_width = image_height = bit_depth = color_type = None
    ended = False
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError("truncated PNG chunk")
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        end = offset + 12 + size
        if end > len(data):
            raise ValueError("PNG chunk exceeds file")
        payload = data[offset + 8:offset + 8 + size]
        crc = struct.unpack_from(">I", data, offset + 8 + size)[0]
        if crc != zlib.crc32(kind + payload) & 0xffffffff:
            raise ValueError(f"CRC mismatch in {kind!r}")
        if kind == b"IHDR":
            if image_width is not None or size != 13:
                raise ValueError("invalid IHDR")
            image_width, image_height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if (image_width, image_height, bit_depth, color_type, compression, filtering, interlace) != (width, height, 8, 6, 0, 0, 0):
                raise ValueError("expected declared non-interlaced RGBA8 dimensions")
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            ended = True
            offset = end
            break
        offset = end
    if not ended or image_width is None:
        raise ValueError("incomplete PNG")
    raw = zlib.decompress(compressed)
    stride = width * 4
    if len(raw) != height * (stride + 1):
        raise ValueError("unexpected decompressed PNG size")
    result = bytearray(height * stride)
    previous = bytearray(stride)
    pos = 0
    for y in range(height):
        filter_type = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + stride])
        pos += stride
        for i in range(stride):
            left = row[i - 4] if i >= 4 else 0
            above = previous[i]
            upper_left = previous[i - 4] if i >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                distances = (abs(p - left), abs(p - above), abs(p - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError(f"unsupported PNG filter {filter_type}")
            row[i] = (row[i] + predictor) & 0xff
        result[y * stride:(y + 1) * stride] = row
        previous = row
    return bytes(result)


def probe_base_color_capture_analyzer(root: Path) -> bool:
    analyzer_path = root / "tools/shader_graph_rd/analyze_shader_graph_rgba32f.py"
    spec = importlib.util.spec_from_file_location(
        "shader_graph_rgba32f_capture_analyzer_probe", analyzer_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load RGBA32F capture analyzer")
    analyzer = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = analyzer
    spec.loader.exec_module(analyzer)

    factors = [0.0, 0.25, 0.5, 0.75, 1.0]
    coordinates = [
        {"factor": factor, "x": index, "y": 0}
        for index, factor in enumerate(factors)
    ]
    expected = [1.0, 2.0, 3.0, 4.0]
    tolerance = 0.125
    operation_ids = {
        "add": "artifact.rd.math.add.vector4.float32",
        "subtract": "artifact.rd.math.subtract.vector4.float32",
        "multiply": "artifact.rd.math.multiply.vector4.float32",
        "dividePositiveFloor": "artifact.rd.math.divide.vector4.positive_floor.float32",
    }
    with tempfile.TemporaryDirectory(prefix="shader-graph-rgba32f-analyzer-") as directory:
        capture_directory = Path(directory)
        capture_records = []
        for variant, operation_id in operation_ids.items():
            legacy_values = expected * len(factors)
            projected_values = []
            samples = []
            for factor in factors:
                projected_sample = expected.copy()
                if variant == "subtract":
                    projected_sample[0] += 0.25
                projected_values.extend(projected_sample)
                samples.append({
                    "panelUv0X": factor,
                    "expectedRgba": expected,
                    "legacyCoreRgba": expected,
                    "projectedCandidateRgba": projected_sample,
                })
            legacy_path = capture_directory / f"{variant}-legacy.rgba32f"
            projected_path = capture_directory / f"{variant}-projected.rgba32f"
            legacy_path.write_bytes(struct.pack("=" + "f" * len(legacy_values), *legacy_values))
            projected_path.write_bytes(
                struct.pack("=" + "f" * len(projected_values), *projected_values))
            capture_records.append({
                "variantId": variant,
                "operationId": operation_id,
                "legacyRawCapture": legacy_path.name,
                "legacyRawCaptureSha256": hashlib.sha256(legacy_path.read_bytes()).hexdigest().upper(),
                "projectedRawCapture": projected_path.name,
                "projectedRawCaptureSha256": hashlib.sha256(projected_path.read_bytes()).hexdigest().upper(),
                "numericSamples": samples,
            })
        report = {
            "samplePixelCoordinates": coordinates,
            "baseColorMathOperationCaptures": capture_records,
            "dynamicBaseColorMath": {
                "numericToleranceAbsolute": tolerance,
                "numericToleranceUnit": "linear RGBA32F channel value",
            },
        }
        analysis = analyzer._sampled_capture_comparisons(
            report, capture_directory / "report.json", len(factors), 1)
        if (analysis is None or analysis.get("sampleCountPerVariant") != 5 or
                analysis.get("absoluteTolerance") != tolerance or
                analysis.get("toleranceUnit") != "linear RGBA32F channel value"):
            return False
        by_variant = {item["variantId"]: item for item in analysis["operationCaptures"]}
        if (set(by_variant) != set(operation_ids) or
                by_variant["subtract"].get("projectedVsGoldenToleranceExceedanceCount") != 5 or
                by_variant["subtract"].get("legacyVsProjectedToleranceExceedanceCount") != 5 or
                any(by_variant[name].get("projectedVsGoldenToleranceExceedanceCount") != 0
                    for name in ("add", "multiply", "dividePositiveFloor"))):
            return False
        capture_records[0]["legacyRawCaptureSha256"] = "0" * 64
        try:
            analyzer._sampled_capture_comparisons(
                report, capture_directory / "report.json", len(factors), 1)
        except ValueError as error:
            return "SHA-256 differs" in str(error)
        return False


def canonical_legacy_document(payload: dict) -> bytes:
    nodes = sorted(payload["nodes"], key=lambda node: node["id"].encode("utf-8"))
    links = sorted(payload["links"], key=lambda link: tuple(
        link[key].encode("utf-8") for key in ("from", "out", "to", "in")
    ))
    canonical = {"version": payload["version"], "kind": payload["kind"], "nodes": nodes, "links": links}
    return (json.dumps(canonical, ensure_ascii=False, separators=(",", ":"), sort_keys=True) + "\n").encode("utf-8")


def semantic_legacy_projection(payload: dict) -> bytes:
    semantic = json.loads(canonical_legacy_document(payload))
    for node in semantic["nodes"]:
        node.pop("x", None)
        node.pop("y", None)
    return (json.dumps(semantic, ensure_ascii=False, separators=(",", ":"), sort_keys=True) + "\n").encode("utf-8")


def apply_legacy_conversion_probe(opcode: str, value: object) -> object:
    if opcode == "convert.scalar.replicate.2":
        return [value, value]
    if opcode == "convert.scalar.replicate.3":
        return [value, value, value]
    if opcode == "convert.scalar.replicate.rgb_alpha_one":
        return [value, value, value, 1.0]
    if opcode in {"convert.vector2.extract_x", "convert.vector3.extract_x", "convert.vector4.extract_x"}:
        return value[0]
    if opcode == "convert.vector2.append_z_zero":
        return [value[0], value[1], 0.0]
    if opcode == "convert.vector2.append_z_zero_alpha_one":
        return [value[0], value[1], 0.0, 1.0]
    if opcode in {"convert.vector3.truncate_xy", "convert.vector4.truncate_xy"}:
        return value[:2]
    if opcode == "convert.vector3.append_alpha_one":
        return [value[0], value[1], value[2], 1.0]
    if opcode == "convert.vector4.truncate_xyz":
        return value[:3]
    raise ValueError(f"unsupported legacy conversion probe opcode: {opcode}")


def validate(root: Path) -> list[str]:
    errors: list[str] = []
    fixture_root = root / "tests/fixtures/shader-graph-rd"
    fixture_payloads: dict[str, dict] = {}
    manifest_path = fixture_root / "evaluation_manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return [f"cannot read manifest: {error}"]
    if manifest.get("manifestVersion") != 1 or manifest.get("evaluation") != "shader-graph-rd":
        errors.append("manifest identity/version is invalid")
    candidate_generator = (root / manifest.get("coreTypedOperationSchemaProposal", {})
                          .get("dynamicGraphPrototype", {})
                          .get("legacyCoreProjection", {}).get("generator", ""))
    if not candidate_generator.is_file():
        errors.append("legacy Core projection candidate generator is missing")
    else:
        try:
            generator_check = subprocess.run(
                [sys.executable, str(candidate_generator), "--check"],
                cwd=root, capture_output=True, text=True, check=False)
            if generator_check.returncode != 0:
                errors.append("legacy Core projection candidate is not reproducible: " +
                              generator_check.stderr.strip())
        except OSError as error:
            errors.append(f"cannot run legacy Core projection candidate check: {error}")

    fixtures = manifest.get("fixtures")
    if not isinstance(fixtures, list):
        return errors + ["fixtures must be an array"]
    declared: set[str] = set()
    disk_fixtures = {
        path.relative_to(fixture_root).as_posix()
        for path in fixture_root.rglob("*.json")
        if path.name != "evaluation_manifest.json" and "planned" not in path.parts
        and path.parent.name != "textures" and path.name not in {"ramp_duplicate_stops_formula_golden.json", "math_operation_formula_golden.json", "mixrgb_formula_golden.json", "lowering_identity_proposal_golden.json", "typed_ir_projection_readiness.json", "typed_ir_explicit_conversion_expression.json", "typed_ir_vector4_to_vector3_expression.json", "operation_registry_audit.json", "constant_value_canonicalization_golden.json", "surface_alpha_formula_golden.json", "luminance_formula_golden.json", "clamp_formula_golden.json", "vector4_invert_formula_golden.json", "map_range_formula_golden.json", "dynamic_add_lowering_candidate.json"}
    }
    for fixture in fixtures:
        if not isinstance(fixture, dict) or not isinstance(fixture.get("file"), str):
            errors.append("fixture entry must declare file")
            continue
        relative = fixture["file"]
        if relative in declared:
            errors.append(f"duplicate fixture declaration: {relative}")
        declared.add(relative)
        path = (fixture_root / relative).resolve()
        try:
            path.relative_to(fixture_root.resolve())
        except ValueError:
            errors.append(f"fixture path escapes fixture directory: {relative}")
            continue
        if not path.is_file():
            errors.append(f"missing fixture: {relative}")
            continue
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            errors.append(f"invalid fixture JSON {relative}: {error}")
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
        if fixture.get("sha256", "").upper() != digest:
            errors.append(f"SHA-256 mismatch for fixture {relative}")
        non_graph_artifact = fixture.get("class") in {"planned", "research-artifact"}
        if not non_graph_artifact and not isinstance(payload, dict):
            errors.append(f"fixture root must be an object: {relative}")
        if isinstance(payload, dict) and not non_graph_artifact:
            fixture_payloads[relative] = payload
            is_research_artifact = relative == "entry_path_compatibility_cases.json"
            if not is_research_artifact and payload.get("version") != manifest.get("application", {}).get("graphFormatVersion"):
                errors.append(f"fixture version differs from application graph format: {relative}")
            if not is_research_artifact and relative not in {"wrong_kind_negative.json", "missing_arrays_negative.json"} and payload.get("kind") != manifest.get("application", {}).get("graphFormat"):
                errors.append(f"fixture kind differs from application graph format: {relative}")
            for field in ("nodes", "links"):
                if not is_research_artifact and relative != "missing_arrays_negative.json" and not isinstance(payload.get(field), list):
                    errors.append(f"fixture must contain {field} array: {relative}")
    if declared != disk_fixtures:
        errors.append("fixture inventory mismatch; undeclared=" + ",".join(sorted(disk_fixtures - declared)) + "; missing=" + ",".join(sorted(declared - disk_fixtures)))
    planned_disk = {
        path.relative_to(fixture_root).as_posix()
        for path in (fixture_root / "planned").glob("*.json")
    }
    planned_declared = {entry.get("file") for entry in manifest.get("plannedFixtures", []) if isinstance(entry, dict)}
    if planned_disk != planned_declared:
        errors.append("planned fixture inventory differs from disk")

    coverage = manifest.get("fixtureEvaluationCoverage", [])
    covered = [name for group in coverage if isinstance(group, dict)
               for name in group.get("fixtures", []) if isinstance(name, str)]
    if len(covered) != len(set(covered)):
        errors.append("fixtureEvaluationCoverage contains duplicate fixture names")
    unknown = sorted(set(covered) - declared)
    if unknown:
        errors.append("fixtureEvaluationCoverage references unknown fixtures: " + ", ".join(unknown))
    coverage_expected = {
        item["file"] for item in fixtures
        if isinstance(item, dict) and isinstance(item.get("file"), str) and
        item.get("class") not in {"planned", "research-artifact", "source-compatibility-case-pair"}
    }
    if set(covered) != coverage_expected:
        errors.append("fixtureEvaluationCoverage must cover each declared graph fixture exactly once and exclude non-graph research artifacts")

    for planned in manifest.get("plannedFixtures", []):
        if not isinstance(planned, dict) or not isinstance(planned.get("file"), str):
            errors.append("planned fixture entry must declare file")
            continue
        path = (fixture_root / planned["file"]).resolve()
        try:
            path.relative_to(fixture_root.resolve())
        except ValueError:
            errors.append(f"planned fixture path escapes fixture directory: {planned['file']}")
            continue
        if not path.is_file():
            errors.append(f"missing planned fixture: {planned['file']}")
        elif hashlib.sha256(path.read_bytes()).hexdigest().upper() != planned.get("sha256", "").upper():
            errors.append(f"SHA-256 mismatch for planned fixture {planned['file']}")

    assets = manifest.get("assets", [])
    asset_data: dict[str, bytes] = {}
    for asset in assets:
        if not isinstance(asset, dict) or not isinstance(asset.get("file"), str):
            errors.append("asset entry must declare file")
            continue
        relative = asset["file"]
        path = (fixture_root / relative).resolve()
        try:
            path.relative_to(fixture_root.resolve())
        except ValueError:
            errors.append(f"asset path escapes fixture directory: {relative}")
            continue
        if not path.is_file():
            errors.append(f"missing asset: {relative}")
            continue
        data = path.read_bytes()
        asset_data[relative] = data
        digest = hashlib.sha256(data).hexdigest().upper()
        if asset.get("sha256", "").upper() != digest:
            errors.append(f"SHA-256 mismatch for asset {relative}")
        if asset.get("format") == "PNG RGBA8":
            try:
                pixels = decode_rgba8(data, asset["width"], asset["height"])
                golden_name = asset.get("pixelGolden")
                golden = json.loads((fixture_root / golden_name).read_text(encoding="utf-8"))
                expected = bytes(c for row in golden["pixels"] for pixel in row for c in pixel)
                if pixels != expected:
                    errors.append(f"decoded pixels differ from pixel golden: {relative}")
            except (OSError, KeyError, TypeError, ValueError, zlib.error, json.JSONDecodeError) as error:
                errors.append(f"cannot verify PNG/pixel golden {relative}: {error}")
        elif asset.get("format") == "JSON pixel bytes":
            try:
                golden = json.loads(data.decode("utf-8"))
                w, h = asset["width"], asset["height"]
                pixels = golden["pixels"]
                if golden["width"] != w or golden["height"] != h or len(pixels) != h or any(len(row) != w for row in pixels) or any(len(px) != 4 or any(not isinstance(c, int) or c < 0 or c > 255 for c in px) for row in pixels for px in row):
                    errors.append(f"invalid pixel-golden shape or values: {relative}")
            except (OSError, KeyError, TypeError, UnicodeError, json.JSONDecodeError) as error:
                errors.append(f"invalid pixel golden {relative}: {error}")
    texture_dir = fixture_root / "textures"
    disk_assets = {path.relative_to(fixture_root).as_posix() for path in texture_dir.rglob("*") if path.is_file()}
    if disk_assets != set(asset_data):
        errors.append("texture asset inventory differs from disk; undeclared=" + ",".join(sorted(disk_assets - set(asset_data))) + "; missing=" + ",".join(sorted(set(asset_data) - disk_assets)))

    apply_contract = manifest.get("applicationOutcomeContract", {})
    stage_results = apply_contract.get("proposedStageResults", [])
    expected_stage_names = ["document", "lowering", "backendCompile", "pipelineBuild", "effectiveMaterial"]
    if [item.get("stage") for item in stage_results if isinstance(item, dict)] != expected_stage_names:
        errors.append("application outcome contract must keep document, lowering, backend, pipeline, and effective-material stages separate")
    if any(not item.get("owner") or not item.get("statuses") for item in stage_results if isinstance(item, dict)):
        errors.append("application outcome stages must identify ownership and explicit statuses")
    required_apply_cases = {
        "parse-reject-preserves-document-and-effective-generation",
        "valid-document-lowering-failure-keeps-document-and-reports-diagnostic-policy",
        "backend-compile-failure-reports-effective-material-and-generation",
        "opaque-pso-failure-does-not-report-graph-active",
        "transparent-pso-failure-reports-opaque-fallback-policy",
        "same-document-repeat-returns-recorded-stage-results",
        "successful-recovery-advances-effective-generation",
        "clear-graph-restores-fixed-pbr-generation",
    }
    if set(apply_contract.get("requiredRuntimeCases", [])) != required_apply_cases:
        errors.append("application outcome contract runtime cases are incomplete or unexpected")
    for source_contract in apply_contract.get("sourceEvidence", []):
        if not isinstance(source_contract, dict) or not isinstance(source_contract.get("path"), str):
            errors.append("application outcome source evidence entry is malformed")
            continue
        try:
            source_text = (root / source_contract["path"]).read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"cannot read application outcome source {source_contract.get('path')}: {error}")
            continue
        for fragment in source_contract.get("fragments", []):
            if fragment not in source_text:
                errors.append(f"application outcome source evidence drift in {source_contract['path']}: {fragment}")
    if apply_contract.get("runtimeStatus") != "not-run-no-build-or-test-authorized":
        errors.append("application outcome runtime state must remain unverified until authorized execution")

    diagnostic = manifest.get("documentDiagnosticContract", {})
    promotion = diagnostic.get("promotionContract", {})
    codes = promotion.get("requiredDiagnosticCodes", [])
    if len(codes) != len(set(codes)) or not codes:
        errors.append("document diagnostic codes must be nonempty and unique")
    for path in promotion.get("diagnosticPathExamples", []):
        if not isinstance(path, str) or not path.startswith("/"):
            errors.append(f"diagnostic path must be a JSON Pointer: {path!r}")
    required_codes = {
        "json.parse.invalid", "document.kind.unsupported", "schema.version.unsupported",
        "schema.nodes.required", "schema.links.required", "node.id.empty",
        "node.id.duplicate", "node.kind.unknown", "node.parameter.domain", "link.endpoint.missing",
        "link.socket.missing", "link.direction.invalid", "link.type.incompatible",
        "link.single_writer.violation", "graph.cycle", "output.ambiguous",
        "graph.limit.exceeded",
    }
    if set(codes) != required_codes:
        errors.append("document diagnostic promotion codes differ from the required contract set")
    core_schema = manifest.get("coreDocumentSchemaProposal", {})
    document_validation = core_schema.get("documentValidationContract", {})
    limits_type = document_validation.get("limitsType", {})
    expected_limit_fields = {"maxJsonBytes": "uint64", "maxNodes": "uint64", "maxLinks": "uint64", "maxTraversalDepth": "uint64"}
    if (document_validation.get("status") != "proposal-only-source-contract-not-implemented"
            or limits_type.get("name") != "DocumentLimits"
            or limits_type.get("fields") != expected_limit_fields
            or "caller supplies one immutable limits value" not in limits_type.get("owner", "")
            or limits_type.get("productionDefaults") != "unselected; numeric production defaults remain null in resourceBounds.supportedLimits"):
        errors.append("document validation must use explicit caller-supplied limits with no selected production defaults")
    validation_order = document_validation.get("validationOrder", [])
    if not (len(validation_order) >= 7 and "before JSON parsing" in validation_order[1]
            and "before graph-index or adjacency allocation" in validation_order[3]
            and "iteratively" in validation_order[5]
            and "atomically" in validation_order[6]):
        errors.append("document limit validation ordering must guard parsing, graph allocation, depth traversal, and atomic commit")
    boundaries = document_validation.get("boundaryCaseTemplate", [])
    if {item.get("dimension") for item in boundaries} != {"jsonBytes", "nodes", "links", "traversalDepth"} or any(item.get("cases") != ["limit-minus-one", "exact-limit", "limit-plus-one"] for item in boundaries):
        errors.append("document limits require parameterized below/at/above boundary cases for all four dimensions")
    resource_bounds = manifest.get("measurements", {}).get("resourceBounds", {})
    if resource_bounds.get("status") != "no-production-limits-defined" or any(value is not None for value in resource_bounds.get("supportedLimits", {}).values()):
        errors.append("production document limits must remain unselected until runtime measurement")
    measurement_protocol = resource_bounds.get("measurementProtocol", {})
    record_schema = measurement_protocol.get("measurementRecordSchema", {})
    expected_stage_duration_fields = ["parseNs", "structuralValidationNs", "canonicalizationNs", "loweringNs"]
    if (record_schema.get("version") != 3
            or record_schema.get("caseIdFormat") != "<limitField>.<boundary>; stable and unique within one generated corpus"
            or record_schema.get("phaseValues") != ["cold-parse", "warm-validation"]
            or record_schema.get("stageDurationFieldsNs") != expected_stage_duration_fields
            or "retain every sample" not in record_schema.get("rawSamples", "")
            or "record null duration" not in record_schema.get("unreachedStage", "")
            or "must record true" not in record_schema.get("destinationUnchanged", "")):
        errors.append("resource-bound measurements must preserve per-stage cold/warm samples and explicit unavailable results")
    required_profiles = {"representative-small", "representative-medium", "adversarial-wide", "adversarial-deep", "malformed-boundary"}
    profile_ids = {item.get("profile") for item in measurement_protocol.get("profiles", []) if isinstance(item, dict)}
    required_measurements = {"serialized UTF-8 byte count including opaque extensions", "parse, structural validation, canonicalization, and lowering wall time separately", "peak live bytes, total allocation count, total allocated bytes, and largest single allocation"}
    if (measurement_protocol.get("status") != "planned-no-instrumentation-or-runtime-data"
            or profile_ids != required_profiles
            or not required_measurements <= set(measurement_protocol.get("measureEach", []))
            or "30 times" not in measurement_protocol.get("executionMethod", "")
            or "sampling bands, not selected limits" not in measurement_protocol.get("unverified", "")):
        errors.append("resource-bound measurement protocol must cover representative/adversarial shapes and keep profiles distinct from production limits")
    if "valid reachable unary chain" not in measurement_protocol.get("generatorCoverageLimit", "") or "disconnected valid Value nodes" not in measurement_protocol.get("generatorCoverageLimit", ""):
        errors.append("resource-bound generator coverage must specify scalable link and independent node-count shapes")
    limit_corpus_path = root / "tools/shader_graph_rd/generate_document_limit_corpus.py"
    try:
        corpus_spec = importlib.util.spec_from_file_location("shader_graph_document_limit_corpus", limit_corpus_path)
        if corpus_spec is None or corpus_spec.loader is None:
            raise RuntimeError("cannot load document limit corpus generator")
        corpus_module = importlib.util.module_from_spec(corpus_spec)
        corpus_spec.loader.exec_module(corpus_module)
        sample_limits = {"maxJsonBytes": 4096, "maxNodes": 64, "maxLinks": 6, "maxTraversalDepth": 2048}
        sample_corpus = corpus_module.build_case_set(sample_limits)
        corpus_errors = corpus_module.validate_case_set(sample_corpus)
        if corpus_errors:
            errors.extend("document limit corpus: " + item for item in corpus_errors)
        if sample_corpus.get("measurementRecordFields") != record_schema.get("requiredFields"):
            errors.append("generated document limit corpus record fields must match the manifest measurement schema")
        if sample_corpus.get("measurementRecordSemantics") != record_schema.get("recordSemantics"):
            errors.append("generated document limit corpus record semantics must match the manifest measurement schema")
        if sample_corpus.get("measurementSummarySchema") != measurement_protocol.get("measurementSummarySchema"):
            errors.append("generated document limit summary schema must match the manifest aggregation and boundary-decision contract")
        summary_path = root / "tools/shader_graph_rd/summarize_document_limit_measurements.py"
        summary_spec = importlib.util.spec_from_file_location("shader_graph_measurement_summary", summary_path)
        if summary_spec is None or summary_spec.loader is None:
            raise RuntimeError("cannot load document limit measurement summarizer")
        summary_module = importlib.util.module_from_spec(summary_spec)
        summary_spec.loader.exec_module(summary_module)
        summary_schema_errors = summary_module._validate_summary_schema(sample_corpus)
        if summary_schema_errors:
            errors.extend("measurement summary reducer schema: " + item for item in summary_schema_errors)
        summary_schema = measurement_protocol.get("measurementSummarySchema", {})
        expected_group_fields = summary_schema.get("groupBy", [])
        expected_metric_fields = summary_schema.get("percentiles", {}).get("fields", [])
        expected_stages = record_schema.get("recordSemantics", {}).get("rejectionStage", [])
        if list(summary_module.GROUP_FIELDS) != expected_group_fields:
            errors.append("measurement summarizer grouping keys must match the manifest summary schema")
        if list(summary_module.PERCENTILE_FIELDS) != expected_metric_fields:
            errors.append("measurement summarizer percentile fields must match the manifest summary schema")
        if [*summary_module.MEASUREMENT_STAGES, "unknown"] != expected_stages:
            errors.append("measurement summarizer rejection stages must match the measurement record schema")
        if set(summary_module.REQUIRED_CONTEXT_FIELDS) != set(expected_group_fields) - {"runId", "caseId", "phase"}:
            errors.append("measurement summarizer report context must match the manifest grouping keys")
        sample_case_ids = [case.get("caseId") for case in sample_corpus.get("cases", [])]
        if len(sample_case_ids) != 12 or len(sample_case_ids) != len(set(sample_case_ids)):
            errors.append("generated document limit cases must have 12 unique stable IDs for measurement records")
    except (OSError, ImportError, AttributeError, TypeError, ValueError, RuntimeError) as error:
        errors.append(f"cannot validate deterministic document limit corpus generator: {error}")
    socket_types = core_schema.get("legacySocketTypeCharacterization", {})
    if not socket_types or manifest.get("coreDocumentSchemaProposal", {}).get("candidateLimits", {}).get("colorRampStops") != 32:
        errors.append("legacy socket types or explicit ramp boundary candidate are missing")
    legacy_node_kinds = core_schema.get("legacyNodeKindMapping", {})
    legacy_parameters = core_schema.get("legacyParameterMapping", {})
    fixture_node_kinds = {node.get("type") for payload in fixture_payloads.values() for node in payload.get("nodes", []) if isinstance(node, dict)}
    if fixture_node_kinds - set(legacy_node_kinds):
        errors.append("Core node-kind mapping omits fixture node kinds: " + ", ".join(sorted(fixture_node_kinds - set(legacy_node_kinds))))
    if any(not re.fullmatch(r"core(?:\.[a-z][a-z0-9_]*)+", kind_id) for kind_id in legacy_node_kinds.values()) or len(set(legacy_node_kinds.values())) != len(legacy_node_kinds):
        errors.append("Core node-kind IDs must be unique stable schema identifiers")
    for fixture_name, payload in fixture_payloads.items():
        for node in payload.get("nodes", []):
            if not isinstance(node, dict):
                continue
            node_type = node.get("type")
            params = node.get("params", {})
            mapped = legacy_parameters.get(node_type, {})
            unmapped = set(params) - set(mapped)
            if unmapped:
                errors.append(f"legacy parameter mapping omits {fixture_name} {node_type} fields: {', '.join(sorted(unmapped))}")
            for field, value in params.items():
                field_mapping = mapped.get(field, {})
                enum_values = field_mapping.get("values")
                if isinstance(enum_values, dict) and str(value) not in enum_values and fixture_name != "out_of_range_enum_negative.json":
                    errors.append(f"unmapped enum value in {fixture_name}: {node_type}.{field}={value}")
                if field_mapping.get("type") == "FiniteFloat32":
                    try:
                        number = float(value)
                        packed = struct.pack(">f", number)
                        if not math.isfinite(number) or not math.isfinite(struct.unpack(">f", packed)[0]):
                            raise ValueError("outside finite binary32 range")
                    except (TypeError, ValueError, OverflowError, struct.error):
                        if fixture_name != "float_overflow_negative.json":
                            errors.append(f"unrepresentable finite Float32 in {fixture_name}: {node_type}.{field}")
    default_fixture = fixture_payloads.get("node_parameter_defaults_legacy.json", {})
    default_nodes = default_fixture.get("nodes", [])
    default_node_types = {node.get("type") for node in default_nodes if isinstance(node, dict)}
    default_table = core_schema.get("legacyParameterDefaults", {})
    if default_node_types != {"Value", "RGB", "Math", "MixRGB", "ColorRamp", "ImageTexture", "Principled", "MaterialOutput"} or any(node.get("params") != {} for node in default_nodes if isinstance(node, dict)):
        errors.append("omitted-parameter fixture must cover each mapped default with empty params")
    if default_node_types - set(default_table):
        errors.append("legacy parameter default table omits fixture node kinds")
    expected_default_table = {
        "Value": {"value": 0.0},
        "RGB": {"r": 0.5, "g": 0.5, "b": 0.5, "a": 1.0},
        "Math": {"op": "add", "clamp": False},
        "MixRGB": {"blend": "mix", "clampFactor": True, "clampResult": False},
        "ColorRamp": {"interp": "linear", "stops": [{"pos": 0.0, "r": 0.0, "g": 0.0, "b": 0.0, "a": 1.0}, {"pos": 1.0, "r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}]},
        "Voronoi": {},
        "ImageTexture": {"slot": "base_color"},
        "Principled": {}, "MaterialOutput": {}, "TexCoord": {}, "Noise": {},
    }
    if default_table != expected_default_table:
        errors.append("legacy effective default table differs from the reviewed source-derived contract")
    omitted_policy = core_schema.get("omittedParameterPolicy", "")
    if not all(term in omitted_policy for term in ("materialize", "opaqueExtensions", "malformed")):
        errors.append("omitted parameter policy must define default materialization, opaque extras, and malformed-value handling")
    compatibility_policy = core_schema.get("legacyParameterCompatibilityPolicy", {})
    compatibility_expected = {
        "status": "source-audited-legacy-behavior-not-core-validation-policy",
        "unknownParameterKeys": "legacy loader ignores keys not consumed by matReadParams; proposed adapter must preserve opaque extensions or report loss before Core promotion",
        "nonFiniteScalarValues": "matFiniteDouble falls back to the constructor value before float conversion",
        "enumValues": "Math, MixRGB, ColorRamp, ImageTexture, and MapRange integer values are clamped to each enum's valid range",
        "colorRampStops": "empty stop arrays leave constructor defaults; populated arrays clamp stop position to [0,1] at lowering, replace non-finite channels with per-channel defaults, and loader truncates after 64 entries",
        "linkResolution": "legacy lowering resolves the first matching link encountered for an input; input cardinality and incompatible links are not validated atomically",
        "materialOutputSelection": "the first stored MaterialOutput node is selected; extra outputs are ignored by legacy lowering",
        "runtimeStatus": "source-inspection-only-no-loader-contract-test-executed",
    }
    if compatibility_policy != compatibility_expected:
        errors.append("legacy parameter/link/output compatibility policy differs from the source-audited contract")
    compatibility_fragments = (
        'std::clamp(p.value(QStringLiteral("op")).toInt(0), 0, 16)',
        'std::clamp(p.value(QStringLiteral("blend")).toInt(0), 0, 7)',
        'if (v->stops.size() >= 64)',
        'if (l->toPin == inPin.get())',
        'MaterialOutputNode* root = outputs.front();',
    )
    compatibility_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
    if any(fragment not in compatibility_source for fragment in compatibility_fragments):
        errors.append("legacy compatibility policy no longer matches loader/link/output source fragments")
    math_semantics = core_schema.get("mathOperationContracts", {})
    math_semantics_expected = {
        "executionShape": "all operations consume and emit float4; scalar values are explicitly replicated or components are truncated/appended by legacy matConvert",
        "divide": "each denominator component is max(B, 1e-5); negative small denominators are replaced with positive 1e-5",
        "power": "each base component is max(A, 0); exponent B is unmodified",
        "modulo": "floored modulo A - B*floor(A/B); each divisor is max(B, 1e-5), including negative divisors",
        "mix": "lerp(A,B,saturate(Fac)) regardless of clampResult; clampResult then saturates the full float4 result",
        "comparisons": "LessThan and GreaterThan compare components independently and emit exact 0.0 or 1.0 per component",
        "unaryDefaults": "unconnected A uses float4(0.5,0.5,0.5,0.5); unused B does not contribute",
        "binaryDefaults": "Add/Subtract/Minimum/Maximum B=0; Multiply B=1; Divide B=1 before denominator floor; Power B=1; Modulo B=1",
        "runtimeStatus": "source-inspection-only-existing-contract-tests-not-run",
    }
    if math_semantics != math_semantics_expected:
        errors.append("source-audited Math execution-shape and numeric-boundary proposal differs")
    diagnostic_goldens = promotion.get("diagnosticGoldens", [])
    golden_fixtures: set[str] = set()
    for golden_case in diagnostic_goldens:
        if not isinstance(golden_case, dict) or not isinstance(golden_case.get("fixture"), str):
            errors.append("diagnostic golden must declare a fixture")
            continue
        fixture_name = golden_case["fixture"]
        golden_fixtures.add(fixture_name)
        try:
            fixture_data = json.loads((fixture_root / fixture_name).read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            errors.append(f"cannot load diagnostic golden fixture {fixture_name}: {error}")
            continue
        expected = golden_case.get("expected")
        if not isinstance(expected, list) or not expected:
            errors.append(f"diagnostic golden requires nonempty expected list: {fixture_name}")
            continue
        for item in expected:
            if not isinstance(item, dict) or item.get("severity") != "error" or not isinstance(item.get("code"), str) or not isinstance(item.get("path"), str) or not item["path"].startswith("/"):
                errors.append(f"invalid diagnostic record in golden: {fixture_name}")
                continue
            path_tokens = [token.replace("~1", "/").replace("~0", "~") for token in item["path"].lstrip("/").split("/")]
            cursor = fixture_data
            found = True
            try:
                for token in path_tokens:
                    cursor = cursor[int(token)] if isinstance(cursor, list) else cursor[token]
            except (KeyError, IndexError, TypeError, ValueError):
                found = (
                    fixture_name == "missing_arrays_negative.json"
                    and len(path_tokens) == 1
                    and path_tokens[0] in {"nodes", "links"}
                    and path_tokens[0] not in fixture_data
                )
            if fixture_name == "wrong_kind_negative.json" and item["code"] == "document.kind.unsupported":
                found &= cursor != manifest.get("application", {}).get("graphFormat")
            elif fixture_name == "missing_arrays_negative.json":
                found &= item["code"] == f"schema.{path_tokens[-1]}.required" and path_tokens[-1] not in fixture_data
            elif fixture_name in {"skipped_type_mismatch_link_negative.json", "duplicate_input_negative.json"}:
                if path_tokens[:1] == ["links"] and len(path_tokens) >= 3:
                    link_index = int(path_tokens[1])
                    link = fixture_data["links"][link_index]
                    found &= cursor == link.get(path_tokens[2])
                    if "nodeIds" in item:
                        found &= set(item["nodeIds"]) == {link.get("from"), link.get("to")}
                    nodes_by_id = {node["id"]: node for node in fixture_data["nodes"]}
                    source_node = nodes_by_id[link["from"]]
                    target_node = nodes_by_id[link["to"]]
                    source_socket_type = socket_types.get(source_node["type"], {}).get("outputs", {}).get(link["out"])
                    target_socket_type = socket_types.get(target_node["type"], {}).get("inputs", {}).get(link["in"])
                    if item["code"] == "link.type.incompatible":
                        found &= source_socket_type is not None and target_socket_type is not None and source_socket_type != target_socket_type
                    if item["code"] == "link.single_writer.violation":
                        writers = [candidate for candidate in fixture_data["links"] if candidate.get("to") == link.get("to") and candidate.get("in") == link.get("in")]
                        found &= len(writers) > 1 and source_socket_type == target_socket_type
            elif fixture_name in {"empty_ramp_stops_negative.json", "oversized_ramp_stops_negative.json"}:
                stops = cursor
                found &= isinstance(stops, list)
                if fixture_name == "empty_ramp_stops_negative.json":
                    found &= len(stops) == 0 and item["code"] == "node.parameter.domain"
                else:
                    found &= len(stops) > manifest.get("coreDocumentSchemaProposal", {}).get("candidateLimits", {}).get("colorRampStops", 0) and item["code"] == "graph.limit.exceeded"
                found &= item.get("nodeIds") == ["ramp"]
            if not found:
                errors.append(f"diagnostic code/path golden does not match fixture structure: {fixture_name} {item.get('path')}")
        records = golden_case.get("expected", [])
        order_keys = [
            (item.get("path", "").encode("utf-8"), item.get("code", "").encode("utf-8"),
             (item.get("nodeIds", [""]) + [""])[0].encode("utf-8"),
             (item.get("nodeIds", [""]) + [""])[1].encode("utf-8"))
            for item in records if isinstance(item, dict)
        ]
        if order_keys != sorted(order_keys):
            errors.append(f"diagnostic golden entries are not in stable sort order: {fixture_name}")
    required_golden_fixtures = {"wrong_kind_negative.json", "missing_arrays_negative.json", "skipped_type_mismatch_link_negative.json", "duplicate_input_negative.json", "empty_ramp_stops_negative.json", "oversized_ramp_stops_negative.json"}
    if golden_fixtures != required_golden_fixtures:
        errors.append("diagnostic golden fixture coverage is incomplete or has unexpected entries")
    if promotion.get("diagnosticOrdering") != "JSON Pointer, code, node id, socket id bytewise ascending":
        errors.append("diagnostic ordering rule is missing or changed")
    required_document_cases = {
        "malformed-payload-preserves-existing-document", "wrong-kind-preserves-existing-document",
        "missing-arrays-preserve-existing-document", "type-mismatch-link-rejects-entire-document",
        "duplicate-writer-link-rejects-entire-document",
        "diagnostic-code-and-json-pointer-identify-failure-location",
        "multiple-missing-fields-return-stably-sorted-diagnostics",
    }
    if set(promotion.get("requiredRuntimeCases", [])) != required_document_cases:
        errors.append("document diagnostic promotion runtime cases are incomplete or unexpected")
    if promotion.get("diagnosticAggregation") != "report every independent top-level schema requirement failure in one validation result; suppress cascaded node/link diagnostics when their parent collection is missing; wrong-kind does not suppress independent version/required-collection checks":
        errors.append("diagnostic aggregation behavior must be explicitly defined")
    missing_arrays_cases = [case for case in diagnostic_goldens if isinstance(case, dict) and case.get("fixture") == "missing_arrays_negative.json"]
    if len(missing_arrays_cases) != 1 or [item.get("path") for item in missing_arrays_cases[0].get("expected", [])] != ["/links", "/nodes"]:
        errors.append("missing arrays diagnostic golden must report both independent fields in stable order")
    current_impl = diagnostic.get("currentImplementation", {})
    if not all(current_impl.get(field) for field in ("resultShape", "documentKind", "missingNodeOrLinkArrays", "invalidLink", "diagnosticCodeAndPath")):
        errors.append("current document-loader characterization is incomplete")
    if (current_impl.get("missingNodeOrLinkArrays") !=
            "nodes must exist and be an array; links are not required and missing or non-array links become an empty QJsonArray through toArray()" or
            current_impl.get("invalidLink") !=
            "unresolved endpoints, pins, or type mismatches are counted; any skipped link sets a human-readable error and returns with ok=false before staged graph commit"):
        errors.append("current JSON loader characterization must distinguish required nodes from optional links and reject skipped-link loads atomically")
    if not promotion.get("rejectPartialDocument") or not promotion.get("preservePreviouslyLoadedGraphOnFailure"):
        errors.append("document promotion contract must require atomic reject and preservation")

    if core_schema.get("schemaId") != "artifact.material-graph.document" or core_schema.get("schemaVersion") != 1:
        errors.append("Core document schema proposal identity/version is invalid")
    if "opaqueExtensions" not in core_schema.get("nodeRecordFields", []) or "opaqueExtensions" not in core_schema.get("linkRecordFields", []) or core_schema.get("documentExtensionField") != "opaqueExtensions":
        errors.append("Core Document must provide opaque extension storage at document, node, and link scopes")
    if core_schema.get("status") != "proposal-only-not-implemented" or core_schema.get("runtimeStatus") != "not-implemented-not-runtime-verified":
        errors.append("Core document schema proposal must remain explicitly unimplemented and unverified")
    promotion_sequence = manifest.get("corePromotionSequence", [])
    phase_names = [phase.get("name") for phase in promotion_sequence if isinstance(phase, dict)]
    if [phase.get("phase") for phase in promotion_sequence if isinstance(phase, dict)] != [1, 2, 3, 4] or phase_names != ["DocumentAndValidator", "ArtifactSchemaAdapter", "TypedMaterialIR", "RendererBackendAdapter"]:
        errors.append("Core promotion sequence must preserve the four ordered migration slices")
    for phase in promotion_sequence:
        if not isinstance(phase, dict) or not phase.get("scope") or not phase.get("targetBoundary") or not phase.get("prerequisites") or not phase.get("acceptanceEvidence") or not phase.get("compatibilityPlan") or not phase.get("rollback") or not phase.get("status"):
            errors.append("Core promotion phase lacks scope, boundaries, gates, compatibility, rollback, or status")
    if promotion_sequence and promotion_sequence[0].get("status") != "proposal-only-runtime-gates-open":
        errors.append("Document/Validator phase cannot be marked accepted before runtime gates pass")
    migration_cases = manifest.get("legacyMigrationCases", [])
    migration_by_fixture = {case.get("fixture"): case for case in migration_cases if isinstance(case, dict)}
    required_migration_fixtures = {
        "node_parameter_defaults_legacy.json", "empty_ramp_stops_negative.json", "unknown_extension_legacy.json", "principled_default.json", "wrong_kind_negative.json", "missing_arrays_negative.json",
        "skipped_type_mismatch_link_negative.json", "out_of_range_enum_negative.json",
        "missing_output_compile_failure.json", "planned/separate_color_packed_mr.json",
    }
    if set(migration_by_fixture) != required_migration_fixtures or len(migration_cases) != len(migration_by_fixture):
        errors.append("legacy migration case inventory is incomplete or duplicated")
    for fixture_name, case in migration_by_fixture.items():
        if fixture_name not in declared | planned_declared or not all(case.get(key) for key in ("sourceBehavior", "adapterPolicy", "lossPrevention", "expected")):
            errors.append(f"legacy migration case is incomplete or references unknown fixture: {fixture_name}")
    expected_migration_actions = {
        "node_parameter_defaults_legacy.json": "migrate-with-explicit-effective-defaults",
        "empty_ramp_stops_negative.json": "reject-and-retain-source",
        "unknown_extension_legacy.json": "migrate-known-semantics-preserve-opaque-extension",
        "principled_default.json": "lossless-semantic-migration",
        "wrong_kind_negative.json": "reject-and-retain-source",
        "missing_arrays_negative.json": "reject-and-retain-source",
        "skipped_type_mismatch_link_negative.json": "reject-and-retain-source",
        "out_of_range_enum_negative.json": "reject-and-retain-source",
        "planned/separate_color_packed_mr.json": "reject-and-retain-source",
        "missing_output_compile_failure.json": "preserve-document-report-lowering-diagnostic",
    }
    if {name: case.get("expected") for name, case in migration_by_fixture.items()} != expected_migration_actions:
        errors.append("legacy migration outcomes changed without updating the reviewed policy")
    expected_rules = {
        "nodes": "sort by stable node id",
        "links": "sort by fromNodeId, fromSocketId, toNodeId, toSocketId",
        "layout": "preserved in canonical document but excluded from semantic projection and semantic hash",
        "diagnostics": "sort by JSON Pointer, code, node id, socket id",
    }
    canonical = core_schema.get("canonicalizationRules", {})
    for key, expected in expected_rules.items():
        if canonical.get(key) != expected:
            errors.append(f"Core document canonicalization rule is missing or changed: {key}")
    if core_schema.get("semanticProjectionExcludes") != ["layout.x", "layout.y"]:
        errors.append("Core semantic projection must exclude node layout coordinates")
    socket_identity = core_schema.get("socketIdentityProposal", {})
    stable_socket_ids = socket_identity.get("legacyLabelToStableId", {})
    expected_socket_keys = {
        f"{node_kind}.{label}"
        for node_kind, node_pins in socket_types.items()
        for direction in ("inputs", "outputs")
        for label in node_pins.get(direction, {})
    }
    if socket_identity.get("displayLabelIsIdentity") is not False or set(stable_socket_ids) != expected_socket_keys:
        errors.append("stable socket identity proposal must separate IDs from display labels")
    seen_socket_ids: set[str] = set()
    legacy_types = core_schema.get("legacySocketTypeCharacterization", {})
    for legacy_key, stable_id in stable_socket_ids.items():
        try:
            node_kind, socket_label = legacy_key.split(".", 1)
            stable_parts = stable_id.split(".")
            direction = stable_parts[-2] if len(stable_parts) >= 3 else None
            pin_direction = "inputs" if direction == "in" else "outputs" if direction == "out" else None
            declared_type = legacy_types[node_kind][pin_direction][socket_label]
            if not pin_direction or not re.fullmatch(r"[a-z][a-z0-9_.]*", stable_id):
                raise ValueError("stable socket ID does not follow the declared lowercase naming convention")
            if stable_id in seen_socket_ids:
                raise ValueError("stable socket ID is duplicated")
            seen_socket_ids.add(stable_id)
            if declared_type not in {"Float", "Vector2", "Vector3", "Vector4", "Shader"}:
                raise ValueError("unknown legacy socket type")
        except (KeyError, ValueError) as error:
            errors.append(f"invalid legacy-to-stable socket identity {legacy_key}: {error}")
    if "direct links only" not in socket_identity.get("legacyConversionPolicy", ""):
        errors.append("socket conversion policy must reject implicit numeric link coercion")
    operation_schema = manifest.get("coreTypedOperationSchemaProposal", {})
    operation_records = operation_schema.get("exampleOperationRecords", [])
    expected_conversion_opcodes = {
        (source_type, target_type): opcode
        for source_type, target_type, opcode in (
            ("Float", "Vector2", "convert.scalar.replicate.2"),
            ("Float", "Vector3", "convert.scalar.replicate.3"),
            ("Float", "Vector4", "convert.scalar.replicate.rgb_alpha_one"),
            ("Vector2", "Float", "convert.vector2.extract_x"),
            ("Vector2", "Vector3", "convert.vector2.append_z_zero"),
            ("Vector2", "Vector4", "convert.vector2.append_z_zero_alpha_one"),
            ("Vector3", "Float", "convert.vector3.extract_x"),
            ("Vector3", "Vector2", "convert.vector3.truncate_xy"),
            ("Vector3", "Vector4", "convert.vector3.append_alpha_one"),
            ("Vector4", "Float", "convert.vector4.extract_x"),
            ("Vector4", "Vector2", "convert.vector4.truncate_xy"),
            ("Vector4", "Vector3", "convert.vector4.truncate_xyz"),
        )
    }
    conversion_matrix = socket_identity.get("legacyConversionMatrix", [])
    opcode_to_operation_id = {
        opcode: f"artifact.{opcode}"
        for opcode in expected_conversion_opcodes.values()
    }
    actual_conversion_opcodes = {
        (item.get("from"), item.get("to")): item.get("proposedOpcode")
        for item in conversion_matrix if isinstance(item, dict)
    }
    if (len(conversion_matrix) != 12 or len(actual_conversion_opcodes) != 12 or
            actual_conversion_opcodes != expected_conversion_opcodes):
        errors.append("legacy numeric conversion matrix must enumerate the 12 non-identity Float/Vector2/Vector3/Vector4 pairs with explicit typed opcodes")
    float_to_vector4 = next((item for item in conversion_matrix if item.get("from") == "Float" and item.get("to") == "Vector4"), {})
    operation_record_by_id = {record.get("operationId"): record for record in operation_records if isinstance(record, dict)}
    for item in conversion_matrix:
        if not isinstance(item, dict):
            continue
        expected_id = opcode_to_operation_id.get(item.get("proposedOpcode"))
        if item.get("operationId") != expected_id:
            errors.append(f"legacy conversion row must name its explicit operation record: {item.get('from')} -> {item.get('to')}")
            continue
        record = operation_record_by_id.get(expected_id, {})
        source_type = item.get("from")
        target_type = item.get("to")
        expected_output_id = "value" if target_type == "Float" else "vector"
        if ([(pin.get("inputId"), pin.get("valueType")) for pin in record.get("inputs", [])] != [("value", source_type)] or
                [(pin.get("outputId"), pin.get("valueType")) for pin in record.get("outputs", [])] != [(expected_output_id, target_type)] or
                record.get("evidenceStatus") != "source-audited-cpu-formula-candidate"):
            errors.append(f"legacy conversion operation record types/evidence do not match matrix row: {source_type} -> {target_type}")
    test_source_path = root / "tests/ArtifactCore/ShaderGraphContractTest.cpp"
    try:
        conversion_test_source = test_source_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read legacy conversion contract source: {error}")
        conversion_test_source = ""
    for item in conversion_matrix:
        if not isinstance(item, dict) or not isinstance(item.get("currentHlslEvidence"), str):
            errors.append("each legacy conversion matrix row needs current HLSL evidence")
            continue
        if item["currentHlslEvidence"] not in conversion_test_source:
            errors.append(f"legacy conversion evidence is missing from contract source: {item['from']} -> {item['to']}")
        try:
            probe_input = item["numericProbeInput"]
            expected_value = item["expectedSemanticValue"]
            source_components = {"Float": 1, "Vector2": 2, "Vector3": 3, "Vector4": 4}[item["from"]]
            target_components = {"Float": 1, "Vector2": 2, "Vector3": 3, "Vector4": 4}[item["to"]]
            input_values = [probe_input] if source_components == 1 else probe_input
            expected_values = [expected_value] if target_components == 1 else expected_value
            if not isinstance(input_values, list) or len(input_values) != source_components or any(
                    isinstance(component, bool) or not isinstance(component, (int, float)) or not math.isfinite(float(component))
                    for component in input_values):
                raise ValueError("numeric probe input has the wrong type, size, or domain")
            if not isinstance(expected_values, list) or len(expected_values) != target_components:
                raise ValueError("expected semantic result has the wrong type or size")
            if any(isinstance(component, bool) or not isinstance(component, (int, float)) or not math.isfinite(float(component))
                   for component in expected_values):
                raise ValueError("expected semantic result contains a non-numeric or non-finite component")
            actual_value = apply_legacy_conversion_probe(item["proposedOpcode"], probe_input)
            if actual_value != expected_value:
                errors.append(f"legacy conversion semantic golden mismatch: {item['from']} -> {item['to']}")
        except (KeyError, TypeError, ValueError) as error:
            errors.append(f"invalid legacy conversion semantic probe: {error}")
    entry_path_cases_path = fixture_root / "entry_path_compatibility_cases.json"
    try:
        entry_path_cases = json.loads(entry_path_cases_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        errors.append(f"cannot read entry-path compatibility cases: {error}")
        entry_path_cases = {}
    expected_entry_path_cases = {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-entry-path-compatibility-cases",
        "status": "source-derived-observations-no-runtime-execution",
        "promotionRequirement": "adapter must choose and test one explicit policy for each case; source-derived observation is not an approved product policy",
    }
    case_ids = [item.get("caseId") for item in entry_path_cases.get("cases", []) if isinstance(item, dict)]
    duplicate_case = next((item for item in entry_path_cases.get("cases", []) if item.get("caseId") == "single-writer-duplicate-input"), {})
    mismatch_case = next((item for item in entry_path_cases.get("cases", []) if item.get("caseId") == "numeric-type-mismatch"), {})
    apply_failure_case = next((item for item in entry_path_cases.get("cases", []) if item.get("caseId") == "invalid-graph-stored-before-compile-result"), {})
    if (any(entry_path_cases.get(key) != value for key, value in expected_entry_path_cases.items()) or
            len(case_ids) != 3 or set(case_ids) != {"single-writer-duplicate-input", "numeric-type-mismatch", "invalid-graph-stored-before-compile-result"} or
            duplicate_case.get("sourceFixture") != "duplicate_input_negative.json" or
            duplicate_case.get("jsonImport", {}).get("linkCountAfterLoad") != 3 or
            duplicate_case.get("jsonImport", {}).get("expectedSelectedSourceByCurrentSource") != "red" or
            duplicate_case.get("directApi", {}).get("compileValidationRejectsMultipleWriters") is not False or
            mismatch_case.get("sourceFixture") != "skipped_type_mismatch_link_negative.json" or
            mismatch_case.get("jsonImport", {}).get("linkCountAfterLoad") != 1 or
            mismatch_case.get("directApi", {}).get("expectedBaseColorExpression") != "float4(scalar_Value,scalar_Value,scalar_Value,1.0f)" or
            apply_failure_case.get("sourceFixture") != "cycle_negative.json" or
            apply_failure_case.get("layerMutation", {}).get("controllerReturn") is not True or
            apply_failure_case.get("rendererBehavior", {}).get("compileFailure") != "compileMaterialGraph returns ok=false with a magenta fallback block" or
            apply_failure_case.get("rendererBehavior", {}).get("setMaterialGraphJsonReturn") is not True or
            apply_failure_case.get("rendererBehavior", {}).get("storesJson") is not True):
        errors.append("paired JSON-import/direct-API compatibility observations are incomplete or contradictory")
    expected_observation_sources = (
        'if (!fromPin || !toPin || fromPin->type != toPin->type)',
        'if (l->toPin == inPin.get())',
        'MaterialOutputNode* root = outputs.front();',
        'float4({0},{0},{0},1.0f)',
        'pImpl_->materialGraphJson_ = json;',
        'setMaterialGraph(compiled.helperHlsl, compiled.blockHlsl);',
        'return true;',
        'modelLayer->setMaterialGraphJson(',
        'if (!probe.ok)',
        'float4 graphBaseColor = float4(1.0f,0.0f,1.0f,1.0f);',
    )
    observation_sources = "\n".join((root / relative).read_text(encoding="utf-8") for relative in (
        "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp",
        "ArtifactCore/src/Graphics/MeshRenderer.cpp",
        "Artifact/src/Widgets/ArtifactShaderGraphWidget.cpp",
        "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp",
    ))
    if any(fragment not in observation_sources for fragment in expected_observation_sources):
        errors.append("paired entry-path observations no longer match the loader and lowering source")
    expected_procedural_algorithms = {
        "Noise": "artifact.noise.value_fbm4.1",
        "Voronoi": "artifact.voronoi.f1_2d_3x3.1",
        "ColorRamp": "artifact.color_ramp.linear_constant_ease.1",
    }
    procedural_contracts = manifest.get("proceduralAlgorithmContracts", [])
    procedural_by_node = {
        item.get("legacyNode"): item for item in procedural_contracts
        if isinstance(item, dict)
    }
    if (len(procedural_contracts) != 3 or set(procedural_by_node) != set(expected_procedural_algorithms) or
            {node: item.get("algorithmId") for node, item in procedural_by_node.items()} != expected_procedural_algorithms):
        errors.append("procedural algorithm contracts must pin one versioned Noise, Voronoi, and ColorRamp algorithm")
    math_contract = manifest.get("mathOperationContracts", {})
    math_enum = legacy_parameters.get("Math", {}).get("op", {}).get("values", {})
    expected_math_ops = {
        "0": "add", "1": "subtract", "2": "multiply", "3": "divide", "4": "mix",
        "5": "sine", "6": "cosine", "7": "power", "8": "minimum", "9": "maximum",
        "10": "less_than", "11": "greater_than", "12": "modulo", "13": "absolute",
        "14": "floor", "15": "ceil", "16": "fract",
    }
    math_fixture_by_operation = {name: f"math_{name}.json" for name in expected_math_ops.values()}
    if len(fixtures) < 17 or not set(math_fixture_by_operation.values()) <= declared:
        errors.append("each of the 17 Math enum operations requires a declared positive graph fixture")
    for ordinal, operation in expected_math_ops.items():
        fixture_name = math_fixture_by_operation[operation]
        payload = fixture_payloads.get(fixture_name, {})
        math_nodes = [node for node in payload.get("nodes", []) if isinstance(node, dict) and node.get("type") == "Math"]
        if (len(math_nodes) != 1 or math_nodes[0].get("id") != "math" or
                math_nodes[0].get("params", {}).get("op") != int(ordinal) or
                math_nodes[0].get("params", {}).get("clamp") is not False):
            errors.append(f"Math operation fixture does not pin explicit ordinal/clamp: {fixture_name}")
        links = {(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in payload.get("links", []) if isinstance(link, dict)}
        if not {("math", "Value", "surface", "BaseColor"), ("surface", "BSDF", "output", "Surface")} <= links:
            errors.append(f"Math operation fixture must connect its value to Principled BaseColor and preserve MaterialOutput: {fixture_name}")
    expected_math_defaults = {
        "add": {"B": [0.0] * 4}, "subtract": {"B": [0.0] * 4},
        "multiply": {"B": [1.0] * 4}, "divide": {"B": [1.0] * 4},
        "mix": {"B": [0.0] * 4, "Fac": 0.5},
        "sine": {}, "cosine": {}, "power": {"B": [1.0] * 4},
        "minimum": {"B": [0.0] * 4}, "maximum": {"B": [0.0] * 4},
        "less_than": {"B": [0.0] * 4}, "greater_than": {"B": [0.0] * 4},
        "modulo": {"B": [1.0] * 4},
        "absolute": {}, "floor": {}, "ceil": {}, "fract": {},
    }
    expected_math_pins = {"inputs": {"A": "Vector4", "B": "Vector4", "Fac": "Float"}, "outputs": {"Value": "Vector4"}}
    expected_semantic_operands = {
        "binary": ["add", "subtract", "multiply", "divide", "power", "minimum", "maximum", "less_than", "greater_than", "modulo"],
        "unary": ["sine", "cosine", "absolute", "floor", "ceil", "fract"],
        "ternary": ["mix"],
    }
    expected_unused_inputs = {"unary": ["B", "Fac"], "binary": ["Fac"], "mix": []}
    expected_unused_input_behavior = {
        "operationOperandBehavior": "lowering does not request semantically unused B/Fac values for unary/binary operations, so they do not enter that Math expression",
        "graphTraversalBehavior": "material lowering depth-first traversal follows every declared input link, including semantically unused Math inputs; linked source nodes can still be emitted into the HLSL block",
        "diagnosticBehavior": "no source-audited unused-input diagnostic or link removal",
        "indirectRisk": "unused linked source nodes may still affect generated source through identifier collisions or other source-generation side effects; runtime impact is unverified",
        "sourceFragments": [
            "for (const auto& inPin : n->inputs)", "const Pin* src = getConnectedInput(inPin.get())",
            "for (const auto& inPin : node->inputs)", "if (l->toPin == inPin.get())",
            "return {l->fromPin->owner, l->fromPin};", "const std::string a = resolver.valueExpr(",
            "\"A\", PinType::Vector4,", "const auto twoOp =",
            "resolver.valueExpr(node, \"B\", PinType::Vector4, dfltB)",
            "PinType::Float, \"0.5f\"",
        ],
        "runtimeStatus": "source-audited-no-unused-input-compile-or-runtime-probe",
    }
    expected_math_semantics = {
        "divide": "componentwise A / max(B, 1e-5); negative denominators are also replaced by positive epsilon",
        "mix": "componentwise lerp(A, B, saturate(Fac))",
        "power": "componentwise pow(max(A, 0), B); negative bases clamp to zero",
        "less_than": "componentwise A < B yields 1.0 or 0.0",
        "greater_than": "componentwise A > B yields 1.0 or 0.0",
        "modulo": "componentwise A - max(B, 1e-5) * floor(A / max(B, 1e-5)); negative B also becomes positive epsilon",
        "clamp": "when enabled, saturate each output component after the operation",
    }
    math_source_fragments = math_contract.get("sourceEvidence", [])
    try:
        math_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read Math operation source contract: {error}")
        math_source = ""
    if (math_contract.get("legacyNode") != "Math" or math_contract.get("semanticIdPrefix") != "artifact.math.vectorized" or
            math_contract.get("componentType") != "Vector4" or math_enum != expected_math_ops or
            math_contract.get("inputDefaultsByOperation") != expected_math_defaults or
            math_contract.get("declaredPins") != expected_math_pins or
            math_contract.get("semanticOperands") != expected_semantic_operands or
            math_contract.get("unusedDeclaredInputs") != expected_unused_inputs or
            math_contract.get("unusedInputBehavior") != expected_unused_input_behavior or
            math_contract.get("defaultValues") != {"A": [0.5] * 4, "BByOperand": {"zero": [0.0] * 4, "one": [1.0] * 4}, "Fac": 0.5} or
            math_contract.get("operationSemantics") != expected_math_semantics or
            math_contract.get("status") != "source-audited-numeric-boundary-probes-and-cross-backend-goldens-open" or
            not math_source_fragments or any(fragment not in math_source for fragment in math_source_fragments)):
        errors.append("Math operation semantics/defaults must be pinned to every enum and current source evidence, with runtime gates open")
    elif any(fragment not in math_source for fragment in expected_unused_input_behavior["sourceFragments"]):
        errors.append("Math unused-input traversal contract no longer matches lowering source")
    mix_contract = manifest.get("mixRgbOperationContracts", {})
    expected_mix_modes = {
        "mix": "Color2.rgb",
        "add": "Color1.rgb + Color2.rgb",
        "multiply": "Color1.rgb * Color2.rgb",
        "screen": "1 - (1 - Color1.rgb) * (1 - Color2.rgb)",
        "overlay": "lerp(2 * Color1.rgb * Color2.rgb, 1 - 2 * (1 - Color1.rgb) * (1 - Color2.rgb), step(0.5, Color1.rgb))",
        "darken": "min(Color1.rgb, Color2.rgb)",
        "lighten": "max(Color1.rgb, Color2.rgb)",
        "difference": "abs(Color1.rgb - Color2.rgb)",
    }
    mix_contract_source = mix_contract.get("sourceEvidence", [])
    mix_mode_map = legacy_parameters.get("MixRGB", {}).get("blend", {}).get("values", {})
    expected_mix_ordinal_map = {str(index): mode for index, mode in enumerate(expected_mix_modes)}
    if (mix_contract.get("legacyNode") != "MixRGB" or mix_contract.get("semanticId") != "artifact.color.mix_rgb.legacy_v1" or
            mix_contract.get("blendRgbExpressions") != expected_mix_modes or
            mix_contract.get("channels") != {"blendModes": "RGB only", "alpha": "always linear interpolation between Color1.a and Color2.a using effective Fac"} or
            mix_contract.get("effectiveFactor") != "clampFactor ? saturate(Fac) : Fac" or
            mix_contract.get("factorDefault") != 0.5 or
            mix_contract.get("inputDefaults") != {"Color1": [0.5, 0.5, 0.5, 1.0], "Color2": [1.0, 1.0, 1.0, 1.0]} or
            mix_contract.get("resultRgb") != "lerp(Color1.rgb, blendRgb, effectiveFactor)" or
            mix_contract.get("resultAlpha") != "lerp(Color1.a, Color2.a, effectiveFactor)" or
            mix_contract.get("clampResult") != "when enabled, saturate resultRgb only; resultAlpha remains unclamped" or
            mix_contract.get("status") != "source-audited-finite-runtime-pixel-and-backend-parity-open" or
            mix_mode_map != expected_mix_ordinal_map or not mix_contract_source or any(fragment not in math_source for fragment in mix_contract_source)):
        errors.append("MixRGB blend, alpha, factor, and clamp semantics must be pinned to all eight source operations")
    mix_golden_name = mix_contract.get("formulaGolden")
    mix_golden_path = fixture_root / mix_golden_name if isinstance(mix_golden_name, str) else None
    if not mix_golden_path or not mix_golden_path.is_file():
        errors.append("MixRGB formula golden artifact is missing")
    else:
        try:
            mix_golden = json.loads(mix_golden_path.read_text(encoding="utf-8"))
            oracle_path = root / "tools/shader_graph_rd/generate_mixrgb_golden.py"
            spec = importlib.util.spec_from_file_location("shader_graph_mixrgb_golden_oracle", oracle_path)
            if spec is None or spec.loader is None:
                raise RuntimeError("cannot load MixRGB formula oracle")
            oracle = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(oracle)
            expected_mix_golden = oracle.make_golden(fixture_root, root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp")
            if mix_golden != expected_mix_golden:
                errors.append("MixRGB formula golden differs from the finite binary32 source-expression oracle")
            if hashlib.sha256(mix_golden_path.read_bytes()).hexdigest().upper() != str(mix_contract.get("formulaGoldenSha256", "")).upper():
                errors.append("MixRGB formula golden SHA-256 differs from manifest")
            if mix_golden.get("kind") != "artifact-shader-graph-mixrgb-formula-golden" or mix_golden.get("status") != "candidate-cpu-formula-only-not-hlsl-runtime-or-backend-evidence":
                errors.append("MixRGB formula golden must remain candidate-only and explicitly unverified")
            if len(mix_golden.get("cases", [])) != 8 or {item.get("mode") for item in mix_golden.get("cases", []) if isinstance(item, dict)} != set(expected_mix_modes):
                errors.append("MixRGB formula golden must cover each of its eight modes exactly once")
            if len(mix_golden.get("clampBoundaryCases", [])) != 6:
                errors.append("MixRGB formula golden must cover factor and RGB-only result clamp boundaries")
            for case in mix_golden.get("cases", []):
                fixture_name = case.get("fixture")
                payload = fixture_payloads.get(fixture_name, {})
                nodes_by_id = {node.get("id"): node for node in payload.get("nodes", []) if isinstance(node, dict)}
                inputs = case.get("inputs", {})
                for node_id, input_name, channels in (("color1", "Color1", ("r", "g", "b", "a")), ("color2", "Color2", ("r", "g", "b", "a"))):
                    node = nodes_by_id.get(node_id, {})
                    expected_params = dict(zip(channels, inputs.get(input_name, [])))
                    if node.get("type") != "RGB" or node.get("params") != expected_params:
                        errors.append(f"MixRGB formula golden input differs from fixture: {fixture_name} {input_name}")
                if nodes_by_id.get("factor", {}).get("params") != {"value": inputs.get("Fac")}:
                    errors.append(f"MixRGB formula golden factor differs from fixture: {fixture_name}")
                if nodes_by_id.get("mixrgb", {}).get("params") != {"blend": list(expected_mix_modes).index(case.get("mode")), "clampFactor": inputs.get("clampFactor"), "clampResult": inputs.get("clampResult")}:
                    errors.append(f"MixRGB formula golden mode/clamp fields differ from fixture: {fixture_name}")
        except (OSError, UnicodeError, json.JSONDecodeError, ImportError, AttributeError, TypeError, ValueError, RuntimeError) as error:
            errors.append(f"cannot verify MixRGB formula golden: {error}")
    for ordinal, mode in enumerate(expected_mix_modes):
        fixture_name = f"mixrgb_{mode}.json"
        payload = fixture_payloads.get(fixture_name, {})
        nodes_by_id = {node.get("id"): node for node in payload.get("nodes", []) if isinstance(node, dict)}
        expected_nodes = {
            "color1": ("RGB", {"r": 0.25, "g": 0.75, "b": 0.5, "a": 0.25}),
            "color2": ("RGB", {"r": 0.75, "g": 0.25, "b": 0.5, "a": 0.75}),
            "factor": ("Value", {"value": 0.4}),
            "mixrgb": ("MixRGB", {"blend": ordinal, "clampFactor": False, "clampResult": False}),
        }
        if fixture_name not in declared:
            errors.append(f"MixRGB mode fixture is missing: {fixture_name}")
        for node_id, (node_type, params) in expected_nodes.items():
            node = nodes_by_id.get(node_id, {})
            if node.get("type") != node_type or node.get("params") != params:
                errors.append(f"MixRGB fixture inputs or mode ordinal differ from contract: {fixture_name} {node_id}")
        links = {(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in payload.get("links", []) if isinstance(link, dict)}
        expected_links = {
            ("color1", "Color", "mixrgb", "Color1"), ("color2", "Color", "mixrgb", "Color2"),
            ("factor", "Value", "mixrgb", "Fac"), ("mixrgb", "Color", "surface", "BaseColor"),
            ("surface", "BSDF", "output", "Surface"),
        }
        if links != expected_links:
            errors.append(f"MixRGB fixture topology differs from contract: {fixture_name}")
    mix_image_probe = manifest.get("imageConditions", {}).get("requiredEvaluationMatrix", {}).get("effectCorrectness", {}).get("mixRgbProbe", {})
    if (mix_image_probe.get("fixtures") != [f"mixrgb_{mode}.json" for mode in expected_mix_modes] or
            mix_image_probe.get("captureRoute") != "not-implemented" or
            mix_image_probe.get("formulaGolden") != mix_contract.get("formulaGolden") or
            mix_image_probe.get("formulaGoldenSha256") != mix_contract.get("formulaGoldenSha256") or
            mix_image_probe.get("fixtureCondition") != {"Color1": [0.25, 0.75, 0.5, 0.25], "Color2": [0.75, 0.25, 0.5, 0.75], "Fac": 0.4, "clampFactor": False, "clampResult": False} or
            mix_image_probe.get("overlayBranchCoverage") != "Color1 RGB components below, equal to, and above 0.5 are present; shader/runtime branch behavior remains unverified" or
            len(mix_image_probe.get("plannedFollowupCases", [])) != 3 or
            mix_image_probe.get("runtimeStatus") != "planned-no-capture-source-or-pixel-evidence"):
        errors.append("MixRGB image evaluation plan must pin eight modes, shared inputs, overlay threshold, and open runtime state")
    mix_measurement = mix_image_probe.get("measurement", {})
    if (mix_measurement.get("source") != "linear RGBA32F GPU readback" or
            mix_measurement.get("roi") != "fixed interior pixels on a flat, constant-input surface; record exact pixel coordinates and exclude silhouette/lighting gradients" or
            set(mix_measurement.get("metrics", [])) != {"per-channel mean/min/max and finite-value count", "absolute error at fixed probe pixels versus declared finite formula", "RGB and alpha difference reported independently", "alpha composites over black and white"} or
            mix_measurement.get("reference") != "mixrgb_formula_golden.json is a candidate CPU formula; capture must establish actual runtime values and tolerances before parity is claimed" or
            not mix_measurement.get("displayReview")):
        errors.append("MixRGB image evaluation must define linear RGBA32F sampling, ROI, separate alpha metrics, and visual review")
    operation_schema = manifest.get("coreTypedOperationSchemaProposal", {})
    expected_record_fields = ["operationId", "semanticVersion", "inputs", "outputs", "parameters", "numericDomain", "coordinateSpace", "resourceDependencies", "determinism", "semanticHashFields", "evidenceStatus"]
    expected_input_fields = ["inputId", "valueType", "required", "arity", "defaultPolicy", "linkPolicy"]
    expected_output_fields = ["outputId", "valueType", "cardinality"]
    expected_parameter_fields = ["parameterId", "valueType", "defaultOrRequired", "domainPolicy", "canonicalization"]
    operation_records = operation_schema.get("exampleOperationRecords", [])
    operation_ids = []
    for record in operation_records:
        if not isinstance(record, dict):
            errors.append("typed operation schema example must be an object")
            continue
        operation_id = record.get("operationId")
        operation_ids.append(operation_id)
        if (set(record) != set(expected_record_fields) or not isinstance(operation_id, str) or
                not re.fullmatch(r"artifact\.[a-z][a-z0-9_.]*", operation_id) or
                isinstance(record.get("semanticVersion"), bool) or not isinstance(record.get("semanticVersion"), int) or record["semanticVersion"] <= 0):
            errors.append(f"typed operation record identity/fields are invalid: {operation_id!r}")
        for direction, records, fields, id_key in (("input", record.get("inputs", []), expected_input_fields, "inputId"), ("output", record.get("outputs", []), expected_output_fields, "outputId"), ("parameter", record.get("parameters", []), expected_parameter_fields, "parameterId")):
            ids = []
            if not isinstance(records, list):
                errors.append(f"typed operation {direction} records must be an array: {operation_id}")
                continue
            for item in records:
                if not isinstance(item, dict) or set(item) != set(fields) or not isinstance(item.get(id_key), str) or not re.fullmatch(r"[a-z][a-z0-9_]*", item[id_key]):
                    errors.append(f"typed operation {direction} record is incomplete: {operation_id}")
                    continue
                ids.append(item[id_key])
                if direction == "input" and (isinstance(item.get("arity"), bool) or not isinstance(item.get("arity"), int) or item["arity"] < 1 or not isinstance(item.get("required"), bool) or not item.get("valueType") or not item.get("defaultPolicy") or not item.get("linkPolicy")):
                    errors.append(f"typed operation input type/arity/default/link policy is invalid: {operation_id}.{item[id_key]}")
                if direction == "output" and (not item.get("valueType") or isinstance(item.get("cardinality"), bool) or not isinstance(item.get("cardinality"), int) or item["cardinality"] < 1):
                    errors.append(f"typed operation output type/cardinality is invalid: {operation_id}.{item[id_key]}")
                if direction == "parameter" and (not item.get("valueType") or "defaultOrRequired" not in item or not item.get("domainPolicy") or not item.get("canonicalization")):
                    errors.append(f"typed operation parameter type/domain/default policy is invalid: {operation_id}.{item[id_key]}")
            if len(ids) != len(set(ids)):
                errors.append(f"typed operation {direction} IDs are duplicated: {operation_id}")
        if (not isinstance(record.get("numericDomain"), dict) or set(record["numericDomain"]) != {"input", "output", "rounding", "clamp"} or any(not record["numericDomain"].get(key) for key in ("input", "output", "rounding", "clamp")) or
                not record.get("coordinateSpace") or not isinstance(record.get("resourceDependencies"), list) or
                any(not isinstance(resource, dict) or not resource.get("resourceKey") or not resource.get("sampling") or not resource.get("colorInterpretation") for resource in record.get("resourceDependencies", [])) or
                not record.get("determinism") or not isinstance(record.get("semanticHashFields"), list) or
                not {"operationId", "semanticVersion"} <= set(record.get("semanticHashFields", [])) or
                (bool(record.get("parameters")) and "parameters" not in record.get("semanticHashFields", [])) or
                (bool(record.get("inputs")) and "typed input edges" not in record.get("semanticHashFields", [])) or
                record.get("evidenceStatus") not in {"source-audited-cpu-formula-candidate", "source-audited-runtime-unverified"}):
            errors.append(f"typed operation numeric/hash/evidence contract is incomplete: {operation_id}")
    expected_operation_ids = {
        "artifact.constant.scalar.float32", "artifact.constant.color4.float32",
        "artifact.context.texture_coordinates.uv",
        "artifact.math.add.vector4", "artifact.color.mix_rgb.overlay",
        "artifact.color.ramp.linear_constant_ease", "artifact.texture.material_slot_sample.color4",
        "artifact.surface.principled.fields", "artifact.constant.vector4.float32",
        "artifact.vector4.weighted_luminance.legacy_v1",
        "artifact.math.clamp.sorted_bounds",
        "artifact.vector4.invert.linear_factor",
        *opcode_to_operation_id.values(),
    }
    conversion_record = operation_record_by_id.get("artifact.convert.scalar.replicate.rgb_alpha_one", {})
    if ([(item.get("inputId"), item.get("valueType")) for item in conversion_record.get("inputs", [])] != [("value", "Float")] or
            [(item.get("outputId"), item.get("valueType")) for item in conversion_record.get("outputs", [])] != [("vector", "Vector4")] or
            conversion_record.get("numericDomain", {}).get("output") != "[value,value,value,1.0] Vector4; no saturation"):
        errors.append("explicit scalar replication operation record must pin Float input, Vector4 output, and alpha-one semantics")
    vector_constant_record = operation_record_by_id.get("artifact.constant.vector4.float32", {})
    if ([(item.get("outputId"), item.get("valueType")) for item in vector_constant_record.get("outputs", [])] != [("value", "Vector4")] or
            [(item.get("parameterId"), item.get("valueType")) for item in vector_constant_record.get("parameters", [])] != [("value", "Vector4")] or
            "not Color4" not in vector_constant_record.get("numericDomain", {}).get("output", "")):
        errors.append("raw Vector4 expression source must remain distinct from Color4 semantics")
    truncation_expression = operation_schema.get("vectorTruncationExpression", {})
    if (truncation_expression.get("fixture") != "tests/fixtures/shader-graph-rd/typed_ir_vector4_to_vector3_expression.json" or
            truncation_expression.get("operationId") != "artifact.convert.vector4.truncate_xyz" or
            truncation_expression.get("runtimeStatus") != "candidate-cpu-formula-only-no-core-lowering"):
        errors.append("Vector4-to-Vector3 truncation expression proposal metadata is incomplete")
    legacy_node_mapping = manifest.get("coreDocumentSchemaProposal", {}).get("legacyNodeKindMapping", {})
    legacy_coverage = operation_schema.get("legacyNodeCoverage", {})
    family_coverage = legacy_coverage.get("families", {})
    expected_family_coverage = {
        "Value": "representative-record", "RGB": "representative-record", "TexCoord": "representative-record",
        "Noise": "no-record-yet", "Voronoi": "no-record-yet",
        "ColorRamp": "representative-record", "ImageTexture": "representative-record",
        "Math": "partial-record", "MixRGB": "partial-record",
        "Principled": "representative-record", "MaterialOutput": "no-record-yet",
        "Luminance": "representative-record",
        "Clamp": "representative-record",
        "Invert": "representative-record",
    }
    expected_family_operation_ids = {
        "Value": ["artifact.constant.scalar.float32"],
        "RGB": ["artifact.constant.color4.float32"],
        "TexCoord": ["artifact.context.texture_coordinates.uv"],
        "Noise": [], "Voronoi": [],
        "ColorRamp": ["artifact.color.ramp.linear_constant_ease"],
        "ImageTexture": ["artifact.texture.material_slot_sample.color4"],
        "Math": ["artifact.math.add.vector4"],
        "MixRGB": ["artifact.color.mix_rgb.overlay"],
        "Principled": ["artifact.surface.principled.fields"], "MaterialOutput": [],
        "Luminance": ["artifact.vector4.weighted_luminance.legacy_v1"],
        "Clamp": ["artifact.math.clamp.sorted_bounds"],
        "Invert": ["artifact.vector4.invert.linear_factor"],
    }
    source_characterized_no_record_families = {
        "Color", "Output", "Add", "Geometry", "Fresnel", "LayerWeight", "Blackbody",
        "MixShader", "NormalMap", "Bump", "Gamma", "HueSatVal",
        "MapRange",
    }
    expected_family_coverage.update({name: "no-record-yet" for name in source_characterized_no_record_families})
    expected_family_operation_ids.update({name: [] for name in source_characterized_no_record_families})
    expected_legacy_core_output_mapping = {
        "Value": ({"Value": "Float"}, {"value": "Float"}, "identity scalar; canonicalize to binary32 at the Document boundary"),
        "RGB": ({"Color": "Vector4"}, {"color": "Color4"}, "component-wise identity Vector4-to-Color4 semantic relabel; no swizzle or conversion"),
    }
    observed_fixture_kinds = {
        node.get("type")
        for payload in fixture_payloads.values()
        for node in payload.get("nodes", [])
        if isinstance(node, dict) and isinstance(node.get("type"), str)
    }
    family_coverage_errors = []
    if set(family_coverage) != set(expected_family_coverage) or set(legacy_node_mapping) != set(expected_family_coverage):
        family_coverage_errors.append("legacy node family inventory differs")
    for node_kind, expected_coverage in expected_family_coverage.items():
        entry = family_coverage.get(node_kind, {})
        if not isinstance(entry, dict) or entry.get("coverage") != expected_coverage:
            family_coverage_errors.append(f"coverage classification is invalid for {node_kind}")
            continue
        operation_ids_for_kind = entry.get("operationIds")
        if (not isinstance(operation_ids_for_kind, list) or
                operation_ids_for_kind != expected_family_operation_ids[node_kind] or
                any(operation_id not in operation_ids for operation_id in operation_ids_for_kind) or
                len(operation_ids_for_kind) != len(set(operation_ids_for_kind)) or
                (expected_coverage == "no-record-yet") != (not operation_ids_for_kind) or
                not entry.get("gap")):
            family_coverage_errors.append(f"record references or gap are invalid for {node_kind}")
        if node_kind in expected_legacy_core_output_mapping:
            legacy_types, core_types, adapter = expected_legacy_core_output_mapping[node_kind]
            if (entry.get("legacyOutputTypes") != legacy_types or
                    entry.get("coreOutputTypes") != core_types or
                    entry.get("adapter") != adapter):
                family_coverage_errors.append(f"legacy-to-core output type mapping is invalid for {node_kind}")
    if observed_fixture_kinds - set(family_coverage):
        family_coverage_errors.append("fixture node kinds are missing legacy operation-family rows: " + ", ".join(sorted(observed_fixture_kinds - set(family_coverage))))
    if (legacy_coverage.get("status") != "proposal-coverage-audit-not-support-claim" or
            legacy_coverage.get("runtimeStatus") != "manifest-audited-proposal-only-no-registry-or-lowering-implementation" or
            legacy_coverage.get("socketMappingPolicy") != "Every family operation record declares operationSocketMappings for every input/output. Each mapping names one source-characterized legacy socket label and stable socket ID; type changes require a named explicit adapter. Mapping is document metadata and does not establish runtime registry support." or
            family_coverage_errors):
        errors.append("typed operation family coverage must enumerate every fixture node kind without implying implementation support: " + "; ".join(family_coverage_errors))
    texcoord_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
    texcoord_interface = (root / "ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx").read_text(encoding="utf-8")
    if ("addOutput(\"UV\", PinType::Vector2);" not in texcoord_source or
            'float2 {0}_UV = In.UV;' not in texcoord_source or
            "// Phase 1 exposes UV only (Blender TexCoord has more sockets)." not in texcoord_interface):
        errors.append("TexCoord operation proposal must stay bounded to the source-audited UV-only In.UV behavior")
    required_operation_schema_fields = {"schemaId", "schemaVersion", "status", "recordFields", "inputRecordFields", "outputRecordFields", "parameterRecordFields", "recordRules", "exampleOperationRecords", "legacyNodeCoverage", "promotionGate", "fixtureProjectionReadiness", "explicitConversionExpression", "vectorTruncationExpression", "typedLoweringCandidateContract", "operationRegistryAudit", "operationPromotionWaves", "constantValueCanonicalizationGolden", "color4SemanticPolicy", "color4PolicyEvaluationCases", "color4PolicyEvaluationStatus", "surfaceAlphaFormulaGolden", "luminanceFormulaGolden", "clampFormulaGolden", "vector4InvertFormulaGolden", "mapRangeFormulaGolden", "dynamicGraphPrototype"}
    if (not required_operation_schema_fields <= set(operation_schema) or
            operation_schema.get("schemaId") != "artifact.material-graph.typed-operation-schema" or
            operation_schema.get("schemaVersion") != 1 or operation_schema.get("status") != "proposal-only-not-implemented" or
            operation_schema.get("recordFields") != expected_record_fields or
            operation_schema.get("inputRecordFields") != expected_input_fields or
            operation_schema.get("outputRecordFields") != expected_output_fields or
            operation_schema.get("parameterRecordFields") != expected_parameter_fields or
            set(operation_ids) != expected_operation_ids or len(operation_ids) != len(set(operation_ids)) or
            not all(operation_schema.get("recordRules", {}).get(key) for key in ("operationId", "inputAndOutputIds", "arity", "linkPolicy", "defaultPolicy", "numericDomain", "coordinateSpace", "resourceDependencies", "determinism", "semanticHashFields", "evidenceStatus")) or
            not operation_schema.get("promotionGate")):
        errors.append("Core typed operation schema proposal is incomplete, implemented prematurely, or changed without review")
    lowering_candidate = operation_schema.get("typedLoweringCandidateContract", {})
    lowering_candidate_expected = {
        "status": "proposal-only-no-Core-registry-or-lowerer",
        "operationRegistryKey": "(operationId, semanticVersion)",
        "operationIdentity": "stable operation ID identifies semantics; schema version is not a substitute for semanticVersion",
        "socketIdentity": "stable inputId/outputId independent of UI label, pin ordering, and localization",
        "inputBinding": "exact declared type, bounded arity, explicit missing/default policy, single-writer unless variadic bounds are declared",
        "conversion": "type changes are represented by explicit typed conversion operations, not adapter-side coercion",
        "outputSelection": "one stable MaterialOutput node ID is selected before active-graph traversal",
        "activeGraph": "traverse only semantically consumed inputs of selected output; structurally validate the entire document before pruning",
        "resourceRequirements": "operations declare symbolic requirements; renderer adapter resolves assets, revisions, samplers, color interpretation, and backend bindings",
        "loweringResult": "all reachable operations must lower or return one diagnostic result with no partial IR/source/hash",
        "hashBoundary": "typed IR hash includes operation IDs/versions, stable typed edges, canonical parameters/defaults, selected output, and compile-time specialization; excludes layout, UI labels, diagnostics, and nonsemantic opaque fields",
        "dynamicRegistration": "graph documents dynamically instantiate versioned operation IDs from declarative immutable definitions that drive palette/socket/parameter metadata; each definition selects a closed trusted implementation ID, never executable source or HLSL payload; a new algorithm requires a reviewed native implementation and semantic version change",
        "runtimeStatus": "design-candidate-not-implemented-or-runtime-verified",
    }
    if lowering_candidate != lowering_candidate_expected:
        errors.append("typed lowering candidate contract omits registry, graph, resource, atomicity, or identity boundaries")
    color4_policy = operation_schema.get("color4SemanticPolicy", {})
    color4_policy_expected = {
        "status": "unresolved-product-contract-source-audited",
        "authoredColorTransferOptions": ["linear-srgb", "srgb-encoded", "explicit-color-encoding-tag"],
        "authoredAlphaAssociationOptions": ["straight", "premultiplied"],
        "textureSamplingTransfer": "current-sRGB texture resources decode RGB through the resource format; alpha is not sRGB transformed",
        "graphConstantCurrentPath": "raw RGBA tuple is emitted directly into HLSL; no graph-node color decode, premultiply, or unpremultiply is visible",
        "graphBaseColorComposition": "graphBaseColor replaces fixed-path baseColor after texture sampling and vertex/material color multiplication",
        "transparentBlendCurrentPath": "SrcBlend=SRC_ALPHA and DestBlend=INV_SRC_ALPHA; alpha uses ONE and INV_SRC_ALPHA",
        "promotionRequirement": "select color encoding, alpha association, and renderer conversion boundary; add linear float and transparent multi-background captures before color4 operation promotion",
        "runtimeStatus": "source-inspection-only-no-color-policy-review-or-capture",
    }
    if color4_policy != color4_policy_expected:
        errors.append("Color4 product policy must separate source-observed adapter behavior from unresolved color/alpha choices")
    color4_evaluation_cases = operation_schema.get("color4PolicyEvaluationCases", [])
    transfer_case = next((item for item in color4_evaluation_cases if item.get("caseId") == "color-transfer-opaque-gray"), {})
    alpha_case = next((item for item in color4_evaluation_cases if item.get("caseId") == "alpha-association-half-red-over-black-white"), {})
    if (operation_schema.get("color4PolicyEvaluationStatus") != "planned-no-capture-or-product-choice" or
            {item.get("caseId") for item in color4_evaluation_cases} != {"color-transfer-opaque-gray", "alpha-association-half-red-over-black-white"} or
            transfer_case.get("input", {}).get("rgba") != [0.5, 0.5, 0.5, 1.0] or
            "linear RGBA32F" not in transfer_case.get("measurement", "") or
            alpha_case.get("input", {}).get("rgba") != [1.0, 0.0, 0.0, 0.5] or
            alpha_case.get("backgrounds") != ["linear black", "linear white"] or
            not all(term in alpha_case.get("measurement", "") for term in ("RGBA32F", "linear-light"))):
        errors.append("Color4 policy evaluation must include an opaque transfer probe and half-alpha black/white association probe without claiming captures")
    color_operation = operation_record_by_id.get("artifact.constant.color4.float32", {})
    if ("unresolved" not in color_operation.get("numericDomain", {}).get("output", "") or
            "unresolved" not in color_operation.get("determinism", "")):
        errors.append("Color4 constant operation record must retain unresolved colorimetric semantics pending policy/evaluation")
    color_source = (root / "ArtifactCore/src/Graphics/MeshRenderer.cpp").read_text(encoding="utf-8")
    color_shader_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
    color_source_fragments = (
        "desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;",
        "float3 srgbToLinear(float3 value)",
        '"    baseColor = graphBaseColor;\\n"',
        "RT0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;",
        "RT0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;",
        "RT0.SrcBlendAlpha = BLEND_FACTOR_ONE;",
        'matFloat4Lit(n->r, n->g, n->b, n->a)',
    )
    if any(fragment not in color_source + color_shader_source for fragment in color_source_fragments):
        errors.append("Color4 source evidence for texture transfer, graph replacement, tuple emission, or alpha blend state has drifted")
    surface_alpha_contract = operation_schema.get("surfaceAlphaFormulaGolden", {})
    surface_alpha_path = root / surface_alpha_contract.get("fixture", "")
    surface_alpha_generator = root / surface_alpha_contract.get("generator", "")
    try:
        surface_alpha_golden = json.loads(surface_alpha_path.read_text(encoding="utf-8"))
        surface_alpha_spec = importlib.util.spec_from_file_location("shader_graph_surface_alpha_golden", surface_alpha_generator)
        if surface_alpha_spec is None or surface_alpha_spec.loader is None:
            raise RuntimeError("cannot load surface alpha formula oracle")
        surface_alpha_module = importlib.util.module_from_spec(surface_alpha_spec)
        surface_alpha_spec.loader.exec_module(surface_alpha_module)
        regenerated_surface_alpha = surface_alpha_module.build_golden()
        association = surface_alpha_golden.get("associationComparison", {})
        straight = association.get("straightInputComposites", {})
        premultiplied = association.get("premultipliedInputPassedToCurrentStraightBlend", {})
        alpha_cases = surface_alpha_golden.get("alphaCompositionCases", [])
        if (surface_alpha_contract.get("status") != "source-derived-cpu-formula-candidate-no-renderer-runtime" or
                surface_alpha_golden != regenerated_surface_alpha or
                surface_alpha_golden.get("kind") != "artifact-shader-graph-surface-alpha-golden" or
                [item.get("baseColorAlpha") for item in alpha_cases] != [0.0, 0.5, 1.0] or
                any(abs(item.get("expectedMaterialAlpha", -1.0) - item.get("baseColorAlpha", -1.0) * 0.4 * 0.5) > 1e-7 for item in alpha_cases) or
                any(abs(actual - expected) > 1e-7 for actual, expected in zip(straight.get("black", {}).get("rgb", []), [0.4, 0.1, 0.05])) or
                any(abs(actual - expected) > 1e-7 for actual, expected in zip(premultiplied.get("black", {}).get("rgb", []), [0.2, 0.05, 0.025])) or
                surface_alpha_golden.get("sourceContract", {}).get("transparentAlpha") != "srcAlpha + dstAlpha * (1-srcAlpha)"):
            errors.append("surface alpha formula golden must reproduce current graph-alpha and straight-blend equations without claiming runtime evidence")
        alpha_source_fragments = (
            'float materialAlpha = baseColor.a * opacitySample.a;',
            '"    materialAlpha = graphBaseColor.a * graphAlpha;\\n"',
            "RT0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;",
            "RT0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;",
            "RT0.SrcBlendAlpha = BLEND_FACTOR_ONE;",
            "RT0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;",
        )
        if any(fragment not in color_source for fragment in alpha_source_fragments):
            errors.append("surface alpha formula source evidence has drifted from current MeshRenderer equations")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error) as error:
        errors.append(f"cannot validate surface alpha formula golden: {error}")
    luminance_contract = operation_schema.get("luminanceFormulaGolden", {})
    luminance_path = root / luminance_contract.get("fixture", "")
    luminance_generator_path = root / luminance_contract.get("generator", "")
    luminance_record = operation_record_by_id.get("artifact.vector4.weighted_luminance.legacy_v1", {})
    try:
        luminance_golden = json.loads(luminance_path.read_text(encoding="utf-8"))
        luminance_spec = importlib.util.spec_from_file_location("shader_graph_luminance_golden", luminance_generator_path)
        if luminance_spec is None or luminance_spec.loader is None:
            raise RuntimeError("cannot load Luminance formula oracle")
        luminance_module = importlib.util.module_from_spec(luminance_spec)
        luminance_spec.loader.exec_module(luminance_module)
        source_luminance = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        expected_luminance_source = 'dot(({1}).rgb, float3(0.2126f, 0.7152f, 0.0722f))'
        if (luminance_contract.get("status") != "source-derived-cpu-formula-candidate-no-renderer-runtime" or
                luminance_golden != luminance_module.build_golden() or
                hashlib.sha256(luminance_path.read_bytes()).hexdigest().upper() != luminance_contract.get("sha256") or
                luminance_golden.get("operationId") != "artifact.vector4.weighted_luminance.legacy_v1" or
                len(luminance_golden.get("cases", [])) != 6 or
                luminance_golden["cases"][3]["expectedFloat32"] != luminance_golden["cases"][4]["expectedFloat32"] or
                [(pin.get("inputId"), pin.get("valueType")) for pin in luminance_record.get("inputs", [])] != [("color", "Vector4")] or
                luminance_record.get("inputs", [{}])[0].get("defaultPolicy", "").find("[0,0,0,1]") < 0 or
                [(pin.get("outputId"), pin.get("valueType")) for pin in luminance_record.get("outputs", [])] != [("val", "Float")] or
                "alpha ignored" not in luminance_record.get("numericDomain", {}).get("output", "") or
                expected_luminance_source not in source_luminance):
            errors.append("Luminance operation record and CPU golden must match the source Vector4-to-Float formula without claiming color-space or runtime semantics")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error, RuntimeError, IndexError) as error:
        errors.append(f"cannot validate Luminance operation formula golden: {error}")
    clamp_contract = operation_schema.get("clampFormulaGolden", {})
    clamp_path = root / clamp_contract.get("fixture", "")
    clamp_generator_path = root / clamp_contract.get("generator", "")
    clamp_record = operation_record_by_id.get("artifact.math.clamp.sorted_bounds", {})
    try:
        clamp_golden = json.loads(clamp_path.read_text(encoding="utf-8"))
        clamp_spec = importlib.util.spec_from_file_location("shader_graph_clamp_golden", clamp_generator_path)
        if clamp_spec is None or clamp_spec.loader is None:
            raise RuntimeError("cannot load Clamp formula oracle")
        clamp_module = importlib.util.module_from_spec(clamp_spec)
        clamp_spec.loader.exec_module(clamp_module)
        source_clamp = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        source_fragments = (
            'node, "Value", PinType::Float, "0.0f"',
            'node, "Min", PinType::Float, "0.0f"',
            'node, "Max", PinType::Float, "1.0f"',
            'clamp(({1}), min(({2}), ({3})), max(({2}), ({3})))',
        )
        expected_case_ids = {"inside-range", "below-range", "above-range", "reversed-bounds-low", "reversed-bounds-high", "equal-bounds", "negative-domain"}
        if (clamp_contract.get("status") != "source-derived-cpu-formula-candidate-no-renderer-runtime" or
                clamp_golden != clamp_module.build_golden() or
                hashlib.sha256(clamp_path.read_bytes()).hexdigest().upper() != clamp_contract.get("sha256") or
                clamp_golden.get("operationId") != "artifact.math.clamp.sorted_bounds" or
                {case.get("caseId") for case in clamp_golden.get("cases", [])} != expected_case_ids or
                [(pin.get("inputId"), pin.get("valueType")) for pin in clamp_record.get("inputs", [])] != [("value", "Float"), ("min", "Float"), ("max", "Float")] or
                [(pin.get("outputId"), pin.get("valueType")) for pin in clamp_record.get("outputs", [])] != [("result", "Float")] or
                "reversed bounds are sorted" not in clamp_record.get("numericDomain", {}).get("output", "") or
                any(fragment not in source_clamp for fragment in source_fragments)):
            errors.append("Clamp operation record and CPU golden must match finite scalar inputs, sorted endpoint bounds, source defaults, and current HLSL formula")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error, RuntimeError) as error:
        errors.append(f"cannot validate Clamp operation formula golden: {error}")
    invert_contract = operation_schema.get("vector4InvertFormulaGolden", {})
    invert_path = root / invert_contract.get("fixture", "")
    invert_generator_path = root / invert_contract.get("generator", "")
    invert_record = operation_record_by_id.get("artifact.vector4.invert.linear_factor", {})
    try:
        invert_golden = json.loads(invert_path.read_text(encoding="utf-8"))
        invert_spec = importlib.util.spec_from_file_location("shader_graph_vector4_invert_golden", invert_generator_path)
        if invert_spec is None or invert_spec.loader is None:
            raise RuntimeError("cannot load Vector4 Invert formula oracle")
        invert_module = importlib.util.module_from_spec(invert_spec)
        invert_spec.loader.exec_module(invert_module)
        source_invert = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        expected_cases = {"factor-zero", "factor-half", "factor-one", "factor-below-zero-saturates", "factor-above-one-saturates", "raw-vector-components-outside-unit-range", "legacy-unconnected-color-default"}
        if (invert_contract.get("status") != "source-derived-cpu-formula-candidate-no-renderer-runtime" or
                invert_golden != invert_module.build_golden() or
                hashlib.sha256(invert_path.read_bytes()).hexdigest().upper() != invert_contract.get("sha256") or
                invert_golden.get("operationId") != "artifact.vector4.invert.linear_factor" or
                {case.get("caseId") for case in invert_golden.get("cases", [])} != expected_cases or
                [(pin.get("inputId"), pin.get("valueType")) for pin in invert_record.get("inputs", [])] != [("color", "Vector4"), ("factor", "Float")] or
                [(pin.get("outputId"), pin.get("valueType")) for pin in invert_record.get("outputs", [])] != [("color", "Vector4")] or
                "saturate({1})" not in source_invert or
                "lerp(({1}), float4(1.0f,1.0f,1.0f,1.0f) - ({1}), {0}_f)" not in source_invert or
                "Vector4 is treated as a raw numeric tuple" not in " ".join(invert_golden.get("limitations", []))):
            errors.append("Vector4 Invert operation record and CPU golden must match current all-component source formula without assigning Color4 semantics")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error, RuntimeError) as error:
        errors.append(f"cannot validate Vector4 Invert operation formula golden: {error}")
    map_range_contract = operation_schema.get("mapRangeFormulaGolden", {})
    map_range_path = root / map_range_contract.get("fixture", "")
    map_range_generator_path = root / map_range_contract.get("generator", "")
    try:
        map_range_golden = json.loads(map_range_path.read_text(encoding="utf-8"))
        map_range_spec = importlib.util.spec_from_file_location("shader_graph_map_range_golden", map_range_generator_path)
        if map_range_spec is None or map_range_spec.loader is None:
            raise RuntimeError("cannot load MapRange formula oracle")
        map_range_module = importlib.util.module_from_spec(map_range_spec)
        map_range_spec.loader.exec_module(map_range_module)
        source_map_range = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        expected_map_range_cases = {"linear-midpoint", "linear-extrapolates-below", "linear-clamps-output", "smoothstep-midpoint", "smoothstep-does-not-clamp-input-t", "smootherstep-midpoint", "smootherstep-does-not-clamp-input-t", "stepped-quarter", "stepped-saturates-t-above-range", "stepped-floor-steps-below-one", "zero-span-fallback-one", "reversed-source-span", "reversed-target-range-clamp"}
        source_fragments = ["abs({0}_span) < 1e-5f", "(floor(saturate({0}_t) * max(floor({1}), 1.0f))", "(({0}_t) * ({0}_t) * (3.0f - 2.0f * ({0}_t)))", "(({0}_t) * ({0}_t) * ({0}_t) * (({0}_t) * (({0}_t) * 6.0f - 15.0f) + 10.0f))", "clamp(({0}), min(({1}), ({2})), max(({1}), ({2})))"]
        if (map_range_contract.get("status") != "source-derived-cpu-formula-candidate-no-renderer-runtime" or
                map_range_golden != map_range_module.build_golden() or
                hashlib.sha256(map_range_path.read_bytes()).hexdigest().upper() != map_range_contract.get("sha256") or
                {case.get("caseId") for case in map_range_golden.get("cases", [])} != expected_map_range_cases or
                any(fragment not in source_map_range for fragment in source_fragments) or
                "SmoothStep and SmootherStep use the raw t without saturation" not in " ".join(map_range_golden.get("limitations", []))):
            errors.append("MapRange formula golden must match source defaults, all interpolation modes, clamp policy, and degenerate/reversed range behavior")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error, RuntimeError) as error:
        errors.append(f"cannot validate MapRange formula golden: {error}")
    dynamic_math_golden = {}
    dynamic_math_golden_name = math_contract.get("formulaGolden")
    dynamic_math_golden_path = fixture_root / dynamic_math_golden_name if isinstance(dynamic_math_golden_name, str) else None
    if dynamic_math_golden_path and dynamic_math_golden_path.is_file():
        try:
            dynamic_math_golden = json.loads(dynamic_math_golden_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            dynamic_math_golden = {}
    dynamic_contract = operation_schema.get("dynamicGraphPrototype", {})
    dynamic_registry_path = root / dynamic_contract.get("registryFixture", "")
    dynamic_document_path = root / dynamic_contract.get("documentFixture", "")
    dynamic_candidate_path = root / dynamic_contract.get("candidateFixture", "")
    dynamic_vector3_constant_document_path = root / dynamic_contract.get("vector3ConstantDocumentFixture", "")
    dynamic_vector3_constant_candidate_path = root / dynamic_contract.get("vector3ConstantCandidateFixture", "")
    dynamic_vector3_arithmetic_document_path = root / dynamic_contract.get("vector3ArithmeticDocumentFixture", "")
    dynamic_vector3_arithmetic_candidate_path = root / dynamic_contract.get("vector3ArithmeticCandidateFixture", "")
    dynamic_vector3_graph_parameter_document_path = root / dynamic_contract.get("vector3GraphParameterDocumentFixture", "")
    dynamic_vector3_graph_parameter_candidate_path = root / dynamic_contract.get("vector3GraphParameterCandidateFixture", "")
    dynamic_subtract_document_path = root / dynamic_contract.get("subtractDocumentFixture", "")
    dynamic_subtract_candidate_path = root / dynamic_contract.get("subtractCandidateFixture", "")
    dynamic_divide_document_path = root / dynamic_contract.get("divideDocumentFixture", "")
    dynamic_divide_candidate_path = root / dynamic_contract.get("divideCandidateFixture", "")
    dynamic_vector4_divide_document_path = root / dynamic_contract.get("vector4DivideDocumentFixture", "")
    dynamic_vector4_divide_candidate_path = root / dynamic_contract.get("vector4DivideCandidateFixture", "")
    dynamic_vector4_subtract_document_path = root / dynamic_contract.get("vector4SubtractDocumentFixture", "")
    dynamic_vector4_subtract_candidate_path = root / dynamic_contract.get("vector4SubtractCandidateFixture", "")
    dynamic_vector4_invert_document_path = root / dynamic_contract.get("vector4InvertDocumentFixture", "")
    dynamic_vector4_invert_candidate_path = root / dynamic_contract.get("vector4InvertCandidateFixture", "")
    dynamic_clamp_document_path = root / dynamic_contract.get("clampDocumentFixture", "")
    dynamic_clamp_candidate_path = root / dynamic_contract.get("clampCandidateFixture", "")
    dynamic_vector4_mix_document_path = root / dynamic_contract.get("vector4MixDocumentFixture", "")
    dynamic_vector4_mix_candidate_path = root / dynamic_contract.get("vector4MixCandidateFixture", "")
    dynamic_vector4_separate_document_path = root / dynamic_contract.get("vector4SeparateDocumentFixture", "")
    dynamic_vector4_separate_candidate_path = root / dynamic_contract.get("vector4SeparateCandidateFixture", "")
    dynamic_vector4_ramp_document_path = root / dynamic_contract.get("vector4RampDocumentFixture", "")
    dynamic_vector4_ramp_candidate_path = root / dynamic_contract.get("vector4RampCandidateFixture", "")
    dynamic_optional_default_document_path = root / dynamic_contract.get("optionalDefaultDocumentFixture", "")
    dynamic_optional_default_candidate_path = root / dynamic_contract.get("optionalDefaultCandidateFixture", "")
    dynamic_optional_vector4_default_document_path = root / dynamic_contract.get("optionalVector4DefaultDocumentFixture", "")
    dynamic_optional_vector4_default_candidate_path = root / dynamic_contract.get("optionalVector4DefaultCandidateFixture", "")
    dynamic_optional_vector3_default_document_path = root / dynamic_contract.get("optionalVector3DefaultDocumentFixture", "")
    dynamic_optional_vector3_default_candidate_path = root / dynamic_contract.get("optionalVector3DefaultCandidateFixture", "")
    dynamic_vector2_uv_document_path = root / dynamic_contract.get("vector2UvDocumentFixture", "")
    dynamic_vector2_uv_candidate_path = root / dynamic_contract.get("vector2UvCandidateFixture", "")
    dynamic_vector2_mapping_document_path = root / dynamic_contract.get("vector2MappingDocumentFixture", "")
    dynamic_vector2_mapping_candidate_path = root / dynamic_contract.get("vector2MappingCandidateFixture", "")
    dynamic_base_color_texture_document_path = root / dynamic_contract.get("baseColorTextureDocumentFixture", "")
    dynamic_base_color_texture_candidate_path = root / dynamic_contract.get("baseColorTextureCandidateFixture", "")
    dynamic_mapped_base_color_texture_document_path = root / dynamic_contract.get("mappedBaseColorTextureDocumentFixture", "")
    dynamic_mapped_base_color_texture_candidate_path = root / dynamic_contract.get("mappedBaseColorTextureCandidateFixture", "")
    dynamic_texture_channels_document_path = root / dynamic_contract.get("textureChannelsDocumentFixture", "")
    dynamic_texture_channels_candidate_path = root / dynamic_contract.get("textureChannelsCandidateFixture", "")
    dynamic_vector4_ramp_golden_path = root / dynamic_contract.get("vector4RampFormulaGoldenFixture", "")
    dynamic_ramp_image_plan = dynamic_contract.get("rampImageEvaluationPlan", {})
    dynamic_ramp_image_report_path = root / dynamic_ramp_image_plan.get("generatedOracle", {}).get("report", "")
    dynamic_prototype_path = root / dynamic_contract.get("prototype", "")
    try:
        dynamic_registry_source = json.loads(dynamic_registry_path.read_text(encoding="utf-8"))
        dynamic_document = json.loads(dynamic_document_path.read_text(encoding="utf-8"))
        dynamic_candidate = json.loads(dynamic_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector3_constant_document = json.loads(dynamic_vector3_constant_document_path.read_text(encoding="utf-8"))
        dynamic_vector3_constant_candidate = json.loads(dynamic_vector3_constant_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector3_arithmetic_document = json.loads(dynamic_vector3_arithmetic_document_path.read_text(encoding="utf-8"))
        dynamic_vector3_arithmetic_candidate = json.loads(dynamic_vector3_arithmetic_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector3_graph_parameter_document = json.loads(dynamic_vector3_graph_parameter_document_path.read_text(encoding="utf-8"))
        dynamic_vector3_graph_parameter_candidate = json.loads(dynamic_vector3_graph_parameter_candidate_path.read_text(encoding="utf-8"))
        dynamic_subtract_document = json.loads(dynamic_subtract_document_path.read_text(encoding="utf-8"))
        dynamic_subtract_candidate = json.loads(dynamic_subtract_candidate_path.read_text(encoding="utf-8"))
        dynamic_divide_document = json.loads(dynamic_divide_document_path.read_text(encoding="utf-8"))
        dynamic_divide_candidate = json.loads(dynamic_divide_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_divide_document = json.loads(dynamic_vector4_divide_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_divide_candidate = json.loads(dynamic_vector4_divide_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_subtract_document = json.loads(dynamic_vector4_subtract_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_subtract_candidate = json.loads(dynamic_vector4_subtract_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_invert_document = json.loads(dynamic_vector4_invert_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_invert_candidate = json.loads(dynamic_vector4_invert_candidate_path.read_text(encoding="utf-8"))
        dynamic_clamp_document = json.loads(dynamic_clamp_document_path.read_text(encoding="utf-8"))
        dynamic_clamp_candidate = json.loads(dynamic_clamp_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_mix_document = json.loads(dynamic_vector4_mix_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_mix_candidate = json.loads(dynamic_vector4_mix_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_separate_document = json.loads(dynamic_vector4_separate_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_separate_candidate = json.loads(dynamic_vector4_separate_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_ramp_document = json.loads(dynamic_vector4_ramp_document_path.read_text(encoding="utf-8"))
        dynamic_vector4_ramp_candidate = json.loads(dynamic_vector4_ramp_candidate_path.read_text(encoding="utf-8"))
        dynamic_optional_default_document = json.loads(dynamic_optional_default_document_path.read_text(encoding="utf-8"))
        dynamic_optional_default_candidate = json.loads(dynamic_optional_default_candidate_path.read_text(encoding="utf-8"))
        dynamic_optional_vector4_default_document = json.loads(dynamic_optional_vector4_default_document_path.read_text(encoding="utf-8"))
        dynamic_optional_vector4_default_candidate = json.loads(dynamic_optional_vector4_default_candidate_path.read_text(encoding="utf-8"))
        dynamic_optional_vector3_default_document = json.loads(dynamic_optional_vector3_default_document_path.read_text(encoding="utf-8"))
        dynamic_optional_vector3_default_candidate = json.loads(dynamic_optional_vector3_default_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector2_uv_document = json.loads(dynamic_vector2_uv_document_path.read_text(encoding="utf-8"))
        dynamic_vector2_uv_candidate = json.loads(dynamic_vector2_uv_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector2_mapping_document = json.loads(dynamic_vector2_mapping_document_path.read_text(encoding="utf-8"))
        dynamic_vector2_mapping_candidate = json.loads(dynamic_vector2_mapping_candidate_path.read_text(encoding="utf-8"))
        dynamic_base_color_texture_document = json.loads(dynamic_base_color_texture_document_path.read_text(encoding="utf-8"))
        dynamic_base_color_texture_candidate = json.loads(dynamic_base_color_texture_candidate_path.read_text(encoding="utf-8"))
        dynamic_mapped_base_color_texture_document = json.loads(dynamic_mapped_base_color_texture_document_path.read_text(encoding="utf-8"))
        dynamic_mapped_base_color_texture_candidate = json.loads(dynamic_mapped_base_color_texture_candidate_path.read_text(encoding="utf-8"))
        dynamic_texture_channels_document = json.loads(dynamic_texture_channels_document_path.read_text(encoding="utf-8"))
        dynamic_texture_channels_candidate = json.loads(dynamic_texture_channels_candidate_path.read_text(encoding="utf-8"))
        dynamic_vector4_ramp_golden = json.loads(dynamic_vector4_ramp_golden_path.read_text(encoding="utf-8"))
        dynamic_ramp_image_report = json.loads(dynamic_ramp_image_report_path.read_text(encoding="utf-8"))
        dynamic_mixrgb_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        dynamic_mixrgb_interface = (root / "ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx").read_text(encoding="utf-8")
        dynamic_asset_database_interface = (root / "ArtifactCore/include/Asset/AssetDatabase.ixx").read_text(encoding="utf-8")
        dynamic_asset_manager_interface = (root / "ArtifactCore/include/Utils/AssetManager.ixx").read_text(encoding="utf-8")
        dynamic_asset_manager_source = (root / "ArtifactCore/src/Asset/AssetManager.cpp").read_text(encoding="utf-8")
        dynamic_project_items_interface = (root / "Artifact/include/Project/ArtifactProjectItems.ixx").read_text(encoding="utf-8")
        dynamic_mesh_renderer_source = (root / "ArtifactCore/src/Graphics/MeshRenderer.cpp").read_text(encoding="utf-8")
        generic_image_contract = dynamic_contract.get("genericImageResourceReferencePrototype", {})
        generic_image_document_path = root / generic_image_contract.get("documentFixture", "")
        generic_image_catalog_path = root / generic_image_contract.get("assetCatalogFixture", "")
        generic_image_source_states_path = root / generic_image_contract.get("assetSourceStatesFixture", "")
        generic_image_mapped_uv_document_path = root / generic_image_contract.get("mappedUvDocumentFixture", "")
        generic_image_mapped_dynamic_document_path = root / generic_image_contract.get("mappedUvDynamicDocumentFixture", "")
        generic_image_candidate_path = root / generic_image_contract.get("candidateFixture", "")
        generic_image_prototype_path = root / generic_image_contract.get("prototype", "")
        generic_image_document = json.loads(generic_image_document_path.read_text(encoding="utf-8"))
        generic_image_catalog = json.loads(generic_image_catalog_path.read_text(encoding="utf-8"))
        generic_image_source_states = json.loads(generic_image_source_states_path.read_text(encoding="utf-8"))
        generic_image_mapped_uv_document = json.loads(generic_image_mapped_uv_document_path.read_text(encoding="utf-8"))
        generic_image_mapped_dynamic_document = json.loads(generic_image_mapped_dynamic_document_path.read_text(encoding="utf-8"))
        generic_image_candidate = json.loads(generic_image_candidate_path.read_text(encoding="utf-8"))
        generic_image_spec = importlib.util.spec_from_file_location(
            "shader_graph_generic_image_resource_prototype", generic_image_prototype_path)
        if generic_image_spec is None or generic_image_spec.loader is None:
            raise RuntimeError("cannot load generic Image Texture resource prototype")
        generic_image_module = importlib.util.module_from_spec(generic_image_spec)
        sys.modules[generic_image_spec.name] = generic_image_module
        generic_image_spec.loader.exec_module(generic_image_module)
        dynamic_resolver_path = root / dynamic_contract.get("prototype", "")
        dynamic_resolver_spec = importlib.util.spec_from_file_location(
            "shader_graph_dynamic_binding_resolver_contract", dynamic_resolver_path)
        if dynamic_resolver_spec is None or dynamic_resolver_spec.loader is None:
            raise RuntimeError("cannot load dynamic graph binding resolver contract")
        dynamic_resolver_module = importlib.util.module_from_spec(dynamic_resolver_spec)
        sys.modules[dynamic_resolver_spec.name] = dynamic_resolver_module
        dynamic_resolver_spec.loader.exec_module(dynamic_resolver_module)
        dynamic_adapter_registry_path = root / dynamic_contract.get("registryFixture", "")
        dynamic_adapter_registry = dynamic_resolver_module.load_registry(
            dynamic_adapter_registry_path)

        def project_generic_image_through_dynamic_resolver(document, catalog, source_states):
            projection = generic_image_module.project_resource_requirements(document, catalog)
            resolutions = {item["resourceKey"]: item for item in projection["assetResolutions"]}
            nodes_by_id = {item["nodeId"]: item for item in document["nodes"]}
            states_by_id = {key.lower(): value for key, value in source_states.items()}
            bindings = {}
            for requirement in projection["resourceRequirements"]:
                resolution = resolutions.get(requirement["resourceKey"])
                if resolution is None or resolution["assetType"] is None:
                    continue
                state = states_by_id.get(resolution["assetId"].lower())
                if state is None:
                    continue
                node = nodes_by_id[requirement["nodeId"]]
                sampler_bytes = json.dumps(node["sampler"], sort_keys=True,
                                           separators=(",", ":"),
                                           ensure_ascii=False).encode("utf-8")
                bindings[requirement["resourceKey"]] = {
                    "assetId": resolution["assetId"],
                    "assetType": resolution["assetType"],
                    "sourceRevision": state["sourceRevision"],
                    "samplerKey": "sampler." + hashlib.sha256(sampler_bytes).hexdigest().lower(),
                    "uvSet": 0,
                    "uvTransform": {"scaleU": 1.0, "scaleV": 1.0,
                                    "rotationDegrees": 0.0, "offsetU": 0.0,
                                    "offsetV": 0.0},
                    "colorInterpretation": node["colorInterpretation"],
                }
            resolved = dynamic_resolver_module._project_resolved_resource_bindings(
                projection["resourceRequirements"], bindings)
            return projection, resolved

        def compile_generic_image_through_dynamic_registry(document, catalog, states):
            projection = generic_image_module.project_resource_requirements(document, catalog)
            adapted_document = generic_image_module.adapt_document_to_dynamic_registry(
                document, projection)
            resolved_bindings = generic_image_module.build_dynamic_resolved_bindings(
                document, projection, states)
            return dynamic_resolver_module.compile_document_from_data(
                adapted_document, dynamic_adapter_registry,
                resolved_resource_bindings=resolved_bindings)

        regenerated_generic_image_candidate = generic_image_module.build_candidate(
            generic_image_document, generic_image_catalog, generic_image_source_states)
        generic_image_projection = generic_image_module.project_resource_requirements(
            generic_image_document, generic_image_catalog)
        generic_image_binding_projection = generic_image_module.project_resource_bindings(
            generic_image_document, generic_image_projection, generic_image_source_states)
        generic_image_dynamic_projection, generic_image_dynamic_binding = (
            project_generic_image_through_dynamic_resolver(
                generic_image_document, generic_image_catalog, generic_image_source_states))
        generic_image_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            generic_image_document, generic_image_catalog, generic_image_source_states)
        shared_resource_generic_document = copy.deepcopy(generic_image_document)
        shared_resource_generic_node = next(
            node for node in shared_resource_generic_document["nodes"]
            if node["nodeId"] == "roughness_sample")
        shared_resource_generic_node["resourceReferences"]["image"] = "image.albedo"
        shared_resource_generic_projection = generic_image_module.project_resource_requirements(
            shared_resource_generic_document, generic_image_catalog)
        shared_resource_generic_binding = generic_image_module.project_resource_bindings(
            shared_resource_generic_document, shared_resource_generic_projection,
            generic_image_source_states)
        shared_resource_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            shared_resource_generic_document, generic_image_catalog,
            generic_image_source_states)
        shared_resource_generic_dynamic_document = (
            generic_image_module.adapt_document_to_dynamic_registry(
                shared_resource_generic_document, shared_resource_generic_projection))
        generic_image_adapted_document = generic_image_module.adapt_document_to_dynamic_registry(
            generic_image_document, generic_image_projection)
        generic_image_resolver_projection = {
            key: generic_image_dynamic_candidate[key] for key in (
                "resourceBindingStatus", "resolvedResourceBindings",
                "unresolvedResourceKeys", "resourceBindingKeySha256")}
        regenerated_generic_image_candidate["dynamicResolverBindingAdapter"] = {
            "status": "generic-node-document-adapted-and-lowered-by-dynamic-prototype",
            "dynamicDocument": generic_image_adapted_document,
            "projection": generic_image_resolver_projection,
            "compilerCandidate": generic_image_dynamic_candidate,
            "sharedResourceSelection": {
                "dynamicDocument": shared_resource_generic_dynamic_document,
                "compilerCandidate": shared_resource_generic_dynamic_candidate,
                "contract": "two image nodes may share one resourceId while retaining node-scoped resource keys and shader symbols",
            },
            "adapterPolicy": {
                "samplerKey": "SHA-256 of canonical authored sampler descriptor, prefixed sampler.",
                "uv": "UV remains an explicit graph link; this fixture maps the UV0 source to set 0 and identity transform only as resolver compatibility metadata.",
                "scope": "The translated graph persists node resourceReference fields and the document resourceId-to-Asset-UUID table. Asset type/source revision resolution, shader compilation, and GPU binding remain host/runtime responsibilities."
            }
        }
        mapped_generic_projection = generic_image_module.project_resource_requirements(
            generic_image_mapped_uv_document, generic_image_catalog)
        mapped_generic_dynamic_document = generic_image_module.adapt_document_to_dynamic_registry(
            generic_image_mapped_uv_document, mapped_generic_projection)
        mapped_generic_dynamic_bindings = generic_image_module.build_dynamic_resolved_bindings(
            generic_image_mapped_uv_document, mapped_generic_projection,
            generic_image_source_states)
        mapped_generic_dynamic_candidate = dynamic_resolver_module.compile_document_from_data(
            mapped_generic_dynamic_document, dynamic_adapter_registry,
            resolved_resource_bindings=mapped_generic_dynamic_bindings)
        direct_uv_generic_document = copy.deepcopy(generic_image_mapped_uv_document)
        direct_uv_generic_document["nodes"] = [
            node for node in direct_uv_generic_document["nodes"]
            if node["nodeId"] != "mapping"]
        direct_uv_generic_document["links"] = [
            {"from": {"nodeId": "uv", "socketId": "value"},
             "to": {"nodeId": "image_sample", "socketId": "uv"}}]
        direct_uv_generic_projection = generic_image_module.project_resource_requirements(
            direct_uv_generic_document, generic_image_catalog)
        direct_uv_generic_dynamic_document = generic_image_module.adapt_document_to_dynamic_registry(
            direct_uv_generic_document, direct_uv_generic_projection)
        direct_uv_generic_bindings = generic_image_module.build_dynamic_resolved_bindings(
            direct_uv_generic_document, direct_uv_generic_projection,
            generic_image_source_states)
        direct_uv_generic_dynamic_candidate = dynamic_resolver_module.compile_document_from_data(
            direct_uv_generic_dynamic_document, dynamic_adapter_registry,
            resolved_resource_bindings=direct_uv_generic_bindings)
        regenerated_generic_image_candidate["mappedUvDynamicResolverAdapter"] = {
            "dynamicDocument": mapped_generic_dynamic_document,
            "resourceProjection": mapped_generic_projection,
            "compilerCandidate": mapped_generic_dynamic_candidate,
            "directUvCompilerCandidate": direct_uv_generic_dynamic_candidate,
            "contract": "Mapping is retained as a graph operation and its output reaches the generic Image Texture UV input; resource and resolved binding identities match the direct-UV graph",
        }
        relinked_generic_image_document = copy.deepcopy(generic_image_document)
        relinked_generic_asset_id = "22222222-2222-4222-8222-222222222222"
        relinked_generic_image_document["resources"][0]["source"]["assetId"] = relinked_generic_asset_id
        relinked_generic_image_candidate = generic_image_module.project_resource_requirements(
            relinked_generic_image_document,
            {relinked_generic_asset_id: "Image",
             "33333333-3333-4333-8333-333333333333": "Image"})
        relinked_generic_source_states = copy.deepcopy(generic_image_source_states)
        relinked_generic_source_states.pop("11111111-1111-4111-8111-111111111111")
        relinked_generic_source_states[relinked_generic_asset_id] = {"sourceRevision": 1}
        relinked_generic_binding_projection = generic_image_module.project_resource_bindings(
            relinked_generic_image_document, relinked_generic_image_candidate,
            relinked_generic_source_states)
        _, relinked_generic_dynamic_binding = project_generic_image_through_dynamic_resolver(
            relinked_generic_image_document,
            {relinked_generic_asset_id: "Image",
             "33333333-3333-4333-8333-333333333333": "Image"},
            relinked_generic_source_states)
        relinked_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            relinked_generic_image_document,
            {relinked_generic_asset_id: "Image",
             "33333333-3333-4333-8333-333333333333": "Image"},
            relinked_generic_source_states)
        revision_changed_generic_source_states = copy.deepcopy(generic_image_source_states)
        revision_changed_generic_source_states["33333333-3333-4333-8333-333333333333"][
            "sourceRevision"] += 1
        revision_changed_generic_binding_projection = generic_image_module.project_resource_bindings(
            generic_image_document, generic_image_projection,
            revision_changed_generic_source_states)
        _, revision_changed_generic_dynamic_binding = project_generic_image_through_dynamic_resolver(
            generic_image_document, generic_image_catalog,
            revision_changed_generic_source_states)
        revision_changed_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            generic_image_document, generic_image_catalog,
            revision_changed_generic_source_states)
        sampler_changed_generic_document = copy.deepcopy(generic_image_document)
        sampler_changed_generic_document["nodes"][1]["sampler"]["minFilter"] = "nearest"
        sampler_changed_generic_projection = generic_image_module.project_resource_requirements(
            sampler_changed_generic_document, generic_image_catalog)
        sampler_changed_generic_binding_projection = generic_image_module.project_resource_bindings(
            sampler_changed_generic_document, sampler_changed_generic_projection,
            generic_image_source_states)
        _, sampler_changed_generic_dynamic_binding = project_generic_image_through_dynamic_resolver(
            sampler_changed_generic_document, generic_image_catalog,
            generic_image_source_states)
        sampler_changed_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            sampler_changed_generic_document, generic_image_catalog,
            generic_image_source_states)
        color_changed_generic_document = copy.deepcopy(generic_image_document)
        color_changed_generic_document["nodes"][1]["colorInterpretation"] = "linear"
        color_changed_generic_projection = generic_image_module.project_resource_requirements(
            color_changed_generic_document, generic_image_catalog)
        color_changed_generic_binding_projection = generic_image_module.project_resource_bindings(
            color_changed_generic_document, color_changed_generic_projection,
            generic_image_source_states)
        _, color_changed_generic_dynamic_binding = project_generic_image_through_dynamic_resolver(
            color_changed_generic_document, generic_image_catalog,
            generic_image_source_states)
        color_changed_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            color_changed_generic_document, generic_image_catalog,
            generic_image_source_states)
        rewired_uv_generic_document = copy.deepcopy(generic_image_document)
        next(node for node in rewired_uv_generic_document["nodes"]
             if node["nodeId"] == "uv")["nodeId"] = "uv_rewired"
        for generic_link in rewired_uv_generic_document["links"]:
            if generic_link["from"]["nodeId"] == "uv":
                generic_link["from"]["nodeId"] = "uv_rewired"
        rewired_uv_generic_projection, rewired_uv_generic_dynamic_binding = (
            project_generic_image_through_dynamic_resolver(
                rewired_uv_generic_document, generic_image_catalog,
                generic_image_source_states))
        rewired_uv_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            rewired_uv_generic_document, generic_image_catalog,
            generic_image_source_states)
        unresolved_generic_image_candidate = generic_image_module.project_resource_requirements(
            generic_image_document, {})
        unresolved_generic_binding_projection = generic_image_module.project_resource_bindings(
            generic_image_document, generic_image_projection, {})
        _, unresolved_generic_dynamic_binding = project_generic_image_through_dynamic_resolver(
            generic_image_document, generic_image_catalog, {})
        unresolved_generic_dynamic_candidate = compile_generic_image_through_dynamic_registry(
            generic_image_document, generic_image_catalog, {})
        dangling_generic_resource_document = copy.deepcopy(generic_image_document)
        dangling_generic_resource_document["nodes"][1]["resourceReferences"]["image"] = "image.missing"
        dangling_generic_resource_diagnostic = None
        try:
            generic_image_module.project_resource_requirements(
                dangling_generic_resource_document, generic_image_catalog)
        except generic_image_module.ContractError as error:
            dangling_generic_resource_diagnostic = error.to_json()
        unsupported_generic_schema_document = copy.deepcopy(generic_image_document)
        unsupported_generic_schema_document["schemaVersion"] = 3
        unsupported_generic_schema_diagnostic = None
        try:
            generic_image_module.project_resource_requirements(
                unsupported_generic_schema_document, generic_image_catalog)
        except generic_image_module.ContractError as error:
            unsupported_generic_schema_diagnostic = error.to_json()
        malformed_generic_endpoint_document = copy.deepcopy(generic_image_document)
        malformed_generic_endpoint_document["links"][0]["from"]["nodeId"] = []
        malformed_generic_endpoint_diagnostic = None
        try:
            generic_image_module.project_resource_requirements(
                malformed_generic_endpoint_document, generic_image_catalog)
        except generic_image_module.ContractError as error:
            malformed_generic_endpoint_diagnostic = error.to_json()
        invalid_generic_sampler_document = copy.deepcopy(generic_image_document)
        invalid_generic_sampler_document["nodes"][1]["sampler"]["maxAnisotropy"] = 0
        invalid_generic_sampler_diagnostic = None
        try:
            generic_image_module.project_resource_requirements(
                invalid_generic_sampler_document, generic_image_catalog)
        except generic_image_module.ContractError as error:
            invalid_generic_sampler_diagnostic = error.to_json()
        invalid_generic_source_states = copy.deepcopy(generic_image_source_states)
        invalid_generic_source_states["11111111-1111-4111-8111-111111111111"][
            "sourceRevision"] = True
        invalid_generic_source_revision_diagnostic = None
        try:
            generic_image_module.project_resource_bindings(
                generic_image_document, generic_image_projection, invalid_generic_source_states)
        except generic_image_module.ContractError as error:
            invalid_generic_source_revision_diagnostic = error.to_json()
        incompatible_generic_image_diagnostic = None
        try:
            generic_image_module.project_resource_requirements(
                generic_image_document,
                {"11111111-1111-4111-8111-111111111111": "Model"})
        except generic_image_module.ContractError as error:
            incompatible_generic_image_diagnostic = error.to_json()
        dynamic_spec = importlib.util.spec_from_file_location("shader_graph_dynamic_prototype", dynamic_prototype_path)
        if dynamic_spec is None or dynamic_spec.loader is None:
            raise RuntimeError("cannot load dynamic graph prototype")
        dynamic_module = importlib.util.module_from_spec(dynamic_spec)
        sys.modules[dynamic_spec.name] = dynamic_module
        dynamic_spec.loader.exec_module(dynamic_module)

        def diagnostic_for(action) -> dict[str, str] | None:
            try:
                action()
            except dynamic_module.ContractError as error:
                return {key: error.to_json()[key] for key in ("code", "severity", "path")}
            return None

        local_ui_module_name = "shader_graph_local_ui"
        local_ui_spec = importlib.util.spec_from_file_location(
            local_ui_module_name, dynamic_prototype_path.with_name("local_graph_lab.py"))
        if local_ui_spec is None or local_ui_spec.loader is None:
            raise RuntimeError("cannot load local graph UI session")
        prior_dynamic_alias = sys.modules.get("dynamic_graph_prototype")
        sys.modules["dynamic_graph_prototype"] = dynamic_module
        local_ui_module = importlib.util.module_from_spec(local_ui_spec)
        sys.modules[local_ui_spec.name] = local_ui_module
        local_ui_spec.loader.exec_module(local_ui_module)
        local_ui_html_source = dynamic_prototype_path.with_name("local_graph_lab.html").read_text(encoding="utf-8")
        if prior_dynamic_alias is None:
            del sys.modules["dynamic_graph_prototype"]
        else:
            sys.modules["dynamic_graph_prototype"] = prior_dynamic_alias
        dynamic_registry = dynamic_module.load_registry(dynamic_registry_path)
        local_image_resolver_contract = dynamic_contract.get(
            "localImageResourceResolverSnapshotContract", {})
        local_image_document_path = root / local_image_resolver_contract.get(
            "documentFixture", "")
        local_image_catalog_path = root / local_image_resolver_contract.get(
            "assetCatalogFixture", "")
        local_image_source_states_path = root / local_image_resolver_contract.get(
            "assetSourceStatesFixture", "")
        local_image_document, _ = dynamic_module.load_document_snapshot(
            local_image_document_path)
        local_image_catalog, local_image_catalog_hash = (
            local_ui_module.load_asset_catalog_snapshot(local_image_catalog_path))
        local_image_source_states, local_image_source_states_hash = (
            local_ui_module.load_asset_source_states(local_image_source_states_path))
        local_image_session = local_ui_module.Session(
            dynamic_registry, local_image_document, None, [],
            asset_catalog=local_image_catalog,
            asset_catalog_sha256=local_image_catalog_hash,
            asset_source_states=local_image_source_states,
            asset_source_states_sha256=local_image_source_states_hash)
        local_image_resolved_state = local_image_session.state()
        local_image_catalog_only_session = local_ui_module.Session(
            dynamic_registry, local_image_document, None, [],
            asset_catalog=local_image_catalog,
            asset_catalog_sha256=local_image_catalog_hash)
        local_image_catalog_only_state = local_image_catalog_only_session.state()
        local_image_no_snapshot_state = local_ui_module.Session(
            dynamic_registry, local_image_document, None, []).state()
        local_image_changed_source_states = copy.deepcopy(local_image_source_states)
        local_image_changed_source_states[
            local_image_resolver_contract["assetId"]]["sourceRevision"] += 1
        local_image_changed_source_states_hash = hashlib.sha256(json.dumps(
            local_image_changed_source_states, sort_keys=True, separators=(",", ":"),
            ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest().upper()
        local_image_changed_revision_state = local_ui_module.Session(
            dynamic_registry, local_image_document, None, [],
            asset_catalog=local_image_catalog,
            asset_catalog_sha256=local_image_catalog_hash,
            asset_source_states=local_image_changed_source_states,
            asset_source_states_sha256=local_image_changed_source_states_hash).state()
        local_image_resolved_compilation = local_image_resolved_state["compilation"]
        local_image_changed_compilation = local_image_changed_revision_state["compilation"]
        local_image_resolved_bindings = local_image_resolved_compilation.get(
            "resolvedResourceBindings", [])
        local_image_expected_binding = next((binding for binding in local_image_resolved_bindings
                                             if binding.get("resourceKey") ==
                                             local_image_resolver_contract.get("resourceKey")), {})
        local_image_mapping_edit = dynamic_module.edit_node_parameters(
            local_image_document, dynamic_registry, "mapping", {"scaleX": 3.0})
        local_image_mapping_edit_state = local_ui_module.Session(
            dynamic_registry, local_image_mapping_edit, None, [],
            asset_catalog=local_image_catalog,
            asset_catalog_sha256=local_image_catalog_hash,
            asset_source_states=local_image_source_states,
            asset_source_states_sha256=local_image_source_states_hash).state()
        local_image_mapping_edit_compilation = local_image_mapping_edit_state["compilation"]
        uv_node = copy.deepcopy(next(node for node in local_image_document["nodes"]
                                     if node.get("operationId") ==
                                     "artifact.input.texcoord.uv0.vector2"))
        image_node = copy.deepcopy(next(node for node in local_image_document["nodes"]
                                        if node.get("operationId") ==
                                        "artifact.texture.sample.image2d"))
        mapping_operation = dynamic_registry[("artifact.mapping.vector2.scale_offset", 1)]
        long_mapping_nodes = [dynamic_module.instantiate_node(
            mapping_operation, f"mapping_{index:04d}") for index in range(1500)]
        long_mapping_links = [{"from": {"nodeId": uv_node["nodeId"], "socketId": "value"},
                               "to": {"nodeId": long_mapping_nodes[0]["nodeId"], "socketId": "vector"}}]
        long_mapping_links.extend(
            {"from": {"nodeId": source["nodeId"], "socketId": "vector"},
             "to": {"nodeId": target["nodeId"], "socketId": "vector"}}
            for source, target in zip(long_mapping_nodes, long_mapping_nodes[1:]))
        long_mapping_links.append({
            "from": {"nodeId": long_mapping_nodes[-1]["nodeId"], "socketId": "vector"},
            "to": {"nodeId": image_node["nodeId"], "socketId": "uv"}})
        long_mapping_document = copy.deepcopy(local_image_document)
        long_mapping_document["nodes"] = [uv_node, *long_mapping_nodes, image_node]
        long_mapping_document["links"] = long_mapping_links
        long_mapping_session = local_ui_module.Session(
            dynamic_registry, long_mapping_document, None, [],
            asset_catalog=local_image_catalog,
            asset_catalog_sha256=local_image_catalog_hash,
            asset_source_states=local_image_source_states,
            asset_source_states_sha256=local_image_source_states_hash)
        long_mapping_bindings = long_mapping_session._resolved_resource_bindings()
        long_mapping_binding = long_mapping_bindings.get(
            local_image_resolver_contract.get("resourceKey"), {})
        def snapshot_loader_error(loader, path):
            try:
                loader(path)
            except dynamic_module.ContractError as error:
                return error.code
            return None
        with tempfile.TemporaryDirectory(prefix="shader-graph-rd-asset-snapshot-") as snapshot_directory:
            snapshot_directory_path = Path(snapshot_directory)
            oversized_catalog_path = snapshot_directory_path / "oversized-catalog.json"
            with oversized_catalog_path.open("wb") as oversized_catalog_file:
                oversized_catalog_file.truncate(local_ui_module.MAX_ASSET_CATALOG_BYTES + 1)
            oversized_source_states_path = snapshot_directory_path / "oversized-source-states.json"
            with oversized_source_states_path.open("wb") as oversized_source_states_file:
                oversized_source_states_file.truncate(
                    local_ui_module.MAX_ASSET_SOURCE_STATES_BYTES + 1)
            deeply_nested_snapshot = b"[" * 4096 + b"]" * 4096
            nested_catalog_path = snapshot_directory_path / "nested-catalog.json"
            nested_catalog_path.write_bytes(deeply_nested_snapshot)
            nested_source_states_path = snapshot_directory_path / "nested-source-states.json"
            nested_source_states_path.write_bytes(deeply_nested_snapshot)
            oversized_catalog_diagnostic = snapshot_loader_error(
                local_ui_module.load_asset_catalog_snapshot, oversized_catalog_path)
            oversized_source_states_diagnostic = snapshot_loader_error(
                local_ui_module.load_asset_source_states, oversized_source_states_path)
            nested_catalog_diagnostic = snapshot_loader_error(
                local_ui_module.load_asset_catalog_snapshot, nested_catalog_path)
            nested_source_states_diagnostic = snapshot_loader_error(
                local_ui_module.load_asset_source_states, nested_source_states_path)
        image_sample_operation = dynamic_registry[("artifact.texture.sample.image2d", 1)]
        incomplete_image_node = dynamic_module.instantiate_node(
            image_sample_operation, "incomplete_image")
        incomplete_image_document = {
            "schemaVersion": 1,
            "kind": "artifact-shader-graph-rd-document",
            "nodes": [incomplete_image_node], "links": [],
            "output": {"nodeId": "incomplete_image", "socketId": "color"},
        }
        incomplete_image_validation = dynamic_module.validate_document_for_editing(
            incomplete_image_document, dynamic_registry)
        incomplete_image_compile_error = None
        try:
            dynamic_module.compile_document_from_data(
                incomplete_image_document, dynamic_registry)
        except dynamic_module.ContractError as error:
            incomplete_image_compile_error = error.to_json()
        generic_image_dynamic_document = generic_image_module.adapt_document_to_dynamic_registry(
            generic_image_document, generic_image_projection)
        generic_image_dynamic_bindings = generic_image_module.build_dynamic_resolved_bindings(
            generic_image_document, generic_image_projection, generic_image_source_states)
        generic_image_missing_asset_validation = dynamic_module.validate_document_for_editing(
            generic_image_dynamic_document, dynamic_registry,
            resolved_resource_bindings={})
        generic_image_resolved_asset_validation = dynamic_module.validate_document_for_editing(
            generic_image_dynamic_document, dynamic_registry,
            resolved_resource_bindings=generic_image_dynamic_bindings)
        mismatched_image_bindings = copy.deepcopy(generic_image_dynamic_bindings)
        mismatched_image_bindings["artifact.shadergraph.image.image_sample"][
            "samplerKey"] = "sampler.stale"
        mismatched_image_binding_error = None
        try:
            dynamic_module.compile_document_from_data(
                generic_image_dynamic_document, dynamic_registry,
                resolved_resource_bindings=mismatched_image_bindings)
        except dynamic_module.ContractError as error:
            mismatched_image_binding_error = error.to_json()
        generic_image_reference = next(
            node["resourceReference"] for node in generic_image_dynamic_document["nodes"]
            if node["nodeId"] == "image_sample")
        generic_image_roundtrip = dynamic_module.parse_json_text(
            dynamic_module.serialize_document(generic_image_dynamic_document))
        generic_image_roundtrip_reference = next(
            node["resourceReference"] for node in generic_image_roundtrip["nodes"]
            if node["nodeId"] == "image_sample")
        generic_image_layout_edited = dynamic_module.edit_node_layout(
            generic_image_dynamic_document, "image_sample", {"x": 300.0})
        generic_image_command_document = copy.deepcopy(generic_image_dynamic_document)
        relinked_image_reference = copy.deepcopy(generic_image_reference)
        relinked_image_reference["resourceId"] = "image.roughness"
        generic_image_command_document = dynamic_module.edit_node_resource_reference(
            generic_image_dynamic_document, dynamic_registry, "image_sample",
            relinked_image_reference)
        generic_image_document_command = dynamic_module.create_document_command(
            "Relink graph image", generic_image_dynamic_document,
            generic_image_command_document)
        generic_image_undo_document = dynamic_module.apply_document_command(
            generic_image_document_command, generic_image_command_document, direction="undo")
        generic_image_redo_document = dynamic_module.apply_document_command(
            generic_image_document_command, generic_image_undo_document, direction="redo")
        asset_relinked_dynamic_document = dynamic_module.set_graph_resource_asset(
            generic_image_dynamic_document, "image.albedo",
            "22222222-2222-4222-8222-222222222222")
        asset_relink_command = dynamic_module.create_document_command(
            "Relink graph resource", generic_image_dynamic_document,
            asset_relinked_dynamic_document)
        asset_relink_undo_document = dynamic_module.apply_document_command(
            asset_relink_command, asset_relinked_dynamic_document, direction="undo")
        asset_relink_redo_document = dynamic_module.apply_document_command(
            asset_relink_command, asset_relink_undo_document, direction="redo")
        asset_relinked_bindings = copy.deepcopy(generic_image_dynamic_bindings)
        asset_relinked_bindings["artifact.shadergraph.image.image_sample"]["assetId"] = (
            "22222222-2222-4222-8222-222222222222")
        stale_asset_binding_error = None
        try:
            dynamic_module.compile_document_from_data(
                asset_relinked_dynamic_document, dynamic_registry,
                resolved_resource_bindings=generic_image_dynamic_bindings)
        except dynamic_module.ContractError as error:
            stale_asset_binding_error = error.to_json()
        asset_relinked_compile = dynamic_module.compile_document_from_data(
            asset_relinked_dynamic_document, dynamic_registry,
            resolved_resource_bindings=asset_relinked_bindings)
        generic_image_clipboard = dynamic_module.copy_nodes_subgraph(
            generic_image_dynamic_document, ["uv", "image_sample"], dynamic_registry)
        generic_image_pasted_document = dynamic_module.paste_nodes_subgraph(
            generic_image_dynamic_document, dynamic_registry, generic_image_clipboard,
            {"uv": "uv_pasted", "image_sample": "image_pasted"})
        clipboard_target_node = dynamic_module.instantiate_node(
            dynamic_registry[("artifact.rd.constant.float32", 1)], "clipboard_target")
        clipboard_target_document = {
            "schemaVersion": 1,
            "kind": "artifact-shader-graph-rd-document",
            "nodes": [clipboard_target_node], "links": [],
            "output": {"nodeId": "clipboard_target", "socketId": "value"},
        }
        portable_image_paste = dynamic_module.paste_nodes_subgraph(
            clipboard_target_document, dynamic_registry, generic_image_clipboard,
            {"uv": "uv_portable", "image_sample": "image_portable"})
        conflicting_clipboard_target = copy.deepcopy(clipboard_target_document)
        conflicting_clipboard_target["resources"] = [{
            "resourceId": "image.albedo", "resourceKind": "sampledTexture2D",
            "source": {"kind": "projectAsset", "assetId": "99999999-9999-4999-8999-999999999999"},
        }]
        resource_conflict_paste_error = None
        try:
            dynamic_module.paste_nodes_subgraph(
                conflicting_clipboard_target, dynamic_registry, generic_image_clipboard,
                {"uv": "uv_conflict", "image_sample": "image_conflict"})
        except dynamic_module.ContractError as error:
            resource_conflict_paste_error = error.to_json()
        generic_image_duplicated_document = dynamic_module.duplicate_nodes_batch(
            generic_image_dynamic_document, dynamic_registry,
            [{"sourceNodeId": "uv", "nodeId": "uv_duplicate"},
             {"sourceNodeId": "image_sample", "nodeId": "image_duplicate"}])
        baseline_generic_image_compile = dynamic_module.compile_document_from_data(
            generic_image_dynamic_document, dynamic_registry,
            resolved_resource_bindings=generic_image_dynamic_bindings)
        sampler_edited_reference = copy.deepcopy(generic_image_reference)
        sampler_edited_reference["sampler"]["minFilter"] = "nearest"
        sampler_edited_document = dynamic_module.edit_node_resource_reference(
            generic_image_dynamic_document, dynamic_registry, "image_sample",
            sampler_edited_reference)
        sampler_edited_bindings = copy.deepcopy(generic_image_dynamic_bindings)
        sampler_edited_bytes = json.dumps(sampler_edited_reference["sampler"], sort_keys=True,
                                          separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        sampler_edited_bindings["artifact.shadergraph.image.image_sample"]["samplerKey"] = (
            "sampler." + hashlib.sha256(sampler_edited_bytes).hexdigest().lower())
        sampler_edited_compile = dynamic_module.compile_document_from_data(
            sampler_edited_document, dynamic_registry,
            resolved_resource_bindings=sampler_edited_bindings)
        color_edited_reference = copy.deepcopy(generic_image_reference)
        color_edited_reference["colorInterpretation"] = "linear"
        color_edited_document = dynamic_module.edit_node_resource_reference(
            generic_image_dynamic_document, dynamic_registry, "image_sample",
            color_edited_reference)
        color_edited_bindings = copy.deepcopy(generic_image_dynamic_bindings)
        color_edited_bindings["artifact.shadergraph.image.image_sample"][
            "colorInterpretation"] = "linear"
        color_edited_compile = dynamic_module.compile_document_from_data(
            color_edited_document, dynamic_registry,
            resolved_resource_bindings=color_edited_bindings)
        invalid_image_reference = copy.deepcopy(generic_image_reference)
        invalid_image_reference["sampler"]["minFilter"] = "cubic"
        invalid_image_edit_error = None
        try:
            dynamic_module.edit_node_resource_reference(
                generic_image_dynamic_document, dynamic_registry, "image_sample",
                invalid_image_reference)
        except dynamic_module.ContractError as error:
            invalid_image_edit_error = error.to_json()
        def generic_reference_for(graph, node_id):
            return next(node["resourceReference"] for node in graph["nodes"]
                        if node.get("nodeId") == node_id)
        generic_image_persistence_contract = {
            "missingAssetKeepsDocumentRepairableAndBlocksApplication": (
                generic_image_missing_asset_validation["status"] == "repairable" and
                not generic_image_missing_asset_validation["compilationAvailable"] and
                {item["path"] for item in generic_image_missing_asset_validation["diagnostics"]
                 if item["code"] == "resource.binding.unresolved"} == {
                    "/resourceBindings/artifact.shadergraph.image.image_sample",
                    "/resourceBindings/artifact.shadergraph.image.roughness_sample"} and
                generic_image_resolved_asset_validation["status"] == "valid" and
                generic_image_resolved_asset_validation["compilationAvailable"] and
                not generic_image_resolved_asset_validation["diagnostics"]),
            "projectAssetUuidStaysOutOfTypedIrAndHlsl": all(
                resource["source"]["assetId"] not in generic_image_dynamic_candidate["hlslBody"] and
                resource["source"]["assetId"] not in json.dumps(
                    generic_image_dynamic_candidate["typedIR"], sort_keys=True)
                for resource in generic_image_dynamic_document["resources"]),
            "sharedResourceIdKeepsPerNodeBindingAndShaderSymbols": (
                regenerated_generic_image_candidate.get("sharedResourceSelectionProjection", {}).get(
                    "resourceRequirements", []) ==
                shared_resource_generic_projection.get("resourceRequirements") and
                len({item.get("resourceKey") for item in
                     shared_resource_generic_dynamic_candidate.get("resourceRequirements", [])}) == 2 and
                {item.get("resourceId") for item in
                 shared_resource_generic_dynamic_candidate.get("resourceRequirements", [])} ==
                {"image.albedo"} and
                len({item.get("shaderSymbol") for item in
                     shared_resource_generic_dynamic_candidate.get("resourceRequirements", [])}) == 2 and
                len({item.get("assetId") for item in
                     shared_resource_generic_dynamic_candidate.get("resolvedResourceBindings", [])}) == 1 and
                shared_resource_generic_binding.get("resourceBindingStatus") == "resolved" and
                len(shared_resource_generic_binding.get("resolvedResourceBindings", [])) == 2 and
                shared_resource_generic_dynamic_candidate["hlslBody"] ==
                generic_image_dynamic_candidate["hlslBody"] and
                shared_resource_generic_dynamic_candidate["semanticHashSha256"] !=
                generic_image_dynamic_candidate["semanticHashSha256"] and
                shared_resource_generic_dynamic_candidate["resourceBindingKeySha256"] !=
                generic_image_dynamic_candidate["resourceBindingKeySha256"]),
            "resourceReferenceEditsAreAtomicAndKeepShaderSourceSeparateFromBindingState": (
                invalid_image_edit_error is not None and
                invalid_image_edit_error["code"] == "resource.sampler.invalid" and
                generic_reference_for(generic_image_dynamic_document, "image_sample") ==
                generic_image_reference and
                sampler_edited_compile["semanticHashSha256"] ==
                baseline_generic_image_compile["semanticHashSha256"] and
                sampler_edited_compile["hlslBody"] == baseline_generic_image_compile["hlslBody"] and
                sampler_edited_compile["resourceBindingKeySha256"] !=
                baseline_generic_image_compile["resourceBindingKeySha256"] and
                color_edited_compile["semanticHashSha256"] ==
                baseline_generic_image_compile["semanticHashSha256"] and
                color_edited_compile["hlslBody"] == baseline_generic_image_compile["hlslBody"] and
                color_edited_compile["resourceBindingKeySha256"] !=
                baseline_generic_image_compile["resourceBindingKeySha256"]),
            "resolvedBindingCannotOverridePersistedSamplerOrColor": (
                mismatched_image_binding_error is not None and
                mismatched_image_binding_error["code"] ==
                "resource.binding.authored_state.mismatch"),
            "unconfiguredImageCanBeEditedButCannotCompile": (
                incomplete_image_validation["status"] == "repairable" and
                any(item["code"] == "resource.reference.required" and
                    item["severity"] == "warning" and
                    item["path"] == "/nodes/0/resourceReference"
                    for item in incomplete_image_validation["diagnostics"]) and
                incomplete_image_compile_error is not None and
                incomplete_image_compile_error["code"] == "resource.reference.invalid"),
            "jsonRoundtripPreservesResourceReference":
                generic_image_roundtrip_reference == generic_image_reference,
            "nodeEditPreservesResourceReference":
                generic_reference_for(generic_image_layout_edited, "image_sample") ==
                generic_image_reference,
            "undoRedoPreservesResourceReference":
                generic_reference_for(generic_image_undo_document, "image_sample") == generic_image_reference and
                generic_reference_for(generic_image_redo_document, "image_sample")["resourceId"] == "image.roughness",
            "assetRelinkPersistsInResourceTableWithoutChangingShaderIdentity": (
                asset_relinked_dynamic_document["resources"][0]["source"]["assetId"] ==
                "22222222-2222-4222-8222-222222222222" and
                asset_relink_undo_document["resources"] ==
                generic_image_dynamic_document["resources"] and
                asset_relink_redo_document["resources"] ==
                asset_relinked_dynamic_document["resources"] and
                generic_reference_for(asset_relinked_dynamic_document, "image_sample") ==
                generic_image_reference and
                stale_asset_binding_error is not None and
                stale_asset_binding_error["code"] ==
                "resource.binding.asset_reference.mismatch" and
                asset_relinked_compile["semanticHashSha256"] ==
                baseline_generic_image_compile["semanticHashSha256"] and
                asset_relinked_compile["hlslBody"] == baseline_generic_image_compile["hlslBody"] and
                asset_relinked_compile["resourceBindingKeySha256"] !=
                baseline_generic_image_compile["resourceBindingKeySha256"]),
            "clipboardPastePreservesResourceIdAndRemapsNodeScopedKey": (
                generic_reference_for(generic_image_pasted_document, "image_pasted")["resourceId"] ==
                generic_image_reference["resourceId"] and
                generic_reference_for(generic_image_pasted_document, "image_pasted")["resourceKey"] ==
                "artifact.shadergraph.image.image_pasted"),
            "clipboardCarriesResourceTableAndRejectsConflictingRelink": (
                generic_image_clipboard["schemaVersion"] == 4 and
                len(generic_image_clipboard["resources"]) == 1 and
                generic_image_clipboard["resources"][0]["resourceId"] == "image.albedo" and
                portable_image_paste["resources"] == generic_image_clipboard["resources"] and
                dynamic_module.compile_document_from_data(
                    portable_image_paste, dynamic_registry).get("outputType") == "Float" and
                resource_conflict_paste_error is not None and
                resource_conflict_paste_error["code"] ==
                "resource.reference.resource.conflict"),
            "duplicatePreservesResourceIdAndRemapsNodeScopedKey": (
                generic_reference_for(generic_image_duplicated_document, "image_duplicate")["resourceId"] ==
                generic_image_reference["resourceId"] and
                generic_reference_for(generic_image_duplicated_document, "image_duplicate")["resourceKey"] ==
                "artifact.shadergraph.image.image_duplicate"),
            "roundtripClipboardAndDuplicateRemainCompilerValid": all(
                dynamic_module.compile_document_from_data(graph, dynamic_registry).get("outputType") == "Vector4"
                for graph in (generic_image_roundtrip, generic_image_pasted_document,
                              generic_image_duplicated_document)),
        }
        candidate_generator_source = (root / "tools/shader_graph_rd/generate_dynamic_graph_candidate.py").read_text(encoding="utf-8")
        constant_operation = dynamic_registry[("artifact.rd.constant.float32", 1)]
        created_constant_node = dynamic_module.instantiate_node(constant_operation, "created_constant", x=12.0, y=34.0)
        created_constant_document = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                                     "nodes": [created_constant_node], "links": [],
                                     "output": {"nodeId": "created_constant", "socketId": "value"}}
        created_constant_compile = dynamic_module.compile_document_from_data(created_constant_document, dynamic_registry)
        changed_constant_document = dynamic_module.edit_node_parameters(
            created_constant_document, dynamic_registry, "created_constant", {"value": 0.75})
        changed_constant_compile = dynamic_module.compile_document_from_data(changed_constant_document, dynamic_registry)
        add_operation_for_graph = dynamic_registry[("artifact.rd.math.add.float32", 1)]
        graph_nodes = [
            dynamic_module.instantiate_node(constant_operation, "source_left", x=0, y=0),
            dynamic_module.instantiate_node(constant_operation, "source_right", x=0, y=100),
            dynamic_module.instantiate_node(add_operation_for_graph, "sum_node", x=180, y=50),
        ]
        connected_graph = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                           "nodes": graph_nodes, "links": [],
                           "output": {"nodeId": "sum_node", "socketId": "result"}}
        connected_graph = dynamic_module.connect_nodes(connected_graph, dynamic_registry, "source_left", "value", "sum_node", "a")
        connected_graph = dynamic_module.connect_nodes(connected_graph, dynamic_registry, "source_right", "value", "sum_node", "b")
        connected_graph_compile = dynamic_module.compile_document_from_data(connected_graph, dynamic_registry)
        optional_b_document = copy.deepcopy(connected_graph)
        optional_b_node = next(node for node in optional_b_document["nodes"]
                               if node["nodeId"] == "sum_node")
        optional_b_node["operationId"] = "artifact.rd.math.add.float32.default_b"
        optional_b_document["links"] = [
            link for link in optional_b_document["links"]
            if link["to"] != {"nodeId": "sum_node", "socketId": "b"}
        ]
        optional_b_compile = dynamic_module.compile_document_from_data(
            optional_b_document, dynamic_registry)
        optional_b_ir = next(item for item in optional_b_compile["typedIR"]["operations"]
                             if item["nodeId"] == "sum_node")
        optional_b_input = next(item for item in optional_b_ir["inputs"]
                                if item["socketId"] == "b")
        optional_b_override_document = dynamic_module.connect_nodes(
            optional_b_document, dynamic_registry,
            "source_right", "value", "sum_node", "b")
        optional_b_override_compile = dynamic_module.compile_document_from_data(
            optional_b_override_document, dynamic_registry)
        socket_default_edit_document = dynamic_module.edit_node_socket_default(
            optional_b_document, dynamic_registry, "sum_node", "b", 0.25)
        socket_default_roundtrip = dynamic_module.parse_json_text(
            dynamic_module.serialize_document(socket_default_edit_document))
        socket_default_edit_compile = dynamic_module.compile_document_from_data(
            socket_default_roundtrip, dynamic_registry)
        socket_default_command = dynamic_module.create_document_command(
            "Edit socket default", optional_b_document, socket_default_edit_document)
        socket_default_undo = dynamic_module.apply_document_command(
            socket_default_command, socket_default_edit_document, direction="undo")
        socket_default_redo = dynamic_module.apply_document_command(
            socket_default_command, socket_default_undo, direction="redo")
        socket_default_clipboard = dynamic_module.copy_nodes_subgraph(
            socket_default_edit_document, ["sum_node"], dynamic_registry)
        socket_default_paste = dynamic_module.paste_nodes_subgraph(
            optional_b_document, dynamic_registry, socket_default_clipboard,
            {"sum_node": "sum_copy"})
        linked_socket_default_baseline_document = dynamic_module.edit_node_socket_default(
            optional_b_override_document, dynamic_registry, "sum_node", "b", 0.0)
        linked_socket_default_baseline_compile = dynamic_module.compile_document_from_data(
            linked_socket_default_baseline_document, dynamic_registry)
        linked_socket_default_edit_document = dynamic_module.edit_node_socket_default(
            linked_socket_default_baseline_document, dynamic_registry, "sum_node", "b", 0.75)
        linked_socket_default_edit_compile = dynamic_module.compile_document_from_data(
            linked_socket_default_edit_document, dynamic_registry)
        required_socket_default_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_socket_default(
            optional_b_document, dynamic_registry, "sum_node", "a", 0.25))
        overflow_socket_default_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_socket_default(
            optional_b_document, dynamic_registry, "sum_node", "b", 1e100))
        schema2_socket_default_document = copy.deepcopy(optional_b_document)
        schema2_socket_default_document["schemaVersion"] = 2
        schema2_socket_default_document["graphParameters"] = []
        schema2_socket_default_edit = dynamic_module.edit_node_socket_default(
            schema2_socket_default_document, dynamic_registry, "sum_node", "b", 0.5)
        changed_optional_default_registry = dict(dynamic_registry)
        default_b_operation = changed_optional_default_registry[
            ("artifact.rd.math.add.float32.default_b", 1)]
        changed_optional_default_registry[
            ("artifact.rd.math.add.float32.default_b", 1)] = replace(
                default_b_operation,
                inputs=(default_b_operation.inputs[0],
                        replace(default_b_operation.inputs[1], default=0.5)))
        changed_optional_default_compile = dynamic_module.compile_document_from_data(
            optional_b_document, changed_optional_default_registry)
        changed_optional_default_connected_compile = dynamic_module.compile_document_from_data(
            optional_b_override_document, changed_optional_default_registry)
        required_input_missing_document = copy.deepcopy(optional_b_document)
        required_input_missing_document["links"] = []
        required_input_missing_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(
            required_input_missing_document, dynamic_registry))
        socket_default_ui_document = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                                      "nodes": [dynamic_module.instantiate_node(
                                          dynamic_registry[("artifact.rd.math.add.float32.default_b", 1)],
                                          "ui_optional")],
                                      "links": [], "output": {"nodeId": "ui_optional", "socketId": "result"}}
        socket_default_ui_session = local_ui_module.Session(
            dynamic_registry, socket_default_ui_document, None, [])
        socket_default_ui_session.apply({"type": "socketDefault", "nodeId": "ui_optional",
                                         "socketId": "b", "value": 0.625})
        vector4_socket_default_ui_session = local_ui_module.Session(
            dynamic_registry, dynamic_optional_vector4_default_document, None, [])
        vector4_socket_default_ui_session.apply({
            "type": "socketDefault", "nodeId": "sum", "socketId": "b",
            "value": [0.7, 0.6, 0.5, 0.4]})
        vector4_default_wrong_arity_diagnostic = diagnostic_for(
            lambda: dynamic_module.edit_node_socket_default(
                dynamic_optional_vector4_default_document, dynamic_registry, "sum", "b",
                [0.1, 0.2, 0.3]))
        vector4_default_overflow_diagnostic = diagnostic_for(
            lambda: dynamic_module.edit_node_socket_default(
                dynamic_optional_vector4_default_document, dynamic_registry, "sum", "b",
                [0.1, 0.2, 1e100, 0.4]))
        vector4_default_connected_document = dynamic_module.connect_nodes(
            dynamic_optional_vector4_default_document, dynamic_registry,
            "source", "value", "sum", "b")
        vector4_default_connected_baseline = dynamic_module.compile_document_from_data(
            vector4_default_connected_document, dynamic_registry)
        vector4_default_connected_edited_document = dynamic_module.edit_node_socket_default(
            vector4_default_connected_document, dynamic_registry, "sum", "b",
            [0.9, 0.8, 0.7, 0.6])
        vector4_default_connected_edited = dynamic_module.compile_document_from_data(
            vector4_default_connected_edited_document, dynamic_registry)
        ui_empty_document = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                             "nodes": [], "links": [], "output": {"nodeId": "", "socketId": ""}}
        ui_session = local_ui_module.Session(dynamic_registry, ui_empty_document, None, [])
        ui_session.apply({"type": "add", "operationId": "artifact.rd.constant.float32",
                          "semanticVersion": 1, "nodeId": "ui_left", "x": 32, "y": 48})
        ui_session.apply({"type": "parameter", "nodeId": "ui_left", "values": {"value": 0.25}})
        ui_session.apply({"type": "add", "operationId": "artifact.rd.constant.float32",
                          "semanticVersion": 1, "nodeId": "ui_right", "x": 32, "y": 148})
        ui_session.apply({"type": "parameter", "nodeId": "ui_right", "values": {"value": 0.5}})
        ui_session.apply({"type": "add", "operationId": "artifact.rd.math.add.float32",
                          "semanticVersion": 1, "nodeId": "ui_sum", "x": 260, "y": 96})
        ui_session.apply({"type": "connect", "fromNode": "ui_left", "fromSocket": "value",
                          "toNode": "ui_sum", "toSocket": "a"})
        ui_session.apply({"type": "connect", "fromNode": "ui_right", "fromSocket": "value",
                          "toNode": "ui_sum", "toSocket": "b"})
        ui_session.apply({"type": "output", "nodeId": "ui_sum", "socketId": "result"})
        ui_session_document = dynamic_module.preserve_document(ui_session.document)
        ui_session_before_reject = dynamic_module.preserve_document(ui_session.document)
        ui_session_history_before_reject = (len(ui_session.history), ui_session.cursor)
        try:
            ui_session.apply({"type": "connect", "fromNode": "ui_left", "fromSocket": "value",
                              "toNode": "ui_sum", "toSocket": "missing"})
            ui_session_rejected_connect = None
        except dynamic_module.ContractError as error:
            ui_session_rejected_connect = {key: error.to_json()[key] for key in ("code", "severity", "path")}
        ui_session_after_reject = dynamic_module.preserve_document(ui_session.document)
        ui_session_history_after_reject = (len(ui_session.history), ui_session.cursor)
        ui_session_undo = ui_session.apply({"type": "undo"})
        ui_session_undo_document = dynamic_module.preserve_document(ui_session.document)
        ui_session_redo = ui_session.apply({"type": "redo"})
        ui_session_redo_document = dynamic_module.preserve_document(ui_session.document)
        with tempfile.TemporaryDirectory(prefix="shader-graph-rd-ui-session-") as ui_directory:
            ui_session.document_path = Path(ui_directory) / "ui-graph.json"
            ui_session.apply({"type": "save"})
            ui_session_saved = dynamic_module.load_document(ui_session.document_path)
        import threading
        from urllib.error import HTTPError
        from urllib.request import Request, urlopen
        http_session = local_ui_module.Session(dynamic_registry, connected_graph, None, [])
        http_page = dynamic_prototype_path.with_name("local_graph_lab.html").read_bytes()
        http_server = local_ui_module.HTTPServer(
            ("127.0.0.1", 0), local_ui_module.make_handler(http_session, http_page))
        http_handler = http_server.RequestHandlerClass
        http_handler.log_message = lambda self, format, *args: None
        http_thread = threading.Thread(target=http_server.serve_forever, daemon=True)
        http_thread.start()
        def ui_http_request(route: str, payload: dict[str, Any] | None = None) -> tuple[int, bytes]:
            request_data = None if payload is None else json.dumps(payload).encode("utf-8")
            request = Request(f"http://127.0.0.1:{http_server.server_port}{route}", data=request_data,
                              headers={"Content-Type": "application/json"} if request_data is not None else {})
            try:
                with urlopen(request, timeout=3) as response:
                    return response.status, response.read()
            except HTTPError as error:
                return error.code, error.read()
        try:
            ui_http_page_status, ui_http_page_body = ui_http_request("/")
            ui_http_state_status, ui_http_state_body = ui_http_request("/api/state")
            ui_http_reject_status, ui_http_reject_body = ui_http_request("/api/action", {
                "type": "connect", "fromNode": "source_left", "fromSocket": "value",
                "toNode": "sum_node", "toSocket": "missing"})
            ui_http_edit_status, ui_http_edit_body = ui_http_request("/api/action", {
                "type": "layout", "nodeId": "source_left", "values": {"x": 88}})
            ui_http_undo_status, ui_http_undo_body = ui_http_request("/api/action", {"type": "undo"})
            ui_http_state = json.loads(ui_http_state_body)
            ui_http_rejected = json.loads(ui_http_reject_body)
            ui_http_edited = json.loads(ui_http_edit_body)
            ui_http_undone = json.loads(ui_http_undo_body)
        finally:
            http_server.shutdown()
            http_server.server_close()
            http_thread.join(timeout=3)
        empty_graph = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                       "nodes": [], "links": [], "output": {"nodeId": "", "socketId": ""}}
        authoring_edits = [
            lambda graph: dynamic_module.add_node(graph, dynamic_registry, "artifact.rd.constant.float32", 1,
                                                  "author_left", x=16, y=48),
            lambda graph: dynamic_module.edit_node_parameters(graph, dynamic_registry, "author_left", {"value": 0.25}),
            lambda graph: dynamic_module.add_node(graph, dynamic_registry, "artifact.rd.constant.float32", 1,
                                                  "author_right", x=16, y=128),
            lambda graph: dynamic_module.edit_node_parameters(graph, dynamic_registry, "author_right", {"value": 0.5}),
            lambda graph: dynamic_module.add_node(graph, dynamic_registry, "artifact.rd.math.add.float32", 1,
                                                  "author_sum", x=192, y=88),
            lambda graph: dynamic_module.connect_nodes_batch(
                graph, dynamic_registry,
                [("author_left", "value", "author_sum", "a"),
                 ("author_right", "value", "author_sum", "b")]),
            lambda graph: dynamic_module.set_graph_output(graph, dynamic_registry, "author_sum", "result"),
        ]
        authoring_transaction = dynamic_module.create_document_transaction(
            "Build Dynamic Add Graph", empty_graph, authoring_edits)
        authoring_graph = dynamic_module.apply_document_command(authoring_transaction, empty_graph, direction="redo")
        authoring_compile = dynamic_module.compile_document_from_data(authoring_graph, dynamic_registry)
        authoring_validation = dynamic_module.validate_document_for_editing(authoring_graph, dynamic_registry)
        authoring_undo = dynamic_module.apply_document_command(authoring_transaction, authoring_graph, direction="undo")
        authoring_redo = dynamic_module.apply_document_command(authoring_transaction, authoring_undo, direction="redo")
        replicate_operation = dynamic_registry[("artifact.convert.scalar.replicate.vector4.rgb_alpha_one", 1)]
        extract_x_operation = dynamic_registry[("artifact.convert.vector4.extract_x.scalar", 1)]
        vector_graph = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": [dynamic_module.instantiate_node(constant_operation, "scalar_source"),
                      dynamic_module.instantiate_node(replicate_operation, "replicate")],
            "links": [{"from": {"nodeId": "scalar_source", "socketId": "value"},
                       "to": {"nodeId": "replicate", "socketId": "value"}}],
            "output": {"nodeId": "replicate", "socketId": "vector"},
        }
        vector_graph["nodes"][0]["parameters"]["value"] = 0.375
        vector_compile = dynamic_module.compile_document_from_data(vector_graph, dynamic_registry)
        vector_validation = dynamic_module.validate_document_for_editing(vector_graph, dynamic_registry)
        chained_conversion_graph = dynamic_module.add_node(
            vector_graph, dynamic_registry, "artifact.convert.vector4.extract_x.scalar", 1, "extract_x")
        chained_conversion_graph = dynamic_module.connect_nodes_batch(
            chained_conversion_graph, dynamic_registry,
            [("replicate", "vector", "extract_x", "vector")])
        chained_conversion_graph = dynamic_module.set_graph_output(
            chained_conversion_graph, dynamic_registry, "extract_x", "value")
        chained_conversion_compile = dynamic_module.compile_document_from_data(
            chained_conversion_graph, dynamic_registry)
        vector4_constant_operation = dynamic_registry[("artifact.rd.constant.vector4.float32", 1)]
        vector4_constant_node = dynamic_module.instantiate_node(vector4_constant_operation, "color_source")
        vector4_constant_document = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": [vector4_constant_node], "links": [],
            "output": {"nodeId": "color_source", "socketId": "value"},
        }
        vector4_default_compile = dynamic_module.compile_document_from_data(
            vector4_constant_document, dynamic_registry)
        vector3_constant_compile = dynamic_module.compile_document_from_data(
            dynamic_vector3_constant_document, dynamic_registry)
        invalid_vector3_document = copy.deepcopy(dynamic_vector3_constant_document)
        invalid_vector3_document["nodes"][0]["parameters"]["value"] = [0.25, -0.5]
        try:
            dynamic_module.compile_document_from_data(invalid_vector3_document, dynamic_registry)
            invalid_vector3_parameter_diagnostic = None
        except dynamic_module.ContractError as error:
            invalid_vector3_parameter_diagnostic = {
                key: error.to_json()[key] for key in ("code", "severity", "path")}
        try:
            dynamic_module.expose_node_parameter(
                dynamic_vector3_constant_document, dynamic_registry, "vector_source",
                "value", "shared_vector", "Shared Vector")
            vector3_graph_parameter_exposure_diagnostic = None
        except dynamic_module.ContractError as error:
            vector3_graph_parameter_exposure_diagnostic = {
                key: error.to_json()[key] for key in ("code", "severity", "path")}
        vector3_arithmetic_compile = dynamic_module.compile_document_from_data(
            dynamic_vector3_arithmetic_document, dynamic_registry)
        vector3_mismatch_document = copy.deepcopy(dynamic_vector3_arithmetic_document)
        vector3_mismatch_document["nodes"][1]["operationId"] = "artifact.rd.constant.float32"
        vector3_mismatch_document["nodes"][1]["parameters"] = {"value": 2.0}
        try:
            dynamic_module.compile_document_from_data(vector3_mismatch_document, dynamic_registry)
            vector3_type_mismatch_diagnostic = None
        except dynamic_module.ContractError as error:
            vector3_type_mismatch_diagnostic = {
                key: error.to_json()[key] for key in ("code", "severity", "path")}
        vector4_edited_document = dynamic_module.edit_node_parameters(
            vector4_constant_document, dynamic_registry, "color_source",
            {"value": [0.25, 0.5, 0.75, 0.4]})
        vector4_edited_compile = dynamic_module.compile_document_from_data(
            vector4_edited_document, dynamic_registry)
        vector4_extract_graph = dynamic_module.add_node(
            vector4_edited_document, dynamic_registry,
            "artifact.convert.vector4.extract_x.scalar", 1, "color_x")
        vector4_extract_graph = dynamic_module.connect_nodes_batch(
            vector4_extract_graph, dynamic_registry,
            [("color_source", "value", "color_x", "vector")])
        vector4_extract_graph = dynamic_module.set_graph_output(
            vector4_extract_graph, dynamic_registry, "color_x", "value")
        vector4_extract_compile = dynamic_module.compile_document_from_data(
            vector4_extract_graph, dynamic_registry)
        vector4_add_operation = dynamic_registry[("artifact.rd.math.add.vector4.float32", 1)]
        vector4_multiply_operation = dynamic_registry[("artifact.rd.math.multiply.vector4.float32", 1)]
        vector4_arithmetic_graph = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": [dynamic_module.instantiate_node(vector4_constant_operation, "v4_left"),
                      dynamic_module.instantiate_node(vector4_constant_operation, "v4_right"),
                      dynamic_module.instantiate_node(vector4_add_operation, "v4_sum"),
                      dynamic_module.instantiate_node(vector4_multiply_operation, "v4_product")],
            "links": [
                {"from": {"nodeId": "v4_left", "socketId": "value"}, "to": {"nodeId": "v4_sum", "socketId": "a"}},
                {"from": {"nodeId": "v4_right", "socketId": "value"}, "to": {"nodeId": "v4_sum", "socketId": "b"}},
                {"from": {"nodeId": "v4_sum", "socketId": "result"}, "to": {"nodeId": "v4_product", "socketId": "a"}},
                {"from": {"nodeId": "v4_right", "socketId": "value"}, "to": {"nodeId": "v4_product", "socketId": "b"}},
            ], "output": {"nodeId": "v4_product", "socketId": "result"},
        }
        vector4_arithmetic_graph["nodes"][0]["parameters"]["value"] = [0.2, 0.4, 0.6, 0.8]
        vector4_arithmetic_graph["nodes"][1]["parameters"]["value"] = [0.5, 0.25, 1.5, 0.75]
        vector4_arithmetic_compile = dynamic_module.compile_document_from_data(
            vector4_arithmetic_graph, dynamic_registry)
        vector4_arithmetic_edited = dynamic_module.edit_node_parameters(
            vector4_arithmetic_graph, dynamic_registry, "v4_right",
            {"value": [0.5, 0.5, 0.5, 0.5]})
        vector4_arithmetic_edited_compile = dynamic_module.compile_document_from_data(
            vector4_arithmetic_edited, dynamic_registry)
        round_binary32 = lambda value: struct.unpack("<f", struct.pack("<f", value))[0]
        vector4_left_values = [round_binary32(value) for value in vector4_arithmetic_graph["nodes"][0]["parameters"]["value"]]
        vector4_right_values = [round_binary32(value) for value in vector4_arithmetic_graph["nodes"][1]["parameters"]["value"]]
        vector4_cpu_candidate = [round_binary32(round_binary32(left + right) * right)
                                 for left, right in zip(vector4_left_values, vector4_right_values)]
        vector4_arithmetic_type_mismatch = copy.deepcopy(vector4_arithmetic_graph)
        vector4_arithmetic_type_mismatch["links"][0]["from"]["nodeId"] = "source_left"
        vector4_arithmetic_type_mismatch["nodes"].append(dynamic_module.instantiate_node(
            constant_operation, "source_left"))
        try:
            dynamic_module.compile_document_from_data(vector4_arithmetic_type_mismatch, dynamic_registry)
            vector4_arithmetic_type_diagnostic = None
        except dynamic_module.ContractError as error:
            vector4_arithmetic_type_diagnostic = {key: error.to_json()[key] for key in ("code", "severity", "path")}
        conversion_type_mismatch_graph = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": [dynamic_module.instantiate_node(constant_operation, "mismatch_source"),
                      dynamic_module.instantiate_node(extract_x_operation, "mismatch_extract")],
            "links": [{"from": {"nodeId": "mismatch_source", "socketId": "value"},
                       "to": {"nodeId": "mismatch_extract", "socketId": "vector"}}],
            "output": {"nodeId": "mismatch_extract", "socketId": "value"},
        }
        connected_validation = dynamic_module.validate_document_for_editing(connected_graph, dynamic_registry)
        alternate_implementation_registry = dict(dynamic_registry)
        alternate_add_operation = alternate_implementation_registry[("artifact.rd.math.add.float32", 1)]
        alternate_implementation_registry[("artifact.rd.math.add.float32", 1)] = replace(
            alternate_add_operation, implementation_id="float.multiply")
        alternate_implementation_compile = dynamic_module.compile_document_from_data(
            connected_graph, alternate_implementation_registry)
        palette_added_graph = dynamic_module.add_node(
            connected_graph, dynamic_registry, "artifact.rd.math.add.float32", 1, "palette_added_add", x=320, y=80)
        palette_added_validation = dynamic_module.validate_document_for_editing(palette_added_graph, dynamic_registry)
        palette_wired_graph = dynamic_module.connect_nodes_batch(
            palette_added_graph, dynamic_registry,
            [("source_left", "value", "palette_added_add", "a"),
             ("source_right", "value", "palette_added_add", "b")])
        palette_wired_graph = dynamic_module.set_graph_output(
            palette_wired_graph, dynamic_registry, "palette_added_add", "result")
        palette_complete_validation = dynamic_module.validate_document_for_editing(palette_wired_graph, dynamic_registry)
        composite_graph_edit = dynamic_module.add_node_with_connections(
            connected_graph, dynamic_registry, "artifact.rd.math.add.float32", 1, "composite_add",
            [("source_left", "value", "composite_add", "a"),
             ("source_right", "value", "composite_add", "b")],
            "result", x=340, y=100)
        composite_graph_validation = dynamic_module.validate_document_for_editing(composite_graph_edit, dynamic_registry)
        composite_graph_command = dynamic_module.create_document_command(
            "Add and Connect Node", connected_graph, composite_graph_edit)
        composite_graph_undo = dynamic_module.apply_document_command(
            composite_graph_command, composite_graph_edit, direction="undo")
        composite_graph_redo = dynamic_module.apply_document_command(
            composite_graph_command, composite_graph_undo, direction="redo")
        composite_parameter_edit = dynamic_module.edit_node_parameters(
            composite_graph_edit, dynamic_registry, "source_left", {"value": 0.75})
        composite_parameter_command = dynamic_module.create_document_command(
            "Edit Source Value", composite_graph_edit, composite_parameter_edit)
        composite_grouped_command = dynamic_module.compose_document_commands(
            "Add and Configure Node", [composite_graph_command, composite_parameter_command])
        composite_group_undo = dynamic_module.apply_document_command(
            composite_grouped_command, composite_parameter_edit, direction="undo")
        composite_group_redo = dynamic_module.apply_document_command(
            composite_grouped_command, composite_group_undo, direction="redo")
        composite_before_compile = dynamic_module.compile_document_from_data(composite_graph_edit, dynamic_registry)
        composite_after_compile = dynamic_module.compile_document_from_data(composite_parameter_edit, dynamic_registry)
        incomplete_graph = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
                            "nodes": [dynamic_module.instantiate_node(add_operation_for_graph, "incomplete_sum")],
                            "links": [], "output": {"nodeId": "incomplete_sum", "socketId": "result"}}
        incomplete_validation = dynamic_module.validate_document_for_editing(incomplete_graph, dynamic_registry)
        disconnected_graph = dynamic_module.disconnect_nodes(connected_graph, "source_left", "value", "sum_node", "a")
        selected_source_output = dynamic_module.set_graph_output(connected_graph, dynamic_registry, "source_left", "value")
        selected_source_compile = dynamic_module.compile_document_from_data(selected_source_output, dynamic_registry)
        deleted_source_graph = dynamic_module.delete_node(connected_graph, dynamic_registry, "source_left")
        delete_source_command = dynamic_module.create_document_command("Delete Float", connected_graph, deleted_source_graph)
        restored_source_graph = dynamic_module.apply_document_command(delete_source_command, deleted_source_graph, direction="undo")
        deleted_output_diagnostic = None
        parameter_edited_graph = dynamic_module.edit_node_parameters(connected_graph, dynamic_registry, "source_left", {"value": 0.625})
        command_edited_graph = dynamic_module.edit_node_layout(parameter_edited_graph, "source_left", {"x": 25})
        edit_command = dynamic_module.create_document_command("Edit Float", connected_graph, command_edited_graph)
        disconnect_after_edit = dynamic_module.disconnect_nodes(command_edited_graph, "source_left", "value", "sum_node", "a")
        disconnect_command = dynamic_module.create_document_command("Disconnect Float", command_edited_graph, disconnect_after_edit)
        parameter_command = dynamic_module.create_document_command("Change Value", connected_graph, parameter_edited_graph)
        layout_command = dynamic_module.create_document_command("Move Node", parameter_edited_graph, command_edited_graph)
        grouped_command = dynamic_module.compose_document_commands("Edit and Disconnect", [parameter_command, layout_command, disconnect_command])
        transaction_edits = [
            lambda graph: dynamic_module.edit_node_parameters(graph, dynamic_registry, "source_left", {"value": 0.625}),
            lambda graph: dynamic_module.edit_node_layout(graph, "source_left", {"x": 25}),
            lambda graph: dynamic_module.disconnect_nodes(graph, "source_left", "value", "sum_node", "a"),
        ]
        transaction_command = dynamic_module.create_document_transaction("Edit Transaction", connected_graph, transaction_edits)
        transaction_result = dynamic_module.apply_document_command(transaction_command, connected_graph, direction="redo")
        transaction_undo = dynamic_module.apply_document_command(transaction_command, transaction_result, direction="undo")
        undo_disconnect_graph = dynamic_module.apply_document_command(disconnect_command, disconnect_after_edit, direction="undo")
        undo_edit_graph = dynamic_module.apply_document_command(edit_command, undo_disconnect_graph, direction="undo")
        redo_edit_graph = dynamic_module.apply_document_command(edit_command, undo_edit_graph, direction="redo")
        redo_disconnect_graph = dynamic_module.apply_document_command(disconnect_command, redo_edit_graph, direction="redo")
        palette = dynamic_module.build_palette_model(dynamic_registry)
        duplicate_label_registry = dict(dynamic_registry)
        add_definition = duplicate_label_registry[("artifact.rd.math.add.float32", 1)]
        duplicate_label_registry[("vendor.math.add.float32", 1)] = replace(add_definition, operation_id="vendor.math.add.float32")
        duplicate_label_palette = dynamic_module.build_palette_model(duplicate_label_registry)
        duplicate_label_entries = [item for item in duplicate_label_palette if item["label"] == "Add" and item["category"] == "Math"]
        saved_registry_lock = dynamic_module.registry_contract_lock(dynamic_registry)
        presentation_only_registry = dict(dynamic_registry)
        first_key = sorted(presentation_only_registry)[0]
        presentation_only_registry[first_key] = replace(presentation_only_registry[first_key], display_name="Renamed palette entry", category="Presentation")
        changed_contract_registry = dict(dynamic_registry)
        add_key_for_contract = ("artifact.rd.math.add.float32", 1)
        changed_add = changed_contract_registry[add_key_for_contract]
        changed_contract_registry[add_key_for_contract] = replace(
            changed_add, inputs=(replace(changed_add.inputs[0], socket_id="left"), changed_add.inputs[1]))
        changed_default_registry = dict(dynamic_registry)
        constant_for_default = changed_default_registry[("artifact.rd.constant.float32", 1)]
        changed_default_registry[("artifact.rd.constant.float32", 1)] = replace(
            constant_for_default, parameters=(replace(constant_for_default.parameters[0], default=0.25),))
        changed_requiredness_registry = dict(dynamic_registry)
        changed_add_requiredness = changed_requiredness_registry[add_key_for_contract]
        changed_requiredness_registry[add_key_for_contract] = replace(
            changed_add_requiredness, inputs=(replace(changed_add_requiredness.inputs[0], required=False), changed_add_requiredness.inputs[1]))
        removed_contract_registry = dict(dynamic_registry)
        removed_contract_registry.pop(first_key)
        additive_version_registry = dict(dynamic_registry)
        constant_v1 = additive_version_registry[("artifact.rd.constant.float32", 1)]
        additive_version_registry[("artifact.rd.constant.float32", 2)] = replace(constant_v1, semantic_version=2)
        presentation_compatibility = dynamic_module.compare_registry_contracts(saved_registry_lock, presentation_only_registry)
        changed_contract_diagnostics = dynamic_module.compare_registry_contracts(saved_registry_lock, changed_contract_registry)
        removed_contract_diagnostics = dynamic_module.compare_registry_contracts(saved_registry_lock, removed_contract_registry)
        additive_version_diagnostics = dynamic_module.compare_registry_contracts(saved_registry_lock, additive_version_registry)
        regenerated_candidate = dynamic_module.compile_document_from_data(dynamic_document, dynamic_registry)
        regenerated_subtract_candidate = dynamic_module.compile_document_from_data(dynamic_subtract_document, dynamic_registry)
        regenerated_divide_candidate = dynamic_module.compile_document_from_data(dynamic_divide_document, dynamic_registry)
        regenerated_vector4_divide_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_divide_document, dynamic_registry)
        regenerated_vector4_subtract_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_subtract_document, dynamic_registry)
        regenerated_vector4_invert_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_invert_document, dynamic_registry)
        regenerated_clamp_candidate = dynamic_module.compile_document_from_data(dynamic_clamp_document, dynamic_registry)
        regenerated_vector4_mix_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_mix_document, dynamic_registry)
        regenerated_vector4_separate_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_separate_document, dynamic_registry)
        regenerated_vector4_ramp_candidate = dynamic_module.compile_document_from_data(dynamic_vector4_ramp_document, dynamic_registry)
        regenerated_optional_default_candidate = dynamic_module.compile_document_from_data(
            dynamic_optional_default_document, dynamic_registry)
        regenerated_optional_vector4_default_candidate = dynamic_module.compile_document_from_data(
            dynamic_optional_vector4_default_document, dynamic_registry)
        regenerated_optional_vector3_default_candidate = dynamic_module.compile_document_from_data(
            dynamic_optional_vector3_default_document, dynamic_registry)
        regenerated_vector2_uv_candidate = dynamic_module.compile_document_from_data(
            dynamic_vector2_uv_document, dynamic_registry)
        regenerated_vector2_mapping_candidate = dynamic_module.compile_document_from_data(
            dynamic_vector2_mapping_document, dynamic_registry)
        edited_vector2_mapping_document = copy.deepcopy(dynamic_vector2_mapping_document)
        next(node for node in edited_vector2_mapping_document["nodes"]
             if node.get("nodeId") == "mapping")["parameters"]["offsetX"] = 0.5
        edited_vector2_mapping_candidate = dynamic_module.compile_document_from_data(
            edited_vector2_mapping_document, dynamic_registry)
        regenerated_base_color_texture_candidate = dynamic_module.compile_document_from_data(
            dynamic_base_color_texture_document, dynamic_registry)
        regenerated_mapped_base_color_texture_candidate = dynamic_module.compile_document_from_data(
            dynamic_mapped_base_color_texture_document, dynamic_registry)
        texture_resource_binding_a = {
            "assetId": "11111111-1111-4111-8111-111111111111",
            "assetType": "Image",
            "sourceRevision": 1,
            "samplerKey": "legacy.base_color.v1",
            "uvSet": 0,
            "uvTransform": {"scaleU": 1.0, "scaleV": 1.0, "rotationDegrees": 0.0,
                            "offsetU": 0.0, "offsetV": 0.0},
            "colorInterpretation": "srgb",
        }
        texture_resource_key = "artifact.material.texture.base_color"
        resolved_texture_candidate_a = dynamic_module.compile_document_from_data(
            dynamic_base_color_texture_document, dynamic_registry,
            resolved_resource_bindings={texture_resource_key: texture_resource_binding_a})
        texture_binding_mutations = []
        for field, value in (
                ("assetId", "22222222-2222-4222-8222-222222222222"),
                ("sourceRevision", 2),
                ("samplerKey", "legacy.base_color.alternate"),
                ("uvSet", 1),
                ("colorInterpretation", "linear")):
            changed_binding = copy.deepcopy(texture_resource_binding_a)
            changed_binding[field] = value
            texture_binding_mutations.append(dynamic_module.compile_document_from_data(
                dynamic_base_color_texture_document, dynamic_registry,
                resolved_resource_bindings={texture_resource_key: changed_binding}))
        changed_uv_transform_binding = copy.deepcopy(texture_resource_binding_a)
        changed_uv_transform_binding["uvTransform"]["offsetU"] = 0.25
        texture_binding_mutations.append(dynamic_module.compile_document_from_data(
            dynamic_base_color_texture_document, dynamic_registry,
            resolved_resource_bindings={texture_resource_key: changed_uv_transform_binding}))
        incompatible_texture_binding = copy.deepcopy(texture_resource_binding_a)
        incompatible_texture_binding["assetType"] = "Model"
        try:
            dynamic_module.compile_document_from_data(
                dynamic_base_color_texture_document, dynamic_registry,
                resolved_resource_bindings={texture_resource_key: incompatible_texture_binding})
            incompatible_texture_asset_diagnostic = None
        except dynamic_module.ContractError as error:
            incompatible_texture_asset_diagnostic = error.to_json()
        regenerated_texture_channels_candidate = dynamic_module.compile_document_from_data(
            dynamic_texture_channels_document, dynamic_registry)
        dynamic_texture_channels_blue_document = copy.deepcopy(dynamic_texture_channels_document)
        dynamic_texture_channels_blue_document["output"] = {"nodeId": "channels", "socketId": "z"}
        regenerated_texture_channels_blue_candidate = dynamic_module.compile_document_from_data(
            dynamic_texture_channels_blue_document, dynamic_registry)
        texture_slot_candidates = []
        for slot_key in ("base_color", "opacity", "emission", "metallic_roughness", "normal", "occlusion"):
            uv_node = dynamic_module.instantiate_node(
                dynamic_registry[("artifact.input.texcoord.uv0.vector2", 1)], "uv")
            sample_node = dynamic_module.instantiate_node(
                dynamic_registry[(f"artifact.texture.sample.{slot_key}", 1)], "texture")
            sample_document = {
                "schemaVersion": 1,
                "kind": "artifact-shader-graph-rd-document",
                "nodes": [uv_node, sample_node],
                "links": [],
                "output": {"nodeId": "texture", "socketId": "color"},
            }
            sample_document = dynamic_module.connect_nodes(
                sample_document, dynamic_registry, "uv", "value", "texture", "uv")
            texture_slot_candidates.append((slot_key, dynamic_module.compile_document_from_data(
                sample_document, dynamic_registry)))
        optional_vector4_default_edit_document = dynamic_module.edit_node_socket_default(
            dynamic_optional_vector4_default_document, dynamic_registry, "sum", "b",
            [0.2, 0.3, 0.4, 0.5])
        optional_vector4_default_edit_candidate = dynamic_module.compile_document_from_data(
            optional_vector4_default_edit_document, dynamic_registry)
        optional_vector3_default_edit_document = dynamic_module.edit_node_socket_default(
            dynamic_optional_vector3_default_document, dynamic_registry, "vector_sum", "b",
            [0.4, 0.5, 0.6])
        optional_vector3_default_edit_candidate = dynamic_module.compile_document_from_data(
            optional_vector3_default_edit_document, dynamic_registry)
        vector3_default_ui_session = local_ui_module.Session(
            dynamic_registry, dynamic_optional_vector3_default_document, None, [])
        vector3_default_ui_session.apply({
            "type": "socketDefault", "nodeId": "vector_sum", "socketId": "b",
            "value": [0.7, 0.8, 0.9]})
        vector3_default_ui_edited = dynamic_module.preserve_document(vector3_default_ui_session.document)
        vector3_default_ui_session.apply({"type": "undo"})
        vector3_default_ui_undo = dynamic_module.preserve_document(vector3_default_ui_session.document)
        vector3_default_ui_session.apply({"type": "redo"})
        vector3_default_ui_redo = dynamic_module.preserve_document(vector3_default_ui_session.document)
        dynamic_vector4_ramp_alpha_document = copy.deepcopy(dynamic_vector4_ramp_document)
        dynamic_vector4_ramp_alpha_document["output"] = {"nodeId": "ramp", "socketId": "alpha"}
        regenerated_vector4_ramp_alpha_candidate = dynamic_module.compile_document_from_data(
            dynamic_vector4_ramp_alpha_document, dynamic_registry)
        multi_context_document = dynamic_module.add_node(
            dynamic_vector4_ramp_document, dynamic_registry,
            "artifact.input.texcoord.uv0.x", 1, "uv_second", x=0, y=100)
        multi_context_document = dynamic_module.add_node(
            multi_context_document, dynamic_registry,
            "artifact.rd.math.add.float32", 1, "uv_sum", x=180, y=50)
        multi_context_document = dynamic_module.connect_nodes(
            multi_context_document, dynamic_registry, "uv", "value", "uv_sum", "a")
        multi_context_document = dynamic_module.connect_nodes(
            multi_context_document, dynamic_registry, "uv_second", "value", "uv_sum", "b")
        multi_context_document = dynamic_module.rewire_input(
            multi_context_document, dynamic_registry, "uv_sum", "result", "ramp", "factor")
        multi_context_candidate = dynamic_module.compile_document_from_data(
            multi_context_document, dynamic_registry)
        alternate_context_candidate = dynamic_module.compile_document_from_data(
            dynamic_vector4_ramp_document, dynamic_registry,
            hlsl_context_bindings={
                ("artifact.material.surface.uv0", "x"): "(MaterialInput.UV).x",
            })
        try:
            dynamic_module.compile_document_from_data(
                dynamic_vector4_ramp_document, dynamic_registry,
                hlsl_context_bindings={"artifact.material.surface.uv0.x": "In.UV.x"})
            malformed_context_binding_diagnostic = None
        except dynamic_module.ContractError as error:
            malformed_context_binding_diagnostic = {
                key: error.to_json()[key] for key in ("code", "severity", "path")}
        try:
            dynamic_module.compile_document_from_data(
                dynamic_vector4_ramp_document, dynamic_registry,
                hlsl_context_bindings={})
            missing_context_binding_diagnostic = None
        except dynamic_module.ContractError as error:
            missing_context_binding_diagnostic = {
                key: error.to_json()[key] for key in ("code", "severity", "path")}
        reordered_multi_context_document = copy.deepcopy(multi_context_document)
        reordered_multi_context_document["nodes"].reverse()
        reordered_multi_context_document["links"].reverse()
        reordered_multi_context_candidate = dynamic_module.compile_document_from_data(
            reordered_multi_context_document, dynamic_registry)
        with tempfile.TemporaryDirectory(prefix="shader-graph-ramp-stops-roundtrip-") as ramp_roundtrip_directory:
            ramp_roundtrip_path = Path(ramp_roundtrip_directory) / "dynamic_vector4_ramp.json"
            ramp_roundtrip_saved_hash = dynamic_module.save_document(
                ramp_roundtrip_path, dynamic_vector4_ramp_document)
            ramp_roundtrip_document, ramp_roundtrip_loaded_hash = dynamic_module.load_document_snapshot(
                ramp_roundtrip_path)
            ramp_roundtrip_resaved_hash = dynamic_module.save_document(
                ramp_roundtrip_path, ramp_roundtrip_document,
                expected_existing_sha256=ramp_roundtrip_loaded_hash)
            ramp_roundtrip_resaved_document = dynamic_module.load_document(ramp_roundtrip_path)
        ramp_stops_before_edit = copy.deepcopy(dynamic_vector4_ramp_document["nodes"][1]["parameters"]["stops"])
        ramp_stops_after_edit = copy.deepcopy(ramp_stops_before_edit)
        ramp_stops_after_edit.insert(2, {"position": 0.25, "color": [0.25, 0.5, 0.75, 0.9]})
        ramp_stops_after_edit = [{"position": round_binary32(stop["position"]), "color": [round_binary32(channel) for channel in stop["color"]]} for stop in ramp_stops_after_edit]
        ramp_edited_document = dynamic_module.edit_node_parameters(
            dynamic_vector4_ramp_document, dynamic_registry, "ramp", {"stops": ramp_stops_after_edit})
        ramp_invalid_edits = []
        for invalid_stops in ([], ramp_stops_before_edit[:1],
                              ramp_stops_before_edit + [ramp_stops_before_edit[-1]] * 29,
                              [{"position": 0.0, "color": [0.0, 0.0, float("inf"), 1.0]}] * 2):
            try:
                dynamic_module.edit_node_parameters(
                    dynamic_vector4_ramp_document, dynamic_registry, "ramp", {"stops": invalid_stops})
                ramp_invalid_edits.append(None)
            except dynamic_module.ContractError as error:
                ramp_invalid_edits.append(error.to_json())
        ramp_session = local_ui_module.Session(
            dynamic_registry, copy.deepcopy(dynamic_vector4_ramp_document), None, [])
        ramp_session_before = dynamic_module.preserve_document(ramp_session.document)
        ramp_session.apply({"type": "parameter", "nodeId": "ramp",
                            "values": {"stops": ramp_stops_after_edit}})
        ramp_session_after = dynamic_module.preserve_document(ramp_session.document)
        ramp_session.apply({"type": "undo"})
        ramp_session_undo = dynamic_module.preserve_document(ramp_session.document)
        ramp_session.apply({"type": "redo"})
        ramp_session_redo = dynamic_module.preserve_document(ramp_session.document)
        dynamic_mixrgb_graph_candidates = {}
        for mode in expected_mix_modes:
            implementation_id = next(key for key, value in dynamic_module.VECTOR4_MIXRGB_MODE_BY_IMPLEMENTATION.items()
                                     if value == mode)
            operation = next(item for item in dynamic_registry.values()
                             if item.implementation_id == implementation_id)
            mix_document = {
                "schemaVersion": 1,
                "kind": "artifact-shader-graph-rd-document",
                "nodes": [
                    dynamic_module.instantiate_node(dynamic_registry[("artifact.rd.constant.float32", 1)], "mix_factor"),
                    dynamic_module.instantiate_node(dynamic_registry[("artifact.rd.constant.vector4.float32", 1)], "mix_color1"),
                    dynamic_module.instantiate_node(dynamic_registry[("artifact.rd.constant.vector4.float32", 1)], "mix_color2"),
                    dynamic_module.instantiate_node(operation, "mix_result"),
                ],
                "links": [
                    {"from": {"nodeId": "mix_factor", "socketId": "value"}, "to": {"nodeId": "mix_result", "socketId": "factor"}},
                    {"from": {"nodeId": "mix_color1", "socketId": "value"}, "to": {"nodeId": "mix_result", "socketId": "color1"}},
                    {"from": {"nodeId": "mix_color2", "socketId": "value"}, "to": {"nodeId": "mix_result", "socketId": "color2"}},
                ],
                "output": {"nodeId": "mix_result", "socketId": "result"},
            }
            mix_document["nodes"][0]["parameters"]["value"] = 0.4
            mix_document["nodes"][1]["parameters"]["value"] = [0.25, 0.75, 0.5, 0.25]
            mix_document["nodes"][2]["parameters"]["value"] = [0.75, 0.25, 0.5, 0.75]
            dynamic_mixrgb_graph_candidates[mode] = dynamic_module.compile_document_from_data(mix_document, dynamic_registry)
        preserved_document = dynamic_module.preserve_document(dynamic_document)
        unknown_version_document = copy.deepcopy(dynamic_document)
        unknown_version_document["nodes"][0]["semanticVersion"] = 99
        unknown_version_document["nodes"].append({
            "nodeId": "future_node", "operationId": "vendor.future.noise", "semanticVersion": 7,
            "parameters": {"opaqueCurve": [0.1, 0.8], "vendorData": {"mode": "blue"}},
            "layout": {"x": 17, "y": 29}, "futureField": {"keep": True}
        })
        unknown_link = {"from": {"nodeId": "future_node", "socketId": "future-output"},
                        "to": {"nodeId": "sum", "socketId": "future-input"}, "vendorLinkData": [1, 2, 3]}
        unknown_version_document["links"].append(unknown_link)
        unknown_version_diagnostics = dynamic_module.diagnose_unavailable_operations(unknown_version_document, dynamic_registry)
        edited_with_unknown_node = dynamic_module.edit_node_parameters(
            unknown_version_document, dynamic_registry, "constant_b", {"value": 0.75})
        edited_with_unknown_node = dynamic_module.edit_node_layout(edited_with_unknown_node, "constant_a", {"x": 99})
        opaque_edit_command = dynamic_module.create_document_command(
            "Edit Known Node Beside Opaque Data", unknown_version_document, edited_with_unknown_node)
        opaque_undo_document = dynamic_module.apply_document_command(
            opaque_edit_command, edited_with_unknown_node, direction="undo")
        opaque_redo_document = dynamic_module.apply_document_command(
            opaque_edit_command, opaque_undo_document, direction="redo")
        unknown_output_document = copy.deepcopy(unknown_version_document)
        unknown_output_document["output"] = {"nodeId": "future_node", "socketId": "future-output"}
        unknown_output_validation = dynamic_module.validate_document_for_editing(unknown_output_document, dynamic_registry)
        repaired_unknown_output = dynamic_module.delete_node(
            unknown_output_document, dynamic_registry, "future_node", replacement_output=("sum", "result"))
        repaired_unknown_output_validation = dynamic_module.validate_document_for_editing(repaired_unknown_output, dynamic_registry)
        if preserved_document != dynamic_document:
            errors.append("opaque document preservation changed known graph data")
        if dynamic_module.preserve_document(unknown_version_document) != unknown_version_document:
            errors.append("opaque document preservation changed an unknown operation payload")
        if len(unknown_version_diagnostics) != 2 or any(item.get("code") != "node.operation.version.unsupported" for item in unknown_version_diagnostics):
            errors.append("unknown operation version did not produce a stable non-destructive diagnostic")

        invalid_generic_mapping_parameter_document = copy.deepcopy(
            generic_image_mapped_uv_document)
        invalid_generic_mapping_parameter_document["nodes"][1]["parameters"]["scaleX"] = 1.0e100
        try:
            generic_image_module.project_resource_requirements(
                invalid_generic_mapping_parameter_document, generic_image_catalog)
            invalid_generic_mapping_parameter_diagnostic = None
        except generic_image_module.ContractError as error:
            invalid_generic_mapping_parameter_diagnostic = error.to_json()

        graph_limits = dynamic_contract.get("graphResourceLimits", {})
        graph_limit_goldens = dynamic_contract.get("sourceGuardGoldens", {}).get("graphLimitDiagnostics", [])
        ui_limit_document = copy.deepcopy(dynamic_document)
        base_limit_node = copy.deepcopy(dynamic_document["nodes"][0])
        ui_limit_document["nodes"] = [
            {**base_limit_node, "nodeId": f"limit_node_{index}",
             "layout": {"x": float(index), "y": 0.0}}
            for index in range(dynamic_module.MAX_GRAPH_NODES - len(dynamic_document["nodes"]))
        ] + ui_limit_document["nodes"]
        ui_limit_session = local_ui_module.Session(dynamic_registry, ui_limit_document, None, [])
        ui_limit_before_hash = ui_limit_session.current_document_hash
        ui_limit_add_diagnostic = diagnostic_for(
            lambda: ui_limit_session.apply({
                "type": "add", "operationId": "artifact.rd.constant.float32",
                "semanticVersion": 1, "nodeId": "over_limit_node",
            }))
        ui_repair_document = copy.deepcopy(ui_limit_document)
        ui_repair_document["nodes"].append(
            {**base_limit_node, "nodeId": "loaded_over_limit_node",
             "layout": {"x": float(dynamic_module.MAX_GRAPH_NODES), "y": 0.0}})
        ui_repair_session = local_ui_module.Session(dynamic_registry, ui_repair_document, None, [])
        ui_repair_session.apply({"type": "delete", "nodeId": "loaded_over_limit_node"})
        with tempfile.TemporaryDirectory() as limit_directory:
            oversized_document_path = Path(limit_directory) / "oversized_graph.json"
            oversized_document_hash = dynamic_module.save_document(
                oversized_document_path, ui_repair_document)
            oversized_reloaded_document, oversized_reloaded_hash = dynamic_module.load_document_snapshot(
                oversized_document_path)
            oversized_reloaded_validation = dynamic_module.validate_document_for_editing(
                oversized_reloaded_document, dynamic_registry)
        oversized_save_reload_preserved = (
            oversized_reloaded_document == ui_repair_document and
            oversized_reloaded_hash == oversized_document_hash and
            oversized_reloaded_validation["status"] == "invalid" and
            any(item["code"] == "graph.nodes.limit_exceeded"
                for item in oversized_reloaded_validation["diagnostics"]))
        oversized_nodes_document = copy.deepcopy(dynamic_document)
        oversized_nodes_document["nodes"] = [{}] * (dynamic_module.MAX_GRAPH_NODES + 1)
        oversized_nodes_validation = dynamic_module.validate_document_for_editing(
            oversized_nodes_document, dynamic_registry)
        oversized_nodes_compile_diagnostic = diagnostic_for(
            lambda: dynamic_module.compile_document_from_data(oversized_nodes_document, dynamic_registry))
        oversized_links_document = copy.deepcopy(dynamic_document)
        oversized_links_document["links"] = [{}] * (dynamic_module.MAX_GRAPH_LINKS + 1)
        oversized_links_validation = dynamic_module.validate_document_for_editing(
            oversized_links_document, dynamic_registry)
        oversized_links_compile_diagnostic = diagnostic_for(
            lambda: dynamic_module.compile_document_from_data(oversized_links_document, dynamic_registry))
        oversized_both_document = copy.deepcopy(oversized_nodes_document)
        oversized_both_document["links"] = [{}] * (dynamic_module.MAX_GRAPH_LINKS + 1)
        oversized_both_validation = dynamic_module.validate_document_for_editing(
            oversized_both_document, dynamic_registry)

        graph_parameter_source = dynamic_module.edit_node_parameters(
            connected_graph, dynamic_registry, "source_left", {"value": 0.25})
        graph_parameter_source = dynamic_module.edit_node_parameters(
            graph_parameter_source, dynamic_registry, "source_right", {"value": 0.5})
        graph_parameter_baseline_compile = dynamic_module.compile_document_from_data(
            graph_parameter_source, dynamic_registry)
        exposed_graph_parameter = dynamic_module.expose_node_parameter(
            graph_parameter_source, dynamic_registry, "source_left", "value", "shared_gain", "Shared Gain")
        shared_graph_parameter = dynamic_module.expose_node_parameter(
            exposed_graph_parameter, dynamic_registry, "source_right", "value", "shared_gain", "Shared Gain")
        shared_graph_parameter_compile = dynamic_module.compile_document_from_data(
            shared_graph_parameter, dynamic_registry)
        edited_graph_parameter = dynamic_module.edit_graph_parameter(
            shared_graph_parameter, dynamic_registry, "shared_gain", 0.75)
        edited_graph_parameter_compile = dynamic_module.compile_document_from_data(
            edited_graph_parameter, dynamic_registry)
        graph_parameter_uniforms_stable = (
            shared_graph_parameter_compile["shaderStructureHashSha256"] == edited_graph_parameter_compile["shaderStructureHashSha256"] and
            shared_graph_parameter_compile["parameterValuesHashSha256"] != edited_graph_parameter_compile["parameterValuesHashSha256"] and
            shared_graph_parameter_compile["hlslBody"] == edited_graph_parameter_compile["hlslBody"] and
            "float P_shared_gain : packoffset(c0.x);" in shared_graph_parameter_compile["hlslBody"] and
            "n_source_left_value = P_shared_gain;" in shared_graph_parameter_compile["hlslBody"] and
            "n_source_right_value = P_shared_gain;" in shared_graph_parameter_compile["hlslBody"] and
            "0.25f" not in shared_graph_parameter_compile["hlslBody"] and
            shared_graph_parameter_compile["parameterBindings"] == [{
                "parameterId": "shared_gain", "valueType": "Float32",
                "hlslSymbol": "P_shared_gain", "byteOffset": 0, "byteSize": 4,
                "blockBindingId": "artifact.shadergraph.parameters",
                "registerIndex": 0, "registerComponentOffset": 0,
                "hlslPackOffset": "c0.x", "bindings": [
                    {"nodeId": "source_left", "parameterId": "value"},
                    {"nodeId": "source_right", "parameterId": "value"}]}])
        layout_operation = lambda implementation_id: next(
            operation for operation in dynamic_registry.values()
            if operation.implementation_id == implementation_id)
        layout_nodes = [
            dynamic_module.instantiate_node(layout_operation(implementation_id), node_id)
            for implementation_id, node_id in [
                ("float.constant", "a_float"), ("float.constant", "b_float"),
                ("float.add", "sum_float"), ("float.replicate.rgb_alpha_one", "replicate"),
                ("vector4.constant", "m_color"), ("vector4.add", "vector_sum")]]
        parameter_layout_graph = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": layout_nodes,
            "links": [
                {"from": {"nodeId": "a_float", "socketId": "value"}, "to": {"nodeId": "sum_float", "socketId": "a"}},
                {"from": {"nodeId": "b_float", "socketId": "value"}, "to": {"nodeId": "sum_float", "socketId": "b"}},
                {"from": {"nodeId": "sum_float", "socketId": "result"}, "to": {"nodeId": "replicate", "socketId": "value"}},
                {"from": {"nodeId": "replicate", "socketId": "vector"}, "to": {"nodeId": "vector_sum", "socketId": "a"}},
                {"from": {"nodeId": "m_color", "socketId": "value"}, "to": {"nodeId": "vector_sum", "socketId": "b"}}],
            "output": {"nodeId": "vector_sum", "socketId": "result"}}
        parameter_layout_graph = dynamic_module.edit_node_parameters(
            parameter_layout_graph, dynamic_registry, "a_float", {"value": 0.125})
        parameter_layout_graph = dynamic_module.edit_node_parameters(
            parameter_layout_graph, dynamic_registry, "b_float", {"value": -0.25})
        parameter_layout_graph = dynamic_module.edit_node_parameters(
            parameter_layout_graph, dynamic_registry, "m_color", {"value": [1.5, -2.25, 0.5, 1.0]})
        for node_id, parameter_id, graph_id, label in [
                ("a_float", "value", "a_gain", "A Gain"),
                ("b_float", "value", "b_gain", "B Gain"),
                ("m_color", "value", "m_color", "Color")]:
            parameter_layout_graph = dynamic_module.expose_node_parameter(
                parameter_layout_graph, dynamic_registry, node_id, parameter_id, graph_id, label)
        parameter_block_layout_compile = dynamic_module.compile_document_from_data(
            parameter_layout_graph, dynamic_registry)
        resident_pipeline_path = root / "Artifact/src/Render/ArtifactRenderLayerPipeline.cpp"
        resident_pipeline_source = resident_pipeline_path.read_text(encoding="utf-8")
        prelude_match = re.search(
            r"kResidentGenericPrelude\s*=\s*R\"\((.*?)\)\";",
            resident_pipeline_source, re.DOTALL)
        resident_prelude = prelude_match.group(1) if prelude_match else ""
        resident_parameter_indices = [int(value) for value in re.findall(
            r"^\s*float\s+g_P([0-9]+)\s*;", resident_prelude, re.MULTILINE)]
        resident_standard_uniforms = re.findall(
            r"^\s*float\s+g_(Width|Height|Time|Frame|HistoryValid)\s*;",
            resident_prelude, re.MULTILINE)
        resident_cpp_capacity = re.search(
            r"struct\s+alignas\(16\)\s+ResidentGenericParams\s*\{\s*float\s+p\[([0-9]+)\]",
            resident_pipeline_source, re.DOTALL)
        resident_generic_source_matches_contract = (
            "cbuffer ResidentGenericParams : register(b0)" in resident_prelude and
            resident_parameter_indices == list(range(16)) and
            resident_standard_uniforms == ["Width", "Height", "Time", "Frame", "HistoryValid"] and
            resident_cpp_capacity is not None and int(resident_cpp_capacity.group(1)) == 16)
        resident_generic_layout = dynamic_module.adapt_to_resident_generic_abi(
            parameter_block_layout_compile)
        packed_values = resident_generic_layout["packedParameterValues"]
        malformed_value_hash = copy.deepcopy(parameter_block_layout_compile)
        malformed_value_hash["parameterValuesHashSha256"] = "0" * 64
        packed_hash_mismatch_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_value_hash))
        malformed_value_type = copy.deepcopy(parameter_block_layout_compile)
        malformed_value_type["parameterValues"][0]["valueType"] = "Vector4F32"
        packed_value_type_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_value_type))
        malformed_non_finite_value = copy.deepcopy(parameter_block_layout_compile)
        malformed_non_finite_value["parameterValues"][0]["value"] = math.nan
        packed_non_finite_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_non_finite_value))
        malformed_binding_order = copy.deepcopy(parameter_block_layout_compile)
        malformed_binding_order["parameterBindings"].reverse()
        packed_binding_order_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_binding_order))
        malformed_missing_value = copy.deepcopy(parameter_block_layout_compile)
        malformed_missing_value["parameterValues"].pop()
        packed_missing_value_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_missing_value))
        malformed_extra_value = copy.deepcopy(parameter_block_layout_compile)
        malformed_extra_value["parameterValues"].append({
            "parameterId": "unused", "valueType": "Float32", "value": 1.0})
        packed_extra_value_diagnostic = diagnostic_for(
            lambda: dynamic_module.pack_resident_generic_parameter_values(malformed_extra_value))
        resident_shared_base = dynamic_module.adapt_to_resident_generic_abi(
            shared_graph_parameter_compile)
        resident_shared_edited = dynamic_module.adapt_to_resident_generic_abi(
            edited_graph_parameter_compile)
        resident_synthetic_bindings = [
            {**parameter_block_layout_compile["parameterBindings"][0],
             "parameterId": f"lane_{index}", "hlslSymbol": f"P_lane_{index}"}
            for index in range(17)]
        resident_sixteen_bindings = dynamic_module.assign_resident_generic_parameter_slots(
            resident_synthetic_bindings[:16])
        resident_generic_slot_overflow_diagnostic = diagnostic_for(
            lambda: dynamic_module.assign_resident_generic_parameter_slots(resident_synthetic_bindings))
        resident_four_vector_bindings = [
            {**parameter_block_layout_compile["parameterBindings"][0],
             "parameterId": f"vector_{index}", "hlslSymbol": f"P_vector_{index}",
             "valueType": "Vector4F32"}
            for index in range(4)]
        resident_four_vectors = dynamic_module.assign_resident_generic_parameter_slots(
            resident_four_vector_bindings)
        resident_five_vector_overflow = diagnostic_for(
            lambda: dynamic_module.assign_resident_generic_parameter_slots(
                resident_four_vector_bindings + [{**resident_four_vector_bindings[0],
                    "parameterId": "vector_4", "hlslSymbol": "P_vector_4"}]))
        resident_generic_duplicate_id_diagnostic = diagnostic_for(
            lambda: dynamic_module.assign_resident_generic_parameter_slots([
                parameter_block_layout_compile["parameterBindings"][0],
                parameter_block_layout_compile["parameterBindings"][0]]))
        reflected_parameter_block = dynamic_module.resolve_reflected_parameter_block(
            parameter_block_layout_compile["parameterBlock"], [
                {"name": "FrameParams", "resourceKind": "constantBuffer", "physicalSlot": "b0"},
                {"name": "ArtifactGraphParameters", "resourceKind": "constantBuffer", "physicalSlot": "b4"},
                {"name": "InputTexture", "resourceKind": "texture", "physicalSlot": "t0"}])
        parameter_block_slot_collision_diagnostic = diagnostic_for(
            lambda: dynamic_module.resolve_reflected_parameter_block(
                parameter_block_layout_compile["parameterBlock"], [
                    {"name": "ArtifactGraphParameters", "resourceKind": "constantBuffer", "physicalSlot": "b4"},
                    {"name": "OtherParams", "resourceKind": "constantBuffer", "physicalSlot": "b4"}]))
        parameter_block_reflection_missing_diagnostic = diagnostic_for(
            lambda: dynamic_module.resolve_reflected_parameter_block(
                parameter_block_layout_compile["parameterBlock"], []))
        parameter_block_logical_id_duplicate_diagnostic = diagnostic_for(
            lambda: dynamic_module.validate_parameter_block_contracts([
                parameter_block_layout_compile["parameterBlock"],
                copy.deepcopy(parameter_block_layout_compile["parameterBlock"])]))
        unsupported_parameter_block_schema = copy.deepcopy(parameter_block_layout_compile["parameterBlock"])
        unsupported_parameter_block_schema["schemaVersion"] = 2
        parameter_block_schema_version_diagnostic = diagnostic_for(
            lambda: dynamic_module.validate_parameter_block_contracts([unsupported_parameter_block_schema]))
        unsupported_reflected_resource_diagnostic = diagnostic_for(
            lambda: dynamic_module.resolve_reflected_parameter_block(
                parameter_block_layout_compile["parameterBlock"], [
                    {"name": "ArtifactGraphParameters", "resourceKind": "mysteryBuffer", "physicalSlot": "b2"}]))
        parameter_block_layout_valid = (
            parameter_block_layout_compile["parameterBlock"]["layoutContract"] == "hlsl-cbuffer-packoffset-16byte-v1" and
            parameter_block_layout_compile["parameterBlock"]["schemaId"] == "artifact.shadergraph.parameter-block" and
            parameter_block_layout_compile["parameterBlock"]["schemaVersion"] == 1 and
            parameter_block_layout_compile["parameterBlock"]["bindingId"] == "artifact.shadergraph.parameters" and
            parameter_block_layout_compile["parameterBlock"]["resourceKind"] == "constantBuffer" and
            parameter_block_layout_compile["parameterBlock"]["physicalSlot"] is None and
            parameter_block_layout_compile["parameterBlock"]["physicalSlotStatus"] == "requires-compiler-reflection" and
            parameter_block_layout_compile["parameterBlock"]["byteSize"] == 32 and
            [(field["parameterId"], field["byteOffset"], field["byteSize"],
              field["registerIndex"], field["registerComponentOffset"], field["hlslPackOffset"])
             for field in parameter_block_layout_compile["parameterBlock"]["fields"]] == [
                 ("a_gain", 0, 4, 0, 0, "c0.x"), ("b_gain", 4, 4, 0, 1, "c0.y"),
                 ("m_color", 16, 16, 1, 0, "c1.x")] and
            "cbuffer ArtifactGraphParameters {" in parameter_block_layout_compile["hlslBody"] and
            "float P_a_gain : packoffset(c0.x);" in parameter_block_layout_compile["hlslBody"] and
            "float P_b_gain : packoffset(c0.y);" in parameter_block_layout_compile["hlslBody"] and
            "float4 P_m_color : packoffset(c1.x);" in parameter_block_layout_compile["hlslBody"] and
            reflected_parameter_block["bindingId"] == "artifact.shadergraph.parameters" and
            reflected_parameter_block["physicalSlot"] == "b4" and
            reflected_parameter_block["physicalSlotStatus"] == "compiler-reflected" and
            resident_generic_layout["targetAbi"] == "artifact.resident-generic-params.scalar16.v1" and
            resident_generic_source_matches_contract and
            packed_values["lanes"] == [0.125, -0.25, 1.5, -2.25, 0.5, 1.0] + [0.0] * 10 and
            packed_values["laneOwners"] == ["a_gain", "b_gain", "m_color", "m_color", "m_color", "m_color"] + [None] * 10 and
            packed_values["activeLaneCount"] == 6 and
            packed_values["parameterValuesHashSha256"] == parameter_block_layout_compile["parameterValuesHashSha256"] and
            packed_hash_mismatch_diagnostic == {"code": "shader.binding.parameter_values.hash_mismatch", "severity": "error", "path": "/parameterValuesHashSha256"} and
            packed_value_type_diagnostic == {"code": "shader.binding.parameter_type.mismatch", "severity": "error", "path": "/parameterValues/a_gain/valueType"} and
            packed_non_finite_diagnostic == {"code": "graph.parameter.value.invalid", "severity": "error", "path": "/parameterValues/a_gain/value"} and
            packed_binding_order_diagnostic == {"code": "shader.binding.parameter_order.invalid", "severity": "error", "path": "/parameterBindings/1/parameterId"} and
            packed_missing_value_diagnostic == {"code": "shader.binding.parameter_values.invalid", "severity": "error", "path": "/parameterValues"} and
            packed_extra_value_diagnostic == {"code": "shader.binding.parameter_values.invalid", "severity": "error", "path": "/parameterValues"} and
            len(resident_sixteen_bindings) == 16 and
            resident_sixteen_bindings[-1]["slotIndex"] == 15 and
            resident_sixteen_bindings[-1]["abiNames"] == ["g_P15"] and
            resident_sixteen_bindings[-1]["hlslExpression"] == "g_P15" and
            len(resident_four_vectors) == 4 and
            resident_four_vectors[-1]["slotIndices"] == [12, 13, 14, 15] and
            resident_four_vectors[-1]["hlslExpression"] == "float4(g_P12, g_P13, g_P14, g_P15)" and
            resident_shared_base["shaderStructureHashSha256"] == resident_shared_edited["shaderStructureHashSha256"] and
            resident_shared_base["parameterValuesHashSha256"] != resident_shared_edited["parameterValuesHashSha256"] and
            resident_shared_base["hlslBody"] == resident_shared_edited["hlslBody"] and
            "g_P0" in resident_shared_base["hlslBody"] and
            "0.25f" not in resident_shared_base["hlslBody"] and
            [(item["parameterId"], item["slotIndex"], item["slotCount"], item["abiNames"], item["hlslExpression"])
             for item in resident_generic_layout["parameterBindings"]] == [
                 ("a_gain", 0, 1, ["g_P0"], "g_P0"),
                 ("b_gain", 1, 1, ["g_P1"], "g_P1"),
                 ("m_color", 2, 4, ["g_P2", "g_P3", "g_P4", "g_P5"],
                  "float4(g_P2, g_P3, g_P4, g_P5)")] and
            "cbuffer " not in resident_generic_layout["hlslBody"] and
            "register(b0)" not in resident_generic_layout["hlslBody"] and
            "n_a_float_value = g_P0;" in resident_generic_layout["hlslBody"] and
            "n_b_float_value = g_P1;" in resident_generic_layout["hlslBody"] and
            "n_m_color_value = float4(g_P2, g_P3, g_P4, g_P5);" in resident_generic_layout["hlslBody"] and
            resident_generic_slot_overflow_diagnostic == {"code": "shader.binding.resident_slot_overflow", "severity": "error", "path": "/parameterBindings"} and
            resident_five_vector_overflow == {"code": "shader.binding.resident_slot_overflow", "severity": "error", "path": "/parameterBindings"} and
            resident_generic_duplicate_id_diagnostic == {"code": "shader.binding.logical_id.duplicate", "severity": "error", "path": "/parameterBindings/1"} and
            parameter_block_slot_collision_diagnostic == {"code": "shader.binding.slot_collision", "severity": "error", "path": "/reflection/resources/1"} and
            parameter_block_reflection_missing_diagnostic == {"code": "shader.binding.reflection.missing", "severity": "error", "path": "/reflection/resources"} and
            parameter_block_logical_id_duplicate_diagnostic == {"code": "shader.binding.logical_id.duplicate", "severity": "error", "path": "/parameterBlocks/1/bindingId"} and
            parameter_block_schema_version_diagnostic == {"code": "shader.binding.schema.unsupported", "severity": "error", "path": "/parameterBlocks/0"} and
            unsupported_reflected_resource_diagnostic == {"code": "shader.binding.reflection.invalid", "severity": "error", "path": "/reflection/resources/0"})
        renamed_graph_parameter = copy.deepcopy(shared_graph_parameter)
        renamed_graph_parameter["graphParameters"][0]["label"] = "Drive"
        renamed_graph_parameter_compile = dynamic_module.compile_document_from_data(
            renamed_graph_parameter, dynamic_registry)
        reordered_parameter_graph = copy.deepcopy(parameter_layout_graph)
        reordered_parameter_graph["graphParameters"].reverse()
        for parameter in reordered_parameter_graph["graphParameters"]:
            parameter["label"] = "Renamed " + parameter["parameterId"]
            parameter["bindings"].reverse()
        reordered_parameter_compile = dynamic_module.compile_document_from_data(
            reordered_parameter_graph, dynamic_registry)
        changed_graph_parameter_binding = copy.deepcopy(shared_graph_parameter)
        changed_graph_parameter_binding["graphParameters"][0]["bindings"].pop()
        changed_binding_compile = dynamic_module.compile_document_from_data(
            changed_graph_parameter_binding, dynamic_registry)
        graph_parameter_presentation_stable = (
            renamed_graph_parameter_compile["semanticHashSha256"] == shared_graph_parameter_compile["semanticHashSha256"] and
            renamed_graph_parameter_compile["shaderStructureHashSha256"] == shared_graph_parameter_compile["shaderStructureHashSha256"] and
            renamed_graph_parameter_compile["parameterValuesHashSha256"] == shared_graph_parameter_compile["parameterValuesHashSha256"] and
            renamed_graph_parameter_compile["hlslBody"] == shared_graph_parameter_compile["hlslBody"])
        graph_parameter_definition_order_stable = (
            reordered_parameter_compile["semanticHashSha256"] == parameter_block_layout_compile["semanticHashSha256"] and
            reordered_parameter_compile["shaderStructureHashSha256"] == parameter_block_layout_compile["shaderStructureHashSha256"] and
            reordered_parameter_compile["parameterValuesHashSha256"] == parameter_block_layout_compile["parameterValuesHashSha256"] and
            reordered_parameter_compile["hlslBody"] == parameter_block_layout_compile["hlslBody"] and
            reordered_parameter_compile["parameterBlock"] == parameter_block_layout_compile["parameterBlock"])
        graph_parameter_binding_change_updates_structure = (
            changed_binding_compile["shaderStructureHashSha256"] != shared_graph_parameter_compile["shaderStructureHashSha256"] and
            changed_binding_compile["parameterValuesHashSha256"] == shared_graph_parameter_compile["parameterValuesHashSha256"] and
            changed_binding_compile["hlslBody"] != shared_graph_parameter_compile["hlslBody"] and
            len(changed_binding_compile["parameterBindings"][0]["bindings"]) == 1)
        graph_parameter_uniforms_stable = (
            graph_parameter_uniforms_stable and parameter_block_layout_valid and
            graph_parameter_presentation_stable and graph_parameter_definition_order_stable and
            graph_parameter_binding_change_updates_structure)
        graph_parameter_command = dynamic_module.create_document_command(
            "Tune Shared Gain", shared_graph_parameter, edited_graph_parameter)
        graph_parameter_undo = dynamic_module.apply_document_command(
            graph_parameter_command, edited_graph_parameter, direction="undo")
        graph_parameter_redo = dynamic_module.apply_document_command(
            graph_parameter_command, graph_parameter_undo, direction="redo")
        bound_node_parameter_edit_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_parameters(
            shared_graph_parameter, dynamic_registry, "source_left", {"value": 0.9}))
        duplicate_graph_parameter_binding_diagnostic = diagnostic_for(lambda: dynamic_module.expose_node_parameter(
            shared_graph_parameter, dynamic_registry, "source_left", "value", "other_gain", "Other Gain"))
        invalid_graph_parameter_value_diagnostic = diagnostic_for(lambda: dynamic_module.edit_graph_parameter(
            shared_graph_parameter, dynamic_registry, "shared_gain", float("inf")))
        vector4_for_shared_type = dynamic_module.add_node(
            shared_graph_parameter, dynamic_registry, "artifact.rd.constant.vector4.float32", 1, "unbound_color")
        incompatible_graph_parameter_type_diagnostic = diagnostic_for(lambda: dynamic_module.expose_node_parameter(
            vector4_for_shared_type, dynamic_registry, "unbound_color", "value", "shared_gain", "Shared Gain"))
        duplicate_graph_parameter_document = copy.deepcopy(shared_graph_parameter)
        duplicate_graph_parameter_document["graphParameters"][0]["bindings"].append(
            copy.deepcopy(duplicate_graph_parameter_document["graphParameters"][0]["bindings"][0]))
        duplicate_graph_parameter_compile_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(
            duplicate_graph_parameter_document, dynamic_registry))
        single_graph_parameter = dynamic_module.expose_node_parameter(
            graph_parameter_source, dynamic_registry, "source_left", "value", "left_gain", "Left Gain")
        deleted_graph_parameter_node = dynamic_module.delete_node(
            single_graph_parameter, dynamic_registry, "source_left")
        round_trip_graph_parameter = dynamic_module.parse_json_text(
            dynamic_module.serialize_document(shared_graph_parameter))
        graph_parameter_session = local_ui_module.Session(
            dynamic_registry, shared_graph_parameter, None, [])
        graph_parameter_session.apply({"type": "graphParameter", "graphParameterId": "shared_gain", "value": 0.625})
        graph_parameter_session_document = dynamic_module.preserve_document(graph_parameter_session.document)
        graph_parameter_exposure_session = local_ui_module.Session(
            dynamic_registry, graph_parameter_source, None, [])
        graph_parameter_exposure_session.apply({"type": "expose", "nodeId": "source_left",
                                                "parameterId": "value", "graphParameterId": "ui_gain",
                                                "label": "UI Gain"})
        graph_parameter_exposure_session_document = dynamic_module.preserve_document(
            graph_parameter_exposure_session.document)
        graph_parameter_exposure_session.apply({"type": "undo"})
        graph_parameter_exposure_session_undo = dynamic_module.preserve_document(
            graph_parameter_exposure_session.document)
        graph_parameter_exposure_session.apply({"type": "redo"})
        graph_parameter_exposure_session_redo = dynamic_module.preserve_document(
            graph_parameter_exposure_session.document)
        vector4_exposed_graph_parameter = dynamic_module.expose_node_parameter(
            vector4_constant_document, dynamic_registry, "color_source", "value", "surface_tint", "Surface Tint")
        vector4_edited_graph_parameter = dynamic_module.edit_graph_parameter(
            vector4_exposed_graph_parameter, dynamic_registry, "surface_tint", [0.1, 0.2, 0.3, 0.4])
        vector4_graph_parameter_compile = dynamic_module.compile_document_from_data(
            vector4_edited_graph_parameter, dynamic_registry)
        vector4_graph_parameter_base_compile = dynamic_module.compile_document_from_data(
            vector4_exposed_graph_parameter, dynamic_registry)
        vector3_graph_parameter_session = local_ui_module.Session(
            dynamic_registry, dynamic_vector3_arithmetic_document, None, [])
        vector3_graph_parameter_session.apply({"type": "expose", "nodeId": "left",
                                               "parameterId": "value", "graphParameterId": "shared_normal",
                                               "label": "Shared Normal"})
        vector3_graph_parameter_session.apply({"type": "expose", "nodeId": "right",
                                               "parameterId": "value", "graphParameterId": "shared_normal",
                                               "label": "Shared Normal"})
        vector3_graph_parameter_session_shared = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "graphParameter",
                                               "graphParameterId": "shared_normal",
                                               "value": [0.1, 0.2, 0.3]})
        vector3_graph_parameter_session_edited = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "undo"})
        vector3_graph_parameter_session_undo = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "redo"})
        vector3_graph_parameter_session_redo = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "unexpose", "nodeId": "right",
                                               "parameterId": "value"})
        vector3_graph_parameter_session_detached = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "unexpose", "nodeId": "left",
                                               "parameterId": "value"})
        vector3_graph_parameter_session_unexposed = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "undo"})
        vector3_graph_parameter_session_unexpose_undo = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_graph_parameter_session.apply({"type": "redo"})
        vector3_graph_parameter_session_unexpose_redo = dynamic_module.preserve_document(
            vector3_graph_parameter_session.document)
        vector3_exposed_graph_parameter = dynamic_module.expose_node_parameter(
            dynamic_vector3_constant_document, dynamic_registry,
            "vector_source", "value", "surface_normal", "Surface Normal")
        vector3_edited_graph_parameter = dynamic_module.edit_graph_parameter(
            vector3_exposed_graph_parameter, dynamic_registry,
            "surface_normal", [0.1, 0.2, 0.3])
        vector3_graph_parameter_compile = dynamic_module.compile_document_from_data(
            vector3_edited_graph_parameter, dynamic_registry)
        vector3_graph_parameter_base_compile = dynamic_module.compile_document_from_data(
            vector3_exposed_graph_parameter, dynamic_registry)
        vector3_resident_parameter_candidate = dynamic_module.adapt_to_resident_generic_abi(
            vector3_graph_parameter_compile)
        vector3_resident_parameter_values = dynamic_module.pack_resident_generic_parameter_values(
            vector3_graph_parameter_compile)
        version2_graph_base = copy.deepcopy(connected_graph)
        version2_graph_base["schemaVersion"] = 2
        version2_graph_base["graphParameters"] = []
        version2_graph_without_exposed_unreachable = dynamic_module.add_node(
            version2_graph_base, dynamic_registry,
            "artifact.rd.constant.vector4.float32", 1, "unreachable_v4")
        version2_graph_with_exposed_unreachable = dynamic_module.expose_node_parameter(
            version2_graph_without_exposed_unreachable, dynamic_registry,
            "unreachable_v4", "value", "unused_tint", "Unused Tint")
        unreachable_parameter_hash_unchanged = (
            dynamic_module.compile_document_from_data(version2_graph_without_exposed_unreachable, dynamic_registry)["semanticHashSha256"] ==
            dynamic_module.compile_document_from_data(version2_graph_with_exposed_unreachable, dynamic_registry)["semanticHashSha256"])

        unknown_output_delete_diagnostic = diagnostic_for(lambda: dynamic_module.delete_node(
            unknown_output_document, dynamic_registry, "future_node"))
        duplicate_palette_node_diagnostic = diagnostic_for(lambda: dynamic_module.add_node(
            connected_graph, dynamic_registry, "artifact.rd.math.add.float32", 1, "sum_node"))
        explicit_conversion_type_mismatch_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(
            conversion_type_mismatch_graph, dynamic_registry))
        invalid_vector4_parameter_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_parameters(
            vector4_constant_document, dynamic_registry, "color_source", {"value": [0.1, 0.2, 0.3]}))
        invalid_vector4_component_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_parameters(
            vector4_constant_document, dynamic_registry, "color_source", {"value": [0.1, 0.2, float("inf"), 1.0]}))
        vector4_boundaries_document = dynamic_module.edit_node_parameters(
            vector4_constant_document, dynamic_registry, "color_source",
            {"value": [-0.0, 1.0 + 2.0 ** -24, 2.0 ** -149, -2.0 ** -149]})
        vector4_boundaries_compile = dynamic_module.compile_document_from_data(
            vector4_boundaries_document, dynamic_registry)
        vector4_repeated_edit_document = dynamic_module.edit_node_parameters(
            vector4_boundaries_document, dynamic_registry, "color_source",
            {"value": [-0.0, 1.0 + 2.0 ** -24, 2.0 ** -149, -2.0 ** -149]})
        vector4_repeated_edit_compile = dynamic_module.compile_document_from_data(
            vector4_repeated_edit_document, dynamic_registry)
        vector4_constant_undo = dynamic_module.create_document_command(
            "Edit Vector4 Constant", vector4_constant_document, vector4_boundaries_document)
        vector4_constant_undo_document = dynamic_module.apply_document_command(
            vector4_constant_undo, vector4_boundaries_document, direction="undo")
        vector4_constant_redo_document = dynamic_module.apply_document_command(
            vector4_constant_undo, vector4_constant_undo_document, direction="redo")
        failed_link_batch_document = dynamic_module.add_node(
            connected_graph, dynamic_registry, "artifact.rd.math.add.float32", 1, "batch_target")
        failed_link_batch_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes_batch(
            failed_link_batch_document, dynamic_registry,
            [("source_left", "value", "batch_target", "a"),
             ("source_right", "value", "batch_target", "a")]))
        malformed_link_batch_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes_batch(
            palette_added_graph, dynamic_registry,
            [("source_left", "value", "palette_added_add", "a"), ("bad-entry",)]))
        failed_composite_diagnostic = diagnostic_for(lambda: dynamic_module.add_node_with_connections(
            connected_graph, dynamic_registry, "artifact.rd.math.add.float32", 1, "failed_composite_add",
            [("source_left", "value", "failed_composite_add", "a"),
             ("source_right", "value", "failed_composite_add", "a")], "result"))
        unknown_version_editing = dynamic_module.validate_document_for_editing(unknown_version_document, dynamic_registry)
        with tempfile.TemporaryDirectory(prefix="shader-graph-rd-opaque-io-") as opaque_directory:
            opaque_path = Path(opaque_directory) / "repairable.json"
            dynamic_module.save_document(opaque_path, unknown_version_document)
            opaque_loaded_document = dynamic_module.load_document(opaque_path)
            opaque_loaded_editing = dynamic_module.validate_document_for_editing(opaque_loaded_document, dynamic_registry)
            opaque_edited_document = dynamic_module.edit_node_parameters(
                opaque_loaded_document, dynamic_registry, "constant_b", {"value": 0.75})
            opaque_edited_document = dynamic_module.edit_node_layout(
                opaque_edited_document, "constant_a", {"x": 99})
            dynamic_module.save_document(opaque_path, opaque_edited_document)
            opaque_resaved_document = dynamic_module.load_document(opaque_path)
            opaque_resaved_editing = dynamic_module.validate_document_for_editing(opaque_resaved_document, dynamic_registry)
            opaque_resaved_compile_diagnostic = diagnostic_for(
                lambda: dynamic_module.compile_document_from_data(opaque_resaved_document, dynamic_registry))
            opaque_persisted_command = dynamic_module.create_document_command(
                "Edit Known Data Beside Opaque Payload", opaque_loaded_document, opaque_edited_document)
            opaque_persisted_undo = dynamic_module.apply_document_command(
                opaque_persisted_command, opaque_resaved_document, direction="undo")
            dynamic_module.save_document(opaque_path, opaque_persisted_undo)
            opaque_undo_loaded = dynamic_module.load_document(opaque_path)
            opaque_persisted_redo = dynamic_module.apply_document_command(
                opaque_persisted_command, opaque_undo_loaded, direction="redo")
            dynamic_module.save_document(opaque_path, opaque_persisted_redo)
            opaque_redo_loaded = dynamic_module.load_document(opaque_path)
        temporarily_missing_registry = dict(dynamic_registry)
        temporarily_missing_document = copy.deepcopy(dynamic_document)
        temporarily_missing_document["nodes"][0]["semanticVersion"] = 2
        missing_registry_validation = dynamic_module.validate_document_for_editing(
            temporarily_missing_document, temporarily_missing_registry)
        restored_version_registry = dict(temporarily_missing_registry)
        restored_version_registry[("artifact.rd.constant.float32", 2)] = replace(
            constant_operation, semantic_version=2)
        restored_registry_validation = dynamic_module.validate_document_for_editing(
            temporarily_missing_document, restored_version_registry)
        restored_registry_compile = dynamic_module.compile_document_from_data(
            temporarily_missing_document, restored_version_registry)

        canonical_document_text = dynamic_module.serialize_document(connected_graph)
        canonical_round_trip = dynamic_module.parse_json_text(canonical_document_text)
        reordered_object_document = dict(reversed(list(connected_graph.items())))
        reordered_array_document = copy.deepcopy(connected_graph)
        reordered_array_document["nodes"].reverse()
        duplicate_key_diagnostic = diagnostic_for(lambda: dynamic_module.parse_json_text('{"x":1,"x":2}'))
        nonstandard_number_diagnostic = diagnostic_for(lambda: dynamic_module.parse_json_text('{"value":NaN}'))
        cyclic_snapshot_document = copy.deepcopy(connected_graph)
        cyclic_extension = {"vendor": "opaque"}
        cyclic_extension["self"] = cyclic_extension
        cyclic_snapshot_document["vendorExtension"] = cyclic_extension
        cyclic_snapshot_diagnostic = diagnostic_for(lambda: dynamic_module.create_document_command(
            "Reject Cyclic Snapshot", connected_graph, cyclic_snapshot_document))
        deeply_nested_value: Any = "leaf"
        for _ in range(1500):
            deeply_nested_value = [deeply_nested_value]
        deep_json_diagnostic = diagnostic_for(lambda: dynamic_module.canonical_json_text(deeply_nested_value))
        shared_extension = {"curve": [0.0, 1.0]}
        shared_reference_round_trip = dynamic_module.parse_json_text(dynamic_module.canonical_json_text(
            {"left": shared_extension, "right": shared_extension}))
        with tempfile.TemporaryDirectory(prefix="shader-graph-rd-io-") as temporary_directory:
            round_trip_path = Path(temporary_directory) / "graph.json"
            io_document = copy.deepcopy(connected_graph)
            io_document["vendorExtension"] = {"opaque": ["keep", 3, False]}
            dynamic_module.save_document(round_trip_path, io_document)
            saved_graph_bytes = round_trip_path.read_bytes()
            loaded_graph = dynamic_module.load_document(round_trip_path)
            unknown_extension_editing = dynamic_module.validate_document_for_editing(loaded_graph, dynamic_registry)
            stable_saved_graph_bytes = round_trip_path.read_bytes()
            unknown_extension_compile_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(loaded_graph, dynamic_registry))
            failed_save_diagnostic = diagnostic_for(lambda: dynamic_module.save_document(
                round_trip_path, {**io_document, "invalidNumber": float("inf")}))
            preserved_after_failed_save = round_trip_path.read_bytes() == saved_graph_bytes

        sealed_document = dynamic_module.seal_document(dynamic_document, dynamic_registry)
        sealed_round_trip = dynamic_module.preserve_document(sealed_document)
        sealed_presentation_compile = dynamic_module.compile_document_from_data(sealed_round_trip, presentation_only_registry)
        sealed_additive_version_compile = dynamic_module.compile_document_from_data(sealed_document, additive_version_registry)
        sealed_contract_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(sealed_document, changed_contract_registry))
        invalid_layout_coordinate_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_layout(connected_graph, "source_left", {"x": float("nan")}))
        invalid_layout_field_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_layout(connected_graph, "source_left", {"z": 3.0}))
        layout_only_edit = dynamic_module.edit_node_layout(connected_graph, "source_left", {"x": 25})
        layout_batch_edit = dynamic_module.edit_node_layout_batch(
            connected_graph, [("source_left", {"x": 25}), ("source_right", {"y": 200})])
        layout_boundary_edit = dynamic_module.edit_node_layout_batch(
            connected_graph, [("source_left", {"x": 1.0 + 2.0 ** -24, "y": -0.0}),
                              ("source_right", {"x": 2.0 ** -149})])
        layout_boundary_command = dynamic_module.create_document_command(
            "Move Nodes at Float32 Boundaries", connected_graph, layout_boundary_edit)
        layout_boundary_undo = dynamic_module.apply_document_command(
            layout_boundary_command, layout_boundary_edit, direction="undo")
        layout_boundary_redo = dynamic_module.apply_document_command(
            layout_boundary_command, layout_boundary_undo, direction="redo")
        duplicate_layout_batch_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_layout_batch(
            connected_graph, [("source_left", {"x": 25}), ("source_left", {"y": 25})]))
        invalid_layout_batch_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_layout_batch(
            connected_graph, [("source_left", {"x": 25}), ("source_right", {"y": float("nan")})]))

        unknown_socket_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(connected_graph, dynamic_registry, "source_left", "missing", "sum_node", "a"))
        multiple_writer_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(connected_graph, dynamic_registry, "source_right", "value", "sum_node", "a"))
        wrong_type_registry = dict(dynamic_registry)
        wrong_type_registry[("artifact.rd.math.add.float32", 1)] = replace(
            add_operation_for_graph,
            inputs=(replace(add_operation_for_graph.inputs[0], value_type="Vector4"), add_operation_for_graph.inputs[1]))
        wrong_type_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(connected_graph, wrong_type_registry, "source_left", "value", "sum_node", "a"))
        cycle_nodes = graph_nodes + [dynamic_module.instantiate_node(add_operation_for_graph, "second_sum")]
        cycle_graph = {"schemaVersion": 1, "kind": "artifact-shader-graph-rd-document", "nodes": cycle_nodes,
                       "links": [{"from": {"nodeId": "sum_node", "socketId": "result"}, "to": {"nodeId": "second_sum", "socketId": "a"}}],
                       "output": {"nodeId": "sum_node", "socketId": "result"}}
        cycle_link_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(cycle_graph, dynamic_registry, "second_sum", "result", "sum_node", "a"))
        missing_disconnect_diagnostic = diagnostic_for(lambda: dynamic_module.disconnect_nodes(connected_graph, "source_left", "value", "sum_node", "missing"))
        batch_disconnected_graph = dynamic_module.disconnect_nodes_batch(
            connected_graph,
            [("source_left", "value", "sum_node", "a"),
             ("source_right", "value", "sum_node", "b")])
        missing_batch_disconnect_diagnostic = diagnostic_for(lambda: dynamic_module.disconnect_nodes_batch(
            connected_graph,
            [("source_left", "value", "sum_node", "a"),
             ("source_right", "value", "sum_node", "missing")]))
        duplicate_batch_disconnect_diagnostic = diagnostic_for(lambda: dynamic_module.disconnect_nodes_batch(
            connected_graph,
            [("source_left", "value", "sum_node", "a"),
             ("source_left", "value", "sum_node", "a")]))
        deleted_output_diagnostic = diagnostic_for(lambda: dynamic_module.delete_node(connected_graph, dynamic_registry, "sum_node"))
        replacement_delete_graph = dynamic_module.delete_node(
            connected_graph, dynamic_registry, "sum_node", replacement_output=("source_left", "value"))
        replacement_delete_compile = dynamic_module.compile_document_from_data(replacement_delete_graph, dynamic_registry)
        invalid_output_diagnostic = diagnostic_for(lambda: dynamic_module.set_graph_output(connected_graph, dynamic_registry, "source_left", "missing"))
        duplicate_id_graph = copy.deepcopy(connected_graph)
        duplicate_id_graph["nodes"].append(copy.deepcopy(duplicate_id_graph["nodes"][0]))
        duplicate_id_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(duplicate_id_graph, dynamic_registry, "source_left", "value", "sum_node", "a"))
        malformed_link_graph = copy.deepcopy(connected_graph)
        malformed_link_graph["links"][0]["from"]["nodeId"] = []
        malformed_link_diagnostic = diagnostic_for(lambda: dynamic_module.connect_nodes(malformed_link_graph, dynamic_registry, "source_left", "value", "sum_node", "a"))
        stale_history_diagnostic = diagnostic_for(lambda: dynamic_module.apply_document_command(edit_command, disconnected_graph, direction="undo"))
        replaced_graph = dynamic_module.replace_node_operation(
            connected_graph, dynamic_registry, "sum_node", "artifact.rd.math.multiply.float32", 1)
        replaced_graph_compile = dynamic_module.compile_document_from_data(replaced_graph, dynamic_registry)
        replaced_graph_validation = dynamic_module.validate_document_for_editing(replaced_graph, dynamic_registry)
        replacement_command = dynamic_module.create_document_command(
            "Replace Add with Multiply", connected_graph, replaced_graph)
        replacement_undo = dynamic_module.apply_document_command(
            replacement_command, replaced_graph, direction="undo")
        replacement_redo = dynamic_module.apply_document_command(
            replacement_command, replacement_undo, direction="redo")
        incompatible_replacement_diagnostic = diagnostic_for(lambda: dynamic_module.replace_node_operation(
            connected_graph, dynamic_registry, "sum_node", "artifact.rd.math.add.vector4.float32", 1))
        replacement_default_registry = dict(dynamic_registry)
        changed_default_constant = replace(
            constant_operation, operation_id="vendor.rd.constant.float32",
            parameters=(replace(constant_operation.parameters[0], default=1.0),))
        replacement_default_registry[(changed_default_constant.operation_id, 1)] = changed_default_constant
        unmaterialized_default_document = {
            "schemaVersion": 1, "kind": "artifact-shader-graph-rd-document",
            "nodes": [{"nodeId": "legacy_constant", "operationId": constant_operation.operation_id,
                       "semanticVersion": 1, "parameters": {}, "layout": {"x": 0, "y": 0}}],
            "links": [], "output": {"nodeId": "legacy_constant", "socketId": "value"},
        }
        changed_unmaterialized_default_diagnostic = diagnostic_for(lambda: dynamic_module.replace_node_operation(
            unmaterialized_default_document, replacement_default_registry, "legacy_constant",
            changed_default_constant.operation_id, 1))
        sealed_replacement_source = dynamic_module.seal_document(connected_graph, dynamic_registry)
        sealed_replacement = dynamic_module.replace_node_operation(
            sealed_replacement_source, dynamic_registry, "sum_node", "artifact.rd.math.multiply.float32", 1)
        sealed_replacement_compile = dynamic_module.compile_document_from_data(sealed_replacement, dynamic_registry)
        replacement_session = local_ui_module.Session(dynamic_registry, connected_graph, None, [])
        replacement_session.apply({"type": "replace", "nodeId": "sum_node",
                                   "operationId": "artifact.rd.math.multiply.float32", "semanticVersion": 1})
        replacement_session_after = dynamic_module.preserve_document(replacement_session.document)
        replacement_session.apply({"type": "undo"})
        replacement_session_undo = dynamic_module.preserve_document(replacement_session.document)
        replacement_session.apply({"type": "redo"})
        replacement_session_redo = dynamic_module.preserve_document(replacement_session.document)
        vector3_replaced_graph = dynamic_module.replace_node_operation(
            dynamic_vector3_arithmetic_document, dynamic_registry, "difference",
            "artifact.rd.math.multiply.vector3.float32", 1)
        vector3_replaced_compile = dynamic_module.compile_document_from_data(
            vector3_replaced_graph, dynamic_registry)
        vector3_replacement_command = dynamic_module.create_document_command(
            "Replace Vector3 Subtract with Multiply", dynamic_vector3_arithmetic_document,
            vector3_replaced_graph)
        vector3_replacement_undo = dynamic_module.apply_document_command(
            vector3_replacement_command, vector3_replaced_graph, direction="undo")
        vector3_replacement_redo = dynamic_module.apply_document_command(
            vector3_replacement_command, vector3_replacement_undo, direction="redo")
        vector3_replacement_session = local_ui_module.Session(
            dynamic_registry, dynamic_vector3_arithmetic_document, None, [])
        vector3_replacement_session.apply({
            "type": "replace", "nodeId": "difference",
            "operationId": "artifact.rd.math.multiply.vector3.float32", "semanticVersion": 1})
        vector3_replacement_session_after = dynamic_module.preserve_document(
            vector3_replacement_session.document)
        vector3_replacement_session.apply({"type": "undo"})
        vector3_replacement_session_undo = dynamic_module.preserve_document(
            vector3_replacement_session.document)
        vector3_replacement_session.apply({"type": "redo"})
        vector3_replacement_session_redo = dynamic_module.preserve_document(
            vector3_replacement_session.document)
        batch_failure_diagnostic = diagnostic_for(lambda: dynamic_module.apply_document_batch(connected_graph, [
            lambda graph: dynamic_module.edit_node_layout(graph, "source_left", {"x": 777}),
            lambda graph: dynamic_module.edit_node_parameters(graph, dynamic_registry, "source_left", {"unknown": 4.0}),
        ]))
        discontinuous_command_diagnostic = diagnostic_for(lambda: dynamic_module.compose_document_commands(
            "Invalid group", [parameter_command, disconnect_command]))
        atomic_parameter_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_parameters(
            created_constant_document, dynamic_registry, "created_constant", {"value": 0.5, "unknown": 3.0}))
        invalid_parameter_diagnostic = diagnostic_for(lambda: dynamic_module.edit_node_parameters(
            created_constant_document, dynamic_registry, "created_constant", {"value": 1.0e100}))
        explicit_migration = dynamic_module.migrate_document_versions(
            sealed_document, additive_version_registry, {"artifact.rd.constant.float32@1": "artifact.rd.constant.float32@2"})
        explicit_migration_compile = dynamic_module.compile_document_from_data(explicit_migration, additive_version_registry)
        v2_only_registry = {key: value for key, value in additive_version_registry.items() if key[1] == 2 or key[0] != "artifact.rd.constant.float32"}
        silent_upgrade_diagnostic = diagnostic_for(lambda: dynamic_module.compile_document_from_data(sealed_document, v2_only_registry))
        changed_v2_registry = dict(additive_version_registry)
        changed_v2_registry[("artifact.rd.constant.float32", 2)] = replace(constant_v1, semantic_version=2, implementation_id="float.multiply")
        unsafe_migration_diagnostic = diagnostic_for(lambda: dynamic_module.migrate_document_versions(
            sealed_document, changed_v2_registry, {"artifact.rd.constant.float32@1": "artifact.rd.constant.float32@2"}))
        changed_default_v2_registry = dict(additive_version_registry)
        changed_default_v2_registry[("artifact.rd.constant.float32", 2)] = replace(
            constant_v1, semantic_version=2, parameters=(replace(constant_v1.parameters[0], default=0.25),))
        default_migration_diagnostic = diagnostic_for(lambda: dynamic_module.migrate_document_versions(
            sealed_document, changed_default_v2_registry, {"artifact.rd.constant.float32@1": "artifact.rd.constant.float32@2"}))
        changed_requiredness_v2_registry = dict(additive_version_registry)
        add_v1 = changed_requiredness_v2_registry[("artifact.rd.math.add.float32", 1)]
        changed_requiredness_v2_registry[("artifact.rd.math.add.float32", 2)] = replace(
            add_v1, semantic_version=2, inputs=(replace(add_v1.inputs[0], required=False), add_v1.inputs[1]))
        requiredness_migration_diagnostic = diagnostic_for(lambda: dynamic_module.migrate_document_versions(
            sealed_document, changed_requiredness_v2_registry, {"artifact.rd.math.add.float32@1": "artifact.rd.math.add.float32@2"}))
        cross_identity_v2_registry = dict(additive_version_registry)
        cross_identity_v2_registry[("vendor.rd.constant.float32", 2)] = replace(
            constant_v1, operation_id="vendor.rd.constant.float32", semantic_version=2)
        cross_identity_migration_diagnostic = diagnostic_for(lambda: dynamic_module.migrate_document_versions(
            sealed_document, cross_identity_v2_registry, {"artifact.rd.constant.float32@1": "vendor.rd.constant.float32@2"}))

        layout_variant = copy.deepcopy(dynamic_document)
        layout_variant["nodes"][0]["layout"]["x"] += 101
        layout_result = dynamic_module.compile_document_from_data(layout_variant, dynamic_registry)
        unreachable_variant = copy.deepcopy(dynamic_document)
        unreachable_variant["nodes"].append({
            "nodeId": "unreachable_constant", "operationId": "artifact.rd.constant.float32",
            "semanticVersion": 1, "parameters": {"value": 17.0}, "layout": {"x": -20, "y": 500}
        })
        unreachable_result = dynamic_module.compile_document_from_data(unreachable_variant, dynamic_registry)
        reordered_variant = copy.deepcopy(dynamic_document)
        reordered_variant["nodes"].reverse()
        reordered_variant["links"].reverse()
        reordered_result = dynamic_module.compile_document_from_data(reordered_variant, dynamic_registry)
        bad_version = copy.deepcopy(dynamic_document)
        bad_version["nodes"][0]["semanticVersion"] = 2
        malformed_version_documents = []
        malformed_version_edit_diagnostics = []
        malformed_version_validation_codes = []
        for malformed_version in (True, [], {}):
            malformed_document = copy.deepcopy(dynamic_document)
            malformed_document["nodes"][0]["semanticVersion"] = malformed_version
            malformed_version_documents.append(malformed_document)
            malformed_version_edit_diagnostics.append(diagnostic_for(
                lambda document=malformed_document: dynamic_module.edit_node_parameters(
                    document, dynamic_registry, "constant_a", {"value": 0.75})))
            malformed_validation = dynamic_module.validate_document_for_editing(
                malformed_document, dynamic_registry)
            malformed_version_validation_codes.append(
                [item["code"] for item in malformed_validation["diagnostics"]
                 if item["path"] == "/nodes/0/semanticVersion"])
        wrong_kind = copy.deepcopy(dynamic_document)
        wrong_kind["kind"] = "unsupported"
        wrong_schema_version = copy.deepcopy(dynamic_document)
        wrong_schema_version["schemaVersion"] = 99
        missing_link_array = copy.deepcopy(dynamic_document)
        missing_link_array["links"] = None
        duplicate_writer = copy.deepcopy(dynamic_document)
        duplicate_writer["links"].append(copy.deepcopy(duplicate_writer["links"][0]))
        cyclic_graph = copy.deepcopy(dynamic_document)
        cyclic_graph["links"][0]["from"] = {"nodeId": "result", "socketId": "result"}
        cyclic_validation = dynamic_module.validate_document_for_editing(cyclic_graph, dynamic_registry)
        multi_invalid_graph = copy.deepcopy(connected_graph)
        multi_invalid_graph["nodes"][0]["parameters"]["value"] = 1.0e100
        multi_invalid_graph["links"][1]["from"]["socketId"] = "missing_output"
        multi_invalid_graph["links"].append({"from": {"nodeId": "source_right", "socketId": "value"},
                                              "to": {"nodeId": "sum_node", "socketId": "a"}})
        multi_invalid_validation = dynamic_module.validate_document_for_editing(multi_invalid_graph, dynamic_registry)
        missing_input = copy.deepcopy(dynamic_document)
        missing_input["links"].pop(0)
        collision_graph = copy.deepcopy(dynamic_document)
        collision_graph["nodes"].append({
            "nodeId": "constant-a", "operationId": "artifact.rd.constant.float32",
            "semanticVersion": 1, "parameters": {"value": 1.0}, "layout": {"x": 0, "y": 0}
        })
        non_finite_graph = copy.deepcopy(dynamic_document)
        non_finite_graph["nodes"][0]["parameters"]["value"] = 1.0e100
        injected_registry = copy.deepcopy(dynamic_registry_source)
        injected_registry["operations"][0]["HLSL"] = "arbitrary source"
        changed_type_registry = dict(dynamic_registry)
        add_key = ("artifact.rd.math.add.float32", 1)
        add_operation = changed_type_registry[add_key]
        changed_type_registry[add_key] = replace(
            add_operation,
            inputs=(replace(add_operation.inputs[0], value_type="Vector4"), add_operation.inputs[1]))
        diagnostic_probes = {
            "unsupportedSchemaVersionRejected": lambda: dynamic_module.compile_document_from_data(wrong_schema_version, dynamic_registry),
            "unsupportedDocumentKindRejected": lambda: dynamic_module.compile_document_from_data(wrong_kind, dynamic_registry),
            "requiredLinkArrayRejected": lambda: dynamic_module.compile_document_from_data(missing_link_array, dynamic_registry),
            "unsupportedOperationVersionRejected": lambda: dynamic_module.compile_document_from_data(bad_version, dynamic_registry),
            "multipleInputWritersRejected": lambda: dynamic_module.compile_document_from_data(duplicate_writer, dynamic_registry),
            "cyclesRejectedBeforeOutputPruning": lambda: dynamic_module.compile_document_from_data(cyclic_graph, dynamic_registry),
            "requiredInputEnforced": lambda: dynamic_module.compile_document_from_data(missing_input, dynamic_registry),
            "sanitizedIdentifierCollisionRejected": lambda: dynamic_module.compile_document_from_data(collision_graph, dynamic_registry),
            "nonFiniteBinary32ParameterRejected": lambda: dynamic_module.compile_document_from_data(non_finite_graph, dynamic_registry),
            "inlineShaderSourceRejected": lambda: dynamic_module.reject_code_payload(injected_registry),
            "socketTypeMismatchRejected": lambda: dynamic_module.compile_document_from_data(dynamic_document, changed_type_registry),
        }
        diagnostics = {case_id: diagnostic_for(action) for case_id, action in diagnostic_probes.items()}
        unknown_registry_field = copy.deepcopy(dynamic_registry_source)
        unknown_registry_field["operations"][0]["vendorMetadata"] = {"opaque": True}
        duplicate_operation_registry = copy.deepcopy(dynamic_registry_source)
        duplicate_operation_registry["operations"].append(copy.deepcopy(duplicate_operation_registry["operations"][0]))
        duplicate_socket_registry = copy.deepcopy(dynamic_registry_source)
        duplicate_socket_target = next(operation for operation in duplicate_socket_registry["operations"]
                                       if operation.get("operationId") == "artifact.rd.math.add.float32")
        duplicate_socket_target["inputs"][1]["socketId"] = "a"
        duplicate_operation_path = f'/operations/{len(dynamic_registry_source["operations"])}'
        duplicate_socket_operation_index = next(
            index for index, operation in enumerate(dynamic_registry_source["operations"])
            if operation.get("operationId") == "artifact.rd.math.add.float32")
        non_string_implementation_registry = copy.deepcopy(dynamic_registry_source)
        non_string_implementation_registry["operations"][0]["implementationId"] = {"untrusted": "tag"}
        non_string_socket_type_registry = copy.deepcopy(dynamic_registry_source)
        non_string_socket_type_registry["operations"][0]["outputs"][0]["valueType"] = ["Float"]
        deep_registry_value = "[" * (dynamic_module.MAX_JSON_NESTING + 1) + "0" + "]" * (dynamic_module.MAX_JSON_NESTING + 1)
        with tempfile.TemporaryDirectory(prefix="shader-graph-rd-registry-") as registry_directory:
            registry_directory = Path(registry_directory)
            registry_diagnostics = []
            for index, candidate_registry in enumerate((unknown_registry_field, duplicate_operation_registry,
                                                        duplicate_socket_registry, non_string_implementation_registry,
                                                        non_string_socket_type_registry)):
                registry_path = registry_directory / f"registry-{index}.json"
                dynamic_module.save_document(registry_path, candidate_registry)
                registry_diagnostics.append(diagnostic_for(lambda path=registry_path: dynamic_module.load_registry(path)))
            deep_registry_path = registry_directory / "registry-deep-value.json"
            deep_registry_path.write_text(
                '{"schemaVersion":1,"kind":"artifact-shader-graph-rd-operation-registry",'
                '"operations":[],"vendor":' + deep_registry_value + '}',
                encoding="utf-8", newline="\n")
            deep_registry_diagnostic = diagnostic_for(lambda: dynamic_module.load_registry(deep_registry_path))
        divide_golden_case = next((item for item in dynamic_math_golden.get("cases", [])
                                   if isinstance(item, dict) and item.get("operation") == "divide"), {})
        divide_golden_a = divide_golden_case.get("inputs", {}).get("A", [])
        divide_golden_b = divide_golden_case.get("inputs", {}).get("B", [])
        divide_golden_result = divide_golden_case.get("expectedBinary32", [])
        divide_document_nodes = dynamic_divide_document.get("nodes", [])
        divide_numerator_node = divide_document_nodes[0] if len(divide_document_nodes) > 0 else {}
        divide_divisor_node = divide_document_nodes[1] if len(divide_document_nodes) > 1 else {}
        vector4_divide_nodes = dynamic_vector4_divide_document.get("nodes", [])
        vector4_divide_numerator_node = vector4_divide_nodes[0] if len(vector4_divide_nodes) > 0 else {}
        vector4_divide_denominator_node = vector4_divide_nodes[1] if len(vector4_divide_nodes) > 1 else {}
        vector4_subtract_nodes = dynamic_vector4_subtract_document.get("nodes", [])
        vector4_subtract_minuend = vector4_subtract_nodes[0] if len(vector4_subtract_nodes) > 0 else {}
        vector4_subtract_subtrahend = vector4_subtract_nodes[1] if len(vector4_subtract_nodes) > 1 else {}
        vector4_subtract_result = [
            struct.unpack("<f", struct.pack("<f", left - right))[0]
            for left, right in zip(vector4_subtract_minuend.get("parameters", {}).get("value", []),
                                   vector4_subtract_subtrahend.get("parameters", {}).get("value", []))
        ]
        vector4_subtract_result_bits = [struct.pack(">f", value).hex().upper() for value in vector4_subtract_result]
        vector4_invert_case = next((case for case in invert_golden.get("cases", [])
                                    if isinstance(case, dict) and case.get("caseId") == "factor-half"), {})
        vector4_invert_nodes = dynamic_vector4_invert_document.get("nodes", [])
        vector4_invert_color_node = next((node for node in vector4_invert_nodes
                                          if isinstance(node, dict) and node.get("nodeId") == "source_color"), {})
        vector4_invert_factor_node = next((node for node in vector4_invert_nodes
                                           if isinstance(node, dict) and node.get("nodeId") == "source_factor"), {})
        vector4_invert_golden_candidates = []
        for invert_case in invert_golden.get("cases", []):
            if not isinstance(invert_case, dict):
                continue
            case_document = copy.deepcopy(dynamic_vector4_invert_document)
            case_nodes = {node.get("nodeId"): node for node in case_document.get("nodes", [])
                          if isinstance(node, dict)}
            case_nodes["source_color"]["parameters"]["value"] = invert_case.get("color")
            case_nodes["source_factor"]["parameters"]["value"] = invert_case.get("factor")
            case_candidate = dynamic_module.compile_document_from_data(case_document, dynamic_registry)
            vector4_invert_golden_candidates.append((invert_case, case_document, case_candidate))
        clamp_golden_candidates = []
        for clamp_case in clamp_golden.get("cases", []):
            if not isinstance(clamp_case, dict):
                continue
            case_document = copy.deepcopy(dynamic_clamp_document)
            case_nodes = {node.get("nodeId"): node for node in case_document.get("nodes", [])
                          if isinstance(node, dict)}
            case_nodes["input_value"]["parameters"]["value"] = clamp_case.get("value")
            case_nodes["input_minimum"]["parameters"]["value"] = clamp_case.get("minimum")
            case_nodes["input_maximum"]["parameters"]["value"] = clamp_case.get("maximum")
            case_candidate = dynamic_module.compile_document_from_data(case_document, dynamic_registry)
            clamp_golden_candidates.append((clamp_case, case_document, case_candidate))
        dynamic_mix_rgb_fragments = {
            "mix": "lerp((n_mix_color1_value).rgb, (n_mix_color2_value).rgb, saturate(n_mix_factor_value))",
            "add": "((n_mix_color1_value).rgb + (n_mix_color2_value).rgb)",
            "multiply": "((n_mix_color1_value).rgb * (n_mix_color2_value).rgb)",
            "screen": "(1.0f - (1.0f - (n_mix_color1_value).rgb) * (1.0f - (n_mix_color2_value).rgb))",
            "overlay": "step(0.5f, (n_mix_color1_value).rgb)",
            "darken": "min((n_mix_color1_value).rgb, (n_mix_color2_value).rgb)",
            "lighten": "max((n_mix_color1_value).rgb, (n_mix_color2_value).rgb)",
            "difference": "abs((n_mix_color1_value).rgb - (n_mix_color2_value).rgb)",
        }
        dynamic_candidate_results = [
            regenerated_candidate, regenerated_subtract_candidate,
            regenerated_divide_candidate, regenerated_vector4_divide_candidate,
            regenerated_vector4_subtract_candidate, regenerated_vector4_invert_candidate,
            regenerated_clamp_candidate, regenerated_vector4_mix_candidate,
            regenerated_vector4_separate_candidate, regenerated_vector4_ramp_candidate,
            regenerated_optional_default_candidate, regenerated_optional_vector4_default_candidate,
            regenerated_vector2_uv_candidate, regenerated_vector2_mapping_candidate,
            edited_vector2_mapping_candidate,
            regenerated_base_color_texture_candidate,
            regenerated_mapped_base_color_texture_candidate,
            regenerated_texture_channels_candidate,
        ]
        def typed_ir_hash_matches(candidate: dict[str, Any]) -> bool:
            typed_ir = candidate.get("typedIR")
            if not isinstance(typed_ir, dict):
                return False
            encoded_ir = json.dumps(
                typed_ir, sort_keys=True, separators=(",", ":"), ensure_ascii=False
            ).encode("utf-8")
            return candidate.get("typedIRSemanticHashSha256") == hashlib.sha256(encoded_ir).hexdigest().upper()
        def typed_ir_edges_are_well_typed(candidate: dict[str, Any]) -> bool:
            typed_ir = candidate.get("typedIR")
            if (not isinstance(typed_ir, dict) or
                    typed_ir.get("schemaId") != "artifact.shadergraph.typed-ir" or
                    typed_ir.get("schemaVersion") != 1 or
                    not isinstance(typed_ir.get("operations"), list)):
                return False
            operations = typed_ir["operations"]
            by_node = {item.get("nodeId"): item for item in operations
                       if isinstance(item, dict) and isinstance(item.get("nodeId"), str)}
            if len(by_node) != len(operations):
                return False
            operation_order = {item["nodeId"]: index
                               for index, item in enumerate(operations)}
            outputs_by_node = {}
            for operation in operations:
                if not isinstance(operation, dict):
                    return False
                raw_outputs = operation.get("outputs")
                raw_inputs = operation.get("inputs")
                if not isinstance(raw_outputs, list) or not isinstance(raw_inputs, list):
                    return False
                output_types = {}
                for output_socket in raw_outputs:
                    if (not isinstance(output_socket, dict) or
                            not isinstance(output_socket.get("socketId"), str) or
                            not isinstance(output_socket.get("valueType"), str) or
                            output_socket["socketId"] in output_types):
                        return False
                    output_types[output_socket["socketId"]] = output_socket["valueType"]
                outputs_by_node[operation["nodeId"]] = output_types
                for input_socket in raw_inputs:
                    if (not isinstance(input_socket, dict) or
                            not isinstance(input_socket.get("socketId"), str) or
                            not isinstance(input_socket.get("valueType"), str) or
                            not isinstance(input_socket.get("required"), bool)):
                        return False
                    producer = input_socket.get("producer")
                    default_value = input_socket.get("defaultValue")
                    if default_value is not None:
                        if input_socket["valueType"] == "Float":
                            if isinstance(default_value, bool) or not isinstance(default_value, (int, float)):
                                return False
                        elif (input_socket["valueType"] == "Vector4" and
                              (not isinstance(default_value, list) or len(default_value) != 4 or
                               any(isinstance(lane, bool) or not isinstance(lane, (int, float))
                                   for lane in default_value))):
                            return False
                    if producer is None:
                        if input_socket.get("required") or default_value is None:
                            return False
                        continue
                    if (not isinstance(producer, dict) or
                            not isinstance(producer.get("nodeId"), str) or
                            not isinstance(producer.get("socketId"), str)):
                        return False
                    producer_operation = by_node.get(producer.get("nodeId"))
                    if (producer_operation is None or
                            operation_order.get(producer["nodeId"], len(operations)) >=
                            operation_order[operation["nodeId"]]):
                        return False
                    producer_type = outputs_by_node.get(producer["nodeId"], {}).get(
                        producer.get("socketId"))
                    if producer_type is None or producer_type != input_socket.get("valueType"):
                        return False
            output = typed_ir.get("output")
            if not isinstance(output, dict):
                return False
            return outputs_by_node.get(output.get("nodeId"), {}).get(
                output.get("socketId")) == output.get("valueType")
        typed_ir_contract_expected = (
            "schema artifact.shadergraph.typed-ir v1; canonical SHA-256; typed socket operands, outputs, "
            "parameter values or shared graph-parameter references and values, and typed host-context "
            "identities; excludes HLSL expressions, editor layout, and source code"
        )
        socket_default_contract_expected = (
            "optional input sockets carry a typed binary32 operation default; schema version 3 stores "
            "editable per-node overrides; a connected producer overrides its stored default; required "
            "inputs still need links; only an unconnected default participates in active semantic and "
            "typed-IR identity"
        )
        expected_generic_resource_requirements = [
            {"resourceKey": "artifact.shadergraph.image.image_sample", "resourceId": "image.albedo",
             "nodeId": "image_sample", "resourceKind": "sampledTexture2D"},
            {"resourceKey": "artifact.shadergraph.image.roughness_sample", "resourceId": "image.roughness",
             "nodeId": "roughness_sample", "resourceKind": "sampledTexture2D"},
        ]
        expected_generic_asset_resolutions = [
            {"resourceKey": "artifact.shadergraph.image.image_sample", "resourceId": "image.albedo",
             "assetId": "11111111-1111-4111-8111-111111111111", "assetType": "Image"},
            {"resourceKey": "artifact.shadergraph.image.roughness_sample", "resourceId": "image.roughness",
             "assetId": "33333333-3333-4333-8333-333333333333", "assetType": "Image"},
        ]
        generic_image_relink_contract_ok = (
            generic_image_candidate == regenerated_generic_image_candidate and
            regenerated_generic_image_candidate.get("selectedOutput") ==
            {"nodeId": "combine", "socketId": "result", "valueType": "Vector4"} and
            regenerated_generic_image_candidate.get("activeNodeIds") ==
            ["combine", "image_sample", "roughness_sample", "uv"] and
            regenerated_generic_image_candidate.get("resourceRequirements") ==
            expected_generic_resource_requirements and
            regenerated_generic_image_candidate.get("assetResolutions") ==
            expected_generic_asset_resolutions and
            len(regenerated_generic_image_candidate.get("resourceRequirementIdentitySha256", "")) == 64 and
            regenerated_generic_image_candidate.get("resourceBindingStatus") == "not-projected" and
            regenerated_generic_image_candidate.get("resourceBindingKeySha256") is None and
            generic_image_binding_projection ==
            regenerated_generic_image_candidate.get("runtimeBindingProjection") and
            generic_image_binding_projection.get("resourceBindingStatus") == "resolved" and
            len(generic_image_binding_projection.get("resourceBindingKeySha256", "")) == 64 and
            generic_image_binding_projection.get("bindingIdentityFields") ==
            generic_image_contract.get("runtimeBindingProjectionContract", {}).get(
                "bindingIdentityFields") and
            regenerated_generic_image_candidate.get("dynamicResolverBindingAdapter", {}).get(
                "projection") == generic_image_dynamic_binding and
            regenerated_generic_image_candidate.get("dynamicResolverBindingAdapter", {}).get(
                "compilerCandidate") == generic_image_dynamic_candidate and
            generic_image_dynamic_candidate.get("status") == "candidate-lowered-no-shader-runtime" and
            generic_image_dynamic_candidate.get("outputType") == "Vector4" and
            len(generic_image_dynamic_candidate.get("resourceRequirements", [])) == 2 and
            len({item.get("shaderSymbol") for item in
                 generic_image_dynamic_candidate.get("resourceRequirements", [])}) == 2 and
            "g_GraphTexture_image_sample.Sample(g_GraphSampler_image_sample, n_uv_value)" in
            generic_image_dynamic_candidate.get("hlslBody", "") and
            "g_GraphTexture_roughness_sample.Sample(g_GraphSampler_roughness_sample, n_uv_value)" in
            generic_image_dynamic_candidate.get("hlslBody", "") and
            generic_image_dynamic_binding.get("resourceBindingStatus") == "resolved" and
            len(generic_image_dynamic_binding.get("resolvedResourceBindings", [])) == 2 and
            len(generic_image_dynamic_binding.get("resourceBindingKeySha256", "")) == 64 and
            generic_image_dynamic_binding.get("resourceBindingKeySha256") !=
            relinked_generic_dynamic_binding.get("resourceBindingKeySha256") and
            generic_image_dynamic_binding.get("resourceBindingKeySha256") !=
            revision_changed_generic_dynamic_binding.get("resourceBindingKeySha256") and
            generic_image_dynamic_binding.get("resourceBindingKeySha256") !=
            sampler_changed_generic_dynamic_binding.get("resourceBindingKeySha256") and
            generic_image_dynamic_binding.get("resourceBindingKeySha256") !=
            color_changed_generic_dynamic_binding.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("semanticHashSha256") ==
            relinked_generic_dynamic_candidate.get("semanticHashSha256") ==
            revision_changed_generic_dynamic_candidate.get("semanticHashSha256") ==
            sampler_changed_generic_dynamic_candidate.get("semanticHashSha256") ==
            color_changed_generic_dynamic_candidate.get("semanticHashSha256") and
            generic_image_dynamic_candidate.get("hlslBody") ==
            relinked_generic_dynamic_candidate.get("hlslBody") ==
            revision_changed_generic_dynamic_candidate.get("hlslBody") ==
            sampler_changed_generic_dynamic_candidate.get("hlslBody") ==
            color_changed_generic_dynamic_candidate.get("hlslBody") and
            generic_image_dynamic_candidate.get("resourceBindingKeySha256") !=
            relinked_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("resourceBindingKeySha256") !=
            revision_changed_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("resourceBindingKeySha256") !=
            sampler_changed_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("resourceBindingKeySha256") !=
            color_changed_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            generic_image_dynamic_binding.get("resourceBindingKeySha256") ==
            rewired_uv_generic_dynamic_binding.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("resourceBindingKeySha256") ==
            rewired_uv_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            generic_image_dynamic_candidate.get("semanticHashSha256") !=
            rewired_uv_generic_dynamic_candidate.get("semanticHashSha256") and
            generic_image_dynamic_candidate.get("hlslBody") !=
            rewired_uv_generic_dynamic_candidate.get("hlslBody") and
            generic_image_dynamic_projection.get("resourceRequirementIdentitySha256") ==
            rewired_uv_generic_projection.get("resourceRequirementIdentitySha256") and
            unresolved_generic_dynamic_binding.get("resourceBindingStatus") == "unresolved" and
            unresolved_generic_dynamic_binding.get("resourceBindingKeySha256") is None and
            unresolved_generic_dynamic_candidate.get("resourceBindingStatus") == "unresolved" and
            unresolved_generic_dynamic_candidate.get("resourceBindingKeySha256") is None and
            regenerated_generic_image_candidate.get("resourceRequirements") ==
            relinked_generic_image_candidate.get("resourceRequirements") and
            regenerated_generic_image_candidate.get("resourceRequirementIdentitySha256") ==
            relinked_generic_image_candidate.get("resourceRequirementIdentitySha256") and
            regenerated_generic_image_candidate.get("assetResolutions") !=
            relinked_generic_image_candidate.get("assetResolutions") and
            generic_image_binding_projection.get("resourceBindingKeySha256") !=
            relinked_generic_binding_projection.get("resourceBindingKeySha256") and
            generic_image_binding_projection.get("resourceBindingKeySha256") !=
            revision_changed_generic_binding_projection.get("resourceBindingKeySha256") and
            generic_image_projection.get("resourceRequirementIdentitySha256") ==
            sampler_changed_generic_projection.get("resourceRequirementIdentitySha256") and
            generic_image_binding_projection.get("resourceBindingKeySha256") !=
            sampler_changed_generic_binding_projection.get("resourceBindingKeySha256") and
            generic_image_projection.get("resourceRequirementIdentitySha256") ==
            color_changed_generic_projection.get("resourceRequirementIdentitySha256") and
            generic_image_binding_projection.get("resourceBindingKeySha256") !=
            color_changed_generic_binding_projection.get("resourceBindingKeySha256") and
            unresolved_generic_binding_projection.get("resourceBindingStatus") == "unresolved" and
            unresolved_generic_binding_projection.get("resourceBindingKeySha256") is None and
            regenerated_generic_image_candidate.get("assetRelinkProjection", {}).get(
                "resourceRequirementsUnchanged") is True and
            regenerated_generic_image_candidate.get("assetRelinkProjection", {}).get(
                "resourceRequirementIdentityUnchanged") is True and
            regenerated_generic_image_candidate.get("assetRelinkProjection", {}).get(
                "resourceBindingKeyChanged") is True and
            regenerated_generic_image_candidate.get("assetRelinkProjection", {}).get(
                "shaderCodeIdentity") == "not-measured-no-shader-code-is-generated" and
            all(regenerated_generic_image_candidate.get("bindingIdentityMutationProbes", {}).values()) and
            regenerated_generic_image_candidate.get("mappedUvDynamicResolverAdapter", {}).get(
                "compilerCandidate") == mapped_generic_dynamic_candidate and
            regenerated_generic_image_candidate.get("mappedUvDynamicResolverAdapter", {}).get(
                "directUvCompilerCandidate") == direct_uv_generic_dynamic_candidate and
            mapped_generic_projection.get("documentStatus") == "valid" and
            mapped_generic_projection.get("activeNodeIds") == ["image_sample", "mapping", "uv"] and
            mapped_generic_dynamic_document["nodes"][1]["parameters"] ==
            generic_image_mapped_uv_document["nodes"][1]["parameters"] and
            mapped_generic_dynamic_document == generic_image_mapped_dynamic_document and
            hashlib.sha256(generic_image_mapped_dynamic_document_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("mappedUvDynamicDocumentSha256") and
            "float2 n_mapping_vector = ((n_uv_value * float2(2.0f, 0.5f)) + float2(0.25f, -0.125f));" in
            mapped_generic_dynamic_candidate.get("hlslBody", "") and
            "g_GraphTexture_image_sample.Sample(g_GraphSampler_image_sample, n_mapping_vector)" in
            mapped_generic_dynamic_candidate.get("hlslBody", "") and
            mapped_generic_dynamic_candidate.get("resourceRequirements") ==
            direct_uv_generic_dynamic_candidate.get("resourceRequirements") and
            mapped_generic_dynamic_candidate.get("resourceBindingKeySha256") ==
            direct_uv_generic_dynamic_candidate.get("resourceBindingKeySha256") and
            mapped_generic_dynamic_candidate.get("semanticHashSha256") !=
            direct_uv_generic_dynamic_candidate.get("semanticHashSha256") and
            "artifact.mapping.vector2.scale_offset" in
            generic_image_contract.get("supportedSubset", []) and
            generic_image_contract.get("mappedUvContract") == {
                "operationId": "artifact.mapping.vector2.scale_offset@1",
                "typedBehavior": "four finite Float32 parameters are preserved by the schema-v4 adapter and lowered by the closed dynamic registry implementation",
                "sampling": "the Mapping output feeds Image Texture uv; emitted Sample consumes n_mapping_vector",
                "identity": "resource requirements and resolved binding key equal the same document with a direct UV link; graph semantic and HLSL source identity differ",
                "runtimeStatus": "candidate generation only; production persistence, shader compilation, image sampling, and GPU behavior remain unverified",
            } and
            invalid_generic_mapping_parameter_diagnostic == {
                "code": "parameter.float32.invalid", "severity": "error",
                "path": "/nodes/1/parameters/scaleX",
                "message": "mapping parameter must fit binary32"} and
            generic_image_mapped_uv_document["nodes"][1]["parameters"]["scaleX"] == 2.0 and
            hashlib.sha256(generic_image_mapped_uv_document_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("mappedUvDocumentSha256") and
            hashlib.sha256(generic_image_document_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("documentSha256") and
            hashlib.sha256(generic_image_catalog_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("assetCatalogSha256") and
            hashlib.sha256(generic_image_source_states_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("assetSourceStatesSha256") and
            hashlib.sha256(generic_image_prototype_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("prototypeSha256") and
            hashlib.sha256(dynamic_resolver_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("dynamicResolverPrototypeSha256") and
            hashlib.sha256(dynamic_adapter_registry_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("dynamicRegistrySha256") and
            hashlib.sha256(generic_image_candidate_path.read_bytes()).hexdigest().upper() ==
            generic_image_contract.get("candidateSha256") and
            all(generic_image_persistence_contract.values()))

        ir_isolation_document = copy.deepcopy(dynamic_vector4_ramp_document)
        ir_isolation_compile = dynamic_module.compile_document_from_data(
            ir_isolation_document, dynamic_registry)
        ir_isolation_document["nodes"].clear()
        ir_isolation_document["links"].clear()
        ir_isolation_document["output"] = {"nodeId": "mutated", "socketId": "invalid"}
        ir_isolation_bindings = {
            (item["contextId"], item["component"]): item["hlslExpression"]
            for item in ir_isolation_compile["contextBindings"]
        }
        ir_isolation_hlsl, _ = dynamic_module.emit_hlsl_from_typed_ir(
            ir_isolation_compile["typedIR"],
            ir_isolation_compile["parameterBlock"],
            context_bindings=ir_isolation_bindings)
        ir_isolation_document_mutation_passed = (
            ir_isolation_hlsl == ir_isolation_compile["hlslBody"] and
            not ir_isolation_document["nodes"] and not ir_isolation_document["links"])
        glsl_ramp_bindings = {
            (item["contextId"], item["component"]): "artifactHostUV." + item["component"]
            for item in regenerated_vector4_ramp_candidate["contextBindings"]
        }
        glsl_ramp_source, glsl_ramp_interface = dynamic_module.emit_glsl_from_typed_ir(
            regenerated_vector4_ramp_candidate["typedIR"],
            regenerated_vector4_ramp_candidate["parameterBlock"],
            context_bindings=glsl_ramp_bindings)

        legacy_projection_contract = dynamic_contract.get("legacyCoreProjection", {})
        legacy_projection_source_path = root / legacy_projection_contract.get("sourceFixture", "")
        legacy_projection_candidate_path = root / legacy_projection_contract.get("candidateFixture", "")
        legacy_projection_source_json = legacy_projection_source_path.read_text(encoding="utf-8")
        legacy_projection = dynamic_module.project_legacy_core_material_input(
            legacy_projection_source_json, dynamic_registry,
            target_node_id=legacy_projection_contract.get("targetNodeId", ""),
            target_input_socket=legacy_projection_contract.get("targetInputSocket", ""))
        legacy_projection_compile = dynamic_module.compile_document_from_data(
            legacy_projection["document"], dynamic_registry)
        legacy_projection_candidate = json.loads(
            legacy_projection_candidate_path.read_text(encoding="utf-8"))
        legacy_base_color_variants = legacy_projection_candidate.get(
            "baseColorMathVariants", {})
        legacy_base_color_variant_compiles = {
            name: dynamic_module.compile_document_from_data(
                item.get("document", {}), dynamic_registry)
            for name, item in legacy_base_color_variants.items()
        }
        legacy_base_color_expected = {
            "add": [0.75, -1.0, -1.0, 0.75],
            "subtract": [-0.25, -3.0, 7.0, 0.25],
            "multiply": [0.125, -2.0, -12.0, 0.125],
            "dividePositiveFloor": [0.5, -2.0, 300000.0, 2.0],
        }
        legacy_projection_compiler_contract = legacy_projection_contract.get("compilerProbe", {})
        legacy_projection_compiler_path = root / legacy_projection_compiler_contract.get("tool", "")
        legacy_projection_compiler_source = legacy_projection_compiler_path.read_text(encoding="utf-8")
        legacy_projection_compiler_evidence_path = root / legacy_projection_compiler_contract.get("evidencePath", "")
        legacy_projection_compiler_evidence = json.loads(
            legacy_projection_compiler_evidence_path.read_text(encoding="utf-8"))
        legacy_uv_math_contract = legacy_projection_compiler_contract.get("dynamicInputMath", {})
        legacy_uv_math_fixture_path = root / legacy_uv_math_contract.get("fixture", "")
        legacy_uv_math_probe_path = root / legacy_uv_math_contract.get("tool", "")
        legacy_uv_math_probe_source = legacy_uv_math_probe_path.read_text(encoding="utf-8")
        legacy_uv_math_fixture = json.loads(legacy_uv_math_fixture_path.read_text(encoding="utf-8"))
        legacy_uv_math_compiled = dynamic_module.compile_document_from_data(
            legacy_uv_math_fixture, dynamic_registry,
            hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
        legacy_roughness_math_fixture_path = root / legacy_projection_compiler_contract.get(
            "dynamicInput", {}).get("fixture", "")
        legacy_roughness_math_source_json = legacy_roughness_math_fixture_path.read_text(
            encoding="utf-8")
        legacy_roughness_math_projection = dynamic_module.project_legacy_core_material_input(
            legacy_roughness_math_source_json, dynamic_registry,
            target_node_id=legacy_projection_contract.get("targetNodeId", ""),
            target_input_socket="Roughness")
        legacy_roughness_math_compiled = dynamic_module.compile_document_from_data(
            legacy_roughness_math_projection["document"], dynamic_registry,
            hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
        legacy_roughness_operation_variants = legacy_projection_candidate.get(
            "dynamicRoughnessUvProjection", {}).get("mathOperationVariants", {})
        legacy_roughness_operation_compiles = {
            name: dynamic_module.compile_document_from_data(
                item.get("document", {}), dynamic_registry,
                hlsl_context_bindings={("artifact.material.surface.uv0", "x"): "In.UV.x"})
            for name, item in legacy_roughness_operation_variants.items()
        }
        legacy_roughness_reordered_document = json.loads(legacy_roughness_math_source_json)
        legacy_roughness_reordered_document["nodes"].reverse()
        legacy_roughness_reordered_document["links"].reverse()
        legacy_roughness_reordered_source_json = json.dumps(
            legacy_roughness_reordered_document, ensure_ascii=False, separators=(",", ":"))
        legacy_roughness_reordered_projection = dynamic_module.project_legacy_core_material_input(
            legacy_roughness_reordered_source_json, dynamic_registry,
            target_node_id=legacy_projection_contract.get("targetNodeId", ""),
            target_input_socket="Roughness")
        legacy_roughness_reordered_compiled = dynamic_module.compile_document_from_data(
            legacy_roughness_reordered_projection["document"], dynamic_registry,
            hlsl_context_bindings={( "artifact.material.surface.uv0", "x"): "In.UV.x"})
        legacy_projection_generator_source = (
            root / legacy_projection_contract.get("generator", "")).read_text(encoding="utf-8")
        legacy_core_shader_node_source = (
            root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        legacy_mesh_renderer_source = (
            root / "ArtifactCore/src/Graphics/MeshRenderer.cpp").read_text(encoding="utf-8")
        dynamic_module_source = dynamic_prototype_path.read_text(encoding="utf-8")
        scalar_legacy_source = {
            "version": 1,
            "kind": "artifact-material-graph",
            "nodes": [
                {"id": "scalar", "type": "Value", "params": {"value": 0.375}},
                {"id": "surface", "type": "Principled", "params": {}},
                {"id": "output", "type": "MaterialOutput", "params": {}},
            ],
            "links": [
                {"from": "scalar", "out": "Value", "to": "surface", "in": "BaseColor"},
                {"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"},
            ],
        }
        scalar_legacy_source_json = json.dumps(
            scalar_legacy_source, ensure_ascii=False, separators=(",", ":"))
        scalar_legacy_projection_from_source = dynamic_module.project_legacy_core_material_input(
            scalar_legacy_source_json, dynamic_registry,
            target_node_id="surface", target_input_socket="BaseColor")
        scalar_legacy_compiled_from_source = dynamic_module.compile_document_from_data(
            scalar_legacy_projection_from_source["document"], dynamic_registry)
        default_legacy_source = {
            "version": 1,
            "kind": "artifact-material-graph",
            "nodes": [
                {"id": "surface", "type": "Principled", "params": {}},
                {"id": "output", "type": "MaterialOutput", "params": {}},
            ],
            "links": [{"from": "surface", "out": "BSDF", "to": "output", "in": "Surface"}],
        }
        default_legacy_source_json = json.dumps(
            default_legacy_source, ensure_ascii=False, separators=(",", ":"))
        default_legacy_projection_from_source = dynamic_module.project_legacy_core_material_input(
            default_legacy_source_json, dynamic_registry,
            target_node_id="surface", target_input_socket="BaseColor")
        default_legacy_compiled_from_source = dynamic_module.compile_document_from_data(
            default_legacy_projection_from_source["document"], dynamic_registry)

        base_color_capture_contract = dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("baseColorMathCapture", {})
        base_color_capture_source = (root / dynamic_ramp_image_plan.get(
            "gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8")
        base_color_capture_analyzer_probe = probe_base_color_capture_analyzer(root)
        checks = {
            "layoutExcludedFromSemanticIdentity": layout_result["semanticHashSha256"] == regenerated_candidate["semanticHashSha256"] and layout_result["hlslBody"] == regenerated_candidate["hlslBody"],
            "implementationSelectionChangesSemanticIdentity": alternate_implementation_compile["semanticHashSha256"] != connected_graph_compile["semanticHashSha256"] and alternate_implementation_compile["hlslBody"] != connected_graph_compile["hlslBody"],
            "layoutEditIsValidatedAndSemanticallyNeutral": layout_only_edit["nodes"][0]["layout"]["x"] == 25 and dynamic_module.compile_document_from_data(layout_only_edit, dynamic_registry)["semanticHashSha256"] == connected_graph_compile["semanticHashSha256"] and invalid_layout_coordinate_diagnostic == {"code": "node.layout.coordinate.invalid", "severity": "error", "path": "/nodes/0/layout/x"} and invalid_layout_field_diagnostic == {"code": "node.layout.invalid", "severity": "error", "path": "/layout"},
            "layoutBatchIsAtomicAndSemanticallyNeutral": layout_batch_edit["nodes"][0]["layout"] == {"x": 25.0, "y": 0.0} and layout_batch_edit["nodes"][1]["layout"] == {"x": 0.0, "y": 200.0} and dynamic_module.compile_document_from_data(layout_batch_edit, dynamic_registry)["semanticHashSha256"] == connected_graph_compile["semanticHashSha256"] and dynamic_module.compile_document_from_data(layout_batch_edit, dynamic_registry)["hlslBody"] == connected_graph_compile["hlslBody"] and invalid_layout_batch_diagnostic == {"code": "node.layout.coordinate.invalid", "severity": "error", "path": "/edits/1/1/y"} and connected_graph["nodes"][0]["layout"] == {"x": 0.0, "y": 0.0} and connected_graph["nodes"][1]["layout"] == {"x": 0.0, "y": 100.0},
            "layoutBatchCanonicalizesAndUndoRedoRestores": layout_boundary_edit["nodes"][0]["layout"] == {"x": 1.0, "y": 0.0} and layout_boundary_edit["nodes"][1]["layout"] == {"x": 1.401298464324817e-45, "y": 100.0} and layout_boundary_undo == connected_graph and layout_boundary_redo == layout_boundary_edit and dynamic_module.compile_document_from_data(layout_boundary_edit, dynamic_registry)["semanticHashSha256"] == connected_graph_compile["semanticHashSha256"] and duplicate_layout_batch_diagnostic == {"code": "node.layout.batch.duplicate", "severity": "error", "path": "/edits/1"},
            "canonicalDocumentSerializationIsStable": canonical_round_trip == connected_graph and dynamic_module.serialize_document(canonical_round_trip) == canonical_document_text and dynamic_module.serialize_document(reordered_object_document) == canonical_document_text and dynamic_module.serialize_document(reordered_array_document) != canonical_document_text,
            "strictJsonRejectsDuplicateKeysAndNonFiniteConstants": duplicate_key_diagnostic == {"code": "json.key.duplicate", "severity": "error", "path": "/x"} and nonstandard_number_diagnostic == {"code": "json.parse.invalid", "severity": "error", "path": "/"},
            "commandSnapshotsRejectCyclesAndBoundNesting": cyclic_snapshot_diagnostic == {"code": "json.value.cycle", "severity": "error", "path": "/vendorExtension/self"} and deep_json_diagnostic is not None and deep_json_diagnostic["code"] == "json.value.depth" and deep_json_diagnostic["path"].count("/0") == dynamic_module.MAX_JSON_NESTING and shared_reference_round_trip == {"left": shared_extension, "right": shared_extension},
            "registryRejectsUnknownFieldsAndDuplicateIdentities": registry_diagnostics[:3] == [{"code": "registry.operation.shape.invalid", "severity": "error", "path": "/operations/0"}, {"code": "operation.registration.duplicate", "severity": "error", "path": duplicate_operation_path}, {"code": "socket.id.duplicate", "severity": "error", "path": f"/operations/{duplicate_socket_operation_index}/inputs/1/socketId"}],
            "documentFileRoundTripIsCanonicalAndAtomic": loaded_graph == io_document and stable_saved_graph_bytes == saved_graph_bytes and failed_save_diagnostic == {"code": "json.number.non_finite", "severity": "error", "path": "/invalidNumber"} and preserved_after_failed_save and unknown_extension_compile_diagnostic == {"code": "schema.field.unknown", "severity": "error", "path": "/vendorExtension"},
            "unknownSchemaStaysEditableButCannotCompile": unknown_extension_editing["status"] == "repairable" and unknown_extension_editing["repairable"] and not unknown_extension_editing["compilationAvailable"] and any(item["code"] == "schema.field.unknown" and item["severity"] == "warning" for item in unknown_extension_editing["diagnostics"]),
            "nodeAndLinkOrderDeterministic": reordered_result["semanticHashSha256"] == regenerated_candidate["semanticHashSha256"] and reordered_result["hlslBody"] == regenerated_candidate["hlslBody"],
            "unreachableNodeExcludedFromSemanticOutput": unreachable_result["semanticHashSha256"] == regenerated_candidate["semanticHashSha256"] and unreachable_result["hlslBody"] == regenerated_candidate["hlslBody"] and "node unreachable_constant is unreachable from the selected output" in unreachable_result["warnings"],
            "knownDocumentPreservedExactly": preserved_document == dynamic_document,
            "unknownVersionAndLinksRemainRepairable": unknown_version_editing["status"] == "repairable" and unknown_version_editing["repairable"] and not unknown_version_editing["compilationAvailable"] and any(item["code"] == "link.operation.unavailable" for item in unknown_version_editing["diagnostics"]) and unknown_version_editing["semanticHashSha256"] is None,
            "restoredOperationDefinitionReenablesSavedGraph": missing_registry_validation["status"] == "repairable" and not missing_registry_validation["compilationAvailable"] and restored_registry_validation["status"] == "valid" and restored_registry_validation["compilationAvailable"] and restored_registry_compile["outputType"] == regenerated_candidate["outputType"] and restored_registry_compile["hlslBody"] == regenerated_candidate["hlslBody"] and temporarily_missing_document["nodes"][0]["semanticVersion"] == 2,
            "unknownVersionPayloadPreserved": dynamic_module.preserve_document(unknown_version_document) == unknown_version_document,
            "opaquePayloadSurvivesSaveLoadEditAndResave": opaque_loaded_document == unknown_version_document and opaque_loaded_editing["status"] == "repairable" and opaque_resaved_document["nodes"][-1] == unknown_version_document["nodes"][-1] and opaque_resaved_document["links"][-1] == unknown_link and opaque_resaved_document["nodes"][1]["parameters"]["value"] == 0.75 and opaque_resaved_document["nodes"][0]["layout"]["x"] == 99 and opaque_resaved_editing["status"] == "repairable" and not opaque_resaved_editing["compilationAvailable"] and opaque_resaved_compile_diagnostic == {"code": "node.operation.version.unsupported", "severity": "error", "path": "/nodes/0/semanticVersion"} and opaque_undo_loaded == unknown_version_document and opaque_redo_loaded == opaque_edited_document and opaque_redo_loaded["nodes"][-1] == unknown_version_document["nodes"][-1] and opaque_redo_loaded["links"][-1] == unknown_link,
            "unknownOutputStaysRepairableUntilExplicitReplacement": unknown_output_validation["status"] == "repairable" and any(item["code"] == "output.operation.unavailable" and item["severity"] == "warning" for item in unknown_output_validation["diagnostics"]) and unknown_output_delete_diagnostic == {"code": "output.replacement.required", "severity": "error", "path": "/output"} and repaired_unknown_output["output"] == {"nodeId": "sum", "socketId": "result"} and all(node["nodeId"] != "future_node" for node in repaired_unknown_output["nodes"]) and all(not any(endpoint.get("nodeId") == "future_node" for endpoint in (link.get("from", {}), link.get("to", {}))) for link in repaired_unknown_output["links"]) and not {"/nodes/5/futureField", "/nodes/5/semanticVersion", "/links/4", "/links/4/vendorLinkData"} & {item["path"] for item in repaired_unknown_output_validation["diagnostics"]},
            "unknownVersionDiagnosedWithoutMutation": len(unknown_version_diagnostics) == 2 and all(item.get("code") == "node.operation.version.unsupported" for item in unknown_version_diagnostics),
            "knownNodeEditableWithOpaqueNodeAndLinkPresent": edited_with_unknown_node["nodes"][1]["parameters"]["value"] == 0.75 and edited_with_unknown_node["nodes"][0]["layout"]["x"] == 99 and edited_with_unknown_node["nodes"][-1] == unknown_version_document["nodes"][-1] and edited_with_unknown_node["links"][-1] == unknown_link,
            "undoRedoPreservesOpaqueGraphPayload": opaque_undo_document == unknown_version_document and opaque_redo_document == edited_with_unknown_node and opaque_redo_document["nodes"][-1] == unknown_version_document["nodes"][-1] and opaque_redo_document["links"][-1] == unknown_link and dynamic_module.validate_document_for_editing(opaque_undo_document, dynamic_registry)["status"] == "repairable" and dynamic_module.validate_document_for_editing(opaque_redo_document, dynamic_registry)["status"] == "repairable",
            "registryPresentationChangeIsCompatible": presentation_compatibility == [],
            "paletteHasStableDistinctIDsForDuplicateLabels": len(duplicate_label_entries) == 2 and len({item["entryId"] for item in duplicate_label_entries}) == 2 and all(item["inputs"][0]["socketId"] == "a" and item["outputs"][0]["socketId"] == "result" for item in duplicate_label_entries) and palette == dynamic_module.build_palette_model(dynamic_registry),
            "paletteNodeCanBeAddedAndCompletedThroughTypedLinks": len(palette_added_graph["nodes"]) == len(connected_graph["nodes"]) + 1 and palette_added_graph["nodes"][-1]["nodeId"] == "palette_added_add" and palette_added_graph["nodes"][-1]["parameters"] == {} and palette_added_graph["nodes"][-1]["layout"] == {"x": 320.0, "y": 80.0} and palette_added_validation["status"] == "incomplete" and any(item["code"] == "node.input.required_unconnected" and item["severity"] == "warning" for item in palette_added_validation["diagnostics"]) and palette_complete_validation["status"] == "incomplete" and palette_complete_validation["compilationAvailable"] and not any(item["code"] == "node.input.required_unconnected" for item in palette_complete_validation["diagnostics"]) and duplicate_palette_node_diagnostic == {"code": "node.id.duplicate", "severity": "error", "path": "/nodes"} and connected_graph["nodes"][-1]["nodeId"] == "sum_node",
            "batchLinkEditingIsAtomicAndTyped": len(palette_wired_graph["links"]) == len(palette_added_graph["links"]) + 2 and palette_complete_validation["compilationAvailable"] and failed_link_batch_diagnostic == {"code": "link.input.multiple_writers", "severity": "error", "path": "/links"} and failed_link_batch_document["links"] == connected_graph["links"] and malformed_link_batch_diagnostic == {"code": "link.batch.entry.invalid", "severity": "error", "path": "/connections/1"},
            "compositeAddConnectOutputEditIsOneUndoableCommand": composite_graph_edit["output"] == {"nodeId": "composite_add", "socketId": "result"} and len(composite_graph_edit["nodes"]) == len(connected_graph["nodes"]) + 1 and len(composite_graph_edit["links"]) == len(connected_graph["links"]) + 2 and composite_graph_validation["compilationAvailable"] and composite_graph_undo == connected_graph and composite_graph_redo == composite_graph_edit and composite_group_undo == connected_graph and composite_group_redo == composite_parameter_edit and composite_after_compile["semanticHashSha256"] != composite_before_compile["semanticHashSha256"] and failed_composite_diagnostic == {"code": "link.input.multiple_writers", "severity": "error", "path": "/links"} and len(connected_graph["nodes"]) == 3 and len(connected_graph["links"]) == 2 and connected_graph["output"] == {"nodeId": "sum_node", "socketId": "result"},
            "transactionAuthorsGraphFromEmptyDocumentAsOneUndoableCommand": len(authoring_graph["nodes"]) == 3 and len(authoring_graph["links"]) == 2 and authoring_validation["status"] == "valid" and authoring_validation["compilationAvailable"] and authoring_compile["hlslBody"].count(" = ") == 3 and authoring_compile["outputType"] == "Float" and authoring_undo == empty_graph and authoring_redo == authoring_graph,
            "localUiSessionActionsStayAtomicAndUndoable": ui_session_document["output"] == {"nodeId": "ui_sum", "socketId": "result"} and len(ui_session_document["nodes"]) == 3 and len(ui_session_document["links"]) == 2 and ui_session.state()["validation"]["status"] == "valid" and ui_session.state()["compilation"]["status"] == "candidate-lowered-no-shader-runtime" and ui_session_rejected_connect == {"code": "link.socket.missing_or_direction_invalid", "severity": "error", "path": "/links/to/socketId"} and ui_session_after_reject == ui_session_before_reject and ui_session_history_after_reject == ui_session_history_before_reject and ui_session_undo_document["output"] == {"nodeId": "", "socketId": ""} and len(ui_session_undo_document["links"]) == 2 and ui_session_redo_document == ui_session_document and ui_session_saved == ui_session_document and ui_session_undo is None and ui_session_redo is None,
            "localImageResolverRequiresAssetRevisionAndSupportedUvPath": local_image_resolved_state["validation"]["status"] == "valid" and local_image_resolved_compilation["resourceBindingStatus"] == "resolved" and local_image_expected_binding.get("assetId") == local_image_resolver_contract.get("assetId") and local_image_expected_binding.get("sourceRevision") == local_image_resolver_contract.get("sourceRevision") and local_image_expected_binding.get("uvSet") == local_image_resolver_contract.get("uvSet") and local_image_expected_binding.get("uvTransform") == {"scaleU": 1.0, "scaleV": 1.0, "rotationDegrees": 0.0, "offsetU": 0.0, "offsetV": 0.0} and local_image_catalog_only_state["compilation"]["resourceBindingStatus"] == "unresolved" and local_image_no_snapshot_state["compilation"]["resourceBindingStatus"] == "unresolved" and local_image_changed_compilation["resourceBindingStatus"] == "resolved" and local_image_changed_compilation["resourceBindingKeySha256"] != local_image_resolved_compilation["resourceBindingKeySha256"] and local_image_changed_compilation["semanticHashSha256"] == local_image_resolved_compilation["semanticHashSha256"] and local_image_changed_compilation["typedIRSemanticHashSha256"] == local_image_resolved_compilation["typedIRSemanticHashSha256"] and local_image_changed_compilation["shaderStructureHashSha256"] == local_image_resolved_compilation["shaderStructureHashSha256"] and local_image_changed_compilation["hlslBodySha256"] == local_image_resolved_compilation["hlslBodySha256"] and local_image_catalog_hash == hashlib.sha256(local_image_catalog_path.read_bytes()).hexdigest().upper() == local_image_resolver_contract.get("assetCatalogSha256") and local_image_source_states_hash == hashlib.sha256(local_image_source_states_path.read_bytes()).hexdigest().upper() == local_image_resolver_contract.get("assetSourceStatesSha256") and hashlib.sha256(local_image_document_path.read_bytes()).hexdigest().upper() == local_image_resolver_contract.get("documentSha256"),
            "localImageLongMappingChainResolvesIteratively": len(long_mapping_nodes) == 1500 and len(long_mapping_document["nodes"]) < dynamic_module.MAX_GRAPH_NODES and long_mapping_binding.get("assetId") == local_image_resolver_contract.get("assetId") and long_mapping_binding.get("sourceRevision") == local_image_resolver_contract.get("sourceRevision") and long_mapping_binding.get("uvSet") == local_image_resolver_contract.get("uvSet"),
            "localImageMappingChangesShaderWithoutChangingResolvedBinding": local_image_mapping_edit_state["validation"]["status"] == "valid" and local_image_mapping_edit_compilation["resourceBindingStatus"] == "resolved" and local_image_mapping_edit_compilation["resourceBindingKeySha256"] == local_image_resolved_compilation["resourceBindingKeySha256"] and local_image_mapping_edit_compilation["semanticHashSha256"] != local_image_resolved_compilation["semanticHashSha256"] and local_image_mapping_edit_compilation["typedIRSemanticHashSha256"] != local_image_resolved_compilation["typedIRSemanticHashSha256"] and local_image_mapping_edit_compilation["shaderStructureHashSha256"] != local_image_resolved_compilation["shaderStructureHashSha256"] and local_image_mapping_edit_compilation["hlslBodySha256"] != local_image_resolved_compilation["hlslBodySha256"],
            "assetSnapshotReadersEnforceByteAndJsonDepthBounds": oversized_catalog_diagnostic == "asset.catalog.size.invalid" and oversized_source_states_diagnostic == "asset.source_states.size.invalid" and nested_catalog_diagnostic == "asset.catalog.json.invalid" and nested_source_states_diagnostic == "asset.source_states.json.invalid",
            "operationReplacementPreservesCompatibleLinksAndHistory": replaced_graph["nodes"][2]["operationId"] == "artifact.rd.math.multiply.float32" and replaced_graph["links"] == connected_graph["links"] and replaced_graph_validation["status"] == "valid" and " * " in replaced_graph_compile["hlslBody"] and replaced_graph_compile["semanticHashSha256"] != connected_graph_compile["semanticHashSha256"] and replacement_undo == connected_graph and replacement_redo == replaced_graph and incompatible_replacement_diagnostic == {"code": "node.operation.incompatible", "severity": "error", "path": "/nodes/2/operationId"} and changed_unmaterialized_default_diagnostic == {"code": "node.operation.incompatible", "severity": "error", "path": "/nodes/0/parameters/value"} and sealed_replacement_compile["outputType"] == "Float" and "artifact.rd.math.add.float32@1" not in sealed_replacement["registryContractLock"] and "artifact.rd.math.multiply.float32@1" in sealed_replacement["registryContractLock"] and replacement_session_after["nodes"][2]["operationId"] == "artifact.rd.math.multiply.float32" and replacement_session_undo == connected_graph and replacement_session_redo == replacement_session_after,
            "vector3OperationReplacementPreservesTypedLinksAndUndo": next(node for node in vector3_replaced_graph["nodes"] if node["nodeId"] == "difference")["operationId"] == "artifact.rd.math.multiply.vector3.float32" and vector3_replaced_graph["links"] == dynamic_vector3_arithmetic_document["links"] and [item["implementationId"] for item in vector3_replaced_compile["typedIR"]["operations"]] == ["vector3.constant", "vector3.constant", "vector3.add", "vector3.multiply", "vector3.multiply"] and "float3 n_difference_result = (n_sum_result * n_right_value);" in vector3_replaced_compile["hlslBody"] and vector3_replacement_undo == dynamic_vector3_arithmetic_document and vector3_replacement_redo == vector3_replaced_graph and next(node for node in vector3_replacement_session_after["nodes"] if node["nodeId"] == "difference")["operationId"] == "artifact.rd.math.multiply.vector3.float32" and vector3_replacement_session_undo == dynamic_vector3_arithmetic_document and vector3_replacement_session_redo == vector3_replacement_session_after and vector3_replacement_session.history,
            "localUiHttpRoutesHandleStateEditAndReject": ui_http_page_status == 200 and b"Shader Graph Lab" in ui_http_page_body and ui_http_state_status == 200 and ui_http_state["validation"]["status"] == "valid" and bool(ui_http_state["compilation"]["hlslBody"]) and ui_http_reject_status == 400 and not ui_http_rejected["ok"] and ui_http_rejected["diagnostics"][0]["code"] == "link.socket.missing_or_direction_invalid" and ui_http_edit_status == 200 and ui_http_edited["document"]["nodes"][0]["layout"]["x"] == 88.0 and ui_http_undo_status == 200 and ui_http_undone["document"]["nodes"][0]["layout"]["x"] == 0.0,
            "vector3LocalUiEditsThreeComponentsAndExposesParameterValues": ".wire.Vector3 { stroke: #64a6d8; }" in local_ui_html_source and ".socket.Vector3 { background: #4d83ad; border-color: #8dc2e8; }" in local_ui_html_source and "Node parameter · 3 components" not in local_ui_html_source and "socketDefault.length===3?'vector-3':''" in local_ui_html_source and "param.valueType==='Vector3F32'?'vector-3':''" in local_ui_html_source and "definition.valueType==='Vector3F32'||definition.valueType==='Vector4F32'" in local_ui_html_source and "definition.valueType==='Vector3F32'?3:4" in local_ui_html_source and "definition.valueType==='Float32'?typeof item.value==='number'" in local_ui_html_source and "param.valueType==='Float32'||param.valueType==='Vector3F32'||param.valueType==='Vector4F32'" in local_ui_html_source and "parameter.valueType==='Vector3F32'?3:4" in local_ui_html_source,
            "instantiatedNodeUsesDeclaredParameterDefault": created_constant_node["parameters"] == {"value": 0.0} and "= 0.0f;" in created_constant_compile["hlslBody"],
            "editingParameterChangesSemanticOutput": changed_constant_compile["semanticHashSha256"] != created_constant_compile["semanticHashSha256"] and changed_constant_compile["hlslBody"] != created_constant_compile["hlslBody"],
            "vector3GraphParameterSessionSharesEditsAndDetachesUndoably": vector3_graph_parameter_session_shared["schemaVersion"] == 2 and vector3_graph_parameter_session_shared["graphParameters"][0]["valueType"] == "Vector3F32" and vector3_graph_parameter_session_shared["graphParameters"][0]["bindings"] == [{"nodeId": "left", "parameterId": "value"}, {"nodeId": "right", "parameterId": "value"}] and vector3_graph_parameter_session_edited["graphParameters"][0]["value"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896] and vector3_graph_parameter_session_undo == vector3_graph_parameter_session_shared and vector3_graph_parameter_session_redo == vector3_graph_parameter_session_edited and vector3_graph_parameter_session_detached["graphParameters"][0]["bindings"] == [{"nodeId": "left", "parameterId": "value"}] and next(node for node in vector3_graph_parameter_session_detached["nodes"] if node["nodeId"] == "right")["parameters"]["value"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896] and vector3_graph_parameter_session_unexposed["graphParameters"] == [] and next(node for node in vector3_graph_parameter_session_unexposed["nodes"] if node["nodeId"] == "left")["parameters"]["value"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896] and vector3_graph_parameter_session_unexpose_undo == vector3_graph_parameter_session_detached and vector3_graph_parameter_session_unexpose_redo == vector3_graph_parameter_session_unexposed,
            "graphParametersExposeShareAndEditTypedNodeValues": dynamic_contract.get("graphParameterLayoutContract") == {"valueTypes": ["Float32", "Vector3F32", "Vector4F32"], "vector3Cbuffer": "float3 at an explicit 16-byte register start; reserve all 16 bytes in host layout", "vector3ResidentAbi": "three consecutive scalar float lanes lowered as float3; capacity remains sixteen lanes", "status": "source-candidate-no-compiler-reflection-or-runtime-upload"} and shared_graph_parameter["schemaVersion"] == 2 and shared_graph_parameter["graphParameters"] == [{"parameterId": "shared_gain", "label": "Shared Gain", "valueType": "Float32", "value": 0.25, "bindings": [{"nodeId": "source_left", "parameterId": "value"}, {"nodeId": "source_right", "parameterId": "value"}]}] and graph_parameter_baseline_compile["semanticHashSha256"] != shared_graph_parameter_compile["semanticHashSha256"] and graph_parameter_uniforms_stable and edited_graph_parameter_compile["semanticHashSha256"] != shared_graph_parameter_compile["semanticHashSha256"] and bound_node_parameter_edit_diagnostic == {"code": "node.parameter.exposed", "severity": "error", "path": "/graphParameters/0/bindings/value"} and duplicate_graph_parameter_binding_diagnostic == {"code": "graph.parameter.binding.duplicate", "severity": "error", "path": "/graphParameters/0/bindings"} and invalid_graph_parameter_value_diagnostic == {"code": "graph.parameter.value.invalid", "severity": "error", "path": "/graphParameters/0/value"} and incompatible_graph_parameter_type_diagnostic == {"code": "graph.parameter.type.incompatible", "severity": "error", "path": "/graphParameters/shared_gain/valueType"} and duplicate_graph_parameter_compile_diagnostic == {"code": "graph.parameter.binding.duplicate", "severity": "error", "path": "/graphParameters/0/bindings/2"} and graph_parameter_undo == shared_graph_parameter and graph_parameter_redo == edited_graph_parameter and not deleted_graph_parameter_node["graphParameters"] and round_trip_graph_parameter == shared_graph_parameter and graph_parameter_session_document["graphParameters"][0]["value"] == 0.625 and graph_parameter_exposure_session_document["schemaVersion"] == 2 and graph_parameter_exposure_session_undo == graph_parameter_source and graph_parameter_exposure_session_redo == graph_parameter_exposure_session_document and dynamic_module.validate_document_for_editing(shared_graph_parameter, dynamic_registry)["status"] == "valid" and vector4_edited_graph_parameter["graphParameters"][0]["value"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896, 0.4000000059604645] and vector4_graph_parameter_compile["outputType"] == "Vector4" and vector4_graph_parameter_compile["semanticHashSha256"] != vector4_graph_parameter_base_compile["semanticHashSha256"] and vector4_graph_parameter_compile["shaderStructureHashSha256"] == vector4_graph_parameter_base_compile["shaderStructureHashSha256"] and "float4 P_surface_tint : packoffset(c0.x);" in vector4_graph_parameter_compile["hlslBody"] and "n_color_source_value = P_surface_tint;" in vector4_graph_parameter_compile["hlslBody"] and "0.400000006f" not in vector4_graph_parameter_compile["hlslBody"] and vector3_edited_graph_parameter["graphParameters"][0]["value"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896] and vector3_graph_parameter_compile["outputType"] == "Vector3" and vector3_graph_parameter_compile["semanticHashSha256"] != vector3_graph_parameter_base_compile["semanticHashSha256"] and vector3_graph_parameter_compile["shaderStructureHashSha256"] == vector3_graph_parameter_base_compile["shaderStructureHashSha256"] and "float3 P_surface_normal : packoffset(c0.x);" in vector3_graph_parameter_compile["hlslBody"] and "n_vector_source_value = P_surface_normal;" in vector3_graph_parameter_compile["hlslBody"] and vector3_graph_parameter_compile["parameterBlock"]["byteSize"] == 16 and vector3_graph_parameter_compile["parameterBindings"][0]["byteSize"] == 16 and vector3_resident_parameter_candidate["parameterBindings"][0]["slotCount"] == 3 and vector3_resident_parameter_candidate["parameterBindings"][0]["hlslExpression"] == "float3(g_P0, g_P1, g_P2)" and vector3_resident_parameter_values["lanes"][:4] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896, 0.0] and dynamic_vector3_graph_parameter_candidate == dynamic_module.compile_document_from_data(dynamic_vector3_graph_parameter_document, dynamic_registry) and dynamic_vector3_graph_parameter_document["graphParameters"] == [{"parameterId": "surface_normal", "label": "Surface Normal", "valueType": "Vector3F32", "value": [0.25, -0.5, 0.75], "bindings": [{"nodeId": "vector_source", "parameterId": "value"}]}] and dynamic_vector3_graph_parameter_candidate["outputType"] == "Vector3" and dynamic_vector3_graph_parameter_candidate["parameterBlock"]["byteSize"] == 16 and dynamic_vector3_graph_parameter_candidate["parameterBindings"][0]["byteSize"] == 16 and "float3 P_surface_normal : packoffset(c0.x);" in dynamic_vector3_graph_parameter_candidate["hlslBody"] and "n_vector_source_value = P_surface_normal;" in dynamic_vector3_graph_parameter_candidate["hlslBody"] and hashlib.sha256(dynamic_vector3_graph_parameter_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3GraphParameterDocumentSha256") and hashlib.sha256(dynamic_vector3_graph_parameter_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3GraphParameterCandidateSha256") and vector3_resident_parameter_candidate["parameterBindings"][0]["slotCount"] == 3 and vector3_resident_parameter_candidate["parameterBindings"][0]["hlslExpression"] == "float3(g_P0, g_P1, g_P2)" and vector3_resident_parameter_values["lanes"][:4] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896, 0.0] and unreachable_parameter_hash_unchanged,
            "parameterEditsRejectAtomically": atomic_parameter_diagnostic == {"code": "node.parameter.unknown", "severity": "error", "path": "/nodes/0/parameters/unknown"} and invalid_parameter_diagnostic == {"code": "parameter.float32.invalid", "severity": "error", "path": "/nodes/0/parameters/value"} and created_constant_document["nodes"][0]["parameters"] == {"value": 0.0},
            "vector3ConstantUsesTypedFloat3Path": dynamic_vector3_constant_candidate == vector3_constant_compile and dynamic_vector3_constant_document["nodes"][0]["parameters"]["value"] == [0.25, -0.5, 0.75] and vector3_constant_compile["outputType"] == "Vector3" and "float3 n_vector_source_value = float3(0.25f, -0.5f, 0.75f);" in vector3_constant_compile["hlslBody"] and "float3 n_vector_sum_result = (n_vector_source_value + float3(0.0f, 0.0f, 0.0f));" in vector3_constant_compile["hlslBody"] and vector3_constant_compile["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.0, 0.0, 0.0] and invalid_vector3_parameter_diagnostic == {"code": "parameter.vector3.invalid", "severity": "error", "path": "/nodes/0/parameters/value"} and vector3_graph_parameter_exposure_diagnostic is None and hashlib.sha256(dynamic_vector3_constant_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3ConstantDocumentSha256") and hashlib.sha256(dynamic_vector3_constant_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3ConstantCandidateSha256"),
            "vector3ArithmeticUsesTypedOperationsAndRejectsMismatchedLinks": dynamic_vector3_arithmetic_candidate == vector3_arithmetic_compile and vector3_arithmetic_compile["outputType"] == "Vector3" and "float3 n_sum_result = (n_left_value + n_right_value);" in vector3_arithmetic_compile["hlslBody"] and "float3 n_difference_result = (n_sum_result - n_right_value);" in vector3_arithmetic_compile["hlslBody"] and "float3 n_product_result = (n_difference_result * n_right_value);" in vector3_arithmetic_compile["hlslBody"] and [item["implementationId"] for item in vector3_arithmetic_compile["typedIR"]["operations"]] == ["vector3.constant", "vector3.constant", "vector3.add", "vector3.subtract", "vector3.multiply"] and [((a + b) - b) * b for a, b in zip(dynamic_vector3_arithmetic_document["nodes"][0]["parameters"]["value"], dynamic_vector3_arithmetic_document["nodes"][1]["parameters"]["value"])] == [1.0, 1.0, 3.0] and vector3_type_mismatch_diagnostic == {"code": "link.type.incompatible", "severity": "error", "path": "/links/1"} and hashlib.sha256(dynamic_vector3_arithmetic_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3ArithmeticDocumentSha256") and hashlib.sha256(dynamic_vector3_arithmetic_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector3ArithmeticCandidateSha256"),
            "vector4ConstantParametersCanonicalizeAndLower": vector4_constant_node["parameters"]["value"] == [0.0, 0.0, 0.0, 1.0] and vector4_default_compile["outputType"] == "Vector4" and "float4 n_color_source_value = float4(0.0f, 0.0f, 0.0f, 1.0f);" in vector4_default_compile["hlslBody"] and vector4_edited_document["nodes"][0]["parameters"]["value"] == [0.25, 0.5, 0.75, 0.4000000059604645] and vector4_edited_compile["semanticHashSha256"] != vector4_default_compile["semanticHashSha256"] and vector4_extract_compile["outputType"] == "Float" and "float n_color_x_value = (n_color_source_value).x;" in vector4_extract_compile["hlslBody"] and invalid_vector4_parameter_diagnostic == {"code": "parameter.vector4.invalid", "severity": "error", "path": "/nodes/0/parameters/value"} and invalid_vector4_component_diagnostic == {"code": "parameter.float32.invalid", "severity": "error", "path": "/nodes/0/parameters/value/2"} and vector4_boundaries_document["nodes"][0]["parameters"]["value"] == [0.0, 1.0, 1.401298464324817e-45, -1.401298464324817e-45] and vector4_boundaries_compile["semanticHashSha256"] == vector4_repeated_edit_compile["semanticHashSha256"] and vector4_constant_undo_document == vector4_constant_document and vector4_constant_redo_document == vector4_boundaries_document and vector4_constant_document["nodes"][0]["parameters"]["value"] == [0.0, 0.0, 0.0, 1.0],
            "vector4ArithmeticLowersComponentwiseWithTypedLinks": vector4_arithmetic_compile["outputType"] == "Vector4" and "float4 n_v4_sum_result = (n_v4_left_value + n_v4_right_value);" in vector4_arithmetic_compile["hlslBody"] and "float4 n_v4_product_result = (n_v4_sum_result * n_v4_right_value);" in vector4_arithmetic_compile["hlslBody"] and vector4_arithmetic_edited_compile["semanticHashSha256"] != vector4_arithmetic_compile["semanticHashSha256"] and vector4_arithmetic_type_diagnostic == {"code": "link.type.incompatible", "severity": "error", "path": "/links/0"} and vector4_cpu_candidate == [0.3499999940395355, 0.16249999403953552, 3.1499998569488525, 1.1624999046325684] and vector4_cpu_candidate[3] > 1.0,
            "explicitScalarVectorConversionsAreTypedAndDeterministic": vector_validation["status"] == "valid" and vector_compile["outputType"] == "Vector4" and "float4 n_replicate_vector = float4(n_scalar_source_value, n_scalar_source_value, n_scalar_source_value, 1.0f);" in vector_compile["hlslBody"] and chained_conversion_compile["outputType"] == "Float" and "float n_extract_x_value = (n_replicate_vector).x;" in chained_conversion_compile["hlslBody"] and explicit_conversion_type_mismatch_diagnostic == {"code": "link.type.incompatible", "severity": "error", "path": "/links/0"},
            "typedLinkEditingBuildsCompilableGraph": len(connected_graph["links"]) == 2 and " + " in connected_graph_compile["hlslBody"],
            "editingValidatorAcceptsCompilableGraph": connected_validation["status"] == "valid" and connected_validation["compilationAvailable"] and not connected_validation["diagnostics"],
            "editingValidatorReportsRequiredInputsAsWarnings": incomplete_validation["status"] == "incomplete" and not incomplete_validation["compilationAvailable"] and [item["path"] for item in incomplete_validation["diagnostics"]] == ["/nodes/0/inputs/a", "/nodes/0/inputs/b"] and all(item["severity"] == "warning" for item in incomplete_validation["diagnostics"]),
            "editingValidatorKeepsStructuralFailuresAsErrors": cyclic_validation["status"] == "invalid" and any(item["code"] == "graph.cycle" and item["severity"] == "error" for item in cyclic_validation["diagnostics"]),
            "editingValidatorAggregatesIndependentIssues": multi_invalid_validation["status"] == "invalid" and {"parameter.float32.invalid", "link.socket.missing_or_direction_invalid", "link.input.multiple_writers", "node.input.required_unconnected"} <= {item["code"] for item in multi_invalid_validation["diagnostics"]} and sum(item["severity"] == "error" for item in multi_invalid_validation["diagnostics"]) >= 3,
            "outputSelectionChangesActiveRoot": selected_source_output["output"] == {"nodeId": "source_left", "socketId": "value"} and selected_source_compile["semanticHashSha256"] != connected_graph_compile["semanticHashSha256"],
            "nodeDeleteRemovesIncidentLinksAndUndoRestores": len(deleted_source_graph["nodes"]) == 2 and len(deleted_source_graph["links"]) == 1 and restored_source_graph == connected_graph,
            "selectedOutputDeleteRequiresAndUsesExplicitReplacement": deleted_output_diagnostic == {"code": "output.replacement.required", "severity": "error", "path": "/output"} and len(replacement_delete_graph["nodes"]) == 2 and not replacement_delete_graph["links"] and replacement_delete_compile["outputType"] == "Float",
            "outputSelectionRejectsUnknownSocket": invalid_output_diagnostic == {"code": "output.socket.missing", "severity": "error", "path": "/output/socketId"},
            "disconnectRemovesOnlyAddressedLink": len(disconnected_graph["links"]) == 1 and disconnected_graph["links"][0] == connected_graph["links"][1] and len(connected_graph["links"]) == 2,
            "batchDisconnectPreflightsAllEdgesAtomically": batch_disconnected_graph["links"] == [] and len(connected_graph["links"]) == 2 and missing_batch_disconnect_diagnostic == {"code": "link.not_unique_or_missing", "severity": "error", "path": "/connections/1"} and duplicate_batch_disconnect_diagnostic == {"code": "link.not_unique_or_missing", "severity": "error", "path": "/connections/1"} and len(connected_graph["links"]) == 2,
            "disconnectMissingLinkIsNonDestructive": missing_disconnect_diagnostic == {"code": "link.not_unique_or_missing", "severity": "error", "path": "/links"} and len(connected_graph["links"]) == 2,
            "undoRedoRoundTripsExactGraphPayload": undo_edit_graph == connected_graph and undo_disconnect_graph == command_edited_graph and redo_edit_graph == command_edited_graph and redo_disconnect_graph == disconnect_after_edit,
            "staleUndoIsRejected": stale_history_diagnostic == {"code": "command.history.stale", "severity": "error", "path": "/"},
            "batchTransactionIsAtomicAndUndoable": transaction_result == disconnect_after_edit and transaction_undo == connected_graph and batch_failure_diagnostic == {"code": "node.parameter.unknown", "severity": "error", "path": "/nodes/0/parameters/unknown"} and connected_graph["nodes"][0]["layout"]["x"] == 0,
            "commandGroupingRequiresContiguousHistory": dynamic_module.apply_document_command(grouped_command, disconnect_after_edit, direction="undo") == connected_graph and discontinuous_command_diagnostic == {"code": "command.transaction.discontinuous", "severity": "error", "path": "/commands/1"},
            "linkEditorRejectsInvalidEndpointsAndWriters": unknown_socket_diagnostic == {"code": "link.socket.missing_or_direction_invalid", "severity": "error", "path": "/links/from/socketId"} and multiple_writer_diagnostic == {"code": "link.input.multiple_writers", "severity": "error", "path": "/links"} and duplicate_id_diagnostic == {"code": "node.id.duplicate", "severity": "error", "path": "/nodes/3/nodeId"} and malformed_link_diagnostic == {"code": "link.endpoint.required", "severity": "error", "path": "/links"} and len(connected_graph["links"]) == 2,
            "linkEditorRejectsTypeMismatchAndCycle": wrong_type_diagnostic == {"code": "link.type.incompatible", "severity": "error", "path": "/links"} and cycle_link_diagnostic == {"code": "graph.cycle", "severity": "error", "path": "/links"},
            "sameVersionSemanticContractChangeDetected": len(changed_contract_diagnostics) == 1 and changed_contract_diagnostics[0].get("code") == "registry.operation.contract_changed",
            "operationLockDetectsParameterDefaultAndRequirednessDrift": len(changed_default_diagnostics := dynamic_module.compare_registry_contracts(saved_registry_lock, changed_default_registry)) == 1 and changed_default_diagnostics[0]["code"] == "registry.operation.contract_changed" and len(changed_requiredness_diagnostics := dynamic_module.compare_registry_contracts(saved_registry_lock, changed_requiredness_registry)) == 1 and changed_requiredness_diagnostics[0]["code"] == "registry.operation.contract_changed",
            "removedOperationDetected": len(removed_contract_diagnostics) == 1 and removed_contract_diagnostics[0].get("code") == "registry.operation.removed",
            "newVersionCanBeAddedAlongsidePinnedVersion": additive_version_diagnostics == [] and sealed_additive_version_compile["semanticHashSha256"] == regenerated_candidate["semanticHashSha256"] and "artifact.rd.constant.float32@2" not in sealed_round_trip["registryContractLock"],
            "explicitIdentityMigrationChangesPinWithoutChangingShader": explicit_migration_compile["hlslBody"] == regenerated_candidate["hlslBody"] and all(node["semanticVersion"] == 2 for node in explicit_migration["nodes"] if node["operationId"] == "artifact.rd.constant.float32") and "artifact.rd.constant.float32@2" in explicit_migration["registryContractLock"],
            "missingPinnedVersionDoesNotSilentlyUpgrade": silent_upgrade_diagnostic == {"code": "registry.operation.removed", "severity": "error", "path": "/registryContractLock/artifact.rd.constant.float32@1"},
            "semanticMigrationRequiresExplicitTransform": unsafe_migration_diagnostic == {"code": "registry.migration.semantic_change_requires_transform", "severity": "error", "path": "/nodes/0/semanticVersion"},
            "migrationRejectsDefaultRequirednessAndIdentityChanges": default_migration_diagnostic == {"code": "registry.migration.semantic_change_requires_transform", "severity": "error", "path": "/nodes/0/semanticVersion"} and requiredness_migration_diagnostic == {"code": "registry.migration.semantic_change_requires_transform", "severity": "error", "path": "/nodes/2/semanticVersion"} and cross_identity_migration_diagnostic == {"code": "registry.migration.semantic_change_requires_transform", "severity": "error", "path": "/nodes/0/semanticVersion"},
            "registryContractLockPersistsAndAllowsPresentationChanges": sealed_round_trip["registryContractLock"] == sealed_document["registryContractLock"] and sealed_presentation_compile["semanticHashSha256"] == regenerated_candidate["semanticHashSha256"],
            "sealedGraphRejectsSameVersionContractDrift": sealed_contract_diagnostic == {"code": "registry.operation.contract_changed", "severity": "error", "path": "/registryContractLock/artifact.rd.math.add.float32@1"},
            **{case_id: result is not None for case_id, result in diagnostics.items()},
            "unsupportedSchemaVersionRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("unsupportedSchemaVersionRejected"), dict) and diagnostics.get("unsupportedSchemaVersionRejected") == dynamic_contract["diagnosticGoldens"]["unsupportedSchemaVersionRejected"],
            "unsupportedDocumentKindRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("unsupportedDocumentKindRejected"), dict) and diagnostics.get("unsupportedDocumentKindRejected") == dynamic_contract["diagnosticGoldens"]["unsupportedDocumentKindRejected"],
            "requiredLinkArrayRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("requiredLinkArrayRejected"), dict) and diagnostics.get("requiredLinkArrayRejected") == dynamic_contract["diagnosticGoldens"]["requiredLinkArrayRejected"],
            "unsupportedOperationVersionRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("unsupportedOperationVersionRejected"), dict) and diagnostics.get("unsupportedOperationVersionRejected") == dynamic_contract["diagnosticGoldens"]["unsupportedOperationVersionRejected"],
            "multipleInputWritersRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("multipleInputWritersRejected"), dict) and diagnostics.get("multipleInputWritersRejected") == dynamic_contract["diagnosticGoldens"]["multipleInputWritersRejected"],
            "cyclesRejectedBeforeOutputPruning": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("cyclesRejectedBeforeOutputPruning"), dict) and diagnostics.get("cyclesRejectedBeforeOutputPruning") == dynamic_contract["diagnosticGoldens"]["cyclesRejectedBeforeOutputPruning"],
            "requiredInputEnforced": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("requiredInputEnforced"), dict) and diagnostics.get("requiredInputEnforced") == dynamic_contract["diagnosticGoldens"]["requiredInputEnforced"],
            "sanitizedIdentifierCollisionRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("sanitizedIdentifierCollisionRejected"), dict) and diagnostics.get("sanitizedIdentifierCollisionRejected") == dynamic_contract["diagnosticGoldens"]["sanitizedIdentifierCollisionRejected"],
            "nonFiniteBinary32ParameterRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("nonFiniteBinary32ParameterRejected"), dict) and diagnostics.get("nonFiniteBinary32ParameterRejected") == dynamic_contract["diagnosticGoldens"]["nonFiniteBinary32ParameterRejected"],
            "inlineShaderSourceRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("inlineShaderSourceRejected"), dict) and diagnostics.get("inlineShaderSourceRejected") == dynamic_contract["diagnosticGoldens"]["inlineShaderSourceRejected"],
            "socketTypeMismatchRejected": isinstance(dynamic_contract.get("diagnosticGoldens", {}).get("socketTypeMismatchRejected"), dict) and diagnostics.get("socketTypeMismatchRejected") == dynamic_contract["diagnosticGoldens"]["socketTypeMismatchRejected"],
            "subtractCandidateReproducesDedicatedGraph": dynamic_subtract_candidate == regenerated_subtract_candidate and regenerated_subtract_candidate.get("outputType") == "Float" and "(n_minuend_value - n_subtrahend_value)" in regenerated_subtract_candidate.get("hlslBody", "") and dynamic_subtract_document.get("output") == {"nodeId": "difference", "socketId": "result"},
            "divideCandidateReproducesPositiveFloorContract": dynamic_divide_candidate == regenerated_divide_candidate and regenerated_divide_candidate.get("outputType") == "Float" and "(n_numerator_value / max(n_divisor_value, 1e-5f))" in regenerated_divide_candidate.get("hlslBody", "") and len(divide_document_nodes) == 3 and divide_divisor_node.get("parameters", {}).get("value") == -0.00002,
            "divideCandidateMatchesLegacyNegativeDenominatorGolden": len(divide_golden_a) == 4 and len(divide_golden_b) == 4 and len(divide_golden_result) == 4 and divide_golden_a[1] == divide_numerator_node.get("parameters", {}).get("value") and divide_golden_b[1] == divide_divisor_node.get("parameters", {}).get("value") and divide_golden_result[1] == 100000.0,
            "vector4DivideCandidateMatchesAllLegacyBinary32Lanes": dynamic_vector4_divide_candidate == regenerated_vector4_divide_candidate and regenerated_vector4_divide_candidate.get("outputType") == "Vector4" and "(n_numerator_value / max(n_denominator_value, float4(1e-5f,1e-5f,1e-5f,1e-5f)))" in regenerated_vector4_divide_candidate.get("hlslBody", "") and len(vector4_divide_nodes) == 3 and vector4_divide_nodes[2].get("operationId") == "artifact.rd.math.divide.vector4.positive_floor.float32" and dynamic_vector4_divide_document.get("output") == {"nodeId": "quotient", "socketId": "result"} and vector4_divide_numerator_node.get("parameters", {}).get("value") == divide_golden_a and vector4_divide_denominator_node.get("parameters", {}).get("value") == divide_golden_b and divide_golden_result == [100000.0, 100000.0, 50000.0, 100000.0],
            "vector4SubtractCandidateMatchesBinary32LaneGolden": dynamic_vector4_subtract_candidate == regenerated_vector4_subtract_candidate and regenerated_vector4_subtract_candidate.get("outputType") == "Vector4" and "(n_minuend_value - n_subtrahend_value)" in regenerated_vector4_subtract_candidate.get("hlslBody", "") and len(vector4_subtract_nodes) == 3 and vector4_subtract_nodes[2].get("operationId") == "artifact.rd.math.subtract.vector4.float32" and dynamic_vector4_subtract_document.get("output") == {"nodeId": "difference", "socketId": "result"} and vector4_subtract_minuend.get("parameters", {}).get("value") == [4.0, 3.0, 2.0, 1.0] and vector4_subtract_subtrahend.get("parameters", {}).get("value") == [1.0, 2.0, 3.0, 4.0] and vector4_subtract_result == [3.0, 1.0, -1.0, -3.0] and vector4_subtract_result_bits == ["40400000", "3F800000", "BF800000", "C0400000"],
            "vector4InvertCandidateReproducesSourceFormulaCase": dynamic_vector4_invert_candidate == regenerated_vector4_invert_candidate and regenerated_vector4_invert_candidate.get("outputType") == "Vector4" and "lerp(n_source_color_value, (float4(1.0f, 1.0f, 1.0f, 1.0f) - n_source_color_value), saturate(n_source_factor_value))" in regenerated_vector4_invert_candidate.get("hlslBody", "") and vector4_invert_color_node.get("parameters", {}).get("value") == vector4_invert_case.get("color") and vector4_invert_factor_node.get("parameters", {}).get("value") == vector4_invert_case.get("factor") and vector4_invert_case.get("expectedVector4") == [0.5, 0.5, 0.5, 0.5] and dynamic_vector4_invert_document.get("output") == {"nodeId": "inverted", "socketId": "result"},
            "vector4InvertCandidateLowersAllGoldenInputs": len(vector4_invert_golden_candidates) == len(invert_golden.get("cases", [])) == 7 and all(case_document["nodes"][0]["parameters"]["value"] == case.get("color") and case_document["nodes"][1]["parameters"]["value"] == case.get("factor") and candidate.get("outputType") == "Vector4" and "lerp(n_source_color_value" in candidate.get("hlslBody", "") and "saturate(n_source_factor_value)" in candidate.get("hlslBody", "") for case, case_document, candidate in vector4_invert_golden_candidates),
            "clampCandidateLowersAllSortedBoundsGoldenInputs": dynamic_clamp_candidate == regenerated_clamp_candidate and regenerated_clamp_candidate.get("outputType") == "Float" and "clamp(n_input_value_value, min(n_input_minimum_value, n_input_maximum_value), max(n_input_minimum_value, n_input_maximum_value))" in regenerated_clamp_candidate.get("hlslBody", "") and len(clamp_golden_candidates) == len(clamp_golden.get("cases", [])) == 7 and all(case_document["nodes"][0]["parameters"]["value"] == case.get("value") and case_document["nodes"][1]["parameters"]["value"] == case.get("minimum") and case_document["nodes"][2]["parameters"]["value"] == case.get("maximum") and candidate.get("outputType") == "Float" and "clamp(" in candidate.get("hlslBody", "") and ", min(" in candidate.get("hlslBody", "") and ", max(" in candidate.get("hlslBody", "") for case, case_document, candidate in clamp_golden_candidates),
            "vector4MixCandidateMatchesLegacyMixRgbDefaultContract": dynamic_vector4_mix_candidate == regenerated_vector4_mix_candidate and regenerated_vector4_mix_candidate.get("outputType") == "Vector4" and dynamic_vector4_mix_document.get("output") == {"nodeId": "mixed", "socketId": "result"} and "float4(lerp((n_color1_value).rgb, (n_color2_value).rgb, saturate(n_factor_value)), lerp((n_color1_value).a, (n_color2_value).a, saturate(n_factor_value)))" in regenerated_vector4_mix_candidate.get("hlslBody", "") and [item.get("implementationId") for item in dynamic_registry_source.get("operations", [])].count("vector4.mix.saturated_factor") == 1 and "Blend blend = Blend::Mix;" in dynamic_mixrgb_interface and "bool clampFactor = true;" in dynamic_mixrgb_interface and "bool clampResult = false;" in dynamic_mixrgb_interface and "case MixRGBNode::Blend::Mix:" in dynamic_mixrgb_source and "float4 {0}_Color = float4({0}_rgb, lerp(({1}).a, ({2}).a, {0}_f));" in dynamic_mixrgb_source,
            "vector4MixAllLegacyModesLowerAsTypedGraphs": set(dynamic_mixrgb_graph_candidates) == set(expected_mix_modes) == set(dynamic_mix_rgb_fragments) and all(candidate.get("outputType") == "Vector4" and dynamic_mix_rgb_fragments[mode] in candidate.get("hlslBody", "") and "saturate(n_mix_factor_value)" in candidate.get("hlslBody", "") and "lerp((n_mix_color1_value).a, (n_mix_color2_value).a, saturate(n_mix_factor_value))" in candidate.get("hlslBody", "") for mode, candidate in dynamic_mixrgb_graph_candidates.items()),
            "vector4SeparateChannelsSupportsLinksFromEachOutput": dynamic_vector4_separate_candidate == regenerated_vector4_separate_candidate and regenerated_vector4_separate_candidate.get("outputType") == "Float" and dynamic_vector4_separate_document.get("output") == {"nodeId": "sum", "socketId": "result"} and all(f"float n_channels_{channel} = (n_color_value).{channel};" in regenerated_vector4_separate_candidate.get("hlslBody", "") for channel in ("x", "y", "z", "w")) and "(n_channels_z + n_offset_value)" in regenerated_vector4_separate_candidate.get("hlslBody", "") and [(item.socket_id, item.value_type) for item in dynamic_registry[("artifact.convert.vector4.separate_channels", 1)].outputs] == [("x", "Float"), ("y", "Float"), ("z", "Float"), ("w", "Float")],
            "vector4RampLowersEditableStableStopsAndBothOutputs": dynamic_vector4_ramp_candidate == regenerated_vector4_ramp_candidate and regenerated_vector4_ramp_candidate.get("outputType") == "Vector4" and dynamic_vector4_ramp_document.get("output") == {"nodeId": "ramp", "socketId": "color"} and [stop["position"] for stop in dynamic_vector4_ramp_document["nodes"][1]["parameters"]["stops"]] == [0.0, 0.5, 0.5, 1.0] and dynamic_vector4_ramp_document["nodes"][1]["parameters"]["stops"][1]["color"] != dynamic_vector4_ramp_document["nodes"][1]["parameters"]["stops"][2]["color"] and "float n_ramp_t_1 = saturate((n_ramp_factor - n_ramp_stop_0) / max(n_ramp_stop_1 - n_ramp_stop_0, 1e-5f));" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and "float n_ramp_t_2 = saturate((n_ramp_factor - n_ramp_stop_1) / max(n_ramp_stop_2 - n_ramp_stop_1, 1e-5f));" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and "float n_ramp_t_3 = saturate((n_ramp_factor - n_ramp_stop_2) / max(n_ramp_stop_3 - n_ramp_stop_2, 1e-5f));" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and "if (n_ramp_factor >= n_ramp_stop_0 && n_ramp_factor < n_ramp_stop_1) n_ramp_color = lerp(n_ramp_stop_color_0, n_ramp_stop_color_1, n_ramp_t_1);" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and "if (n_ramp_factor >= n_ramp_stop_3) n_ramp_color = n_ramp_stop_color_3;" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and "float n_ramp_alpha = n_ramp_color.a;" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and [(item.socket_id, item.value_type) for item in dynamic_registry[("artifact.color.ramp.linear_stops", 1)].outputs] == [("color", "Vector4"), ("alpha", "Float")] and dynamic_module.MAX_RAMP_STOPS == 32 and dynamic_module.MIN_RAMP_STOPS == 2,
            "vector4RampAlphaCanBeSelectedAsFloatOutput": regenerated_vector4_ramp_alpha_candidate.get("outputType") == "Float" and dynamic_vector4_ramp_alpha_document.get("output") == {"nodeId": "ramp", "socketId": "alpha"} and "return n_ramp_alpha;" in regenerated_vector4_ramp_alpha_candidate.get("hlslBody", ""),
            "vector4RampFactorComesFromSurfaceUv0X": dynamic_vector4_ramp_document["nodes"][0].get("operationId") == "artifact.input.texcoord.uv0.x" and {"from": {"nodeId": "uv", "socketId": "value"}, "to": {"nodeId": "ramp", "socketId": "factor"}} in dynamic_vector4_ramp_document.get("links", []) and "float n_uv_value = (In.UV).x;" in regenerated_vector4_ramp_candidate.get("hlslBody", "") and regenerated_vector4_ramp_candidate.get("contextBindings") == [{"contextId": "artifact.material.surface.uv0", "component": "x", "valueType": "Float", "hlslExpression": "(In.UV).x", "nodeIds": ["uv"]}] and {"implementationId": "float.uv0.x", "contextId": "artifact.material.surface.uv0", "component": "x", "valueType": "Float", "hlslCandidate": "(In.UV).x", "evidence": "ArtifactCore TexCoordNode lowers In.UV in the existing material graph path", "backendNeutral": False} in dynamic_contract.get("sourceContextBindings", []) and "float2 {0}_UV = In.UV;" in dynamic_mixrgb_source,
            "multipleSourceContextNodesDeduplicateAndRemainOrderStable": multi_context_candidate.get("contextBindings") == [{"contextId": "artifact.material.surface.uv0", "component": "x", "valueType": "Float", "hlslExpression": "(In.UV).x", "nodeIds": ["uv", "uv_second"]}] and multi_context_candidate["contextBindings"] == reordered_multi_context_candidate["contextBindings"] and multi_context_candidate["semanticHashSha256"] == reordered_multi_context_candidate["semanticHashSha256"] and multi_context_candidate["shaderStructureHashSha256"] == reordered_multi_context_candidate["shaderStructureHashSha256"] and multi_context_candidate["hlslBody"] == reordered_multi_context_candidate["hlslBody"],
            "hostContextAdapterChangesOnlyBackendExpression": alternate_context_candidate["semanticHashSha256"] == regenerated_vector4_ramp_candidate["semanticHashSha256"] and alternate_context_candidate["shaderStructureHashSha256"] == regenerated_vector4_ramp_candidate["shaderStructureHashSha256"] and alternate_context_candidate["hlslBody"] != regenerated_vector4_ramp_candidate["hlslBody"] and alternate_context_candidate["hlslBodySha256"] != regenerated_vector4_ramp_candidate["hlslBodySha256"] and alternate_context_candidate["hlslBodySha256"] == hashlib.sha256(alternate_context_candidate["hlslBody"].encode("utf-8")).hexdigest().upper() and "(MaterialInput.UV).x" in alternate_context_candidate["hlslBody"] and alternate_context_candidate["contextBindings"] == [{"contextId": "artifact.material.surface.uv0", "component": "x", "valueType": "Float", "hlslExpression": "(MaterialInput.UV).x", "nodeIds": ["uv"]}],
            "typedIRIsBackendSourceNeutralAndContentAddressed": dynamic_contract.get("typedIRContract") == typed_ir_contract_expected and all(typed_ir_hash_matches(item) for item in dynamic_candidate_results) and alternate_context_candidate["typedIR"] == regenerated_vector4_ramp_candidate["typedIR"] and alternate_context_candidate["typedIRSemanticHashSha256"] == regenerated_vector4_ramp_candidate["typedIRSemanticHashSha256"] and reordered_multi_context_candidate["typedIRSemanticHashSha256"] == multi_context_candidate["typedIRSemanticHashSha256"] and shared_graph_parameter_compile["typedIR"]["graphParameters"][0]["value"] == 0.25 and edited_graph_parameter_compile["typedIR"]["graphParameters"][0]["value"] == 0.75 and edited_graph_parameter_compile["typedIRSemanticHashSha256"] != shared_graph_parameter_compile["typedIRSemanticHashSha256"] and "(In.UV).x" not in json.dumps(regenerated_vector4_ramp_candidate["typedIR"], sort_keys=True) and "(MaterialInput.UV).x" not in json.dumps(alternate_context_candidate["typedIR"], sort_keys=True),
            "typedIREmitterConsumesOnlyIRAndExplicitBindings": "typed_ir" in inspect.signature(dynamic_module.emit_hlsl_from_typed_ir).parameters and "parameter_block" in inspect.signature(dynamic_module.emit_hlsl_from_typed_ir).parameters and "context_bindings" in inspect.signature(dynamic_module.emit_hlsl_from_typed_ir).parameters and not ({"document", "registry", "nodes", "writers"} & set(dynamic_module.emit_hlsl_from_typed_ir.__code__.co_names)) and "hlsl, emitted_context_bindings = emit_hlsl_from_typed_ir(" in inspect.getsource(dynamic_module.compile_document_from_data) and "hlsl_lines" not in inspect.getsource(dynamic_module.compile_document_from_data) and "input_expression(node_id" not in inspect.getsource(dynamic_module.compile_document_from_data),
            "typedIRHasSecondGLSL450Backend": "typed_ir" in inspect.signature(dynamic_module.emit_glsl_from_typed_ir).parameters and "parameter_block" in inspect.signature(dynamic_module.emit_glsl_from_typed_ir).parameters and "context_bindings" in inspect.signature(dynamic_module.emit_glsl_from_typed_ir).parameters and "emit_hlsl_from_typed_ir" not in dynamic_module.emit_glsl_from_typed_ir.__code__.co_names and glsl_ramp_interface.get("backendId") == "vulkan-glsl450-verbatim" and glsl_ramp_source.startswith("#version 450\n") and "layout(location = 0) out vec4 artifactGraphOutput;" in glsl_ramp_source and "artifactHostUV.x" in glsl_ramp_source and (glsl_ramp_interface.get("parameterBlockBinding") is None or glsl_ramp_interface.get("parameterBlockBinding") == {"descriptorSet": 1, "binding": 0}) and all(item.get("descriptorSet") == 1 and item.get("textureBinding") > 0 and item.get("samplerBinding") > 0 and item.get("textureBinding") != item.get("samplerBinding") for item in glsl_ramp_interface.get("resourceBindings", [])),
            "typedIREmissionIsIsolatedFromPostProjectionDocumentMutation": ir_isolation_document_mutation_passed,
            "typedIRHasWellTypedCompleteEdges": all(typed_ir_edges_are_well_typed(item) for item in dynamic_candidate_results + [multi_context_candidate, regenerated_vector4_ramp_alpha_candidate, optional_b_compile, optional_b_override_compile]) and dynamic_vector4_ramp_candidate["typedIR"]["schemaId"] == "artifact.shadergraph.typed-ir" and dynamic_vector4_separate_candidate["typedIR"]["schemaVersion"] == 1,
            "vector2UvContextLowersAsTypedHostInput": dynamic_vector2_uv_candidate == regenerated_vector2_uv_candidate and dynamic_vector2_uv_document.get("output") == {"nodeId": "uv", "socketId": "value"} and regenerated_vector2_uv_candidate.get("outputType") == "Vector2" and "float2 n_uv_value = In.UV;" in regenerated_vector2_uv_candidate.get("hlslBody", "") and regenerated_vector2_uv_candidate.get("typedIR", {}).get("operations", [])[0].get("sourceContext") == {"contextId": "artifact.material.surface.uv0", "component": "xy", "valueType": "Vector2"} and regenerated_vector2_uv_candidate.get("contextBindings") == [{"contextId": "artifact.material.surface.uv0", "component": "xy", "valueType": "Vector2", "hlslExpression": "In.UV", "nodeIds": ["uv"]}] and hashlib.sha256(dynamic_vector2_uv_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector2UvDocumentSha256") and hashlib.sha256(dynamic_vector2_uv_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector2UvCandidateSha256") and typed_ir_hash_matches(dynamic_vector2_uv_candidate) and {"implementationId": "vector2.uv0", "contextId": "artifact.material.surface.uv0", "component": "xy", "valueType": "Vector2", "hlslCandidate": "In.UV", "evidence": "ArtifactCore TexCoordNode lowers In.UV in the existing material graph path", "backendNeutral": False} in dynamic_contract.get("sourceContextBindings", []),
            "vector2MappingIsGraphSemanticAndLowersDeterministically": dynamic_vector2_mapping_candidate == regenerated_vector2_mapping_candidate and dynamic_vector2_mapping_document.get("output") == {"nodeId": "mapping", "socketId": "vector"} and "float2 n_uv_value = In.UV;" in regenerated_vector2_mapping_candidate.get("hlslBody", "") and "float2 n_mapping_vector = ((n_uv_value * float2(2.0f, 0.5f)) + float2(0.25f, -0.125f));" in regenerated_vector2_mapping_candidate.get("hlslBody", "") and regenerated_vector2_mapping_candidate.get("outputType") == "Vector2" and hashlib.sha256(dynamic_vector2_mapping_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector2MappingDocumentSha256") and hashlib.sha256(dynamic_vector2_mapping_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector2MappingCandidateSha256") and typed_ir_hash_matches(dynamic_vector2_mapping_candidate) and regenerated_vector2_mapping_candidate.get("semanticHashSha256") != regenerated_vector2_uv_candidate.get("semanticHashSha256") and edited_vector2_mapping_candidate.get("semanticHashSha256") != regenerated_vector2_mapping_candidate.get("semanticHashSha256") and edited_vector2_mapping_candidate.get("hlslBodySha256") != regenerated_vector2_mapping_candidate.get("hlslBodySha256") and "vector2.mapping.scale_offset" in dynamic_contract.get("supportedImplementationIds", []),
            "baseColorTextureHasTypedResourceAndExplicitUv": dynamic_base_color_texture_candidate == regenerated_base_color_texture_candidate and dynamic_base_color_texture_document.get("output") == {"nodeId": "texture", "socketId": "color"} and "float4 n_texture_color = g_BaseColorTexture.Sample(g_BaseColorSampler, n_uv_value);" in regenerated_base_color_texture_candidate.get("hlslBody", "") and regenerated_base_color_texture_candidate.get("resourceRequirements") == [{"resourceKey": "artifact.material.texture.base_color", "resourceKind": "sampledTexture2D", "slot": "BaseColor", "shaderSymbol": "g_BaseColorTexture", "samplerSymbol": "g_BaseColorSampler", "samplerContract": "legacy-shared-sampler-uncharacterized", "uvContract": "explicit-Vector2-input-no-implicit-transform", "sampleContract": "raw-float4-no-implicit-swizzle-or-color-decode"}] and regenerated_base_color_texture_candidate.get("typedIR", {}).get("operations", [])[1].get("resourceDependencies") == regenerated_base_color_texture_candidate.get("resourceRequirements") and "float n_texture_alpha = n_texture_color.a;" in regenerated_base_color_texture_candidate.get("hlslBody", "") and hashlib.sha256(dynamic_base_color_texture_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("baseColorTextureDocumentSha256") and hashlib.sha256(dynamic_base_color_texture_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("baseColorTextureCandidateSha256") and typed_ir_hash_matches(dynamic_base_color_texture_candidate) and [operation.get("operationId") for operation in dynamic_registry_source.get("operations", []) if operation.get("implementationId", "").startswith("texture.sample.") and operation.get("implementationId") != "texture.sample.image2d"] == dynamic_contract.get("textureSampleResourceContract", {}).get("operationIds"),
            "mappedUvFlowsThroughMappingNodeIntoTextureSample": dynamic_mapped_base_color_texture_candidate == regenerated_mapped_base_color_texture_candidate and dynamic_mapped_base_color_texture_document.get("output") == {"nodeId": "texture", "socketId": "color"} and [link.get("to") for link in dynamic_mapped_base_color_texture_document.get("links", []) if link.get("to", {}).get("socketId") == "uv"] == [{"nodeId": "texture", "socketId": "uv"}] and "float2 n_mapping_vector = ((n_uv_value * float2(2.0f, 0.5f)) + float2(0.25f, -0.125f));" in regenerated_mapped_base_color_texture_candidate.get("hlslBody", "") and "float4 n_texture_color = g_BaseColorTexture.Sample(g_BaseColorSampler, n_mapping_vector);" in regenerated_mapped_base_color_texture_candidate.get("hlslBody", "") and regenerated_mapped_base_color_texture_candidate.get("resourceRequirements") == regenerated_base_color_texture_candidate.get("resourceRequirements") and regenerated_mapped_base_color_texture_candidate.get("semanticHashSha256") != regenerated_base_color_texture_candidate.get("semanticHashSha256") and typed_ir_hash_matches(dynamic_mapped_base_color_texture_candidate) and hashlib.sha256(dynamic_mapped_base_color_texture_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("mappedBaseColorTextureDocumentSha256") and hashlib.sha256(dynamic_mapped_base_color_texture_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("mappedBaseColorTextureCandidateSha256") and dynamic_contract.get("textureSampleResourceContract", {}).get("imageEvaluationPlan", {}).get("mappedUvCandidate") == {"documentFixture": dynamic_contract.get("mappedBaseColorTextureDocumentFixture"), "documentSha256": dynamic_contract.get("mappedBaseColorTextureDocumentSha256"), "candidateFixture": dynamic_contract.get("mappedBaseColorTextureCandidateFixture"), "candidateSha256": dynamic_contract.get("mappedBaseColorTextureCandidateSha256"), "expectedCoordinateExpression": "(UV0 * float2(2.0, 0.5)) + float2(0.25, -0.125)", "contract": "the texture Sample consumes the Mapping node output; symbolic resource requirement equals the direct-UV candidate; GPU sampling remains unverified", "parameterCases": [{"id": "identity", "scale": [1.0, 1.0], "offset": [0.0, 0.0], "expected": "mappedUV equals input UV; compare pixel output with the direct-UV graph"}, {"id": "nonuniform-scale-offset", "scale": [2.0, 0.5], "offset": [0.25, -0.125], "expected": "mappedUV = inputUV * scale + offset; this is the persisted candidate fixture"}, {"id": "negative-scale", "scale": [-1.0, 1.0], "offset": [1.0, 0.0], "expected": "horizontal coordinate is mirrored around the unit interval before sampler addressing"}, {"id": "zero-scale", "scale": [0.0, 0.0], "offset": [0.375, 0.375], "expected": "all pixels sample the same texel-center coordinate, subject to the recorded filter and UV convention"}], "samplingRegions": ["UV texel centers and image corners", "UV seam and repeat-address crossings introduced by scale or offset", "individual RGBA channels and alpha transition", "full frame and named edge crops"], "imageAcceptance": "Capture five repeated direct-UV baselines before candidate review; derive per-region tolerance from baseline noise, then require identity Mapping to remain within that tolerance and each nonidentity case to match its declared coordinate response. Record filter and repeat behavior rather than assuming them."},
            "textureResourceBindingIdentityIsSeparateFromShaderIdentity": regenerated_base_color_texture_candidate.get("resourceBindingStatus") == "unresolved" and regenerated_base_color_texture_candidate.get("resourceBindingKeySha256") is None and resolved_texture_candidate_a.get("resourceBindingStatus") == "resolved" and resolved_texture_candidate_a.get("unresolvedResourceKeys") == [] and all(candidate.get("resourceBindingKeySha256") != resolved_texture_candidate_a.get("resourceBindingKeySha256") and candidate.get("semanticHashSha256") == resolved_texture_candidate_a.get("semanticHashSha256") and candidate.get("typedIRSemanticHashSha256") == resolved_texture_candidate_a.get("typedIRSemanticHashSha256") and candidate.get("shaderStructureHashSha256") == resolved_texture_candidate_a.get("shaderStructureHashSha256") and candidate.get("hlslBodySha256") == resolved_texture_candidate_a.get("hlslBodySha256") for candidate in texture_binding_mutations),
            "genericImageResourceReferenceProjectsNodeScopedRequirement": generic_image_relink_contract_ok,
            "genericImageResourceReferencePersistsThroughRoundtripUndoClipboardAndDuplicate":
                generic_image_relink_contract_ok and all(generic_image_persistence_contract.values()),
            "genericImageResourceReferenceFailsClosedAndKeepsMissingAssetUnresolved": unresolved_generic_image_candidate.get("assetResolutionStatus") == "unresolved" and unresolved_generic_image_candidate.get("resourceBindingKeySha256") is None and dangling_generic_resource_diagnostic == {"code": "resource.reference.missing", "severity": "error", "path": "/nodes/1/resourceReferences/image", "message": "node references a missing resourceId"} and unsupported_generic_schema_diagnostic == {"code": "schema.version.unsupported", "severity": "error", "path": "/schemaVersion", "message": "document schema identity is unsupported"} and incompatible_generic_image_diagnostic == {"code": "resource.binding.asset_type.incompatible", "severity": "error", "path": "/resources/image.albedo/source/assetId", "message": "sampledTexture2D requires an Image asset"} and unresolved_generic_binding_projection.get("resourceBindingStatus") == "unresolved" and unresolved_generic_binding_projection.get("resourceBindingKeySha256") is None,
            "genericImageResourceReferenceRejectsMalformedLinkEndpoints": malformed_generic_endpoint_diagnostic == {"code": "link.endpoint.invalid", "severity": "error", "path": "/links/0", "message": "link endpoint nodeId and socketId must be strings"} and invalid_generic_sampler_diagnostic == {"code": "sampler.anisotropy.invalid", "severity": "error", "path": "/nodes/1/sampler/maxAnisotropy", "message": "maxAnisotropy must be an integer from 1 to 16"} and invalid_generic_source_revision_diagnostic == {"code": "resource.source_revision.invalid", "severity": "error", "path": "/assetSourceStates/11111111-1111-4111-8111-111111111111/sourceRevision", "message": "sourceRevision must be a positive integer"},
            "textureBindingSourceAuditPinsStableAssetAndPathBoundary": all(fragment in dynamic_asset_database_interface for fragment in ("struct AssetInfo", "QUuid id;", "QString absolutePath;", "QUuid findAssetByPath")) and all(fragment in dynamic_project_items_interface for fragment in ("QString filePath;", "QUuid assetId;")) and all(fragment in dynamic_asset_manager_interface for fragment in ("sourceVersion(const QUuid& assetId)", "invalidateSource(const QUuid& assetId)")) and "std::uint64_t version = 1;" in dynamic_asset_manager_source and "enum class AssetType" in (root / "ArtifactCore/include/Asset/AssetType.ixx").read_text(encoding="utf-8") and "Image," in (root / "ArtifactCore/include/Asset/AssetType.ixx").read_text(encoding="utf-8") and "target.path == path && target.srv" in dynamic_mesh_renderer_source and "current != incoming.trimmed()" in dynamic_mesh_renderer_source and "materialSlotTextures" in dynamic_mesh_renderer_source and incompatible_texture_asset_diagnostic == {"code": "resource.binding.asset_type.incompatible", "severity": "error", "path": "/resourceBindings/artifact.material.texture.base_color/assetType", "message": "sampledTexture2D requires an Image asset"},
            "allSixMaterialTextureSlotsHaveDistinctTypedRequirements": [key for key, _ in texture_slot_candidates] == ["base_color", "opacity", "emission", "metallic_roughness", "normal", "occlusion"] and all(candidate.get("outputType") == "Vector4" and len(candidate.get("resourceRequirements", [])) == 1 and candidate["resourceRequirements"][0].get("resourceKey") == f"artifact.material.texture.{key}" and candidate["typedIR"]["operations"][1].get("resourceDependencies") == candidate["resourceRequirements"] and f"{candidate['resourceRequirements'][0]['shaderSymbol']}.Sample(g_BaseColorSampler, n_uv_value)" in candidate.get("hlslBody", "") for key, candidate in texture_slot_candidates),
            "textureSampleConnectsToSeparateChannelsAndSelectsPackedChannels": dynamic_texture_channels_candidate == regenerated_texture_channels_candidate and dynamic_texture_channels_document.get("output") == {"nodeId": "channels", "socketId": "y"} and dynamic_texture_channels_candidate.get("resourceRequirements", [{}])[0].get("resourceKey") == "artifact.material.texture.metallic_roughness" and dynamic_texture_channels_candidate.get("resourceRequirements", [{}])[0].get("shaderSymbol") == "g_MetallicRoughnessTexture" and "float n_channels_y = (n_texture_color).y;" in regenerated_texture_channels_candidate.get("hlslBody", "") and "return n_channels_y;" in regenerated_texture_channels_candidate.get("hlslBody", "") and "return n_channels_z;" in regenerated_texture_channels_blue_candidate.get("hlslBody", "") and regenerated_texture_channels_candidate.get("resourceRequirements") == regenerated_texture_channels_blue_candidate.get("resourceRequirements") and regenerated_texture_channels_candidate.get("semanticHashSha256") != regenerated_texture_channels_blue_candidate.get("semanticHashSha256") and hashlib.sha256(dynamic_texture_channels_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("textureChannelsDocumentSha256") and hashlib.sha256(dynamic_texture_channels_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("textureChannelsCandidateSha256") and typed_ir_hash_matches(dynamic_texture_channels_candidate),
            "vector2SocketsHaveDistinctCanvasColor": '.wire.Vector2 { stroke: #6bbdc2; }' in local_ui_html_source and '.socket.Vector2 { background: #4c9aa0; border-color: #8bd1d4; }' in local_ui_html_source,
            "optionalSocketDefaultIsEditableAndPersistedPerNode": dynamic_contract.get("socketDefaultContract") == socket_default_contract_expected and dynamic_optional_default_document.get("schemaVersion") == 3 and dynamic_optional_default_candidate == regenerated_optional_default_candidate and hashlib.sha256(dynamic_optional_default_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalDefaultDocumentSha256") and hashlib.sha256(dynamic_optional_default_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalDefaultCandidateSha256") and typed_ir_hash_matches(dynamic_optional_default_candidate) and dynamic_optional_default_document["nodes"][1]["socketDefaults"] == {"b": 0.375} and dynamic_optional_default_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == 0.375 and "(n_source_value + 0.375f)" in dynamic_optional_default_candidate["hlslBody"] and dynamic_optional_vector4_default_document.get("schemaVersion") == 3 and dynamic_optional_vector4_default_candidate == regenerated_optional_vector4_default_candidate and hashlib.sha256(dynamic_optional_vector4_default_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalVector4DefaultDocumentSha256") and hashlib.sha256(dynamic_optional_vector4_default_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalVector4DefaultCandidateSha256") and typed_ir_hash_matches(dynamic_optional_vector4_default_candidate) and dynamic_optional_vector4_default_document["nodes"][1]["socketDefaults"] == {"b": [0.1, 0.2, 0.3, 0.4]} and dynamic_optional_vector4_default_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896, 0.4000000059604645] and "(n_source_value + float4(0.100000001f, 0.200000003f, 0.300000012f, 0.400000006f))" in dynamic_optional_vector4_default_candidate["hlslBody"] and optional_vector4_default_edit_document["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.2, 0.3, 0.4, 0.5)]} and optional_vector4_default_edit_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.20000000298023224, 0.30000001192092896, 0.4000000059604645, 0.5] and "float4(0.200000003f, 0.300000012f, 0.400000006f, 0.5f)" in optional_vector4_default_edit_candidate["hlslBody"] and socket_default_roundtrip == socket_default_edit_document and socket_default_roundtrip["schemaVersion"] == 3 and socket_default_roundtrip["nodes"][2]["socketDefaults"] == {"b": 0.25} and socket_default_edit_compile["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == 0.25 and "(n_source_left_value + 0.25f)" in socket_default_edit_compile["hlslBody"] and socket_default_undo == optional_b_document and socket_default_redo == socket_default_edit_document and socket_default_clipboard["schemaVersion"] == 3 and socket_default_paste["schemaVersion"] == 3 and next(node for node in socket_default_paste["nodes"] if node["nodeId"] == "sum_copy")["socketDefaults"] == {"b": 0.25} and schema2_socket_default_edit["schemaVersion"] == 3 and schema2_socket_default_edit["graphParameters"] == [] and schema2_socket_default_edit["nodes"][2]["socketDefaults"] == {"b": 0.5} and linked_socket_default_edit_document["nodes"][2]["socketDefaults"] == {"b": 0.75} and linked_socket_default_edit_compile["typedIR"] == linked_socket_default_baseline_compile["typedIR"] and linked_socket_default_edit_compile["semanticHashSha256"] == linked_socket_default_baseline_compile["semanticHashSha256"] and linked_socket_default_edit_compile["hlslBody"] == linked_socket_default_baseline_compile["hlslBody"] and required_socket_default_diagnostic == {"code": "node.socket_default.not_optional", "severity": "error", "path": "/nodes/2/socketDefaults/a"} and overflow_socket_default_diagnostic == {"code": "socket.default.invalid", "severity": "error", "path": "/nodes/2/socketDefaults/b"} and socket_default_ui_session.document["schemaVersion"] == 3 and socket_default_ui_session.document["nodes"][0]["socketDefaults"] == {"b": 0.625} and socket_default_ui_session.history,
            "optionalVector4SocketDefaultSupportsLaneEditingAndDormantLinks": optional_vector4_default_edit_document["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.2, 0.3, 0.4, 0.5)]} and optional_vector4_default_edit_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.20000000298023224, 0.30000001192092896, 0.4000000059604645, 0.5] and vector4_default_wrong_arity_diagnostic == {"code": "socket.default.invalid", "severity": "error", "path": "/nodes/1/socketDefaults/b"} and vector4_default_overflow_diagnostic == {"code": "socket.default.invalid", "severity": "error", "path": "/nodes/1/socketDefaults/b/2"} and vector4_socket_default_ui_session.document["schemaVersion"] == 3 and vector4_socket_default_ui_session.document["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.7, 0.6, 0.5, 0.4)]} and vector4_socket_default_ui_session.history and vector4_default_connected_document["nodes"][1]["socketDefaults"] == {"b": [0.1, 0.2, 0.3, 0.4]} and vector4_default_connected_edited_document["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.9, 0.8, 0.7, 0.6)]} and vector4_default_connected_edited["typedIR"] == vector4_default_connected_baseline["typedIR"] and vector4_default_connected_edited["semanticHashSha256"] == vector4_default_connected_baseline["semanticHashSha256"] and vector4_default_connected_edited["hlslBody"] == vector4_default_connected_baseline["hlslBody"] and 'data-lane="${index}"' in local_ui_html_source,
            "optionalVector3SocketDefaultEditsUndoAndRedoAsThreeLanes": dynamic_optional_vector3_default_document.get("schemaVersion") == 3 and dynamic_optional_vector3_default_candidate == regenerated_optional_vector3_default_candidate and dynamic_optional_vector3_default_document["nodes"][1]["socketDefaults"] == {"b": [0.1, 0.2, 0.3]} and dynamic_optional_vector3_default_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.10000000149011612, 0.20000000298023224, 0.30000001192092896] and "float3(0.100000001f, 0.200000003f, 0.300000012f)" in dynamic_optional_vector3_default_candidate["hlslBody"] and optional_vector3_default_edit_document["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.4, 0.5, 0.6)]} and optional_vector3_default_edit_candidate["typedIR"]["operations"][1]["inputs"][1]["defaultValue"] == [0.4000000059604645, 0.5, 0.6000000238418579] and "float3(0.400000006f, 0.5f, 0.600000024f)" in optional_vector3_default_edit_candidate["hlslBody"] and vector3_default_ui_edited["nodes"][1]["socketDefaults"] == {"b": [round_binary32(v) for v in (0.7, 0.8, 0.9)]} and vector3_default_ui_undo == dynamic_optional_vector3_default_document and vector3_default_ui_redo == vector3_default_ui_edited and vector3_default_ui_session.history and "input.dataset.lane===undefined?input.valueAsNumber:fields.map(field=>field.valueAsNumber)" in local_ui_html_source and "post({type:'socketDefault',nodeId:input.dataset.node,socketId:input.dataset.socket,value})" in local_ui_html_source and hashlib.sha256(dynamic_optional_vector3_default_document_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalVector3DefaultDocumentSha256") and hashlib.sha256(dynamic_optional_vector3_default_candidate_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("optionalVector3DefaultCandidateSha256"),
            "optionalSocketDefaultIsTypedHashedAndOverridable": dynamic_contract.get("socketDefaultContract") == socket_default_contract_expected and optional_b_input == {"socketId": "b", "valueType": "Float", "required": False, "defaultValue": 0.0, "producer": None} and "(n_source_left_value + 0.0f)" in optional_b_compile["hlslBody"] and optional_b_override_compile["typedIRSemanticHashSha256"] != optional_b_compile["typedIRSemanticHashSha256"] and "(n_source_left_value + n_source_right_value)" in optional_b_override_compile["hlslBody"] and optional_b_override_compile["typedIR"] == changed_optional_default_connected_compile["typedIR"] and optional_b_override_compile["semanticHashSha256"] == changed_optional_default_connected_compile["semanticHashSha256"] and optional_b_override_compile["hlslBody"] == changed_optional_default_connected_compile["hlslBody"] and changed_optional_default_compile["semanticHashSha256"] != optional_b_compile["semanticHashSha256"] and "(n_source_left_value + 0.5f)" in changed_optional_default_compile["hlslBody"] and required_input_missing_diagnostic == {"code": "node.input.required_unconnected", "severity": "error", "path": "/nodes/2/inputs/a"} and any(item["operationId"] == "artifact.rd.math.add.float32.default_b" and item["inputs"][1].get("default") == 0.0 for item in dynamic_module.build_palette_model(dynamic_registry)) and "defaultText=socketDefault!==undefined" in local_ui_html_source and "'default'" in local_ui_html_source,
            "emittedSourceHashMatchesPinnedDynamicCandidate": regenerated_candidate.get("hlslBodySha256") == dynamic_contract.get("hlslSha256") == hashlib.sha256(regenerated_candidate.get("hlslBody", "").encode("utf-8")).hexdigest().upper() and dynamic_contract.get("emittedSourceHashContract") == "SHA-256 of exact UTF-8 hlslBody bytes; adapter-specific source identity, separate from semantic and structure hashes",
            "hostContextAdapterRejectsMalformedAndMissingBindings": malformed_context_binding_diagnostic == {"code": "shader.context.binding.invalid", "severity": "error", "path": "/contextBindings"} and missing_context_binding_diagnostic == {"code": "shader.context.binding.unsupported", "severity": "error", "path": "/typedIR/operations/uv/sourceContext"},
            "rampStopsPersistExactlyThroughFileRoundTrip": ramp_roundtrip_saved_hash == ramp_roundtrip_loaded_hash == ramp_roundtrip_resaved_hash and ramp_roundtrip_resaved_document == dynamic_vector4_ramp_document and ramp_roundtrip_resaved_document["nodes"][1]["parameters"]["stops"] == ramp_stops_before_edit,
            "rampStopsEditIsBoundedFiniteAtomicAndUndoable": ramp_edited_document["nodes"][1]["parameters"]["stops"] == ramp_stops_after_edit and dynamic_vector4_ramp_document["nodes"][1]["parameters"]["stops"] == ramp_stops_before_edit and [item.get("code") if item else None for item in ramp_invalid_edits] == ["parameter.ramp_stops.invalid"] * 4 and ramp_session_before == ramp_session_undo and ramp_session_after == ramp_session_redo and ramp_session_after["nodes"][1]["parameters"]["stops"] == ramp_stops_after_edit,
            "rampBinary32GoldenPinsDuplicateBoundaryAndImagePlan": hashlib.sha256(dynamic_vector4_ramp_golden_path.read_bytes()).hexdigest().upper() == dynamic_contract.get("vector4RampFormulaGoldenSha256") and dynamic_vector4_ramp_golden.get("sourceDocumentSha256") == dynamic_contract.get("vector4RampDocumentSha256") and dynamic_vector4_ramp_golden.get("factors") == [0.0, 0.4999, 0.5, 0.5001, 1.0] and dynamic_vector4_ramp_golden.get("cases", [])[2].get("selection") == {"kind": "segment", "leftAuthoredIndex": 2, "rightAuthoredIndex": 3} and dynamic_vector4_ramp_golden.get("cases", [])[2].get("color") == [0.0, 1.0, 0.0, 0.800000011920929] and (root / dynamic_contract.get("vector4RampFormulaGoldenGenerator", "")).is_file() and dynamic_contract.get("rampImageEvaluationPlan", {}).get("captureRoute") == "vulkan-meshrenderer-rgba32f-capture-source-added-not-run-candidate-injected-via-setMaterialGraphBlock" and dynamic_contract.get("rampImageEvaluationPlan", {}).get("comparison", "").startswith("legacy Core ColorRamp GPU output at all samples as a characterization baseline") and "repeat baseline captures five times" in dynamic_contract.get("rampImageEvaluationPlan", {}).get("acceptance", "") and dynamic_contract.get("rampImageEvaluationPlan", {}).get("captureConditions", {}).get("alpha") == "record raw alpha and composite the same capture over black and white",
            "rampCpuImageOracleArtifactsAreDeterministicAndFinite": dynamic_ramp_image_report.get("kind") == "artifact-shader-graph-ramp-cpu-image-oracle" and dynamic_ramp_image_report.get("status") == "cpu-formula-image-only-no-core-hlsl-runtime-or-backend-capture" and dynamic_ramp_image_report.get("sourceDocumentSha256") == dynamic_contract.get("vector4RampDocumentSha256") and dynamic_ramp_image_report.get("width") == 256 and dynamic_ramp_image_report.get("height") == 256 and dynamic_ramp_image_report.get("swatchFactors") == [0.0, 0.4999, 0.5, 0.5001, 1.0] and dynamic_ramp_image_report.get("swatchCenterSamples", [])[2].get("rgba") == dynamic_vector4_ramp_golden.get("cases", [])[2].get("color") and dynamic_ramp_image_report.get("swatchCenterSamples", [])[2].get("selection") == {"kind": "segment", "leftAuthoredIndex": 2, "rightAuthoredIndex": 3} and hashlib.sha256(dynamic_ramp_image_report_path.read_bytes()).hexdigest().upper() == dynamic_ramp_image_plan.get("generatedOracle", {}).get("reportSha256") and all((root / artifact.get("rawPath", "")).is_file() and (root / artifact.get("rawPath", "")).stat().st_size == 256 * 256 * 16 and hashlib.sha256((root / artifact.get("rawPath", "")).read_bytes()).hexdigest().upper() == artifact.get("rawSha256") and all(channel.get("nonFiniteCount") == 0 for channel in artifact.get("statistics", {}).values()) and all((root / image.get("path", "")).is_file() and hashlib.sha256((root / image.get("path", "")).read_bytes()).hexdigest().upper() == image.get("sha256") for image in artifact.get("displayPngs", {}).values()) for artifact in dynamic_ramp_image_report.get("artifacts", {}).values()) and dynamic_ramp_image_plan.get("captureRoute") == "vulkan-meshrenderer-rgba32f-capture-source-added-not-run-candidate-injected-via-setMaterialGraphBlock",
            "rampGpuCaptureSourcePinsHostAdapterAndFixtures": dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("status") == "source-added-not-run" and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testName") in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "setMaterialGraphBlock(candidateBlock)" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "writeRgba32f" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "writeDisplayComposite" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and all(field in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") for field in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("reportEvidence", {}).get("inputSha256Fields", []) + dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("reportEvidence", {}).get("outputSha256Fields", []) + dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("reportEvidence", {}).get("measurementFields", [])) and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("legacyFixture", "")).is_file() and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("candidateFixture", "")).is_file() and "test-only host binding" in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("adapterScope", "") and "capture has not been run" in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("limitations", []) and any("compiler version and build identity are not exposed" in item for item in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("limitations", [])) and "Vulkan" in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("route", "") and "every supported backend" in dynamic_ramp_image_plan.get("captureConditions", {}).get("runtime", "") and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("roughnessMathCapture", {}).get("legacyFixture") == "tests/fixtures/shader-graph-rd/texcoord_math_roughness.json" and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("roughnessMathCapture", {}).get("projectionCandidate") == "experiments/ShaderGraphLab/core_math_add_projection_candidate.json" and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("roughnessMathCapture", {}).get("legacyFixture", "")).is_file() and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("roughnessMathCapture", {}).get("projectionCandidate", "")).is_file() and "renderer.setMaterialGraphJson" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "renderer.setMaterialGraphBlock(roughnessMathCandidateBlock)" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "redChannelNumericSamples" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8"),
            "rampGpuCaptureComparesBaseColorTypedMathVariants": (
                base_color_capture_contract.get("variants") ==
                    ["add", "subtract", "multiply", "dividePositiveFloor"] and
                base_color_capture_contract.get("legacyMathOps") == [0, 1, 2, 3] and
                base_color_capture_contract.get("reportField") ==
                    "dynamicBaseColorMath.operationCaptures" and
                base_color_capture_contract.get("status") == "source-added-not-run" and
                base_color_capture_contract.get("numericTolerance", {}).get("kind") ==
                    "per-channel-absolute" and
                base_color_capture_contract.get("numericTolerance", {}).get("value") == 1.0e-5 and
                base_color_capture_contract.get("numericTolerance", {}).get("unit") ==
                    "linear RGBA32F channel value" and
                base_color_capture_contract.get("numericTolerance", {}).get("frozenBeforeCapture") is True and
                base_color_capture_contract.get("numericTolerance", {}).get("status") ==
                    "source-threshold-predeclared-runtime-unmeasured" and
                (root / base_color_capture_contract.get("legacyFixture", "")).is_file() and
                (root / base_color_capture_contract.get("projectionCandidate", "")).is_file() and
                all(fragment in base_color_capture_source for fragment in (
                    "baseColorMathFixturePath", "baseColorMathVariants",
                    "renderer.setMaterialGraphJson",
                    "renderer.setMaterialGraphBlock(candidateBlock)",
                    "dynamicBaseColorMath", "baseColorMathOperationCaptures",
                    "baseColorMathNumericSamples", "writeRgba32f",
                    "constexpr float baseColorMathAbsoluteTolerance = 1.0e-5f;",
                    "numericToleranceAbsolute", "numericToleranceUnit")) and
                all(f'QStringLiteral("{variant}")' in base_color_capture_source
                    for variant in base_color_capture_contract.get("variants", [])) and
                f'BaseColorMathCaptureRecord, {len(base_color_capture_contract.get("variants", []))}>'
                    in base_color_capture_source and
                "baseColorLegacyOperations{" + ", ".join(str(value) for value in
                    base_color_capture_contract.get("legacyMathOps", [])) + "}"
                    in base_color_capture_source),
            "rampAnalyzerRecomputesBaseColorMathSamplesFromRawCapture": dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("sampledCaptureAnalysis", {}).get("reportField") == "baseColorMathSampleAnalysis" and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("sampledCaptureAnalysis", {}).get("status") == "analysis-source-added-not-run" and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("sampledCaptureAnalysis", {}).get("sourceValidationCheck") == "baseColorCaptureAnalyzerVerifiesToleranceCounts" and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "") and all(fragment in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") for fragment in ("def _sampled_capture_comparisons(", "baseColorMathOperationCaptures", "samplePixelCoordinates", "legacyVsProjectedAbsoluteErrorRgba", "legacyVsGoldenToleranceExceedanceCount", "projectedVsGoldenToleranceExceedanceCount", "numericToleranceAbsolute", "raw capture")),
            "baseColorCaptureAnalyzerVerifiesToleranceCounts": base_color_capture_analyzer_probe,
            "rampGpuCaptureUsesRawBaseColorAovAndFormulaGolden": "instance.timeOffset = 1.0f" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("reportEvidence", {}).get("renderOutputModeField") in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("formulaGoldenFixture", "").split("/")[-1] in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and "candidateVsFormulaGoldenAbsoluteError" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("formulaGoldenFixture", "")).is_file() and (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).is_file() and "--capture-report" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "capture-source-executed-on-test-run" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "rawCaptureLayout" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "captureReportSha256" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "baselineRepeatNoiseRange" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "candidateVsBaselineMeanError" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") and "outsideUnitRangeCount" in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8"),
            "legacyCoreProjectionMapsSelectedRgbAddAndRetainsSource": legacy_projection_contract.get("status") == "read-only-projection-source-audited-python-only" and legacy_projection.get("status") == "read-only-projection" and legacy_projection.get("opaqueSourceJson") == legacy_projection_source_json and legacy_projection.get("sourceSha256") == legacy_projection_contract.get("sourceSha256") and legacy_projection.get("includedNodeIds") == legacy_projection_contract.get("includedNodeIds") and legacy_projection.get("excludedNodeIds") == legacy_projection_contract.get("excludedNodeIds") and legacy_projection_candidate.get("document") == legacy_projection["document"] and legacy_projection_candidate.get("candidateHlslBody") == legacy_projection_compile.get("hlslBody") and legacy_projection_candidate.get("candidateHlslBodySha256") == legacy_projection_contract.get("candidateHlslBodySha256") == legacy_projection_compile.get("hlslBodySha256") and legacy_projection_candidate.get("cpuVector4AddGolden") == [0.75, -1.0, -1.0, 0.75] and legacy_projection_candidate.get("negativeProbe", {}).get("unsupportedMathOperation") == "legacy.node.operation.unsupported" and legacy_projection_candidate.get("negativeProbe", {}).get("opaqueExtensionPreservedVerbatim") is True and "case MathNode::Op::Add:" in legacy_core_shader_node_source and 'expr = std::format("({0} + {1})"' in legacy_core_shader_node_source and 'float2 {0}_UV = In.UV;' in legacy_core_shader_node_source and 'return v + ".x";' in legacy_core_shader_node_source and 'Out.UV = In.uv;' in legacy_mesh_renderer_source and all(fragment in legacy_projection_generator_source for fragment in ("unsupported legacy Math operation was not rejected", "opaque source extension was lost or changed projected semantics", "source JSON text is retained by the adapter result")),
            "legacyBaseColorProjectionCoversVector4MathVariants": (
                '"add": {' in legacy_projection_generator_source and
                '"operationId": "artifact.rd.math.add.vector4.float32"' in legacy_projection_generator_source and
                'parser.add_argument("--check", action="store_true"' in legacy_projection_generator_source and
                "checked-in candidate differs from generated projection" in legacy_projection_generator_source and
                all(name in legacy_base_color_variants for name in
                    ("add", "subtract", "multiply", "dividePositiveFloor")) and
                legacy_base_color_variants.get("add", {}).get("operationId") ==
                    "artifact.rd.math.add.vector4.float32" and
                legacy_base_color_variants.get("subtract", {}).get("operationId") ==
                    "artifact.rd.math.subtract.vector4.float32" and
                legacy_base_color_variants.get("multiply", {}).get("operationId") ==
                    "artifact.rd.math.multiply.vector4.float32" and
                legacy_base_color_variants.get("dividePositiveFloor", {}).get("operationId") ==
                    "artifact.rd.math.divide.vector4.positive_floor.float32" and
                all(legacy_base_color_variant_compiles.get(name, {}).get("hlslBody") ==
                        legacy_base_color_variants[name].get("candidateHlslBody") and
                    legacy_base_color_variant_compiles.get(name, {}).get("hlslBodySha256") ==
                        legacy_base_color_variants[name].get("candidateHlslBodySha256") and
                    legacy_base_color_variants[name].get("cpuVector4Golden") ==
                        legacy_base_color_expected[name] and
                    len(legacy_base_color_variants[name].get("cpuVector4Golden", [])) == 4
                    for name in ("add", "subtract", "multiply", "dividePositiveFloor")) and
                all(f"case MathNode::Op::{operation}:" in legacy_core_shader_node_source
                    for operation in ("Add", "Subtract", "Multiply", "Divide")) and
                "float4(1e-5f,1e-5f,1e-5f,1e-5f)" in legacy_core_shader_node_source),
            "legacyCoreProjectionCompilesThroughBothVulkanFrontends": legacy_projection_compiler_contract.get("targetEnvironment") == "vulkan1.2" and legacy_projection_compiler_contract.get("candidateSha256") == hashlib.sha256(legacy_projection_candidate_path.read_bytes()).hexdigest().upper() and legacy_projection_compiler_contract.get("candidateCases") == ["base_color_add", "nested_add", "scalar_conversion", "base_color_default", "base_color_subtract", "base_color_multiply", "base_color_divide_positive_floor", "dynamic_roughness_uv0_x", "legacy_roughness_subtract", "legacy_roughness_dividePositiveFloor"] and legacy_projection_compiler_contract.get("dynamicInput", {}).get("fixture") == "tests/fixtures/shader-graph-rd/texcoord_math_roughness.json" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("fixtureSha256") == hashlib.sha256(legacy_roughness_math_fixture_path.read_bytes()).hexdigest().upper() and legacy_projection_compiler_contract.get("dynamicInput", {}).get("targetInputSocket") == "Roughness" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("candidateProperty") == "dynamicRoughnessUvProjection" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("rendererEpilogue", {}).get("coreSourceExpression") == "roughness = clamp(graphRoughness, 0.04f, 1.0f);" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("rendererEpilogue", {}).get("glslExpression") == "artifactGraphOutput = clamp(n_roughness_result, 0.04, 1.0);" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("rendererEpilogue", {}).get("hlslExpression") == "return clamp(n_roughness_result, 0.04f, 1.0f);" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("rendererEpilogue", {}).get("sampleUv0X") == [-0.25, 0.0, 0.125, 0.5, 1.0, 1.75] and all(fragment in legacy_projection_compiler_source for fragment in ("glsl_output_match = re.search(", "hlsl_output_match = re.search(", "f\"artifactGraphOutput = clamp(", "f\"return clamp(", "coreRoughnessEpilogue")) and "roughness = clamp(graphRoughness, 0.04f, 1.0f);" in legacy_mesh_renderer_source and legacy_uv_math_contract.get("fixture") == "experiments/ShaderGraphLab/dynamic_uv_roughness_math.json" and legacy_uv_math_contract.get("fixtureSha256") == hashlib.sha256(legacy_uv_math_fixture_path.read_bytes()).hexdigest().upper() and legacy_uv_math_contract.get("semanticHashSha256") == legacy_uv_math_compiled.get("semanticHashSha256") and legacy_uv_math_contract.get("hlslBodySha256") == legacy_uv_math_compiled.get("hlslBodySha256") and legacy_uv_math_contract.get("sampleUv0X") == [-0.25, 0.0, 0.125, 0.5, 1.0, 1.75] and legacy_uv_math_contract.get("hlslOutputExpression") in legacy_uv_math_compiled.get("hlslBody", "") and legacy_uv_math_contract.get("hlslOutputExpression") in legacy_uv_math_probe_source and all(fragment in legacy_uv_math_probe_source for fragment in ("artifact.material.surface.uv0", "artifactHostUV.x", "In.UV.x", "numeric.f32_bits(glsl_case[\"output\"])", "numeric.f32_bits(hlsl_case[\"output\"])", "spirv-opt", '"semanticHashSha256": compiled["semanticHashSha256"]', '"hlslBodySha256": compiled["hlslBodySha256"]')) and legacy_projection_compiler_contract.get("dynamicInput", {}).get("sampleUv0X") == [-0.25, 0.0, 0.125, 0.5, 1.0, 1.75] and legacy_projection_compiler_contract.get("dynamicInput", {}).get("numericOracle") == "six UV0.x values specialized in both frontends, spirv-opt -O, and exact binary32 scalar output comparison against f32(f32(UV0.x * 0.5) + 0.125)" and legacy_projection_compiler_contract.get("dynamicInput", {}).get("negativeCases") == {"unsupportedOperation": "legacy.node.operation.unsupported", "clampedMath": "legacy.node.operation.unsupported", "duplicateInputWriter": "legacy.link.cardinality.invalid", "cycle": "legacy.graph.cycle"} and legacy_projection_compiler_contract.get("dynamicInput", {}).get("nodeLinkStorageOrderInvariant") is True and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("document") == legacy_roughness_math_projection.get("document") == legacy_roughness_reordered_projection.get("document") and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("includedNodeIds") == legacy_roughness_math_projection.get("includedNodeIds") == ["uv", "gain", "scale", "bias", "roughness"] and legacy_roughness_reordered_compiled.get("hlslBody") == legacy_roughness_math_compiled.get("hlslBody") and legacy_roughness_reordered_projection.get("opaqueSourceJson") == legacy_roughness_reordered_source_json and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("candidateHlslBody") == legacy_roughness_math_compiled.get("hlslBody") and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("candidateHlslBodySha256") == legacy_roughness_math_compiled.get("hlslBodySha256") and legacy_roughness_math_projection.get("opaqueSourceJson") == legacy_roughness_math_source_json and legacy_projection_candidate.get("negativeProbe", {}).get("unsupportedRoughnessMathOperation") == "legacy.node.operation.unsupported" and legacy_projection_candidate.get("negativeProbe", {}).get("clampedRoughnessMath") == "legacy.node.operation.unsupported" and legacy_projection_candidate.get("negativeProbe", {}).get("duplicateRoughnessMathWriter") == "legacy.link.cardinality.invalid" and legacy_projection_candidate.get("negativeProbe", {}).get("roughnessMathCycle") == "legacy.graph.cycle" and legacy_projection_candidate.get("negativeProbe", {}).get("roughnessNodeLinkOrderInvariant") is True and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("outputType") == "Float" and legacy_projection_candidate.get("dynamicRoughnessUvProjection", {}).get("sourceSha256") == hashlib.sha256(legacy_roughness_math_fixture_path.read_bytes()).hexdigest().upper() and legacy_projection_compiler_contract.get("numericOracle") == "exact binary32 output comparison against each projection CPU golden after spirv-opt -O" and legacy_projection_compiler_contract.get("evidencePath") == "experiments/ShaderGraphLab/legacy_core_projection_compiler_evidence.json" and legacy_projection_compiler_evidence_path.is_file() and hashlib.sha256(legacy_projection_compiler_evidence_path.read_bytes()).hexdigest().upper() == legacy_projection_compiler_contract.get("evidenceSha256") and legacy_projection_compiler_evidence.get("schemaId") == "artifact.shadergraph.legacy-core-projection-compiler-evidence" and legacy_projection_compiler_evidence.get("status") == "passed" and legacy_projection_compiler_evidence.get("sourceCandidateSha256") == legacy_projection_compiler_contract.get("candidateSha256") and legacy_projection_compiler_evidence.get("candidateCount") == 10 and legacy_projection_compiler_evidence.get("passedCount") == 10 and all(item.get("status") == "passed" for item in legacy_projection_compiler_evidence.get("candidates", [])) and legacy_projection_compiler_contract.get("runtimeStatus") == "compiler-and-constant-folded-numeric-golden-no-gpu-execution" and all(fragment in legacy_projection_compiler_source for fragment in ("CASES = (", '("document", "base_color_add")', '("nestedAddProjection", "nested_add")', '("scalarBaseColorConversionProbe", "scalar_conversion")', '("unconnectedBaseColorDefaultProbe", "base_color_default")', '"-fspv-target-env=vulkan1.2"', '"--target-env", "vulkan1.2"', 'str(args.spirv_opt), "-O"', "numeric_probe.output_vector", "numeric_probe.f32_bits", '"exactBinary32GoldenMatch": output_match', 'uv_projection = candidate["dynamicRoughnessUvProjection"]', '"dynamicInputRetained": "In.UV.x" in hlsl_uv and "artifactHostUV.x" in glsl_uv', '"interfaceMatch": interface_match', "def extract_output_scalar(", "def compile_scalar_frontend(", 'uv_projection["numericCases"]', 'numeric_probe.f32_bits(glsl_sample["output"])')),
            "legacyRoughnessProjectionCoversSubtractAndPositiveFloorDivide": legacy_roughness_operation_variants.get("subtract", {}).get("operationId") == "artifact.rd.math.subtract.float32" and legacy_roughness_operation_variants.get("dividePositiveFloor", {}).get("operationId") == "artifact.rd.math.divide.positive_floor.float32" and len(legacy_roughness_operation_variants.get("subtract", {}).get("numericCases", [])) == 6 and len(legacy_roughness_operation_variants.get("dividePositiveFloor", {}).get("numericCases", [])) == 6 and "(n_uv_value - n_gain_value)" in legacy_roughness_operation_compiles.get("subtract", {}).get("hlslBody", "") and "(n_uv_value / max(n_gain_value, 1e-5f))" in legacy_roughness_operation_compiles.get("dividePositiveFloor", {}).get("hlslBody", "") and "case MathNode::Op::Subtract:" in legacy_core_shader_node_source and "case MathNode::Op::Divide:" in legacy_core_shader_node_source and "artifact.rd.math.divide.positive_floor.float32" in legacy_projection_generator_source and legacy_projection_candidate.get("negativeProbe", {}).get("unsupportedRoughnessMathOperation") == "legacy.node.operation.unsupported",
            "rampNumericProbeSeparatesShaderMixFromLerpRounding": all(fragment in (root / "tools/shader_graph_rd/probe_vulkan_ramp_numeric_parity.py").read_text(encoding="utf-8") for fragment in ("def shader_mix_f32(", '"cpuShaderMixRGBA32F"', '"glslShaderMixExactBinary32"', '"hlslShaderMixExactBinary32"')),
            "rampCaptureAnalyzerConsumesNamedReportRegions": all(field in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") for field in dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("reportEvidence", {}).get("measurementFields", [])) and all(field in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("testSource", "")).read_text(encoding="utf-8") for field in ("captureRegions", "\"name\"", "\"x\"", "\"y\"", "\"width\"", "\"height\"")) and all(fragment in (root / dynamic_ramp_image_plan.get("gpuCaptureSource", {}).get("analysisTool", "")).read_text(encoding="utf-8") for fragment in ("capture_report.get(\"captureRegions\")", "_parse_roi(region_value, width, height)", "regions.extend(capture_input[\"regions\"])", "ROI names must be unique")),
            "candidateGeneratorRegistersAllTwentyOnePersistedGraphs": all(fragment in candidate_generator_source for fragment in (
                '("documentFixture", "candidateFixture", "candidateSha256", "documentSha256")',
                '("subtractDocumentFixture", "subtractCandidateFixture", "subtractCandidateSha256", "subtractDocumentSha256")',
                '("divideDocumentFixture", "divideCandidateFixture", "divideCandidateSha256", "divideDocumentSha256")',
                '("vector4DivideDocumentFixture", "vector4DivideCandidateFixture", "vector4DivideCandidateSha256", "vector4DivideDocumentSha256")',
                '("vector4SubtractDocumentFixture", "vector4SubtractCandidateFixture", "vector4SubtractCandidateSha256", "vector4SubtractDocumentSha256")',
                '("vector4InvertDocumentFixture", "vector4InvertCandidateFixture", "vector4InvertCandidateSha256", "vector4InvertDocumentSha256")',
                '("clampDocumentFixture", "clampCandidateFixture", "clampCandidateSha256", "clampDocumentSha256")',
                '("vector4MixDocumentFixture", "vector4MixCandidateFixture", "vector4MixCandidateSha256", "vector4MixDocumentSha256")',
                '("vector4SeparateDocumentFixture", "vector4SeparateCandidateFixture", "vector4SeparateCandidateSha256", "vector4SeparateDocumentSha256")',
                '("vector4RampDocumentFixture", "vector4RampCandidateFixture", "vector4RampCandidateSha256", "vector4RampDocumentSha256")')) and all(fragment in candidate_generator_source for fragment in (
                '"vector3ConstantDocumentFixture", "vector3ConstantCandidateFixture"',
                '"vector3ConstantCandidateSha256", "vector3ConstantDocumentSha256"',
                '"vector3ArithmeticDocumentFixture", "vector3ArithmeticCandidateFixture"',
                '"vector3ArithmeticCandidateSha256", "vector3ArithmeticDocumentSha256"',
                '"vector3GraphParameterDocumentFixture", "vector3GraphParameterCandidateFixture"',
                '"vector3GraphParameterCandidateSha256", "vector3GraphParameterDocumentSha256"',
                '"optionalDefaultDocumentFixture", "optionalDefaultCandidateFixture"',
                '"optionalDefaultCandidateSha256", "optionalDefaultDocumentSha256"',
                '"optionalVector4DefaultDocumentFixture", "optionalVector4DefaultCandidateFixture"',
                '"optionalVector4DefaultCandidateSha256", "optionalVector4DefaultDocumentSha256"',
                '"optionalVector3DefaultDocumentFixture", "optionalVector3DefaultCandidateFixture"',
                '"optionalVector3DefaultCandidateSha256", "optionalVector3DefaultDocumentSha256"',
                '"vector2UvDocumentFixture", "vector2UvCandidateFixture"',
                '"vector2UvCandidateSha256", "vector2UvDocumentSha256"',
                '"vector2MappingDocumentFixture", "vector2MappingCandidateFixture"',
                '"vector2MappingCandidateSha256", "vector2MappingDocumentSha256"',
                '"baseColorTextureDocumentFixture", "baseColorTextureCandidateFixture"',
                '"baseColorTextureCandidateSha256", "baseColorTextureDocumentSha256"',
                '"mappedBaseColorTextureDocumentFixture", "mappedBaseColorTextureCandidateFixture"',
                '"mappedBaseColorTextureCandidateSha256", "mappedBaseColorTextureDocumentSha256"',
                '"textureChannelsDocumentFixture", "textureChannelsCandidateFixture"',
                '"textureChannelsCandidateSha256", "textureChannelsDocumentSha256"')) and all(key in dynamic_contract for key in (
                    "documentFixture", "candidateFixture", "candidateSha256", "documentSha256",
                    "vector3ConstantDocumentFixture", "vector3ConstantCandidateFixture", "vector3ConstantCandidateSha256", "vector3ConstantDocumentSha256",
                    "vector3ArithmeticDocumentFixture", "vector3ArithmeticCandidateFixture", "vector3ArithmeticCandidateSha256", "vector3ArithmeticDocumentSha256",
                    "vector3GraphParameterDocumentFixture", "vector3GraphParameterCandidateFixture", "vector3GraphParameterCandidateSha256", "vector3GraphParameterDocumentSha256",
                    "subtractDocumentFixture", "subtractCandidateFixture", "subtractCandidateSha256", "subtractDocumentSha256",
                    "divideDocumentFixture", "divideCandidateFixture", "divideCandidateSha256", "divideDocumentSha256",
                    "vector4DivideDocumentFixture", "vector4DivideCandidateFixture", "vector4DivideCandidateSha256", "vector4DivideDocumentSha256",
                    "vector4SubtractDocumentFixture", "vector4SubtractCandidateFixture", "vector4SubtractCandidateSha256", "vector4SubtractDocumentSha256",
                    "vector4InvertDocumentFixture", "vector4InvertCandidateFixture", "vector4InvertCandidateSha256", "vector4InvertDocumentSha256",
                    "clampDocumentFixture", "clampCandidateFixture", "clampCandidateSha256", "clampDocumentSha256",
                    "vector4MixDocumentFixture", "vector4MixCandidateFixture", "vector4MixCandidateSha256", "vector4MixDocumentSha256",
                    "vector4SeparateDocumentFixture", "vector4SeparateCandidateFixture", "vector4SeparateCandidateSha256", "vector4SeparateDocumentSha256",
                    "vector4RampDocumentFixture", "vector4RampCandidateFixture", "vector4RampCandidateSha256", "vector4RampDocumentSha256",
                    "optionalDefaultDocumentFixture", "optionalDefaultCandidateFixture", "optionalDefaultCandidateSha256", "optionalDefaultDocumentSha256",
                    "optionalVector4DefaultDocumentFixture", "optionalVector4DefaultCandidateFixture", "optionalVector4DefaultCandidateSha256", "optionalVector4DefaultDocumentSha256",
                    "optionalVector3DefaultDocumentFixture", "optionalVector3DefaultCandidateFixture", "optionalVector3DefaultCandidateSha256", "optionalVector3DefaultDocumentSha256",
                    "vector2UvDocumentFixture", "vector2UvCandidateFixture", "vector2UvCandidateSha256", "vector2UvDocumentSha256",
                    "vector2MappingDocumentFixture", "vector2MappingCandidateFixture", "vector2MappingCandidateSha256", "vector2MappingDocumentSha256",
                    "baseColorTextureDocumentFixture", "baseColorTextureCandidateFixture", "baseColorTextureCandidateSha256", "baseColorTextureDocumentSha256",
                    "mappedBaseColorTextureDocumentFixture", "mappedBaseColorTextureCandidateFixture", "mappedBaseColorTextureCandidateSha256", "mappedBaseColorTextureDocumentSha256",
                    "textureChannelsDocumentFixture", "textureChannelsCandidateFixture", "textureChannelsCandidateSha256", "textureChannelsDocumentSha256")),
            "paletteSearchFiltersRegisteredOperations": '<input id="palette-search" type="search"' in local_ui_html_source and 'aria-label="Filter node palette"' in local_ui_html_source and 'function renderPalette()' in local_ui_html_source and 'state.palette.filter(item=>[item.label,item.category,item.operationId]' in local_ui_html_source and "$('palette-empty').hidden=matching.length>0;" in local_ui_html_source and "$('palette-search').addEventListener('input'" in local_ui_html_source,
            "paletteEntriesExposeTypedSignatures": 'const pinSummary=pins=>pins.map(pin=>`${pin.socketId}:${pin.valueType}${pin.required?\' (required)\':\'\'}`).join(\', \')||\'none\';' in local_ui_html_source and 'Inputs: ${pinSummary(item.inputs)} · Outputs: ${pinSummary(item.outputs)}' in local_ui_html_source and 'title="${esc(signature)}"' in local_ui_html_source and 'aria-label="${esc(`${item.label}. ${signature}`)}"' in local_ui_html_source,
            "graphNodeLimitRejectsBeforeNodeIndexing": graph_limits == {"maxNodes": dynamic_module.MAX_GRAPH_NODES, "maxLinks": dynamic_module.MAX_GRAPH_LINKS, "evidenceStatus": "provisional-unprofiled"} and oversized_nodes_validation["status"] == "invalid" and oversized_nodes_validation["semanticHashSha256"] is None and [item["code"] for item in oversized_nodes_validation["diagnostics"]] == [graph_limit_goldens[0]["code"]] and oversized_nodes_compile_diagnostic == graph_limit_goldens[0],
            "graphLinkLimitRejectsBeforeLinkIndexing": graph_limits == {"maxNodes": dynamic_module.MAX_GRAPH_NODES, "maxLinks": dynamic_module.MAX_GRAPH_LINKS, "evidenceStatus": "provisional-unprofiled"} and oversized_links_validation["status"] == "invalid" and oversized_links_validation["semanticHashSha256"] is None and [item["code"] for item in oversized_links_validation["diagnostics"]] == [graph_limit_goldens[1]["code"]] and oversized_links_compile_diagnostic == graph_limit_goldens[1],
            "graphCollectionLimitsAggregateAndSortDiagnostics": [item["code"] for item in oversized_both_validation["diagnostics"]] == [item["code"] for item in dynamic_contract.get("sourceGuardGoldens", {}).get("bothGraphLimitDiagnostics", [])] and [item["path"] for item in oversized_both_validation["diagnostics"]] == ["/links", "/nodes"] and oversized_both_validation["semanticHashSha256"] is None,
            "uiSessionRejectsGraphGrowthPastLimitAtomically": ui_limit_add_diagnostic == dynamic_contract.get("sourceGuardGoldens", {}).get("uiGrowthLimitDiagnostic") and len(ui_limit_session.document["nodes"]) == dynamic_module.MAX_GRAPH_NODES and ui_limit_session.current_document_hash == ui_limit_before_hash and ui_limit_session.history == [] and ui_limit_session.cursor == 0,
            "uiSessionCanShrinkOversizedGraphForRepair": len(ui_repair_session.document["nodes"]) == dynamic_module.MAX_GRAPH_NODES and ui_repair_session.history and ui_repair_session.cursor == 1 and all(node.get("nodeId") != "loaded_over_limit_node" for node in ui_repair_session.document["nodes"]),
            "oversizedDocumentSaveReloadPreservesPayloadForRepair": oversized_save_reload_preserved,
            "registryRejectsScalarTypeConfusion": registry_diagnostics[3:] == dynamic_contract.get("sourceGuardGoldens", {}).get("registryScalarTypeDiagnostics"),
            "registryFileRejectsNestedValuesBeyondLimit": deep_registry_diagnostic is not None and deep_registry_diagnostic["code"] == dynamic_contract.get("sourceGuardGoldens", {}).get("registryDepthDiagnostic", {}).get("code") and deep_registry_diagnostic["path"].startswith(dynamic_contract.get("sourceGuardGoldens", {}).get("registryDepthDiagnostic", {}).get("pathPrefix", "/unconfigured")),
            "malformedOperationVersionsStaySafeAtEditBoundary": malformed_version_edit_diagnostics == [dynamic_contract.get("sourceGuardGoldens", {}).get("malformedOperationVersionEditDiagnostic")] * 3 and malformed_version_validation_codes == [["node.operation.version.invalid"]] * 3 and malformed_version_documents[0]["nodes"][0]["semanticVersion"] is True and malformed_version_documents[1]["nodes"][0]["semanticVersion"] == [] and malformed_version_documents[2]["nodes"][0]["semanticVersion"] == {} and all(document["nodes"][0]["parameters"] == {"value": 0.25} for document in malformed_version_documents),
        }
        nested_legacy_projection = legacy_projection_candidate.get("nestedAddProjection", {})
        nested_legacy_compiled = dynamic_module.compile_document_from_data(
            nested_legacy_projection.get("document", {}), dynamic_registry)
        scalar_legacy_projection = legacy_projection_candidate.get("scalarBaseColorConversionProbe", {})
        scalar_legacy_compiled = dynamic_module.compile_document_from_data(
            scalar_legacy_projection.get("document", {}), dynamic_registry)
        default_legacy_projection = legacy_projection_candidate.get("unconnectedBaseColorDefaultProbe", {})
        default_legacy_compiled = dynamic_module.compile_document_from_data(
            default_legacy_projection.get("document", {}), dynamic_registry)
        checks["legacyProjectionCoversNestedAddScalarConversionAndDefault"] = (
            nested_legacy_projection.get("candidateHlslBody") == nested_legacy_compiled.get("hlslBody") and
            nested_legacy_projection.get("candidateHlslBodySha256") == nested_legacy_compiled.get("hlslBodySha256") and
            nested_legacy_projection.get("includedNodeIds") == ["input_a", "input_b", "math", "input_c", "math_outer"] and
            nested_legacy_projection.get("cpuVector4AddGolden") == [0.625, -0.75, -0.5, 0.5] and
            "float4 n_math_outer_result = (n_math_result + n_input_c_value);" in nested_legacy_compiled.get("hlslBody", "") and
            scalar_legacy_projection.get("candidateHlslBody") == scalar_legacy_compiled.get("hlslBody") and
            scalar_legacy_projection.get("candidateHlslBodySha256") == scalar_legacy_compiled.get("hlslBodySha256") and
            scalar_legacy_projection.get("conversionOperationId") == "artifact.convert.scalar.replicate.vector4.rgb_alpha_one" and
            legacy_projection_candidate.get("negativeProbe", {}).get("generatedConversionNodeCollision") == "legacy.projection.generated_id.collision" and
            scalar_legacy_projection.get("document") == scalar_legacy_projection_from_source.get("document") and
            scalar_legacy_projection.get("sourceSha256") == scalar_legacy_projection_from_source.get("sourceSha256") and
            scalar_legacy_compiled.get("hlslBody") == scalar_legacy_compiled_from_source.get("hlslBody") and
            scalar_legacy_projection.get("cpuVector4Golden") == [0.375, 0.375, 0.375, 1.0] and
            "float4 n_legacy_BaseColor_replicate_vector = float4(n_scalar_value, n_scalar_value, n_scalar_value, 1.0f);" in scalar_legacy_compiled.get("hlslBody", "") and
            'return std::format("float4({0},{0},{0},1.0f)", v);' in legacy_core_shader_node_source and
            "expression = f\"float4({value}, {value}, {value}, 1.0f)\"" in dynamic_module_source and
            default_legacy_projection.get("document") == default_legacy_projection_from_source.get("document") and
            default_legacy_projection.get("sourceSha256") == default_legacy_projection_from_source.get("sourceSha256") and
            default_legacy_compiled.get("hlslBody") == default_legacy_compiled_from_source.get("hlslBody") and
            default_legacy_projection.get("includedNodeIds") == [] and
            legacy_projection_candidate.get("negativeProbe", {}).get("generatedDefaultNodeCollision") == "legacy.projection.generated_id.collision" and
            default_legacy_projection.get("cpuVector4Golden") == [0.8, 0.8, 0.8, 1.0] and
            '"float4(0.8f,0.8f,0.8f,1.0f)"' in legacy_core_shader_node_source and
            "float4 n_legacy_BaseColor_default_value = float4(0.800000012f, 0.800000012f, 0.800000012f, 1.0f);" in default_legacy_compiled.get("hlslBody", "")
        )
        if (dynamic_contract.get("status") != "source-only-prototype-no-Core-integration" or
                dynamic_candidate != regenerated_candidate or
                dynamic_subtract_candidate != regenerated_subtract_candidate or
                dynamic_divide_candidate != regenerated_divide_candidate or
                dynamic_vector4_divide_candidate != regenerated_vector4_divide_candidate or
                dynamic_vector4_subtract_candidate != regenerated_vector4_subtract_candidate or
                dynamic_vector4_invert_candidate != regenerated_vector4_invert_candidate or
                dynamic_clamp_candidate != regenerated_clamp_candidate or
                dynamic_vector4_mix_candidate != regenerated_vector4_mix_candidate or
                dynamic_vector4_separate_candidate != regenerated_vector4_separate_candidate or
                dynamic_vector4_ramp_candidate != regenerated_vector4_ramp_candidate or
                dynamic_vector2_uv_candidate != regenerated_vector2_uv_candidate or
                dynamic_vector2_mapping_candidate != regenerated_vector2_mapping_candidate or
                dynamic_base_color_texture_candidate != regenerated_base_color_texture_candidate or
                dynamic_mapped_base_color_texture_candidate != regenerated_mapped_base_color_texture_candidate or
                dynamic_texture_channels_candidate != regenerated_texture_channels_candidate or
                hashlib.sha256(dynamic_registry_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("registrySha256") or
                hashlib.sha256(dynamic_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("documentSha256") or
                hashlib.sha256(dynamic_subtract_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("subtractDocumentSha256") or
                hashlib.sha256(dynamic_divide_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("divideDocumentSha256") or
                hashlib.sha256(dynamic_vector4_divide_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4DivideDocumentSha256") or
                hashlib.sha256(dynamic_vector4_subtract_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4SubtractDocumentSha256") or
                hashlib.sha256(dynamic_vector4_invert_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4InvertDocumentSha256") or
                hashlib.sha256(dynamic_clamp_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("clampDocumentSha256") or
                hashlib.sha256(dynamic_vector4_mix_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4MixDocumentSha256") or
                hashlib.sha256(dynamic_vector4_separate_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4SeparateDocumentSha256") or
                hashlib.sha256(dynamic_vector4_ramp_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4RampDocumentSha256") or
                hashlib.sha256(dynamic_vector2_uv_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector2UvDocumentSha256") or
                hashlib.sha256(dynamic_vector2_mapping_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector2MappingDocumentSha256") or
                hashlib.sha256(dynamic_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("candidateSha256") or
                hashlib.sha256(dynamic_vector3_constant_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector3ConstantDocumentSha256") or
                hashlib.sha256(dynamic_vector3_constant_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector3ConstantCandidateSha256") or
                hashlib.sha256(dynamic_vector3_arithmetic_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector3ArithmeticDocumentSha256") or
                hashlib.sha256(dynamic_vector3_arithmetic_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector3ArithmeticCandidateSha256") or
                hashlib.sha256(dynamic_subtract_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("subtractCandidateSha256") or
                hashlib.sha256(dynamic_divide_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("divideCandidateSha256") or
                hashlib.sha256(dynamic_vector4_divide_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4DivideCandidateSha256") or
                hashlib.sha256(dynamic_vector4_subtract_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4SubtractCandidateSha256") or
                hashlib.sha256(dynamic_vector4_invert_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4InvertCandidateSha256") or
                hashlib.sha256(dynamic_clamp_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("clampCandidateSha256") or
                hashlib.sha256(dynamic_vector4_mix_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4MixCandidateSha256") or
                hashlib.sha256(dynamic_vector4_separate_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4SeparateCandidateSha256") or
                hashlib.sha256(dynamic_vector4_ramp_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector4RampCandidateSha256") or
                hashlib.sha256(dynamic_vector2_uv_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector2UvCandidateSha256") or
                hashlib.sha256(dynamic_vector2_mapping_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("vector2MappingCandidateSha256") or
                hashlib.sha256(dynamic_base_color_texture_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("baseColorTextureDocumentSha256") or
                hashlib.sha256(dynamic_mapped_base_color_texture_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("mappedBaseColorTextureDocumentSha256") or
                hashlib.sha256(dynamic_base_color_texture_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("baseColorTextureCandidateSha256") or
                hashlib.sha256(dynamic_mapped_base_color_texture_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("mappedBaseColorTextureCandidateSha256") or
                hashlib.sha256(dynamic_texture_channels_document_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("textureChannelsDocumentSha256") or
                hashlib.sha256(dynamic_texture_channels_candidate_path.read_bytes()).hexdigest().upper() != dynamic_contract.get("textureChannelsCandidateSha256") or
                regenerated_candidate.get("semanticHashSha256") != dynamic_contract.get("semanticHashSha256") or
                hashlib.sha256(regenerated_candidate.get("hlslBody", "").encode("utf-8")).hexdigest().upper() != dynamic_contract.get("hlslSha256") or
                dynamic_contract.get("supportedImplementationIds") != ["float.constant", "float.uv0.x", "vector2.uv0", "vector2.mapping.scale_offset", "vector3.constant", "vector3.add", "vector3.subtract", "vector3.multiply", "vector4.constant", "float.add", "float.subtract", "float.multiply", "float.divide.positive_floor_1e5", "vector4.add", "vector4.subtract", "vector4.multiply", "vector4.divide.positive_floor_1e5", "vector4.invert.linear_factor", "float.clamp.sorted_bounds", "float.replicate.rgb_alpha_one", "vector4.extract_x", "vector4.mix.saturated_factor", "vector4.mixrgb.add.saturated_factor", "vector4.mixrgb.multiply.saturated_factor", "vector4.mixrgb.screen.saturated_factor", "vector4.mixrgb.overlay.saturated_factor", "vector4.mixrgb.darken.saturated_factor", "vector4.mixrgb.lighten.saturated_factor", "vector4.mixrgb.difference.saturated_factor", "vector4.separate_channels", "vector4.ramp.linear_stops", "texture.sample.base_color", "texture.sample.opacity", "texture.sample.emission", "texture.sample.metallic_roughness", "texture.sample.normal", "texture.sample.occlusion", "texture.sample.image2d"] or
                dynamic_contract.get("contractChecks") != list(checks) or
                not all(checks.values()) or
                diagnostics != dynamic_contract.get("diagnosticGoldens") or
                not any(operation.get("operationId") == "artifact.rd.constant.float32" and operation.get("displayName") == "Float" for operation in dynamic_registry_source.get("operations", [])) or
                any("HLSL" in operation or "source" in operation or "code" in operation for operation in dynamic_registry_source.get("operations", [])) or
                regenerated_candidate.get("status") != "candidate-lowered-no-shader-runtime" or
                "no shader compiler/backend/runtime" not in " ".join(regenerated_candidate.get("limitations", []))):
            failed_dynamic_checks = [name for name, passed in checks.items() if not passed]
            errors.append("dynamic graph prototype evidence must reproduce the data-only registry, typed validation, deterministic active lowering, and fail-closed probes without claiming runtime support; failed checks: " + ", ".join(failed_dynamic_checks) + "; parameter uniforms stable=" + str(graph_parameter_uniforms_stable) + "; parameter block layout valid=" + str(parameter_block_layout_valid) + "; manifest checks match: " + str(dynamic_contract.get("contractChecks") == list(checks)) + "; diagnostic goldens match: " + str(diagnostics == dynamic_contract.get("diagnosticGoldens")) + "; operation IDs match: " + str(dynamic_contract.get("supportedImplementationIds") == ["float.constant", "float.uv0.x", "vector2.uv0", "vector2.mapping.scale_offset", "vector3.constant", "vector3.add", "vector3.subtract", "vector3.multiply", "vector4.constant", "float.add", "float.subtract", "float.multiply", "float.divide.positive_floor_1e5", "vector4.add", "vector4.subtract", "vector4.multiply", "vector4.divide.positive_floor_1e5", "vector4.invert.linear_factor", "float.clamp.sorted_bounds", "float.replicate.rgb_alpha_one", "vector4.extract_x", "vector4.mix.saturated_factor", "vector4.mixrgb.add.saturated_factor", "vector4.mixrgb.multiply.saturated_factor", "vector4.mixrgb.screen.saturated_factor", "vector4.mixrgb.overlay.saturated_factor", "vector4.mixrgb.darken.saturated_factor", "vector4.mixrgb.lighten.saturated_factor", "vector4.mixrgb.difference.saturated_factor", "vector4.separate_channels", "vector4.ramp.linear_stops", "texture.sample.base_color", "texture.sample.opacity", "texture.sample.emission", "texture.sample.metallic_roughness", "texture.sample.normal", "texture.sample.occlusion", "texture.sample.image2d"]))
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error, RuntimeError, KeyError) as error:
        errors.append(f"cannot validate dynamic graph prototype evidence: {error}")
    promotion_waves = operation_schema.get("operationPromotionWaves", [])
    wave_ids = [item.get("wave") for item in promotion_waves if isinstance(item, dict)]
    promoted_operation_ids = [operation_id for item in promotion_waves if isinstance(item, dict) for operation_id in item.get("operationIds", [])]
    if (wave_ids != list(range(5)) or len(promoted_operation_ids) != len(set(promoted_operation_ids)) or
            set(promoted_operation_ids) != set(operation_ids) or
            any(not item.get("name") or not item.get("exitEvidence") or not item.get("status") for item in promotion_waves if isinstance(item, dict)) or
            promotion_waves[0].get("operationIds") != [] or
            promotion_waves[0].get("status") != "not-started-blocked-by-runtime-compatibility-and-design-review"):
        errors.append("operation promotion waves must be ordered, cover each registry record once, and keep Document/compatibility admission first")
    registry_audit_contract = operation_schema.get("operationRegistryAudit", {})
    registry_audit_path = root / registry_audit_contract.get("report", "")
    registry_audit_generator_path = root / registry_audit_contract.get("generator", "")
    try:
        registry_audit_report = json.loads(registry_audit_path.read_text(encoding="utf-8"))
        registry_spec = importlib.util.spec_from_file_location("shader_graph_operation_registry_audit", registry_audit_generator_path)
        if registry_spec is None or registry_spec.loader is None:
            raise RuntimeError("cannot load operation registry audit generator")
        registry_module = importlib.util.module_from_spec(registry_spec)
        registry_spec.loader.exec_module(registry_module)
        regenerated_registry_audit = registry_module.build_report(root)
        if (registry_audit_contract.get("runtimeStatus") != "manifest-derived-proposal-audit-no-core-registry" or
                registry_audit_report != regenerated_registry_audit or
                registry_audit_report.get("kind") != "artifact-shader-graph-operation-registry-audit" or
                registry_audit_report.get("status") != "manifest-derived-proposal-audit-no-core-registry" or
                registry_audit_report.get("operationCount") != len(operation_records) or
                registry_audit_report.get("uniqueRegistryKeyCount") != len(operation_records) or
                registry_audit_report.get("duplicateRegistryKeys") or
                registry_audit_report.get("legacyPinContractAudit", {}).get("status") != "manifest-cross-check-only-no-runtime-registry" or
                registry_audit_report.get("legacyPinContractAudit", {}).get("familiesWithSourcePins", 0) < registry_audit_report.get("legacyPinContractAudit", {}).get("familiesWithDeclaredPins", 0) or
                any(family.get("runtimeSupportClaim", True) for family in registry_audit_report.get("legacyPinContractAudit", {}).get("families", [])) or
                not isinstance(registry_audit_report.get("legacyPinContractAudit", {}).get("mismatchedOrUnidentifiedPins"), list) or
                registry_audit_report.get("legacyPinContractAudit", {}).get("mismatchedOrUnidentifiedPins") != [] or
                registry_audit_report.get("legacyPinContractAudit", {}).get("sourceFamiliesMissingCoverageRows") != [] or
                registry_audit_report.get("legacyPinContractAudit", {}).get("operationSocketBindingErrors") != [] or
                len(registry_audit_report.get("legacyPinContractAudit", {}).get("operationSocketBindings", [])) != sum(len(item.get("operationIds", [])) for item in operation_schema.get("legacyNodeCoverage", {}).get("families", {}).values()) or
                any(family.get("uncharacterizedSourcePins") or not family.get("completeDeclaredPinCoverage") for family in registry_audit_report.get("legacyPinContractAudit", {}).get("families", [])) or
                any(not isinstance(family.get("sourcePins"), dict) or not isinstance(family.get("uncharacterizedSourcePins"), list) for family in registry_audit_report.get("legacyPinContractAudit", {}).get("families", [])) or
                registry_audit_report.get("manifestSha256") != hashlib.sha256(manifest_path.read_bytes()).hexdigest().upper()):
            errors.append("operation registry audit report must reproduce the proposal catalog without duplicate IDs/versions")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError) as error:
        errors.append(f"cannot validate operation registry audit report: {error}")
    constant_golden_contract = operation_schema.get("constantValueCanonicalizationGolden", {})
    constant_golden_path = root / constant_golden_contract.get("fixture", "")
    constant_golden_generator_path = root / constant_golden_contract.get("generator", "")
    try:
        constant_golden = json.loads(constant_golden_path.read_text(encoding="utf-8"))
        constant_spec = importlib.util.spec_from_file_location("shader_graph_constant_value_golden", constant_golden_generator_path)
        if constant_spec is None or constant_spec.loader is None:
            raise RuntimeError("cannot load constant canonicalization oracle")
        constant_module = importlib.util.module_from_spec(constant_spec)
        constant_spec.loader.exec_module(constant_module)
        regenerated_constant_golden = constant_module.build_golden(root)
        scalar_cases = constant_golden.get("scalarCases", [])
        rejection_cases = constant_golden.get("rejectionCases", [])
        typed_tuples = constant_golden.get("typedTupleCases", {})
        if (constant_golden_contract.get("status") != "candidate-binary32-canonicalization-no-Core-runtime" or
                constant_golden != regenerated_constant_golden or
                constant_golden.get("kind") != "artifact-shader-graph-constant-value-golden" or
                len(scalar_cases) != 6 or not all(case.get("matchesExpected") is True and case.get("actual") == case.get("expected") and case.get("actualBytes") == case.get("bytes") for case in scalar_cases) or
                len(rejection_cases) != 3 or
                typed_tuples.get("color", {}).get("valueType") != "Color4" or
                typed_tuples.get("rawVector", {}).get("valueType") != "Vector4" or
                typed_tuples.get("sameNumericBytesDoNotImplySameSemanticType") is not True):
            errors.append("constant canonicalization golden must reproduce finite binary32/signed-zero cases and keep Color4 distinct from Vector4")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError, ValueError, OverflowError, struct.error) as error:
        errors.append(f"cannot validate constant canonicalization golden: {error}")
    readiness = operation_schema.get("fixtureProjectionReadiness", {})
    readiness_report_path = root / readiness.get("report", "")
    readiness_generator_path = root / readiness.get("generator", "")
    try:
        readiness_report = json.loads(readiness_report_path.read_text(encoding="utf-8"))
        readiness_spec = importlib.util.spec_from_file_location("shader_graph_typed_ir_readiness_generator", readiness_generator_path)
        readiness_module = importlib.util.module_from_spec(readiness_spec)
        readiness_spec.loader.exec_module(readiness_module)
        regenerated_readiness = readiness_module.build_report(root)
        readiness_pin_source = (root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp").read_text(encoding="utf-8")
        readiness_pin_errors = []
        for legacy_kind, socket_map in readiness_module.LEGACY_SOCKET_MAPS.items():
            for socket, pin_type in socket_map.get("inputTypes", {}).items():
                if f'addInput("{socket}", PinType::{pin_type});' not in readiness_pin_source:
                    readiness_pin_errors.append(f"{legacy_kind}.{socket} input PinType::{pin_type}")
            for socket, pin_type in socket_map.get("outputTypes", {}).items():
                if f'addOutput("{socket}", PinType::{pin_type});' not in readiness_pin_source:
                    readiness_pin_errors.append(f"{legacy_kind}.{socket} output PinType::{pin_type}")
        for socket, pin_type in readiness_module.LEGACY_BOUNDARY_TYPES["MaterialOutput"]["inputs"].items():
            if f'addInput("{socket}", PinType::{pin_type});' not in readiness_pin_source:
                readiness_pin_errors.append(f"MaterialOutput.{socket} input PinType::{pin_type}")
        if (not readiness_report_path.is_file() or not readiness_generator_path.is_file() or
                hashlib.sha256(readiness_report_path.read_bytes()).hexdigest().upper() != readiness.get("sha256", "").upper() or
                readiness_report != regenerated_readiness or
                readiness_report.get("kind") != "artifact-typed-ir-fixture-readiness-report" or
                readiness_report.get("status") != "fixture-driven-candidate-projection-created-no-core-ir" or
                any(report.get("irProduced") is not False for report in readiness_report.get("fixtures", [])) or
                any(report.get("rootBoundary", {}).get("classification") != "selected-output-boundary-not-a-typed-operation" for report in readiness_report.get("fixtures", [])) or
                readiness_pin_errors or
                readiness.get("status") != "three-positive-fixtures-project; mismatched-type and multi-link fixtures are blocked without partial projection; no Core IR is implemented"):
            errors.append("fixture-driven typed IR readiness report must match its generator, source hash, and no-partial-IR status")
        for report in readiness_report.get("fixtures", []):
            if report.get("status") == "candidate-projection-unverified":
                boundary = report.get("rootBoundary", {})
                candidate_ir = report.get("candidateTypedIrProjection")
                expected_stability = {
                    "reverse-node-and-link-storage-order": True,
                    "change-layout-only": True,
                    "change-stable-node-id": False,
                }
                stability = {case.get("id"): case.get("matchesBaseline") for case in report.get("stabilityCases", [])}
                stability_expectations = {case.get("id"): case.get("expectedMatchesBaseline") for case in report.get("stabilityCases", [])}
                if len(boundary.get("surfaceInputLinks", [])) != 1 or report.get("unsupportedNodes") or report.get("diagnosticCodes") or not isinstance(candidate_ir, dict):
                    errors.append(f"candidate projection must resolve one selected Surface root and all active operation records: {report.get('fixture')}")
                if stability != expected_stability or stability_expectations != expected_stability:
                    errors.append(f"candidate projection storage/layout/identity metamorphic cases differ from contract: {report.get('fixture')}")
                if candidate_ir:
                    digest = candidate_ir.get("candidateSerializationSha256")
                    digest_input = dict(candidate_ir)
                    digest_input.pop("candidateSerializationSha256", None)
                    canonical_candidate = json.dumps(digest_input, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
                    operation_nodes = [item.get("nodeId") for item in candidate_ir.get("operations", [])]
                    if (candidate_ir.get("kind") != "artifact-typed-ir-projection-candidate" or
                            candidate_ir.get("status") != "proposal-fixture-projection-not-core-ir" or
                            hashlib.sha256(canonical_candidate.encode("utf-8")).hexdigest().upper() != digest or
                            set(operation_nodes) != {item.get("nodeId") for item in report.get("candidateOperations", [])} or len(operation_nodes) != len(set(operation_nodes)) or
                            candidate_ir.get("root", {}).get("valueType") != "SurfaceFields" or
                            candidate_ir.get("root", {}).get("sourceLegacyPinType") != candidate_ir.get("root", {}).get("boundaryInputLegacyPinType") or
                            candidate_ir.get("root", {}).get("typeAdapter") == "unresolved-type-adapter"):
                        errors.append(f"candidate projection serialization, operation order, or root differs from its declared candidate: {report.get('fixture')}")
                    operation_order = {node_id: index for index, node_id in enumerate(operation_nodes)}
                    for operation in candidate_ir.get("operations", []):
                        if any(not output.get("legacyPinType") or output.get("typeAdapter") == "unresolved-type-adapter" for output in operation.get("outputs", [])):
                            errors.append(f"candidate projection operation has unresolved legacy output pin type: {report.get('fixture')}:{operation.get('nodeId')}")
                        for typed_input in operation.get("inputs", []):
                            edge = typed_input.get("binding", {})
                            source_id = edge.get("sourceNodeId") if edge.get("kind") == "edge" else None
                            if source_id is not None and operation_order.get(source_id, len(operation_order)) >= operation_order.get(operation.get("nodeId"), -1):
                                errors.append(f"candidate projection is not in source-before-consumer order: {report.get('fixture')}:{operation.get('nodeId')}")
                            if source_id is not None and edge.get("sourceLegacyPinType") != typed_input.get("legacyPinType"):
                                errors.append(f"candidate edge legacy pin types differ: {report.get('fixture')}:{operation.get('nodeId')}.{typed_input.get('inputId')}")
                for candidate in report.get("candidateOperations", []):
                    record = next((item for item in operation_records if item.get("operationId") == candidate.get("operationId")), {})
                    if ([item.get("inputId") for item in candidate.get("inputs", [])] != [item.get("inputId") for item in record.get("inputs", [])] or
                            [item.get("outputId") for item in candidate.get("outputs", [])] != [item.get("outputId") for item in record.get("outputs", [])] or
                            any(not item.get("legacySocket") or not item.get("legacyPinType") or item.get("typeAdapter") == "unresolved-type-adapter" for item in candidate.get("inputs", []) + candidate.get("outputs")) or
                            any(item.get("binding", {}).get("status") == "ambiguous-multiple-source-links" for item in candidate.get("inputs", []))):
                        errors.append(f"fixture projection does not map declared sockets unambiguously: {report.get('fixture')}:{candidate.get('nodeId')}")
            elif report.get("status") == "blocked-no-partial-ir":
                if report.get("candidateTypedIrProjection") is not None or not report.get("projectionDiagnostics"):
                    errors.append(f"blocked fixture must withhold its entire candidate projection: {report.get('fixture')}")
        expected_blocked_projection_cases = {
            "skipped_type_mismatch_link_negative.json": "candidate edge type mismatch Float -> Color4",
            "duplicate_input_negative.json": "ambiguous-multiple-source-links",
        }
        for fixture_name, expected_fragment in expected_blocked_projection_cases.items():
            report = next((item for item in readiness_report.get("fixtures", []) if item.get("fixture") == fixture_name), {})
            diagnostic_text = " ".join(report.get("projectionDiagnostics", []))
            if (report.get("status") != "blocked-no-partial-ir" or
                    report.get("candidateTypedIrProjection") is not None or
                    expected_fragment not in diagnostic_text):
                errors.append(f"candidate projection must reject the expected invalid link case without partial output: {fixture_name}")
        explicit_case = readiness_report.get("explicitConversionExpression", {})
        explicit_case_contract = operation_schema.get("explicitConversionExpression", {})
        expression_path = root / explicit_case_contract.get("fixture", "")
        source_legacy_path = fixture_root / "skipped_type_mismatch_link_negative.json"
        expression_projection = explicit_case.get("candidateExpression")
        expression_digest_input = dict(expression_projection) if isinstance(expression_projection, dict) else {}
        expression_digest = expression_digest_input.pop("candidateSerializationSha256", None)
        canonical_expression = json.dumps(expression_digest_input, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        if (not expression_path.is_file() or
                explicit_case.get("fixture") != expression_path.relative_to(root).as_posix() or
                explicit_case.get("fixtureSha256") != hashlib.sha256(expression_path.read_bytes()).hexdigest().upper() or
                explicit_case.get("status") != "explicit-conversion-candidate-valid-no-core-ir" or
                explicit_case.get("irProduced") is not False or
                explicit_case.get("operationId") != "artifact.convert.scalar.replicate.rgb_alpha_one" or
                explicit_case.get("from") != "Float" or explicit_case.get("to") != "Vector4" or
                explicit_case.get("computedOutput") != [0.25, 0.25, 0.25, 1.0] or
                explicit_case.get("computedOutput") != explicit_case.get("expectedOutput") or
                explicit_case.get("expectedOutput") != float_to_vector4.get("expectedSemanticValue") or
                explicit_case.get("sourceLegacyFixtureSha256") != hashlib.sha256(source_legacy_path.read_bytes()).hexdigest().upper() or
                not isinstance(expression_projection, dict) or
                hashlib.sha256(canonical_expression.encode("utf-8")).hexdigest().upper() != expression_digest or
                [item.get("operationId") for item in expression_projection.get("operations", [])] != ["artifact.constant.scalar.float32", "artifact.convert.scalar.replicate.rgb_alpha_one"] or
                expression_projection.get("root", {}).get("valueType") != "Vector4" or
                explicit_case_contract.get("fixtureSha256") != hashlib.sha256(expression_path.read_bytes()).hexdigest().upper() or
                explicit_case_contract.get("generator") != "tools/shader_graph_rd/generate_typed_ir_readiness.py" or
                explicit_case_contract.get("runtimeStatus") != "candidate-cpu-formula-only-no-core-lowering"):
            errors.append("explicit Float-to-Vector4 conversion expression must resolve the declared op and CPU formula without claiming Core IR")
        truncation_case = readiness_report.get("vectorTruncationExpression", {})
        truncation_contract = operation_schema.get("vectorTruncationExpression", {})
        truncation_path = root / truncation_contract.get("fixture", "")
        truncation_projection = truncation_case.get("candidateExpression")
        truncation_projection_body = dict(truncation_projection) if isinstance(truncation_projection, dict) else {}
        truncation_digest = truncation_projection_body.pop("candidateSerializationSha256", None)
        canonical_truncation = json.dumps(truncation_projection_body, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
        if (not truncation_path.is_file() or
                truncation_case.get("fixture") != truncation_path.relative_to(root).as_posix() or
                truncation_case.get("fixtureSha256") != hashlib.sha256(truncation_path.read_bytes()).hexdigest().upper() or
                truncation_contract.get("fixtureSha256") != hashlib.sha256(truncation_path.read_bytes()).hexdigest().upper() or
                truncation_case.get("operationId") != "artifact.convert.vector4.truncate_xyz" or
                truncation_case.get("from") != "Vector4" or truncation_case.get("to") != "Vector3" or
                truncation_case.get("status") != "explicit-conversion-candidate-valid-no-core-ir" or
                truncation_case.get("computedOutput") != [0.25, -0.5, 0.75] or
                truncation_case.get("computedOutput") != truncation_case.get("expectedOutput") or
                truncation_case.get("expectedOutput") != next((item.get("expectedSemanticValue") for item in conversion_matrix if item.get("operationId") == "artifact.convert.vector4.truncate_xyz"), None) or
                not isinstance(truncation_projection, dict) or
                hashlib.sha256(canonical_truncation.encode("utf-8")).hexdigest().upper() != truncation_digest or
                [item.get("operationId") for item in truncation_projection.get("operations", [])] != ["artifact.constant.vector4.float32", "artifact.convert.vector4.truncate_xyz"] or
                truncation_projection.get("root", {}).get("valueType") != "Vector3" or
                truncation_contract.get("generator") != "tools/shader_graph_rd/generate_typed_ir_readiness.py" or
                truncation_contract.get("runtimeStatus") != "candidate-cpu-formula-only-no-core-lowering"):
            errors.append("explicit Vector4-to-Vector3 truncation expression must resolve typed records and CPU formula without claiming Core IR")
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError, ImportError, TypeError) as error:
        errors.append(f"cannot validate fixture-driven typed IR readiness report: {error}")
    disposition_contract = manifest.get("loweringValidationDispositionContract", {})
    expected_dispositions = {
        "unknownLegacyNodeKind": ("legacyImport", "node.kind.unknown", "rejected", "not-attempted"),
        "unknownCoreOperationId": ("documentValidation", "node.kind.unknown", "rejected", "not-attempted"),
        "knownOperationWithoutActiveLowerer": ("typedLowering", "lowering.operation.unsupported", "retained-valid", "no-partial-ir"),
        "multipleMaterialOutputs": ("documentValidation", "output.ambiguous", "rejected", "not-attempted"),
        "cycleAnywhereInDocument": ("documentValidation", "graph.cycle", "rejected", "not-attempted"),
        "unreachableKnownNode": ("activeGraphAnalysis", "graph.node.unreachable", "retained-valid", "complete-active-ir"),
        "semanticallyUnusedInputLink": ("activeGraphAnalysis", "link.input.unused", "retained-valid", "complete-active-ir"),
        "structurallyInvalidUnusedLink": ("documentValidation", "link.type.incompatible", "rejected", "not-attempted"),
    }
    disposition_records = disposition_contract.get("dispositions", {})
    if (disposition_contract.get("status") != "proposal-only-not-implemented" or
            disposition_contract.get("stages") != ["legacyImport", "documentValidation", "activeGraphAnalysis", "typedLowering", "rendererBackendBuild"] or
            set(disposition_records) != set(expected_dispositions) or
            any((record.get("stage"), record.get("diagnosticCode"), record.get("documentDisposition"), record.get("irDisposition")) != expected for case, expected in expected_dispositions.items() for record in [disposition_records.get(case, {})]) or
            disposition_contract.get("loweringDiagnosticCodes") != ["lowering.operation.unsupported"] or
            disposition_contract.get("warningDiagnosticCodes") != ["graph.node.unreachable", "link.input.unused"] or
            disposition_contract.get("atomicity") != "document validation either returns the whole validated Document or no Document; typed lowering either returns a complete active IR or no IR" or
            disposition_contract.get("runtimeStatus") != "source-audited-current-boundaries-plus-proposed-stage-policy-no-core-runtime-evidence"):
        errors.append("lowering validation disposition must separate document errors, analysis warnings, unsupported lowering, and renderer backend policy")
    identity_contract = manifest.get("loweringIdentityContract", {})
    current_identity = identity_contract.get("currentImplementation", {})
    identity_layers = identity_contract.get("proposedIdentityLayers", {})
    identity_source = math_source
    expected_identity_layer_names = {"documentSemanticHash", "typedIrSemanticHash", "emittedSourceHash", "shaderCompileKey", "resourceBindingKey", "pipelineKey"}
    expected_hash_descriptions = {
        "documentSemanticHash": "SHA-256 over versioned canonical semantic Document bytes",
        "typedIrSemanticHash": "SHA-256 over canonical typed IR record bytes",
        "emittedSourceHash": "SHA-256 over exact normalized source bytes after adapter lowering",
    }
    for layer_name, description in expected_hash_descriptions.items():
        layer = identity_layers.get(layer_name, {})
        if layer.get("algorithm") != description or not layer.get("include") or not layer.get("exclude"):
            errors.append(f"lowering identity hash layer lacks explicit algorithm/include/exclude fields: {layer_name}")
    active_ir_projection = identity_contract.get("activeIrProjection", {})
    input_classification = manifest.get("compilerInputClassification", {})
    typed_ir_include = identity_layers.get("typedIrSemanticHash", {}).get("include", [])
    typed_ir_exclude = identity_layers.get("typedIrSemanticHash", {}).get("exclude", [])
    required_order_keys = {"documentNodes", "documentLinks", "irOperations", "diagnostics", "sourceEmitter"}
    provenance = identity_contract.get("provenanceContract", {})
    if (current_identity.get("sourceHash") != "64-bit FNV-1a over helperHlsl followed by blockHlsl on success; failure hashes blockHlsl only" or
            current_identity.get("emittedSymbols") != "sanitized persisted node IDs are embedded in HLSL temporary names" or
            current_identity.get("order") != "first stored MaterialOutput is selected; output-rooted depth-first traversal visits every declared input in pin order and emits postorder" or
            set(identity_layers) != expected_identity_layer_names or
            not required_order_keys <= set(identity_contract.get("canonicalOrder", {})) or
            not provenance.get("mapping") or "excluded from typed IR semantic hash" not in provenance.get("hashPolicy", "") or
            "only operations reachable from the selected Material Output through semantically consumed typed input sockets" not in typed_ir_include or
            not {"unreachable editable nodes and their links", "links to declared inputs unused by that operation semantic version"} <= set(typed_ir_exclude) or
            active_ir_projection.get("documentRetention") != "preserve every valid node/link and layout in canonical editable Document regardless of reachability or semantic operand use" or
            active_ir_projection.get("activeOperations") != "walk backward from selected output through only the input socket IDs declared semantically consumed by each operationId/semanticVersion" or
            active_ir_projection.get("unusedInputLinks") != "retain in Document; emit a non-fatal stable link.input.unused warning; omit the edge and its source subgraph from active IR/source/resource requirements" or
            active_ir_projection.get("unreachableNodes") != "retain in Document; emit a non-fatal stable graph.node.unreachable warning; exclude from active IR/source/resource requirements" or
            active_ir_projection.get("wholeDocumentValidation") != "all nodes, links, parameters, IDs, and cycles remain subject to validation even when unreachable; unknown/invalid records do not become valid by being inactive" or
            active_ir_projection.get("warningCodes") != {"unusedInput": "link.input.unused", "unreachableNode": "graph.node.unreachable"} or
            active_ir_projection.get("diagnosticsAffectShaderKeys") is not False or
            active_ir_projection.get("evidenceStatus") != "proposal-only-current-source-traverses-unused-inputs-but-not-unreachable-nodes" or
            input_classification.get("unusedDeclaredInputs", {}).get("promotionContract") != active_ir_projection.get("unusedInputLinks") or
            input_classification.get("unreachableNodes", {}).get("promotionContract") != "preserve editable nodes in Document, emit stable non-fatal graph.node.unreachable warning, and exclude them from active IR/source/resource requirements without changing output semantics" or
            identity_contract.get("currentRuntimeStatus") != "source-audited-proposal-only-no-canonical-ir-or-separated-hash-implementation" or
            any(fragment not in identity_source for fragment in current_identity.get("sourceFragments", [])) or
            not current_identity.get("sourceFragments")):
        errors.append("lowering identity must separate current HLSL hash from proposed document/IR/source/backend/resource/pipeline identities")
    identity_golden_path = fixture_root / identity_contract.get("proposalGolden", "")
    try:
        identity_golden = json.loads(identity_golden_path.read_text(encoding="utf-8"))
        identity_oracle_path = root / "tools/shader_graph_rd/generate_lowering_identity_golden.py"
        identity_spec = importlib.util.spec_from_file_location("shader_graph_identity_golden_oracle", identity_oracle_path)
        if identity_spec is None or identity_spec.loader is None:
            raise RuntimeError("cannot load lowering identity proposal oracle")
        identity_oracle = importlib.util.module_from_spec(identity_spec)
        identity_spec.loader.exec_module(identity_oracle)
        expected_identity_golden = identity_oracle.make_golden()
        if identity_golden != expected_identity_golden:
            errors.append("lowering identity proposal golden differs from CPU projection oracle")
        if hashlib.sha256(identity_golden_path.read_bytes()).hexdigest().upper() != str(identity_contract.get("proposalGoldenSha256", "")).upper():
            errors.append("lowering identity proposal golden SHA-256 differs from manifest")
        if identity_golden.get("kind") != "artifact-shader-graph-lowering-identity-proposal-golden" or identity_golden.get("status") != "cpu-projection-model-only-not-core-or-renderer-evidence":
            errors.append("lowering identity proposal golden must remain explicitly non-runtime evidence")
        if [case.get("id") for case in identity_golden.get("scenarios", [])] != identity_contract.get("proposalGoldenScenarios") or any(case.get("expectedChangedKeys") != case.get("actualChangedKeys") for case in identity_golden.get("scenarios", [])):
            errors.append("lowering identity scenario inventory/change-key behavior differs from the declared contract")
    except (OSError, UnicodeError, json.JSONDecodeError, ImportError, AttributeError, TypeError, ValueError, RuntimeError) as error:
        errors.append(f"cannot verify lowering identity proposal golden: {error}")
    math_golden_name = math_contract.get("formulaGolden")
    math_golden_path = fixture_root / math_golden_name if isinstance(math_golden_name, str) else None
    math_golden = {}
    if not math_golden_path or not math_golden_path.is_file():
        errors.append("Math operation formula golden artifact is missing")
    else:
        try:
            math_golden = json.loads(math_golden_path.read_text(encoding="utf-8"))
            oracle_path = root / "tools/shader_graph_rd/generate_math_operation_golden.py"
            spec = importlib.util.spec_from_file_location("shader_graph_math_golden_oracle", oracle_path)
            if spec is None or spec.loader is None:
                raise RuntimeError("cannot load Math formula oracle")
            oracle = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(oracle)
            expected_math_golden = oracle.make_golden(
                root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp", fixture_root
            )
            if math_golden != expected_math_golden:
                errors.append("Math operation formula golden differs from the source-bound binary32 oracle")
            if hashlib.sha256(math_golden_path.read_bytes()).hexdigest().upper() != str(math_contract.get("formulaGoldenSha256", "")).upper():
                errors.append("Math operation formula golden SHA-256 differs from manifest")
            if math_golden.get("kind") != "artifact-shader-graph-math-formula-golden" or math_golden.get("status") != "candidate-core-math-only-not-runtime-or-cross-backend-evidence":
                errors.append("Math formula golden must remain candidate-only and explicitly unverified")
            if math_contract.get("formulaGoldenScope") != "finite explicitly listed probes only; Python CPU oracle does not specify HLSL NaN/Inf, signed-zero min/max, transcendental accuracy, lerp rounding, fast-math, or backend behavior":
                errors.append("Math formula golden scope must exclude uncharacterized GPU and exceptional-value behavior")
            golden_cases = {item.get("operation"): item for item in math_golden.get("cases", []) if isinstance(item, dict)}
            if set(golden_cases) != set(expected_math_ops.values()) or len(math_golden.get("cases", [])) != 17:
                errors.append("Math formula golden must include exactly one probe for each of the 17 operations")
            for ordinal, operation in expected_math_ops.items():
                case = golden_cases.get(operation, {})
                fixture_name = math_fixture_by_operation[operation]
                if case.get("fixture") != fixture_name:
                    errors.append(f"Math numeric case references wrong fixture: {operation}")
                    continue
                payload = fixture_payloads.get(fixture_name, {})
                nodes_by_id = {node.get("id"): node for node in payload.get("nodes", []) if isinstance(node, dict)}
                expected_inputs = case.get("inputs", {})
                for input_name, source_id in (("A", "input_a"), ("B", "input_b")):
                    if input_name not in expected_inputs:
                        continue
                    input_node = nodes_by_id.get(source_id, {})
                    rgba = expected_inputs[input_name]
                    expected_params = dict(zip(("r", "g", "b", "a"), rgba))
                    if input_node.get("type") != "RGB" or input_node.get("params") != expected_params:
                        errors.append(f"Math probe fixture input differs from formula golden: {fixture_name} {input_name}")
                if "Fac" in expected_inputs:
                    input_node = nodes_by_id.get("input_fac", {})
                    if input_node.get("type") != "Value" or input_node.get("params") != {"value": expected_inputs["Fac"]}:
                        errors.append(f"Math probe fixture Fac differs from formula golden: {fixture_name}")
                fixture_links = {(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in payload.get("links", []) if isinstance(link, dict)}
                expected_links = {("math", "Value", "surface", "BaseColor"), ("surface", "BSDF", "output", "Surface")}
                if "A" in expected_inputs: expected_links.add(("input_a", "Color", "math", "A"))
                if "B" in expected_inputs: expected_links.add(("input_b", "Color", "math", "B"))
                if "Fac" in expected_inputs: expected_links.add(("input_fac", "Value", "math", "Fac"))
                if fixture_links != expected_links:
                    errors.append(f"Math probe fixture links differ from formula input contract: {fixture_name}")
        except (OSError, UnicodeError, json.JSONDecodeError, ImportError, AttributeError, TypeError, ValueError, RuntimeError) as error:
            errors.append(f"cannot verify Math operation formula golden: {error}")
    procedural_source_path = root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp"
    try:
        procedural_source = procedural_source_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read procedural node source: {error}")
        procedural_source = ""
    for node_kind, contract in procedural_by_node.items():
        source_fragments = contract.get("helperSourceFragments", []) + contract.get("loweringSourceFragments", []) + contract.get("sourceFragments", [])
        for fragment in source_fragments:
            if fragment not in procedural_source:
                errors.append(f"procedural source contract drift for {node_kind}: {fragment}")
    procedural_test_source_path = root / "tests/ArtifactCore/ShaderGraphContractTest.cpp"
    try:
        procedural_test_source = procedural_test_source_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read procedural algorithm contract test source: {error}")
        procedural_test_source = ""
    for node_kind, contract in procedural_by_node.items():
        for fragment in contract.get("contractTestFragments", []):
            if fragment not in procedural_test_source:
                errors.append(f"procedural contract test evidence missing for {node_kind}: {fragment}")
        for fixture_name in contract.get("evaluationFixtures", []):
            if fixture_name not in declared:
                errors.append(f"procedural algorithm references undeclared evaluation fixture: {fixture_name}")
    expected_procedural_fixtures = {
        "Noise": {"noise_ramp_basecolor.json"},
        "Voronoi": {"voronoi_cells_surface.json"},
        "ColorRamp": {"noise_ramp_basecolor.json", "ramp_position_domain_characterization.json", "ramp_duplicate_stops_forward.json", "ramp_duplicate_stops_reversed.json"},
    }
    for node_kind, expected_names in expected_procedural_fixtures.items():
        contract = procedural_by_node.get(node_kind, {})
        if set(contract.get("evaluationFixtures", [])) != expected_names:
            errors.append(f"procedural evaluation fixture coverage is incomplete for {node_kind}")
    ramp_policy = procedural_by_node.get("ColorRamp", {}).get("equalPositionPolicyProposal", {})
    if (ramp_policy.get("orderedStopArrayIsSemantic") is not True or
            ramp_policy.get("stableSortKey") != ["position", "originalArrayIndex"] or
            ramp_policy.get("evaluationProbePositions") != ["0.0", "0.4999", "0.5", "0.5001", "1.0"] or
            ramp_policy.get("probeFields") != ["linearRgb", "alpha"] or
            ramp_policy.get("exactTieRule") != "first-authored-stop-at-exact-position; candidate policy requires product review" or
            ramp_policy.get("negativeDomainPolicy") != "reject-nonfinite-and-out-of-range-stop-positions-before-lowering"):
        errors.append("ColorRamp tie-order proposal must pin stable ordering, numeric probes, and unresolved exact-tie/domain decisions")
    ramp_image_probe = manifest.get("imageConditions", {}).get("requiredEvaluationMatrix", {}).get("effectCorrectness", {}).get("colorRampDuplicateStopProbe", {})
    if (ramp_image_probe.get("fixtures") != ["ramp_duplicate_stops_forward.json", "ramp_duplicate_stops_reversed.json"] or
            ramp_image_probe.get("factorValues") != [0.0, 0.4999, 0.5, 0.5001, 1.0] or
            ramp_image_probe.get("captureRoute") != "not-implemented" or
            ramp_image_probe.get("runtimeStatus") != "cpu-formula-golden-added-no-capture-source" or
            ramp_image_probe.get("expectedSourceColorsLinearRgba", {}).get("redStop") != [1.0, 0.0, 0.0, 0.25] or
            ramp_image_probe.get("expectedSourceColorsLinearRgba", {}).get("blueStop") != [0.0, 0.0, 1.0, 0.75]):
        errors.append("ColorRamp duplicate-stop image evaluation plan must retain its fixed samples and explicitly unimplemented runtime status")
    golden_name = ramp_image_probe.get("formulaGolden")
    golden_path = fixture_root / golden_name if isinstance(golden_name, str) else None
    if not golden_path or not golden_path.is_file():
        errors.append("ColorRamp formula golden artifact is missing")
    else:
        try:
            golden = json.loads(golden_path.read_text(encoding="utf-8"))
            expected_golden_hash = ramp_image_probe.get("formulaGoldenSha256")
            if hashlib.sha256(golden_path.read_bytes()).hexdigest().upper() != str(expected_golden_hash).upper():
                errors.append("ColorRamp formula golden SHA-256 differs from manifest")
            oracle_path = root / "tools/shader_graph_rd/generate_color_ramp_tie_golden.py"
            spec = importlib.util.spec_from_file_location("shader_graph_ramp_golden_oracle", oracle_path)
            if spec is None or spec.loader is None:
                raise RuntimeError("cannot load ColorRamp formula oracle")
            oracle = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(oracle)
            expected = oracle.make_golden(fixture_root)
            if golden != expected:
                errors.append("ColorRamp formula golden differs from independent binary32 oracle output")
            if golden.get("status") != "candidate-core-math-only-not-runtime-evidence":
                errors.append("ColorRamp formula golden must not claim runtime evidence")
            if golden.get("schemaVersion") != 1 or golden.get("kind") != "artifact-shader-graph-ramp-formula-golden":
                errors.append("ColorRamp formula golden identity/version is invalid")
            if golden.get("factorDomain") != "clamp to the minimum and maximum finite stop positions after stable sorting":
                errors.append("ColorRamp formula golden must specify sorted stop-domain clamping")
        except (OSError, UnicodeError, json.JSONDecodeError, ImportError, AttributeError, TypeError, ValueError, RuntimeError) as error:
            errors.append(f"cannot verify ColorRamp formula golden: {error}")
    ramp_execution = [item for item in manifest.get("imageConditions", {}).get("executionMatrix", []) if isinstance(item, dict) and item.get("id") == "color-ramp-duplicate-stop-numeric-contract"]
    if (len(ramp_execution) != 1 or ramp_execution[0].get("status") != "capture-source-added-runtime-unverified" or
            ramp_execution[0].get("route") != "Vulkan MeshRenderer RGBA32F capture source; source added, not run" or len(ramp_execution[0].get("knownGaps", [])) < 3):
        errors.append("ColorRamp duplicate-stop execution gate must identify the unrun Vulkan capture source and list its remaining runtime gaps")
    voronoi_fixture = fixture_payloads.get("voronoi_cells_surface.json", {})
    voronoi_nodes = {node.get("id"): node.get("type") for node in voronoi_fixture.get("nodes", []) if isinstance(node, dict)}
    voronoi_links = {(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in voronoi_fixture.get("links", []) if isinstance(link, dict)}
    expected_voronoi_nodes = {"uv": "TexCoord", "voronoi": "Voronoi", "surface": "Principled", "output": "MaterialOutput"}
    expected_voronoi_links = {
        ("uv", "UV", "voronoi", "Vector"),
        ("voronoi", "Color", "surface", "BaseColor"),
        ("voronoi", "Distance", "surface", "Roughness"),
        ("surface", "BSDF", "output", "Surface"),
    }
    if voronoi_nodes != expected_voronoi_nodes or voronoi_links != expected_voronoi_links:
        errors.append("Voronoi evaluation fixture must connect UV, cell color, distance, and Material Output with the declared topology")
    ramp_fixture = fixture_payloads.get("ramp_position_domain_characterization.json", {})
    ramp_nodes = {node.get("id"): node for node in ramp_fixture.get("nodes", []) if isinstance(node, dict)}
    ramp_payload = ramp_nodes.get("ramp", {})
    ramp_stops = ramp_payload.get("params", {}).get("stops", [])
    ramp_positions = [stop.get("pos") for stop in ramp_stops if isinstance(stop, dict)]
    if ramp_payload.get("type") != "ColorRamp" or ramp_payload.get("params", {}).get("interp") != 2 or ramp_positions != [-0.25, 1.25]:
        errors.append("ramp position characterization fixture must use Ease interpolation and positions outside both [0,1] endpoints")
    ramp_test_body = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*ColorRampFixtureClampsStopPositionsBeforeInterpolation\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", procedural_test_source, re.DOTALL)
    if not ramp_test_body or not all(fragment in ramp_test_body.group(1) for fragment in (
        "ramp_position_domain_characterization.json", "graph.fromJson(payload)",
        "float4 ramp_ramp = float4(0.1f,0.2f,0.3f,1.0f)",
        "saturate((ramp_t - 0.0f) / 1.0f)",
    )):
        errors.append("ramp domain test must load the boundary fixture and assert endpoint-clamped HLSL")
    tie_names = ("ramp_duplicate_stops_forward.json", "ramp_duplicate_stops_reversed.json")
    tie_payloads = [fixture_payloads.get(name, {}) for name in tie_names]
    tie_ramps = []
    for name, payload in zip(tie_names, tie_payloads):
        nodes = {node.get("id"): node for node in payload.get("nodes", []) if isinstance(node, dict)}
        ramp_node = nodes.get("ramp", {})
        stops = ramp_node.get("params", {}).get("stops", [])
        tie_ramps.append(stops)
        if ramp_node.get("type") != "ColorRamp" or ramp_node.get("params", {}).get("interp") != 0:
            errors.append(f"{name} must use a linear ColorRamp")
        if [stop.get("pos") for stop in stops if isinstance(stop, dict)] != [0.0, 0.5, 0.5, 1.0]:
            errors.append(f"{name} must contain two adjacent equal-position stops at 0.5")
        links = {(link.get("from"), link.get("out"), link.get("to"), link.get("in")) for link in payload.get("links", []) if isinstance(link, dict)}
        expected_links = {
            ("factor", "Value", "ramp", "Fac"),
            ("ramp", "Color", "surface", "BaseColor"),
            ("surface", "BSDF", "output", "Surface"),
        }
        if links != expected_links:
            errors.append(f"{name} must use the declared Value → ColorRamp → Principled topology")
    if len(tie_ramps) == 2 and tie_ramps[0] and tie_ramps[1]:
        forward_middle = tie_ramps[0][1:3]
        reverse_middle = tie_ramps[1][1:3]
        if forward_middle != list(reversed(reverse_middle)) or tie_ramps[0][0] != tie_ramps[1][0] or tie_ramps[0][3] != tie_ramps[1][3]:
            errors.append("duplicate-stop pair must differ only by reversing the equal-position stop order")
    tie_test_body = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*ColorRampDuplicateStopOrderRemainsSemanticInDocument\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", procedural_test_source, re.DOTALL)
    if not tie_test_body or not all(fragment in tie_test_body.group(1) for fragment in (
        *tie_names, "forwardGraph.fromJson", "reversedGraph.fromJson", "forwardGraph.toJson({})", "compileMaterialGraph()",
        "EXPECT_NE(forwardGraph.toJson({}), reversedGraph.toJson({}))",
        "Legacy std::sort tie ordering is unspecified",
    )):
        errors.append("duplicate-stop source case must preserve authored document order without asserting unspecified legacy tie lowering")
    voronoi_test_body = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*VoronoiFixtureConnectsCellColorAndDistance\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", procedural_test_source, re.DOTALL)
    if not voronoi_test_body or not all(fragment in voronoi_test_body.group(1) for fragment in (
        "voronoi_cells_surface.json", "graph.fromJson(payload)",
        "voronoi_Color = float4(voronoi_cell, 1.0f)",
        "float voronoi_Distance = 0.0f",
        "artMatGraph_voronoi(voronoi_p, voronoi_Distance, voronoi_cell)",
        "dist = sqrt(dist)",
    )):
        errors.append("Voronoi test must load the declared fixture and assert color/distance lowering")
    scalar_cases = core_schema.get("scalarCanonicalizationCases", [])
    observed_bits: set[str] = set()
    for case in scalar_cases:
        try:
            value = float(case["input"])
            if case["expected"] == "accept":
                canonical_value = 0.0 if value == 0.0 else value
                packed = struct.pack(">f", canonical_value)
                bits = packed.hex().upper()
                if not math.isfinite(value) or not math.isfinite(struct.unpack(">f", packed)[0]) or bits != case.get("binary32Hex"):
                    errors.append(f"scalar canonicalization golden mismatch for {case.get('input')}")
                observed_bits.add(bits)
            elif case["expected"] == "reject":
                try:
                    packed = struct.pack(">f", value)
                    finite32 = math.isfinite(struct.unpack(">f", packed)[0])
                except OverflowError:
                    finite32 = False
                if finite32 or case.get("diagnosticCode") != "node.parameter.domain" or not str(case.get("path", "")).startswith("/nodes/"):
                    errors.append(f"out-of-range scalar must reject with node parameter diagnostic: {case.get('input')}")
            else:
                errors.append("scalar canonicalization case must accept or reject")
        except (KeyError, TypeError, ValueError, OverflowError, struct.error) as error:
            errors.append(f"invalid scalar canonicalization case: {error}")
    if len(scalar_cases) != 5 or observed_bits != {"00000000", "7F7FFFFF"}:
        errors.append("scalar canonicalization corpus must cover positive/negative zero, max finite, and two overflow rejects")
    fixture_bindings = core_schema.get("fixtureBindings", [])
    bound_fixtures = {item.get("fixture") for item in fixture_bindings if isinstance(item, dict)}
    required_bound_fixtures = {
        "node_parameter_defaults_legacy.json", "unknown_extension_legacy.json", "principled_default.json", "surface_numeric_semantics.json", "noise_ramp_basecolor.json",
        "image_texture_slots.json", "wrong_kind_negative.json", "missing_arrays_negative.json",
        "skipped_type_mismatch_link_negative.json", "duplicate_input_negative.json",
        "empty_ramp_stops_negative.json", "oversized_ramp_stops_negative.json",
    }
    if bound_fixtures != required_bound_fixtures or not required_bound_fixtures <= declared:
        errors.append("Core schema fixture bindings are incomplete, duplicated, or reference unknown fixtures")
    permutation_pairs = core_schema.get("canonicalPermutationPairs", [])
    pair_sources = {pair.get("source") for pair in permutation_pairs if isinstance(pair, dict) and pair.get("expectedSameCanonicalDocument") is True}
    if pair_sources != {"principled_default.json", "surface_numeric_semantics.json"}:
        errors.append("Core canonical permutation pairs must cover baseline and surface numeric fixtures")
    for pair in permutation_pairs:
        if not isinstance(pair, dict):
            continue
        source_path = fixture_root / pair.get("source", "")
        try:
            original = json.loads(source_path.read_text(encoding="utf-8"))
            canonical_bytes = canonical_legacy_document(original)
            permuted = dict(original)
            permuted["nodes"] = list(reversed(original["nodes"]))
            permuted["links"] = list(reversed(original["links"]))
            if canonical_legacy_document(permuted) != canonical_bytes:
                errors.append(f"canonical permutation is order-sensitive: {pair.get('source')}")
            expected_hash = pair.get("canonicalJsonSha256", "").upper()
            actual_hash = hashlib.sha256(canonical_bytes).hexdigest().upper()
            if expected_hash != actual_hash:
                errors.append(f"canonical JSON golden hash mismatch for {pair.get('source')}: expected {expected_hash}, got {actual_hash}")
        except (OSError, KeyError, TypeError, UnicodeError, json.JSONDecodeError) as error:
            errors.append(f"cannot canonicalize Core schema fixture {pair.get('source')}: {error}")
    for case in core_schema.get("layoutSemanticProjectionCases", []):
        if not isinstance(case, dict):
            errors.append("layout/semantic projection case must be an object")
            continue
        try:
            source_payload = json.loads((fixture_root / case["source"]).read_text(encoding="utf-8"))
            documents = []
            semantics = []
            known_ids = {node["id"] for node in source_payload["nodes"]}
            for layout in (case["layoutA"], case["layoutB"]):
                if set(layout) - known_ids:
                    raise ValueError("layout refers to unknown node id")
                candidate = json.loads(json.dumps(source_payload))
                for node in candidate["nodes"]:
                    if node["id"] in layout:
                        node["x"], node["y"] = layout[node["id"]]
                documents.append(canonical_legacy_document(candidate))
                semantics.append(semantic_legacy_projection(candidate))
            if (documents[0] != documents[1]) != case.get("expectedDocumentHashesDiffer"):
                errors.append(f"layout document distinction contract failed: {case.get('id')}")
            if (semantics[0] == semantics[1]) != case.get("expectedSemanticHashesEqual"):
                errors.append(f"layout semantic-projection contract failed: {case.get('id')}")
            for layout in (case["layoutA"], case["layoutB"]):
                if any(not isinstance(pos, list) or len(pos) != 2 or any(not isinstance(value, (int, float)) or not __import__('math').isfinite(value) for value in pos) for pos in layout.values()):
                    errors.append(f"layout contains invalid coordinates: {case.get('id')}")
        except (OSError, KeyError, TypeError, ValueError, UnicodeError, json.JSONDecodeError) as error:
            errors.append(f"cannot validate layout/semantic projection case {case.get('id')}: {error}")

    source_contracts = manifest.get("sourceContracts", [])
    shader_node_source = root / "ArtifactCore/src/ShaderNode/ArtifactShaderNode.cpp"
    try:
        shader_node_source_text = shader_node_source.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read ShaderNode adapter source for migration audit: {error}")
        shader_node_source_text = ""
    shader_node_interface = root / "ArtifactCore/include/ShaderNode/ArtifactShaderNode.ixx"
    try:
        shader_node_interface_text = shader_node_interface.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read ShaderNode interface for default audit: {error}")
        shader_node_interface_text = ""
    default_source_fragments = (
        "float value = 0.0f;",
        "float r = 0.5f, g = 0.5f, b = 0.5f, a = 1.0f;",
        "Op op = Op::Add;",
        "bool clampResult = false;",
        "bool clampFactor = true;",
        "Blend blend = Blend::Mix;",
        "Interp interp = Interp::Linear;",
        "slot = Slot::BaseColor;",
        "p[QStringLiteral(\"value\")] = static_cast<double>(v->value);",
        "p[QStringLiteral(\"r\")] = static_cast<double>(v->r);",
        "p[QStringLiteral(\"clampFactor\")] = v->clampFactor;",
        "p[QStringLiteral(\"blend\")] = static_cast<int>(v->blend);",
        "p[QStringLiteral(\"interp\")] = static_cast<int>(v->interp);",
        "p[QStringLiteral(\"stops\")] = stops;",
        "p[QStringLiteral(\"slot\")] = static_cast<int>(v->slot);",
    )
    for fragment in default_source_fragments[:8]:
        if fragment not in shader_node_interface_text:
            errors.append(f"legacy node default source observation changed: {fragment}")
    for fragment in default_source_fragments[8:]:
        if fragment not in shader_node_source_text:
            errors.append(f"legacy serializer default materialization observation changed: {fragment}")
    ramp_constructor_start = shader_node_source_text.find("ColorRampNode::ColorRampNode(")
    math_start = shader_node_source_text.find("MathNode::MathNode(", ramp_constructor_start)
    ramp_constructor = shader_node_source_text[ramp_constructor_start:math_start] if ramp_constructor_start >= 0 and math_start >= 0 else ""
    if "Stop{0.0f, 0.0f, 0.0f, 0.0f, 1.0f}" not in ramp_constructor or "Stop{1.0f, 1.0f, 1.0f, 1.0f, 1.0f}" not in ramp_constructor:
        errors.append("legacy ColorRamp constructor default stop golden changed")
    required_migration_source_fragments = {
        "wrong_kind_negative.json": ('root.value(QStringLiteral("version"))', 'root.value(QStringLiteral("kind"))', 'result.error = "unsupported material graph kind"'),
        "missing_arrays_negative.json": ('root.value(QStringLiteral("nodes")).toArray()', 'root.value(QStringLiteral("links")).toArray()'),
        "skipped_type_mismatch_link_negative.json": ('++skippedLinks;', 'result.ok = true;'),
        "out_of_range_enum_negative.json": ('std::clamp(p.value(QStringLiteral("op")).toInt(0), 0, 16)',),
        "planned/separate_color_packed_mr.json": ('result.error = "unknown node type: " + type;',),
        "empty_ramp_stops_negative.json": ('if (!stops.isEmpty())', 'if (v->stops.empty())'),
        "missing_output_compile_failure.json": ('return fail("no Material Output node")',),
    }
    for fixture_name, fragments in required_migration_source_fragments.items():
        for fragment in fragments:
            if fragment not in shader_node_source_text:
                errors.append(f"legacy migration source observation is stale for {fixture_name}: {fragment}")
    loader_start = shader_node_source_text.find("MaterialGraphLoadResult NodeGraph::fromJson(")
    compiler_start = shader_node_source_text.find("MaterialGraphResult NodeGraph::compileMaterialGraph(", loader_start)
    if (loader_start < 0 or compiler_start < 0 or
            'root.value(QStringLiteral("kind"))' not in shader_node_source_text[loader_start:compiler_start] or
            'result.error = "unsupported material graph kind"' not in shader_node_source_text[loader_start:compiler_start]):
        errors.append("wrong-kind migration observation changed: fromJson must reject a mismatched string kind")
    serialization_start = shader_node_source_text.find("std::string NodeGraph::toJson(")
    if serialization_start < 0 or loader_start < 0 or "extension" in shader_node_source_text[serialization_start:loader_start].lower() or "extension" in shader_node_source_text[loader_start:compiler_start].lower():
        errors.append("legacy extension-field loss observation changed: review JSON extension handling")
    extension_fixture = fixture_root / "unknown_extension_legacy.json"
    try:
        extension_document = json.loads(extension_fixture.read_text(encoding="utf-8"))
        extension_node = next(node for node in extension_document["nodes"] if node["id"] == "surface")
        extension_link = extension_document["links"][0]
        if not all(("documentExtension" in extension_document, "extension" in extension_node, "extension" in extension_link)):
            errors.append("unknown-extension fixture must cover document, node, and link scopes")
    except (OSError, KeyError, StopIteration, TypeError, UnicodeError, json.JSONDecodeError) as error:
        errors.append(f"cannot validate unknown-extension fixture structure: {error}")
    for contract in source_contracts:
        if not isinstance(contract, dict):
            errors.append("source contract entry must be an object")
            continue
        source_path = root / contract.get("source", "")
        try:
            source_text = source_path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"cannot read source contract {contract.get('source')}: {error}")
            continue
        for fragment in contract.get("requiredFragments", []):
            if fragment not in source_text:
                errors.append(f"source contract fragment missing in {contract['source']}: {fragment}")

    validator = manifest.get("manifestValidator", {})
    manifest_validator_scope = validator.get("scope", "")
    for scope_term in ("fixture inventory", "pixel-golden agreement", "legacy node-kind/parameter mapping", "constructor defaults", "serializer materialization", "capture route", "image-evaluation matrix", "baseline", "noise protocol"):
        if scope_term not in manifest_validator_scope:
            errors.append(f"manifestValidator.scope omits declared check: {scope_term}")

    smoke_cases = manifest.get("productionLayerSmoke", {})
    for route_name, cases in smoke_cases.items():
        if not isinstance(cases, list):
            errors.append(f"productionLayerSmoke entry must be an array: {route_name}")
            continue
        for case in cases:
            if not isinstance(case, dict):
                errors.append(f"productionLayerSmoke case entry must be an object: {route_name}")
                continue
            source_path = root / case.get("source", "")
            try:
                source_text = source_path.read_text(encoding="utf-8")
            except (OSError, UnicodeError) as error:
                errors.append(f"cannot read production smoke source {case.get('source')}: {error}")
                continue
            case_name = case.get("case", "")
            if case_name and not re.search(rf"\bTEST(?:_F)?\s*\(\s*[^,]+\s*,\s*{re.escape(case_name)}\s*\)", source_text):
                errors.append(f"production smoke case missing from source: {case_name}")
            for fragment in case.get("requiredSourceFragments", []):
                if fragment not in source_text:
                    errors.append(f"production smoke source fragment missing for {case_name}: {fragment}")
            for capture_name in case.get("captureNames", []):
                if capture_name not in source_text:
                    errors.append(f"production smoke capture name missing for {case_name}: {capture_name}")

    validator = manifest.get("manifestValidator", {})
    if validator.get("source") != "tools/shader_graph_rd/validate_shader_graph_manifest.py":
        errors.append("manifestValidator source path is inconsistent")
    for key in ("analysisTool", "displayPngAnalysisTool", "displayPngSweepAnalysisTool"):
        tool_record = manifest.get(key)
        if not isinstance(tool_record, dict):
            errors.append(f"missing analyzer record: {key}")
            continue
        tool_path = root / tool_record.get("source", "")
        if not tool_path.is_file():
            errors.append(f"analyzer source is missing: {tool_record.get('source')}")

    image_conditions = manifest.get("imageConditions", {})
    required_matrix = image_conditions.get("requiredEvaluationMatrix", {})
    required_eval_keys = {"effectCorrectness", "detailAndArtifacts", "colorAndAlpha", "temporalStability", "routesAndResolution", "performance", "visualReview"}
    if required_eval_keys - set(required_matrix):
        errors.append("required image evaluation matrix is incomplete: " + ", ".join(sorted(required_eval_keys - set(required_matrix))))
    for key, record in required_matrix.items():
        if not isinstance(record, dict) or not record.get("status"):
            errors.append(f"image evaluation item lacks a status: {key}")
    color_matrix = required_matrix.get("colorAndAlpha", {})
    color_matrix_cases = color_matrix.get("color4PolicyCases", [])
    if (color_matrix.get("status") != "partially-planned-runtime-unverified" or
            len(color_matrix_cases) != 2 or
            not any(item.startswith("color-transfer-opaque-gray:") for item in color_matrix_cases) or
            not any(item.startswith("alpha-association-half-red-over-black-white:") for item in color_matrix_cases) or
            "does not establish Color4 transfer or alpha association" not in color_matrix.get("currentEvidenceLimit", "")):
        errors.append("image evaluation color/alpha matrix must retain both planned Color4 policy probes and mark current evidence insufficient")
    matrix = image_conditions.get("executionMatrix", [])
    if not matrix or any(not isinstance(row, dict) or not row.get("route") or not row.get("status") for row in matrix):
        errors.append("image evaluation execution matrix is incomplete")
    noise_protocol = manifest.get("measurements", {}).get("toleranceProtocol", {})
    if noise_protocol.get("baselineCaptureCountMinimum") != 5 or not noise_protocol.get("freezeBeforeCandidateComparison"):
        errors.append("baseline-noise protocol must require five captures and pre-comparison threshold freeze")

    capture_routes = manifest.get("captureRoutes", {})
    for route_name, route in capture_routes.items():
        if not isinstance(route, dict):
            errors.append(f"capture route must be an object: {route_name}")
            continue
        route_source = root / route.get("source", "")
        try:
            route_text = route_source.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"cannot read capture route source {route.get('source')}: {error}")
            continue
        case = route.get("case")
        if case:
            case_name = case.rsplit(".", 1)[-1]
            match = re.search(rf"\bTEST(?:_F)?\s*\(\s*{re.escape(case.split('.', 1)[0])}\s*,\s*{re.escape(case_name)}\s*\)", route_text)
            if not match:
                errors.append(f"capture route case is missing from {route.get('source')}: {case}")
        for fragment in route.get("requiredSourceFragments", []):
            if fragment not in route_text:
                errors.append(f"capture source fragment missing in {route.get('source')}: {fragment}")
        for capture_name in route.get("captureNames", []):
            source_capture_name = capture_name[:-4] + ".png" if capture_name.endswith(".pfm") else capture_name
            if source_capture_name not in route_text:
                errors.append(f"capture output name missing in {route.get('source')}: {capture_name}")
        graph_fixture = route.get("graphFixture")
        if graph_fixture and graph_fixture not in declared:
            errors.append(f"capture route references undeclared graph fixture: {graph_fixture}")
        texture_asset = route.get("textureAsset")
        if texture_asset and not (root / texture_asset).is_file():
            errors.append(f"capture route texture asset is missing: {texture_asset}")
        for group in route.get("parameterSamples", {}).values():
            for sample in group:
                fixture_name = sample.get("fixture")
                if fixture_name not in declared:
                    errors.append(f"parameter capture references undeclared fixture: {fixture_name}")
                output_name = sample.get("captureName")
                if output_name and output_name not in route_text:
                    errors.append(f"parameter capture output name missing in source: {output_name}")

    evidence = manifest.get("evidence", {})
    test_source_path = evidence.get("contractTestSource", "tests/ArtifactCore/ShaderGraphContractTest.cpp")
    test_source = root / test_source_path
    try:
        source = test_source.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read contract test source: {error}")
        source = ""
    test_cases = evidence.get("contractTestSourceCases", [])
    found_cases = re.findall(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*([A-Za-z_][A-Za-z_0-9]*)\s*\)", source)
    if len(found_cases) != len(set(found_cases)):
        errors.append("contract test source contains duplicate test case names")
    if set(found_cases) != set(test_cases):
        errors.append("contract test source case list differs from manifest; undeclared=" + ",".join(sorted(set(found_cases) - set(test_cases))) + "; missing=" + ",".join(sorted(set(test_cases) - set(found_cases))))
    lane_plan = evidence.get("contractTestLanePlan", {})
    if not isinstance(lane_plan, dict):
        lane_plan = {}
    expected_failure_cases = lane_plan.get("expectedFailureCandidates", [])
    if not isinstance(expected_failure_cases, list):
        expected_failure_cases = []
    expected_failure_names = [item.get("testCase") for item in expected_failure_cases if isinstance(item, dict)]
    required_expected_failure_cases = {
        "CanonicalSerializationIgnoresNodeAndLinkInsertionOrder",
        "MultipleMaterialOutputsRequireExplicitSelection",
        "DuplicateInputLinksAreRejectedWithoutReplacingExistingGraph",
        "EmptyColorRampStopsAreNotSilentlyIgnored",
        "ExcessColorRampStopsAreNotSilentlyTruncated",
        "LinkRejectsPinsOwnedByAnotherGraph",
        "LinkRejectsIncompatiblePinTypes",
        "LinkRejectsMultipleWritersForOneInput",
        "SanitizedNodeIdentifiersDoNotCollide",
        "OutOfRangeGraphBaseColorAlphaIsDiagnosed",
    }
    source_coverage = evidence.get("contractTestSourceCoverage", {})
    coverage_keys = set(source_coverage) if isinstance(source_coverage, dict) else set()
    if (lane_plan.get("status") != "source-classified-not-executed"
            or lane_plan.get("defaultLane") != "unclassified-do-not-register-as-green"
            or "stay outside the green target" not in lane_plan.get("registrationPolicy", "")
            or set(expected_failure_names) != required_expected_failure_cases
            or len(expected_failure_names) != len(set(expected_failure_names))
            or not required_expected_failure_cases <= set(found_cases)
            or any(item.get("sourceCoverageKey") not in coverage_keys for item in expected_failure_cases if isinstance(item, dict))
            or evidence.get("contractTestRegistered") is not False
            or evidence.get("buildOrTestExecuted") is not False):
        errors.append("legacy C++ contract test lanes must keep source-audited expected failures and unclassified cases out of the unexecuted green target")
    for item in expected_failure_cases:
        if not isinstance(item, dict):
            continue
        test_case = item.get("testCase", "")
        assertion_fragments = item.get("assertionFragments", [])
        body = re.search(
            rf"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*{re.escape(test_case)}\s*\)\s*\{{(.*?)(?=\nTEST\s*\(|\Z)",
            source,
            re.DOTALL,
        ) if isinstance(test_case, str) and test_case else None
        if (not body or not isinstance(assertion_fragments, list) or not assertion_fragments
                or any(not isinstance(fragment, str) or fragment not in body.group(1) for fragment in assertion_fragments)):
            errors.append(f"expected-failure lane must retain its declared assertion fragments: {test_case}")
    required_contract_tests = {"CanonicalSerializationIgnoresNodeAndLinkInsertionOrder", "LayoutRoundTripsWithoutChangingSemanticCompileHash", "FailedLoadPreservesExistingGraph"}
    if not required_contract_tests <= set(found_cases):
        errors.append("contract source lacks canonicalization/layout/atomic-load gate cases")
    document_lowering = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*DocumentLoadCanSucceedWhenMaterialLoweringFails\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", source, re.DOTALL)
    if not document_lowering or not all(fragment in document_lowering.group(1) for fragment in (
        "missing_output_compile_failure.json", "graph.fromJson(payload)",
        "ASSERT_TRUE(loaded.ok)", "graph.compileMaterialGraph()",
        "EXPECT_FALSE(lowered.ok)", "no Material Output",
    )):
        errors.append("document/lowering separation case must load the missing-output fixture successfully and assert an explicit lowering failure")
    mismatch = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*TypeMismatchLinkDoesNotReportSuccessfulDocumentLoad\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", source, re.DOTALL)
    if mismatch and not all(fragment in mismatch.group(1) for fragment in ("before", "EXPECT_FALSE(loaded.ok)", "graph.toJson({}), before")):
        errors.append("type mismatch case must assert failure and preserve prior graph bytes")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    errors = validate(args.project_root.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Shader Graph R&D manifest is consistent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
