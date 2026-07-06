# Milestone: Glow Variants Pack (発光亜種の実験群)

> 2026-06-13

Status: ✅ Complete (All five variants implemented; static audit completed 2026-06-28)
Build and runtime visual verification remain pending under the repository
execution policy.

Current implementation:

- `EdgeBloomEffect`
- `ReactiveGlowEffect`
- `ChromaticGlowEffect`
- `LiquidGlowEffect`
- `ResidualGlowEffect`
- Effect Service restoration, effect listing, and Inspector creation entries

## Purpose

`Glow Variants Pack` は、単純な「光らせる」だけではなく、
**光の出方・広がり方・色の割れ方・残光の残り方** を変える発光系 effect 群である。

共通テーマは glow だが、用途ごとに性格を変える。
ロゴ、UI、ネオン、空気感、オーラ、ヴィンテージレンズ感までを一つの系統として扱う。

## Why This Exists

- `Glow` だけでは発光の表情が単調になりやすい
- 発光の「方向」「粒度」「残光」「色収差」を分けると用途が広がる
- 既存の glow / bloom / halation / chroma spread 資産を整理しやすい
- 効果名と役割を分離すると、UI とプリセット設計がしやすい

## Existing Signals

- `Artifact/src/Effects/Glow/GlowEffect.cpp` に標準的な glow effect がある
- `Artifact/src/Effects/DirectionalGlowEffect.cppm` に方向性のある streak 系がある
- `ArtifactCore/include/ImageProcessing/ChromaSpreadGlow.ixx` と `src/ImageProcessing/ChromaSpreadGlow.cpp` に色散り込み型の glow がある
- `ArtifactCore/include/ImageProcessing/Halation.ixx` にフィルム的な滲みと残光がある
- `ArtifactCore/src/ImageProcessing/OpenCV/Glow.cppm` に複数の OpenCV glow path がある
- `ArtifactCore/src/ImageProcessing/OpenCV/SpectralGlowCV.cppm` にスペクトル寄りの glow 実装がある
- `Artifact\App\shaders\bloomseparateCS.hlsl` に bloom 分離系のシェーダ資産がある

## Core Idea

この pack は、次の 5 系統で構成する。

1. EdgeBloom
   - 輪郭だけが先に発光して内側へ滲む
2. ReactiveGlow
   - エッジ、色、動きに反応して発光の強さが変わる
3. ChromaticGlow
   - 発光の縁に色収差やスペクトル分離を乗せる
4. LiquidGlow
   - 光が液体のように流れ、固体っぽさを失う
5. ResidualGlow
   - 発光の残り香が時間方向に積層する

## Recommended First Slice

### Phase 1: EdgeBloom Base

**目標**: 輪郭優先の発光を安定させる。

- エッジ検出で glow を強める
- 輪郭から内側へにじませる
- 文字やロゴが潰れないようにする

### Phase 2: Reactive Variants

**目標**: 入力に反応して glow の性格を変える。

- 明るさで発光強度を変える
- 色相や彩度でにじみの色を変える
- 動きのある部分だけ発光を強める

### Phase 3: Chromatic / Spectral Modes

**目標**: 色の割れ方を発光の表情にする。

- chromatic aberration を縁にだけ入れる
- スペクトル分離で虹色の滲みを作る
- ネオン寄り / SF 寄りの見た目を用意する

### Phase 4: Liquid / Residual Modes

**目標**: 物質感と時間感を足す。

- 光が液体のように流れる
- いったん光った場所に余韻が残る
- 連続フレームで残光が積み上がる

## Candidate Variants

- `EdgeBloomGlow`
- `ReactiveGlow`
- `ChromaticGlow`
- `LiquidGlow`
- `ResidualGlow`

## In Scope

- edge-aware glow
- chromatic / spectral glow
- residual / temporal glow
- directional streak / halo variation
- glow を用途別に分割するプリセット整理

## Out Of Scope

- 単一の固定 bloom だけで全部を済ませること
- 物理厳密なレンズシミュレーションの完全再現
- glow と blur の区別が付かない実装
- 既存 glow 系を上書きしてしまう大規模な UI 変更

## Implementation Notes

- 既存の `Glow` と `DirectionalGlow` はベースラインとして維持する
- `ChromaSpreadGlow` と `Halation` は色と残光の派生として扱いやすい
- 実装はひとまとめでも、UI 上は用途別プリセットに分けた方が使いやすい
- `EdgeBloom` と `ReactiveGlow` を最初の実験対象にすると、差分が分かりやすい

## Success Criteria

- 単なる「明るくする」だけではない glow の表情が出る
- 文字、ロゴ、UI、人物、それぞれに合う亜種がある
- ネオン系、映画系、オーラ系、レンズ系を分けて扱える
- 既存の glow 資産を活かしながら、表現幅が増える

## Likely Touch Points

- [Artifact/src/Effects/Glow/GlowEffect.cpp](X:/Dev/ArtifactStudio/Artifact/src/Effects/Glow/GlowEffect.cpp)
- [Artifact/src/Effects/DirectionalGlowEffect.cppm](X:/Dev/ArtifactStudio/Artifact/src/Effects/DirectionalGlowEffect.cppm)
- [ArtifactCore/include/ImageProcessing/ChromaSpreadGlow.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/ChromaSpreadGlow.ixx)
- [ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cpp](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cpp)
- [ArtifactCore/include/ImageProcessing/Halation.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Halation.ixx)
- [ArtifactCore/src/ImageProcessing/OpenCV/Glow.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/OpenCV/Glow.cppm)
- [ArtifactCore/src/ImageProcessing/OpenCV/SpectralGlowCV.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/OpenCV/SpectralGlowCV.cppm)
- [Artifact/App/shaders/bloomseparateCS.hlsl](X:/Dev/ArtifactStudio/Artifact/App/shaders/bloomseparateCS.hlsl)

## Related

- [docs/planned/MILESTONE_LUMINESCENCE_CAUSTICS_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LUMINESCENCE_CAUSTICS_2026-06-13.md)
- [docs/planned/MILESTONE_APERTURE_SHAPE_BLUR_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APERTURE_SHAPE_BLUR_2026-06-13.md)
