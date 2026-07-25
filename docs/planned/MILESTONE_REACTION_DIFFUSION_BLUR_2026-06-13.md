# Milestone: ReactionDiffusionBlur (自己組織化・細胞分裂ブラー)

> 2026-06-13

## Purpose

`ReactionDiffusionBlur` は、拡散によってボケる過程に反応拡散の自己組織化を重ねた、
**模様を育てながら広がるアート系ブラー**である。

単に平滑化して情報を失わせるのではなく、ボケた成分が自己干渉して
縞、ドット、波紋、細胞分裂のようなパターンを生み出しながら、画面全体へ拡散していく。

## Why This Exists

- 一般的な blur よりも、時間とともに見た目が育つ
- トランジションで使うと、前の画面が溶けながら次の画面へ変質していく
- サイケデリック、バイオ、実験映像の文脈に強い
- 反応拡散を「模様生成」だけでなく「ブラーの運動」へ拡張できる

## Existing Signals

- `ArtifactCore/include/Physics/FluidSolver2D.ixx` に低解像度格子系の反復計算の土台がある
- `ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx` に方向性のある局所サンプリングの土台がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` に画像変形の基盤がある
- `ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm` にパターンの初期化や乱流の雰囲気がある
- `ArtifactCore/src/ImageProcessing/ProceduralTexture.cppm` に有機的なマテリアル生成の雰囲気がある
- `docs/planned/MILESTONE_REACTION_DIFFUSION_STYLIZER_2026-06-13.md` に近縁の反応拡散系マイルストーンがある

## Core Idea

この effect は次の 3 層で構成する。

1. Diffusive Blur
   - まずは通常のブラーとして拡散させる
2. Reaction Coupling
   - 拡散場に Turing pattern の増幅を混ぜる
3. Organic Materialization
   - ボケた領域に縞・網点・波紋を生成し、質感を作る

## Recommended First Slice

### Phase 1: Diffusion Base

**目標**: まずは安定した拡散ブラーとして成立させる。

- 低解像度格子で拡散を回す
- 拡散率を一定にせず、局所濃度で少し揺らす
- alpha と前景の可読性を壊しすぎない

### Phase 2: Reaction Coupling

**目標**: 拡散の途中で自己組織化が起きるようにする。

- feed / kill 相当の係数を導入する
- 空間周波数の一部を自己増幅させる
- 縞、ドット、波紋が自然に出るようにする

### Phase 3: Transition Variant

**目標**: 画面切り替え用途で強く見せる。

- 前フレームを細胞状に溶かす
- 次フレームへの遷移を有機的に見せる
- 画面全体の位相がゆっくり崩れていく印象を作る

### Phase 4: Stylized Presets

**目標**: 用途別に見た目を分ける。

- `cell split`
- `ripple dissolve`
- `psychedelic melt`
- `bio blur`

## In Scope

- reaction diffusion coupling
- diffusion blur with self-amplifying spatial frequencies
- organic line / dot / ripple patterns
- transition-oriented dissolve
- low-resolution iterative simulation

## Out Of Scope

- 厳密な物理ブラーの再現
- 大規模な高解像度 PDE solver の常時運用
- 単純な平均化だけの blur
- 反応拡散要素を持たない一般的な soft blur

## Implementation Notes

- `ReactionDiffusionStylizer` と共通する考え方を持つが、こちらは「ブラーの中で模様が育つ」方向に寄せる
- 最初は低解像度の simulation を拡大合成する方が安全
- 不安定になりやすいので、preview 用と final 用で反復回数を分ける
- 画面遷移に使う場合は、前景の輪郭を完全に壊さない制御が重要

## Success Criteria

- ただぼけるだけでなく、パターンが生成される
- 縞、網点、波紋、細胞分裂のような質感が見える
- トランジションで前画面が「溶けて変質する」感じが出る
- 反応拡散スタイライザーとは違う、ブラー起点の表現になっている
- 既存の core image processing 資産を壊さずに拡張できる

## Likely Touch Points

- [ArtifactCore/include/Physics/FluidSolver2D.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/Physics/FluidSolver2D.ixx)
- [ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/AnisotropicFlowBlur.ixx)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- [ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm)
- [ArtifactCore/src/ImageProcessing/ProceduralTexture.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/ProceduralTexture.cppm)

## Related

- [docs/planned/MILESTONE_REACTION_DIFFUSION_STYLIZER_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_REACTION_DIFFUSION_STYLIZER_2026-06-13.md)
- [docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md)

## 2026-07-25 実装監査

判定: Phase 1〜2 の CPU Rasterizer 基盤は実装済み。Phase 3〜4、時間状態の保持、preview/final 分離は未確認・未接続。

- `ReactionDiffusionBlurEffect` は Gaussian blur の後に低解像度の U/V 格子を作り、feed / kill、Laplacian、反復更新、patternStrength で画像へ混合する Gray-Scott 風の CPU 処理を持つ。
- Blur Radius / Feed / Kill / Pattern Strength / Iterations / Evolution は effect の編集プロパティとして公開され、Rasterizer effect として登録されている。
- ただし各適用で seed と格子を作り直しており、フレームをまたいで模様を育てる persistent state や transition の前フレーム→次フレーム経路は確認できない。
- GPU solver、preview / final の反復数切替、cell split / ripple dissolve 等のプリセット、深い専用 transition UI は未実装。
- 次の実装単位は、まず時間状態の所有方法と preview / final の反復契約を定義し、既存 CPU 経路を transition へ接続すること。

ビルド・実行確認はリポジトリ方針により未実施。

### 追加静的確認

`ArtifactInspectorWidget.cppm` の effect catalog、`ArtifactEffectService.cppm` の factory／catalog、`ReactionDiffusionBlurEffect.ixx`／実装の三者が `reaction_diffusion_blur` を共有しているため、Rasterizer effect としての登録・生成導線はソース上確認できる。未完了なのは、登録ではなく persistent な時間状態、transition 経路、GPU化、preview／final 契約、プリセットである。
