> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_EASING_LAB.md](MILESTONE_EASING_LAB.md)

# EasingLab - Phase 2 Execution

Date: 2026-04-21

## Purpose

Implement the visible comparison surface without touching the authoring path yet.

## Current Anchors

- `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineWidget.ixx`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

## Work Items

### 1. Add the preview tile widget

- render the easing curve and the moving marker
- keep each tile independent so the grid can reuse it

### 2. Add the dialog shell

- create the grid layout
- add titles and candidate labels
- wire the scrub control to every tile

### 3. Keep the surface read-only

- do not yet change the composition
- do not yet add undo commands
- keep the scope limited to comparison

## Done When

- the lab can be opened and used for visual comparison
- scrubbing affects every tile in sync
- no authoring data is modified yet

---

## Static audit follow-up (2026-07-25)

現行の `EasingLabWidget` を確認した。

| Work item | 現状 | 判定 |
|---|---|---|
| preview tile widget | 曲線と moving marker を `EasingPreviewWidget` が描画する | 実装済み |
| dialog shell | grid、candidate labels、scrub slider、Apply control がある | 実装済み |
| synchronized preview | slider の normalized progress を全 tile に反映する | 実装済み（静的確認） |
| authoring boundary | 現行コードには Apply callback が先行しているため、本文の「authoring data を変更しない」境界は既に Phase 3 相当に拡張されている | 更新必要 |

**判定**: Phase 2 Execution の比較 surface はソース上完了。表示・scrub の runtime 検証と Apply/Undo の実行確認は未実施。
