# マイルストーン: Contents Viewer 拡充

> 2026-03-27 作成

## 目的

`ArtifactContentsViewer` を、単なるファイルプレビューではなく、2D / 3D / 動画 / 画像を横断して確認できるアプリ層の inspection viewer として育てる。

このマイルストーンは、`ArtifactContentsViewer` の責務を明確にしつつ、`Artifact3DModelViewer`、`QMediaPlayer`、画像表示、音声再生、比較、メタデータ表示を整理する。

---

## 背景

現状の Contents Viewer は以下を既に持つ。

- image preview
- video playback
- audio playback
- 3D model preview
- zoom / rotation
- playback range

ただし、用途が増えるにつれて責務が混ざりやすい。

- 2D 画像と 3D モデルで操作感が揃っていない
- 動画の再生状態と file state の区別が薄い
- 音声アセットを viewer 内で即再生できる導線がない
- source / metadata / duration / fps / resolution の見え方が弱い
- `Source` / `Final` / `Compare` の視点が整理されていない
- project / asset / inspector との接続がまだ薄い

---

## 方針

### 原則

1. Contents Viewer は「編集」ではなく「確認」に徹する
2. 表示対象ごとの責務を分ける
3. 2D / 3D / video / audio の切替を同じ UI パターンに寄せる
4. メタデータ表示は viewer の補助情報として扱う
5. project / asset / inspector から自然に開けるようにする

### 想定対象

- image
- video
- audio
- 3D model
- generated asset
- source file

---

## 既存資産

- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

---

## Phase 1: Responsibility Split

### 目的

`ArtifactContentsViewer` の中で、表示対象ごとの責務を整理する。

### 作業項目

- image / video / audio / model の表示分岐を明確化する
- `resetCurrentMode()` を「状態初期化」として安定させる
- zoom / rotation / playback range の state を対象別に整理する
- info label の役割を「補助メタデータ表示」に固定する

### 完了条件

- どの file type で何が表示されるか説明できる
- 状態リセットで前の対象の残骸が残らない
- viewer の state と preview 対象が混ざらない

### 進捗

- 2026-03-27 時点で、video / 3D model の heavy page を first use まで遅延生成する方向に進めた
- 2026-04-03 時点で、audio playback を Contents Viewer の対象に含める方針を追加した
- 2026-03-27 時点で、header / action state は source / final / compare を意識した整理に寄せている

### 進捗メモ

- 2026-03-27: header / metadata / state / action bar を追加
- 2026-03-27: image / video / model の state reset を維持したまま、操作導線を viewer 内へ集約
- 2026-03-27: video 用の scrub slider を追加し、再生位置を確認・操作できるようにした
- 2026-04-03: audio playback を Contents Viewer の対象として扱う追加方針を入れ、再生系の scope を拡張した
- 2026-04-03: audio playback を `MediaPlaybackController` / FFmpeg backend で実際に動かす実装に着手した
- 2026-04-04: audio file で live waveform preview surface を追加し、再生だけでなく確認面としての見え方を強めた
- 2026-04-05: compare mode の wipe / swap 状態を永続化し、A/B 比較を再訪時に維持できるようにした
- 2026-03-27: `Reset` 操作を追加し、image / video / 3D model の表示状態を各タイプごとに戻せるようにした
- 2026-03-27: AppMain から `Contents Viewer` を dock として開けるようにし、Asset Browser の double-click で file を直接送れるようにした
- 2026-03-27: Project View の double-click からも footage を `Contents Viewer` へ送れるようにし、project / asset / viewer の接続を強めた

---

## Phase 2: Unified Metadata Surface

### 目的

画像・動画・音声・3D モデルに共通する情報を viewer で見えるようにする。

### 作業項目

- file path / source path 表示
- file size / resolution / duration / fps / sample rate の表示
- audio file の基本情報と再生状態の表示
- missing / unsupported / load failure の区別
- 2D / 3D / video / audio の簡易 badge
- project asset との関連表示

### 完了条件

- viewer だけで最低限の file 状態が読める
- missing / unsupported / loaded の違いが一目で分かる

### 進捗メモ

- 2026-03-27: file name / type badge / size / path / duration / resolution の表示を追加
- 2026-03-27: copy path / open containing folder の補助導線を追加
- 2026-03-27: video の position / duration を state 表示に反映した
- 2026-04-03: audio file の basic info と playback state を viewer metadata に含める方針を追加した
- 2026-03-27: reset 操作で image / video / 3D model の view state を揃えた
- 2026-03-27: `Contents Viewer` を Asset Browser の double-click から開けるようにして workflow に接続した
- 2026-03-27: `Contents Viewer` を Project View の double-click からも開けるようにして、project / asset からの review 導線を揃えた

---

## Phase 3: Source / Final / Compare Views

### 目的

Contents Viewer を「source を見る」「処理後を確認する」「比較する」の 3 視点へ拡張する。

### 作業項目

- `Source` view
- `Final` view
- `Compare` view
- playback / scrub との整合
- 3D model の view state との差を明確化

### 完了条件

- viewer 内で view モードを切り替えられる
- 2D / 3D / video の比較視点が揃う

---

## Phase 4: Project / Asset Integration

### 目的

Project / Asset から Contents Viewer へ自然に飛べるようにする。

### 作業項目

- asset browser からの open
- project manager からの open
- double click / context menu の整理
- recent / favorite / missing asset の扱い

### 完了条件

- viewer を「開く場所」が複数でも操作が迷わない
- asset browser と project manager から同じ viewer を開ける

---

## Phase 5: Playback / Diagnostics Polish

### 目的

動画・音声・モデル・画像の再生/検証を、実用的な確認窓として仕上げる。

### 作業項目

- playback state の表示
- audio playback の停止 / 再開 / シーク / ループの整理
- loop / range / seek の整理
- 3D model camera state の記録
- error diagnostics の文言整理
- screenshot / export の補助導線
- audio waveform / live preview の整理

### 完了条件

- viewer が「何を見ているか」と「なぜ見えないか」を説明できる
- diagnostic widget と役割が衝突しない

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Related

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`
- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md`
- `Artifact/docs/MILESTONE_PRIMITIVE3D_RENDER_PATH_2026-03-21.md`
