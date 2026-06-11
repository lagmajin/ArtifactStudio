# Repository Audit: Property System

## Purpose

Check that property storage, serialization, UI labels, and timeline exposure use the same contract.

## Scope

- `Artifact/include/Layer/*`
- `Artifact/src/Layer/*`
- `Artifact/src/Widgets/*`
- property group generation and persistence code

## Questions to Answer

- Which properties are editor-visible but not round-trippable?
- Which property paths are duplicated or renamed in different layers?
- Where are animatable flags missing or inconsistent?

## Deliverable

- A mismatch list with concrete property paths.

