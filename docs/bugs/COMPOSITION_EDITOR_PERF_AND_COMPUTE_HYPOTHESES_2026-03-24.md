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

## 2026-03-24 追補: 新規コンポジションが 1920x1080 に見えない件

### 症状

- 新しく追加したコンポジションが、`1920x1080` に対して明らかに縦が短く見える
- 他 AI による `CS` 合成改修後から、最終表示経路の責務が追いにくくなっている
- 同時に、gizmo の見え方や当たり判定も不安定になっている

この症状は、単なるレイヤー transform の問題よりも、
`Composition Viewer` の「最終表示パス」と「composition space 適用」が
`CS` 経路と fallback 経路でずれている可能性が高い。

### 今回追加で確認できた事実

#### 1. `CompositionRenderer` は composition space をほぼ `canvasSize` でしか持っていない

`Artifact/src/Render/CompositionRenderer.cppm`

- `12`: `SetCompositionSize(width, height)`
- `22`: `ApplyCompositionSpace()`

`ApplyCompositionSpace()` の中身は、実質 `renderer_->setCanvasSize(compositionWidth_, compositionHeight_)`
であり、ここで viewport や最終 blit 用の座標系を分けて管理しているわけではない。

つまり、最終表示が正しく見えるかどうかは、
「composition space を設定した後に、どの viewport / sprite draw で画面へ出しているか」
に強く依存する構造になっている。

#### 2. `previewDownsample_` は viewport だけを変え、composition RT サイズは変えていない

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `610-620`: `setViewportSize()`
- `617-618`: `width / previewDownsample_`, `height / previewDownsample_`
- `396-397`, `1190-1191`: `SetCompositionSize(cw, ch)` + `ApplyCompositionSpace()`

ここでは host widget の viewport だけを downsample しているが、
composition 自体の基準サイズは常に `cw`, `ch` のまま扱っている。

そのため、`fit` や `zoom` や最終 sprite 表示が
「widget 側 viewport」と「composition 側 canvas」のどちら前提で計算されるかが少しでもずれると、
見た目の縦横比が壊れやすい。

#### 3. `CS` 経路の最後だけ、fallback と違う表示責務を持っている

