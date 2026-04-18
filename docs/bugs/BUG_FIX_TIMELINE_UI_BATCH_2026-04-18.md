# Bug Fix Report — Timeline UI Batch (2026-04-18)

6 issues reported from screenshot review (batch 1) and follow-up review (batch 2).
All fixes are surgical; no QSS added, no new signals/slots.

---

## Issue 1 — QADS Tab Title Font Too Small ✅

**Symptom**  
Panel tab labels (e.g. "Comp 1") rendered at the system default font size (~8–9 pt), making them hard to read.

**Root Cause**  
`DockStyleManager::applyTabLabelColors()` iterated the QLabel children of each tab and applied palette + bold/normal font weight, but never set an explicit point size.

**Fix** *(corrected in batch 2)*  
`Artifact/src/Widgets/Dock/DockStyleManager.cppm` — `applyTabLabelColors()`  
Set `font.setPointSize(12)` (initial batch 1 attempt used 10 pt, which was still too small; corrected to 12 pt in batch 2).

---

## Issue 2 — Timecode Label Background Colour Mismatch ✅

**Symptom**  
`ArtifactTimeCodeWidget` rendered with a visibly different background colour from the surrounding left-panel header, creating a visible seam.

**Root Cause**  
The widget set its own `QPalette::Window` to `secondaryBackgroundColor`, which differed from the parent container's inherited theme background.

**Fix** *(corrected in batch 2)*  
`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` — constructor  
Removed `setAutoFillBackground(true)` and the explicit `QPalette::Window` palette override from the widget entirely.  Set `setAutoFillBackground(false)` so the widget is fully transparent and inherits the container's background.  Label text colours (WindowText/Text) are still applied; only the background overrides were removed.

---

## Issue 3 — Timecode and Frame Number on Same Line ✅

**Symptom**  
The display showed "00:00:00:00 0f" on a single row.

**Root Cause**  
`ArtifactTimeCodeWidget` used a `QHBoxLayout`.

**Fix** *(batch 1)*  
`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` — constructor  
Replaced `QHBoxLayout` with `QVBoxLayout`.  Frame font reduced to 10 pt non-bold.  Fixed height set to 54 px.

---

## Issue 4 — Playhead Paint Artifacts (Ghost Pixels) ✅

**Symptom**  
Previous playhead position left ghost pixels.  The batch-1 "fix" of calling `parent->update(strip)` alone made things worse — the new playhead stopped appearing entirely.

**Root Cause (revised)**  
`TimelinePlayheadOverlayWidget` has `WA_NoSystemBackground`, so Qt does not erase the overlay's backing-store region before `paintEvent`.  The backing-store model for the single top-level window works as follows:

