# マイルストーン: Timeline Feature Implementation / Interaction Surface

> 2026-04-03 作成

## 目的

`ArtifactTimelineWidget` を中心に、日常制作で触る timeline の機能群を一つの実行計画として束ねる。

このマイルストーンは、既存の個別 milestone を置き換えるものではなく、次の workstream を並行に進めるための上位 execution plan として扱う。

- Timeline layer / clip の操作
- keyframe の visibility / edit / navigation
- search / filter / state presentation
- visual language の統一
- owner-draw への段階移行

Asset Browser 側の改善は、`M-UI-21 Asset Browser Navigator / Search / Presentation Surface` を並走対象として扱う。

---

## 背景

現在の timeline は、機能ごとに以下のように分かれている。

- layer search
- keyframe editing
- search / keyframe integration
- visual language
- owner-draw migration
- QGraphicsScene elimination

これらは正しい分割だが、実装順や UI の見え方を一本の計画として追いにくい。

このマイルストーンでは、timeline を「選ぶ・見つける・打つ・動かす・読む」の surface としてまとめ、制作導線を崩さずに機能を積み上げる。

---

## 取り込み方針

### 原則

1. 既存の timeline milestone は廃止しない
2. search / keyframe / visual / owner-draw を別々の作業に分けつつ、順序だけをまとめる
3. 操作は `current layer` / `selected layer` / `playhead` の同期を壊さない
4. 表示と編集の責務を分け、先に見える化、後で編集強化を進める
5. 旧 `QGraphicsScene` 実装は互換層として残し、いきなり消さない

---

## 現状資産

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScene.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md`
- `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`
- `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md`

---

## Phase 1: Interaction Core

### 目的

timeline 上での基本状態を安定させる。

### 作業項目

- current composition / current layer / selected layer の同期を明確にする
- playhead と selection の責務境界を整理する
- layer row の visible / hidden / solo / locked 状態を安定表示する
- 左ペインと右ペインの選択状態がずれないようにする

### 完了条件

- どの layer を触っているかが常に読める
- selection / current / playhead が破綻しない

### 進行メモ

- 2026-04-05: ヘッダーに current layer だけでなく selection summary も追加し、Project View の summary と意味を揃え始めた
- 2026-04-05: current composition / selected layer / current layer の見え方を、Contents Viewer の slot / focus 表示と並ぶ状態チップとして扱う方向で整理している
- 2026-04-05: selection summary に playhead frame も入れて、scrub / playback 中もヘッダーの状態が一本化されるようにした
- 2026-04-05: current / selected / frame のヘッダー表示をクリックして遷移できるようにし、Phase 1 の navigation 導線を強化した
- 2026-04-05: search status を `Hit x/y` 表示にし、左クリックで next / 右クリックで prev に移動できるようにして Phase 3 の導線を少し進めた

---

## Phase 2: Keyframe Editing Surface

### 目的

timeline から keyframe を見て、打って、動かす導線を固める。

### 作業項目

- keyframe visibility を timeline に重ねる
- context menu / shortcut から add / remove を通す
- keyframe の前後ジャンプを通す
- drag で keyframe を移動できるようにする
- property path ごとの lane 表示を整える

### 完了条件

- timeline 上だけで keyframe 操作が完結する
- inspector 側と結果が一致する

---

## Phase 3: Search / Filter / Navigation

### 目的

layer / effect / state の探索を、制作導線として使える surface にする。

### 作業項目

- incremental search を timeline 上部に常設する
- search mode を通常表示と分ける
- search hit count / current hit を表示する
- layer / effect / tag / state の簡易 query を扱う
- keyframe navigation と search 結果を連携する

### 完了条件

- 目的の layer まで素早く辿れる
- search が単なる絞り込みで終わらない

---

## Phase 4: Visual Language / Affordance

### 目的

操作対象と状態を見た目で瞬時に判別できるようにする。

### 作業項目

- layer bar の意味色を統一する
- keyframe marker の色と形を固定する
- playhead / selection / hover / disabled の見え方を揃える
- 左ラベルと右バーの対応を強める
- keyframe 密度が高い場合の省略表示を決める

### 完了条件

- timeline の状態が色だけで読める
- selected / hovered / inactive の差が崩れない

---

## Phase 5: Owner-Draw Consolidation

### 目的

右ペインの描画責務を painter 側へ寄せ、性能と保守性を上げる。

### 作業項目

- `ArtifactTimelineTrackPainterView` を正規描画経路に寄せる
- `TimelineTrackView` / `TimelineScene` の責務を縮退する
- clip / keyframe / playhead の描画を painter 側に集約する
- hover / selection / drag の再描画を局所化する

### 完了条件

- 旧 scene ベースとの二重管理が減る
- 大量 layer でも操作感を維持しやすい

---

## 既存 milestone との関係

- `M-TL-4 Timeline TrackView Owner-Draw Migration`
- `M-TL-5 Timeline Keyframe Editing`
- `M-TL-6 Timeline Layer Search`
- `M-TL-7 Timeline Search / Keyframe Integration`
- `M-TL-8 Timeline QGraphicsScene Elimination`
- `M-TL-9 Timeline Visual Language`

この文書は、それらを順番に実行しやすくするための上位計画である。

---

## 推奨順序

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## 想定効果

- timeline の機能実装を「見える化・編集・探索・表現・移行」に分けて進めやすくなる
- Asset Browser 側の改善と並行して、制作導線の入口と出口を揃えやすくなる
- 個別 milestone の重複を避けつつ、全体の進行順を読みやすくできる
