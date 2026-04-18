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

## Batch 6 — Menu Transparency, Dialog Rounded Corners, Gizmo Mode::None (2026-04-18)

### Issue 1 — QMenu Rounded Corners Not Transparent

**Symptom**
QMenu corner areas outside the rounded rect appeared as opaque background color instead of showing through to the desktop.

**Root Cause**
On Windows, Qt::Popup windows are assigned the CS_DROPSHADOW window class which uses the standard system drop shadow. This shadow mechanism prevents per-pixel alpha compositing (WA_TranslucentBackground) from working: the compositor applies the opaque shadow layer over the entire window area, including the corners, filling them with the background color rather than leaving them transparent.

**Fix**
Artifact/src/Widgets/CommonStyle.cppm — polish(QWidget*):
Added menu->setWindowFlag(Qt::NoDropShadowWindowHint, true) before setting WA_TranslucentBackground. Removing the system drop shadow switches the window to DWM per-pixel alpha compositing, which correctly renders the areas outside the rounded rect as transparent.

---

### Issue 2 — Dialog Rounded Corners (CreateCompositionDialog, CreateSolidLayerSettingDialog)

**Symptom**
Composition-creation and solid-layer-creation dialogs used frameless windows but had no rounded corners or transparency.

**Root Cause**
Both dialogs set Qt::FramelessWindowHint in their constructors but did not enable WA_TranslucentBackground. The dialog backgrounds were opaque rectangles.

**Fix**
Artifact/src/Widgets/CommonStyle.cppm:
1. Added #include <QDialog> to the global module fragment.
2. In polish(QWidget*): detect QDialog* widgets with Qt::FramelessWindowHint, then set WA_TranslucentBackground, WA_NoSystemBackground, and setAutoFillBackground(false).
3. In drawPrimitive(), PE_Widget case: detect frameless QDialog* and draw a rounded-rect background (r=8, theme ackgroundColor fill + orderColor border stroke). The per-pixel alpha from WA_TranslucentBackground leaves corner areas fully transparent.

This applies to every frameless dialog in the application consistently.

---

### Issue 3 — Mode::None Hides Selection Bounding Box

**Symptom**
Switching to a non-transform tool (pan, draw, etc.) triggers setGizmoMode(Mode::None). With Mode::None, the 2D selection bounding box also disappeared, leaving no visual selection indicator on the canvas.

**Root Cause**
TransformGizmo::draw() used showScale = (mode == All || mode == Scale) to gate both the bounding box outline AND the scale handle squares. When mode == None, showScale = false, so the four edge lines were never drawn.

**Fix**
Artifact/src/Widgets/Render/TransformGizmo.cppm:
Added showBBox = showScale || mode_ == Mode::None. Split the original if (showScale) block into:
1. if (showBBox) — draws only the four bounding box edge lines (selection outline).
2. if (showScale) — draws the corner + edge handle squares (interactive scale handles).

Result: Mode::None now shows a thin selection outline without interactive handles, matching standard DCC tool behavior.

