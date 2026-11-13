#!/usr/bin/env python3
"""Validate Shader Graph R&D fixtures, assets, and evidence-manifest consistency."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import sys
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

    fixtures = manifest.get("fixtures")
    if not isinstance(fixtures, list):
        return errors + ["fixtures must be an array"]
    declared: set[str] = set()
    disk_fixtures = {
        path.relative_to(fixture_root).as_posix()
        for path in fixture_root.rglob("*.json")
        if path.name != "evaluation_manifest.json" and "planned" not in path.parts
        and path.parent.name != "textures" and path.name not in {"ramp_duplicate_stops_formula_golden.json", "math_operation_formula_golden.json", "mixrgb_formula_golden.json", "lowering_identity_proposal_golden.json", "typed_ir_projection_readiness.json", "typed_ir_explicit_conversion_expression.json", "typed_ir_vector4_to_vector3_expression.json", "operation_registry_audit.json", "constant_value_canonicalization_golden.json", "surface_alpha_formula_golden.json", "luminance_formula_golden.json", "clamp_formula_golden.json", "vector4_invert_formula_golden.json", "map_range_formula_golden.json"}
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
        if fixture.get("class") != "planned" and not isinstance(payload, dict):
            errors.append(f"fixture root must be an object: {relative}")
        if isinstance(payload, dict) and fixture.get("class") != "planned":
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
    if set(covered) != declared:
        if set(covered) != declared - {"entry_path_compatibility_cases.json"}:
            errors.append("fixtureEvaluationCoverage must cover every declared current graph fixture exactly once, excluding source-only research artifacts")

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
        sample_limits = {"maxJsonBytes": 4096, "maxNodes": 64, "maxLinks": 6, "maxTraversalDepth": 32}
        sample_corpus = corpus_module.build_case_set(sample_limits)
        corpus_errors = corpus_module.validate_case_set(sample_corpus)
        if corpus_errors:
            errors.extend("document limit corpus: " + item for item in corpus_errors)
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
    required_operation_schema_fields = {"schemaId", "schemaVersion", "status", "recordFields", "inputRecordFields", "outputRecordFields", "parameterRecordFields", "recordRules", "exampleOperationRecords", "legacyNodeCoverage", "promotionGate", "fixtureProjectionReadiness", "explicitConversionExpression", "vectorTruncationExpression", "typedLoweringCandidateContract", "operationRegistryAudit", "operationPromotionWaves", "constantValueCanonicalizationGolden", "color4SemanticPolicy", "color4PolicyEvaluationCases", "color4PolicyEvaluationStatus", "surfaceAlphaFormulaGolden", "luminanceFormulaGolden", "clampFormulaGolden", "vector4InvertFormulaGolden"}
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
        "dynamicRegistration": "new operation support requires a registered immutable semantic record and lowering implementation; arbitrary runtime code or HLSL strings are not executable registry payload",
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
    if (len(ramp_execution) != 1 or ramp_execution[0].get("status") != "planned-runtime-unverified" or
            ramp_execution[0].get("route") != "not-implemented" or len(ramp_execution[0].get("knownGaps", [])) < 3):
        errors.append("ColorRamp duplicate-stop execution gate must remain explicitly planned and list its runtime gaps")
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
        *tie_names, "forwardGraph.fromJson", "reversedGraph.fromJson", "forwardGraph.toJson()", "compileMaterialGraph()",
        "EXPECT_NE(forwardGraph.toJson(), reversedGraph.toJson())",
        "Legacy std::sort tie ordering is unspecified",
    )):
        errors.append("duplicate-stop source case must preserve authored document order without asserting unspecified legacy tie lowering")
    voronoi_test_body = re.search(r"\bTEST\s*\(\s*ShaderGraphContractTest\s*,\s*VoronoiFixtureConnectsCellColorAndDistance\s*\)\s*\{(.*?)(?=\nTEST\s*\(|\Z)", procedural_test_source, re.DOTALL)
    if not voronoi_test_body or not all(fragment in voronoi_test_body.group(1) for fragment in (
        "voronoi_cells_surface.json", "graph.fromJson(payload)",
        "voronoi_Color = float4(voronoi_cell, 1.0f)",
        "voronoi_Distance = sqrt(voronoi_Distance)",
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
        "wrong_kind_negative.json": ('root.value(QStringLiteral("version"))',),
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
    if loader_start < 0 or compiler_start < 0 or 'QStringLiteral("kind")' in shader_node_source_text[loader_start:compiler_start]:
        errors.append("wrong-kind migration observation changed: fromJson kind handling needs source review")
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
