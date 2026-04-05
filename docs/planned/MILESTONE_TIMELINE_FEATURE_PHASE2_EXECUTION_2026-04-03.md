# マイルストーン: Timeline Feature Implementation Phase 2 Execution

> 2026-04-03 作成

## 目的

`M-TL-10 Timeline Feature Implementation / Interaction Surface` の Phase 2 を、keyframe editing の実行粒度に落とす。

この文書は timeline の「打つ・見る・飛ぶ」を固める初手として、keyframe visibility / add-remove / navigation / lane 表示をまとめる。

---

## Phase 2 の範囲

### 2-1. Keyframe Visibility

selected layer の keyframe を timeline 上で見える状態にする。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

完了条件:

- property path ごとの marker が見える
- current frame と marker の位置関係が分かる

### 2-2. Add / Remove / Jump

timeline から keyframe を追加・削除・前後移動できるようにする。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

完了条件:

- playhead 位置で keyframe を打てる
- context menu / shortcut から remove できる
- 前後の keyframe へジャンプできる

### 2-3. Lane Coverage

主要 property を lane として timeline に展開する。

対象:

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

完了条件:

- transform / opacity / effect parameter などが lane に乗る
- property 名と lane 表示が一致する

### 2-4. Search / Keyframe Link

search 状態から keyframe への導線を繋ぐ。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

完了条件:

- search hit から keyframe を辿れる
- search state と keyframe visibility が矛盾しない

---

## 実装順

1. Keyframe Visibility
2. Add / Remove / Jump
3. Lane Coverage
4. Search / Keyframe Link

---

## リスク

- keyframe 表示だけ先に立てると、編集 API とズレやすい
- lane の意味が property editor と食い違うと操作が迷子になる
- search との連携が遅れると、見つけても辿れない状態になる

