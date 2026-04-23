# EasingLab - Phase 3 Execution

Date: 2026-04-21

## Purpose

Hook the comparison surface back into the timeline and property editing flow.

## Current Anchors

- `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
- `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- `Artifact/include/Property/AbstractProperty.ixx`
- `Artifact/src/Property/AbstractProperty.cppm`

## Work Items

### 1. Selection bridge

- capture the selected keyframe segment from the current timeline context
- keep the selection stable while the dialog is open

### 2. Apply bridge

- translate the candidate back to the stored interpolation / easing value
- commit the change through the existing undoable command path

### 3. Entry point

- expose an action from the timeline or inspector
- keep the entry point lightweight and discoverable

## Done When

- the selected segment can be previewed and updated from the dialog
- apply / undo / redo all work through the existing model
- the UI surface still feels like part of the current editor
