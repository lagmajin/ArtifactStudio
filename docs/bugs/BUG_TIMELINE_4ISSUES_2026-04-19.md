# Timeline: 4 Issue Bug Report (2026-04-19)

## Overview

Four persistent timeline bugs investigated and fixed.

---

## Issue 1 — Composition Creation Spawns Excessive Worker Threads

### Root Cause

Two compounding sources:

1. **Redundant `changeCurrentComposition` call** — `ArtifactTimelineWidget::setComposition` was calling
   `svc->changeCurrentComposition(id)` even though this is already called by the upstream event handler
   (`CurrentCompositionChangedEvent`). This queued a second `QTimer::singleShot` which re-published
   the event and caused an unnecessary cascade through `PlaybackCompositionChangedEvent` listeners.

2. **Lazy thread pool initialization** — `QThreadPool::globalInstance()->setMaxThreadCount(N)` (N=10 by
   default) only reserves capacity; threads are created on first use. When the first composition renders,
   all N threads are spawned simultaneously, causing a visible freeze.

### Fix

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`: removed `svc->changeCurrentComposition(id)` from
  `setComposition()` — the composition lookup (`svc->findComposition`) is still done; only the redundant
  re-notification is dropped.
- `Artifact/src/AppMain.cppm`: after `pool->setMaxThreadCount(configuredRenderThreads)`, submit
  `configuredRenderThreads` no-op lambdas via `QtConcurrent::run` to pre-warm all threads at startup,
  before any UI becomes visible.

---

## Issue 2 — Playhead Leaves Ghost Artifacts in Right Pane

### Root Cause

`ArtifactTimelineTrackPainterView::setCurrentFrame()` computed a dirty rect with a ±4px margin:

```cpp
QRect(floor(min(oldX,newX)) - 4, 0, ceil(abs(newX-oldX)) + 8, height())
```

The playhead triangle head has `headWidth = 16.0`, so each side extends ±8px from centre. With only a 4px
margin the outer 4px of the triangle head lay outside the dirty rect, were not cleared by Qt's background
repaint, and accumulated as ghost pixels.

### Fix

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`: changed dirty rect margin from
  ±4 to ±10 (headWidth/2 + 2 buffer):
  ```cpp
  QRect(floor(min(oldX,newX)) - 10, 0, ceil(abs(newX-oldX)) + 20, height())
  ```

---

## Issue 3 — Playhead and Time Ruler Use Inconsistent Orange Colors

### Root Cause

Two files had independently hardcoded different orange values:

| File | Color |
|------|-------|
| `ArtifactTimelineTrackPainterView.cpp:1127` | `QColor(255, 106, 71)` — reddish-orange |
| `ArtifactTimelineScrubBar.cppm:317` | `QColor(255, 142, 73)` — yellow-orange |

Additionally, ruler tick labels used `playheadColor.lighter(195)` — a very pale wash of the playhead
colour — instead of a readable text colour.

Both files already define a `TimelineTheme`/`TimelineThemeColors` struct that exposes `theme.accent`
derived from `ArtifactCore::currentDCCTheme().accentColor`.

### Fix

- `ArtifactTimelineTrackPainterView.cpp`: `playheadColor` → `timelineThemeColors().accent`
- `ArtifactTimelineScrubBar.cppm`: `playheadColor` → `theme.accent`
- `ArtifactTimelineScrubBar.cppm`: ruler label pen → `theme.text.darker(130)` (readable contrast,
  independent of playhead colour)

---

## Issue 4 — Left/Right Pane Rows Misalign After Tree Expansion

### Root Cause

Two separate bugs combined:

1. **Signal never emitted on tree expansion** — `ArtifactLayerPanelWidget::performUpdateLayout()` published
   `TimelineVisibleRowsChangedEvent` to the EventBus when `structureChanged == true`, but never emitted the
   `visibleRowsChanged()` Qt signal. `ArtifactTimelineWidget` is connected to
   `ArtifactLayerTimelinePanelWrapper::visibleRowsChanged` (which wraps the panel signal) and calls
   `refreshTracks()` from that handler. Because the signal was not emitted, `refreshTracks()` was never
   called after expand/collapse, leaving the right pane stale.

2. **Row height hardcoded in `refreshTracks`** — `refreshTracks()` always pushed
   `static_cast<int>(kTimelineRowHeight)` (28) for every row height, ignoring any custom row height set on
   `layerTimelinePanel_`. If the panel row height ever differs, track height arrays would mismatch.

### Fix

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` `performUpdateLayout()`: added
  `visibleRowsChanged();` inside the `if (structureChanged)` block.
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` `refreshTracks()`: replaced hardcoded constant with
  ```cpp
  impl_->layerTimelinePanel_ ? impl_->layerTimelinePanel_->rowHeight()
                              : static_cast<int>(kTimelineRowHeight)
  ```

---

## Files Changed

| File | Changes |
|------|---------|
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | Issue 1: remove redundant `changeCurrentComposition`; Issue 4: use actual row height |
| `Artifact/src/AppMain.cppm` | Issue 1: pre-warm thread pool at startup |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp` | Issue 2: dirty rect margin ±4→±10; Issue 3: theme accent colour |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm` | Issue 3: theme accent + text colours |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | Issue 4: emit `visibleRowsChanged()` signal |
