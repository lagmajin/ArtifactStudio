# Timeline Visual Language (2026-03-31)

**最終更新:** 2026-08-15

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
- `ArtifactTimelineWidget::layerTimelineColor()` で Video／Audio／Text／Shape／Image／Particle／3D／Camera などの種別色が定義され、clipへ渡されている
- playhead は right panel の overlay同期経路へ整理され、旧scene由来の二重描画は現行コードでは確認できない
- キーフレームは visible rows ベースで収集し始めており、色分けとレーン分割の入口がある

## Update 2026-08-15

現行コードでは、種別色、選択／hoverの派生色、keyframeのinterpolation／easing／color label、レーン分割、playhead overlay、左パネルのactive borderが主要経路まで実装済み。残るのは共通token/helperへの整理、色覚差・light/dark themeを含む実機回帰、状態形状の横断統一である。

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

---

## 2026-08-15 現状確認

実装は Phase 1〜4 相当まで進んでいる。`ArtifactTimelineWidget.cppm` に Video / Audio / Text / Shape / Image / Solid / Camera / Light / Particle 等の layer type 色があり、選択状態は layer panel と右側 timeline の両方で accent／左ボーダーとして表現される。`ArtifactTimelineTrackPainterView.cppm` では marker の interpolation、easing、color label、lane、selected／hover／current 状態を分け、playhead は `TimelinePlayheadDraw` 共通 helper を使って描画している。Scrubbar も同じ theme token を参照する。

未完了・未確認:

- layer color の定義を共通 token／enum に一元化すること
- 通常／選択／easing／disabled の marker 形状ルールを専用 state helper に集約すること
- overlay、scrubbar、track painter、playhead の全設定組み合わせで二重描画がないことの実機確認
- 色覚差へのコントラスト検証と light/dark theme 回帰

したがって「視覚表現の主要な実装と同期は済み、共通化と品質検証が残る」と整理する。

---

## Next Execution Slice

Phase 1 は、色の意味を layer type と状態の 2 軸に分けて固定する。

### Phase 1A の着手点

1. `Video / Audio / Text / Effect` の既定色を layer type ごとに固定する
2. `selection / hover / disabled` の派生色を既定色から作る
3. keyframe の通常 / 選択 / easing / disabled の状態色を先に決める
4. 色が増えすぎないよう、同系色の派生だけで運ぶ

### Phase 1 完了条件

- layer type ごとの色ルールがコード上で明確に実装される
- 選択、ホバー、非選択が見た目で判別できる
- keyframe の状態色が固定される

### Phase 2A の着手点

1. keyframe の形状を通常 / 選択 / easing / disabled で分ける
2. 同一フレームに複数ある場合のレーン分割を先に決める
3. marker hit area を見た目と揃える
4. label 表示は color semantics が安定してから重ねる

### Phase 2 完了条件

- キーフレームが状態別に見分けやすくなる
- 重なりが読める
- hit area と見た目が乖離しない

### Phase 3 への前提

- playhead は marker semantics が固まってから一本化する
- selection highlight は layer color と衝突しない範囲で入れる
