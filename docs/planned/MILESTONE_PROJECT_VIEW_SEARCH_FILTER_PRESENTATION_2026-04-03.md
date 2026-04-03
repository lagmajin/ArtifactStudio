# Project View Search / Filter / Presentation

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
