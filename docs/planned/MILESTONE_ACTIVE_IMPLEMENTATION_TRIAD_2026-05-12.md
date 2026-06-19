# Active Implementation Triad / May 2026

**Date**: 2026-05-12

This note groups the three current implementation slices that are now ready for direct work.

The goal is to make the next starting point obvious and keep the work order stable across sessions.

---

## Active Slices

### 1. Project Health / Problem View Wiring

- Goal: connect `DiagnosticEngine` and `ProjectDiagnostic` into the app-side validation flow
- Phase 1 memo: 親文書へ統合済み
- Best for: load / save / render preflight validation wiring

### 2. Timeline Keyframe Editing

- Goal: make selected layer keyframes visible and readable on the timeline
- Phase 1 memo: 親文書へ統合済み
- Best for: marker visibility, lane emphasis, header summary

### 3. Composition Editor Mask / Roto Editing

- Goal: stabilize `Mask` tool entry and mode routing before deeper path editing
- Phase 1 memo: 親文書へ統合済み
- Best for: entry bridge, edit-mode routing, state sync

---

## Recommended Order

1. Project Health / Problem View Wiring
2. Timeline Keyframe Editing
3. Composition Editor Mask / Roto Editing

This order keeps the work moving from app-wide validation, to timeline readability, to composition editing entry stability.

---

## Why This Order

- `Project Health` gives a safer validation loop for the other two slices
- `Timeline Keyframe Editing` improves the core animation workflow before deeper editor work
- `Mask / Roto Editing` is easiest to advance once the entry routing is stable

---

## First Move

If starting now:

1. Open `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`
2. Fix the `DiagnosticEngine` / `ProjectDiagnostic` result path first
3. Then move to `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
4. Leave `Composition Editor Mask / Roto Editing` for after the validation path is stable

---

## Working Notes

- `Project Health` is the safest slice to start with because it improves validation for the other two slices
- `Timeline Keyframe Editing` is the animation-facing slice and should follow once validation is stable
- `Mask / Roto Editing` is an editor-routing slice and benefits from the first two being clear
- if a session stalls, return to the Phase 1 memo instead of jumping to the broader milestone

---

## Related Docs

- [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)
- [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)
- [`MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`](./MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [`../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)
