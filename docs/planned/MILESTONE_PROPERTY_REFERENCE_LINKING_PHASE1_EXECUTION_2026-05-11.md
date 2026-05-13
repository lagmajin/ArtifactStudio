# Phase 1 Execution: Property Target Resolution

## Goal

Introduce a read-only property target resolver for AE-style pick-whip linking.

## Scope

- property path resolution
- referenceable target catalog
- inspector / property-row target display
- link state observation without mutating the expression evaluator

## First Slice

1. build a stable list of referenceable property targets
2. resolve targets by layer / comp / property path
3. expose the currently linked target in the UI
4. keep keyframes and references separate

## Guardrails

- No global event bus
- No auto-creation of links
- No changes to keyframe storage
- No new overlay surfaces for the first pass

## Done When

- a property can identify a valid target path
- the UI can show that target path back to the user
- the linking model is still read-only

## Related Docs

- [Property Reference Linking / Pick Whip](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md)
- [Property / Keyframe Integration Plan](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md)
- [Expression System](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_EXPRESSION_SYSTEM_2026-03-29.md)

