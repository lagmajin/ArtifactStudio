# OpenAssetIO Integration Implementation Note

**Date:** 2026-07-20  
**Status:** Foundation complete / Manager selection pending

## Completed

- Registered OpenAssetIO as an optional CMake dependency for `Artifact`.
- Kept the local filesystem Asset Browser workflow as the default fallback.
- Added persisted OpenAssetIO settings:
  - enabled flag
  - Manager identifier
  - Manager configuration path
- Added a common asset reference model that can represent either:
  - a local filesystem path
  - an OpenAssetIO Entity Reference

## Deferred: phases 4 and 5

The following work is intentionally deferred until a concrete OpenAssetIO Manager is selected:

1. Replace or extend Asset Browser exploration with Manager-backed entities.
2. Resolve Entity References to media locations.
3. Connect Resolve to import, relink, preview, and drag-and-drop workflows.
4. Preserve the Entity Reference when a project is saved, even when the resolved local path changes.

OpenAssetIO defines the Host/Manager protocol, but it does not provide the asset database or the actual resolution policy. Implementing these phases without selecting a Manager would create a temporary provider that would likely need to be replaced.

## Next implementation prerequisites

- Select the first supported Manager and confirm its distribution/license model.
- Define the Manager configuration format and runtime discovery path.
- Map the Manager's entity traits to Artifact's common asset reference and preview metadata.
- Add offline and Manager-unavailable behavior before enabling external references by default.

## Relevant code

- `Artifact/CMakeLists.txt`
- `Artifact/include/Asset/ArtifactAssetIntegrationSettings.ixx`
- `Artifact/include/Asset/ArtifactAssetReference.ixx`

