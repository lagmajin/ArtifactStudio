# マイルストーン: Contents Viewer DCC Surface Layout / A-B / Wipe

> 2026-04-03 作成

## 目的

`ArtifactContentsViewer` を、単なるプレビュー枠ではなく、Nuke / Resolve / AE 系の「制作向け inspection surface」として整える。

このマイルストーンでは、以下を 1 つの独立したウィジェット構造として固める。

- タイトルバー
- ビュー本体
- トランスポートバー
- チャンネル / メタバー

加えて、複数ビューワー運用と A/B 比較、Wipe 比較の入口も同じ文脈で扱う。

---

## 背景

現在の `Contents Viewer` は、image / video / 3D model / metadata / source-final-compare の入口を既に持っている。

ただし、制作現場で使いやすい UI としてはまだ以下が弱い。

- 情報の置き場所が散りやすい
- 2D / 3D / audio の表示責務が UI 側で明確に分かれていない
- 最近開いた素材の再選択導線が薄い
- A/B 比較や wipe 比較の構造がまだ明示されていない
- channel / meta 表示が viewer の一部として統一されていない

このマイルストーンは、`Contents Viewer Expansion` の上に乗る「完成形の surface 定義」を担う。

---

## UI 構造

### 1. タイトルバー

- 高さ約 24px
- 左端に現在の source 名 / node 名
- source 履歴の dropdown
- 中央に viewer 番号バッジ
- 右端に GPU 負荷 / FPS 表示
- mode badge を小さく表示

### 2. ビュー本体

- 2D image / video
- 3D geometry / scene
- audio waveform
- checkerboard / solid background toggle
- zoom / pan / orbit / dolly のモード整備
- 右上に zoom pill / camera mode pill / audio mode badge

### 3. トランスポートバー

- 先頭 / 逆再生 / 1 frame back / play / 1 frame forward / fast forward / 末尾
- timecode input
- In / Out
- duration 表示
- loop toggle
- playback speed dropdown

### 4. チャンネル / メタバー

- RGBA / Z / UV / Luma / audio channel の切替
- cursor sample 表示
- 3D のときは world coordinates
- viewer 状態の簡易診断

---

## 複数ビューワー運用

- viewer widget は複数並べられる
- `1` / `2` / `3` ... でフォーカス中の source を各 viewer に割り当てられる
- Before / After 比較をすぐ構成できる
- A/B バッジと割り当てを分けて、比較の意味を UI に残す

### Wipe Mode

- A viewer と B viewer を 1 面に重ねて比較する
- ドラッグラインで wipe 比較を行う
- 比較モードは viewer の中で完結させ、別ダイアログに逃がさない

---

## コンテンツ種別ごとの役割

- 2D image / video: ピクセルバッファ描画と playback / scrub
- 3D: camera / orbit / top / front / side の切り替え
- audio: waveform 全域表示と playback head 表示
- unknown / unsupported: metadata 中心の案内 surface

---

## Phase 1: Surface Shell

### 目的

`Contents Viewer` の見た目を 4 段構成へ整理し、責務を見える化する。

### 実装項目

- タイトルバー / 本体 / transport / channel-meta の 4 段レイアウトを明示
- viewer 番号バッジを導入
- source 名 / mode badge / FPS / GPU 負荷のレイアウトを整理
- recent source dropdown の入口を用意する

### 完了条件

- viewer の情報が 1 か所に密集しない
- どの素材を見ているかが上部だけで分かる

---

## Phase 2: Mode Routing

### 目的

2D / 3D / audio の表示ロジックを viewer 本体内で自然に切り替える。

### 実装項目

- MIME / 拡張子 / backend 属性から mode を自動判定
- mode badge を更新
- 2D / 3D / audio ごとの input 操作を整理
- checkerboard / solid background toggle を統一する

### 完了条件

- viewer が「今どの種類を見ているか」を説明できる
- mode switch で操作の意味が混ざらない

---

## Phase 3: Transport and Time Surface

### 目的

再生・停止・範囲指定・タイムコード入力を 1 本の導線にまとめる。

### 実装項目

- transport button 群
- timecode input
- In / Out
- duration 表示
- loop / speed control

### 完了条件

- playback / range / seek が viewer 内で一貫する
- 編集前提の review がやりやすい

---

## Phase 4: Channel and Meta Surface

### 目的

Nuke 的な channel / pixel inspection を viewer で扱えるようにする。

### 実装項目

