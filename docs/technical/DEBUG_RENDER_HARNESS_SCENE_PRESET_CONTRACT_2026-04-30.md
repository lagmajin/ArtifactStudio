# Debug Render Harness Scene Preset Contract

**Date:** 2026-04-30  
**Related Milestone:** [`../planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](../planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)

---

## Purpose

This document fixes the minimum inputs and expected outputs for each debug render harness preset.

The intent is to keep the harness small, repeatable, and easy to compare after failures.

---

## Shared Contract

Every preset must expose:

- preset name
- viewport size
- clear/background mode
- `FrameDebugSnapshot`
- draw status
- skipped reason or failure reason
- selected frame number

Every preset must avoid:

- relying on stale state from the previous preset
- silently swallowing `skipped`
- hiding the backend / RTV state

---

## Presets

### particle-only

**Goal**

Show whether particles are emitted and whether they are actually drawn.

**Inputs**

- one particle layer
- one known particle configuration
- dark background
- light background

**Expected outputs**

- alive count
- draw or skip state
- RTV state
- visible output on at least one contrasting background

**Explicit failures**

- `no RTV`
- `empty particle buffer`
- `PSO/SRB invalid`
- `matrix mismatch`

### video-only

**Goal**

Show whether a video layer has a decoded frame and whether the output is visible.

**Inputs**

- one short MP4
- frame 0
- mid-frame seek
- final-frame seek

**Expected outputs**

- open state
- decode state
- source frame / target frame
- visible output or explicit transparent reason

**Explicit failures**

- `open failed`
- `decode pending`
- `decode failed`
- `null frame`
- `fallback miss`

### blend-only

**Goal**

Show whether a blend path produces a visible result when inputs exist.

**Inputs**

- two inputs
- one blend mode
- opacity value

**Expected outputs**

- input count
- blend mode
- opacity
- visible or transparent output

**Explicit failures**

- `empty input`
- `transparent output`
- `missing attachment`

### overlay-only

**Goal**

Show whether overlay guidance remains visible independent of media content.

**Inputs**

- empty media scene
- grid / anchor / guidance enabled

**Expected outputs**

- grid visibility
- anchor visibility
- guidance visibility

**Explicit failures**

- overlay hidden by scene state
- overlay not refreshed after preset switch

### mixed-media

**Goal**

Show a combined scene that exercises particle, video, blend, and overlay in one frame.

**Inputs**

- one particle layer
- one video layer
- one overlay state
- one blend interaction

**Expected outputs**

- all media states present in the report
- explicit skipped reason when a sub-path does not participate

**Explicit failures**

- stale state leaking between sub-paths
- one media path hiding the others without reason

---

## Reset Rule

Switching presets must reset:

- prior draw state
- prior skipped reason
- prior video decode state
- prior particle summary
- prior overlay selection

If a preset cannot fully reset, the report must say so explicitly.

