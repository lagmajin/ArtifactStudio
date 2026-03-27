# マイルストーン: Search / Collections / Smart Organization

> 2026-03-28 作成

## 目的

Asset Browser と Project View に散らばる素材を、探す・まとめる・絞る・見直すための仕組みに揃える。

このマイルストーンは、`favorite` / `recent` / `unused` / `missing` / `duplicate` / `dependency` を
単なる表示ラベルではなく、実際に再利用できる collections と smart filters にする。

---

## 背景

Project / Asset workflow は導線が整いつつあるが、素材数が増えると「見つける」「まとめる」の層が足りない。

- 検索がその場しのぎになりやすい
- `recent` / `favorite` / `unused` が UI ごとに分かれる
- `missing` / `duplicate` / `dependency` を整理する視点が弱い
- browse と organize の責務が混ざりやすい

---

## 方針

### 原則

1. Search は一時的な発見、Collections は継続的な整理として分ける
2. Smart bin は自動分類の入口であって、手動フォルダを置き換えない
3. `missing` / `unused` / `duplicate` は単なる badge ではなく action へつなぐ
4. Project View と Asset Browser で collection の意味を揃える

### 想定対象

- global search
- smart bin
- tag
- favorite
- recent
- unused
- duplicate
- missing
- dependency

---

## 既存資産

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Project/ArtifactProjectModel.cppm`
- `Artifact/src/Service/ArtifactProjectService.cpp`
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`

---

## Phase 1: Search Surface Baseline

### 目的

素材を素早く見つける基本導線を揃える。

### 作業項目

- search の対象を project / asset / collection で揃える
- filter と search の状態を明示する
- type / status / path / dependency を横断検索できるようにする
- 空結果の案内を整える

### 完了条件

- 何を絞っているかが分かる
- 似た名前の素材を辿れる
- search が browse を壊さない

---

## Phase 2: Collections / Virtual Views

### 目的

よく使う視点を固定コレクションとして扱えるようにする。

### 作業項目

- `Recent`
- `Favorites`
- `Unused`
- `Missing`
- `Dependencies`
- `All Media` / `All Images` のような仮想分類

### 完了条件

- collection を切り替えても selection の意味が崩れない
- browser と project で同じ概念を同じ名前で読める

---

## Phase 3: Smart Organization

### 目的

単純な folder 整理を越えて、状態ベースの整理を可能にする。

### 作業項目

- smart bin の入口
- duplicate / missing の整理 action
- dependency を元にした再編成
- favorite / recent の再学習

### 完了条件

- 整理のための入口が見える
- 手作業の folder 整理と自動分類が競合しない

---

## Phase 4: Review Hooks

### 目的

検索結果や collection から次の作業へ繋ぐ。

### 作業項目

- Contents Viewer への open
- timeline への追加
- render queue への投入
- missing / unused / duplicate の action surface

### 完了条件

- 探して終わりにならない
- collection が workflow に接続される

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

---

## Current Status

2026-03-28 時点で、Project / Asset workflow の導線は整いつつあるが、検索と整理の視点はまだ別の改善として残っている。

この文書は、`Asset Browser Improvement` と `Project / Asset Workflow` の上に乗る整理層として扱う。

---

## Related

- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
