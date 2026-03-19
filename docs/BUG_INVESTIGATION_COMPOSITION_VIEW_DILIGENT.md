# 不具合調査記録: Composition View (Diligent) が描画されない

## 対象

`Composition Viewer` のうち、`Diligent` バックエンド側の表示問題のみを扱う。

このメモでは、`Layer View` の表示不具合や software renderer の不具合は切り分け対象外とする。

---

## 現象

- Composition View を開いても、背景色やレイヤーが期待通りに描画されない。
- `Layer View (Diligent)` は別経路として動作しているため、Composition View 側だけが失敗しているように見える。
- software 側の Composition Renderer でも似た症状が出ているため、単純な backend 固有不具合ではなく、上位のデータ受け渡しや composition space 適用の問題が疑わしい。

---

## 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Render/CompositionRenderer.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/ShaderManager.cppm`

---

## 現時点の構造

### 1. 上位ラッパー

`CompositionRenderer` は `ArtifactIRenderer` の上に乗るラッパーとして追加されている。

責務は以下の想定:

- composition size の設定
- composition space の適用
- 背景色の描画
- composition 内の矩形やレイヤー描画

### 2. Diligent 側の描画経路

`ArtifactRenderLayerWidgetv2` が Diligent の device/context を持ち、`CompositionRenderer` を通じて描画している。

### 3. 依存する入力

Composition View が正しく描画されるには、少なくとも次の入力が必要:

- current composition の取得
- composition size
- background color
- 現在フレーム
- レイヤーの有効状態と座標

---

## 重点仮説

### 仮説 1: 背景色が CompositionRenderer に渡っていない

背景描画は composition が空に見える場合でも最初に確認すべき箇所。

確認ポイント:

- composition settings の background color を取得しているか
- `DrawCompositionBackground()` が実際に呼ばれているか
- 画面クリアと背景描画の順序が逆転していないか

### 仮説 2: composition space の適用が抜けている

Composition View では local layer space ではなく composition space を使う必要がある。

確認ポイント:

- `SetCompositionSize()` が呼ばれているか
- `ApplyCompositionSpace()` が描画前に呼ばれているか
- viewport / canvas transform が期待通り更新されているか

### 仮説 3: データ受け渡しが片側で止まっている

Composition View の UI 側は更新されていても、render controller へ値が渡っていない可能性がある。

確認ポイント:

- `currentCompositionChanged` のタイミング
- composition size / background color / layer list の再取得
- `renderOneFrame()` の呼び出し時点で最新 composition を参照しているか

### 仮説 4: Diligent 側の primitive に変換された後の座標が破綻している

見た目が完全に出ない場合、矩形は生成されていても viewport 外へ飛んでいる可能性がある。

確認ポイント:

- `PrimitiveRenderer2D` の local-to-viewport 変換
- `DrawCompositionRect()` の引数が composition space 基準になっているか
- background rect と layer rect の座標基準が揃っているか

### 仮説 5: Composition View の初期化順が不足している

widget / renderer / swap chain / composition の順序が崩れると、最初のフレームだけ空描画になる。

確認ポイント:

- renderer の初期化前に draw していないか
- swap chain / viewport resize 後に再初期化しているか
- dock 表示時の再生成で state が失われていないか

---

## これまでの観測からの判断

- Layer View 側の `visible` / `opacity` は別件として改善済み。
- Composition View の問題は、レイヤーごとの属性ではなく、`CompositionRenderer` の上流で止まっている可能性が高い。
- software renderer でも同様の症状があるため、Diligent 固有の PSO 問題より先に、composition データの渡し方と composition space の適用を疑うべき。

---

## コード上で確認できた強い原因候補

### 原因候補 1: コンポジション初期背景色が生成時に引き継がれていない

`ArtifactCompositionInitParams` は `backgroundColor_` を持っているが、`ArtifactAbstractComposition` のコンストラクタでその値を `impl_->backgroundColor_` に反映していない。

該当箇所:

- `Artifact/src/Composition/ArtifactCompositionInitParams.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`

確認ポイント:

- `ArtifactCompositionInitParams` 側には `backgroundColor_` と `setBackgroundColor()` が存在する
- `ArtifactAbstractComposition::ArtifactAbstractComposition(...)` では `compositionName` と `compositionSize` はコピーしているが、`backgroundColor` はコピーしていない
- そのため新規コンポジションは `ArtifactAbstractComposition` 側の既定値 `{0.1f, 0.1f, 0.1f, 1.0f}` のままになる

