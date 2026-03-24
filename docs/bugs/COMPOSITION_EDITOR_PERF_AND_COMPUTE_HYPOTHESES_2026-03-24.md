# コンポジションエディタ重さ / CS 合成不全 調査メモ (2026-03-24)

## 対象

- `ArtifactCompositionEditor`
- `CompositionRenderController`
- `RenderPipeline`
- `LayerBlendPipeline`
- `PrimitiveRenderer2D`

## 結論サマリ

現状の `Composition Editor` は、見た目は GPU ビューアでも実体は
`CPU で各レイヤーを QImage 化 -> 必要なら OpenCV/CPU effect+mask 適用 -> GPU テクスチャ化して layer RT に描画 -> 最後だけ compute でブレンド`
という経路になっている。

そのため、重さの主因は「CS が遅い」よりも、

- レイヤーごとの CPU サーフェス化
- `QImage -> GPU texture` の再作成 / 再転送
- 再描画要求の多さ
- 毎フレーム `flush/present`

に寄っている可能性が高い。

また、CS 合成が機能しない件は、少なくとも次の 2 つが強い:

- compute shader 側に境界チェックがなく、dispatch は切り上げ実行している
- `swapAccumAndTemp()` が C++ 側のラッパだけを `swap` しており、Diligent のリソース状態追跡と食い違う可能性がある

## コード上の観測

### 1. GPU ブレンド前に CPU サーフェス化が入る

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `137`: `qImageToCvMat`
- `155`: `effect->applyCPUOnly`
- `165`: `mask.applyToImage`
- `194`: `renderer->drawSpriteTransformed(...)`

`drawLayerForCompositionView()` は、レイヤーを直接 GPU リソースとして扱っていない。
effect / mask がある場合は CPU 側で画像加工してから描画している。

### 2. 動画も最終的には `QImage` として扱われる

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `257`: `videoLayer->currentFrameToQImage()`

`Artifact/src/Layer/ArtifactVideoLayer.cppm`

- `388`: `decodeCurrentFrame()`
- `437-438`: `QtConcurrent::run(... getVideoFrameAtFrameDirect(...))`
- `452`: `currentFrameToQImage()`
- `747-754`: `goToFrame()` ごとに非同期デコード起動

つまり動画レイヤーも GPU テクスチャ常駐ではなく、`QImage` ベースの更新になっている。

### 3. transformed sprite は `QImage` の cacheKey 依存で GPU テクスチャを作る

`Artifact/src/Render/PrimitiveRenderer2D.cppm`

- `1233`: `drawSpriteTransformed(..., const QTransform&, const QImage&, ...)`
- `1244`: `image.cacheKey()`
- `1252`: `image.convertToFormat(QImage::Format_RGBA8888)`
- `1263`: `texDesc.Usage = USAGE_IMMUTABLE`
- `1274`: `CreateTexture(...)`

- `1370`: `drawSpriteTransformed(..., const QMatrix4x4&, const QImage&, ...)`
- `1384`: `image.cacheKey()`
- `1396`: `convertToFormat(QImage::Format_RGBA8888)`
- `1408`: `USAGE_IMMUTABLE`
- `1419`: `CreateTexture(...)`

`QImage` の実体が毎フレーム変わると cache が効かず、GPU テクスチャを毎フレーム作り直す。
動画フレーム、effect 適用後画像、mask 適用後画像はこの条件に入りやすい。

### 4. Preview Quality は実レンダー解像度を下げていない

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `371`: `renderPipeline_.initialize(... cw, ch, RenderConfig::PipelineFormat)`
- `576-586`: `setViewportSize()` は `previewDownsample_` を viewport にだけ反映
- `606-611`: `setPreviewQualityPreset()` は viewport を更新して再描画するだけ

`previewDownsample_` は中間テクスチャサイズや layer RT サイズには効いていない。
つまり `Half/Quarter` を選んでも、重い処理の多くはフル解像度のまま走る。

### 5. 再描画トリガーが多い

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `434`: `projectChanged` で再同期
- `452`, `673`: 各 layer の `changed` を購読
- `506`: `frameChanged` で再描画
- `828`, `875`, `1001`: マスク編集中に `selectedLayer->changed()`
- `1038`: hover 頂点検出でも `renderOneFrame()`

