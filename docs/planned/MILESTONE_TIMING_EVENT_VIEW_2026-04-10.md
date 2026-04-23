# Timing Event View Milestone

## Goal

Build a lightweight timeline-adjacent widget for event and timing work only.

This surface is intentionally narrower than the main `ArtifactTimelineWidget`:

- event placement and timing adjustments
- zoom and pan over a time ruler
- playhead and marker visualization
- selection and multi-selection
- a compact inspector for the selected event

The goal is to make timing-centric workflows faster for both users and AI-driven automation.

## UX Intent

- a clean event lane instead of full layer orchestration
- a right-side inspector with label, row, and time
- a minimal move control for precise timing nudges
- version navigation for event snapshots
- download/export affordances for generated or reviewed event layouts

## Scope

- a new `TimingEventView` widget and its supporting model
- event snapshot data for read-only inspection
- basic edit actions for timing, label, and row changes
- hooks for future AI tool integration
- doc alignment with `docs/WIDGET_MAP.md`

## Phases

### Phase 1

- define the event model and widget ownership
- create the core viewport, ruler, and playhead rendering
- wire selection and hover states

### Phase 2

- add the compact inspector panel
- add zoom, pan, and precise move behavior
- add version navigation and export affordances

### Phase 3

- expose the model to AI tooling
- add safe mutation helpers and regression tests
- integrate with the wider timeline/project workflow

