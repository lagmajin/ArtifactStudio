# Critical Render / Media Stability Issues

**Date:** 2026-04-30  
**Status:** Open  
**Severity:** Critical  

This report groups long-running fatal issues that can make core editing features unusable.
The goal is to keep investigation, mitigation, and verification in one place while preserving the existing detailed bug notes.

---

## Issue 1: Particle Layer Not Rendering

### Symptom

Particle layers may be completely invisible in the composition editor or appear only in some render states.

### Current Understanding

The particle path has already had at least one concrete GPU failure fixed, but the long-term risk remains broader than a single bug.

Known risk areas:
- render target binding before particle draw
- view / projection matrix consistency
- PSO / SRB initialization failure
- empty particle buffer handling
- blend mode and background-dependent visibility
- split responsibility between `ArtifactParticleLayer`, `ArtifactIRenderer`, and `ParticleRenderer`

### Existing References

- `docs/bugs/BUG_PARTICLE_GPU_RENDER_2026-04-19.md`
- `docs/bugs/PARTICLE_BILLBOARD_NOT_RENDERING_2026-03-26.md`
- `docs/bugs/PARTICLE_BILLBOARD_ROOT_CAUSE_FIX_2026-03-27.md`
- `docs/bugs/PARTICLE_LAYER_STATUS_AND_ISSUES_2026-03-27.md`
- `Artifact/docs/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md`
- `Artifact/docs/MILESTONE_PARTICLE_LAYER_3D_MIGRATION_2026-03-25.md`
- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md`

### Countermeasures

1. Treat particle visibility as a release-blocking smoke case.
2. Add a small particle scene that can be opened and visually verified.
3. Ensure draw diagnostics can distinguish `no particles`, `no RTV`, `PSO null`, `matrix mismatch`, and `blend invisible`.
4. Keep particle drawing inside a renderer-owned entry point with explicit target and matrix contract.
5. Add frame debug summary fields for particle draw calls.

---

## Issue 2: Video Layer Decode Failure / Transparent Output

### Symptom

Video layers may fail to decode, produce null frames, or appear transparent in the composition editor.

### Current Understanding

The video path has several separate failure modes that surface as the same user-visible problem: no video on canvas.

Known risk areas:
- decoder initialization failure being treated as success
- seek failure and stale decoder state
- packet wait timeout after seek
- B-frame / long GOP decode behavior
- `ArtifactVideoLayer::loadFromPath()` setting loaded state before a valid first frame
- async decode racing with synchronous fallback
- frame buffer state read/write without a clear synchronization contract
- QImage / ImageF32x4 conversion path confusion

### Existing References

- `docs/bugs/VIDEO_DECODE_FAILURE_HYPOTHESES_2026-03-23.md`
- `docs/bugs/VIDEO_LAYER_NOT_DISPLAYING_HYPOTHESES_2026-03-27.md`
- `docs/bugs/VIDEO_LAYER_TRANSPARENT_INVESTIGATION_2026-04-25.md`
- `docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md`
- `docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md`
- `ArtifactCore/docs/MFFrameExtractor_Usage.md`

### Countermeasures

1. Treat decode failure as an explicit layer state, not just a null frame.
2. Make `loadFromPath()` success depend on open success plus first-frame or probe success.
3. Separate `opening`, `loaded`, `decode pending`, `decode failed`, and `frame ready`.
4. Add a retry budget and error reason for sync fallback decode.
5. Add a known-good short MP4 smoke asset or generated fixture.
6. Expose decode status in diagnostics and, later, timeline layer specialization.

---

## Cross-Cutting Policy

These issues should be handled as a stability program rather than one-off fixes.

Minimum bar for each critical media/render bug:
- reproducible scenario or explicit "not yet reproducible" note
- owner path and affected files
- current hypothesis list
- first mitigation
- diagnostics needed to prove the mitigation
- regression smoke case

---

## Next Action

Create a critical stability milestone that tracks these two bugs together:

- Particle render path stabilization
- Video decode / layer visibility stabilization

Phase 1 triage ledger:
- `docs/planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_PHASE1_TRIAGE_2026-04-30.md`

Follow-up execution surface:
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_PHASE1_EXECUTION_2026-04-30.md`
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_PHASE2_EXECUTION_2026-04-30.md`
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_PHASE3_EXECUTION_2026-04-30.md`
- `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_PHASE4_EXECUTION_2026-04-30.md`
- `docs/technical/DEBUG_RENDER_HARNESS_SMOKE_CHECKLIST_2026-04-30.md`
- `docs/technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md`
