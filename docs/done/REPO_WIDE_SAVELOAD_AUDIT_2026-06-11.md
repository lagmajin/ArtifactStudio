# Repository Audit: Save and Load

**ステータス:** Complete (static verification)

**完了日:** 2026-07-12

## Purpose

Verify that project serialization, layer persistence, and import/export paths round-trip the same feature set.

## Scope

- `Artifact/src/Project/*`
- `Artifact/src/Layer/*`
- `ArtifactCore/src/*` serialization helpers
- project import/export code

## Questions to Answer

- Which fields serialize, but do not deserialize fully?
- Which data is preserved in JSON but lost in UI round-trip?
- Which older files still need compatibility handling?

## Deliverable

- A save/load gap list and a compatibility risk list.

## Static verification result

- Project JSON stores version bounds, current composition, creation defaults, guides, extension data, compositions, and the recursive project tree.
- Folder, footage, sequence, solid, and composition items restore stable IDs and parent ownership.
- Composition JSON restores settings, frame/work-area ranges, effects, layers, state variants, transform fields, current frame, and playback state.
- Layer JSON is restored through the layer factory and then attached through `appendLayerTop()`, so composition ownership follows the normal insertion path.
- Parent references are resolved only after every layer has been created. Invalid, missing, and self-parent references are now cleared during restore.
- Property duplication uses `PropertySerializationBridge`, preserving property values, keyframes, and envelopes through the canonical property serializer.
- Project tree integrity is checked before serialization and reports a warning instead of silently hiding structural corruption.

## Compatibility risks retained

- Unknown future layer types are skipped by the current layer factory rather than preserved as opaque payloads.
- This completion is based on source and diff inspection. Runtime reopen cases `SL-01..SL-06` remain unexecuted by user choice.
