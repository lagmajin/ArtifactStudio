# マイルストーン: Project / Asset Workflow

> 2026-03-27 作成

## 目的

Project View と Asset Browser を別の UI ではなく、同じ制作ワークフローの入口として揃える。

このマイルストーンは、import / relink / missing / unused / recent / favorite / dependency を横断して、
「素材がどこにあるか」「今どういう状態か」「どう次へ進めるか」を一貫して読めるようにする。

---

## 背景

現状でも project / asset の基盤はあるが、分散している。

- Project View は project 構造の把握に強い
- Asset Browser は素材探索に強い
- Contents Viewer は個別ファイルの確認に強い
- Source Abstraction は missing / relink の基盤を持つ

ただし、これらが workflow としてまとまっていないため、次の操作が途切れやすい。

- import した asset が project にどう反映されたか分かりにくい
- missing / unused / relink state の見え方が UI ごとに揃っていない
- browser から timeline / viewer / render への導線が弱い
- save / load 後の asset 状態確認が面倒

---

## 方針

### 原則

1. Project View は構造の正、Asset Browser は探索の正として扱う
2. source state は Core の source / relink 基盤に寄せる
3. UI は状態の表示と次の操作の導線に徹する
4. import / relink / missing / unused の意味を揃える
5. 迷いやすい操作は context menu ではなく明示 action で用意する

### 想定対象

- project / composition / folder / bin
- image / video / audio / vector / source file
- missing / unused / relinked / imported
- recent / favorite / dependency

---

## 既存資産

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/src/Project/ArtifactProjectModel.cppm`
- `Artifact/src/Service/ArtifactProjectService.cpp`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md`
- `docs/planned/MILESTONE_SOURCE_ABSTRACTION_CORE_2026-03-25.md`
- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`

---

## Phase 1: Selection / State Sync Foundation

### 目的

Project View と Asset Browser の selection と state を一致させる。

### 作業項目

- current composition と selected asset の同期
- browser / project で同じ item を指せるようにする
- missing / unused / relinked / imported の state 表示を揃える
- selection change 時の preview / inspector 更新を明示化する

### 完了条件

- どちらの view から選んでも同じ item が追える
- selection を切り替えた時に state がズレない

---

## Phase 2: Import / Relink / Missing Flow

### 目的

素材の取り込みと復帰を、project workflow として完結させる。

### 作業項目

- import 結果の即時反映
- relink candidate の列挙
- bulk relink
- missing asset search root
- drag & drop import の整理

### 完了条件

- import と relink が別の操作に見えない
- missing から復帰までの導線が短い

---

## Phase 3: Asset Presentation / Metadata

### 目的

project item の状態を、一覧だけで読めるようにする。

### 作業項目

- thumbnail / type icon / size / duration / fps / resolution 表示
- dependency badge
- source path / relink state 表示
- empty / unsupported / load failure の区別

### 完了条件

- asset の種類と重要状態が見える
- Project View と Asset Browser の見え方が大きく乖離しない

---

## Phase 4: Organization / Collections

### 目的

素材のまとまりを、project と browser の両方で扱いやすくする。

### 作業項目

- folder / bin / tag / favorite / recent
- virtual collections
- unused / duplicate / missing の整理
- smart bin の入口

### 完了条件

- 素材を探す経路と整理する経路が両方ある
- folder と collection の責務が混ざらない

---

## Phase 5: Workflow Bridges

### 目的

Project / Asset から次の作業へすばやく飛べるようにする。

### 作業項目

- browser から timeline への追加
- browser から contents viewer への open
- project から render queue への投入
- recent / favorite / missing asset の action surface
- project view の double-click から footage review を Contents Viewer へ送る

### 完了条件

- 素材を見つけてから使うまでが短い
- viewer / timeline / render への導線が自然
- project と asset で review 導線が違いすぎない

---

## Phase 6: Save / Restore Integrity

### 目的

再起動や再読込後も asset workflow が壊れないようにする。

### 作業項目

- imported / relinked path の復元
- missing state の保持と再評価
- selected / active composition の整合
- project validation との接続

### 完了条件

- 再読込後に state が崩れない
- missing / relink / imported の差分が維持される

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5
6. Phase 6

---

## Current Status

2026-03-27 時点で、Project View / Asset Browser / Contents Viewer の個別改善は進んでいるが、
それらを一枚の workflow として束ねる専用 milestone はまだ弱い。

この文書はその結節点として扱う。
