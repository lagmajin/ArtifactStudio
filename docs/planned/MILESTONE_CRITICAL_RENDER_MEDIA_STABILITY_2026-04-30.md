# Milestone: Critical Render / Media Stability Program

**Date:** 2026-04-30  
**Status:** In Progress
**Priority:** Critical  
**Bug Report:** [`../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)

---

## Goal

Particle invisibility and video decode failure are user-visible fatal bugs.
This milestone treats them as a long-running stability program with diagnostics, smoke cases, and regression gates.

---

## Scope

### In

- particle layer visibility failures
- video layer decode / transparent output failures
- render path diagnostics for skipped or failed draws
- media decode state reporting
- small smoke scenes or fixtures for regression checks

### Out

- particle feature expansion
- full 3D particle migration
- video proxy workflow redesign
- QImage retirement
- codec feature expansion beyond decode stability

---

## Phase 1: Triage Ledger

Create a single working ledger for critical render/media bugs.

Execution ledger:
- [`MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_PHASE1_TRIAGE_2026-04-30.md`](./MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_PHASE1_TRIAGE_2026-04-30.md)

- collect active references from `docs/bugs`
- classify each issue as `reproducible`, `intermittent`, or `needs fixture`
- mark current owner path: particle render, media decode, layer state, or composition blend
- define the first observable signal for each issue

Done when:
- particle and video issues have a current status line
- each issue points to a concrete diagnostic or missing diagnostic

---

## Phase 2: Diagnostics First

Make failure states visible before broad rewrites.

- particleState: report count, target availability, PSO/SRB state, matrix mode, blend mode
- videoState: report open state, decode state, last error, target frame, source frame, fallback result
- composition: report layer skip reason and blend input emptiness

Done when:
- a failed particle draw is distinguishable from an empty particle system
- a failed video decode is distinguishable from an invisible or inactive layer

---

## Phase 3: Smoke Cases

Add small reproducible checks.

Manual checklist:
- [`../technical/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-04-30.md`](../technical/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-04-30.md)
- [`../verification/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-06-03.md`](../verification/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-06-03.md)

- particle scene with visible bright particles over dark and light backgrounds
- short MP4 decode case with frame 0 and mid-frame checks
- video layer visibility case through composition render path

Done when:
- the project has documented smoke steps
- regression can be checked without guessing which subsystem failed

---

## Phase 4: Targeted Fixes

Only after diagnostics and smoke cases are clear, apply fixes.

- particleState: renderer-owned draw contract and target/matrix setup
- videoState: explicit decode state and first-frame/open success boundary
- composition: keep skip reasons and blend inputs observable

Done when:
- fixes are linked back to the bug ledger
- each fix has a smoke case or verification note

---

## Phase 5: Regression Gate

Promote the smoke checks into a recurring release gate.

- particle visible after add/scrub/play
- video frame visible after import/open/seek
- no silent null frame loop
- no silent draw skip without a reason

Done when:
- critical render/media bugs have a repeatable check before release

## Related Execution Surface

- [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)
- [`../verification/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-06-03.md`](../verification/CRITICAL_RENDER_MEDIA_SMOKE_CHECKLIST_2026-06-03.md)

---

## Related

- [`../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](../bugs/BUG_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)
- [`../../Artifact/docs/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md`](../../Artifact/docs/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md)
- [`MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md`](./MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md)
- Phase 1 execution memo is absorbed into the parent milestone
