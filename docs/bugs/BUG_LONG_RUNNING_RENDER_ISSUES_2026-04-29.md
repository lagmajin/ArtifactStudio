# 調査報告書: 長期対応が必要な描画/プレビュー不具合 3件

**調査日**: 2026-04-29
**重大度**: 🟥 高
**状態**: 調査中

---

## 1. マスクとレイヤーブレンドで画面が同一カラーで塗りつぶされる

### 症状

マスク処理とレイヤーブレンドを同時に使うと、プレビューが期待通りに合成されず、画面全体または広い範囲が単一の色で塗りつぶされたように見える。

### 影響

- マスク付きレイヤーの見た目確認ができない
- ブレンド結果の検証ができない
- 見た目上は「描画は出ているが合成が壊れている」状態に見える

### 現時点の見立て

この不具合は、単一レイヤーの描画よりも、次のような合成段階で壊れている可能性が高い。

- マスク適用前後のアルファ処理
- ブレンドモードの最終合成
- レンダーターゲットの初期化やクリア色
- レイヤーごとの描画結果を後段で上書きしている経路

### 調査ポイント

- マスク適用後の出力が正しいアルファを持っているか
- ブレンド時にソース/デスティネーション順が崩れていないか
- 最終合成で定数色や不正なクリアが入っていないか
- 画面全体塗りつぶしが起きる分岐に、マスク有効時だけ入っていないか

### 暫定対応方針

1. マスク単体、ブレンド単体、両方同時の3条件で切り分ける
2. レイヤー出力を一段ずつ確認できるログ/可視化を入れる
3. 合成直前の色・アルファ・RT状態を重点的に確認する
4. 根本原因が合成経路なら、描画本体ではなく controller / compositor 側を修正対象にする

---

## 2. 軽いシーンでもプレビューが引っかかる

### 症状

レイヤー数が少ない、見た目も軽いシーンでも、プレビュー再生や操作時に引っかかりや遅延が発生する。

### 影響

- 再生の体感が悪い
- 軽いプロジェクトでも安定感が出ない
- CPU/GPU のどちらで詰まっているのか判断しづらい

### 現時点の見立て

見た目が軽くても、以下のような要因で詰まる可能性がある。

- 初回描画や初回再生時の warmup
- 低コストに見えるが回数が多い処理
- UI スレッドと描画スレッドの競合
- キャッシュ未使用や再計算の多発
- 小さな更新でも全体再描画になっている経路

### 調査ポイント

- 1 フレームあたりの再計算回数
- レンダー要求が毎回フルパスになっていないか
- キャッシュが効いていない箇所がないか
- スレッド待ちやロック待ちが発生していないか
- マスクやブレンドと無関係でも再現するか

### 暫定対応方針

1. 軽いシーンの再現条件を固定する
2. フレームタイム、再描画回数、キャッシュ命中率を取る
3. UI 側の詰まりとレンダー側の詰まりを分ける
4. 必要なら別途 `perf` 系メモへ分離して、長期的な性能調査に回す

---

## 3. プレイヘッドがワークエリア / ルーラー付近でバラつく

### 症状

プレイヘッドがワークエリアやルーラーの近くで、見た目上ずれたり、別々の位置にあるように見える。
同じタイムライン上の要素なのに、表示経路ごとに位置感覚が揃っていない。

### 影響

- どの位置が現在フレームなのか直感的に分かりにくい
- ワークエリア、ルーラー、トラック本体の見え方が一致しない
- ドラッグやスクラブの信頼感が落ちる

### 現時点の見立て

これは単純に「ウィジェットが一体化されていない」だけではなく、次の複合要因がありそう。

- ルーラー、ワークエリア、トラック、スクラブバーで別々に `frame -> x` 変換している
- 各ウィジェットが少しずつ違う offset / zoom / padding を持っている
- playhead state の source が複数あり、同期が完全ではない
- 描画と入力で参照している座標系が一致していない

つまり、問題の本体は「UI が分かれていること」よりも、
**同じ playhead を指すべき surface が、それぞれ別の座標系と更新タイミングで描いている**点にある可能性が高い。

### 調査ポイント

