# Debug Render Harness Smoke Checklist

**Date:** 2026-04-30  
**Purpose:** Manual verification checklist for the debug render harness presets.

---

## Common Setup

- open the debug render harness surface
- confirm the selected preset name is visible
- confirm `FrameDebugSnapshot` summary is available
- confirm the status path shows `ok`, `skipped`, or `failed`
- confirm the preset switch fully resets previous state

---

## Particle-Only

- add or load the particle-only preset
- verify alive particle count is greater than zero
- verify a particle draw entry appears in frame debug
- verify the output is visible on dark background
- verify the output is still visible on light background
- verify `no RTV` is reported as `skipped`, not a silent blank frame

## Video-Only

- add or load the video-only preset
- verify open success is shown
- verify frame 0 is decoded or clearly marked as failed
- verify a mid-frame seek shows a valid decode state
- verify `decode pending` is distinguishable from `decode failed`
- verify transparent output is reported with an explicit reason

## Blend-Only

- add or load the blend-only preset
- verify both inputs are present
- verify opacity and blend mode are visible in the report
- verify an intentionally empty input path is reported as transparent output

## Overlay-Only

- add or load the overlay-only preset
- verify grid / anchor / guidance overlays render even when media is absent
- verify overlay visibility does not depend on particle or video success

---

## Pass Criteria

- each preset has a visible, named result
- failed cases include an explicit reason
- skipped cases are distinguishable from hard failures
- frame debug report matches what the harness shows on screen
