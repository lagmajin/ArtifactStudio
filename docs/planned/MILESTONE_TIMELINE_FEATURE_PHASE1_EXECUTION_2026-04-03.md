# マイルストーン: Timeline Feature Implementation Phase 1 Execution

> 2026-04-03 作成

## 目的

`M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 1 を、実装順がぶれない粒度に落とす。

この文書は timeline の「見えている状態」を安定させる初手として、current / selected / playhead の同期と layer row の表示責務をまとめる。

---

## Phase 1 の範囲

### 1-1. Selection / Current Sync

current composition / current layer / selected layer を timeline 側で同じ状態として扱う。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

完了条件:

- current layer が変わったときに timeline 左ペインと右ペインの表示がずれない
- selected layer の強調が追従する

### 1-2. Playhead Boundary

playhead の責務を selection と分ける。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`

完了条件:

- playhead 移動で selection が壊れない
- scrub / seek / playback が同じ位置を見ている

### 1-3. Layer Row State

layer row の visible / hidden / solo / locked を安定して見せる。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

完了条件:

- row state の見え方が状態変化に追従する
- 左右ペインで状態差が出ない

### 1-4. Left / Right Selection Affordance

左ペインと右ペインの選択状態の対応を強める。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

完了条件:

- どの row が右側のどの clip に対応するかが読める
- hover / selected の表現が崩れない

---

## 実装順

1. Selection / Current Sync
2. Playhead Boundary
3. Layer Row State
4. Left / Right Selection Affordance

---

## リスク

- current layer と selected layer を混同すると inspector 連携が壊れる
- playhead と selection の責務が曖昧だと、検索や keyframe 表示にも波及する
- 先に描画だけ変えると、state mismatch が残る
