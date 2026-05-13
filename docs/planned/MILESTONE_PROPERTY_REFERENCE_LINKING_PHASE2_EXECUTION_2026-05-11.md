# Phase 2 Execution: Property Row Pick Whip Surface

## Goal

Make property reference linking visible and usable from the property row / inspector surface.

## Scope

- property row affordance placement
- inspector target display
- drag gesture to capture a target link
- read-only target preview while hovering or dragging

## First Slice

1. show the currently linked target inline in the row
2. let the user drag from a property row to a compatible target
3. keep the link surface local to PropertyEditor / Inspector workflows
4. avoid changing keyframe semantics

## Guardrails

- No global link routing
- No auto-linking on selection changes
- No new timeline mode
- No mutation of expression semantics yet

## Done When

- the row shows the link target clearly
- the user can initiate a link gesture from the row
- the UI still treats references and keyframes as separate concepts

## Related Docs

- [Property Reference Linking / Pick Whip](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md)
- [Phase 1 Execution: Property Target Resolution](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_PHASE1_EXECUTION_2026-05-11.md)
- [Inline Interaction Surfaces](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md)
- [Property Widget Row Alignment](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_INSPECTOR_LAYOUT_2026-04-03.md)

