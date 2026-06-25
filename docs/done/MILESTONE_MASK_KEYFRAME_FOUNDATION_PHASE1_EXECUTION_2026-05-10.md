# Phase 1 Execution: Mask Parameter Exposure

## Goal

Expose mask parameters as time-addressable properties without folding geometry editing into the same slice.

## Scope

- parent-side roadmap and contract alignment
- property exposure for mask scalar parameters
- evaluation at current timeline time
- render-path handoff of evaluated mask state

## First Slice

1. define which mask parameters are eligible for property exposure
2. keep geometry editing out of the first pass
3. make the evaluated state observable through existing report text paths

## Guardrails

- Do not change mask vertex editing in this phase
- Do not add fixed viewport overlays
- Do not auto-apply tracked or evaluated state to unrelated properties
- Keep editor contract boundaries unchanged

## Done When

- the parent roadmap clearly separates geometry editing from time-addressable parameters
- the property pipeline can describe mask state at a given time
- the render path can consume the evaluated mask state without changing the editor shell contract

## Related Docs

- [Mask Keyframe Foundation](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md)
- [Composition Editor Contract](/x:/Dev/ArtifactStudio/docs/COMPOSITION_EDITOR_CONTRACT.md)
- [Mask / Roto Editing Milestone](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [Inline Interaction Surfaces Milestone](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md)

