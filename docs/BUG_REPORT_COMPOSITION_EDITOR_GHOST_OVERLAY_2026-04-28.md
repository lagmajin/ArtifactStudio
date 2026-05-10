# Composition Editor Ghost Overlay Investigation - 2026-04-28

## Symptom

Composition editor sometimes shows a large, faint ghost-like rectangle that
tracks layer position. It appears visually separate from the normal selected
frame / transform gizmo.

This is being treated as a long-running investigation rather than a quick fix,
because several overlay paths intentionally draw faint UI feedback.

## Current Suspects

### 1. Non-selected layer ghost opacity

File:
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

Current layer drawing applies this opacity rule in both GPU and fallback paths:

```cpp
constexpr float kGhostOpacityScale = 0.22f;

const float opacity =
    layer->opacity() *
    ((hasSelection && !isLayerSelected(selectedIds, layer))
         ? kGhostOpacityScale
         : 1.0f);
```

This means: when anything is selected, every visible non-selected layer is still
rendered at 22% opacity. If the selected id list is stale, empty, or out of sync
with `selectedLayerId_`, the intended selected layer can accidentally be drawn as
a faint large ghost at its real layer position.

Why this matches:

- It is large and layer-sized.
- It follows layer transform / position.
- It is faint by design (`0.22f`).
- It is separate from the overlay frame and gizmo.

Open questions:

- Was this dimming behavior intentional for multi-layer focus?
- Should it apply only in a specific isolate/focus mode instead of every
  selection?
- Are `selectedLayerId_` and `ArtifactLayerSelectionManager::selectedLayers()`
  ever briefly out of sync?

### 2. Drop ghost overlay

File:
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`drawViewportGhostOverlay()` draws a dedicated Diligent overlay when
`dropGhostVisible_` is true and `dropGhostRect_` is non-null.

Why this is less likely:

- It should only be active during drag/drop preview.
- It uses viewport-space rectangles, not layer-space transforms.

Why it still needs checking:

- If `clearDropGhostPreview()` is missed on a drag/drop edge case, it could leave
  a large faint viewport overlay behind.

## Important Distinction

There are currently at least three separate visual systems that can look like a
"ghost":

- Layer content dimming via `kGhostOpacityScale`
- Drop preview via `drawViewportGhostOverlay()`
- Selection / bounds overlay via `drawSelectionOverlay()` and older bounding-box
  overlay code

The symptom description most strongly points to layer content dimming, not the
drop ghost overlay.

## Next Low-risk Instrumentation

Add debug logging around the opacity decision, but only when a suspicious dimmed
layer is rendered:

- `hasSelection == true`
- `!isLayerSelected(selectedIds, layer)`
- `layer->id() == selectedLayerId_` or selected list is empty/stale

Useful fields:

- current frame
- `selectedLayerId_`
- `selectedIds`
- layer id / name
- layer opacity
- final opacity
- GPU path vs fallback path

This should confirm whether the faint rectangle is real layer content being
drawn at `0.22f`, without changing behavior yet.

## Candidate Fix Direction

Do not remove the code blindly yet. First decide whether global non-selected
layer dimming is desired.

Likely safer fix:

- Keep normal layer rendering at full opacity.
- Move any "dim non-selected layers" behavior into an explicit focus/isolate
  overlay mode.
- Never let selection bookkeeping affect the actual composition render unless a
  mode explicitly asks for it.

## Resolution Update - 2026-05-08

User confirmation: the plane-layer move ghost is fixed.

The confirmed fix landed in:

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

Changes:

- During active viewport interaction, rubber-band selection, drop ghost preview,
  or transform-gizmo dragging, the render-key cache no longer skips redraws.
  This prevents a stale overlay frame from remaining visible while a layer is
  being moved.
- The 2D transform gizmo draw call is suppressed only while the active handle is
  `Move` and the gizmo is dragging. Other handles still draw normally.
- The extra selected-layer bounding overlay is now gated by both
  `showGizmoOverlay_` and `showGuides_`, so guide-disabled editing does not
  leave a faint frame-like overlay.

Root-cause revision:

The original `kGhostOpacityScale` suspicion is still worth keeping in mind for
focus/isolate rendering behavior, but it was not the primary cause of this
specific plane-layer move ghost. The reproduced/fixed symptom was caused by
interaction overlay drawing combined with cached-frame reuse during movement.

Remaining watch item:

If a similar ghost returns, first check the transform-gizmo draw path and the
render-key early-return conditions before changing layer opacity or blend logic.


## Resolution Update - 2026-05-10: Mask Rasterization Bug (Blank Screen)

**Symptom:**
When a mask is completed in the composition editor, nothing is displayed. The composition editor is filled with a dark gray color with a hint of blue (the default background clear color).

**Root Cause:**
The bug was caused by LOD (Level of Detail) downsampling in ArtifactCompositionViewDrawing.cppm not propagating scale correctly to the mask rasterization logic.
When the composition view renders a mask, processedSurface is downsampled based on LOD. The width and height of the mat (converted from processedSurface) become scaled down. However, the mask's vertices in LayerMask::applyToImage and MaskPath::rasterizeToAlpha were drawn using unscaled local coordinates with an unscaled offset.
As a result, cv::fillPoly rendered the unscaled (much larger) mask path on the downscaled image mask8. The mask path coordinates were heavily offset and drawn completely out of bounds, resulting in a black mask8 (fully transparent alpha).
Since the mask incorrectly made the entire layer transparent, only the default Diligent Engine clear color (a dark grayish-blue) was rendered to the screen.

**Fix:**
Added scaleX and scaleY parameters to MaskPath::rasterizeToAlpha and LayerMask::applyToImage (and their LayerMask::compositeAlphaMask counterparts) in MaskPath.cppm, LayerMask.cppm, MaskPath.ixx, and LayerMask.ixx.
In ArtifactCompositionViewDrawing.cppm and ArtifactCompositionRenderController.cppm's uildRasterizedSurfaceBuffer, calculated scaleX and scaleY as the ratio between mat.cols/mat.rows and localBounds.width()/localBounds.height().
Passed the calculated scales down to pplyToImage and correctly scaled the points, expansion, and feather values in MaskPath::rasterizeToAlpha before passing them to cv::fillPoly.
