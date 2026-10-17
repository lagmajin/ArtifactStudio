# Render Format Contract

**Date:** 2026-05-16  
**Status:** Proposed canonical contract for internal composition rendering

**最終更新:** 2026-10-16

Channel orderの現行方針は [IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md](IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md) を参照する。

## Goal

ArtifactStudio をプロ向けの合成系として安定させるため、内部 render path の

1. pixel format
2. channel order
3. alpha contract
4. color space
5. boundary conversion

を 1 つに固定する。

この文書は「現状説明」ではなく、今後寄せていく **canonical contract** を定義する。

## Why This Matters

現状は次が混在しやすい。

1. `QImage` の BGRA 系 memory layout
2. OpenCV の BGR / BGRA convention
3. GPU texture の RGBA expectation
4. `layer=RGBA8_sRGB` と `accum/temp=RGBA32F` の format mismatch
5. source 側 straight alpha と destination 側 premultiplied-style accumulation の混成

この状態では、

- mask
- matte
- plane layer
- non-Normal blend
- HDR 寄りの加算

で不具合が再発しやすい。

## Canonical Internal Contract

### 1. Canonical Pixel Format

内部合成・中間バッファ・blend/effect/mask/matte の canonical format は:

- `RGBA32F`
- linear color

とする。

対象:

1. composition layer intermediate
2. `accum`
3. `temp`
4. blend input/output
5. mask / matte resolved surface
6. effect chain intermediate

### 2. Canonical CPU Image Channel Order

内部CPU画像surface (`ImageF32x4_RGBA`, `ImageF16x4`) の物理配列順は:

- `B, G, R, A` (BGRA)

とする。Float32/Float16で共通とし、descriptorにもBGRAを記録する。

`ImageF32x4_RGBA` の `RGBA` は論理色/API互換名であり、生ポインタの物理順序を意味しない。
`rgba32fData()` / `rgba8Data()` はBGRA backingを返す。raw channel indexを論理RGBAとして扱わず、
descriptor-aware viewを使う。

Artifact image-pipelineのGPU float textureは、Diligentの`RGBA16_FLOAT` / `RGBA32_FLOAT` resourceを使いながら、
各pixel値を`[B,G,R,A]`として扱う。floatのBGRA resource formatはDiligentにないため、resource format名と
Artifact内の値順を区別する。BGRA CPU backingからは画素全体のR/B並べ替えをせずにuploadし、shaderも同じ値順を
維持する。RGBAを要求する外部shader/APIや最終表示境界だけで明示的に変換する。

### 3. Canonical Alpha Model

内部合成 canonical は:

- **premultiplied alpha**

とする。

理由:

1. layer blend と mask/matte の意味を揃えやすい
2. RGB だけ残る系の破綻を減らしやすい
3. 加算・スクリーン・オーバーレイなどを accumulated color として扱いやすい
4. edge fringe と transparent RGB garbage を抑えやすい

補足:

- external input は straight alpha でもよい
- ただし内部 canonical へ入る境界で premultiply する
- internal hot path では straight/premult を混在させない

### 4. Canonical Color Space

内部 canonical は:

- **linear**

とする。

補足:

1. `sRGB` は decode / display boundary でのみ扱う
2. additive / glow / bloom / exposure 的な処理は linear 前提で行う
3. blend shader は linear float source/destination を前提にする

## Boundary Rules

### Allowed Boundaries

変換してよい境界は次だけに絞る。

1. asset decode boundary
2. UI / Qt interop boundary
3. final presentation boundary
4. export encode boundary

### Disallowed Pattern

内部合成途中で次を暗黙変換しない。

1. `QImage` 化
2. CPU download
3. GPU upload
4. `RGBA8_sRGB` への一時降格
5. BGRA/RGBA reorder の場当たり対応

## Concrete Target State

### Composition Path

理想の composition path:

1. source decode
2. normalize CPU Float32/Float16 surfaces to `BGRA32F/16 linear premultiplied`
3. preserve BGRA value order in Artifact image-pipeline GPU float textures
4. layer-local mask/matte/effect
5. blend into `accum`
6. read back BGRA-native image textures directly to CPU BGRA; convert only at external RGBA/presentation boundaries

### GPU Resources

最低限、次は format を一致させる。

1. `layerRTV`
2. `accum`
3. `temp`
4. `OutTex`

推奨:

- 全て `RGBA32F`

これにより、`layer=RGBA8_sRGB` と `accum/temp=RGBA32F` の境界不整合を消す。

### CPU-side Image Type

`ImageF32x4_RGBA` / `ImageF16x4` の内部float imageは、

1. 実データは BGRA order
2. linear
3. premultiplied

へ統一する。

## Transition Plan

### Phase 1: Contract First

1. この文書を canonical contract として固定する
2. `IMAGE_FORMAT_CONVENTIONS` は現状説明として残す
3. `BLEND_MASK_COMPOSITION_CONTRACT` と `RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST` から参照する

### Phase 2: Observability

frame snapshot / harness で次を見えるようにする。

1. resource format
2. channel order assumption
3. alpha model assumption
4. `layerSRV/accumSRV/tempUAV` の RGB/alpha min-max

### Phase 3: Render Resource Unification

1. `layerRTV` を `RGBA32F` へ移行
2. blend shader input/output を同一 format で固定
3. direct fallback path も同じ contract に寄せる

### Phase 4: CPU Image Contract Unification

1. `ImageF32x4_RGBA` と `ImageF16x4` のbackingをdescriptor付きBGRAへ統一する
2. true-RGBA前提のCPU call siteをdescriptor-aware viewへ移す
3. Artifact image-pipelineのGPU float textureもBGRA値順へ移行し、外部RGBA APIとの境界だけで変換する

### Phase 5: Boundary Cleanup

1. hot path での `QImage` 往復を削減
2. mask/matte/effect を canonical image path に寄せる
3. export / UI preview だけで 8-bit / sRGB を扱う

## Guardrails

新しい render code を足すときは、次を守る。

1. 内部中間バッファに `RGBA8_sRGB` を増やさない
2. CPU Float32/Float16 backingとArtifact image-pipeline GPU float値をBGRAへ統一する。resource format名はRGBAでも、値順はBGRAとする
3. alpha model を call site ごとに変えない
4. `QImage` を hot path の canonical container にしない
5. BGRA/RGBA の再解釈で問題を局所修正し続けない

## Short Decision

ArtifactStudio のCPU内部画像は、最終的に

- `RGBA32F` / `RGBA16F`
- `BGRA channel order`
- `linear`
- `premultiplied alpha`

へ統一する。GPU float textureにはnative BGRA formatがないため、RGBA16F/32F resourceの値をBGRA順で扱う。
RGBAを要求する外部APIと画面出力境界でだけ明示変換する。

OpenCVの4ch配列はproducer-definedだが、本アプリ所有のCPU image/OpenCV processing面はBGRAへ統一する。