- `TimelineTrackView`、`ArtifactTimelineScrubBar`、`ArtifactTimelineNavigatorWidget`、`WorkAreaControl` の frame 変換式を比較する
- playhead を更新している箇所が 1 つかどうか確認する
- `rulerPixelsPerFrame` や offset が各 widget で一致しているか確認する
- ルーラー表示時だけ別スケールになっていないか確認する
- work area と playhead が同じ `totalFrames` と正規化方式を使っているか確認する

### 暫定対応方針

1. playhead の基準座標系を 1 つに定義する
2. 各 widget はその基準から派生表示するだけに寄せる
3. 変換式を共通 helper に集約する
4. まずは見た目のバラつきを解消し、その後に入力導線を揃える

---

## 優先度メモ

- 3件とも短期修正だけでは終わらない可能性がある
- まずは再現条件の固定と、合成経路 / キャッシュ経路の切り分けが必要
- playhead のずれは座標系統一の問題として別枠で追うのがよい
- 1件が解決しても、他の不具合が独立して残る可能性が高い

---

## 修正メモ（2026-04-29）

### 変更したこと

1. マスク / マット / rasterizer effect を surface cache key に含めた
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
   - 目的: mask geometry や matte 設定が変わったあとに古い layer surface / GPU texture を再利用しないようにする
   - 補足: track matte は参照元レイヤー側の変化に依存するため、matte 使用時は layer surface cache を使わないようにした

2. 再生中の frameChanged から即時 `renderOneFrame()` しないようにした
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
   - 目的: 軽いシーンでも frameChanged ごとに同期描画が走る経路を減らし、既存の fixed-rate render tick へ寄せる
   - 変更: `renderOneFrame()` 直呼びを `markRenderDirty()` に変更

3. work area の playhead 表示を track / scrubbar と同じ ruler 座標へ寄せた
   - 対象:
     - `Artifact/include/Widgets/Timeline/ArtifactWorkAreaControlWidget.ixx`
     - `Artifact/src/Widgets/Timeline/ArtifactWorkAreaControlWidget.cpp`
     - `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
   - 目的: work area だけ `frame / totalFrames` の正規化表示になっていたため、zoom / horizontal offset がある状態で track 本体や scrubbar とずれる問題を減らす
   - 変更: `WorkAreaControl` に `setRulerPixelsPerFrame()` / `setRulerHorizontalOffset()` を追加し、playhead 描画だけ ruler 座標を使うようにした

### 未確認

- ビルド / 実行テストは未実施
- GPU blend path 自体の shader / RTV state はまだ未調査
- preview stutter は frameChanged 経路を軽くしただけなので、他の `renderOneFrame()` 直呼び経路が残っている

### 次に見るべきこと

1. マスク + 非 Normal blend で単色塗りつぶしが再現するか
2. track matte 使用時に cache 無効化が期待通り効くか
3. 再生中の frame time が `markRenderDirty()` 化で安定するか
4. work area / scrubbar / track playhead が zoom / pan 後も同じ x に揃うか

5. 連続操作中の `renderOneFrame()` 直呼びを一部 `markRenderDirty()` に寄せた
   - 目的: プレビュー更新を tick 側へ集約して、軽いシーンでも詰まりにくくする
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

6. マスク / matte の適用で premultiplied alpha と RGB を同時に減衰させる
   - 目的: mask + blend 時に RGB だけ残って単色塗りつぶしっぽく見える経路を防ぐ
   - 対象: `Artifact/src/Mask/LayerMask.cppm`
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
   - 対象: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`

7. GPU の 2D sprite / glyph マスクでも alpha と RGB を同時に落とす
   - 目的: 同種の premultiplied 不整合を他の描画経路でも起こさないようにする
   - 対象: `Artifact/src/Render/ShaderManager.cppm`

8. 連続操作後の更新は `renderOneFrame()` 直呼びより `markRenderDirty()` を優先する
   - 目的: 反復入力や UI 切り替えのたびに同期描画を積まないようにして、軽いシーンでも引っかかりを減らす
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

9. レガシー layer editor も確定操作は `requestRender()` に寄せる
   - 目的: `ArtifactRenderLayerWidgetv2` の状態変更後に同期描画を積まず、レンダーループ側へまとめる
   - 対象: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

10. `CompositionRenderWidget` のアイドル待ちを `requestRender()` 通知で起こす
   - 目的: 固定 sleep で次フレームを待たず、操作後の描画遅延を減らす
   - 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
