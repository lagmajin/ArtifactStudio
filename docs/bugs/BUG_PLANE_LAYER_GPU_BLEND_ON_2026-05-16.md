# Plane Layer + GPU Blend ON Investigation

**Date:** 2026-05-16  
**Status:** Investigation / mitigation planning

## Symptom

`ArtifactCompositionEditor` で平面レイヤー同士を重ね、`GPU blend` を ON にすると、期待した重なり方にならず、画面全体が単色寄りの塗りつぶしや不自然な色味になることがある。

典型例:

1. 下: salmon 系の plane layer, `Blend: Normal`
2. 上: white 系の plane layer, `Blend: Add`
3. 結果: 期待する加算合成の見え方ではなく、ほぼ一様な bluish fill のように見える

## Why Plane Layers Expose This Easily

平面レイヤーは次の条件を満たしやすく、blend 境界の不具合を非常に目立たせる。

1. 画面を大きく覆う
2. 色が一定で、テクスチャ細部がない
3. alpha が 1.0 に近い
4. `ArtifactSolid2DLayer` / `ArtifactSolidImageLayer` は composition view で GPU texture cache path に入りやすい

そのため、色空間・alpha 契約・accum 初期値のズレがあると、局所的な崩れではなく「画面全体の塗りつぶし」として現れやすい。

## Current Code Path

関連境界:

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
  - `layerUsesGpuTextureCacheForCompositionView()` に `ArtifactSolid2DLayer` / `ArtifactSolidImageLayer` が含まれる
- `Artifact/src/Render/GPUTextureCacheManager.cppm`
  - GPU texture cache への upload を担当
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - `layerRTV -> blendPipeline -> accum/temp` の合成を担当
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
  - compute blend shader の alpha / RGB contract を定義

現状の documented contract:

1. `layer`: `RGBA8_sRGB`
2. `accum` / `temp`: `RGBA32F`
3. shader 側では `srcA = src.a * opacity`
4. shader 側では `srcRGB = src.rgb * srcA`
5. `dst.rgb` は premultiplied-style accumulated color として扱われる

## Most Likely Causes

### 1. Layer RT と accum/temp の format boundary がまだ不安定

既存 contract でも、

- layer input = `RGBA8_sRGB`
- accum/temp = `RGBA32F`

という非対称が残っている。

この状態で plane layer のような全面・高 alpha・定色ソースを `Add` すると、sRGB->linear の解釈差や sampling 境界のズレがそのまま画面全体の色味崩れとして見えやすい。

特に今回の症状が「白飛び」ではなく「bluish fill」寄りである点は、単純な Add の数式ミスだけでなく、format / color boundary の疑いを強める。

### 2. Blend shader の alpha contract が straight input と premult accumulation の混成

`LayerBlendComputeShader.ixx` では:

1. `src` を sampled texture として読む
2. `srcA` を計算する
3. `src.rgb` に対して `srcA` を掛ける
4. `dst.rgb` は既に accumulated color として使う

つまり、

- source は straight alpha 前提寄り
- destination は premultiplied-style 前提寄り

の混成契約になっている。

mask / matte 系では RGB と alpha を同時に減衰する修正が既に入っているが、plane layer のような full-frame source ではこの契約の曖昧さが最も分かりやすく露出する。

### 3. Plane layer が GPU texture cache path を通るため、upload boundary の差分が直接見える

`ArtifactCompositionViewDrawing.cppm` では solid 系レイヤーも GPU texture cache path を使う。

そのため、以下のどこかに小さなズレが残っていると plane layer で即座に再現する。

1. `QImage` -> upload bytes
2. `ImageF32x4_RGBA` -> RGBA8 upload
3. sRGB texture sampling
4. compute blend input/output interpretation

過去の mask investigation で BGRA/RGBA と stride のバグは直っているが、今回の症状は「upload 自体は成立しているが、blend 境界で contract が合っていない」可能性が高い。

### 4. Plane layer は successful dispatch でも「壊れた成功」に見えやすい

現状の fallback は主に `blend failed` を対象にしている。
しかし plane layer のケースでは、dispatch 自体は成功しても結果が視覚的に破綻している可能性がある。

つまり問題は、

- CS が落ちるか

ではなく、

- CS は通るが、`layerSRV -> tempUAV` の結果が契約どおりでない

