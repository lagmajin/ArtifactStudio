# Milestone: Property Reference Linking / Pick Whip

> Parent-side roadmap for AE-style property linking.

This milestone covers the "pick-whip" style workflow where one property can be linked to another property without typing a raw path by hand.

## Purpose

- Make property relationships first-class
- Reduce manual target-path entry
- Give expression and driven-property workflows a visual linking surface
- Keep the property source of truth aligned with `AbstractProperty`

## Boundary

- This milestone does not replace keyframe editing
- This milestone does not replace curve editing
- This milestone does not change the expression evaluator itself
- This milestone does not add a global signal bus

## Intended First Slice

1. expose referenceable properties in a stable catalog
2. resolve property targets by path / layer / comp context
3. show the selected target in the property row or inspector
4. allow a drag gesture to capture the target link

## Execution Phases

### Phase 1

- read-only target resolution
- stable target catalog
- inline target display

### Phase 2

- property row / inspector drag gesture
- hover preview for compatible targets
- local link capture without changing keyframe semantics

## Guardrails

- Do not auto-link unrelated properties
- Do not conflate keyframe values and reference links
- Keep the visual link surface local to property / inspector workflows
- Use existing property paths and context resolution

## Cross References

- [Property / Keyframe Integration Plan](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md)
- [Expression System](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md)
- [Inline Interaction Surfaces](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md)
- [Timeline Flat Keyframe View](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md)

## Next Step

1. add a read-only target resolver for property paths
2. define which property types can be linked safely
3. start with the inspector / property row surface before timeline integration
4. use the Phase 2 execution memo for the surface-level work
