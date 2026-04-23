# EasingLab - Phase 2 Execution

Date: 2026-04-21

## Purpose

Implement the visible comparison surface without touching the authoring path yet.

## Current Anchors

- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineWidget.ixx`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

## Work Items

### 1. Add the preview tile widget

- render the easing curve and the moving marker
- keep each tile independent so the grid can reuse it

### 2. Add the dialog shell

- create the grid layout
- add titles and candidate labels
- wire the scrub control to every tile

### 3. Keep the surface read-only

- do not yet change the composition
- do not yet add undo commands
- keep the scope limited to comparison

## Done When

- the lab can be opened and used for visual comparison
- scrubbing affects every tile in sync
- no authoring data is modified yet
