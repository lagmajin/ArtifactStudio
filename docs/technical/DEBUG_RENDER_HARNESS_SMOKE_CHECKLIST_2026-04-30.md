# Debug Render Harness Smoke Checklist

**Date:** 2026-04-30  
**Purpose:** Manual verification checklist for the debug render harness presets.

---

## Common Setup

- open the debug render harness surface
- confirm the selected preset name is visible
- confirm `FrameDebugSnapshot` summary is available
- confirm `reportId`, `createdAt`, and `viewport` are present in the report
- confirm `status` shows `ok`, `skipped`, or `failed`
- confirm `cacheHealth`, `resourceNotes`, and `skippedReasons` are present
- confirm the preset switch fully resets previous state
- confirm `Copy Report` copies the current text report
- confirm `Save Report` writes a text bundle

---

## Particle-Only

- add or load the particle-only preset
- verify alive particle count is greater than zero
- verify a particle draw entry appears in frame debug
- verify the output is visible on dark background
- verify the output is still visible on light background
- verify `no RTV` is reported as `skipped`, not a silent blank frame
- verify the report includes `particle`, `cacheHealth`, and `traceFrames`

## Video-Only

- add or load the video-only preset
- verify open success is shown
- verify frame 0 is decoded or clearly marked as failed
- verify a mid-frame seek shows a valid decode state
- verify `decode pending` is distinguishable from `decode failed`
- verify transparent output is reported with an explicit reason
- verify the report includes `video`, `shortReason`, and `failureReason` when applicable

## Blend-Only

- add or load the blend-only preset
- verify both inputs are present
- verify opacity and blend mode are visible in the report
- verify an intentionally empty input path is reported as transparent output
- verify the report includes `blend` and `resourceNotes`
- verify the report includes `blendMaskContract`
- verify non-Normal blend layers increase `nonNormal`
- verify masked layers increase `maskedLayers` and set `maskContract=pending`
- verify failed dispatches are reported as `failed` / `directFallback`, not as silent blank output

## Overlay-Only

- add or load the overlay-only preset
- verify grid / anchor / guidance overlays render even when media is absent
- verify overlay visibility does not depend on particle or video success
- verify the report includes `viewportNotes`

## Mixed-Media

- add or load the mixed-media preset
- verify all four scene families contribute to the preview
- verify the report keeps a single consistent `reportId`
- verify the report is still copyable and savable after switching presets

---

## Pass Criteria

- each preset has a visible, named result
- failed cases include an explicit reason
- skipped cases are distinguishable from hard failures
- frame debug report matches what the harness shows on screen
- saved report text matches the on-screen report body
