# GPU Readback Notes (2026-03-26)

## Purpose

This note exists to preserve the current GPU readback path before removing it
from the `Composition Viewer` hot path.

`readback` is still useful. The problem is not the existence of the feature,
but using it during normal interactive viewport rendering.

## Current Path

- Viewer-side call:
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - current call site: `cachedBaseCompositeImage_ = renderer_->readbackToImage();`
- Renderer-side implementation:
  - `Artifact/src/Render/ArtifactIRenderer.cppm`
  - `ArtifactIRenderer::Impl::readbackToImage()`

## Why It Is Expensive

The current implementation does all of the following on the render path:

1. Select source texture
2. Copy GPU texture into a staging texture
3. `Flush()`
4. `fence->Wait(...)`
5. `MapTextureSubresource(...)`
6. Allocate a full `QImage`
7. Copy row-by-row into CPU memory

This is a full GPU->CPU synchronization point. It stalls interactive rendering.

## Current Implementation Details

Renderer side summary:

- Reuses a staging texture when width/height match
- Reuses a fence
- Still performs a blocking wait before CPU mapping
- Still allocates a fresh `QImage` for the final CPU image

The staging texture reuse helps a little, but it does not solve the pipeline
stall because the fence wait and CPU copy remain.

## Valid Uses For Readback

Readback is still appropriate for:

- screenshot capture
- debug snapshot export
- CPU-side image analysis
- offline validation / regression capture
- tooling that explicitly requests a CPU image

These are all explicit or low-frequency uses.

## Uses That Should Avoid Readback

Readback should not stay in the default interactive `Composition Viewer` path:

- normal viewport redraw
- pan / zoom
- gizmo hover / selection overlay updates
- frame stepping while previewing
- base composite caching for on-screen redraw

For these cases, the cache should remain on the GPU side.

## Recommended Replacement

Instead of caching a CPU `QImage` copy of the composed frame:

- keep the cached base composite as GPU texture / SRV
- redraw overlays on top of that GPU resource
- only perform readback when an explicit CPU image is requested

This removes the forced GPU->CPU roundtrip from the viewer path.

## Reintroduction Strategy

If readback needs to be reintroduced later, do it behind an explicit control:

- `needsReadback` flag
- screenshot/debug action
- periodic throttled readback
- non-interactive playback state only
- asynchronous readback path if backend support is available

Do not restore unconditional readback in `renderOneFrameImpl()`.

## Migration Rule

Before deleting the current viewer-side call site:

1. keep this note
2. keep `ArtifactIRenderer::readbackToImage()` available
3. move the viewer cache to GPU resources
4. reconnect readback only for explicit tools and diagnostics
