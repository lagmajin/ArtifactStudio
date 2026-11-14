#!/usr/bin/env python3
"""Regenerate the generic Image Texture reference candidate and manifest pins."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import copy
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
    candidate_path = root / contract["candidateFixture"]
    document = json.loads(document_path.read_text(encoding="utf-8"))
    asset_catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    candidate = module.project_resource_requirements(document, asset_catalog)
    relinked_document = copy.deepcopy(document)
    previous_asset_id = relinked_document["resources"][0]["source"]["assetId"]
    replacement_asset_id = "22222222-2222-4222-8222-222222222222"
    relinked_document["resources"][0]["source"]["assetId"] = replacement_asset_id
    relinked_catalog = {asset_id: asset_type for asset_id, asset_type in asset_catalog.items()
                        if asset_id.lower() != previous_asset_id.lower()}
    relinked_catalog[replacement_asset_id] = "Image"
    relinked_candidate = module.project_resource_requirements(
        relinked_document, relinked_catalog)
    candidate["assetRelinkProjection"] = {
        "previousAssetId": previous_asset_id,
        "replacementAssetId": replacement_asset_id,
        "previousResolution": candidate["assetResolutions"],
        "replacementResolution": relinked_candidate["assetResolutions"],
        "resourceRequirementsUnchanged": candidate["resourceRequirements"] ==
                                         relinked_candidate["resourceRequirements"],
        "resourceRequirementIdentityUnchanged": candidate["resourceRequirementIdentitySha256"] ==
                                                 relinked_candidate["resourceRequirementIdentitySha256"],
        "resourceBindingKeyStatus": "not-projected",
        "shaderCodeIdentity": "not-measured-no-shader-code-is-generated",
    }
    candidate_path.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n",
                              encoding="utf-8", newline="\n")

    for key, path in (("documentSha256", document_path),
                      ("assetCatalogSha256", catalog_path),
                      ("prototypeSha256", prototype_path),
                      ("candidateSha256", candidate_path)):
        contract[key] = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                             encoding="utf-8", newline="\n")
    print(f"Updated {contract['candidateFixture']} ({contract['candidateSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
