# Repository Audit: Module Hygiene

## Purpose

Inspect module boundaries, import placement, forward declarations, and circular dependency risk.

## Scope

- `Artifact/include/**/*.ixx`
- `Artifact/src/**/*.cppm`
- `ArtifactCore/include/**/*.ixx`
- `ArtifactCore/src/**/*.cppm`

## Questions to Answer

- Which modules are importing more than they need?
- Where are forward declarations used to avoid cycles, and where are they masking a deeper issue?
- Which interfaces still mix declaration and implementation responsibilities?

## Deliverable

- A ranked list of module hygiene issues with suggested fixes.

