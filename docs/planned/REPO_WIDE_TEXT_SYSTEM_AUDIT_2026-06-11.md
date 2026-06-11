# Repository Audit: Text System

## Purpose

Text Layer, Text Animator, Glyph Layout, font fallback, and inspector exposure are aligned across `Artifact` and `ArtifactCore`.

## Scope

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/include/Layer/ArtifactTextLayer.ixx`
- `ArtifactCore/include/Text/*`
- `ArtifactCore/src/Text/*`
- text-related inspector and timeline surfaces

## Questions to Answer

- Which text features are implemented, partially implemented, or only documented?
- Where does `ArtifactTextLayer` still duplicate logic already present in `ArtifactCore`?
- Which property names are exposed in UI but not fully supported in the model?

## Deliverable

- A short status table with implemented, partial, and missing items.
- A dependency map for the next text milestone.

