# 2D / 3D Gizmo Camera Coordinate Contract

> 2026-07-16
>
> **Status:** Active implementation contract

## Purpose

Prevent regressions where a 3D gizmo is rendered, picked, or dragged with the
2D composition pan/zoom coordinate system.

## Ownership

| Concern | 2D gizmo | 3D gizmo |
|---|---|---|
| Primary widget | `TransformGizmo` | `Artifact3DGizmo` |
| Render path | `PrimitiveRenderer2D` | `PrimitiveRenderer3D` |
| Coordinate source | composition pan / zoom | active 3D camera or viewport-orientation camera |
| Projection | composition orthographic projection | active camera projection or viewport-orientation projection |

## Non-Negotiable Invariant

For a selected 3D layer, the following three operations must use the same
view/projection pair within a frame:

1. 3D Gizmo rendering
2. Mouse position to picking-ray conversion
3. Gizmo hit testing and drag updates

Using the 2D pan/zoom matrices for any one of these creates a mismatch between
what the user sees and what can be selected or dragged.

## Current Implementation

`CompositionRenderController::Impl` caches the camera pair selected for 3D
layer rendering:

- Active `ArtifactCameraLayer` camera matrices when a 3D camera is available.
- Viewport-orientation camera matrices when that mode is active.
- The existing 2D fallback only when neither 3D camera source is available.

The cached pair is consumed only by the 3D Gizmo path. The 2D `TransformGizmo`
and its pan/zoom behavior must not be changed by 3D Gizmo work.

## Change Checklist

Before modifying 3D Gizmo drawing or interaction, verify:

- [ ] `Artifact3DGizmo::draw()` receives the active 3D camera pair.
- [ ] `createPickingRay()` uses the same pair for 3D Gizmo interaction.
- [ ] 3D hover hit testing uses the same pair.
- [ ] The fallback remains scoped to the 3D Gizmo path.
- [ ] No 2D `TransformGizmo` condition, pan, zoom, or projection behavior changed.
- [ ] A 3D layer is checked with both an active camera and viewport-orientation mode.

## Scope Boundary

This contract fixes coordinate-system parity only. It does not promise complete
3D transform parity: Z scale persistence, undo grouping, snapping, and
multi-selection require separate work and must not be silently folded into a
camera-matrix change.

## Related Files

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/PrimitiveRenderer3D.cppm`
- `docs/bugs/GIZMO3D_COUNTERMEASURES_2026-03-26.md`
