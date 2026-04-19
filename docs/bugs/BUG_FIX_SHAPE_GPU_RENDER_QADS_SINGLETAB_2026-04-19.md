# Bug Fix Report: ShapeLayer GPU Render Invisible + QADS Single-Tab

**Date:** 2026-04-19  
**Scope:** `Artifact/src/Render/ShaderManager.cppm`, `Artifact/src/Render/DiligentImmediateSubmitter.cppm`, `Artifact/src/Widgets/ArtifactMainWindow.cppm`

---

## Issue 1: ArtifactShapeLayer fill triangles always render invisible on GPU

### Root Cause

`solidTrianglePsoAndSrb_` was created by copying `solidInfo` — which uses `drawSolidRectVSSource` + `g_qsSolidColorPSSource` shaders designed for solid-rect rendering where vertex positions are in UV [0, 1] range.

The vertex shader outputs `uv = input.pos` (the raw vertex position). For `solidRectPsoAndSrb_`, vertices are `{0,0}, {1,0}, {0,1}, {1,1}`, so `uv` is correctly in [0, 1].

The pixel shader `g_qsSolidColorPSSource` uses this UV to compute edge antialiasing:
```hlsl
float dx = min(input.uv.x, 1.0 - input.uv.x);
float dy = min(input.uv.y, 1.0 - input.uv.y);
float d  = min(dx, dy);
if (d < edgeWidth) {
    return float4(uColor.rgb, uColor.a * (d / edgeWidth));  // edge alpha
}
return uColor;
```

For `solidTrianglePsoAndSrb_`, vertices are **canvas-space pixel coordinates** (e.g., 850, 400) passed from `ArtifactShapeLayer::draw()` → `PrimitiveRenderer2D::drawSolidTriangleLocal()`.  
So `uv = {850, 400}`, causing:
```
dx = min(850, 1 - 850) = min(850, -849) = -849  ← negative
d  = -849
alpha = d / edgeWidth = -849 / 0.001 = -849000  ← negative → clamped to 0
```
All triangles render with **alpha = 0** (fully transparent) regardless of color.  
The same bug affected `submitSolidCircle` which reuses `solidTrianglePsoAndSrb_`.

### Fix

Changed `solidTrianglePsoAndSrb_` to use `thickLineShaders_` (reads `ATTRIB0=pos` + `ATTRIB1=color` per vertex, outputs vertex color directly — no UV edge antialiasing). Updated `ResourceLayout.Variables` to include only `TransformCB` (no `ColorBuffer`).

Removed the now-unused `ColorBuffer` cbuffer upload and SRB binding from both `submitSolidTri()` and `submitSolidCircle()`.

**Files changed:**
- `Artifact/src/Render/ShaderManager.cppm` — `createLineFamilyPSOs()`: overrode `triangleInfo.pVS/pPS` to use `thickLineShaders_` and replaced `solidVars` with `triVars` (only `TransformCB`)
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm` — removed `m_draw_solid_rect_cb` guard, `mapWriteDiscard` for `ColorBuffer`, and `GetVariableByName("ColorBuffer")` binding from both `submitSolidTri` and `submitSolidCircle`

### Impact

All shape layers with polygon fills and outline strokes should now render correctly on GPU in both the composition view and solo layer view. Circle renders (gizmo handles, etc.) are also fixed by this change.

---

## Issue 2: QADS tab bar disappears when only one dock widget is present

### Investigation Result

This **is normal QADS default behavior**. When a dock area contains only one widget, QADS hides the tab bar unless the `AlwaysShowTabs` config flag is set.

The installed `qtadvanceddocking-qt6` (vcpkg) package exposes:
```cpp
AlwaysShowTabs = 0x2000,  ///< Tab of a dock widget is always displayed - even if it is the only tab
```

### Fix

Added `CDockManager::setConfigFlag(CDockManager::AlwaysShowTabs, true)` to `ArtifactMainWindow.cppm` constructor. This ensures the tab bar remains visible even when only one dock widget occupies a dock area.

**File changed:**
- `Artifact/src/Widgets/ArtifactMainWindow.cppm` — added `AlwaysShowTabs` flag after existing config flags

---

## Status

| Issue | Status |
|-------|--------|
| ShapeLayer fill triangles invisible on GPU (triangle + circle PSO wrong shader) | ✅ Fixed |
| QADS single-widget tab bar hidden | ✅ Fixed (`AlwaysShowTabs`) |
