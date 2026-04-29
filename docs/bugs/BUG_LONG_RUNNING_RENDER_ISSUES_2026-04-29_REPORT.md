# Long-Running Render Issues Report - 2026-04-29

## 1. Mask + Blend single-color fill

### Status
- GPU blend path is kept enabled.
- The temporary GPU bypass was removed.

### Most likely root cause
- Several mask / matte paths were reducing only `alpha` while leaving RGB untouched.
- That breaks premultiplied RGBA consistency and can show up as flat or solid-color fills once the GPU blend path composites the surface.

### Fixed / aligned paths
- `Artifact/src/Mask/LayerMask.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Render/ShaderManager.cppm`

### Notes
- The fix was applied at the source of the mask / matte application rather than disabling GPU blending.

## 2. Preview hitching on light scenes

### Status
- Still being reduced.

### What changed
- More state-change paths now call `markRenderDirty()` instead of `renderOneFrame()` directly.
- `ArtifactRenderLayerWidgetv2.cppm` was moved further toward `requestRender()` for confirm-style updates.
- `ArtifactCompositionRenderWidget.cppm` now wakes the render loop from `requestRender()` instead of relying on fixed idle sleeps only.

### Remaining risk
- Live drag / hover updates still render immediately where the UI needs responsiveness.
- Those paths should be revisited only if profiling still shows stalls.

## 3. Playhead / ruler / work-area consistency

### Status
- Already aligned in the timeline work.

### Notes
- The current work focused on keeping frame clamping and ruler coordinates consistent across timeline widgets.

## 4. Text layer route

### Status
- `layoutMode` / point text vs box text direction is in place.
- This is separate from the render-hitch work, but it touched the same preview pipeline.

## 5. Follow-up

### Recommended next checks
1. Verify the mask / matte fix on the original repro for `mask + blend`.
2. Profile preview interaction after the new dirty / requestRender coalescing.
3. Revisit only the live drag paths if there is still visible lag.

### Not done
- No build or runtime test was run in this turn.
