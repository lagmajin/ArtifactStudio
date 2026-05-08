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

Current diagnostics:
- `DebugRenderHarnessWidget` can bundle a particle smoke report with `reportId`, `createdAt`, `viewport`, `cacheHealth`, `resourceNotes`, and `skippedReasons`.
- `AppDebuggerWidget` already exposes RAM preview stats and a harness tab using the same frame snapshot vocabulary.
- `ArtifactCompositionViewerFooter` mirrors the RAM preview stats for a fast at-a-glance check.
- A preferred short MP4 fixture and generation command are now documented in `docs/technical/CRITICAL_RENDER_MEDIA_SMOKE_FIXTURE_2026-04-30.md`.

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

Current diagnostics:
- `DebugRenderHarnessWidget` can capture a video smoke bundle with the same report shape as particle cases.
- The report bundle keeps `shortReason`, `failureReason`, `cacheHealth`, and `skippedReasons` together.
- `AppDebuggerWidget` and the frame debug surfaces already use the same snapshot vocabulary for comparison.
- The preferred short MP4 fixture is documented for smoke use and fallback path checks.

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

## Issue 3: Non-Normal Blend Solid Fill / Invisible Composition

### Symptom

Composition Viewer may become a flat solid fill, or all layer detail may disappear, when an upper layer is changed from `Normal` to another blend mode such as `Add`.

The 2026-05-08 screenshot shows two plane layers:

1. Upper layer: `ホワイト 平面 1`, `Blend: Add`
2. Lower layer: `salmon 平面 1`, `Blend: Normal`

Instead of showing the expected additive result, the viewer becomes a mostly uniform bluish fill.

### Current Understanding

This is now treated as a long-running composition stability issue, separate from video decode and particle visibility.

Known risk areas:

1. GPU blend compute shader storage format and render-pipeline texture format must match across DX12/Vulkan.
2. The intermediate `accum` / `temp` ping-pong textures are float pipeline resources, while the per-layer render target may still be a different/sRGB-oriented format.
3. The blend shader currently assumes a premultiplied-ish accumulation contract: `dst.rgb` is already accumulated color, while `src.rgb` is multiplied by `srcA`.
4. A plane/video fallback that draws a full-frame solid source can make `Add` look like a viewport-wide fill even when the compute pass itself succeeds.
5. Current fallback only catches explicit blend failure. It does not detect a successful dispatch that produced a visually invalid solid result.

### Recent Mitigation

The Vulkan validation warning for `OutTex` format mismatch was addressed by aligning the compute shader storage image declaration and pipeline texture format:

1. `RenderConfig::PipelineFormat` is `TEX_FORMAT_RGBA32_FLOAT`.
2. `LayerBlendComputeShader.ixx` declares `OutTex` as `RWTexture2D<float4>`.

The composition render controller also retries failed non-Normal blends with `Normal`, then falls back to direct sprite draw if the compute path reports failure.

2026-05-08 Phase 1 diagnostic work adds a `Blend / Mask Contract` resource to `FrameDebugSnapshot` and the Debug Render Harness text report. This is intentionally not a fixed viewport test scene; it reports the current composition frame using the contract in `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`.

### Next Countermeasures

1. Add a deterministic two-plane smoke scene: lower salmon Normal, upper white Add.
2. Capture frame debug fields for `layerSRV`, `accumSRV`, `tempUAV`, blend mode, opacity, and texture formats for every blend dispatch.
3. Add a low-cost solid-output detector in the debug harness or frame snapshot, not directly in the hot render path.
4. Decide whether `RenderPipeline.Layer` should use the same float pipeline format as `accum` / `temp`, or whether a documented sRGB-to-linear conversion boundary is required.
5. Keep the Diligent backend changes minimal until the above smoke case proves the exact boundary.

### Existing References

- `docs/planned/MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md`
- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md`
- `docs/technical/DX12_VULKAN_PARITY_MEMO_2026-05-07.md`

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

---

## Immediate Use

When investigating either issue family, start with:

1. `DebugRenderHarnessWidget` smoke report
2. `AppDebuggerWidget` frame snapshot and RAM preview stats
3. the relevant stabilization milestone phase notes
4. the matching smoke checklist
