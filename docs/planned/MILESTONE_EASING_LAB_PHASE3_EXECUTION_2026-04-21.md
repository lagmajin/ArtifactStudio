> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_EASING_LAB.md](MILESTONE_EASING_LAB.md)

# EasingLab - Phase 3 Execution

Date: 2026-04-21

## Purpose

Hook the comparison surface back into the timeline and property editing flow.

## Current Anchors

- `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
- `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- `Artifact/include/Property/AbstractProperty.ixx`
- `Artifact/src/Property/AbstractProperty.cppm`

## Work Items

### 1. Selection bridge

- capture the selected keyframe segment from the current timeline context
- keep the selection stable while the dialog is open

### 2. Apply bridge

- translate the candidate back to the stored interpolation / easing value
- commit the change through the existing undoable command path

### 3. Entry point

- expose an action from the timeline or inspector
- keep the entry point lightweight and discoverable

## Done When

- the selected segment can be previewed and updated from the dialog
- apply / undo / redo all work through the existing model
- the UI surface still feels like part of the current editor

---

## Static audit follow-up (2026-07-25)

現行の Timeline 実装を確認した。

| Work item | 現状 | 判定 |
|---|---|---|
| selection bridge | selected markers を Timeline 側で抽出し、dialog callback に保持する | 実装済み（静的確認） |
| apply bridge | `InterpolationChangeRecord` を作成し、selected keyframe の interpolation を更新する | 実装済み |
| undoable command | `ApplyInterpolationCommand` の redo/undo が before/after state を適用する | 実装済み |
| entry point | Timeline `Ease+` と Animation menu の action がある | 実装済み |

**判定**: Execution の Done 条件はソース上達成。実行時の apply / undo / redo 結果は未確認。