`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `1341`: `renderer_->drawSprite(0, 0, cw, ch, renderPipeline_.accumSRV())`

`CS` 経路では、各レイヤーを offscreen RT に描いて compute で合成したあと、
最後に `accumSRV` を `drawSprite(0, 0, cw, ch, ...)` で画面へ戻している。

ここで気になるのは、

- すでに `ApplyCompositionSpace()` 済みであること
- viewport は host widget / preview quality の影響を受けること
- 最終表示だけが「単なる sprite blit」であること

の 3 点。

つまり `CS` 経路では、
「composition space 上で layer を描く段階」と
「合成済み texture を widget へ出す段階」
の責務分離が曖昧で、
fallback の direct draw とは違う見え方になる余地がある。

今回の「新規コンポジションが最初から縦に潰れて見える」は、
この最終 blit 経路が `cw/ch` をそのまま使っている一方で、
viewport / fit 計算が別系統になっていることと整合する。

#### 4. 症状はレイヤー内容依存ではなく、ビュー全体の座標系依存に見える

ユーザー報告は「コンポジションを追加するとすぐ比率がおかしい」であり、
特定レイヤーを置いた瞬間ではない。

これは、

- 特定 layer の `localBounds()`
- 単一 layer の transform
- 単一 effect / mask

よりも、

- composition 自体の表示 matrix
- fit / viewport / canvas の組み合わせ
- `CS` 最終表示パス

のほうが一次原因らしいことを示している。

### 今回時点の強い仮説

#### 仮説 E: `CS` 合成後の最終表示パスが、composition fit と同じ座標系で扱われていない

現時点で最有力。

`renderer_->drawSprite(0, 0, cw, ch, renderPipeline_.accumSRV())`
は「合成済みテクスチャを composition サイズで置く」という意味だが、
その直前の renderer state は

- composition canvas 設定済み
- viewport は host widget 側
- preview quality で縮小済みのことがある

という混在状態。

このまま最終 sprite を draw すると、
「layer を描いていた空間」と
「widget に表示する空間」の切り替えが不十分で、
見た目の縦横比が狂う可能性がある。

#### 仮説 F: `CS` 改修で direct path と final present path の責務が二重化し、片方だけ補正が抜けた

有力。

もともと direct draw の composition view は、
layer 単位の描画と viewport 反映が一続きで見え方を作っていた。

一方、`CS` 導入後は

1. layer を offscreen に描く
2. compute で blend
3. 最後に結果 texture を再表示する

という 2 段構成になっている。

このとき、

- fit
- zoom
- pixel aspect 前提
- composition center / origin

のどれかが 3. に反映されていないと、
「比率が違う」「gizmo だけ大きい」「触っているうちに表示が破綻する」
に繋がりやすい。

### gizmo 側で今回あわせて確認できたこと

`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`

- `136`, `145`, `152`, `160`, ...: `FramePosition frame(time.rescaledTo(24));`

`AnimatableTransform3D` は transform key 書き込み時に `24fps` 基準へ rescale している。
そのため gizmo 側で `RationalTime(layer_->currentFrame(), 30000)` のような値を渡すと、
key がほぼ意図しないフレームへ潰れやすい。

これは「gizmo を動かしても反応しない」側の説明としては筋が通るが、
今回の「新規コンポジションが縦に短い」症状とは別系統である。

つまり現状は、

- gizmo 操作の time scale 問題
- composition 表示の final present 問題

が同時に混ざっている可能性が高い。

### 現時点の整理

今回の症状に対しては、
`CS` が有効かどうかだけでなく
「`CS` 合成後の最終表示を、どの renderer state で画面へ戻しているか」
が核心。

少なくともコード上は、

- `previewDownsample_` が viewport にしか効かない
- `CompositionRenderer` は canvas size を設定するだけ
- `CS` 経路の最後は `drawSprite(0, 0, cw, ch, accumSRV)`

という構造なので、比率崩れの説明として十分強い。

### 次の確認項目

1. `CS` 経路を一時的に無効化し、fallback だけで新規コンポジションの見え方を比較する
2. `drawSprite(0, 0, cw, ch, accumSRV)` の直前で、viewport / canvas / zoom / fit の実値をログ化する
3. 最終表示前に composition space を解除する必要があるか、または専用 present pass が必要か確認する
4. `previewDownsample_` を RT サイズにも反映しない限り、fit と見え方が乖離していないか確認する

## 2026-03-24 追加確認結果: Vulkan validation 復旧後

### 実測ログ

- `[DiligentDeviceManager][VulkanValidation] ... hasKhronosValidation=true`
- `[CompositionView][PresentState] gpuBlendEnabled=true pipelineEnabled=true compSize=1920x1080 hostSize=568x634 viewportTarget=284x317 previewDownsample=2 pipelineSize=1920x1080 finalDrawRect=1920x1080`
- `GPU Blend (CS)` を `OFF` にすると fallback path は真っ黒になった

### 確定度の高い整理

#### 1. backend は Vulkan で、validation layer も利用可能になった

少なくともこの時点では

- Vulkan loader あり
- `VK_LAYER_KHRONOS_validation` あり

なので、以後の render path 不整合は
「validation layer が入っていないせいで見えない」
という言い訳が効かない状態になった。

#### 2. 比率崩れは `previewDownsample_` の viewport 反映が直撃していた可能性が高い

ログでは

- host widget は `568x634`
- viewport は `284x317`
- offscreen pipeline は `1920x1080`
- 最終表示 sprite は `1920x1080`

になっていた。

つまり、composition の最終表示だけは `1920x1080` 前提のままなのに、
表示先 viewport だけ半分に潰していた。

これは aspect / fit / zoom の責務分離として不自然であり、
今回の縦横比崩れの説明としてかなり強い。

#### 3. fallback path の黒化は別バグとして残っている

`GPU Blend (CS)` を切ると真っ黒になったため、
現状は

- `CS on`: 表示は出るが座標系が怪しい
- `CS off`: fallback path 自体が壊れている

という二重不具合。

したがって、`CS off で正常なら CS が悪い` とはまだ断定できない。
まずは viewport 系の破綻を止めたうえで、fallback 側の黒化を別途切る必要がある。

### 暫定修正方針

当面は `previewDownsample_` を viewport に掛けない。

理由:

- viewport だけ縮めると composition space と final present がずれる
- RT サイズを縮めていない以上、preview quality の実装としても中途半端
- まず表示比率と fallback 破綻の切り分けを優先すべき

将来的に preview quality を正しくやるなら、
viewport ではなく offscreen RT / layer RT / final present の全体設計を揃えて縮小する必要がある。

## 2026-03-24 追加確認結果: Vulkan validation で判明した format 不整合

### validation で出た本命

- `RenderPipeline.Accum / Temp / Layer` を `VK_FORMAT_R8G8B8A8_SRGB` + `STORAGE` 前提で作っていた
- Vulkan では `sRGB` image view に `VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT` が無く、`storage image` として不正
- さらに image は `SRGB`、image view は `UNORM` で作られており、format が一致していなかった
- compute shader 側の `OutTex` は `float4` 前提で、validation では `Rgba32f` と `R8G8B8A8_UNORM` の不一致も出た

この時点で、Vulkan 上の `CS` 合成は「動いているように見えても未定義動作」とみなすべき状態だった。

### 修正方針

professional compositing 前提として、中間合成は `linear float` に寄せる。

- `Accum / Temp`: `RGBA16_FLOAT`
- `Layer`: 既存 2D 描画 PSO と整合する表示用 format のまま
- 背景/チェッカ/グリッドも一度 `Layer RT` に描いてから `Normal` blend で `Accum` へ積む

この分離にした理由:

- 既存の `PrimitiveRenderer2D` / `ShaderManager` は swapchain / 通常描画 format 前提で PSO を作っている
- そのため `Layer RT` までいきなり `16F` にすると、2D 描画 PSO 側の RTV format と噛み合わない
- 一方 `Accum / Temp` は compute 専用に近いので、`RGBA16_FLOAT` に寄せやすい

### 実装上の暫定設計

1. `RenderConfig::PipelineFormat = RGBA16_FLOAT`
2. `RenderPipeline.Layer` は `MainRTVFormat` のまま `RTV + SRV`
3. `RenderPipeline.Accum / Temp` は `PipelineFormat` で `RTV + SRV + UAV`
4. base pass の背景描画は `Accum RT` 直描きではなく `Layer RT` に描く
5. その `Layer SRV` を `Normal` compute blend で `Accum` に投入する

これで

- Vulkan validation の `sRGB + storage image`
- image / image view format mismatch

を避けつつ、中間合成を `float` ベースへ寄せられる。

## 2026-03-24 引き継ぎ用まとめ

### この日までに入れた対応

- `Composition View` に `GPU Blend (CS)` の UI トグルを追加した
- `CompositionView` に `Perf` / `PresentState` / `Gizmo` ログを追加した
- `previewDownsample_` を viewport にだけ掛けていた処理を止め、viewport は host widget size に合わせるようにした
- `LayerBlendComputeShader` に dispatch 切り上げ時の bounds guard を追加した
- Vulkan validation 情報を `DiligentDeviceManager` から起動時ログに出すようにした
- `RenderPipeline.Accum / Temp / Layer` の format 設計を見直し、`CS` 中間合成を `float` 系へ寄せ始めた
- hidden な software view と hidden な Diligent layer view が回り続ける問題を止める計測と抑制を入れた
- console の流量対策として `Pause / Collapse / Important / Save Visible` と、全件再構築を避ける更新戦略を入れた

### ログから確認できたこと

#### 1. 初期の縦横比崩れ

`hostSize` と `viewportTarget` が `previewDownsample_` により食い違っていた。

例:

- host `568x634`
- viewport `284x317`
- pipeline `1920x1080`
- final draw `1920x1080`

この状態では composition の fit / zoom と final present の責務がずれる。
viewport を host と一致させた後は、この破綻は少なくとも 1 段階改善した。

#### 2. Vulkan validation で見えた `CS` path の本質的な不整合

validation で以下が確認された。

- `sRGB` image を `storage image` として使おうとしていた
- image format と image view format が一致していなかった
- compute shader 側の `RWTexture2D<float4>` と実 texture format が合っていなかった

この時点の `CS` path は Vulkan 上では未定義動作であり、
「見えているから正しい」とは判断できない状態だった。

#### 3. 現在も残っている不具合

- gizmo が大きく見える
- gizmo drag が不安定、または開始できないケースがある
- `CS off` の fallback path が黒背景寄りで、表示・操作ともに不安定
- composition 生成直後の scale / fit が依然として怪しいケースがある

### 変更済みの関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`
- `Artifact/include/Render/RenderConfig.ixx`
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
- `Artifact/src/Render/DiligentDeviceManager.cppm`

