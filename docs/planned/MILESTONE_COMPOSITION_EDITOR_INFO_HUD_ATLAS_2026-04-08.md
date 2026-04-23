# Milestone: Composition Editor Info HUD Atlas

**Status:** Phase 1 In Progress
**Goal:** `ArtifactCompositionEditor` の viewport 上に、選択情報や補助情報を軽量に表示できる HUD を作り、最終的に glyph atlas ベースへ移行する

---

## 背景

コンポジットエディタには、選択レイヤー名、操作モード、スケール値、ドラッグ中の補助情報など、短い文字列を直接 viewport に出したい場面がある。

現状は `CompositionRenderController` の overlay pass に `QPainter::drawText()` を直接置く方針で、少量の表示なら十分だが、常時表示を増やすなら文字描画の責務を整理しておきたい。

---

## 目的

1. composition editor の情報表示を 1 つの HUD 経路に集約する
2. 文字列の生成と描画を分離し、後で atlas に差し替えやすくする
3. 短い HUD 表示はまず cache / static text で軽くする
4. 最終的に glyph atlas に寄せて、常時表示でも draw call と描画コストを抑える

---

## 非目的

- テキストレイヤー編集 UI の刷新
- full Graph Editor の実装
- 文字表示をすべて即座に GPU atlas 化すること
- 既存の drop ghost / scale hint を壊すこと

---

## Phase 1: HUD State Plumbing

### 目的
HUD に出す文字列を controller 側の state として持つ

### 作業項目

- `CompositionRenderController` に汎用 info HUD API を追加する
- selection / tool / operation 情報を 1 か所から渡せるようにする
- overlay pass の中で info HUD を描画する

### 完了条件

- editor 側から「情報テキスト」をセットできる
- viewport 上に固定位置の info chip が出る

---

## Phase 2: CPU Text Cache

### 目的
同じ文字列を毎フレーム描かない

### 作業項目

- info HUD の描画結果をキャッシュする
- 文字列やサイズが変わった時だけ再生成する
- `QStaticText` か等価のキャッシュレイヤーを導入する

### 完了条件

- 操作中でも HUD が安定して軽い

---

## Phase 3: Glyph Atlas

### 目的
文字を glyph 単位で atlas 化して、描画をさらに軽くする

### 作業項目

- フォント glyph の atlas 管理を導入する
- 短いラベルと数値を atlas 描画へ切り替える
- 必要なら fallback で CPU text cache を残す

### 完了条件

- 常時表示 HUD でも文字描画負荷が抑えられる

---

## 対象候補

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- 必要に応じて `Artifact/src/Render/ArtifactIRenderer.cppm`
- 必要に応じて `Artifact/src/Render/TextRenderer.ixx`

---

## 進捗メモ

- Phase 1 の入り口として、composition editor の選択状態から info HUD を controller に流し込む導線を追加した
- まずは固定位置の小さな info chip を `CompositionRenderController` の overlay pass で描く
- 次の段階で、この HUD を `QStaticText` / cache / glyph atlas の順で置き換えやすい形にしていく
