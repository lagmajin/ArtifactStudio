# マイルストーン: Timeline Feature Implementation Phase 4 Execution

> 2026-04-03 作成

## 目的

`M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 4 を、visual language / affordance の実行粒度に落とす。

この文書は timeline の「何が何を示しているか」を見た目で固定する初手として、layer bar / keyframe / playhead / selection の表現をまとめる。

---

## Phase 4 の範囲

### 4-1. Layer Bar Semantics

layer type ごとの色と意味を揃える。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

完了条件:

- video / audio / text / effect の見え方が揃う
- selection 時の強調が崩れない

### 4-2. Keyframe Marker Semantics

keyframe marker の形と色を意味ごとに固定する。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

完了条件:

- 通常 / selected / eased / disabled が判別できる
- hit area と見た目が一致する

### 4-3. Playhead Consolidation

playhead の描画責務を一本化する。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

完了条件:

- 二重表示がない
- line / badge / overlay の役割が混ざらない

### 4-4. Left / Right Affordance

左ラベルと右バーの対応を強める。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

完了条件:

- 左ラベルと右 clip の対応が読める
- hover / selected / inactive が崩れない

---

## 実装順

1. Layer Bar Semantics
2. Keyframe Marker Semantics
3. Playhead Consolidation
4. Left / Right Affordance

---

## リスク

- 色を増やしすぎると意味が薄れる
- keyframe の状態色が selection と競合しやすい
- playhead の表現を増やしすぎると他要素を邪魔する

