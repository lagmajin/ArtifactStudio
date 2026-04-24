# Project View Search / Filter / Presentation Execution Memo

> 2026-04-23 作成

`ArtifactProjectManagerWidget` を、単なる project tree から「素材を探して、状態を見て、次の操作へ進める surface」に寄せるための実装メモです。

この memo は、`MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md` の内容を、実際にどのファイルから進めるかまで落としたものです。

## 目的

- 入力した瞬間に絞り込みが走る search surface を作る
- 複数条件の filter pills を Project View の中心に置く
- list / grid の見え方を整理する
- status surface で結果件数と選択状態を見せる
- `unused` を見落としにくくする

## 先に触るファイル

1. `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
2. `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`
3. 必要なら `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md`

## いま既にあるもの

- incremental search の土台
- `unused:true` / `is:unused` のような検索トークン
- type filter
- status label / selection detail label
- `Esc` で search をクリアする導線
- `contents viewer` へつなぐ detail 表示

## Phase 1: Search Surface

### 触る場所

- `ArtifactProjectManagerWidget.cppm`

### やること

- incremental search の挙動を確認する
- file name / tag / metadata search の見え方を整理する
- clear button と placeholder を読みやすくする
- search state を status に出す

### 完了条件

- Enter を押さなくても絞り込める
- 何を検索中かが見える
- `searchBar` の存在意義が明確になる

## Phase 2: Filter Surface

### 触る場所

- `ArtifactProjectManagerWidget.cppm`

### やること

- `すべて / コンプ / 映像 / 画像 / 音声 / 3D / 未使用` の切り替えを整理する
- multi-select の必要性を確認する
- `unused` の強調を読みやすくする
- list / grid toggle を filter surface と一体で扱う

### 完了条件

- `映像 + 未使用` のような組み合わせが想像しやすい
- 条件が増えても surface が破綻しない

## Phase 3: Content Presentation

### 触る場所

- `ArtifactProjectManagerWidget.cppm`

### やること

- list view / grid view の見え方を揃える
- type icon / type badge の差を読みやすくする
- unused marker の視認性を上げる
- item hover / selection の読みやすさを上げる

### 完了条件

- 何の素材かが見ただけで分かる
- unused が見落としにくい

## Phase 4: Status Surface

### 触る場所

- `ArtifactProjectManagerWidget.cppm`

### やること

- 表示件数を出す
- フィルター状態を出す
- 選択アイテムの容量合計を出す
- selection detail を「今どこを見ているか」に寄せる

### 完了条件

- フィルタ条件と結果件数が追える
- 選択の重さが分かる

## Phase 5: Polish

### 触る場所

- `ArtifactProjectManagerWidget.cppm`
- `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`

### やること

- row hover
- selection highlight
- sort affordance
- view mode transition
- search / filter / view mode の切り替え感を揃える

### 完了条件

- 毎日使っても視認性が崩れない
- 探す / 絞る / 見る の導線が自然になる

## 実装順のおすすめ

1. `ArtifactProjectManagerWidget.cppm` の search bar / filter bar 周辺
2. `ArtifactProjectManagerWidget.cppm` の proxy model / filter model 周辺
3. `ArtifactProjectManagerWidget.cppm` の status / selection detail 周辺
4. `ArtifactProjectManagerWidget.cppm` の list / grid presentation 周辺

## 注意点

- 既に incremental search があるので、まずは責務の整理を優先する
- いきなり Asset Browser と完全統合しない
- `QSS` は追加しない
- 新しい signal / slot は増やさない

## ひとことメモ

- この milestone の本質は「一覧を増やす」ことではなく「探す理由を早く見せる」こと
- Project View は制作導線の入口なので、背骨に直結する改善だけ残す