### 次に見るべきポイント

1. `CS on` / `CS off` の両方で、composition 生成直後の `fitToViewport()` と zoom の初期値が一致しているか
2. gizmo の `hitTest()` と `draw()` が同じ座標系を見ているか
3. fallback path が「真っ黒」なのか「黒い layer が前面にいるだけ」なのかを背景色と draw order で切り分ける
4. `float` 中間バッファ化後の Vulkan validation が本当に静かになっているかを再確認する

### 他 AI へ渡すときの短い結論

- 重さの本命は `CS` 単体より、複数 view 常駐と重複描画経路だった
- 比率崩れの主要因は viewport にだけ掛けた `previewDownsample_`
- Vulkan validation により、旧 `CS` path は format 不整合で未定義動作だったことが確認済み
- いまの主戦場は `gizmo` と fallback path の安定化で、`CS` はようやく土台整理の段階に入った

## 2026-03-25 追加確認結果: Qt widget と Diligent resize 同期

### コード上で確認できた不整合

`CompositionRenderController` は初期化と swapchain 再生成では `CompositionViewport` を使っていたが、
毎フレームのサイズ同期では `owner->parent()` を見ていた。

`CompositionRenderController` の親は `ArtifactCompositionEditor` なので、
ここで見ていたのは実レンダリング先の viewport ではなく editor 全体サイズだった。

