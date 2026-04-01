# Unused Widget Candidates

Date: 2026-03-28

This note collects widget implementations that are not currently wired into the active timeline left pane,
but are plausible cleanup targets later.

## Candidate

- `Artifact/src/Widgets/Timeline/ArtifactLayerHierarchyWidget.cppm`
  - `ArtifactLayerHierarchyView` is a separate `QTreeView`-based implementation.
  - The active timeline left pane is the owner-drawn `ArtifactLayerPanelWidget` in
    `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`.
  - This file is not currently the live left-pane implementation.

## Cleanup idea

- Keep only one active layer-reorder surface in the timeline UI.
- If the hierarchy view is not used by any other screen, consider moving it into a dedicated cleanup step.
- If it is still needed for experiments, keep it behind an explicit feature gate or test-only entry point.
