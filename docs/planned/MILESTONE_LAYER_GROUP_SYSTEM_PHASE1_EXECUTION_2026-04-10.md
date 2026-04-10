# Milestone: Layer Group System Phase 1 Execution

> 2026-04-10

## Goal

Turn the existing `Layer Group` concept from a planning skeleton into a real, serializable data model that the project layer can load, save, and query.

This phase does not try to finish the full UI or transform-hierarchy integration. The main target is to make groups real enough that later timeline/layer-panel work can build on top of them without reworking the data shape.

## What We Already Know

- `ArtifactLayerGroup` / `ArtifactLayerGroupCollection` are referenced in the design docs and AI descriptions.
- The current codebase does not expose a finished `ArtifactLayerGroup` implementation in `Artifact/include` or `Artifact/src`.
- The timeline/layer panel already has per-layer expand/collapse state, but that is not the same as a real group model.

## Current Implementation Slice

- `ArtifactGroupLayer` now propagates composition ownership to its children.
- Child removal / clear operations now release composition ownership more safely.
- A built-in regression test covers group creation, child attachment, composition propagation, and JSON roundtrip.

## Phase 1 Scope

### Data Model

- Add a concrete `ArtifactLayerGroup` type.
- Add `ArtifactLayerGroupCollection` for ownership and lookup.
- Support:
  - `group id`
  - `parent group id`
  - `name`
  - `expanded`
  - `muted`
  - `locked`
  - `color`
  - `opacity`
  - child/group membership bookkeeping

### Serialization

- Serialize groups as part of project save/load.
- Preserve parent-child relationships.
- Keep default/root group behavior explicit.
- Make unknown or missing group data fail safely instead of corrupting the project tree.

### Project API

- Add project-level read/write accessors for groups.
- Add helpers for:
  - create group
  - rename group
  - remove group
  - reparent group
  - move layers into/out of a group
- Keep the API small and predictable so later UI and AI tools can share it.

### Validation

- Reject invalid parent references.
- Prevent cyclic parent chains.
- Keep group removal behavior deterministic.
- Verify that loading an older project without groups still works.

## Out of Scope

- Timeline/layer-panel group row UI
- Transform hierarchy integration
- Solo/shy/visibility propagation rules
- Batch editing UI
- Effect group / virtual group extensions

## Suggested Implementation Order

1. Add `ArtifactLayerGroup` and `ArtifactLayerGroupCollection`.
2. Wire group storage into `ArtifactProject`.
3. Add save/load support.
4. Add service-level helpers.
5. Add regression tests for project roundtrip and invalid parent data.

## Success Criteria

- A project can create, store, and reload layer groups.
- Group parent relationships survive serialization.
- The existing project tree and layer panel still work when no groups are present.
- Later UI work can read group data without touching serialization again.