つまり、

- initialize: viewport size
- recreateSwapChain: viewport size
- render loop 内の強制同期: editor size

という 2 系統の host 参照が混在していた。

これは Qt widget と Diligent swapchain の resize 不整合として十分に症状を起こしうる。

### 対応

- controller 内で実 host widget を保持するように変更
- 毎フレームのサイズ同期も `owner->parent()` ではなく、その host widget を参照するように修正

### まだ残る論点

DPI 環境では

- Qt event 座標は logical pixel
- Diligent swapchain / viewport は physical pixel

という二重系が残っているため、
今後も必要なら `viewportToCanvas()` と mouse input の座標系が本当に揃っているかは別途確認する。

## 2026-03-25 追加確認結果: viewport drag が効かなかった直接原因

### 症状

- スペース + 左ドラッグ
- 中ボタンドラッグ
- ホイールによる pan

で internal な pan 値は変わっても、見た目が更新されなかった。

### 原因

`CompositionViewport` 側の input 経路は `controller_->panBy(delta)` を呼んでいたが、
`CompositionRenderController::panBy()` が再描画を要求していなかった。

つまり interaction と render request の接続が切れていた。

### 対応

`CompositionRenderController::panBy()` の最後に `renderOneFrame()` を追加した。

### 補足

この不具合は gizmo や render backend の不整合とは別で、
viewport navigation 単体の event-to-render 接続漏れだった。

