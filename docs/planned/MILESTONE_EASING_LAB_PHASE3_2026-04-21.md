# EasingLab - Phase 3: Integration and Apply Path

Date: 2026-04-21

## Purpose

Connect the lab to the existing selection and command system so the chosen easing can be applied back to the actual keyframe segment.

## Scope

- selection extraction from timeline or property UI
- apply command path
- undo / redo compatibility
- launch entry point from existing UI

## Out Of Scope

- advanced curve editing
- multi-segment batch editing
- AI recommendations
- any rewrite of the current timeline model

## Execution Steps

### 1. Read the selected segment

- identify the selected keyframe segment from the current surface
- preserve the current selection context when opening the lab

### 2. Apply easing through the existing command path

- update the actual keyframe interpolation / easing value
- route the change through undoable commands
- keep the current composition state intact

### 3. Expose the entry point

- add the lab launch entry to the timeline or property surface
- keep the entry point discoverable but unobtrusive

## Definition Of Done

- the lab can open from the existing editor surface
- applying a candidate updates the selected keyframe
- undo and redo remain valid

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. extend the preset set if needed
2. add per-segment comparison metadata
3. decide whether the lab should also offer bezier-edit shortcuts
