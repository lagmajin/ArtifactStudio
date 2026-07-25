# MILESTONE: EasingLab - Easing Comparison Tool

> 2026-04-21 Created

## Objective

Create a specialized UI ("EasingLab") that allows simultaneous comparison of multiple easing candidates for a selected keyframe segment, enabling quick visual decision-making and application of animation curves.

---

## Background

Currently, the animation system primarily supports linear interpolation or requires manual curve adjustment in the graph editor. For motion graphics, finding the "right feel" for a transition often involves repetitive trial-and-error. EasingLab speeds up this workflow by providing a side-by-side preview of common easing presets.

---

## Phase 1: Basic Comparison & Application

### Goals
- [ ] Implement `EasingLabDialog` for side-by-side comparison.
- [ ] Define `EasingCandidate` and presets (Linear, EaseIn, EaseOut, EaseInOut, Back, Expo).
- [ ] Create `EasingPreviewWidget` for lightweight animation previews.
- [ ] Synchronized scrubbing across all candidates.
- [ ] One-click application of easing to the original keyframe segment.

### Non-Goals (Out of Scope for initial implementation)
- Complex multi-segment editing.
- AI-based recommendations.
- Interactive Bezier editing within the lab (presets only).
- Permanent layer cloning (use preview instances/lightweight data).

---

## Technical Design

### Data Structures

```cpp
enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Back,
    Expo
};

struct EasingCandidate {
    QString name;
    EasingType type;
};
```

### Responsibility Boundaries

- **EasingLabDialog**: Orchestrates selection, preview tiling, and application.
- **EasingPreviewWidget**: Handles rendering of a single candidate's preview (shape movement + curve visualization).
- **EasingCurveUtil**: Shared logic for calculating eased values based on standard formulas.
- **applyEasingToSelectedSegment()**: Utility function to modify the actual keyframe data and trigger Undo.

---

## Tasks

### 1. Core Logic & Presets
- [ ] Implement `EasingCurveUtil` with standard easing functions.
- [ ] Define `EasingCandidate` list.

### 2. UI Components
- [ ] Create `EasingPreviewWidget` (QPainter drawing).
- [ ] Implement `EasingLabDialog` layout (grid).
- [ ] Top scrub slider for synchronized preview.

### 3. Integration
- [ ] Integration with `ArtifactTimelineWidget` or `PropertyEditor` to identify selected keyframes.
- [ ] Apply logic using the existing Command/Undo system.

---

## Success Criteria
- EasingLab can be opened from the Timeline/Inspector.
- At least 6 easing candidates are visible with live (scrubbable) previews.
- Clicking "Apply" correctly updates the selected keyframe's easing in the actual composition.

---

## Links

- [Phase 1](MILESTONE_EASING_LAB_PHASE1_2026-04-21.md)
- [Phase 2](MILESTONE_EASING_LAB_PHASE2_2026-04-21.md)
- [Phase 3](MILESTONE_EASING_LAB_PHASE3_2026-04-21.md)

---

## Static audit follow-up (2026-07-25)

現行ソースを確認した。Easing core、比較ダイアログ、同期 scrub、Timeline/Animation menu の入口、選択 keyframe への apply callback が存在する。

| 項目 | 現状 | 判定 |
|---|---|---|
| EasingCurveUtil / presets | `EasingCandidate`、標準 easing 評価、6 件以上の default candidate がある | 実装済み |
| Preview UI | `EasingPreviewWidget` が曲線と進行位置を描画する | 実装済み |
| Side-by-side comparison | `EasingLabDialog` が候補を grid 表示する | 実装済み |
| Synchronized scrubbing | dialog の scrub slider が全 preview に進行値を反映する | 実装済み |
| Apply integration | Timeline の selected keyframe を対象に `EasingLabDialog` の callback から easing 適用経路がある | 実装済み（静的確認） |
| Undo / runtime result | 実行時に実際の composition が更新されることと undo 復元は未実行確認 | 検証待ち |

**判定**: Phase 1 の実装要件はソース上達成。runtime/build 検証のみ未実施のため、完全完了ではなく「静的実装済み・実行検証待ち」とする。
