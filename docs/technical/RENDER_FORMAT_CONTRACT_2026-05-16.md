# Render Format Contract

**Date:** 2026-05-16  
**Status:** Proposed canonical contract for internal composition rendering

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

### 2. Canonical Channel Order

内部 canonical order は:

- `R, G, B, A`

とする。

クラス名や API 名に `RGBA` を含むものは、原則として実メモリも RGBA を意味するべき。
BGRA を内部 canonical として維持しない。

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
2. convert to canonical `RGBA32F linear premultiplied`
3. layer-local mask/matte/effect
4. blend into `accum`
5. final presentation conversion only at screen/output boundary

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

`ImageF32x4_RGBA` のような内部 float image は、

1. 実データも RGBA order
2. linear
3. premultiplied

へ寄せる。

現状の BGRA 依存は移行対象とする。

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

1. `ImageF32x4_RGBA` の実体を true RGBA に寄せる
2. BGRA 前提の call site を削減
3. upload boundary のみで必要な変換を行う

### Phase 5: Boundary Cleanup

1. hot path での `QImage` 往復を削減
2. mask/matte/effect を canonical image path に寄せる
3. export / UI preview だけで 8-bit / sRGB を扱う

## Guardrails

新しい render code を足すときは、次を守る。

1. 内部中間バッファに `RGBA8_sRGB` を増やさない
2. canonical 以外の channel order を内部標準にしない
3. alpha model を call site ごとに変えない
4. `QImage` を hot path の canonical container にしない
5. BGRA/RGBA の再解釈で問題を局所修正し続けない

## Short Decision

ArtifactStudio の内部合成は、最終的に

- `RGBA32F`
- `linear`
- `premultiplied alpha`
- `RGBA channel order`

へ統一する。

`QImage` / OpenCV / GPU backend ごとの差異は、内部標準ではなく boundary conversion の問題として扱う。
