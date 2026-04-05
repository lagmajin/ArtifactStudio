# マイルストーン: Asset Browser Navigator / Search / Presentation Surface

> 2026-04-03 作成

## 目的

現行の `ArtifactAssetBrowser` を土台に、制作現場で使いやすい Asset Browser の表面を段階的に整える。

このマイルストーンは「完全な作り直し」ではなく、現在すでにある以下の資産を活かして拡張する。

- インクリメンタル検索
- サムネイル warmup / キャッシュ
- 未使用アセット判定
- フォルダ移動・削除系のコンテキストメニュー
- ドラッグ&ドロップ import / reorder
- ブレッドクラム型のパス表示

Unity 風の操作感を参考にしつつ、Artifact の既存の project / timeline / viewer 導線に自然につなぐことを目標にする。

---

## 取り込み方針

### 原則

1. 既存の `ArtifactAssetBrowser` を置き換えず、追加・整理で進める
2. フォルダ階層とフラット検索結果を明確に分ける
3. Asset Browser は探索の正、Project View は構造の正として役割を分ける
4. 重要な状態は見た目で読めるようにし、操作はコンテキストメニューだけに寄せない
5. Sequence grouping など重い論点は別 milestone に切り分ける

---

## 現状資産

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Asset/AssetDirectoryModel.cppm`
- `Artifact/src/Asset/AssetMenuModel.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `docs/planned/MILESTONE_ASSET_BROWSER_SEQUENCE_GROUPING_2026-03-31.md`
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`

---

## Phase 1: Navigator Shell

### 目的

左フォルダツリー / 右コンテンツの 2 ペイン構造を、より DCC らしいナビゲータとしてまとめる。

### 作業項目

- 左右 2 ペインの splitter を前提に整理する
- 左ペインに Assets ルートと Favorites セクションを持たせる
- top rail に search bar と path bar を縦に並べる
- path bar に back / forward の履歴操作を持たせる
- breadcrumb の各セグメントをクリック可能にする

### 完了条件

- フォルダを深く辿っても現在位置が分かる
- 履歴移動で往復できる
- Favorites へ素早く飛べる

---

## Phase 2: Search Mode / Flat Results

### 目的

検索時に folder 階層を無視した flat result surface を出し、探索と絞り込みを同じ UI で扱えるようにする。

### 作業項目

- search bar 入力と同時に結果をインクリメンタル更新する
- 検索中は path bar を `検索結果: "..." — N件` に切り替える
- folder tree を維持したまま、右ペインは flat な検索結果を表示する
- search mode では source / type / status の badge を残しつつ、階層表示を抑える

### 完了条件

- テキスト入力だけで目的の素材へ近づける
- 検索中に「今どこを見ているか」が結果件数とともに分かる

---

## Phase 3: Content Presentation

### 目的

グリッド / リスト / サムネイルを、制作向けに見やすく揃える。

### 作業項目

- grid / list 切替を明示化する
- thumbnail size slider を 32px - 128px の連続制御に寄せる
- slider を最小にした場合は list 表示へ自動遷移する
- list 表示では icon / name / type / size / modified time を揃える
- status bar に item count / selection count / unused count を出す

### 完了条件

- 小さい素材一覧と大きいプレビュー一覧を切り替えやすい
- 数量・選択・未使用の状態が下段で読める

---

## Phase 4: Folder Intelligence / Favorites

### 目的

フォルダをただの階層ではなく、素材の傾向を読める入口にする。

### 作業項目

- folder icon の tint を asset type mix から推定する
- 映像 / 音声 / 画像 / 3D / その他の比率をキャッシュする
- Favorites セクションを導入し、ドラッグ&ドロップで登録できるようにする
- folder context menu に create / rename / delete / reveal in explorer を揃える

### 完了条件

- よく使うフォルダへすぐ移動できる
- フォルダの中身の性質が視覚的に分かる

---

## Phase 5: Workflow Bridges

### 目的

Asset Browser から次の操作へ迷わず進めるようにする。

### 作業項目

- asset を timeline へドラッグした際の preview ghost を出す
- folder 間移動 / Ctrl ドラッグ複製を整理する
- double click で source viewer / folder navigate を分岐する
- unused asset の表示と project workflow の連携を揃える

### 完了条件

- 見つける・選ぶ・使う の距離が短い
- timeline / viewer / project への導線が自然

---

## Phase 6: Scope Boundaries

### 目的

この milestone で扱わない領域を明確にして、実装を肥大化させない。

### 対象外

- Sequence grouping の本格実装
  - これは `docs/planned/MILESTONE_ASSET_BROWSER_SEQUENCE_GROUPING_2026-03-31.md` へ分離する
- Project View の構造整理そのもの
  - これは `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md` 側で扱う
- GPU / render path の低レベル変更
  - Asset Browser の表面改善とは分離する

---

## 推奨順序

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## 想定効果

- 検索とフォルダ移動を同じ surface で扱える
- 目的の素材へ到達するまでのクリック数が減る
- 既存のサムネイル・未使用判定・DnD をそのまま活かせる
- Unity 風の「探す・絞る・見せる」操作感を取り入れやすくなる

