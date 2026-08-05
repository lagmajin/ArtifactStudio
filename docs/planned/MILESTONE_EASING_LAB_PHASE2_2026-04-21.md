> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_EASING_LAB.md](MILESTONE_EASING_LAB.md)

# EasingLab - Phase 2: Preview Widget and Dialog Layout

Date: 2026-04-21

## Purpose

Build the comparison surface that lets the user see multiple easing candidates side by side.

This phase introduces the actual `EasingLab` view, but keeps it read-only.

## Scope

- `EasingPreviewWidget`
- `EasingLabDialog`
- tiled preview layout
- synchronized scrubber
- minimal headers and labels

## Out Of Scope

- apply-to-timeline wiring
- undo stack changes
- editor mode switches
- per-curve manual editing

## Execution Steps

### 1. Build the preview widget

- draw a single easing candidate with `QPainter`
- keep the widget lightweight and self-contained
- visualize both motion and curve shape if space allows

### 2. Build the dialog

- tile at least six candidates in a grid
- keep the dialog readable on smaller windows
- show a top scrub control that drives all previews together

### 3. Keep previews synchronized

- use one normalized time value for all candidates
- make scrubbing deterministic and stable
- avoid creating heavyweight clones of the composition

## Definition Of Done

- the dialog opens and shows the initial preset set
- previews animate in sync with scrubbing
- the surface is read-only and stable

## Suggested Next Slice

After this phase lands, the next implementation slice should be:

1. connect the lab to the selected keyframe segment
2. add apply logic through the existing command/undo path
3. expose the lab entry point from timeline or inspector

---

## Static audit follow-up (2026-07-25)

`EasingPreviewWidget` / `EasingLabDialog` を確認した。Phase 2 の比較 surface は実装済みで、現行コードでは後続 Phase の apply 導線も同じ dialog に既に含まれている。

| Definition of Done | 現状 | 判定 |
|---|---|---|
| dialog opens with initial presets | Timeline と Animation menu に起動入口があり、default candidate catalog を grid にする | 実装済み |
| synchronized scrubbing | normalized slider value を全 preview に反映する | 実装済み |
| read-only stable preview | preview tile は曲線・marker を描画し、composition clone は作らない | 実装済み（静的確認） |
| phase boundary | 現行では Apply button / callback も存在し、本文の Out of Scope を超えて Phase 3 相当まで進んでいる | 更新必要 |

**判定**: Phase 2 の Done 条件はソース上達成。runtime の表示・scrub 安定性は未確認で、Apply/Undo は後続 Phase の検証対象として扱う。
