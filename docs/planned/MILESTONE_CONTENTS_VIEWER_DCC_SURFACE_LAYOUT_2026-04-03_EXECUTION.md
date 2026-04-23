# Contents Viewer DCC Surface Layout Execution Memo

> 2026-04-23 作成

`ArtifactContentsViewer` を、単なるプレビュー枠ではなく制作向け inspection surface として整えるための実装メモです。

この memo は、`MILESTONE_CONTENTS_VIEWER_DCC_SURFACE_LAYOUT_2026-04-03.md` の内容を、実際にどの関数・どの段階から進めるかまで落としたものです。

## 目的

- viewer の情報を 1 か所に密集させない
- 2D / 3D / audio の表示責務を分ける
- recent source の再選択導線を強くする
- A/B 比較と wipe 比較を viewer 内で完結させる
- channel / meta を inspection surface として見せる

## 先に触るファイル

1. `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
2. 必要なら `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

## いま既にあるもの

- `updateHeader()`
- `updatePlaybackState()`
- `updateActionAvailability()`
- `updateModeButtons()`
- `loadRecentSources()`
- `refreshRecentSourceCombo()`
- `loadCompareSurfaceState()`
- `saveCompareSurfaceState()`
- `loadViewerAssignmentState()`
- `saveViewerAssignmentState()`
- `updateViewerBadge()`
- `updateSurfaceMeta()`
- `updateChannelMetaSurface()`
- `ensureCompareWidgets()`
- `updateCompareWipe()`
- `updateCompareSurface()`

## Phase 1: Surface Shell

### 触る場所

- `ArtifactContentsViewer.cpp`

### やること

- タイトルバー / 本体 / transport / channel-meta の 4 段構成を明示する
- recent source dropdown の入口を整える
- viewer badge を中央に置く
- GPU / FPS 表示の見え方を整える

### 完了条件

- viewer の情報が 1 か所に密集しない
- どの素材を見ているかが上部だけで分かる

## Phase 2: Mode Routing

### 触る場所

- `ArtifactContentsViewer.cpp`

### やること

- MIME / 拡張子 / backend 属性から mode を決める helper を確認する
- `updateHeader()` に mode badge を統合する
- 2D / 3D / audio の input 分岐を `Impl` 側へ寄せる
- background toggle を mode 共通の surface にする

### 完了条件

- viewer が今どの種類を見ているかを説明できる
- mode switch で操作の意味が混ざらない

## Phase 3: Transport and Time Surface

### 触る場所

- `ArtifactContentsViewer.cpp`

### やること

- transport button row を専用 surface として扱う
- timecode input を導入する
- `In / Out` と `Duration` を並べる
- loop / speed を右端にまとめる

### 完了条件

- playback / range / seek が viewer 内で一貫する
- 編集前提の review がしやすい

## Phase 4: Channel and Meta Surface

### 触る場所

- `ArtifactContentsViewer.cpp`

### やること

- channel pills を mode 別に切り替える
- cursor sample 表示を更新する
- 3D 時は world coordinates を出す
- audio 時は channel 表示を L / R / Mid / Side に変える
- unsupported / missing / no-data の案内を読むやすくする

### 完了条件

- viewer が「画を出すだけ」から一段上がる
- inspection surface としての使い道が明確になる

## Phase 5: Multi-Viewer and Wipe

### 触る場所

- `ArtifactContentsViewer.cpp`
- 必要なら `ArtifactCompositionEditor.cppm`

### やること

- viewer assignment registry を整理する
- `A / B` badge を分ける
- before / after の比較導線を固める
- wipe bar を組み込む
- viewer focus routing を自然にする

### 完了条件

- 1 画面で比較できる
- source 割り当ての導線が短い

## 実装順のおすすめ

1. `updateHeader()`
2. `updatePlaybackState()`
3. `updateActionAvailability()`
4. `updateModeButtons()`
5. `updateSurfaceMeta()`
6. `updateChannelMetaSurface()`
7. `ensureCompareWidgets()` / `updateCompareSurface()` / `updateCompareWipe()`

## 注意点

- 既存の `Source / Final / Compare` の切り替えを壊さない
- `QSS` は追加しない
- 新しい signal / slot は増やさない
- 2D / 3D / audio の入力ルールを曖昧にしない

## ひとことメモ

- この milestone の本質は「表示を増やす」ことではなく「見る / 比べる / 戻る」を短くすること
- Phase 1 と Phase 2 が通るだけでも、viewer の役割はかなり明確になる