という種類の不具合として扱うべき。

### 5. Repro 条件によっては「期待結果」と「不具合」が見分けにくい

注意点として、`white Add` が全面・不透明・同サイズで重なると、数学的にはかなり白寄りへ飽和しやすい。
ただし今回の観測は warm/white 系ではなく bluish fill 寄りで、既知の format / accum 問題の系統に近い。

したがって、

1. 「Add だから明るくなる」は正常
2. 「全体が不自然な青寄り単色になる」は別問題

として切り分ける必要がある。

## Recommended Countermeasures

### A. Deterministic two-plane smoke case を固定する

最低でも次の 2 パターンを固定する。

1. lower salmon `Normal` + upper white `Add`
2. lower salmon `Normal` + upper semi-transparent white `Add`

さらに、

3. upper layer を comp 全面ではなく部分矩形にずらしたケース

も持つと、全面飽和と blend 破綻を分けやすい。

### B. Blend / Mask Contract report に plane repro 用の観測値を足す

`FrameDebugSnapshot` / harness report に以下を追加したい。

1. `layerSRV` sampled format
2. `layerSRV` alpha min/max
3. `layerSRV` RGB min/max
4. `accumSRV` RGB/alpha min/max before blend
5. `tempUAV` RGB/alpha min/max after blend
6. `layer coverage` の概算

これで「失敗」ではなく「壊れた成功」を捕まえやすくなる。

### C. Temporary repro mitigation として solid 系だけ GPU texture cache path を外して比較する

恒久対応ではないが、再現切り分けとしては有効。

1. solid 系を一時的に CPU/QImage path へ寄せる
2. 同じ scene で GPU blend ON/OFF を比較する
3. 崩れが solid 系 + GPU cache path でのみ出るか確認する

ここで差が出るなら、blend shader 単体ではなく upload / sampling boundary を先に疑える。

### D. Architectural decision を先送りしない

次のどちらかを明示的に選ぶ必要がある。

1. `layer` も `RGBA32F` に寄せて accum/temp と同一 contract にする
2. `RGBA8_sRGB -> linear float` の境界を明示 pass として固定する

現状の「layer は sRGB、accum は float、shader は混成前提」は長期的に plane / mask / blend の再発点になりやすい。

## Short Conclusion

平面レイヤー同士で GPU 合成 ON が破綻しやすい主因は、plane layer 自体の特別なロジックよりも、

1. GPU texture cache path に乗る
2. 全面・定色・高 alpha なので境界不整合が増幅される
3. `layer=RGBA8_sRGB` と `accum/temp=RGBA32F` の format 差
4. straight/premult の混成 alpha contract

が同時に効くためと考えるのが妥当。

まずは repro を固定し、report text で `layerSRV -> accumSRV -> tempUAV` の min/max と format を可視化するのが次の一手。

## 2026-05-16 Fix Applied

`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
の非 `Normal` compute blend shader を、次の契約へ寄せた。

1. `SrcTex` は straight RGBA として読む
2. `DstTex` は premultiplied accum として読む
3. blend mode の数式は `srcColor` / `dstColor` の straight RGB で計算する
4. 出力時に `ComposeBlend(...)` で premultiplied accum へ戻す

修正前は `srcRGB = src.rgb * srcA` を blend color としても使っており、
Multiply / Screen / Overlay / HSL 系などで straight/premult が混ざっていた。
`Normal` は source-over の形なので成立しやすいが、非 `Normal` では plane layer のような
全面・高 alpha・定色ソースで色崩れが増幅される。

今回の対処では `Add` / `Multiply` / `Screen` / `Overlay` / `Darken` /
`Lighten` / `ColorDodge` / `ColorBurn` / `HardLight` / `SoftLight` /
`Difference` / `Exclusion` / HSL 系 / `LinearBurn` / `Divide` /
`PinLight` / `VividLight` / `LinearLight` / `HardMix` を同じ
straight-to-premult contract に揃えた。

残る確認観点:

1. lower salmon `Normal` + upper white `Add`
2. upper opacity 50% の `Add` / `Screen` / `Multiply`
3. 背景 alpha が 1 未満の composition での premult 出力
4. 将来の `RGBA32F` layer input 統一時に `SrcTex` contract を再確認する
