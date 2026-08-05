> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md](MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md)

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

## Static Audit (2026-07-25)

実装メモの主要な着手点は、現行 `ArtifactProjectManagerWidget.cppm` に反映されている。検索欄は textChanged で即時更新し、clear button／Esc、type filter、`tag:`・`unused` 系トークンを持つ。Project View は Tree／Tile を切り替え、Tile の type/status/proxy badge、hover／selection 表現、列ソート・幅調整を備える。上部には view mode／filter／search の概要、表示件数と選択数、選択詳細の表示がある。

2026-07-30 に Project View の type filter、Tree/Tile view mode、Unused only の設定を `QSettings` に保存・復元する導線を追加した。保存は各既存コントロールの変更時に行い、起動時にはコントロールと Project View の初期表示へ反映する。

同日、複数選択された Footage の実ファイルサイズ合計を browse context の選択状態表示へ追加した。存在するファイルだけを合算し、B〜TB の読みやすい単位で表示する。連番 Footage は `sequencePaths` 全体を合算し、代表フレームだけで過小表示しない。Composition 数や選択数と同じ既存の selection chrome 更新経路を利用している。

さらに、Project View の列ソート列と昇順／降順を `ProjectView/SortColumn`、`ProjectView/SortAscending` として保存し、次回のモデル接続時に復元するようにした。ソート矢印の内部状態も復元された値と一致する。

列幅も `ProjectView/ColumnWidth/<column>` に保存するようにした。手動リサイズとヘッダーの自動幅調整の両方で保存され、初回の列レイアウト適用時に復元される。

2026-07-30 に、検索中の語を browse context の selection summary に常時表示するようにした。検索語が空の場合は従来どおり表示せず、検索・filter・view mode の状態を同じ行で確認できる。

同日、Project View の advanced filter に `missing:true` / `is:missing` を追加した。Footage は代表パスを確認し、連番の場合は `sequencePaths` に欠損フレームが一つでもあれば missing として扱う。Composition / Folder / Solid はこの条件では一致しない。

Missing の状態は結果一覧にも `[Missing]` マーカーと赤系の前景色で表示する。Tile の badge も同じ判定を使い、連番では `sequencePaths` の欠損 frame を検出する。Unused と重なる場合は両方のマーカーを保持し、状態を一方で隠さない。

2026-07-30 に advanced filter の `type:` が `,` / `|` / `+` 区切りの複数値を受け付けるようにした。`type:footage,composition` のような条件は OR で評価され、既存の検索語、unused／missing 条件と組み合わせて使える。独立した filter pill UI は引き続き未実装。
繰り返し指定する `type:footage type:composition` も同じ OR 条件へ蓄積し、後半の prefix が前半を上書きしないようにした。

Unused の filter 照合も表示側と同じ `QDir::cleanPath()` 済みパスを使うように統一し、パス表記差で表示と結果が食い違わないようにした。

未達または未確認なのは、仕様に記載された独立した multi-select filter pills、list/grid という名称・状態の統一、条件の完全な status surface、view transition、実データでの複合 filter と視認性の runtime 検証である。設定の保存は type filter / Tree-Tile / Unused only / sort column-direction / column widths、選択容量表示は Footage の既存ファイルについて実装済みだが、他の設定や runtime 検証は未完了である。したがって、Phase 2〜5 の完成条件をすべて満たしたとは判定できない。

判定: **実装反映済み・仕上げ／検証待ち。**
