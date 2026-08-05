> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_EASING_LAB.md](MILESTONE_EASING_LAB.md)

# EasingLab - Phase 3: Integration and Apply Path

Date: 2026-04-21

## Purpose

Connect the lab to the existing selection and command system so the chosen easing can be applied back to the actual keyframe segment.

## Scope

- selection extraction from timeline or property UI
- apply command path
- undo / redo compatibility
- launch entry point from existing UI

## Out Of Scope

- advanced curve editing
- multi-segment batch editing
- AI recommendations
- any rewrite of the current timeline model

## Execution Steps

### 1. Read the selected segment

- identify the selected keyframe segment from the current surface
- preserve the current selection context when opening the lab

### 2. Apply easing through the existing command path

- update the actual keyframe interpolation / easing value
- route the change through undoable commands
- keep the current composition state intact

### 3. Expose the entry point

- add the lab launch entry to the timeline or property surface
- keep the entry point discoverable but unobtrusive

## Definition Of Done

- the lab can open from the existing editor surface
- applying a candidate updates the selected keyframe
- undo and redo remain valid

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. extend the preset set if needed
2. add per-segment comparison metadata
3. decide whether the lab should also offer bezier-edit shortcuts

---

## Static audit follow-up (2026-07-25)

Timeline の easing lab 接続を確認した。selected keyframe marker の抽出、dialog 起動、interpolation change record、undo command、redo path が現行ソースに存在する。

| Definition of Done | 現状 | 判定 |
|---|---|---|
| open from editor surface | Timeline の選択状態に応じて `Ease+` button を表示し、Animation menu にも起動 action がある | 実装済み |
| apply updates selected keyframe | dialog callback が selected markers の interpolation change records を生成し、実際の composition property に適用する | 実装済み（静的確認） |
| undo / redo valid | `ApplyInterpolationCommand` が before/after records を保持し、undo/redo を実装する | 実装済み（静的確認） |
| selection stability | dialog 起動時に selection/markers を capture する経路はあるが、dialog 中の外部 selection 変更は未実行確認 | 部分実装・検証待ち |

**判定**: Phase 3 の実装要件はソース上達成。runtime の apply、undo、redo と selection 競合は未検証。
