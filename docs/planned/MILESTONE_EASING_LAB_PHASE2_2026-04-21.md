# EasingLab - Phase 2: Preview Widget and Dialog Layout

Date: 2026-04-21

## Purpose

Build the comparison surface that lets the user see multiple easing candidates side by side.

This phase introduces the actual `EasingLab` view, but keeps it read-only.

## Scope

- `EasingPreviewWidget`
- `EasingLabDialog`
- tiled preview layout
- synchronized scrubber
- minimal headers and labels

## Out Of Scope

- apply-to-timeline wiring
- undo stack changes
- editor mode switches
- per-curve manual editing

## Execution Steps

### 1. Build the preview widget

- draw a single easing candidate with `QPainter`
- keep the widget lightweight and self-contained
- visualize both motion and curve shape if space allows

### 2. Build the dialog

- tile at least six candidates in a grid
- keep the dialog readable on smaller windows
- show a top scrub control that drives all previews together

### 3. Keep previews synchronized

- use one normalized time value for all candidates
- make scrubbing deterministic and stable
- avoid creating heavyweight clones of the composition

## Definition Of Done

- the dialog opens and shows the initial preset set
- previews animate in sync with scrubbing
- the surface is read-only and stable

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. connect the lab to the selected keyframe segment
2. add apply logic through the existing command/undo path
3. expose the lab entry point from timeline or inspector
