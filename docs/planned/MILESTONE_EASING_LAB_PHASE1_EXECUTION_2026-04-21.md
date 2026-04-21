# EasingLab - Phase 1 Execution

Date: 2026-04-21

## Purpose

Create the easing math and candidate catalog as a shared core that the UI can consume.

## Current Anchors

- `ArtifactCore/include/Geometry/Interpolate.ixx`
- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`

## Work Items

### 1. Add a shared easing helper

- create `EasingCurveUtil` in `ArtifactCore`
- expose functions for the first preset set
- keep it independent from any widget code

### 2. Define the preset catalog

- declare the initial six candidates
- give each candidate a stable display name
- keep the catalog ordered for preview tiling

### 3. Add compatibility notes

- note how the new presets relate to current interpolation modes
- note which existing `InterpolationType` values can be directly applied

## Done When

- there is a single place to evaluate easing curves for preview
- the preset list is available to the dialog layer
- no UI code is required to use the helper
