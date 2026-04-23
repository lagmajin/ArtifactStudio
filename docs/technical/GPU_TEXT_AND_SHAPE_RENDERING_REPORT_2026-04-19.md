# GPU Text and Shape Rendering Report

**Date**: 2026-04-19  
**Scope**: `Artifact` / `ArtifactCore` rendering path for text and shape layers

## 1. Goal

Move text and shape rendering away from `QPainter` rasterization as much as possible and make the regular viewport path use GPU primitives and glyph atlas rendering.

## 2. Current State

### 2.1 GPU Text

The GPU text path is mostly scaffolded and now links correctly.

Implemented pieces:

- `Artifact/include/Render/ArtifactIRenderer.ixx`
  - `drawTextTransformed(...)` is declared
- `Artifact/src/Render/ArtifactIRenderer.cppm`
  - `drawTextTransformed(...)` delegates to `PrimitiveRenderer2D`
- `Artifact/include/Render/PrimitiveRenderer2D.ixx`
  - `drawTextTransformed(...)` declaration exists
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
  - builds a `GlyphTextXformPkt` and pushes it into the command buffer
- `Artifact/include/Render/RenderCommandBuffer.ixx`
  - `GlyphTextXformPkt` was added
- `Artifact/include/Render/DiligentImmediateSubmitter.ixx`
  - `submitGlyphTextTransformed(...)` was added
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
  - submits transformed glyph quads using the glyph atlas

Text layer integration:

- `Artifact/src/Layer/ArtifactTextLayer.cppm`
  - plain text can now go through the GPU path directly
  - rich text, paragraph spacing, and other complex layout cases still fall back to the old raster path
  - shadow and stroke are approximated by multiple GPU passes

Public API type alignment:

- `Artifact/include/Layer/ArtifactTextLayer.ixx`
  - public color API now uses `ArtifactCore::FloatColor`
- `Artifact/src/Layer/ArtifactTextLayer.cppm`
  - internal storage still uses `FloatRGBA`
  - conversion is done at the boundary

The link error for `ArtifactIRenderer::drawTextTransformed(...)` has been resolved.

### 2.2 GPU Shape

Shape rendering has been moved toward GPU primitives, but the path is less stable than text.

Relevant pieces:

- `ArtifactCore/include/Shape/ShapePath.ixx`
- `ArtifactCore/src/Shape/ShapePath.cppm`
- `ArtifactCore/src/Shape/ShapeGroup.cppm`
- `ArtifactCore/src/Shape/ShapeLayer.cppm`
- `Artifact/src/Layer/ArtifactShapeLayer.cppm`

Current behavior:

- basic shape data and `ShapePath` infrastructure exist
- `ArtifactShapeLayer` was experimented with both as:
  - cached raster sprite rendering
  - direct GPU primitive rendering
- direct GPU rendering is currently fragile and has caused missing / unstable output at times

Safe fallback path:

- keep `toQImage()` / cached sprite rendering as the stable fallback
- introduce GPU direct draw only for simple primitives first
- keep complex polygon / custom path cases on the raster fallback until the GPU path is proven stable

## 3. Main Problems Observed

### 3.1 Text

1. GPU text works conceptually, but the module / type boundary needed cleanup.
2. The public API and implementation had `FloatRGBA` / `Color.Float` mixing.
3. Plain text is ready for the GPU path, but complex text layout still falls back.

### 3.2 Shape

1. Direct GPU shape drawing is not stable yet.
2. Shape rendering can disappear if the point conversion or fill/stroke path is wrong.
3. The cached raster path is stable, but it is not the final GPU solution.

## 4. Files to Review

### Text

- [`Artifact/include/Render/ArtifactIRenderer.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactIRenderer.ixx)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](/x:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)
- [`Artifact/include/Render/PrimitiveRenderer2D.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Render/PrimitiveRenderer2D.ixx)
- [`Artifact/src/Render/PrimitiveRenderer2D.cppm`](/x:/Dev/ArtifactStudio/Artifact/src/Render/PrimitiveRenderer2D.cppm)
- [`Artifact/include/Render/RenderCommandBuffer.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Render/RenderCommandBuffer.ixx)
- [`Artifact/include/Render/DiligentImmediateSubmitter.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Render/DiligentImmediateSubmitter.ixx)
- [`Artifact/src/Render/DiligentImmediateSubmitter.cppm`](/x:/Dev/ArtifactStudio/Artifact/src/Render/DiligentImmediateSubmitter.cppm)
- [`Artifact/include/Layer/ArtifactTextLayer.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Layer/ArtifactTextLayer.ixx)
- [`Artifact/src/Layer/ArtifactTextLayer.cppm`](/x:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactTextLayer.cppm)

### Shape

- [`ArtifactCore/include/Shape/ShapePath.ixx`](/x:/Dev/ArtifactStudio/ArtifactCore/include/Shape/ShapePath.ixx)
- [`ArtifactCore/src/Shape/ShapePath.cppm`](/x:/Dev/ArtifactStudio/ArtifactCore/src/Shape/ShapePath.cppm)
- [`ArtifactCore/src/Shape/ShapeGroup.cppm`](/x:/Dev/ArtifactStudio/ArtifactCore/src/Shape/ShapeGroup.cppm)
- [`ArtifactCore/src/Shape/ShapeLayer.cppm`](/x:/Dev/ArtifactStudio/ArtifactCore/src/Shape/ShapeLayer.cppm)
- [`Artifact/include/Layer/ArtifactShapeLayer.ixx`](/x:/Dev/ArtifactStudio/Artifact/include/Layer/ArtifactShapeLayer.ixx)
- [`Artifact/src/Layer/ArtifactShapeLayer.cppm`](/x:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactShapeLayer.cppm)

## 5. Recommended Next Step

1. Keep the text path on the GPU, but limit it to plain text first.
2. Keep shape rendering on the cached raster fallback until the GPU primitive path is proven stable.
3. Once the GPU shape path is stable for simple primitives, expand it to polygons and custom paths.

## 6. Short Summary

- GPU text is now scaffolded and the linker issue has been resolved.
- GPU shape rendering is still unstable and should be treated more cautiously.
- The safest near-term plan is:
  - plain GPU text
  - cached raster shape fallback
  - gradual GPU shape expansion
