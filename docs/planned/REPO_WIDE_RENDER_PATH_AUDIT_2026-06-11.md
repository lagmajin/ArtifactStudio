# Repository Audit: Render Paths

## Purpose

Map the major render paths and identify where preview, composition, software fallback, and queue rendering diverge.

## Scope

- `Artifact/src/Render/*`
- `Artifact/src/Preview/*`
- `Artifact/src/Widgets/Render/*`
- `ArtifactCore/src/Graphics/*`
- Diligent / DX12 boundary code

## Questions to Answer

- Which render paths share the same model and which fork early?
- Where do text, image, and vector layers diverge in a way that affects parity?
- Which caches are authoritative, and which are derived?

## Deliverable

- A path-by-path summary and a list of parity risks.