これは Diligent Composition View だけでなく、software composition path でも同じ背景色不一致を起こし得る。

### 原因候補 2: Composition View でレイヤー transform が適用されていない

`CompositionRenderController::renderOneFrame()` は各レイヤーに対して `layer->draw(renderer)` を直接呼ぶだけで、`getGlobalTransform()` を使っていない。

一方で各レイヤーの `draw()` 実装はほぼすべて local 原点 `(0,0)` 前提で描画している。

該当箇所:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/ArtifactSolid2DLayer.cppm`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`

確認ポイント:

- `renderOneFrame()` は `layer->draw(impl_->renderer_.get())` を呼ぶ
- `ArtifactSolid2DLayer::draw()` は `drawSolidRect(0, 0, width, height, ...)`
- `ArtifactImageLayer::draw()` は `drawSprite(0, 0, width, height, ...)`
- `ArtifactTextLayer::draw()` と `ArtifactVideoLayer::draw()` も同様
- `ArtifactAbstractLayer` には `getLocalTransform()` / `getGlobalTransform()` があるが、Composition View 描画経路では使われていない

これは Layer View なら許容できても、Composition View では成立しない。

### 原因候補 3: `sourceSize_` が未初期化のまま使われる可能性がある

`ArtifactAbstractLayer::Impl` の `sourceSize_` はメンバーとして存在するが、`Impl::Impl()` で初期化されていない。

該当箇所:

- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

確認ポイント:

- `Size_2D sourceSize_;` の宣言はある
- `ArtifactAbstractLayer::Impl::Impl()` は空
- `sourceSize()` はその値をそのまま返す

`sourceSize` を確実に設定するレイヤーでは問題化しにくいが、未設定のレイヤーが混ざると極端なサイズや画面外描画を起こし得る。以前の平面レイヤー不具合と同系統のリスク。

### 原因候補 4: Composition View の描画ロジックが二重化しており、修正が片側にしか効かない

`CompositionRenderController` は `ArtifactPreviewCompositionPipeline` を保持しているが、実際の `renderOneFrame()` では pipeline の `render()` を使わず、自前で背景・レイヤー・ガイド描画を再実装している。

該当箇所:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`

確認ポイント:

- controller は `previewPipeline_.setComposition()` を使う
- しかし実描画は `previewPipeline_.render(renderer)` ではなく controller 側の hand-written path
- そのため pipeline 側の修正が Composition View に反映されない
- software / Diligent / Layer View / Composition View の差分が増えやすい

これは直接の描画失敗原因というより、問題を長引かせている構造要因。

---

## 調査手順

1. `ArtifactCompositionRenderController` が描画時に持っている composition を確認する。
2. `CompositionRenderer::SetCompositionSize()` と `ApplyCompositionSpace()` の呼び出し有無を確認する。
3. `DrawCompositionBackground()` が背景色付きで実行されているかログを入れる。
4. `DrawCompositionRect()` に渡る座標が composition space になっているか確認する。
5. Diligent 側で primitive が生成されているのに見えない場合は、viewport / transform / clear の順序を確認する。

---

## 期待する修正方針

- Composition View の背景色表示を先に安定させる。
- まず `ArtifactAbstractComposition` 生成時に `params.backgroundColor()` を反映する。
- Composition View で layer transform を適用する経路を決める。
- composition space の変換を Diligent / software で揃える。
- その後にレイヤー描画を同じ経路へ載せる。

---

## 補足

この問題は `Layer View (Diligent)` とは別扱い。
`Composition View` のみを直す場合は、まず `CompositionRenderer` と `ArtifactCompositionRenderController` の接続点を優先して見るのが妥当。

---

## 更新記録 (2026-03-20)

### 実施済み修正

1. Composition 初期背景色の引き継ぎ修正
- `ArtifactAbstractComposition` 生成時に `params.backgroundColor()` を反映するよう修正。
- 対象: `Artifact/src/Composition/ArtifactAbstractComposition.cppm`

2. レイヤー `sourceSize_` 未初期化の修正
- `ArtifactAbstractLayer::Impl` で `sourceSize_ = Size_2D(1920, 1080)` を初期化。
- 対象: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

3. Composition View で transform-aware 描画に変更
- `CompositionRenderController` に `drawLayerForCompositionView()` を追加し、`getGlobalTransform().mapRect()` の world rect で描画。
- `Solid2D` / `SolidImage` / `Image` / `Video` は controller 側で直接描画、未対応型は `layer->draw()` にフォールバック。
- 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

4. Composition View 初期化順を見直し
- `showEvent` で `initialize -> recreateSwapChain -> setViewportSize -> setComposition -> zoomFit -> start` の順に調整。
- 対象: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

5. same-composition 再セット時の再適用漏れを修正
- `setComposition()` の early-return 経路でも composition size / space を再適用するよう変更。
- Composition の `changed` も購読し、背景色やサイズ変更時に再描画。
- 対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

6. Text レイヤーを Composition View の direct path に追加
- `ArtifactTextLayer` に `toQImage()` を追加し、`drawLayerForCompositionView()` から world rect ベースで描画するよう修正。
- これにより Text レイヤーが `layer->draw()` フォールバック経由で local 原点描画になる経路を 1 つ削減。
- あわせて fallback に落ちたレイヤー型をログへ出すようにした。
- 対象:
  - `Artifact/include/Layer/ArtifactTextLayer.ixx`
  - `Artifact/src/Layer/ArtifactTextLayer.cppm`
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

7. `ArtifactRenderLayerWidgetv2.cppm` の構文崩れを復旧
- `renderOneFrame()` 内の中括弧不足により `recreateSwapChain()` 以降が巻き込まれていたため、`}` を 1 つ補って構文エラー連鎖を解消。
- これは Layer View 側の修正だが、Composition 周辺の切り分けを継続するための前提復旧として実施。
- 対象:
  - `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

