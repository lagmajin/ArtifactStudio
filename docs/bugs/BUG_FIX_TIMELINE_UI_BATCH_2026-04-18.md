# Bug Fix Report — Timeline UI Batch (2026-04-18)

4 issues reported from screenshot review.  All fixes are surgical, no QSS added.

---

## Issue 1 — QADS Tab Title Font Too Small

**Symptom**  
Panel tab labels (e.g. "Comp 1") rendered at the system default font size (~8–9 pt), making them hard to read.

**Root Cause**  
`DockStyleManager::applyTabLabelColors()` iterated the QLabel children of each tab and applied palette + bold/normal font weight, but never set an explicit point size.  The QFont size was left at whatever the application default was.

**Fix**  
`Artifact/src/Widgets/Dock/DockStyleManager.cppm` — `applyTabLabelColors()`  
Added `font.setPointSize(10)` before applying the font object to the label.

---

## Issue 2 — Timecode Label Background Colour Mismatch

**Symptom**  
The QLabel widgets inside `ArtifactTimeCodeWidget` (timecode line and frame-number line) had a noticeably different background colour from the rest of the left panel, creating a visible seam.

**Root Cause**  
`ArtifactTimeCodeWidget` set its own `QPalette::Window` to `secondaryBackgroundColor` (a theme token), but the QLabel palette objects were copied from the application palette at construction time and therefore carried the application's default `Window`/`Base` colours.  Some paths in `ArtifactCommonStyle` (or its base) use the `Window`/`Base` role for background fill even when `autoFillBackground` is false, producing the mismatch.

**Fix**  
`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` — constructor  
Both label palettes now have `QPalette::Window` and `QPalette::Base` explicitly set to `widgetBg` (= `secondaryBackgroundColor`) before being assigned to the labels.

---

## Issue 3 — Timecode and Frame Number on Same Line

**Symptom**  
The display showed "00:00:00:00 0f" on a single row.  The frame number was too large and visually dominated.

**Root Cause**  
`ArtifactTimeCodeWidget` used a `QHBoxLayout`, placing the timecode and frame labels side-by-side.

**Fix**  
`Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` — constructor  
Replaced `QHBoxLayout` with `QVBoxLayout` so the timecode label is on top and the frame-number label is below.  Secondary changes:
- Frame font reduced from 16 pt bold → 10 pt non-bold (secondary info).
- Fixed height increased from 42 → 54 px to accommodate two lines.
- `minimumWidth` calculation simplified to timecode text width only.

---

## Issue 4 — Playhead Paint Artifacts (Ghost Pixels)

**Symptom**  
When the playhead moved during playback or scrubbing, the previous position was not erased: a ghost line/arrowhead remained at the old x-coordinate until something else happened to invalidate that region.

**Root Cause**  
`TimelinePlayheadOverlayWidget` has `WA_NoSystemBackground` set, which tells Qt **not** to erase the backing store region before calling `paintEvent`.  The overlay's `paintEvent` only draws the new playhead line; it does not clear the previous pixels.

Previously `syncPlayheadOverlay()` called `overlay->update()`.  This schedules only the overlay's own `paintEvent`.  Because the overlay's backing-store region is not erased (due to `WA_NoSystemBackground`), old pixels from the previous playhead position persisted behind the freshly drawn line.

**Fix**  
`Artifact/src/Widgets/ArtifactTimelineWidget.cpp` — `syncPlayheadOverlay()`

Instead of calling `overlay->update()`, we call `parent->update(strip)` where `strip` is a narrow vertical rectangle that covers both the **previous** and **current** playhead x-positions (± 12 px margin).

When the parent widget is dirtied, Qt re-composites the full z-stack in that region from bottom to top:
1. `TimelineRightPanelWidget` (parent) repaints its background in the strip.
2. All sibling children (track view, scrub bar, etc.) that overlap the strip repaint their portion.
3. The overlay's `paintEvent` then draws the new playhead on top of clean pixels.

The previous playhead position is tracked in `Impl::lastPlayheadParentX_` so the union strip can be computed without storing the old frame number separately.

**Performance note**  
The strip width is typically ≤ 30 px wide during normal playback (adjacent frames).  The full-height strip repaint is comparable in cost to what `ArtifactTimelineTrackPainterView::setCurrentFrame()` already schedules for the track area on every frame change.

---

## Files Changed

| File | Change |
|---|---|
| `Artifact/src/Widgets/Dock/DockStyleManager.cppm` | Issue 1 — explicit 10 pt font in `applyTabLabelColors()` |
| `Artifact/src/Widgets/Timeline/ArtifactTimeCodeWidget.cppm` | Issues 2 & 3 — VBox layout, palette bg fix, font/height adjustments |
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | Issue 4 — parent-strip update in `syncPlayheadOverlay()`, added `lastPlayheadParentX_` to `Impl` |
