# Timing Event View Milestone

**最終更新:** 2026-08-15

## Update 2026-08-15

- `Artifact.Widgets.TimingEventView` と `TimingEventItem` が実装済み。イベント lane、ruler、playhead、row 表示、選択、hover／cursor、イベント hit test を持つ。
- 左右端のドラッグによる start／end resize、中央ドラッグによる timing move、クリックによる playhead 移動、Ctrl+wheel の zoom、visible range／pixels-per-frame の API を確認できる。
- `ArtifactTestTimingEventView` には events roundtrip、selection、current frame、visible range、zoom、clear の検査がある。ただし通常のタイムライン／Project workflow への統合、右側 compact inspector、label／row 編集、version navigation、export/download、AI safe mutation は未確認または未実装。
- よって Phase 1 は実装済み、Phase 2 は viewport と timing 編集まで部分実装、Phase 3 は未完了。現状は独立した基礎 widget として存在する段階。

## Goal

Build a lightweight timeline-adjacent widget for event and timing work only.

This surface is intentionally narrower than the main `ArtifactTimelineWidget`:

- event placement and timing adjustments
- zoom and pan over a time ruler
- playhead and marker visualization
- selection and multi-selection
- a compact inspector for the selected event

The goal is to make timing-centric workflows faster for both users and AI-driven automation.

## UX Intent

- a clean event lane instead of full layer orchestration
- a right-side inspector with label, row, and time
- a minimal move control for precise timing nudges
- version navigation for event snapshots
- download/export affordances for generated or reviewed event layouts

## Scope

- a new `TimingEventView` widget and its supporting model
- event snapshot data for read-only inspection
- basic edit actions for timing, label, and row changes
- hooks for future AI tool integration
- doc alignment with `docs/WIDGET_MAP.md`

## Phases

### Phase 1

- define the event model and widget ownership
- create the core viewport, ruler, and playhead rendering
- wire selection and hover states

### Phase 2

- add the compact inspector panel
- add zoom, pan, and precise move behavior
- add version navigation and export affordances

### Phase 3

- expose the model to AI tooling
- add safe mutation helpers and regression tests
- integrate with the wider timeline/project workflow