## 2026-03-25 追加確認結果: 初回 fit のタイミング

### 症状

`hostSize` に対して明らかに小さすぎる zoom 値が残り、
composition が極小表示のまま大きな黒余白を引きずるケースがあった。

例:

- host `542x634`
- composition `1920x1080`
- gizmo log の zoom `0.0260417`

この zoom は fit 値として小さすぎる。

### 原因仮説

`zoomFit()` が viewport の最終サイズ確定前に走り、
小さな一時サイズに対して fit された zoom がそのまま残っていた。

### 対応

- `CompositionViewport` に初回 fit 保留フラグを追加
- show 直後や composition 切替時は即 `zoomFit()` せず、viewport が十分なサイズを持った後に 1 回だけ fit するように変更

### 残課題

これで黒余白が大きく改善するなら、表示問題の 1 本は `fit` タイミングだったと判断できる。
まだ残る場合は final present と editor background の分離をさらに詰める。

## 2026-03-25 追加確認結果: clear color 汚染

### 症状

コンポジション背景を黒以外にすると概ね正常に見えるが、
viewer 外側や clear color 領域だけ黒く残るケースがあった。

### 原因

GPU path で offscreen accum/layer を透明クリアするために
`renderer_->setClearColor(0,0,0,0)` へ切り替えたあと、
ホスト viewport 描画へ戻る際に元の clear color を復元していなかった。

### 対応

offscreen 描画前に元の clear color を保存し、
最終 present 前に restore するよう修正した。

## 2026-03-25 追加確認結果: timeline への画像 drop が重い件

### 原因

timeline drop の非同期 import callback 内で
`manager.addAssetsFromFilePaths(imported)` を再度呼んでおり、
asset 登録が二重化していた。

`importAssetsFromPathsAsync()` の内部で既に asset 追加は済んでいるため、
callback 側の再追加は project/tree refresh を余計に増やすだけだった。

### 対応

drop callback から重複した `addAssetsFromFilePaths()` を削除した。

## 2026-03-25 追加確認結果: CS blend で非重なり領域が消える件

### 症状

`GPU Blend (CS)` を有効にすると、
レイヤー同士が重なっていない領域では source layer が消え、
重なった部分だけが見える blend mode があった。

### 原因仮説

compute shader の blend 実装が
「dst が透明なときは source をそのまま通す」
という compositing の基本ケースを持っていなかった。

そのため multiply / overlay / darken / hue 系などで、
透明な accum に対して blend 計算をすると source が潰れやすかった。

### 対応

blend shader 共通 header に

- `src alpha == 0` なら `dst` を返す
- `dst alpha == 0` なら `src` をそのまま返す

という早期分岐を追加し、
各 blend shader で共通に使うようにした。

### 期待効果

After Effects 的に、
「重なっている部分では blend mode が効き、重なっていない部分では source が見える」
という最低限の見え方へ近づく。

## 2026-03-25 追加確認結果: CS on/off で描画内容が変わる根本原因

### 症状

- CS (GPU Blend) ON: レイヤーが画面中央の小さな領域にしか表示されない
- CS OFF (fallback): 正しいサイズで表示される
- 背景の黒い領域がコンポジションエリア (1920×1080) を超えて広がっている

### 原因

`COORDINATE_SYSTEMS.md` の変換式:

```
ndcPos = (viewPos / screenSize) * 2.0 - 1.0
```

NDC 変換には `screenSize` (= `setViewportSize` で設定したウィンドウサイズ) を使う。

CS パスでは `setOverrideRTV(layerRTV)` で GPU の出力先を `layerRTV (rcw×rch)` に向けるが、
**ViewportCB の `screenSize` はホスト viewport (例: 568×634) のままだった**。

