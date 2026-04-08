# マイルストーン: Timeline Right Pane Full Owner-Draw

> 2026-04-08 作成

## 目的

`ArtifactTimelineWidget` の右ペインを、`TimelineTrackView (QGraphicsView + TimelineScene + ClipItem)` の互換実装から、`ArtifactTimelineTrackPainterView (QWidget + QPainter)` を正規経路とする完全 owner-draw surface に移行する。

このマイルストーンは、タイムライン全体の上位計画である `M-TL-10 Timeline Feature Implementation / Interaction Surface` の下位 workstream として扱う。

---

## 背景

タイムライン右ペインは、すでに painter 側の実装が育っている一方で、内部にはまだ scene / item ベースの互換層が残っている。

その結果、以下が同時に起こりやすい。

- 描画責務が二重化する
- input / selection / playhead の同期が分散する
- clip 数や keyframe 数が増えると、`QGraphicsScene` の管理コストが目立ちやすい
- 右ペインの見た目変更と操作変更が別々に進んでしまう

このマイルストーンでは、右ペインを「見えるものは painter が責任を持つ」「scene は互換層に退く」という方針で整理する。

---

## 進捗メモ

- `ArtifactTimelineWidget` の右ペインは `ArtifactTimelineTrackPainterView` を直接ぶら下げる owner-draw 構成に寄せた
- 右ペインの枠描画は `TimelineRightPanelWidget` に集約した
- `ArtifactTimelineTrackPainterView` では playhead を最後に描画し、上端の三角ヘッドも追加済み
- `ArtifactTimelineWidget.cpp` から `QGraphicsView` 前提の補助関数を削除し、scene 由来の足場を減らした
- `ArtifactTimelineTrackPainterView` の clips / markers / selection 更新に no-op ガードを追加した
- `ArtifactTimelineWidget.cpp` の scrub/playhead 経路を一本化して、右ペインの更新要求を減らした
- `refreshTracks()` から毎回の `clearClips()` と重複 `update()` を外して、clip 再構築を差分ベースに寄せた
- `ArtifactTimelineTrackPainterView` の selection sync に入力キャッシュを入れて、同一状態での marker 再計算を避けるようにした
- `ArtifactTimelineTrackPainterView` の hover 更新を 1 回の `update()` にまとめて、マウス移動時の再描画を軽くした
- `ArtifactTimelineWidget.cpp` から track 高さの行ごとの更新を外して、bulk 更新に寄せた
- `ArtifactTimelineWidget.cpp` の viewport sync を helper 化して、ズームと横オフセットの反映を共通化した
- `ArtifactTimelineScene.cppm` と `ArtifactTimelineObjects.cpp` は削除して、右ペインの正規経路を painter 側に固定した

---

## 対象

- `ArtifactTimelineTrackPainterView`
- `TimelineTrackView`
- `TimelineScene`
- `ClipItem`
- 右ペインの playhead / clip / keyframe / hover / selection / drag / resize / scroll sync

---

## 方針

### 原則

1. 右ペインの正規描画経路は painter 側に置く
2. `TimelineTrackView` は互換層として縮退させる
3. `QGraphicsScene` は遷移期間の裏方に限定し、最終的に消せる状態へ持っていく
4. 表示と入力の責務を分けるのではなく、右ペイン内で一体として扱う
5. selection / current layer / playhead の同期を壊さない

---

## 現状資産

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackView.cpp`
- `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md`
- `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`

---

## Phase 1: Responsibility Freeze

### 目的

右ペインの責務境界を固定し、どちらが正規経路かをコード上で説明できるようにする。

### 作業項目

- `ArtifactTimelineTrackPainterView` を右ペインの正規描画経路として明文化する
- `TimelineTrackView` の役割を互換層として明示する
- clip / marker / playhead の state source を painter 側へ寄せる
- 右ペインの更新経路を 1 本に揃える

### 完了条件

- code review で「右ペインの正規責務」が painter 側だと説明できる
- scene 側の状態更新を読まなくても表示が成立する

---

## Phase 2: Painter Parity

### 目的

見た目と基本操作を、既存の scene 実装と同等以上にする。

### 作業項目

- clip の矩形、角丸、トリム表示を painter 側に集約する
- selection / hover / active / disabled の表示を揃える
- playhead の描画を painter 側へ一本化する
- row stripe と keyframe marker の見え方を整理する

### 完了条件

- `QGraphicsItem` を見なくても、見た目が既存実装と一致する
- playhead の二重描画が起きない
- hover / selection で全体再描画が増えすぎない

---

## Phase 3: Input Parity

### 目的

入力と hit test を painter 中心へ寄せ、scene item 依存を減らす。

### 作業項目

- click / drag / resize / seek の入力導線を painter 側で処理する
- hit test を矩形ベースに整理する
- selection / deselection / current layer の同期を見直す
- modifier key の挙動を既存操作に合わせる

### 完了条件

- `ClipItem` なしで日常の編集導線が成立する
- ドラッグ中の遅延や取りこぼしが減る

---

## Phase 4: Scene Reduction

### 目的

`TimelineScene` と `ClipItem` を削除して、右ペインの scene 互換層を退役させる。

### 作業項目

- scene item の再構築を止める
- resize handle / selection state / playhead state を painter 側へ移す
- scene 固有の管理を順次削る
- `TimelineTrackView` の依存先を減らす
- `ArtifactTimelineScene.cppm` / `ArtifactTimelineObjects.cpp` を削除済みであることを前提に整理する

### 完了条件

- scene 由来の右ペイン互換層が消えた
- `QGraphicsScene` の存在理由が右ペインから消えた

---

## Phase 5: Full Cutover

### 目的

右ペインの正規経路を完全 owner-draw に固定し、scene 系を退役可能にする。

### 作業項目

- `TimelineTrackView` から `QGraphicsView` 依存を外す
- `TimelineScene` / `ClipItem` は既に削除済みなので、残る参照の棚卸しをする
- regression を確認する

### 完了条件

- 右ペインの表示更新が painter 側だけで成立する
- 右ペイン実装から `QGraphicsScene` 依存を外せる

---

## Risks

- input と visual の境界を先に切りすぎると、見た目は合っても操作が壊れる
- 互換層を残しすぎると二重管理が長引く
- current layer / playhead / selection の同期を崩すと timeline 全体の信頼感が落ちる

---

## 推奨順序

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## 期待効果

- 右ペインの責務が明確になる
- owner-draw と interaction が同じ surface で進めやすくなる
- `QGraphicsScene` 由来の重さと複雑さを段階的に減らせる
- タイムラインの見た目変更と操作変更を同じ計画で追いやすくなる
