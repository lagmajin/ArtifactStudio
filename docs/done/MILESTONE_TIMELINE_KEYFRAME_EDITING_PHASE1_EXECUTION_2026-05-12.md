# Timeline Keyframe Editing - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`](./MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)

**Status**: 完了
**Order**: 2 of 3

Progress note:

- right-pane selection summaries now expose nearest/current proximity more clearly
- phase 1 still focuses on visibility first, but the readout is less ambiguous now

---

## Phase 1 Goal

selected layer の keyframe を timeline 上で見えるようにする。

この段階では edit actions を増やす前に、marker / lane / current frame の関係を読める状態にする。

---

## Scope

### In

- keyframe visibility
- selected lane emphasis
- current frame relation
- empty lane state

### Out

- full drag editing
- easing / curve editor
- batch copy / paste

---

## Current Boundary Note

- この Phase 1 は selected layer の keyframe を見える化するところまでに絞る
- `add / remove / move` の本格化は次の段階に回す
- `Keyframes Only` や search 状態と conflict しない可視化を先に固める
- `FrameDebug` 系の診断語彙とは別に、timeline header の summary を安定させる

---

## First Files

1. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
2. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
3. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
4. `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
5. `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

---

## First Move

1. `ArtifactTimelineTrackPainterView.cpp` で marker visibility を固める
2. `ArtifactTimelineWidget.cpp` で header summary を整える
3. `ArtifactLayerPanelWidget.cpp` で selected lane emphasis を入れる
4. `ArtifactTimelineScene.cppm` で入力状態の衝突を確認する

---

## Tasks

### 1. Marker Visibility

- property path ごとの keyframe indicator を描く
- current frame marker との関係を見える化する

### 2. Selected Lane Emphasis

- selected layer の lane を強調する
- lane が空のときの案内を決める

### 3. Header Summary

- frame / selection / keyframe の関係を header に出す
- keyframe が存在するかを即座に分かるようにする

---

## Recommended Order

1. marker visibility を先に見えるようにする
2. selected lane の強調を入れる
3. header summary を整える
4. empty lane state を整理する

---

## Done Criteria

- keyframe が見える
- current frame との関係が読める
- 次の add / remove / move に進める
