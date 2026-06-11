# Repository Audit: Editor Interaction

## Purpose

Inspect viewport editing, gizmo interaction, inline editing, and shortcut handling as one interaction stack.

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm`

## Questions to Answer

- Which interactions are modal, and which should be non-modal?
- Where do hit tests or drag state overlap in surprising ways?
- Which editing flows are still split between dialog and in-canvas behavior?

## Deliverable

- A conflict map for mouse, keyboard, and edit-mode transitions.

