# Timeline Operation Feel Refinement Execution Memo

> 2026-04-23 作成

`ArtifactTimelineWidget` 周辺の操作感を、After Effects 風の「迷わない編集導線」に寄せるための実装メモです。

この memo は、`MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md` の実行順を、実際に触るファイル単位まで落としたものです。

## 目的

- selection を強くして、複数対象を拾いやすくする
- scroll / zoom の役割を固定して、視点操作の迷いを減らす
- property と timeline の往復を減らす
- flat keyframe view と組み合わせて、編集対象の見通しを上げる

## 先に触るファイル

1. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
2. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
3. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
4. `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
5. `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
6. `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
7. 必要なら `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## Phase 1: Selection Strengthening

### 触る場所

- `ArtifactTimelineTrackPainterView.cpp`
- `ArtifactTimelineScene.cppm`
- `ArtifactLayerPanelWidget.cpp`

### やること

- ラバーバンド選択を timeline に寄せる
- Shift / Ctrl 修飾の意味を固定する
- keyframe / clip / layer の hit test を選択意図に合わせる
- 既存の `selectedMarkerKeys_` / selection event の流れを壊さない

### 完了条件

- 複数対象を迷わず拾える
- クリックとドラッグの意味がぶれない
- marker selection の同期が崩れない

## Phase 2: Scroll / Zoom Discipline

### 触る場所

- `ArtifactTimelineWidget.cpp`

### やること

- plain scroll = pan に統一する
- `Ctrl+Scroll` = zoom に統一する
- 原点復帰 / 表示復帰の shortcut を整理する
- zoom 中でも playhead / keyframe / clip を見失いにくくする

### 完了条件

- scroll と zoom の役割が一目で分かる
- 長い timeline でも現在位置に戻しやすい

## Phase 3: Inline Property Surface

### 触る場所

- `ArtifactLayerPanelWidget.cpp`
- `ArtifactPropertyWidget.cppm`

### やること

- timeline 行に property の代表値を出す
- 編集可能な値は inline editor に寄せる
- view-only / edit-only の値を混ぜない
- `keyframe only` 表示と相性が悪い項目は補助表示へ回す

### 完了条件

- inspector に戻る回数が減る
- その場で触れる property が分かる

## Phase 4: Multi-Layer Keyframe Batch Editing

### 触る場所

- `ArtifactTimelineTrackPainterView.cpp`
- `ArtifactAbstractLayer.cppm`
- `ArtifactTimelineScene.cppm`

### やること

- multiple selection 時に keyframe を相対移動できるようにする
- 選択群のフレーム差を保った batch move にする
- snap / collision / overlap の扱いを揃える

### 完了条件

- 複数レイヤーのタイミングを同時に整えられる
- 1 点ずつ修正しなくても制作フローに乗る

## Phase 5: Property Round-Trip Polish

### 触る場所

- `ArtifactTimelineWidget.cpp`
- `ArtifactPropertyWidget.cppm`
- `ArtifactCompositionEditor.cppm`

### やること

- timeline から property へ、property から timeline へ相互に飛べるようにする
- selection の強調を両側で揃える
- いま触っている対象の由来を header / status に残す

### 完了条件

- どこを触っているか迷わない
- timeline が閲覧と編集の両方に使える

## 実装順のおすすめ

1. `ArtifactTimelineTrackPainterView.cpp`
2. `ArtifactTimelineWidget.cpp`
3. `ArtifactLayerPanelWidget.cpp`
4. `ArtifactPropertyWidget.cppm`
5. `ArtifactTimelineScene.cppm`
6. `ArtifactAbstractLayer.cppm`
7. `ArtifactCompositionEditor.cppm`

## 注意点

- 既存の selection event の流れは壊さない
- 新しい signal / slot は増やさない
- `QSS` は追加しない
- `M-TL-10 Timeline Flat Keyframe View / U-Key Style Filter` と並走させると効果が高い

## ひとことメモ

- この milestone の本質は「見た目の追加」ではなく「制作時の手の動きの統一」
- まず Phase 1 / 2 が通ると、残りの拡張がかなりやりやすくなる
