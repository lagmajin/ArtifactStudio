# Timeline TrackView Owner-Draw Migration (2026-03-27)

`ArtifactTimelineWidget` の右ペインは、現状 `TimelineTrackView (QGraphicsView + TimelineScene + ClipItem)` と `ArtifactTimelineTrackPainterView (QWidget + QPainter)` が並走している。
性能仮説では、`QGraphicsView` / `QGraphicsScene` / `QGraphicsItem` のオーバーヘッドが大きく、クリップ数やドラッグ頻度が増えるほど UI 応答が落ちやすい。

このマイルストーンは、右ペインを **完全 owner-draw 化へ段階移行** するための計画である。

## Goal

- 右ペインの描画責務を `QGraphicsScene` から `QPainter` ベースへ移す
- クリップ数増加時でもスクラブ / ドラッグ / リサイズの体感を維持する
- 既存の操作感を壊さずに、後方互換を保ったまま移行する
- 将来的に `TimelineScene` / `ClipItem` を縮退または撤去できる状態にする

## Current State

- `TimelineTrackView` はまだ `QGraphicsView` ベース
- `TimelineScene` がクリップ item とハンドル item を保持している
- `ArtifactTimelineTrackPainterView` はすでに別系統の owner-draw 実装として存在する
- 左ペインは `ArtifactLayerPanelWidget` で既に owner-draw 寄り
- 2026-03-27 時点で、`ArtifactTimelineTrackPainterView` に current frame marker を追加し、右ペインの正規描画候補としての可視性を上げた
- 2026-03-27 時点で、`ArtifactTimelineTrackPainterView` から `clipSelected` / `clipDeselected` を出し、選択起点も painter 側へ寄せ始めた
- 2026-03-27 時点で、`ArtifactLayerSelectionManager::selectionChanged` を使って painter 側の selected 表示を追従させ始めた
- 2026-03-27 時点で、`ArtifactTimelineTrackPainterView` の HUD に選択数とホバー中クリップ名を出し、状態把握を少し強めた
- 2026-03-27 時点で、hover / selection / drag の再描画を局所更新に寄せ、右ペインの repaint 粒度を下げ始めた
- 2026-03-27 時点で、selected layer の keyframe marker を painter 側に重ね描きし、右ペインを keyframe visibility の入口としても使い始めた
- 2026-03-27 時点で、keyframe marker を左クリックするとその frame へ seek できるようにし、右ペインを編集導線の入口へ近づけた

## Definition Of Done

- 右ペインの主要描画が `ArtifactTimelineTrackPainterView` に集約される
- clip の表示、選択、ドラッグ、リサイズ、seek が painter 側で完結する
- `QGraphicsScene` 依存の item 管理が不要になる
- 既存の shortcut / sync / playhead 表示が変わらない
- 大量レイヤーでもスクラブ中の repaint が安定する

## Phases

### Phase 1: Responsibility Split

`TimelineTrackView` と `ArtifactTimelineTrackPainterView` の役割を明示して、移行中の二重実装を整理する。

対象:

- `ArtifactTimelineWidget.cpp`
- `ArtifactTimelineWidget.ixx`
- `ArtifactTimelineTrackPainterView.cpp`
- `ArtifactTimelineScene.cppm`

内容:

- `TimelineTrackView` を「互換レイヤー」か「旧実装」として明示する
- `ArtifactTimelineTrackPainterView` に必要な状態を揃える
- clip / row / playhead / scroll の同期経路を 1 箇所に寄せる
- painter 側の clip selected state が layer selection と同期する

完了条件:

- どちらが正規描画経路かをコード上で説明できる
- 両経路の state mismatch が減る

### Phase 2: Painter Parity

`ArtifactTimelineTrackPainterView` を現在の `QGraphicsView` と同等の見た目に寄せる。

内容:

- クリップ矩形
- 選択 / hover / drag 表示
- row stripe
- playhead 可視化
- zoom / horizontal offset / vertical sync

完了条件:

- 画面上で見た目差がほぼなくなる
- 主要な表示が painter 側だけでも成立する
- hover / selection の切り替えで全体再描画を避けられる

### Phase 3: Input Parity

移動・リサイズ・seek・ホイール・キーボードを painter 側へ揃える。

内容:

- hit test を矩形ベースで実装
- move / resize drag を painter 側で処理
- timeline scrub と clip edit の入力分離
- `Ctrl` / `Shift` / `Alt` の修飾挙動を維持

完了条件:

- `ClipItem` に依存しない操作が成立する
- ドラッグ中のフリーズが減る

### Phase 4: Scene Reduction

`TimelineScene` / `ClipItem` を縮小し、残す理由がある部分だけに限定する。

内容:

- item ベースの選択管理を削る
- resize handle item を painter 側へ移す
- scene index 依存を減らす

完了条件:

- `QGraphicsItem` 数が実運用上ほぼ不要になる
- scene は互換層または補助層に退く

### Phase 5: Full Cutover

右ペインの正規経路を owner-draw に固定する。

内容:

- `TimelineTrackView` から `QGraphicsView` 依存を外す
- `TimelineScene` を削除または別用途へ分離する
- テストと回帰確認を実施する

完了条件:

- 右ペインの描画が完全に owner-draw 化される
- 既存の同期と操作感が保たれる

## Risks

- 既存の clip item ベースの drag / resize と入力差分が出やすい
- selection / focus / scroll sync の責務境界が曖昧だと崩れる
- 先に scene を消すと検証コストが跳ねる

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Notes

- 右ペインは `ArtifactTimelineTrackPainterView` がすでにあるため、ゼロから作り直す必要はない
- まずは painter 側を正規経路に近づけるのが安全
- `TimelineTrackView` を一気に消すのではなく、互換層として残しながら段階的に切り替える
