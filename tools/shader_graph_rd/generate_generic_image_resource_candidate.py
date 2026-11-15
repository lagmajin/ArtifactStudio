#!/usr/bin/env python3
"""Regenerate the generic Image Texture reference candidate and manifest pins."""

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
    contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"][
        "genericImageResourceReferencePrototype"]
    prototype_path = root / contract["prototype"]
    spec = importlib.util.spec_from_file_location("generic_image_resource_prototype", prototype_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load generic image resource prototype")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)

    document_path = root / contract["documentFixture"]
    catalog_path = root / contract["assetCatalogFixture"]
    source_states_path = root / contract["assetSourceStatesFixture"]
    candidate_path = root / contract["candidateFixture"]
    document = json.loads(document_path.read_text(encoding="utf-8"))
    asset_catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    source_states = json.loads(source_states_path.read_text(encoding="utf-8"))
    candidate = module.build_candidate(document, asset_catalog, source_states)
    candidate_path.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n",
                              encoding="utf-8", newline="\n")

    for key, path in (("documentSha256", document_path),
                      ("assetCatalogSha256", catalog_path),
                      ("assetSourceStatesSha256", source_states_path),
                      ("prototypeSha256", prototype_path),
                      ("candidateSha256", candidate_path)):
        contract[key] = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                             encoding="utf-8", newline="\n")
    print(f"Updated {contract['candidateFixture']} ({contract['candidateSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
