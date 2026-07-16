# Component / Effect Inspector Flat UI Reference

Date: 2026-07-11

## Visual direction

- Use flat charcoal surfaces and existing DCC theme tokens.
- Use spacing and one-pixel separators for hierarchy.
- Reserve the blue accent for selected or enabled state.
- Do not add gradients, QtCSS, or persistent summary badges.

## Component surface

The Component section reads in this order:

1. `+ Add Component` menu
2. vertically stacked enabled-state component cards
3. `Layer Utilities`
4. `Active Component | <name>`
5. active component property editor
6. Generator / Field / Clone Modifier stack lists

The active component name is scoped to the selected layer and follows the
last component or stack item the user focused. When the layer changes, stale
focus is cleared.

## Effect surface

Effects use separate `Stack` and `Editor` pages. Composition targets expose
all five pipeline stages and stage-local add buttons. Layer targets retain the
Rasterizer-only visibility contract. Empty composition stages remain visible
but use a compact list height so the stage order stays readable without
dominating the panel.

The Editor page owns effect identity and enable/bypass state in addition to
the existing property editor. The property editor remains the editing
surface, not the sole navigation model.

## References

- `component-inspector-gradient-reference-2026-07-11.png`
- `component-inspector-flat-reference-2026-07-11.png`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
