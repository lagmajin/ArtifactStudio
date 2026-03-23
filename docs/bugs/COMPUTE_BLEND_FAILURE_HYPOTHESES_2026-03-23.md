# コンピュートシェーダ レイヤーブレンディング失敗の仮説 (2026-03-23)

## パイプライン概要

```
renderOneFrame()
  for each layer:
    1. ctx->ClearRenderTarget(layerRTV)
    2. drawLayerForCompositionView() → layer テクスチャに描画
    3. blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV, mode, opacity)
       → Compute shader dispatch
    4. swapAccumAndTemp() → temp が次の accum になる
  drawSprite(accumSRV) → スワップチェーンに最終出力
```

## 仮説

### ★ 仮説1: swapAccumAndTemp() 後のリソース状態追跡不整合（最も有力）

**場所:** `ArtifactRenderLayerPipeline.cppm:222` / `CompositionRenderController.cppm:1219-1223`

```cpp
blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV, blendMode, opacity);
renderPipeline_.swapAccumAndTemp();  // std::swap(accum_, temp_)
accumSRV = renderPipeline_.accumSRV();
tempUAV  = renderPipeline_.tempUAV();
```

Diligent の内部状態追跡は `ITexture*` オブジェクト単位。
`std::swap` は C++ の `TextureBundle` 構造体を入れ替えるだけで、
Diligent の内部状態テーブルは元のテクスチャポインタのまま。

→ 次のイテレーションで `accumSRV` が `UNORDERED_ACCESS` 状態のまま
   `SHADER_RESOURCE` として使われ、または逆の不整合が発生する可能性。

### 仮説2: グラフィックス→コンピュート間の GPU 同期不足

**場所:** `CompositionRenderController.cppm:1215-1218`

```cpp
renderer_->setOverrideRTV(layerRTV);
drawLayerForCompositionView(layer, renderer_.get(), 1.0f);  // Graphics draw
renderer_->setOverrideRTV(nullptr);
blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV);    // Compute read
```

`layer` テクスチャへの描画（グラフィックスパイプライン）が完了する前に
コンピュートシェーダーが読み出す可能性がある。

Diligent は単一コマンドリスト内でバリアを挿入するはずだが、
レンダラーが複数コマンドキューを使用している場合は同期が取れない。

### 仮説3: blendDirect() の DstTex 未バインド

**場所:** `LayerBlendPipeline.cppm:134-175`

```cpp
bool LayerBlendPipeline::blendDirect(..., ITextureView* srcSRV, ITextureView* outUAV, ...) {
    exec.setTextureView("SrcTex", srcSRV);
    // ⚠️ DstTex が未設定！
    exec.setTextureView("OutTex", outUAV);
}
```

シェーダーは `Texture2D<float4> DstTex : register(t1)` を宣言しているが、
`blendDirect()` ではバインドされていない。DYNAMIC 変数の未バインドは
D3D12 バリデーションエラーまたは未定義動作。

※ 現在のコンポジションループでは `blendDirect()` は呼ばれていないが、
将来的に使うと失敗する。

### 仮説4: テクスチャフォーマットの不一致

**場所:** `ArtifactRenderLayerPipeline.cppm:42-57`

```cpp
desc.Format = TEX_FORMAT_RGBA16_FLOAT;  // 16-bit float per channel
```

シェーダーは `Texture2D<float4>` / `RWTexture2D<float4>` を使用。
`RGBA16_FLOAT` は DX12 で `float4` と互換だが、
レイヤー描画で `drawSprite` が `RGBA8_UNORM_SRGB` テクスチャから描画する場合、
ブレンド前に `layer` テクスチャに正しいピクセルが書き込まれているか確認が必要。

### 仮説5: ディスパッチサイズの不正確

**場所:** `LayerBlendPipeline.cppm:108-113`

```cpp
auto attribs = ComputeExecutor::makeDispatchAttribs(64, 8, 1);  // 冗長だが無害
attribs.ThreadGroupCountX = (texDesc.Width + 7) / 8;
attribs.ThreadGroupCountY = (texDesc.Height + 7) / 8;
```

`numthreads(8,8,1)` に対して `Dispatch(threadCountX, threadCountY, 1)` は正しい。
テクスチャサイズが 8 の倍数でない場合も `(size+7)/8` で切り上げるため問題なし。

ただし、`makeDispatchAttribs` が内部で何をしているか不明。
追加のフィールドを上書きしている可能性がある。

### 仮説6: 最終出力時の accum テクスチャ状態不正

**場所:** `CompositionRenderController.cppm:1227`

```cpp
renderer_->drawSprite(0, 0, cw, ch, renderPipeline_.accumSRV());
```

最後のループイテレーションで:
1. `blend()` が `tempUAV` に書き込む → `UNORDERED_ACCESS` 状態
2. `swapAccumAndTemp()` → 元の temp が accum になる
3. `drawSprite(accumSRV)` → `UNORDERED_ACCESS` 状態のテクスチャをサンプラーとして読む

`RESOURCE_STATE_TRANSITION_MODE_TRANSITION` が含まれていれば自動遷移するはずだが、
`drawSprite` の内部実装が `TRANSITION_MODE_NONE` を使用している場合、
バリデーションエラーまたは画面に何も表示されない。

### 仮説7: 初期化失敗（pipelineEnabled = false）

**場所:** `CompositionRenderController.cppm:1202`

```cpp
const bool pipelineEnabled = renderPipeline_.ready() && blendPipelineReady_;
```

`renderPipeline_.ready()` か `blendPipelineReady_` が false の場合、
フォールバックパス（直接描画、コンピュート不使用）に入る。

フォールバックパスでは `drawLayerForCompositionView()` が
スワップチェーンに直接描画するため、ブレンドモードが無視される。

確認方法: `pipelineEnabled` の値をログ出力。

## 確認方法

1. `pipelineEnabled` が true かログ出力
2. D3D12 バリデーションレイヤーを有効化してエラー確認
3. RenderDoc でフレームキャプチャし、各ステップのテクスチャ内容を確認
4. `swapAccumAndTemp()` 前後のテクスチャ状態をダンプ

## 関連ファイル

| ファイル | 行 | 内容 |
|----------|----|------|
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` | 90-132 | blend() |
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` | 134-175 | blendDirect() (DstTex 未バインド) |
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm` | 25-42 | createConstantBuffer |
| `ArtifactCore/src/Graphics/Compute.cppm` | 19-40 | ComputeExecutor::build |
| `ArtifactCore/src/Graphics/Compute.cppm` | 90-115 | ComputeExecutor::dispatch |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | 34-57 | TextureBundle / createTextureBundle |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | 222 | swapAccumAndTemp |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 496-505 | blendPipeline 初期化 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1202-1253 | レンダーループ (ブレンドパス) |
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` | 55-77 | Normal ブレンドシェーダー |
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` | 695 | BlendShaders マップ |