- RGBA / Z / UV / Luma / audio の channel pills
- cursor sample display
- 3D 時の coordinate display
- unsupported / missing / no-data の案内

### 完了条件

- viewer が「画を出すだけ」から一段上がる
- inspection surface としての使い道が明確になる

---

## Phase 5: Multi-Viewer and Wipe

### 目的

Nuke っぽい複数 viewer 運用を本格化する。

### 実装項目

- 1 / 2 / 3 ... の viewer assignment
- A / B badge
- Before / After layout
- wipe comparison
- viewer focus routing

### 完了条件

- 1 画面で比較できる
- source 割り当ての導線が短い

### 進捗メモ

- 2026-04-05: compare view に `A / B` バッジ、`Swap`、`Tab` 切り替え、wipe slider を実装した
- 2026-04-05: `CompareWipePercent` / `CompareSidesSwapped` を `QSettings` に永続化し、比較状態を再起動後も保持するようにした
- 2026-04-05: recent source dropdown をタイトルバーへ置き、source 履歴の再選択導線を維持している
- 2026-04-05: viewer assignment combo と compare A/B assign ボタンを追加し、compare source routing を UI から直接触れるようにした
- 2026-04-05: viewer badge が focus と slot を示すようになり、割り当て状態が見える化された
- 2026-04-05: viewer badge / surface meta の文言を Project View / Timeline の selection summary と同じ「状態チップ」系の読み方に寄せている
- 2026-04-05: viewer assignment に `Ctrl+1..4`、compare A/B に `Ctrl+Shift+A/B` を追加し、Phase 5 の routing をキーボードでも触れるようにした
- 2026-04-05: channel / meta surface を追加し、image では hover probe で RGBA / XY / hex を見られるようにした
- 2026-04-05: 3D viewer の zoom / yaw / pitch / camera position を meta に出し、camera state が見えるようにした
- 2026-04-05: compare の A/B header をクリックで各 source に戻れるようにして、routing をさらに短くした
- 2026-04-05: compare mode の surface/meta に tooltip を付け、A/B 比較の操作意図が見えるようにした
- 2026-04-05: Phase 5 は現行の主戦場で、次の焦点は viewer assignment / focus routing / compare source routing の整理

---

## 推奨順

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## 関連

- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`
- `docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md`
- `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`

---

## 実装順のメモ

1. `Surface Shell`
2. `Mode Routing`
3. `Transport and Time Surface`
4. `Channel and Meta Surface`
5. `Multi-Viewer and Wipe`

この順で進めると、まず見た目と責務を固め、その上に操作系と比較系を重ねられる。

---

## 現行コードの着手点

- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
  - `headerWidget`
  - `titleLabel`
  - `typeBadgeLabel`
  - `metaLabel`
  - `stateLabel`
  - `fitButton`
  - `playButton`
  - `sourceButton / finalButton / compareButton`
  - `seekSlider`
  - `QStackedWidget` による image / video / model の切り替え
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
  - `updateHeader()`
  - `updatePlaybackState()`
  - `updateActionAvailability()`
  - `updateModeButtons()`
  - いまの UI 表示責務の中心
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
  - `wheelEvent()`
  - `mousePressEvent()`
  - `mouseMoveEvent()`
  - `mouseReleaseEvent()`
  - 2D / 3D / video の入力ポリシー整理の入口
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - composition viewer 側との compare / preview 導線整理

---

## Phase 1 着手順

1. タイトルバーを 4 段構成の最上段として確定する
2. `recent source` の dropdown を入れる
3. viewer badge を中央に置く
4. GPU / FPS 表示を右端へ移す

---

## Phase 2 着手順

1. MIME / 拡張子 / backend 属性から mode を決める helper を切る
2. `updateHeader()` に mode badge を統合する
3. 2D / 3D / audio の入力分岐を `Impl` 側へ寄せる
4. background toggle を mode 共通の surface にする

---

## Phase 3 着手順

1. transport button row を専用 surface として扱う
2. timecode input を導入する
3. `In / Out` と `Duration` を並べる
4. loop / speed を右端にまとめる

---

## Phase 4 着手順

1. channel pills を mode 別に切り替える
2. cursor sample 表示を更新する
3. 3D 時は world coordinates を出す
4. audio 時は channel 表示を L / R / Mid / Side に変える

---

## Phase 5 着手順

1. viewer assignment registry を作る
2. `A / B` badge を分ける
3. before / after の比較導線を固める
4. wipe bar を組み込む
