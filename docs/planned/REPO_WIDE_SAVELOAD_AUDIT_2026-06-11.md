# Repository Audit: Save and Load

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