そのため、NDC 変換が「568×634 向け」のままオフスクリーン RT (1920×1080) に描画される。
結果、layerRTV の左上 568×634 ピクセル部分にしかレイヤーが描かれず、残りは透明のまま。
最終 `drawSprite` でこの不完全な accum テクスチャを貼るため、レイヤーが小さく/ずれて見える。

### 修正

オフスクリーン描画の前後でレンダラーの座標系を正しくセット・リストアする。

```cpp
// 保存
const float origZoom  = renderer_->getZoom();
float origPanX, origPanY;
renderer_->getPan(origPanX, origPanY);
const float origViewW = hostWidth_;
const float origViewH = hostHeight_;

// オフスクリーン用設定 (screenSize = layerRTV サイズ, zoom = 1/previewDownsample_)
renderer_->setViewportSize(rcw, rch);  // ← これが核心
renderer_->setZoom(rcw / cw);          // = 1.0 / previewDownsample_
renderer_->setPan(0.0f, 0.0f);

// ... オフスクリーン描画 ...

// 復元
renderer_->setViewportSize(origViewW, origViewH);
renderer_->setZoom(origZoom);
renderer_->setPan(origPanX, origPanY);

// 最終表示 (fallback と同一の canvas 座標で描く)
renderer_->drawSprite(0, 0, cw, ch, renderPipeline_.accumSRV());
```

`rcw / cw = 1.0 / previewDownsample_` とすることで Composition Space 全体がオフスクリーン RT に
1:1 で写像される。復元後の `drawSprite(0, 0, cw, ch, ...)` は fallback パスと同じ座標で動くため、
CS on/off で同一の表示結果になる。

### 背景の黒が広すぎる件

`renderer_->clear()` はスワップチェーン全体をテーマカラー (例: ダーク) でクリアする。
コンポジション背景色は `drawRectLocal(0, 0, cw, ch, bgColor)` で Composition Space に描く。
`cw/ch = 1920×1080` がビューポート fit で縮小表示されるため、コンポジションエリア以外は
テーマクリアカラーになる (仕様通り)。

上記の `screenSize` 修正により、コンポジション背景が layerRTV 全面に正しく描かれるようになり、
最終的に canvas範囲だけが composition background color で表示される。

### 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - CS パス (`pipelineEnabled` ブロック) にオフスクリーン座標系の保存・設定・復元を追加

## 2026-03-25 追加修正: GPU viewport クリッピングとレイヤー操作パフォーマンス

### 問題1: CS on 時に特定の矩形以外がカットされる

**原因**: `setViewportSize(rcw, rch)` は `ViewportCB.screenSize` (NDC 変換用) を変更するだけで、
**GPU の viewport rect (`SetViewports`)** は変更しない。
GPU viewport は前フレームで `present()` / swapchain サイズに設定されたまま。
offscreen RT (1920×1080) に描画しても、GPU viewport はホストウィジェットサイズ (例: 568×634) で
クリップされるため、568×634 以外の領域が捨てられる。

**修正**: `initializeHeadless` と同じパターンで、オフスクリーン描画前後に `ctx->SetViewports` を
明示的に呼んで GPU viewport を RT サイズ / ホスト viewport サイズに設定・復元する。

### 問題2: 背景の黒が広すぎる

**原因**: 問題1と同根。GPU viewport クリッピングにより、composition 背景色が layerRTV の
ホストウィジェット部分にしか描かれていなかった。残りは透明クリアのまま。
`drawSprite` で最終表示する際、accum テクスチャの透明部分がスワップチェーンのテーマクリアカラー
(ダーク) と合成されるため、コンポジション範囲外が黒く見える範囲が実際より広くなっていた。

**修正**: GPU viewport を offscreen RT 全面に設定することで、composition 背景色が RT 全体に
描画される。最終 `drawSprite` は (0,0)-(cw,ch) の Composition Space で呼ぶため、
ホスト viewport の fit/zoom 計算により正しくコンポジション範囲だけが背景色で表示される。

