# 不具合調査記録: タイムライン keyframe marker 表示の階層化欠如

## 対象
タイムライン上での keyframe marker 表示において、プロパティごとの subdivision（sub-lane）が固定化されておらず、複数プロパティの keyframe が同一レイヤー内で識別しづらい状態。

## 現象
- レイヤー行内で keyframe が複数表示されるとき、**Position X, Position Y, Opacity, Effect パラメータ** などの区別がつきにくい
- keyframe が同じフレームに複数ある場合、それらがランダムな順序で sub-lane に割り当てられ、プロパティ固有の位置が一貫しない
-  inspector 側の property tree には細分化されたプロパティ表示があるが、timeline 側の keyframe marker では「Transform」のような大きなグループしか表現できていない（大まかすぎる）

## 仮説と根本原因

現在の `ArtifactTimelineWidget::collectKeyframeMarkers()` では、keyframe marker の `laneIndex` を以下のように決定している：

1. レイヤー内の全アニメータブルプロパティを列挙
2. **各 keyframe を its appearance order で処理**し、同一フレーム上で `laneCursor` をインクリメントしながら `laneIndex` を割り当て
3. 結果として、**同じプロパティの keyframe でも異なるフレームで異なる sub-lane に分散**し、逆に **プロパティ固有の sub-lane 位置が固定されていない**

これはユーザーが keyframe の「縦位置」でプロパティを識別できるようにする設計意図に反する。Timeline 上では、各レイヤー行を property ごとの固定 sub-lane に分割し、keyframe はその property の sub-lane に一意に割り当てられるべきである。

## 該当コード

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
  - 関数 `collectKeyframeMarkers()` （行 262 付近）
  - 構造体 `DraftMarker` と `laneCounts`/`laneCursor` ロジック（行 310-370）

## 修正方針

`collectKeyframeMarkers()` を property-first アプローチに変更する：

1. レイヤーの全アニメータブルプロパティを列挙し、**各プロパティに固定の `laneIndex` を割り当てる**（0 から連続）
2. `laneCount` を総プロパティ数に設定
3. 各 keyframe は所属プロパティの `laneIndex` を使用して marker を生成
4. ソートは `(frame, laneIndex)` 順とし、描画順を決定

これにより：
- 同じプロパティの keyframe は常に同じ sub-lane 上に表示される
- 各 sub-lane はプロパティ名（またはラベル）と一対一対応となり、 inspector との視覚的整合性が取れる
- 同一フレームに複数プロパティの keyframe があれば、それぞれの sub-lane に分かれて表示され、視認性が向上する

## 実装タスク

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` 内 `collectKeyframeMarkers()` を修正
  - `propertyLaneMap`（property と laneIndex のマップ）を構築
  - `totalLanes` をプロパティ総数とする
  - 各 keyframe から `DraftMarker` を生成する際、プロパティの固定 `laneIndex` を注入
  - ソート後、最終 marker に `laneCount = totalLanes` と `laneIndex = propertyLaneIndex` を設定

## 影響範囲

- 描画側 `ArtifactTimelineTrackPainterView` は `laneIndex`/`laneCount` を既に使用しているため、**変更なし**
- keyframe marker の見た目（位置、色、サイズ）は変わらないが、**縦方向の配置がプロパティ固有の位置に固定化**される
- ラベル表示（ホバー時等）は現在のまま property name を出しているので、protery 特定はより容易になる

## 調査日
2026-04-02
