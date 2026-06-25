# Milestone: Mask Keyframe Foundation

> Parent-side planning index for the mask time-addressable slice.

This document is the parent repository's coordination point for mask parameter keyframing.  
The execution-level implementation notes live in the `Artifact` submodule and should remain there unless the parent needs a higher-level roadmap adjustment.

## Purpose

- Keep mask geometry editing and mask parameter animation separate
- Provide a clear parent-side entry for the time-addressable slice
- Avoid mixing property bake concerns into the editor mask / roto workflow

## Boundary

- `M-UI-7 Composition Editor Mask / Roto Editing`
  - owns shape / roto / vertex editing
- `M-UI-15 Inline Interaction Surfaces`
  - owns inline choice and lightweight selection surfaces
- `Mask Keyframe Foundation`
  - owns the time-addressable mask parameter slice

## Intended First Slice

1. expose scalar mask parameters as properties
2. evaluate those properties at timeline time
3. feed the evaluated mask state back into the render path

## Guardrails

- Do not merge mask geometry editing with parameter animation in the same step
- Do not add new fixed overlay surfaces for this slice
- Use `FrameDebugSnapshot` and report text for observation
- Keep the editor contract focused on routing and mode ownership

## Cross References

- [Composition Editor Contract](/x:/Dev/ArtifactStudio/docs/COMPOSITION_EDITOR_CONTRACT.md)
- [Mask / Roto Editing Milestone](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [Inline Interaction Surfaces Milestone](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md)
- [Phase 1 Execution: Mask Parameter Exposure](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_PHASE1_EXECUTION_2026-05-10.md)
- `Artifact/docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`

## Next Step

1. keep the parent roadmap and editor contract aligned with the time-addressable split
2. start from the Phase 1 execution memo for the property-exposure slice
3. let the submodule implementation doc remain the place for low-level changes
4. revisit this index only if the property pipeline contract changes