1. When `parent->update(strip)` is called, Qt schedules the parent's `paintEvent` for that strip.
2. The parent repaints → fresh background pixels written to the shared backing store.
3. **However**, the overlay is NOT added to the dirty list.  Its `paintEvent` is NOT called.  The old playhead pixels (from the overlay's last `paintEvent`) are composited on top of the fresh background — ghost remains.
4. Because the new playhead is never redrawn, the playhead disappears entirely.

**Fix** *(batch 2)*  
`Artifact/src/Widgets/ArtifactTimelineWidget.cpp` — `syncPlayheadOverlay()`  
Call **both** `parent->update(strip)` AND `overlay->update(strip)`:
- Parent repaints first → clears old playhead from the backing store (erases ghost).
- Overlay repaints second → draws new playhead on top of the fresh background.

`WA_NoSystemBackground` is preserved on the overlay (no opaque background fill).  The union strip covers both `lastPlayheadParentX_` (old) and the new position, ensuring the ghost is erased.

---

## Issue 5 — OIIO Thumbnail Colours Wrong (Asset Browser) ✅

**Symptom**  
After migrating from `QImage`/`QMediaPlayer` to OIIO for thumbnail loading, thumbnails displayed in the asset browser showed incorrect colours (red↔blue swapped, dark edges on transparent images).

**Root Cause**  
`loadImageThumbnailViaOIIO()` allocated a `QImage::Format_ARGB32_Premultiplied` image and called `rgba.get_pixels(…, UINT8, image.bits())`.  OIIO writes pixels in RGBA byte order (R at byte 0, G at byte 1, B at byte 2, A at byte 3).  `Format_ARGB32_Premultiplied` on little-endian x86 expects BGRA memory layout → red and blue channels are swapped, and premultiplied alpha is applied to straight-alpha data.

---

# Bug Fix Report — UI Batch 3 (2026-04-18)

4 additional issues reported after batch 2 fixes.  
All fixes are surgical; no QSS added, no new signals/slots.

---

## Issue A — QADS Tab Title Font Still Too Small (batch 3 correction) ✅

**Symptom**  
Tab labels were still hard to read at 12 pt (set in batch 2).

**Root Cause**  
12 pt is still below comfortable reading size for the panel header style.

**Fix**  
`Artifact/src/Widgets/Dock/DockStyleManager.cppm` — `applyTabLabelColors()`  
Changed `font.setPointSize(12)` → `font.setPointSize(16)`.

---

## Issue B — Menu Bar / Action Text Too Small ✅

**Symptom**  
Menu bar and popup menu item text inherited the application default font size, which was noticeably smaller than desired.

**Root Cause**  
`ArtifactMenuBar` constructor did not set an explicit font size; it relied entirely on the inherited app font.

**Fix**  
`Artifact/src/Widgets/ArtifactMenuBar.cppm` — constructor body  
After construction, reads `font()` (inherited app font), scales `pointSizeF` × 1.2, and applies via `setFont(f)` plus iterates `findChildren<QMenu*>()` to propagate to all popup menus.  No QSS used.

---

## Issue C — Composition Editor Not Displayed After Lazy-Init Change ✅

**Symptom**  
After commit `71845df` wrapped all dock widget setup in a `QTimer::singleShot(0, …)`, the Composition Editor (main Diligent DX12 viewport) stopped appearing on startup.

**Root Cause**  
`ArtifactCompositionEditor` owns a `CompositionViewport` which has `WA_NativeWindow`; Qt creates the underlying HWND lazily on the first `show()`.  
When `addDockedWidget("Composition Viewer", CenterDockWidgetArea, compositionEditor)` was deferred inside `singleShot(0)` — fired after `mw->show()` — QADS called `setWidget()` on an already-shown layout.  
Depending on the QADS version, this may not fire a `showEvent` on the newly added child widget.  Even when `showEvent` was invoked, the `!isVisible()` guard in `CompositionViewport::showEvent` (backed by a 16 ms timer) could evaluate while the dock was still processing its layout pass, causing the Diligent renderer never to initialize.

**Fix**  
`Artifact/src/AppMain.cppm`  
Moved `new ArtifactCompositionEditor(mw)` and its `addDockedWidget` / `connect` calls to execute **synchronously** before the `QTimer::singleShot(0, …)` block — i.e. before `mw->show()`.  
The singleShot lambda still handles all other dock widgets and workspace-restore steps (layout settings, `setDockVisible`, `activateDock`), which correctly remain deferred.

---

## Issue D — Composition Editor Initialization Too Slow (Shader Compilation on Render Path) ✅

**Symptom**  
After opening a project or loading a composition, the editor was noticeably sluggish for 1–2 s and then suddenly became responsive.  Application profiling showed bursts of worker threads at startup.

**Root Cause**  
Commit `6fc6295` moved `LayerBlendPipeline::initialize()` (GPU shader compilation, potentially 100–300 ms) from `CompositionRenderController::initialize()` into `renderOneFrameImpl()` behind a 1.2 s elapsed-time gate.  
This caused the **render timer callback** to block mid-frame, producing visible frame drops at exactly the 1.2 s mark.  
Additionally, `QtConcurrent::run` calls for MayaGradient warmup and layer prefetch fire simultaneously with the timer-deferred dock setup, creating a burst of 4+ background threads at startup.

**Fix**  
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`  
- In `initialize()`, added `QTimer::singleShot(1500, this, [this]() { … })` to initialize `GpuContext` and `LayerBlendPipeline` completely off the render path, 1.5 s after Diligent init.  
- Removed the entire lazy-init block from `renderOneFrameImpl()` (the `startupTimer_.elapsed() >= 1200` branch).  
- The render path now reads only the already-set `blendPipelineReady_` flag — zero blocking cost per frame.

Secondary issue: for 1-channel (grayscale) images the channel order `{0,1,2,3}` attempted to read channels 1–3 from a 1-channel image, producing garbage colour.

**Fix**  
`Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` — `loadImageThumbnailViaOIIO()`
- Changed `QImage::Format_ARGB32_Premultiplied` → `QImage::Format_RGBA8888` (expects RGBA byte order, straight alpha — matches OIIO output).
- For the 1-channel grayscale case: changed `channelOrder` to `{0, 0, 0, -1}` with `channelValues[3] = 1.0f` so the single channel is replicated into R, G, and B; alpha is set to opaque.

---

## Issue 6 — Audio Playback Spawns Many Worker Threads, Slow to Start ✅

**Symptom**  
Selecting an audio file in the content viewer spawned many worker threads all at once and playback was slow to start.

**Root Cause**  
`MediaPlaybackController::getNextAudioFrame()` (called every pump tick, ~60 Hz) contained two problems:
1. It called `impl_->mediaReader_->start()` on every invocation.  `start()` calls `tbb::task_group::run()` to schedule a TBB task.  On the **first** call, TBB initialises its global thread pool by creating `hardware_concurrency()` threads simultaneously — the visible "many worker threads" burst.  On subsequent calls (e.g. after the reader hits EOF and resets `isRunning_`), additional TBB tasks are queued.
2. It held `directDecodeMutex_` for the duration of a **spin-wait loop** (up to 500 iterations × 1 ms sleep = up to 125 ms) waiting for an audio packet.  This blocked the UI/audio-pump thread.

**Fix**  
`ArtifactCore/src/Media/MediaPlaybackController.cppm` — `getNextAudioFrame()`
- Removed `impl_->mediaReader_->start()` from inside `getNextAudioFrame()`.  The reader is already started by `play()`.
- Removed the spin-wait loop entirely; replaced with a single non-blocking `try_pop` on the audio queue.
- If no packet is available (reader hasn't produced one yet), the method returns an empty `QByteArray`.  The audio pump timer (16 ms) retries on the next tick with no extra cost.

---

## Files Changed

| File | Change |
|---|---|
| `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | Issue 1 — 12 pt font in `applyTabLabelColors()` |
| `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` | Issues 2 & 3 — transparent bg (no autoFill), VBox layout, font/height |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | Issue 4 — dual `parent->update` + `overlay->update` in `syncPlayheadOverlay()` |
| `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | Issue 5 — RGBA8888 format, grayscale channel fix |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | Issue 6 — non-blocking `getNextAudioFrame()`, removed TBB re-spawn |


---

# Bug Fix Report — UI Batch 4 (2026-04-18)

3 additional issues reported after batch 3, plus 1 investigation question.

---

## Issue A — Tab Font Not Applied Until Activated ✅

**Symptom**  
QADS tab labels displayed at the small system default font on startup; after clicking a tab (activating it), the 16 pt font appeared correctly.

**Root Cause**  
efreshDockDecorations() called pplyTabLabelColors() only inside the if (tabChanged) block. On first startup, epolishWidget(tab) (called to set palette) sends StyleChange to the tab, which Qt propagates to child QLabel widgets via QEvent::FontChange. This async propagation can reset the label's explicitly-set font back to the style default AFTER pplyTabLabelColors() returns. On the next focus/click event that triggers a refresh, 	abChanged = false (properties already set), so pplyTabLabelColors() is skipped and the corrected font is never re-applied until the user activates the tab (triggering a state change).

**Fix**  
Artifact/src/Widgets/Dock/DockStyleManager.cppm — efreshDockDecorations()  
Moved pplyTabLabelColors() outside the if (tabChanged) guard. It is now called unconditionally for every tab on every refresh. The overhead is negligible (iterates 1–2 QLabel children per tab, fired only on focus/input events).

---

## Issue B — Menu Font Still Too Small (+10 % More) ✅

**Symptom**  
Menu bar and popup menus were still slightly small after the ×1.2 scaling from batch 3.

**Fix**  
Artifact/src/Widgets/ArtifactMenuBar.cppm — constructor  
Changed multiplier from 1.2 to 1.32 (i.e., ×1.2 × ×1.1).

---

## Issue C — Composition Editor Zoom Resets on Focus Cycle ✅

**Symptom**  
Switching focus away from the Composition Editor and back caused the viewport zoom to snap back to the default (fill/fit), losing any manual zoom the user had set.

**Root Cause**  
CompositionViewport::showEvent fires on every QADS panel show/activate cycle. When the controller was already initialized, showEvent called syncPreferredComposition(). syncPreferredComposition() always called equestInitialFit() regardless of whether the composition had actually changed, triggering controller_->zoomFill() and resetting the zoom.

**Fix**  
Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm — syncPreferredComposition()  
Added a compositionChanged = (controller_->composition() != comp) check **before** setComposition(). equestInitialFit() (and utoStartPending_) is now only set when the composition pointer actually changes. Re-focus events that find the same composition already loaded are a no-op for zoom.

---

## Issue D — Layer Garbage in Composition Editor (Investigation) ℹ️

**Question**: Is the garbage rendering around layers caused by the native text rendering currently being implemented?

**Answer**: No. PrimitiveRenderer2D / GlyphAtlas (the DX12 native text rendering path) is not yet wired into ArtifactCompositionRenderController. Text layers still render via ArtifactTextLayer::toQImage() (CPU path). The garbage is unrelated to text rendering. Likely candidates: SDF Raymarch layer implementation (4def313), GPU blend pipeline timing, or stale surface cache entries. Separate investigation required.


---

## Batch 5 — Menu Rounded Corners, QADS Tab Rounded Corners, 3D Gizmo Garbage ✅

### Issue 1 — QMenu Dropdown Has Square Corners

**Symptom**  
Popup menus opened from the menu bar had square corners, inconsistent with the DCC tool aesthetic.

**Root Cause**  
ArtifactCommonStyle did not override PE_PanelMenu / PE_FrameMenu, so Fusion's default square panel was used. The QMenu popup window had no transparent alpha channel for corner cut-out.

**Fix**  
Artifact/src/Widgets/CommonStyle.cppm — polish(QWidget*) and drawPrimitive()  
- In polish(): for QMenu* widgets, set WA_TranslucentBackground and WA_NoSystemBackground to give the popup window an alpha channel.  
- In drawPrimitive(): override PE_PanelMenu to fill a rounded rect (radius 6 px) with 	heme.secondaryBackgroundColor and draw a 1 px border using 	heme.borderColor.  
- Override PE_FrameMenu as a no-op (background + border handled in PE_PanelMenu).

---

### Issue 2 — QADS Tabs Have Square Corners

**Symptom**  
Individual dock panel tabs (CDockWidgetTab) had sharp 90° corners.

**Root Cause**  
DockStyleManager::applyTabLabelColors() set WA_StyledBackground on each tab (causing drawPrimitive(PE_Widget) to be called for the background), but ArtifactCommonStyle::drawPrimitive() had no PE_Widget override so Fusion's flat rectangle fill was used.

**Fix**  
- Artifact/src/Widgets/Dock/DockStyleManager.cppm — pplyTabLabelColors(): set 	ab->setProperty("artifactDockTab", true) to mark each tab.  
- Artifact/src/Widgets/CommonStyle.cppm — drawPrimitive(): added PE_Widget case; when widget->property("artifactDockTab").toBool() is true, draw a rounded rect (radius 4 px) using option->palette.color(QPalette::Window).  
Style chain: CDockWidgetTab.style() = DockGlowStyle(base=ArtifactCommonStyle) → our PE_Widget override is reachable through QProxyStyle delegation.

---

### Issue 3 — 3D Gizmo Drawn Over 2D Layers (Layer Garbage)

**Symptom**  
Selecting a 2D layer in the composition editor caused large X/Y/Z axis arrows to appear overlaid on the layer, overlapping the 2D gizmo's move handles. The artifacts moved with the layer and changed color on selection.

**Root Cause**  
enderOneFrameImpl() called gizmo3D_->draw() for **all** selected layers regardless of layer->is3D(). For a 2D layer with z=0, Artifact3DGizmo::draw() calculated s = max(|viewPos.z| * 0.63, 126) = 126 canvas units and drew full X/Y/Z axis arrows at the layer center. These overlapped the 2D gizmo arrows, producing the "garbage" visual.  
Artifact3DGizmo::draw() itself correctly resets setUseExternalMatrices(false) at the end of every call, so there was no secondary matrix state leak.

**Fix**  
Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm — enderOneFrameImpl() line ~3891:  
Changed guard from if (gizmo3D_) to if (gizmo3D_ && selectedLayer->is3D()).  
The selectedLayer pointer is guaranteed non-null at this point (guarded by the enclosing if (selectedLayer && selectedLayer->isVisible()) block).


---

## Batch 6c — Dialog Transparency Regression, 3D Gizmo Stealing 2D Input (2026-04-18)

### Issue 2 (regression from 6b) — Dialog Shows Checkerboard Transparent Areas

**Symptom**
After batch 6b, the header area and name-row of `CreateCompositionDialog` showed checkerboard transparency. The `QTabWidget` content area was solid. Same issue in plane-layer dialogs.

**Root Cause**
`WA_TranslucentBackground` on the top-level dialog makes the entire HWND use DWM per-pixel alpha. Child `QWidget` containers (header bar, name row) have no explicit background fill at the Qt paint level, so their areas render as fully transparent (alpha=0) against the DWM compositing buffer → checkerboard.

**Fix (Batch 6c)**
`Artifact/src/Widgets/CommonStyle.cppm` — `RoundedWindowMaskFilter`:
- Added `bool onlyIfFrameless` constructor parameter (default `false`).
- At `QEvent::Show`/`QEvent::Resize` time, if `onlyIfFrameless_` is true, checks `Qt::FramelessWindowHint` before applying mask. Dialogs that are NOT frameless receive no mask.
- `polish()` now installs the filter on every `QDialog` with property guard `artifactDialogMaskInstalled`.

`Artifact/src/Widgets/Dialog/ArtifactCreateCompositionDialog.cppm` — constructor:
`Artifact/src/Widgets/Dialog/CreatePlaneLayerDialog.cppm` — `buildDialogChrome()`:
- Removed `setAttribute(WA_TranslucentBackground, true)` and `setAttribute(WA_NoSystemBackground, true)`.

The dialog window is now opaque (no per-pixel alpha). The mask clips corners at the OS level, giving rounded appearance without any child-widget transparency.

---

### Issue 3 (regression from all batch 6) — Gizmo Cannot Move 2D Layers; 3D Gizmo Steals Mouse Input

**Symptom**
Selecting a 2D layer and attempting to drag it via the gizmo had no effect. The bounding box showed (Mode::None fix from 6a) but dragging was impossible.

**Root Cause**
In `handleMousePress()` and `handleMouseMove()`, the 3D gizmo hit-test/interaction block ran for **all** selected layers, not just 3D layers:
- `handleMousePress()` line ~2348: `if (selectedLayer && impl_->gizmo3D_)` — no `is3D()` guard. The 3D gizmo `hitTest()` was called with a ray for 2D layers. If the ray intersected the 3D gizmo's plane (which may contain stale geometry from a previously selected 3D layer), `beginDrag()` was called and the function returned early — the 2D `TransformGizmo::handleMousePress()` was **never reached**.
- `handleMouseMove()` line ~2786: `if (impl_->gizmo3D_->isDragging())` — because `beginDrag()` was called at press time for 2D layers, `isDragging()` returned true during drag. `updateDrag()` ran and returned early before the 2D gizmo could process the move.

**Fix (Batch 6c)**
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`:
- `handleMousePress()`: Changed guard to `if (selectedLayer && impl_->gizmo3D_ && selectedLayer->is3D())`.
- `handleMouseMove()`: Wrapped the entire 3D gizmo block inside `if (sel3DLayer && sel3DLayer->is3D())`, fetching the selected layer via `selectedLayerId_`.

Result: 3D gizmo interaction only activates for layers where `is3D()` is true. 2D layers always fall through to the `TransformGizmo` path. Both 2D movement and 3D gizmo remain fully functional.

---


### Issue 1 — QMenu Rounded Corners Not Transparent

**Symptom**
QMenu corner areas outside the rounded rect appeared as opaque background color instead of showing through to the desktop. Adding `Qt::NoDropShadowWindowHint` (first attempt) had no effect.

**Root Cause**
On Windows, Qt::Popup windows use the `CS_DROPSHADOW` window class which is registered once and shared. `NoDropShadowWindowHint` cannot change a window class already registered. Even with the flag, DWM per-pixel alpha compositing does not reliably work for popup windows with system drop shadows.

**Fix (revised)**
`Artifact/src/Widgets/CommonStyle.cppm`:
- Removed `NoDropShadowWindowHint`, `WA_TranslucentBackground`, `WA_NoSystemBackground` from the QMenu block in `polish()`.
- Added `RoundedWindowMaskFilter` (QObject event filter, anonymous namespace): intercepts `QEvent::Show` and `QEvent::Resize`, generates a QBitmap with antialiased rounded corners, calls `widget->setMask()`.
- Installs the filter on each QMenu once (property guard `artifactMenuMaskInstalled`).
- `setMask()` clips corner pixels at the OS window compositor level — no DWM per-pixel alpha required.

---

### Issue 2 — Dialog Rounded Corners: Black Background Regression

**Symptom**
After first attempt, frameless dialogs showed black widget backgrounds; rounded corners were not visible. Caused by setting `WA_TranslucentBackground` in `polish()` after HWND creation.

**Root Cause**
`polish(QWidget*)` is called during `QWidget` construction before subclass constructors run `setWindowFlags()`. The frameless flag check therefore never triggers for fresh dialog construction. When style reapplication calls `polish()` on already-alive widgets, setting `WA_TranslucentBackground` on a widget with an existing native HWND (allocated as opaque 24-bit RGB) causes transparent drawing to composite against a black buffer.

**Fix (revised)**
- Removed the `WA_TranslucentBackground`/`WA_NoSystemBackground` QDialog block from `polish()` entirely.
- Added `setAttribute(Qt::WA_TranslucentBackground, true)` and `setAttribute(Qt::WA_NoSystemBackground, true)` directly in each dialog constructor **after** `setWindowFlags()` and **before** HWND creation:
  - `CreateCompositionDialog` constructor (`ArtifactCreateCompositionDialog.cppm`)
  - `buildDialogChrome()` helper (`CreatePlaneLayerDialog.cppm`) — applies to both `CreateSolidLayerSettingDialog` and `EditPlaneLayerSettingDialog`
- `drawPrimitive(PE_Widget)` rounded-rect background handler in `CommonStyle` is retained.

---

### Issue 3 — Mode::None Bounding Box Visible But Non-Interactive

**Symptom**
After the batch 6 bounding-box fix, the selection outline was visible in Mode::None but clicking/dragging had no effect. Users could not move layers without switching to a transform tool.

**Root Cause**
`TransformGizmo::allowsHandle()` had no case for `Mode::None`, falling through to `return false`. All handles were rejected, so `hitTest()` never returned a valid handle and `handleMousePress()` never started a drag.

**Fix**
`Artifact/src/Widgets/Render/TransformGizmo.cppm` — `allowsHandle()`:
Added `case Mode::None: return handle == HandleType::Move;`

Result: In Mode::None, clicking inside the layer bounding box returns `HandleType::Move`, enabling layer repositioning by drag. Scale and rotate handles remain non-interactive, consistent with selection-tool semantics.


---

# Bug Fix Report — Batch 7 (2026-04-18)

4 issues reported after batch 6c.

---

## Issue 1 — Rounded Corners Jagged / Aliased ✅

**Symptom**
QMenu and QDialog rounded corners appeared jagged (ギザギザ). The outline was noticeably stepped instead of smooth.

**Root Cause**
`RoundedWindowMaskFilter` uses a `QBitmap` (1-bit depth) as the window mask. `QPainter::Antialiasing` has **zero effect** on 1-bit bitmaps — each pixel is either on (color1) or off (color0). There is no partial transparency to smooth the curve. This is a fundamental limitation of the `setMask()` approach.

**Fix**
`Artifact/src/Widgets/CommonStyle.cppm` — `RoundedWindowMaskFilter`:
- On Windows 11 (Build 22000+), the filter now uses **DWM native rounded corners** via `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUNDSMALL = 3)`.
- DWM compositing applies sub-pixel anti-aliasing at the compositor level, producing perfectly smooth corners.
- `dwmapi.dll` is loaded dynamically (`LoadLibraryW` + `GetProcAddress`) — same pattern already used in `ArtifactMainWindow.cppm` and `NativeHelper.cpp`.
- If DWM attribute fails (Windows 10 or older), falls back to the existing `QBitmap` mask.
- `dwmApplied_` flag prevents redundant calls and skips mask application when DWM is active.

---

## Issue 2 — Property Widget Not Showing Selection After Plane Layer Creation ✅

**Symptom**
After adding a plane layer via the dialog, the layer appeared selected in the timeline/composition editor, but the Property Widget remained empty — not showing the layer's properties.

**Root Cause**
The selection notification flow is: `addLayerToCurrentComposition()` → `selectLayer(id)` → `selectionManager->selectLayer()` → `selectionChanged` signal + `LayerSelectionChangedEvent` → `syncPropertyPanelLayer()` → `resolveLayerForUi()` → `propertyPanel->setLayer()`.

The existing `QTimer::singleShot(0)` deferred retry sometimes fails because the layer is not yet fully wired into the composition's layer map at the next event-loop tick (particularly when the dialog close event, undo command push, and layer creation all happen in rapid succession within the same event-loop frame).

**Fix**
`Artifact/src/AppMain.cppm` — `syncPropertyPanelLayer` lambda:
- Refactored into a `deferredResolve` closure that accepts a retry delay parameter.
- The initial `QTimer::singleShot(0)` invokes `deferredResolve(100)`.
- If the first resolution attempt fails and a retry delay was specified (100 ms), a secondary `QTimer::singleShot(100ms)` fires and reattempts `resolveLayerForUi()`.
- This two-tier retry (0 ms → 100 ms) catches the timing window where the layer is not yet resolvable on the immediate next tick but is ready within ~100 ms.

---

## Issue 3 — Opacity Changes Not Affecting Rendered Transparency ✅

**Symptom**
Adjusting a layer's Opacity slider in the Property Widget changed the numeric value but the composition viewport showed no visual change — the layer remained fully opaque.

**Root Cause**
`ArtifactAbstractLayer::setOpacity()` calls `notifyLayerMutation()`, which publishes a `LayerChangedEvent` with `compositionId = comp->id().toString()` where `comp = layer->composition()`. For freshly created layers (or layers whose composition back-pointer is cleared during undo/redo), `composition()` returns `nullptr` → `compositionId` is empty.

The render controller's `LayerChangedEvent` handler (line 1530-1534) had a guard:
```cpp
if (!comp || event.compositionId.isEmpty() ||
    comp->id().toString() != event.compositionId) {
  return;
}
```
When `event.compositionId` is empty, this guard returned early — the render was **silently skipped**.

**Fix**
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — `LayerChangedEvent` handler:
- Changed the guard to:
```cpp
if (!comp) return;
if (!event.compositionId.isEmpty() &&
    comp->id().toString() != event.compositionId) return;
```
- Events with an empty `compositionId` are now treated as applying to the **current** composition (the only reasonable assumption — the user is editing a layer that's visible in the viewport).
- Events with a non-empty `compositionId` that doesn't match are still filtered out.

---

## Issue 4 — Video Layer Frame Decoding Broken ✅

**Symptom**
Video layers displayed no frames. The decode process silently failed with `sendPacket failed` warnings in logs.

**Root Cause**
`MediaPlaybackController::decodeVideoFrameDirectAtFrame()` (FFmpeg path) had a critical bug in packet processing at line 183-188:
```cpp
const int ret = videoDecoder_->sendPacket(pkt);
if (ret < 0) { break; }  // treated ALL errors as fatal
```
`avcodec_send_packet()` returns `AVERROR(EAGAIN)` when the decoder's internal buffer is full and frames must be drained first (via `avcodec_receive_frame()`). This is a **normal flow control signal**, not an error. The old code treated it as fatal and broke out of the decode loop — no frames were ever decoded for streams where the decoder's input buffer filled before the first output frame was ready.

Additionally, when `av_read_frame()` returned `AVERROR_EOF`, the remaining buffered frames in the decoder were never flushed (the loop simply broke without draining).

**Fix**
`ArtifactCore/src/Media/MediaPlaybackController.cppm` — `decodeVideoFrameDirectAtFrame()`:
1. Extracted frame-draining logic into a `drainFrames` lambda for reuse.
2. `AVERROR(EAGAIN)` from `sendPacket`: drain frames first, then continue the read loop (packet data was already consumed/unref'd).
3. `AVERROR_EOF` from `av_read_frame`: flush the decoder with `sendPacket(nullptr)` + `drainFrames()` to retrieve remaining buffered frames.
4. Added `<cerrno>` include for `EAGAIN` constant availability.
5. The outer loop now also checks `result.isNull()` to exit early once a target frame is found.

---

## Files Changed (Batch 7)

| File | Change |
|---|---|
| `Artifact/src/Widgets/CommonStyle.cppm` | Issue 1 — DWM native rounded corners on Win11+, fallback to QBitmap mask |
| `Artifact/src/AppMain.cppm` | Issue 2 — Two-tier deferred retry (0ms + 100ms) for property panel layer resolution |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | Issue 3 — Accept LayerChangedEvent with empty compositionId for current composition |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | Issue 4 — Handle EAGAIN from sendPacket, flush decoder on EOF |
