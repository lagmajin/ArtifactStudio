# Critical Render / Media Stability - Phase 1 Triage Ledger

**Status:** Partial — failure buckets and first-signal guidance are documented; compact state reports, fixtures, and explicit regression-gate execution remain pending.

**Date:** 2026-04-30  
**Parent:** [`MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](./MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)  
**Bug Report:** [`../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)

---

## Purpose

This ledger keeps the two fatal issue families actionable:

- particle layer not rendering
- video layer decode failure / transparent output

The immediate goal is not a broad rewrite. The goal is to make every failure land in a named bucket with an observable signal.

---

## Issue Ledger

| ID | Area | User Symptom | Current Class | Owner Path | First Signal |
|---|---|---|---|---|---|
| CRM-001 | Particle | Particle layer exists but no particles are visible | Intermittent / state-dependent | Render path | `[ParticleLayer]` alive count and `[ParticleRenderer]` draw / skip logs |
| CRM-002 | Particle | GPU path silently skips particle draw | Known failure mode | Renderer target setup | `[ParticleRenderer] No active RTV` |
| CRM-003 | Particle | Particle draw initialized but nothing appears | Needs fixture | Renderer / shader / blend | PSO/SRB state, blend mode, background contrast |
| CRM-004 | Video | Video layer imports but frame is null | Intermittent / file-dependent | Media decode | `[MediaPlayback]` direct decode fail / null frame |
| CRM-005 | Video | Video layer is transparent in composition | Intermittent / render-path dependent | Layer state / composition blend | `[VideoLayerT]` has buffer, QImage null, fallback result |
| CRM-006 | Video | Seek or scrub causes decode failure | File-dependent | Media seek / decoder state | seek failure log, requested frame, source frame |

---

## Current Status

- `DebugRenderHarnessWidget` now captures a text bundle with `reportId`, `createdAt`, `viewport`, `particleDetail`, `blendMaskContract`, `shortReason`, `cacheHealth`, `resourceNotes`, and `skippedReasons`.
- `DebugRenderHarnessWidget` can copy or save the bundle without adding new global wiring.
- `AppDebuggerWidget` already exposes RAM preview stats in `ready / requested / cached / hit` terms.
- `ArtifactCompositionViewerFooter` mirrors the same RAM preview diagnostics for quick inspection.
- The particle stabilization milestone already calls out `no RTV`, `PSO null`, and empty-buffer summary coverage as the next diagnostic target.
- 2026-06-03 review: the live harness bundle already exposes `particleState`, `videoState`, `particleDetail`, `blendMaskContract`, `shortReason`, and the same snapshot vocabulary used by `AppDebuggerWidget`.
- 2026-06-03 review: the harness summary line now surfaces `particleState=...`, `particleDetail=...`, `videoState=...`, `textState=...`, `blendState=...`, and `glyphState=...`.
- 2026-06-03 review: the App Debugger export text now surfaces `particleState`, `particleDetail`, `textState`, `videoState`, `blendState`, `blendMaskContract`, and `glyphState` explicitly alongside the frame snapshot JSON.
- 2026-06-03 review: the smoke checklist now treats both the harness report and App Debugger export text as part of the pass gate.
- 2026-06-03 review: `FrameDebugSnapshot` already carries `failureReason`, and the composition render controller records particle debug notes into snapshot resources.
- 2026-06-03 review: the particle renderer now exposes an explicit `state=` label for `empty`, `device-null`, `invalid-viewport`, `queued`, `no-rtv`, `prepared`, `draw-skipped`, and `drawn`.
- 2026-06-03 review: the video layer now exposes an explicit `state=` label in `decodeState()` / `debugState()` for `not-loaded`, `opening`, `cached`, `decode-pending`, `ready`, `open-failed`, `range-rejected`, `decode-failed`, `sync-fallback-ready`, and `sync-fallback-miss`.
- 2026-06-03 review: the remaining work is to make the particle/video failure buckets explicit enough for smoke gating, not to build the first report surface.

---

## Existing Diagnostics

### Particle

Existing useful signals:

- `Artifact/src/Layer/ArtifactParticleLayer.cppm`
  - `[ParticleLayer]` frame number, alive particle count, renderer state
- `Artifact/src/Render/ArtifactIRenderer.cppm`
  - `[ParticleRenderer] Initialized`
  - `[ParticleRenderer] Drawing`
  - `[ParticleRenderer] No active RTV`
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
  - PSO creation success / failure
  - missing constant buffer warning
  - prepare skipped when PSO/SRB is null

Gaps:

- no single per-frame particle draw summary
- no explicit matrix mode / coordinate-space summary
- no visible smoke fixture recorded in repo docs
- no regression gate that says "particle layer must be visible after add/scrub/play"

### Video

Existing useful signals:

- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
  - load path and initial decode failure
  - `[VideoLayerT] decodeCurrentFrame starting bg decode`
  - async decode null frame
  - sync fallback success / failure via `decodeFrameToQImage`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - `[VideoLayerT] drawLayerForCompositionView`
  - has frame buffer / fallback path visibility
- `ArtifactCore/src/Media/MediaPlaybackController.cppm`
  - direct decode invalid state
  - direct decode seek failure
  - direct decode packet / frame failure
  - FFmpeg / MF backend open and decode warnings

Gaps:

- no explicit user-facing decode state enum
- `loaded`, `decode pending`, `decode failed`, and `frame ready` are still inferred from several fields
- no regression gate for import/open/seek/frame visibility

### Harness and Report Surface

Existing useful signals:

- `Artifact/src/Widgets/Diagnostics/DebugRenderHarnessWidget.cppm`
  - smoke report text bundle
  - preset summary, report identity, viewport, status, and capture notes
  - clipboard / file save path for regression bundles
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
  - `ramPreview ready / requested / cached / hit`
  - frame debug snapshot summary
- `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
  - same preview stats in the footer for at-a-glance review

Gaps:

- no single documented smoke checklist that ties the harness to particle / video stability outcomes
- no report bundle schema for attaching a repro bundle to a bug update

---

## Phase 1 Actions

### A. Keep Particle Work in Diagnostic Mode First

Do this before changing rendering behavior:

- capture a particle frame where alive count is greater than zero
- confirm whether `[ParticleRenderer] Drawing` appears
- if drawing appears but nothing is visible, record blend mode, background, particle color, and matrix mode
- if drawing does not appear, record which skip condition triggered

### B. Keep Video Work in State Classification First

Do this before changing decode policy:

- record whether open succeeded
- record whether first frame decode succeeded
- record requested frame and source frame during failure
- record whether async decode is pending when composition draw asks for a frame
- record whether sync fallback was suppressed by previous failure

### C. Prepare Smoke Cases

Particle smoke case:

- new composition
- add one particle layer
- scrub to frame 10 and frame 30
- verify alive count > 0
- verify renderer draw call appears
- verify visible output over dark and light backgrounds

Video smoke case:

- short MP4, 1-2 seconds from `../technical/CRITICAL_RENDER_MEDIA_SMOKE_FIXTURE_2026-04-30.md`
- import as video layer
- verify frame 0 decode
- seek to middle frame
- verify composition draw path receives a frame buffer
- verify no silent transparent output

---

## Next Implementation Candidates

1. Add a compact `VideoLayer` decode state report method for diagnostics.
2. Add a compact particle draw summary that reports count / RTV / PSO / SRB / blend mode.
3. Document or generate a tiny MP4 smoke fixture.
4. Add a manual smoke checklist under `docs/verification`.
5. Convert the current harness report into an explicit regression gate for particle / video smoke outcomes.
