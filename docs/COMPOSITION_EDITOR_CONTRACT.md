# Composition Editor Contract

This document captures the operating rules for the `ArtifactCompositionRenderWidget` / `ArtifactCompositionRenderController` boundary and the surrounding composition editor surface.

## Scope

- Composition viewport rendering
- Overlay and HUD drawing
- Direct manipulation input routing
- Playback / frame synchronization
- Extension points for editor tools

## Current Implementation Map

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - composition editor shell and viewport host
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - controller-facing state, frame dispatch, and viewport coordination
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`
  - render surface interface
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
  - diagnostic surface that reads the same frame snapshot contract
- `Artifact/src/Widgets/Diagnostics/DebugRenderHarnessWidget.cppm`
  - smoke harness that exercises the same reporting vocabulary

## Active Workstream

The current May 12 execution triad is ordered as follows:

1. `Project Health / Problem View Wiring`
2. `Timeline Keyframe Editing`
3. `Composition Editor Mask / Roto Editing`

The first two slices primarily improve validation and readability. The third slice remains a shell/controller-bound mode-routing task, so it should stay local to the editor boundary and not spill into unrelated widget wiring.

## Responsibilities

### Composition Editor Shell

- Owns the visible editor surface, transport, and high-level tool context.
- Coordinates playback state, selection state, and viewport state.
- Decides when the editor is in interactive, modal, or playback-driven mode.
- Should remain the only place that decides the active tool family.

### Render Controller

- Owns renderer-facing state such as viewport size, zoom, pan, and frame dispatch.
- Bridges editor state to `ArtifactIRenderer` without reinterpreting layer semantics.
- Keeps the render path thin and explicit.
- Must not synthesize business rules from raw input events.

### Renderer

- Draws composition content, overlays, and editor annotations.
- Must not own editor business rules.
- Must not infer selection or tool state from global UI objects.
- Must only receive explicit, already-resolved state.

## State Machine

The editor should be treated as a small set of explicit modes.

- `Idle`
  - no active drag or modal tool
- `Interactive`
  - selection, move, zoom, and hover feedback are active
- `Modal.Transform`
  - drag-based transform session owns pointer input
- `Modal.Mask`
  - mask edit session owns pointer input
- `Modal.Pen`
  - pen / roto session owns pointer input
- `Modal.PlaybackScrub`
  - scrub / step interaction owns pointer input
- `PlaybackDriven`
  - playback clock drives the visible frame position

Rules:

- Only one modal tool should own direct manipulation at a time.
- Input routing should remain local to the editor surface and its controller.
- Playback changes should update the visible frame without rewriting tool state.

## Render Pass Contract

The editor render path is ordered:

1. Clear / background
2. Composition content
3. Direct manipulation overlays
4. HUD / guides / helper text
5. Optional diagnostic annotation

Rules:

- Overlay drawing must remain visually separate from content drawing.
- Business logic such as selection resolution should happen before the draw call.
- The renderer should receive already-resolved state, not raw UI events.
- New overlay types should reuse the same render contract rather than branching into widget-specific paint code.

## Event Flow

- Mouse / keyboard input enters the editor widget.
- The editor resolves tool context and current mode.
- The controller converts the resolved state into renderer-facing state.
- The renderer draws the result.

Do not introduce new global event buses for routine editor gestures.
Prefer the existing controller / service path and keep the ownership local.

## Operational Rules

- Keep editor state transitions explicit and local.
- Prefer existing services over one-off widget wiring.
- Preserve the current render order unless a new overlay stage is justified.
- Treat selection, transform, and playback as separate concerns even when they share the same surface.
- Use frame snapshots and report text for diagnosis instead of adding ad hoc logging surfaces.

## Extension Points

- New overlay annotations should plug into the overlay stage.
- New direct manipulation tools should own their modal state explicitly.
- New render debug hooks should be readable from `FrameDebugSnapshot`.
- New display-only helpers should stay out of layer mutation logic.
- Blend/mask debug hooks should follow `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md` and surface through existing report text rather than permanent viewport fixtures.

## Guardrails

- Do not add QtCSS styling for editor contracts.
- Do not introduce new `QColorDialog` dependencies for editor color work.
- Do not expand the signal surface unless the current path cannot express the behavior.
- Keep `QImage` out of hot-path editor state unless it is a clear boundary artifact.
- Avoid hidden conversions between preview state and render state.
- Do not couple the editor contract to debug-only widgets.

## Contributor Checklist

- Identify the owner of the behavior before editing.
- Check whether the change belongs in shell, controller, or renderer.
- Keep the render contract ordered and explicit.
- Reuse the existing snapshot vocabulary when adding diagnostics.
- Stop and document the boundary if a change would require a new global signal.

## Current Boundary Note

2026-05-11 時点では、composition editor contract の中心は viewport / overlay / modal input routing にある。

- blend / mask の不具合は permanent overlay で隠さず、`FrameDebugSnapshot` と report text で追う
- `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md` を、mask / blend smoke の観測契約として参照する
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md` の inline choice は layer panel 側の責務として扱い、`Modal.Mask` の編集責務とは分ける
- `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md` の主題は shape / roto / vertex editing に固定し、mask parameter の時間化は別スライスに分離する
- property relation / pick-whip は editor viewport ではなく property / inspector 側で扱う
- mask の時間化は editor contract ではなく `docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` 側で進める
- editor contract は shell / controller / renderer の責務境界を維持し、診断は既存の report 経路に寄せる

## Next Step

1. `Modal.Mask` の入力責務を shell/controller に閉じたまま、mask debug report の文言を整理する
2. layer panel の inline choice と mask editing を混同しないようにする
3. mask parameter の time-addressable 化は `Mask Keyframe Foundation` の Phase 1 へ分離する
4. render path 側では、blend / mask の観測値を `FrameDebugSnapshot` に記録するだけに留める
