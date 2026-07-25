# Preview Playback Performance - Low Level AI Implementation Milestone

**Date**: 2026-05-23

**Status**: Planned low-level implementation slice

**Related**:

- `Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`
- `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_2026-05-31.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`

---

## Goal

Preview playback should stop treating a clock tick as a cached final frame.

The immediate goal is to make playback / timeline / diagnostics agree on one
truth:

**A frame is `ready` only when a final composition preview image exists in RAM or
can be rehydrated from the preview disk cache for the current composition policy.**

This is not the full AE-style RAM preview implementation yet. It is the low-level
cleanup needed before a build queue and guaranteed playback mode can be safe.

---

## Current Observed Boundary

Important current behavior:

1. `ArtifactPlaybackEngine` emits `frameChanged(position, QImage())` as a light
   playback clock tick.
2. `ArtifactPlaybackService` owns early RAM preview state:
   `requested / ready / failed / inRam / onDisk`.
3. `ArtifactCompositionRenderController` and
   `ArtifactCompositionRenderWidget` use `tryGetRamPreviewFrameImage()` as the
   actual image fallback.
4. Timeline and diagnostics read `ramPreviewFrameState()` /
   `ramPreviewSummary()`.

Recent safety fix:

- Empty playback ticks should mark frames as `requested`, not `ready`.
- `ready` should be set only after a concrete non-null frame image is stored in
  RAM, or after disk hydration provides a concrete image.

---

## First Files

Read these before editing:

1. `Artifact/src/Service/ArtifactPlaybackService.cppm`
2. `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
3. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
5. `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
6. `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
7. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

---

## Non-Goals

- Do not rewrite the playback engine thread model in this slice.
- Do not add a full disk cache manifest format yet.
- Do not make layer cache hits mean final frame readiness.
- Do not add new global signal/slot architecture.
- Do not add QtCSS.
- Do not introduce new hot-path `QImage` conversions unless the code is already
  at a Qt presentation boundary.

---

## Implementation Phases

### Phase P1 - Readiness Contract Hardening

Target files:

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

Tasks:

1. Ensure empty `QImage` playback ticks call `markFrameRequested(...)`, not
   `markFrameReady(...)`.
2. Ensure `ramPreviewCacheBitmap()` means `ready && inRam && !failed`.
3. Keep `onDisk` visible separately from `inRam`.
4. Make diagnostics label these fields without implying that `requested` is
   playable.
5. Add a short debug note/reason such as `playback-tick` for clock-only frames.

Done criteria:

- Timeline green cache bars do not appear for frames that only received playback
  ticks.
- App debugger can show requested frames without counting them as ready.
- `tryGetRamPreviewFrameImage()` is the final authority for RAM image fallback.

### Phase P2 - Composition Frame Capture Entry Point

Target files:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Service/ArtifactPlaybackService.cppm`

Tasks:

1. Add or reuse a narrow service API that stores a final rendered composition
   frame into RAM preview state.
2. Call that API only after a composition frame is actually rendered at preview
   quality.
3. Carry composition id, frame number, preview downsample / quality policy, and
   render backend into the cache reason/key path as far as existing structures
   allow.
4. If policy is not fully represented yet, mark the limitation in the reason
   string rather than silently claiming cross-policy validity.

Done criteria:

- A rendered frame can become `ready` without relying on the playback clock.
- A playback clock tick can advance the UI without pretending it produced a
  cached frame.

### Phase P3 - Playback Fallback Policy

Target files:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Service/ArtifactPlaybackService.cppm`

Tasks:

1. Keep RAM preview image fallback disabled while the viewport is interacting.
2. Decide whether playback may use RAM preview while playing:
   - Current conservative policy: use RAM fallback while not playing.
   - Future guaranteed preview policy: use RAM fallback while playing only when
     the requested frame is ready.
3. Add explicit diagnostics for why fallback was or was not used:
   `playing`, `not-ready`, `no-image`, `viewport-interacting`,
   `composition-mismatch`, `frame-out-of-range`.

Done criteria:

- A fully ready range can be distinguished from an uncached live playback range.
- Misses are explainable in frame debug output.

### Phase P4 - Build Queue Skeleton

Target files:

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- Possible new files under `Artifact/src/Service/` and `Artifact/include/Service/`
  only if the service file becomes too large.

Tasks:

1. Add a small composition-scoped RAM preview build queue abstraction.
2. Queue frame requests for work area / preview range.
3. Support cancellation on composition change, frame range change, and preview
   quality change.
4. Keep rendering delegated to existing composition render path; do not create a
   second renderer in this slice unless explicitly approved.

Done criteria:

- The service can represent `requested -> ready/failed` transitions independent
  of playback ticks.
- Queue cancellation leaves diagnostics in a clear state.

---

## Low-Level AI Guardrails

- Before editing, search for all callers of:
  - `markRamPreviewFrameReady`
  - `markRamPreviewFrameRequested`
  - `tryGetRamPreviewFrameImage`
  - `ramPreviewCacheBitmap`
  - `ramPreviewSummary`
- Treat `ArtifactPlaybackEngine` as a clock/audio driver first, not as the owner
  of final preview images.
- Treat `ArtifactPlaybackService` as the owner of composition-level readiness.
- Treat `ArtifactCompositionRenderController` as the owner of final frame
  production.
- Keep changes narrow and additive. The cache system is already spread out, so
  broad renames will make review harder.

---

## Verification

Lightweight verification, only when build/test is explicitly allowed:

1. Build `Artifact` and `ArtifactCore`.
2. Open a composition and play without building RAM preview.
3. Confirm timeline cache bar does not fill just because playback advanced.
4. Pause on a frame that has no RAM image and confirm normal render path is used.
5. Build or store a real preview frame and confirm `tryGetRamPreviewFrameImage`
   can drive fallback.
6. Confirm diagnostics show requested / ready / failed / inRam / onDisk
   separately.

---

## Completion Criteria

- Playback tick no longer implies final frame readiness.
- RAM preview image fallback only uses frames with concrete image data.
- Timeline, footer, app debugger, and frame debug share the same readiness
  vocabulary.
- The next AI can implement the build queue without first untangling readiness
  semantics.

## 2026-07-25 実装監査

`ArtifactPlaybackService` に composition-level の requested／ready／failed／inRam／onDisk 状態、RAM／disk preview cache、manifest／hydration、`ArtifactPlaybackEngine` の clock tick と render controller の実画像取得を分離する基盤、Timeline／diagnostic の summary 経路は確認した。一方、全 surface が同一 readiness vocabulary を表示すること、空 tick が ready を汚染しないことの runtime 検証、build queue／guaranteed playback の実装、実画像を使った cache fallback の実機確認は未実施である。低レベルの状態整理は部分実装済み、completion criteria は未検証とする。
