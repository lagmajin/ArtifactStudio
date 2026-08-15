# Project View Search / Filter / Presentation

**最終更新:** 2026-08-15
**ステータス:** 実装完了（runtime検証待ち）

> 2026-04-03 作成

`ArtifactProjectManagerWidget` を、単なる project tree から「素材を探して、状態を見て、次の操作へ進める surface」に寄せるための milestone です。

この workstream は、ユーザー要望にあった仕様のうち、制作現場で効きやすい部分だけを取り込む。

---

## 取り込む要素

- インクリメンタルサーチ
- ファイル名 / タグ / メタデータ検索
- 多重フィルタピル
- `未使用` アイテムの強調表示
- list / grid 表示切替
- ステータスバーの件数 / フィルター状態 / 選択容量表示

---

## 取り込まない要素

この milestone では、以下は明示的に後回しにする。

- タイムライン使用箇所への直接ジャンプ
- 高度な review / compare 専用 UI
- コンテンツエリア内のミニプレビュー実装
- 完全な Asset Browser 統合

---

## 背景

現行の Project View は、構造把握や基本操作には十分近いが、素材を探すための表面がまだ弱い。

特に以下が不足しやすい。

- 入力しながら結果が変わる search surface
- `映像 + 未使用` のような複合フィルタ
- 一目で unused が分かる表現
- list / grid の見え方切替
- いま何件絞れているかの説明

Resolve / Modo / Nuke の project surface は、一覧そのものよりも「今どの条件で見ているか」を常に見せる。
この milestone はその方向に寄せる。

---

## Surface Layout

### 1. Search Bar

- 高さ 30px 程度
- `検索...` プレースホルダー
- 入力即反映の incremental search
- クリアボタン
- フィルタ中は背景を少し変える

### 2. Filter Bar

- 高さ 28px 程度
- 横スクロール可能な filter pills
- `すべて / コンプ / 映像 / 画像 / 音声 / 3D / 未使用`
- 複数同時選択可能
- list / grid toggle を右端に置く

### 3. Content Area

- list 表示
- grid 表示
- type icon / type badge / duration / unused marker

### 4. Status Bar

- 表示件数
- フィルター状態
- 選択アイテムの容量合計

---

## Recommended Order

1. Search / filter model
2. Status surface
3. List presentation
4. Grid presentation
5. Unused emphasis

---

## Phase 1: Search Surface

### 目的

入力した瞬間に絞り込みが走る search surface を作る。

### 実装項目

- incremental search
- file name / tag / metadata search
- clear button
- search state の可視化

### 完了条件

- Enter を押さなくても絞り込める
- 何を検索中かが見える

---

## Phase 2: Filter Surface

### 目的

複数条件の filter pills を Project View の中心にする。

### 実装項目

- `すべて / コンプ / 映像 / 画像 / 音声 / 3D / 未使用`
- multi-select
- `未使用` のオレンジ強調
- list / grid toggle

### 完了条件

- `映像 + 未使用` のような組み合わせで探せる
- 条件が増えても surface が破綻しない

---

## Phase 3: Content Presentation

### 目的

一覧の見え方を、素材の種類と状態が分かる presentation にする。

### 実装項目

- list view
- grid view
- type icon の差別化
- type badge
- unused marker

### 完了条件

- 何の素材かが見ただけで分かる
- unused が見落としにくい

---

## Phase 4: Status Surface

### 目的

いまどのくらい絞れているか、何を選んでいるかを surface に出す。

### 実装項目

- 表示件数
- フィルター状態
- 選択アイテムの容量合計

### 完了条件

- フィルタ条件と結果件数が追える
- 選択の重さが分かる

---

## Phase 5: Polish

### 目的

surface を実運用に耐えるところまで詰める。

### 実装項目

- row hover
- selection highlight
- sort affordance
- view mode transition

### 完了条件

- 毎日使っても視認性が崩れない
- 探す / 絞る / 見る の導線が自然になる

---

## Current Entry Points

- [Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm)
- [Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx)
- [Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md)

---

## Next Execution Slice

Phase 1 は、検索語が入力された瞬間に surface が変わることを先に固める。

### Phase 1A の着手点

1. incremental search を `Enter` なしで反映する
2. file name / tag / metadata の 3 系統を同じ検索状態で扱う
3. clear button で search state を 1 箇所から戻せるようにする
4. 検索中であることを見た目で分かるようにする

### Phase 1 完了条件

- Enter を押さなくても絞り込める
- 何を検索中かが見える
- search state が 1 本にまとまる

### Phase 2A の着手点

1. `すべて / コンプ / 映像 / 画像 / 音声 / 3D / 未使用` の pill を並べる
2. multi-select の状態遷移を先に固定する
3. `未使用` を強調表示して検索語と競合させない
4. list / grid toggle は filter state の延長として扱う

### Phase 2 完了条件

- `映像 + 未使用` のような組み合わせで探せる
- 条件が増えても surface が破綻しない
- filter state が search state と別れて読める

### Phase 3 への前提

- list / grid presentation は search / filter の state が固まってから入れる
- status surface は検索結果の意味が揃ってからつなぐ

## Static Audit (2026-08-15)

現行の Project View には、入力即時反映・クリア可能な検索欄、name/path/metadata を含む検索 blob、`tag:` / `unused:true` / `is:unused` / `missing:true` / `is:missing` の advanced filter、タイプフィルタ、未使用 asset の非同期スナップショット、Tree／Tile 表示切替、Tile のサムネイル・type/status/proxy badge、列ソートと列幅調整が実装されている。選択概要ラベルと選択詳細／プレビュー面も存在する。Missing は一覧マーカーと色で表示され、連番の欠損 frame も判定する。

選択容量合計は Footage の実ファイルを表示し、連番は `sequencePaths` 全体を合算する。Type filter、Tree/Tile、Unused only、sort column/direction、column widths は `QSettings` に保存・復元し、検索語も browse context に表示する。`type:footage,composition` のような複数 type 条件は advanced filter の OR 照合を実装済みだが、仕様上の list / grid 名称とは現行コードの Tree / Tile 名称が一致せず、複数 filter pill の独立した multi-select surface、画像・音声・3D 等の全タイプ選択、未使用件数の包括的表示、view transition、実データでの複合条件、大量アイテム時の非同期更新、runtime 視認性は未検証である。`tag:` 検索は検索語経路のみで、タグの入力・管理 UI は確認できない。

判定: **検索・type／unused／missing filter、Tree／Tile、status表示、フィルター状態サマリーまで実装。独立したpill操作、runtime検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。検索は `textChanged` による即時反映と clear／Esc を持ち、name／path／metadata、`type:`／`tag:`／`regex:`／`unused:true`／`used:true`／`used:false`／`missing:true` を同じ proxy filter で評価する。Tree／Tile 切替、unused／missing emphasis、件数・filter・選択容量の status surface、sort／column width／view mode／filter の QSettings 保存も確認した。独立した multi-select filter pill、list/grid 名称の統一、全 asset type の pill、view transition、複合条件の runtime 視認性は未完了または未検証とする。