`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

- `506`: `currentCompositionChanged`
- `521`: `projectChanged`
- `537`: `playback.currentCompositionChanged`

Editor 本体と controller の両方で composition 追従をしており、`setComposition()` と `renderOneFrame()` が重複しやすい。

### 6. 毎フレーム明示的に `flush/present`

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `1134-1135`
- `1409-1416`

各描画の最後に `renderer_->flush(); renderer_->present();` を呼んでいる。
`flush` は CPU/GPU 同期コストを増やしやすい。

## 重さの仮説

### 仮説 A: 主因は CPU 合成前処理と GPU 再アップロード

最有力。

理由:

- レイヤー描画前に `QImage` 化が入る
- effect / mask は CPU/OpenCV 経由
- transformed sprite は `QImage` から `USAGE_IMMUTABLE` texture を作る
- 動画フレームはフレームごとに画像実体が変わる

結果として、GPU blend path を通っても「前段の重い処理」は消えない。
むしろ `layerRT -> compute blend -> accum/temp swap` が増えるぶん、レイヤー数が多いほど重くなる可能性がある。

### 仮説 B: Preview Quality が体感改善しない

有力。

`previewDownsample_` は viewport の見かけサイズにしか効かず、
`renderPipeline_` や layer の描画素材サイズは `compositionSize` ベースのまま。

そのため `Half/Quarter` を選んでも、

- 動画デコード量
- CPU effect / mask 処理量
- `QImage -> texture` 転送量
- compute dispatch 対象ピクセル数

の多くが下がっていない可能性が高い。

### 仮説 C: 再描画イベント過多

有力。

特に以下が重なっている:

- `projectChanged`
- `currentCompositionChanged`
- `frameChanged`
- layer `changed`
- マスク hover / drag 中の連続再描画

`renderScheduled_` による coalescing はあるが、ソースイベント自体が多く、
再生中や編集中は常に次フレームが積まれ続ける構造になっている。

### 仮説 D: `flush()` がフレーム pacing を悪化させている

中程度。

明示 `Flush` は driver / queue の進行を細かく区切るので、
upload と compute と present が混ざる現状ではボトルネックを悪化させやすい。

## CS 合成が機能しない仮説

### 仮説 1: compute shader に境界チェックがない

最有力。

`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`

- `67`, `94`, `119`, ...: `[numthreads(8,8,1)]`
- `70-71`, `97-98`, ...: `SrcTex[id.xy]`, `DstTex[id.xy]`
- `78`, `103`, `127`, ...: `OutTex[id.xy]`

`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`

- `169`: `makeDispatchAttribs(64, 8, 1)`
- `171-172`: `(Width + 7) / 8`, `(Height + 7) / 8`

dispatch は 8 で切り上げているのに、shader 側で
`if (id.x >= width || id.y >= height) return;`
が無い。

composition size が 8 の倍数でない場合、範囲外アクセスで失敗する可能性が高い。

### 仮説 2: `swapAccumAndTemp()` と状態追跡の不整合

有力。

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `1236`: `blendPipeline_->blend(...)`
- `1244`: `renderPipeline_.swapAccumAndTemp()`

`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`

- `222-224`: `std::swap(impl_->accum_, impl_->temp_)`

swap しているのは C++ 側の bundle だけで、GPU リソース状態はそのまま。
次の loop で、直前に UAV 書き込みしたテクスチャを SRV として読む順序になり、
Diligent の内部状態追跡とズレる可能性がある。

### 仮説 3: 実は compute path に入っていない

有力。

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `1173`: `const bool pipelineEnabled = renderPipeline_.ready() && blendPipelineReady_;`

ここが `false` だと fallback path に落ちる。
見た目上は「CS が動かない」に見えるが、実際は初期化失敗で GPU blend に到達していないだけ、という可能性がある。

### 仮説 4: `blendDirect()` は壊れているが、現状本件の主因ではない

`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`

- `179`: `blendDirect(...)`

`blendDirect()` は設計上まだ危ないが、現在の composition loop では `blend()` を使っている。
したがって今回の「コンポジションエディタで CS 合成が動かない」の直接原因とは限らない。

## 優先度付きの確認項目

### 優先 1

- `pipelineEnabled`
- `blendPipelineReady_`
- initialize 時の executor 作成数

まず compute path に本当に入っているか確認する。

### 優先 2

- composition size を `1919x1079` のような 8 非倍数で試す
- D3D12 validation / RenderDoc で dispatch 直後を見る

これで境界チェック欠如の仮説をかなり早く切り分けられる。

### 優先 3

- `drawLayerForCompositionView()` の時間
- `drawSpriteTransformed()` の texture 作成回数
- `Flush` 前後の時間

重さの本命が CS ではなく upload / CPU effect 側にあることを数値で確定する。

### 優先 4

- `previewDownsample_` 選択時に中間 RT サイズが変わっているか

変わっていなければ preview quality は UI 上の見かけ設定に留まっている。

## 現時点の整理

現行実装では、CS 合成が仮に正常化しても、
コンポジションエディタ全体の重さは大きくは解消しない可能性が高い。

理由は、最も重そうな部分が

- CPU でのレイヤー画像生成
- 動画フレーム取得
- effect / mask の CPU 処理
- `QImage` から GPU texture への再生成

に残っているため。

したがって順序としては、

1. compute path が本当に有効か確認
2. 境界チェックと resource state の問題を潰す
3. その後に `QImage` ベース upload 依存を減らす

で進めるのが妥当。
