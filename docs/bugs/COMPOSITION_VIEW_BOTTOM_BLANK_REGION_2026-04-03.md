# コンポジションビュー下部が描画されない領域になる問題 (2026-04-03)

## 症状

Composition Viewer の下側およそ 200px が、

- クリアはされる
- しかしレイヤーや gizmo が描画されない

という状態になる。

画面上では「下端だけ空いている」ように見える。

## 既知の観測

- 画面全体の present 自体は生きている
- 背景の clear は実行されている
- レイヤーは上側では見える
- 2D gizmo も上側では見える
- 問題は「下側だけ」が抜けること

## 直近の確認ログ

### CompositionView の状態

- `compSize = 1920x1080`
- `hostSize = 1200x800`
- `pipelineEnabled = false`
- `layersTotal = 0` のときでも下側が空く
- `background` は正しい RGBA が出ている

### 描画経路

- background は `drawSprite(..., QImage)` 経由で描けている
- fallback path では `viewportW / viewportH` を使って viewport を明示している
- `setViewportSize()` は `hostWidth_ / hostHeight_` を元に更新している

## 仮説

### 1. viewport 高さと実際の描画領域が不一致

`hostHeight_` は更新されているが、実際の present 領域か `SetViewports()` の値が下端に対して小さい可能性がある。

### 2. composition space と viewport space の切替で下側だけ切れている

背景描画、オフスクリーン描画、最終 present のどこかで、
`viewportH` と `hostHeight_` の使い分けがずれている可能性がある。

### 3. ROI / カリング条件が下側だけ早く効いている

`transformedBoundingBox()` の ROI 判定や `viewportRectToCanvasRect()` の結果が、
実際の見えている下端より少し小さくなっている可能性がある。

### 4. swapchain / widget resize の同期遅れ

`CompositionViewport` と `CompositionRenderController` の resize 同期が遅れて、
上側は新サイズで描いているが、下側だけ古い viewport 前提の状態が残っている可能性がある。

### 5. 初回 fit のタイミングが早すぎる

`CompositionRenderController::setComposition()` では composition をセットした直後に
`renderer_->fitToViewport()` を呼んでいる。一方で `CompositionViewport::resizeEvent()` は
viewport サイズの更新はするが、再度 `fitToViewport()` を呼ばない。

このため、レイアウト確定前の小さいサイズで一度 fit されたあと、実際の表示領域が
あとから広がっても zoom/pan が追従せず、下側に空白が残る可能性がある。

この仮説は、`fitToViewport()` が「1回だけ」呼ばれているなら特に有力。

### 6. そもそも fit-to-viewport のレターボックスである可能性

`compositionSize = 1920x1080` を `hostSize = 1200x800` に `fitToViewport()` すると、
アスペクト比が一致しないため上下に余白が出る。

margin 付き fit の場合、見えている canvas 高さはおおむね 600px 台になり、
下側 100〜200px 程度の空白が発生しても不自然ではない。

この場合は「未描画」ではなく、単に composition が viewport 全体を埋めていないだけ。
もし期待が「下端まで常に何かが描かれてほしい」なら、`fitToViewport()` のモードを
`fit width` / `fill viewport` / `crop` のいずれかに変える必要がある。

### 5. **最終 present 時の `drawSprite` サイズが間違っている（最有力）**

`renderOneFrameImpl` の GPU パス末尾では、`origViewW` / `origViewH` を使って accum を描画しているが、これらの変数が適切に **ホスト viewport サイズ** を参照していない可能性が高い。

```cpp
// ArtifactCompositionRenderController.cppm (GPU パス末尾)
renderer_->setViewportSize(origViewW, origViewH);
renderer_->drawSprite(0, 0, origViewW, origViewH, renderPipeline_.accumSRV());
```

`origViewW` / `origViewH` が以下のいずれかで不正確な値になっている：

- **未定義**：変数が存在せず何か別のローカル変数（例: `rcw`, `rch`）を参照している
- **誤った初期化**：保存前に `rcw/rch`（ダウンサンプル後サイズ）を代入しており、ホストサイズより小さい
- **未初期化**：定義はあるが代入がなく不定値（0 や小さな値）で描画範囲が制限される

これにより、`drawSprite` が小さい領域（例: ダウンサンプル後サイズ）しか描画せず、「下側だけ空く」状態になる。上側はレイヤー/Gizmo 描画時に正しい viewport で描画されるが、最終 present だけが小さい矩形で描画されるため、下端が見えなくなる。

## 修正案

`renderOneFrameImpl` の先頭で `viewportW` / `viewportH` を計算した後、**元のホスト viewport サイズ**を確実に保存する：

```cpp
const float viewportW = hostWidth_ > 0.0f ? hostWidth_ : cw;
const float viewportH = hostHeight_ > 0.0f ? hostHeight_ : ch;
// ★ 追加 ★
const float origViewW = viewportW;
const float origViewH = viewportH;
```

これにより、`drawSprite(0,0, origViewW, origViewH)` は常にホストの全体を描画し、下端が空く問題は解消される。

## 追記

バグ報告の日付は 2026-04-03 だが、実際の発生初発は 2026-04-02 頃と推測される（コミットログ参照）。

## 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`

## 次の確認ポイント

1. `hostWidget->size()` と `hostWidth_ / hostHeight_` の一致確認
2. `SetViewports()` の実値と `present()` 時のバックバッファサイズ確認
3. `viewportRectToCanvasRect()` が下端を切っていないかの確認
4. `CompositionViewport::resizeEvent()` のタイミングと `resizeDebounceTimer_` の遅延確認
5. `setComposition()` 直後の `fitToViewport()` を 0ms/1フレーム遅延に変えて、空白が消えるか確認する
6. 期待しているのが「レターボックスではなく viewport を常時埋める表示」かを確認する

## 現時点の判断

この問題は、描画そのものよりも、

- viewport 更新
- resize 同期
- ROI の切り方

のどれかにある可能性が高い。

加えて、初回 fit のタイミングが早すぎて、その後の resize に追従できていない可能性がある。
また、`fitToViewport()` の結果として生じる単純な上下余白の可能性も高い。
