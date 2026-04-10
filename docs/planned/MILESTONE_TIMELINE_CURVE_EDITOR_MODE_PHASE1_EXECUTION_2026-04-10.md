# Timeline Curve Editor Mode - Phase 1 Execution

Date: 2026-04-10

## Purpose

Lock down the shortcut and mode contract for the curve editor entry path before building the full curve surface.

This phase is about making the timeline behave predictably when the user wants to switch from normal timeline work into curve editing.

## Contract

- `U` toggles `ArtifactTimelineWidget` between normal timeline mode and curve editor mode.
- `Tab` does not change the global mode by itself; inside curve editor mode, it moves focus between curve rows or interactive subregions.
- `Shift+Tab` moves focus in the reverse direction.
- the current playhead position is preserved across mode changes
- the current selection is preserved across mode changes
- zoom and pan state are preserved across mode changes

## Scope

- mode state in `ArtifactTimelineWidget`
- keyboard routing for `U`, `Tab`, and `Shift+Tab`
- mode-specific focus behavior
- selection and playhead preservation on toggle
- a minimal mode indicator in the timeline chrome

## Out Of Scope

- full curve graph rendering
- parameter color system polish
- solo edit / multi edit UX
- AI tool exposure for curve operations
- any rewrite of the existing timeline orchestration path

## Execution Steps

### 1. Mode State Contract

- add a small mode enum or equivalent state holder to `ArtifactTimelineWidget`
- keep the default mode as the current timeline orchestration path
- make the mode switch explicit in the widget state rather than inferred from selection

### 2. Shortcut Routing

- route `U` only when the timeline has focus
- route `Tab` and `Shift+Tab` inside curve mode so focus stays in the editor surface
- avoid stealing shortcuts when another dock or editor already owns focus

### 3. State Preservation

- preserve playhead time during mode switches
- preserve selected layer or property targets during mode switches
- preserve zoom and horizontal offset so the curve view opens at the user's current context

### 4. Minimal Feedback

- show a small visual mode label or chip so the active mode is obvious
- keep the label subtle enough that it does not compete with the main timeline chrome

## Definition Of Done

- `U` reliably toggles between timeline and curve editor mode
- `Tab` traverses focus inside curve editor mode instead of changing global mode
- playhead, selection, and viewport state survive the switch
- the active mode is visible in the UI

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. add the curve editor surface and colored parameter rows
2. draw same-colored curves for each parameter
3. add solo and multi edit behavior for selected curves

