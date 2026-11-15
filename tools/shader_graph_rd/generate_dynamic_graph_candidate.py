#!/usr/bin/env python3
"""Regenerate the saved candidate and its evaluation-manifest digest."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import sys


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    manifest_path = root / "tests/fixtures/shader-graph-rd/evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"]
    prototype_path = root / contract["prototype"]
    spec = importlib.util.spec_from_file_location("dynamic_graph_candidate_generator", prototype_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load dynamic graph prototype")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    registry = module.load_registry(root / contract["registryFixture"])
    candidate_specs = [("documentFixture", "candidateFixture", "candidateSha256", "documentSha256")]
    candidate_specs.append(("vector3ConstantDocumentFixture", "vector3ConstantCandidateFixture",
                            "vector3ConstantCandidateSha256", "vector3ConstantDocumentSha256"))
    candidate_specs.append(("vector3ArithmeticDocumentFixture", "vector3ArithmeticCandidateFixture",
                            "vector3ArithmeticCandidateSha256", "vector3ArithmeticDocumentSha256"))
    candidate_specs.append(("optionalDefaultDocumentFixture", "optionalDefaultCandidateFixture",
                            "optionalDefaultCandidateSha256", "optionalDefaultDocumentSha256"))
    candidate_specs.append(("optionalVector4DefaultDocumentFixture", "optionalVector4DefaultCandidateFixture",
                            "optionalVector4DefaultCandidateSha256", "optionalVector4DefaultDocumentSha256"))
    candidate_specs.append(("vector2UvDocumentFixture", "vector2UvCandidateFixture",
                            "vector2UvCandidateSha256", "vector2UvDocumentSha256"))
    candidate_specs.append(("vector2MappingDocumentFixture", "vector2MappingCandidateFixture",
                            "vector2MappingCandidateSha256", "vector2MappingDocumentSha256"))
    candidate_specs.append(("baseColorTextureDocumentFixture", "baseColorTextureCandidateFixture",
                            "baseColorTextureCandidateSha256", "baseColorTextureDocumentSha256"))
    candidate_specs.append(("mappedBaseColorTextureDocumentFixture", "mappedBaseColorTextureCandidateFixture",
                            "mappedBaseColorTextureCandidateSha256", "mappedBaseColorTextureDocumentSha256"))
    candidate_specs.append(("textureChannelsDocumentFixture", "textureChannelsCandidateFixture",
                            "textureChannelsCandidateSha256", "textureChannelsDocumentSha256"))
    candidate_specs.extend((document_key, candidate_key, digest_key, input_digest_key)
                           for document_key, candidate_key, digest_key, input_digest_key in (
                               ("subtractDocumentFixture", "subtractCandidateFixture", "subtractCandidateSha256", "subtractDocumentSha256"),
                               ("divideDocumentFixture", "divideCandidateFixture", "divideCandidateSha256", "divideDocumentSha256"),
                               ("vector4DivideDocumentFixture", "vector4DivideCandidateFixture", "vector4DivideCandidateSha256", "vector4DivideDocumentSha256"),
                               ("vector4SubtractDocumentFixture", "vector4SubtractCandidateFixture", "vector4SubtractCandidateSha256", "vector4SubtractDocumentSha256"),
                               ("vector4InvertDocumentFixture", "vector4InvertCandidateFixture", "vector4InvertCandidateSha256", "vector4InvertDocumentSha256"),
                               ("clampDocumentFixture", "clampCandidateFixture", "clampCandidateSha256", "clampDocumentSha256"),
                               ("vector4MixDocumentFixture", "vector4MixCandidateFixture", "vector4MixCandidateSha256", "vector4MixDocumentSha256"),
                               ("vector4SeparateDocumentFixture", "vector4SeparateCandidateFixture", "vector4SeparateCandidateSha256", "vector4SeparateDocumentSha256"),
                               ("vector4RampDocumentFixture", "vector4RampCandidateFixture", "vector4RampCandidateSha256", "vector4RampDocumentSha256"),
                           )
                           if all(key in contract for key in (document_key, candidate_key, digest_key, input_digest_key)))
    generated = []
    for document_key, candidate_key, digest_key, input_digest_key in candidate_specs:
        document_path = root / contract[document_key]
        document_bytes = document_path.read_bytes()
        document = json.loads(document_bytes.decode("utf-8"))
        candidate = module.compile_document_from_data(document, registry)
        candidate_path = root / contract[candidate_key]
        candidate_bytes = (json.dumps(candidate, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        candidate_path.write_bytes(candidate_bytes)
        contract[digest_key] = hashlib.sha256(candidate_bytes).hexdigest().upper()
        contract[input_digest_key] = hashlib.sha256(document_bytes).hexdigest().upper()
        generated.append((contract[candidate_key], contract[digest_key]))
    mapped_uv_evaluation = contract.get("textureSampleResourceContract", {}).get(
        "imageEvaluationPlan", {}).get("mappedUvCandidate", {})
    if mapped_uv_evaluation:
        mapped_uv_evaluation["documentSha256"] = contract["mappedBaseColorTextureDocumentSha256"]
        mapped_uv_evaluation["candidateSha256"] = contract["mappedBaseColorTextureCandidateSha256"]
    contract["registrySha256"] = hashlib.sha256((root / contract["registryFixture"]).read_bytes()).hexdigest().upper()
    manifest_path.write_bytes((json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    for candidate_path, digest in generated:
        print(f"Updated {candidate_path} ({digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
