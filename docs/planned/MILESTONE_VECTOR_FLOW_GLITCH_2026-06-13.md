# Milestone: VectorFlowGlitch (ベクトル・フロー・グリッチ)

> 2026-06-13

## Purpose

`VectorFlowGlitch` は、画像の輪郭や局所的な流れに沿って、ピクセルが
**引きちぎられながら流れていくように見える、方向性のあるデジタル・グリッチ**を作る effect である。

単純な横スライドや RGB ずれではなく、エッジの向き、局所コヒーレンス、時間変化を使って
「壊れ方」に意図を持たせるのが狙い。

## Why This Exists

- 既存のグリッチよりも、形状や動きに追従した知的な見え方にできる
- サイバーパンク、AI UI、電脳空間、HUD 系の演出に合う
- ランダム破壊ではなく、被写体の輪郭に沿ったスライスで印象を作れる
- 既存の `StructureTensor` / `Distortion` / `ChromaSpread` 資産を活かしやすい

## Existing Signals

- `ArtifactCore/src/ImageProcessing/VectorFlowGlitch.cpp` に CPU ベースの実装がすでにある
- `ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx` に公開 settings と effect class がある
- `ArtifactCore/include/ImageProcessing/StructureTensor.ixx` に局所方向場の解析基盤がある
- `ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx` に edge-following の考え方がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` にサンプリング変形の基盤がある
- `ArtifactCore/include/ImageProcessing/ChromaSpread.ixx` に色分離・フリンジの土台がある
- `ArtifactCore/src/Tracking/MotionTracker.cppm` に optical flow 系の将来入力がある

## Core Idea

この effect は次の 3 層で構成する。

1. Flow Analysis
   - structure tensor で edge orientation と coherence を取る
2. Slice Modulation
   - 行方向の帯域ごとに波形ノイズで切れ目を作る
3. Channel Tear
   - RGB を別位相でずらして、崩れたデータ感を出す

## Recommended First Slice

### Phase 1: Edge-Aware Static Glitch

**目標**: まずは静止画で、輪郭追従のグリッチとして成立させる。

- structure tensor の角度に沿って displacement を切り替える
- coherence が高い場所ほど edge-follow を強める
- ブロックノイズではなく、細いスライス帯で破断させる

### Phase 2: RGB Tear Variant

**目標**: 色分離で「データがほどける」印象を強める。

- RGB を別 shift にする
- 破断帯だけ chromatic aberration を強める
- alpha は壊しすぎず、前景の読みやすさを残す

### Phase 3: Motion-Aware Flow

**目標**: 動きに追従するグリッチへ昇格する。

- optical flow または motion vector を入力にする
- 動いている輪郭に沿って tear 方向を変える
- フレーム間で位相を少しずらして、流れている感じを出す

### Phase 4: Stylized Presets

**目標**: 用途別の見た目を分ける。

- `cyberpunk tear`
- `ai fracture`
- `neon slice`
- `signal shear`

## In Scope

- edge-aware displacement
- structure tensor ベースの方向場
- RGB channel tear
- line / band based slicing
- static image path と motion-aware path の両立

## Out Of Scope

- 完全な破損信号の再現
- 本格的な codec glitch / file corruption シミュレーション
- ハードウェア依存の特殊なデコード不良再現
- 低解像度専用の荒い noise のみで済ませる実装

## Implementation Notes

- 既存の `VectorFlowGlitch.cpp` は、輪郭方向に沿った displacement の出発点として使える
- 最初は structure tensor の方向場だけで十分に見栄えがする
- 将来的に motion vector を足すと、単なる静的グリッチから脱しやすい
- 破壊感を上げすぎると読めなくなるので、alpha と前景の保持を優先する

## Success Criteria

- 横ブロックだけの単調なグリッチに見えない
- 輪郭や動きに沿って、引き裂かれる方向に意味がある
- RGB 分離が単なる色ズレではなく、データ崩壊の印象になっている
- サイバー系の UI / タイトル / ロゴに強く使える
- 既存の core image processing 資産を壊さず、effect として再利用できる

## Likely Touch Points

- [ArtifactCore/src/ImageProcessing/VectorFlowGlitch.cpp](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/VectorFlowGlitch.cpp)
- [ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/VectorFlowGlitch.ixx)
- [ArtifactCore/include/ImageProcessing/StructureTensor.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/StructureTensor.ixx)
- [ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- [ArtifactCore/include/ImageProcessing/ChromaSpread.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/ChromaSpread.ixx)
- [ArtifactCore/src/Tracking/MotionTracker.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Tracking/MotionTracker.cppm)

## Related

- [docs/planned/MILESTONE_QUANTUM_GLITCH_WAVEFUNCTION_COLLAPSE_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_QUANTUM_GLITCH_WAVEFUNCTION_COLLAPSE_2026-06-13.md)
- [docs/planned/MILESTONE_DYNAMIC_FLUID_VORTEX_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_DYNAMIC_FLUID_VORTEX_2026-06-13.md)
