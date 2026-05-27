# Blend / Mask Composition Contract

**Date:** 2026-05-08
**Phase:** Blend/Mask Smoke Harness Phase 1
**Goal:** stop long-running "nothing visible" failures by making the blend/mask boundary observable without adding fixed visual test panels.

---

## Scope

This contract applies to the Composition Viewer render path and Debug Render Harness reports.

Phase 1 does not add a fixed always-visible scene. It records the current frame's blend/mask state in `FrameDebugSnapshot` so the existing harness report can show where the contract may have broken.

Parent format direction:

- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`

---

## Render Contract

The composition content path is:

1. `layerRTV` is cleared to transparent.
2. A single layer is drawn into `layerRTV`.
3. Any layer-local mask/matte work must be resolved before the blend dispatch consumes `layerSRV`.
4. `layerSRV` is blended with `accumSRV` into `tempUAV`.
5. `accum` and `temp` are swapped only after a successful blend or an explicit fallback.

Current texture contract:

1. `accum` / `temp`: linear float pipeline resources, currently `RGBA32F`.
2. `layer`: graphics layer render target, currently `RGBA8_sRGB`.
3. `OutTex`: storage image matching the float pipeline format.
4. Source alpha is treated as straight alpha on layer input.
5. Accumulated RGB is treated as premultiplied-style accumulated color.

The `layer=RGBA8_sRGB` mismatch is still a documented risk. `PrimitiveRenderer2D` graphics PSOs are currently built for the main `RGBA8_sRGB` render target format, so moving only `RenderPipeline.Layer` to `RGBA32F` can make the layer draw itself disappear. The long-term target is described in `RENDER_FORMAT_CONTRACT_2026-05-16.md`.

If this remains unstable during transition, the next architectural decision is either:

1. make `layer` use the same float linear format as `accum` / `temp`, or
2. keep `layer` as sRGB and add an explicit conversion boundary before compute blend.

---

## Mask Contract

Masks must not silently turn a layer into full transparent output.

Phase 1 report vocabulary:

1. `maskContract=none`: no mask was present on drawn layers.
2. `maskContract=pending`: at least one drawn layer had masks, but the report has not yet measured mask alpha min/max.
3. Future phase: `maskContract=resolved`: mask alpha/range was captured and was non-empty.
4. Future phase: `maskContract=empty`: mask existed and produced zero coverage.
5. Future phase: `maskContract=failed`: mask resource or rasterization failed.

For now, a masked layer appearing in a blank frame is enough to make the harness report say `pending` instead of letting the failure look identical to a blend failure.

---

## Smoke Harness Phase 1 Fields

`FrameDebugSnapshot.resources` gets a text-first resource:

1. `label=Blend / Mask Contract`
2. `type=blendMask`
3. `relation=contract`
4. `note=phase=blend-mask-smoke-v1 ...`

The note records:

1. render path: `gpu-blend` or `fallback`
2. blend contract summary
3. mask contract state
4. pipeline and layer formats
5. drawn layer count
6. non-Normal blend layer count
7. masked layer count and total mask count
8. blend dispatch count
9. Normal retry count
10. failed blend count
11. direct fallback count
12. up to four layer notes for layers with masks or non-Normal blend

---

## Debug Policy

Do not add a permanent fixed viewport overlay just to debug this class of bug.

Preferred path:

1. Use the current composition frame.
2. Record state into `FrameDebugSnapshot`.
3. Surface it through `DebugRenderHarnessWidget` copyable report text.
4. Only add deterministic fixture scenes in later phases if the report proves we need reproducible image comparison.

---

## Next Phase

Phase 2 should add cheap min/max probes, preferably behind harness/debug-only capture:

1. `layerSRV` alpha min/max after layer draw and mask application.
2. `accumSRV` alpha/RGB min/max before blend.
3. `tempUAV` alpha/RGB min/max after blend.
4. Difference between explicit blend failure and successful dispatch producing transparent/solid output.
