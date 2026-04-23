# Timeline QGraphicsScene Elimination (2026-03-31)

`ArtifactTimelineWidget` の右ペインは、`ArtifactTimelineTrackPainterView` が正規経路になり、`TimelineScene` / `ClipItem` の互換層は退役済みになった。

このマイルストーンは、右タイムラインから `QGraphicsScene` 依存を外し切るための計画として残しているが、右ペインについては主要目標を達成した。

## Goal

- 右タイムラインの表示更新を `QGraphicsScene` ではなく `QPainter` に寄せる
- クリップ、キーフレーム、playhead、スクロールの主要描画を painter 側に集約する
- `TimelineTrackView` を互換層へ縮退し、最終的に削除可能な状態へ持っていく
- 既存のショートカットや編集導線を壊さずに移行する

## Current State

- `ArtifactTimelineTrackPainterView` は clip / marker / playhead の描画を担当している
- `ArtifactTimelineWidget::refreshTracks()` は painter 側へ可視レイヤーを流し込み始めている
- `TimelineScene` / `ClipItem` は削除済み
- `QGraphicsScene` の右ペイン依存はなくなった

## Definition Of Done

- `ArtifactTimelineWidget` の通常描画・選択・キーフレーム・playhead 更新が painter 側のみで成立する
- `refreshTracks()` から `TimelineScene` へのデータ投入がなくなる
- `TimelineTrackView` が状態保持の薄い互換層になり、`QGraphicsItem` 依存が不要になる
- 右ペインの表示に `QGraphicsScene` を参照しなくなる

## Phases

### Phase 1: Scene Feed Cut

`refreshTracks()` とその周辺から `TimelineScene` への再投入を止める。

対象:

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

内容:

- `refreshTracks()` の clip / row 再構築を painter 側だけに寄せる
- painter 側の `TrackClipVisual` / `KeyframeMarkerVisual` を正規データとして扱う
- `TimelineTrackView` は既存 API を残しつつ表示更新の主役から外す

完了条件:

- 右ペイン更新時に `QGraphicsScene` へ clip item を積まない
- painter 側だけで clip / marker / playhead が描ける

### Phase 2: Input Bridge Reduction

入力イベントを painter 側へ寄せ、scene item の hit test 依存を減らす。

内容:

- click / drag / resize / seek の主要導線を painter 側に統一する
- selection / deselection の同期を painter 中心にする
- 旧 `TimelineTrackView` の mouse handling を縮退する

完了条件:

- `ClipItem` を参照しなくても編集導線が成立する
- 右ペインの操作感が painter 側で完結する

### Phase 3: Compatibility Shrink

`TimelineTrackView` から scene 固有の管理を段階的に剥がす。

内容:

- `TimelineScene` の item 管理を削る
- `ClipItem` の残機能を必要最小限にする
- `QGraphicsScene` の update path を止める

完了条件:

- scene は互換 API のみに退く
- `QGraphicsItem` の存在理由がほぼなくなる

### Phase 4: Full Cutover

右ペインの正規経路を owner-draw に固定する。

内容:

- `TimelineTrackView` から `QGraphicsView` 依存を外す
- `TimelineScene` / `ClipItem` を削除または別用途へ分離する
- 回帰確認を行う

完了条件:

- `QGraphicsScene` を右タイムライン実装から事実上排除できる
- painter 側だけで日常編集が成立する

## Risks

- 既存の clip item ベース操作を一気に切ると、選択やショートカットが壊れやすい
- scene を削る順番を誤ると、見た目より先に入力系が破綻する
- 互換層の残し方が中途半端だと、二重管理のまま長引く

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

## Notes

- 右ペインはすでに `ArtifactTimelineTrackPainterView` が主役
- `TimelineScene` / `ClipItem` は削除済み
- `TimelineTrackView` はまだ残るが、別用途の棚卸し対象として扱う
