# Repository Audit: Layer Lifecycle

**ステータス:** Complete (static verification)

**完了日:** 2026-07-12

## Purpose

Trace layer creation, activation, mutation, and destruction to find lifecycle mismatches.

## Scope

- layer base classes
- composition add/remove flows
- selection, undo, and render invalidation hooks

## Questions to Answer

- Which layer types have special lifecycle rules?
- Where does a layer change without invalidating the right caches?
- Which flows depend on selection state being already synchronized?

## Deliverable

- A lifecycle diagram and a list of fragile handoff points.

## Canonical lifecycle

`ArtifactProjectService` -> `ArtifactProjectManager` / `ArtifactProject` -> `ArtifactAbstractComposition` -> `ArtifactAbstractLayer`

- Insert: composition assigns ownership, normalizes default duration, updates the layer index, invalidates thumbnail state, recalculates range, and publishes `LayerChangedEvent::Created`.
- Move: composition changes index order and invalidates thumbnail state.
- Remove: child parent-links are cleared, the layer is removed from the index, ownership is detached, thumbnail state and frame range are refreshed, and `LayerChangedEvent::Removed` is published.
- Load: layers are created first, inserted through the canonical path, and parent links are resolved in a second pass.
- Duplicate: properties pass through `PropertySerializationBridge`; name, blend mode, and parent identity are restored explicitly.

## Hardened invariants

- A layer cannot restore itself as its own parent.
- A missing parent ID is cleared instead of leaving a dangling hierarchy reference.
- Removing a layer recalculates the composition frame range as well as invalidating thumbnails.
- Removing a parent clears direct child parent-links before detachment.

## Remaining architectural risk

- Some higher-level precompose rollback logic remains transaction-like procedural code rather than a reusable lifecycle transaction object.
- This completion is based on source and diff inspection; runtime interaction tests were not executed by user choice.
