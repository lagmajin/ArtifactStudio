# EasingLab - Phase 1: Core Easing Math and Presets

Date: 2026-04-21

## Purpose

Lock down the easing vocabulary and calculation helpers before building the dialog UI.

This phase is intentionally headless. It defines the candidates, the shared formulas, and the data shape that the preview layer and apply path will reuse.

## Scope

- `EasingType` definition
- `EasingCandidate` catalog
- shared easing evaluation helpers
- preset naming and display labels
- mapping to existing `QEasingCurve::Type` / interpolation concepts where useful

## Out Of Scope

- preview widgets
- dialog layout
- timeline integration
- undo/apply wiring
- bezier editing

## Execution Steps

### 1. Define the candidate model

- add `EasingType` if no direct equivalent exists in the current animation layer
- define a lightweight `EasingCandidate` record with display name and type
- keep the candidate list stable and small for the first slice

### 2. Implement easing math

- add `EasingCurveUtil`
- cover linear, ease-in, ease-out, ease-in-out, back, and expo
- keep the API pure and deterministic
- clamp inputs to the canonical `[0, 1]` range

### 3. Map to existing animation concepts

- align the new preset catalog with existing `InterpolationType` / `QEasingCurve::Type` semantics where possible
- document which presets are direct mappings and which are preview-only aliases

## Definition Of Done

- the codebase has a reusable easing math helper
- the candidate catalog is defined in one place
- the formulas are deterministic and ready for preview UI reuse

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. build the preview widget
2. tile multiple candidates in a dialog
3. wire synced scrubbing to the preview surface
