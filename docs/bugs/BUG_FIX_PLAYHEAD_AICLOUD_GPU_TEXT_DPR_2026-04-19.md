# BUG FIX: Playhead Ghost / AICloud Repaint / GPU Text DPR Distortion

**Date:** 2026-04-19  
**Commit:** `2a41950` (Artifact), `49dd4ec` (ArtifactStudio parent)  
**Files changed:**
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm`
- `Artifact/include/Render/RenderCommandBuffer.ixx`
- `Artifact/include/Render/PrimitiveRenderer2D.ixx`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`

---

## Issue 1: Playhead ghost / black screen loop

### Symptom
Timeline playhead left ghost pixels at its previous position. An earlier attempted fix introduced
`CompositionMode_Clear + fillRect(transparent)` which caused the entire overlay to turn solid black
on every update, creating a "ghost → black screen" loop.

### Root Cause
`TimelinePlayheadOverlayWidget` had `setAttribute(Qt::WA_NoSystemBackground, true)`, which tells Qt
to skip pre-clearing the widget's backing store region before `paintEvent`. This left stale playhead
pixels when the overlay's dirty rect was smaller than the previous draw.

The attempted fix used `QPainter::CompositionMode_Clear` to manually erase the dirty region before
drawing. However, Qt6's backing store in software mode uses `QImage::Format_RGB32` (no alpha
channel). `CompositionMode_Clear` writes ARGB `(0,0,0,0)`, which on an RGB32 surface renders as
opaque black → the entire overlay filled black on every paint.

### Fix
1. **Removed** `setAttribute(Qt::WA_NoSystemBackground, true)` from `TimelinePlayheadOverlayWidget`.
2. **Removed** the `CompositionMode_Clear / fillRect / CompositionMode_SourceOver` block.
3. With `WA_NoSystemBackground` gone, Qt's normal backing store compositing applies: when
   `parent->update(strip)` is called in `syncPlayheadOverlay`, Qt repaints the full z-stack for
   that strip — parent background → `ArtifactTimelineTrackPainterView` (opaque, overwrites the
   ghost) → `TimelinePlayheadOverlayWidget` (draws new playhead on top). The old ghost is
   naturally erased by the track view repaint.

### Why `parent->update` was already correct
`syncPlayheadOverlay` already computed a `strip` covering both the old and new playhead X positions
and called `parent->update(strip)`. This was correct. The ghost persisted only because
`WA_NoSystemBackground` was set, which broke the automatic compositing chain. Removing it makes the
existing update strategy work as intended.

---

## Issue 2: AICloud widget background artifacts on resize

### Symptom
`ArtifactAICloudWidget`'s transcript scroll area showed stale or garbage-colored pixels in newly
revealed regions when the panel was resized or the content was scrolled.

### Root Cause
`transcriptContent_` (the inner widget of the transcript `QScrollArea`) was created with no
`autoFillBackground` and no custom `paintEvent`. When Qt reveals a new region of this widget (e.g.,
on resize), it has no instruction to fill the background, so stale backing store pixels are exposed.

### Fix
After creating `transcriptContent_`:
```cpp
transcriptContent_->setAutoFillBackground(true);
QPalette tp = transcriptContent_->palette();
tp.setColor(QPalette::Window, palette().color(QPalette::Window));
transcriptContent_->setPalette(tp);
```
Qt now fills the background with the correct window color before child widgets are painted,
eliminating stale pixel exposure.

---

## Issue 3: GPU text DPR distortion

### Symptom
GPU-rendered text in the composition view appeared slightly distorted — characters appeared too
large and/or misaligned relative to the text box at display DPR > 1.0 (e.g., 125% Windows scaling).

### Root Cause
`GlyphAtlas::acquire` rasterizes glyph bitmaps via `QRawFont::fromFont(font)` which uses the
screen's **physical** DPI. At DPR = 1.25, a 14pt font produces glyph bitmaps that are 1.25× larger
in pixels than at 100% scaling. The returned `glyph.rect.width`, `glyph.rect.height`,
`glyph.rect.bearingX`, and `glyph.rect.bearingY` are all in **physical pixels**.

However, `TextLayoutEngine::layout` uses `QFontMetricsF` which returns sizes in **logical pixels**.
The layout positions (`item.basePosition`) are therefore in logical pixels.

In `submitGlyphTextTransformed`, the glyph quad is placed in **canvas space** (= logical pixel
space) using:
```cpp
glyphMat.scale(w, h, 1.0f);  // w,h = physical pixels
```
`canvasToNdc` maps 1 logical canvas pixel → DPR physical screen pixels. So a glyph of width
`w_physical` physical pixels is scaled to `w_physical` logical canvas pixels, which maps to
`w_physical × DPR` physical screen pixels — the glyph is `DPR` times too large.

At DPR = 1.0 (physical = logical), this was invisible. At DPR = 1.25, the distortion became
perceptible.

**Secondary issue:** The pan centering formula `panX = (width() × dpr - cw) / 2` mixed logical
viewport width and physical DPR, producing an incorrect canvas offset at DPR ≠ 1.

### Fix

**GlyphTextXformPkt** — Added `float devicePixelRatio = 1.0f` field.

**PrimitiveRenderer2D** — Added `setDevicePixelRatio(float dpr)` method storing DPR in
`Impl::devicePixelRatio_`. In `drawTextTransformed`, the packet is populated with this value.

**submitGlyphTextTransformed** — Before the outline and fill passes:
```cpp
const float invDpr = (p.devicePixelRatio > 0.0f) ? (1.0f / p.devicePixelRatio) : 1.0f;
```
All four physical values are normalized to logical canvas space:
```cpp
const float left = basePosition.x + glyph.rect.bearingX * invDpr;
const float top  = basePosition.y - glyph.rect.bearingY * invDpr;
const float w    = glyph.rect.width  * invDpr;
const float h    = glyph.rect.height * invDpr;
```

**ArtifactCompositionRenderWidget** — 
- `setDevicePixelRatio` is called alongside `setViewportSize` in `showEvent` and in the resize
  debounce timer callback.
- Pan formula corrected: `panX = (width() - cw) / 2` (logical only; the old formula used
  `width() × dpr` which over-offset the canvas at DPR > 1).

### At DPR = 1.0
`invDpr = 1.0f`, so multiplying/dividing by it is a no-op. Behaviour is identical to before.