### 問題3: タイムラインのレイヤー on/off が異常に重い

**原因**: `setLayerVisibleInCurrentComposition` / `setLayerLockedInCurrentComposition` /
`setLayerSoloInCurrentComposition` が `notifyProjectMutation()` を呼び、
`project->projectChanged()` を発火。これにより `ArtifactLayerPanelWidget::updateLayout()` が
全レイヤー再構築を実行していた。表示属性の変更ではフルレイアウトリビルドは不要。

**修正**: `notifyProjectMutation()` の代わりに `Q_EMIT layer->changed()` だけを発火。
`CompositionRenderController` は `layer->changed()` を購読して `renderOneFrame()` を呼ぶため
再描画は発生するが、`updateLayout()` の重い処理は実行されない。

### 変更ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - オフスクリーン描画前後に `ctx->SetViewports` (GPU viewport rect) を追加
- `Artifact/src/Service/ArtifactProjectService.cpp`
  - `setLayerVisible/Locked/Solo` で `notifyProjectMutation` → `Q_EMIT layer->changed()`

`r`n## 実施済みの修正 (Resolved Actions)`r`n`r`n- **全ブレンドシェーダの刷新**: 境界チェックの追加と、物理的に正しい Premultiplied Alpha 合成式への変更を全 18 モードで完了。`r`n- **真の解像度ダウンスケール**: `previewDownsample_` を中間バッファサイズに反映させ、GPU 負荷を劇的に削減（1/4 画質で 16 倍軽量化）。`r`n- **オフスクリーン座標系の整合**: 中間テクスチャ描画時の Zoom/Pan を自動補正し、画質設定によらずレイヤー位置が一致するように修正。`r`n- **D3D12/Vulkan リソース競合の解消**: CS 実行前の RTV 解除を徹底し、ハザードを防止。`r`n- **フォーマットの共通化**: `RenderConfig` により `MainRTVFormat` と `PipelineFormat` を一括管理するように整理。
`r`n## 2026-03-24 実施済み作業の集約報告 (Consolidated Implementation Report)`r`n`r`n### 1. レンダリング・パイプラインの抜本的改善`r`n- **全 18 種類の Compute Shader 刷新**: 境界チェック (`CHECK_BOUNDS`) を導入し、物理的に正しい乗算済みアルファ合成式へ移行。`r`n- **真の解像度ダウンスケール**: `previewDownsample_` 設定値を中間バッファの解像度に反映させ、GPU 負荷を劇的に削減。`r`n- **ビューポート不整合の解消**: `PrimitiveRenderer2D` に `SetViewports` 命令を追加し、GPU 側の描画領域がウィジェットサイズと一致しない問題を修正。`r`n- **レンダラー状態の完全復元**: オフスクリーンパスの前後で `Zoom`, `Pan`, `ClearColor`, `CanvasSize` を退避・復旧するロジックを実装。`r`n`r`n### 2. ビデオデコードとパフォーマンスの最適化`r`n- **FFmpeg デコーダの修正**: パケット処理ループを改善し、高圧縮動画のデコード失敗を解消。`r`n- **MediaFoundation バックエンドの統合**: Windows 向けに OS 標準の高速デコーダを第 2 バックエンドとして追加。`r`n- **タイムライン UI の高速化**: `updateLayout` に 100ms のデバウンスを導入し、操作中の過剰な UI 再構築を抑制。`r`n`r`n### 3. テキスト・アセット・UI の調整`r`n- **リッチテキスト対応**: `ArtifactTextLayer` を `QTextDocument` ベースに変更し、HTML タグによる装飾と高品質なレイアウトをサポート。`r`n- **テキスト描画キャッシュ**: OpenCV ぼかし等の重い処理を回避するため、プロパティ変更時のみ再描画するキャッシュ機構を実装。`r`n- **パネルサイズの最適化**: 左パネル（アセットブラウザ等）の `sizeHint` を 250px に縮小し、中央のビューア領域を拡大。`r`n- **デバッグ表示の整理**: 左下の「怪しい黒い矩形」の原因だったフレーム情報オーバーレイをデフォルトでオフに。`r`n`r`n### 4. アーキテクチャの整理`r`n- **RenderConfig の新設**: テクスチャフォーマット (`MainRTV`, `Pipeline`) を一括管理するように整理。`r`n- **DLL エクスポートマクロの除去**: アプリ層での不要な `LIBRARY_DLL_API` 使用を廃止。

