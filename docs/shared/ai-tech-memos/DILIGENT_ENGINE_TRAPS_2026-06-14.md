# Diligent Engine Trap Notes

このメモは、Artifact で Diligent Engine を触るときに再発しやすい罠を短くまとめたものです。

## 1. static shader variable は `CreateShaderResourceBinding(true)` だけでは安心しない

`ComputePipelineStateCreateInfo::PSODesc.ResourceLayout.DefaultVariableType` を `STATIC` にした場合、`setBuffer()` / `setTextureView()` で後から埋めるつもりでも、PSO 側からは「初期化時に未設定」と見えることがあります。

安全策:
- cbuffer や動的に差し替えるリソースは `DYNAMIC` として明示する
- `CreateShaderResourceBinding(true)` の後でも `setBuffer()` の戻り値を必ず見る
- `setTextureView()` が false なら、その場で CPU fallback へ戻す

## 2. UAV 書き込み後の readback は copy state を明示する

GPU で `RWTexture2D` に書いたあと staging texture にコピーする場合、`CopyTextureAttribs` に遷移モードを入れないと、D3D12 では `COPY_SOURCE` / `COPY_DEST` の状態違反になることがあります。

推奨:

```cpp
CopyTextureAttribs copy(srcTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                        stagingTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
ctx->CopyTexture(copy);
```

`CopyTextureAttribs` をデフォルト構築してから手で埋めるより、既存の成功パターンに寄せたほうが安全です。

## 3. readback は同期と stride を見る

Diligent の staging texture 読み出しは自動で GPU 完了を待たない backend があります。

確認ポイント:
- `Flush()` だけで終わらせず、必要なら `WaitForIdle()` まで行う
- `MapTextureSubresource()` の `Stride` が想定行幅より小さくないか見る
- `mapped.pData == nullptr` を失敗扱いにする

## 4. リソース名は早めにつける

`TextureDesc.Name` / `BufferDesc.Name` / PSO 名を入れておくと、D3D12 の warning や validation のログがかなり読みやすくなります。

最低限つけたい名前:
- 入力テクスチャ
- 出力テクスチャ
- staging texture
- constant buffer
- PSO 名

## 5. 迷ったら既存の成功例に合わせる

Artifact では同じ Diligent 周りでも、すでに動いている経路がいくつかあります。

まず見る候補:
- `ArtifactCore/src/Graphics/Shader/Compute/Compute.cppm`
- `Artifact/src/Effects/Blur/BlurEffect.cppm`
- `ArtifactCore/src/Render/GPURayTracer.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

新しい書き方を試すより、既存の成功パターンに寄せるほうが事故が少ないです。
