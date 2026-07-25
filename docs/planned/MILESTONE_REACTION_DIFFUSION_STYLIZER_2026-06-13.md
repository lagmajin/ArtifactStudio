# Milestone: ReactionDiffusionStylizer (反応拡散スタイライザー / 自律絵画化)

> 2026-06-13

## Purpose

`ReactionDiffusionStylizer` は、入力画像の明暗や色相を反応拡散系のパラメータに写し込み、
**自律的に自己組織化する有機パターン** へ画像を再構成する effect である。

狙いは、単なる油絵風やハーフトーンではなく、
**キリン柄、シマウマのストライプ、指紋状の溝、サンゴの迷路模様**
のような自己生成パターンを、入力画像のトーンに応じてリアルタイムに得ることにある。

## Why This Exists

- 既存の cartoon / halftone / mosaic と違い、パターン自体が動的に育つ
- モーションポスターや VJ ループに強い
- 人物やロゴが、うねる有機的パターンへ morph していく表現ができる
- 画像の情報を捨てるのではなく、化学反応として再解釈できる

## Core Idea

この effect は、入力画像の局所情報を次のように扱う。

1. 入力マップ化
   - luminance / hue / saturation を feed / kill / diffusion に変換する
2. 反応拡散更新
   - Gray-Scott 系の反復更新を回す
3. パターン投影
   - 生成された濃度場を画像に重ねる
4. 形状同化
   - 元画像の輪郭や明部に引き寄せる
5. 時間変化
   - 模様が静止せず、わずかに育ち続ける

## Existing Signals

- `ArtifactCore/include/Physics/FluidSolver2D.ixx` に、拡散・渦度・並列更新を持つ solver がある
- `ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx` に、場の見た目を出す既存経路がある
- `ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx` に、方向性を持った局所処理の土台がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` に、画像の変形経路がある
- `ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm` や `ProceduralTexture.cppm` に、パターン生成の雰囲気がある

## Recommended First Slice

### Phase 1: Parameter Mapper

**目標**: 入力画像から反応拡散の制御パラメータを作る。

- luminance を feed に写す
- hue を kill / bias に写す
- saturation を instability に写す
- alpha と premultiplied state を壊さない

### Phase 2: Minimal Gray-Scott Solver

**目標**: 軽量な反応拡散ループを回す。

- U/V の 2 濃度場を持つ
- 低解像度グリッドで計算する
- 反復数は preview / final で切り替える
- 時間方向に少しずつ育てる

### Phase 3: Image Coupling

**目標**: 画像の輪郭や色とパターンを結びつける。

- 明部を pattern amplification に使う
- エッジ周辺でストライプを強める
- 曲率の高い部分を organic growth の起点にする
- 元画像がまだ読める程度に留める

### Phase 4: Stylized Output Modes

**目標**: 代表的な有機模様へ寄せる。

- `giraffe`
- `zebra`
- `fingerprint`
- `coral maze`
- `soft cellular`

### Phase 5: Interactive Evolution

**目標**: 動く演出として成立させる。

- time-dependent feed / kill perturbation
- mouse / motion / boundary からの軽い励起
- ループしても破綻しない
- music video / poster / live visuals に向く

## In Scope

- Gray-Scott 風の反応拡散
- luminance / hue 連動のパラメータマップ
- 低解像度 solver + 高解像度投影
- 有機的な自己組織化パターン
- 時間変化する模様の成長
- CPU reference と GPU path の分離

## Out Of Scope

- 厳密な化学反応の再現
- 高負荷なフル解像度長時間シミュレーション
- 物理化学の完全な妥当性
- 大規模な reaction editor UI
- 画像を完全に破壊するだけの glitch

## Implementation Notes

- 最初は `Rasterizer` として始めるのが安全
- 低解像度 solver を使い、最後に upsample / composite する
- `FluidSolver2D` の拡散・渦度・並列化の考え方を参考にできる
- いきなり複雑にせず、まずは 2-3 種の代表 pattern に絞る

## Success Criteria

- 入力画像から有機的な自己生成模様が出る
- 元画像の輪郭やトーンが残る
- 模様が静止せず、時間で育つ
- `giraffe / zebra / fingerprint / coral` の差が見分けられる
- 既存の halftone や mosaic と役割が被りすぎない

## Likely Touch Points

- [ArtifactCore/include/Physics/FluidSolver2D.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/Physics/FluidSolver2D.ixx)
- [ArtifactCore/src/ImageProcessing/FluidVisualizer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/FluidVisualizer.cppm)
- [ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx)
- [ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- [ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm)

## Related

- [docs/planned/MILESTONE_DYNAMIC_FLUID_VORTEX_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_DYNAMIC_FLUID_VORTEX_2026-06-13.md)
- [docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md)
- [ArtifactCore/include/Physics/FluidSolver2D.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/Physics/FluidSolver2D.ixx)

## 2026-07-25 実装監査

判定: 専用 Stylizer は未実装。近縁の Reaction Diffusion Blur の CPU 実装はあるが、本マイルストーンの入力依存パラメータ化・スタイル出力・時間発展を満たさない。

- ソース上で `ReactionDiffusionStylizer`、Gray-Scott 用の専用 effect、luminance / hue / saturation から feed / kill / instability を作る経路は確認できない。
- 既存の `ReactionDiffusionBlurEffect` は固定 seed と固定の低解像度 U/V 反復を行い、Gaussian blur と混合する blur effect であり、stylizer の用途別出力モードではない。
- `giraffe` / `zebra` / `fingerprint` / `coral maze` / `soft cellular` の preset、輪郭・曲率との coupling、時間をまたぐ interactive evolution、CPU reference と GPU path の分離は未実装。
- `FluidSolver2D`、`AnisotropicFlowBlur`、Noise / Procedural Texture は再利用候補の基盤であり、Stylizer の実装完了を示すものではない。
- 次の実装単位は、入力画像から feed / kill を作る parameter mapper と、低解像度 U/V solver の状態契約を専用 effect として定義すること。

ビルド・実行確認はリポジトリ方針により未実施。