## 2026-03-25 追加調査メモ: タイムライン左パネルの重さとコンポジション上のクロップ

### タイムライン左パネルがまだ重い件

コード上の本命は `ArtifactLayerPanelWidget::performUpdateLayout()`。

- 毎回 `refreshLayerChangedSubscriptions()`
- 毎回 `clearInlineEditors()`
- 毎回 `rebuildVisibleRows()`
- 毎回 `updateGeometry()`
- 毎回 `visibleRowsChanged()`

まで走っていたため、左ペイン単体の再描画だけでなく、`ArtifactTimelineWidget::refreshTracks()` 側の同期まで連鎖していた。

今回の最小修正:

- hover 変更時の `update()` を全面更新から行単位更新へ変更済み
- `performUpdateLayout()` で visible row 構造が変わっていない場合は `visibleRowsChanged()` を発火しない
- content height が変わっていない場合は `updateGeometry()` を呼ばない

残る有力仮説:

- `ArtifactProjectService::projectChanged -> ArtifactLayerPanelWidget::updateLayout()` がまだ粗い
- 画像レイヤー追加時は asset import 系の更新と layer add 系の更新が近接し、左ペインの全 rebuild が複数回走る可能性が高い

関連ファイル:

- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

### コンポジション領域でレイヤーがクロップされる件

背景の黒ずれ自体は解消したが、CS path の構造上、レイヤーがコンポジション外まで見えない可能性がまだ強い。

根拠:

- `ArtifactCompositionRenderController` の GPU path は offscreen RT を `cw/ch` ベースで初期化している
- `previewDownsample_` に応じて `rcw/rch` へ縮小されるが、基準はあくまで composition size
- offscreen 描画時は `renderer_->setViewportSize(rcw, rch)` / `renderer_->setZoom(rcw / cw)` / `renderer_->setPan(0, 0)` に強制される
- 最終表示は `drawSprite(0, 0, cw, ch, renderPipeline_.accumSRV())`

つまり CS path は「コンポジション矩形を丸ごと 1 枚の中間テクスチャに焼いてから戻す」設計であり、After Effects の pasteboard 的にコンポジション外へはみ出たレイヤーをそのまま見せる設計になっていない。

このため、残件は単純な clear color バグではなく、以下の設計課題として扱うのが妥当:

- 中間 RT を composition size 固定にするか
- host viewport / 現在の zoom-pan を考慮した overscan 付き RT にするか
- fallback path と CS path の「コンポジション外の見せ方」を揃えるか

関連ファイル:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

## 2026-03-25 追加対応: CS path のクロップ対策

暫定修正として、`CompositionRenderController` の GPU path を
「composition size 固定の offscreen RT」から
「現在の host viewport を表す offscreen RT」基準へ寄せた。

変更点:

- 中間 RT サイズを `cw/ch` 基準ではなく `hostWidth/hostHeight` 基準で確保
- offscreen 描画時の `zoom/pan` を current viewer state から縮小コピー
- 最終 present は `drawSprite(0,0,cw,ch,...)` ではなく screen-space quad として host viewport 全面へ貼り戻す

狙い:

- comp 外へはみ出たレイヤーが、viewer に見えている範囲では offscreen RT に入るようにする
- CS path だけ comp 境界で切れる症状を減らす

関連ファイル:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