8. geometry / draw path ログを追加
- `drawLayerForCompositionView()` に `layer geometry` / `draw solid2d` / `draw solid-image` / `draw image` / `draw video` / `draw text` を追加。
- `sourceSize`、`worldRect`、viewport 変換後矩形、サーフェスサイズ、solid color を追えるようにした。
- 対象:
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

9. Viewer のコンポ追従元を `ActiveContext` / `Playback` 優先に変更
- `CompositionEditor` と `CompositionRenderController` で、参照元を `ProjectService` 固定から `ActiveContext -> PlaybackService -> ProjectService` に変更。
- `Timeline` 側が更新している active/playback composition と Viewer の参照先を揃える目的。
- 対象:
  - `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### ログ運用の変更

ノイズ削減のため、以下のログを停止済み:
- `PrimitiveRenderer2D` の `[VIEWPORT]`, `[SOLIDRECT]`, `[SPRITE]`
- LayerView 側の `[LayerView]`

Composition 調査専用として、以下を追加:
- `[CompositionView] setComposition`
- `[CompositionView] renderOneFrame ...`
- 背景色/サイズ
- レイヤー総数
- レイヤーごとの `draw` / `skip` 理由

対象:
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### ここまでの考察 (最新版)

1. 「シェーダー変数名の不一致」は本命ではない
- `TransformCB` / `ColorBuffer` / `g_texture` / `g_sampler` の接続自体は整合している。

2. 本命は Composition View の state 問題
- 初期化順、swapchain/viewport 同期、composition 再適用漏れで「描いているのに見えない」状態が起き得る。

3. まだ残る可能性
- `drawLayerForCompositionView()` で未対応レイヤー型が `layer->draw()` フォールバックになり、local 原点描画で崩れるケース。
- `currentComposition` は取れていても、レイヤー入力（画像 null、frame 非active、sourceSize 異常）で実質何も描けていないケース。

4. 最新ログでは「空のコンポジションを見ている」症状が出ている
- 追加ログ適用後、`[CompositionView] frame begin ...` に対して `layers total=0` が継続。
- この時点では描画 API 手前で、Viewer が空の composition instance を掴んでいるか、UI 側の active composition 選択が別コンポへずれている可能性が高い。
- したがって、焦点は「描いているのに見えない」から「どの composition を current として追っているか」へ移った。

### 次の切り分け観点 (Composition ログベース)

1. `setComposition` が `null` でないか
2. `frame begin` の `size` と `bg` が期待値か
3. `layers total` が 0 でないか
4. 全レイヤーが `skip` になっていないか
5. `draw layer` が出ているのに見えない場合、該当レイヤー型がフォールバック経路かどうか
6. `layers total=0` の場合、`ActiveContext` / `PlaybackService` / `ProjectService` のどの composition ID を Viewer が採用したか
7. Timeline / Project View / Viewer の composition ID が同一か
