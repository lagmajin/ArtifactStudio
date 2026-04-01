# Timeline Visual Language (2026-03-31)

タイムラインの視覚表現を、単なる装飾ではなく意味ベースの UI 言語として整理する。

このマイルストーンは、レイヤーバー、キーフレーム、再生ヘッド、選択ハイライトを一貫したルールで描き分け、操作対象と状態を瞬時に判別しやすくすることを目的とする。

## Goal

- レイヤー種別ごとの色分けを統一する
- 選択 / ホバー / 非選択の視認性を揃える
- キーフレームの状態を色と形で明確に区別する
- 再生ヘッドを他要素と混ざらない独立した記号として扱う
- タイムライン左ラベルと右バーの対応関係を視覚的に分かりやすくする

## Visual Rules

### Layer Bars

- `Video` は青系
- `Audio` は緑系
- `Text` は紫系
- `Effect` は橙系
- 選択時は枠線を強め、同系色の薄い塗りとグローで強調する

### Keyframe Diamonds

- 通常は黄色系で視認性を優先する
- 選択中は白系 + リングで明確に区別する
- イージング付きはシアン系で「なめらかさ」を示す
- 同一フレームに複数ある場合はレーン分割で重なりを避ける

### Playhead

- 赤系で統一する
- 縦線と数値バッジの意味を固定する
- 他の UI 色と混ざらないようにする

### Selection Highlight

- 青系の半透明を基本とする
- 右バーだけでなく、左のレイヤーラベル側にも左ボーダーを入れて対応関係を示す
- current selection と hover を混同しない

## Current State

- `ArtifactTimelineTrackPainterView` でクリップとキーフレームの owner-draw が始まっている
- クリップはまだ単一の既定色が強く、種別別の整理は未完
- playhead は painter 側と overlay 側の二重描画が起きていたため、片側へ寄せる途中である
- キーフレームは visible rows ベースで収集し始めており、色分けとレーン分割の入口がある

## Definition Of Done

- レイヤー種別ごとの色ルールがコード上で明確に実装される
- 選択、ホバー、非選択が見た目で確実に判別できる
- キーフレームが状態別に見分けやすくなる
- playhead が二重表示されない
- 左ラベルと右クリップの対応が視覚的に通じる

## Phases

### Phase 1: Color Semantics

タイムラインの色を意味ベースに整理する。

内容:

- layer type ごとの既定色を定義する
- selection / hover / disabled の派生色を整理する
- keyframe の状態色を固定する

### Phase 2: Marker Semantics

キーフレームの形状と色を意味ごとに分ける。

内容:

- 通常 / 選択 / easing / disabled を区別する
- marker のレーン分割とラベル表示を整える
- marker hit area を見た目と揃える

### Phase 3: Playhead Consolidation

再生ヘッドの描画責務を一本化する。

内容:

- overlay と painter の二重描画をなくす
- line / head / number badge の役割を統一する

### Phase 4: Left/Right Affordance

左ラベルと右バーの対応を視覚的に強める。

内容:

- 左ラベルに active border を入れる
- 選択レイヤーと右バーの見た目を同期する
- hover 時の対応強調を入れる

## Risks

- 色を増やしすぎると逆に意味が伝わりにくくなる
- 選択とホバーの表現が近すぎると操作感が鈍る
- 左右両ペインの同期が遅いと、視覚ルールだけ整っても体感が悪い

## Recommended Order

1. Layer Bars
2. Keyframe Diamonds
3. Playhead
4. Selection Highlight
5. Left/Right Affordance
