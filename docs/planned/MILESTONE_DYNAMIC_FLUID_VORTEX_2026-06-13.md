# Milestone: DynamicFluidVortex (流体渦動ベクトル変形)

> 2026-06-13

## Purpose

`DynamicFluidVortex` は、流体の速度場・渦度・密度を使って、画像や別レイヤーの映像を
**インクが水に溶けるように引きずり、渦を巻かせ、動的に歪ませる** effect である。

静的な displacement map ではなく、内部で流体 solver を回しながら、
被写体の動き、マウス入力、境界条件から力を発生させて、背景をリアルタイムに掻き混ぜる。

## Why This Exists

- 既存の displacement / warp より「動いている理由」が見える
- 文字、ロゴ、図形の軌跡に沿って背景が流れると、映像として強い
- water / ink / smoke / gel のような液体的な演出に向く
- `FluidVisualizer` と `FluidSolver2D` の既存資産をそのまま活かしやすい

## Existing Signals

- `ArtifactCore/include/Physics/FluidSolver2D.ixx` に 2D stable fluids 系の solver がある
- `ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx` に velocity / density を見せるビジュアライザがある
- `ArtifactCore/include/Particle/FluidForce.ixx` に fluid を force field として使う土台がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` に displacement 系の画像変形基盤がある
- `Artifact/App/shaders/ShaderInterop_Weather.h` に流体・渦度系のシェーダ周辺パラメータがある

## Core Idea

この effect は次の 3 層で構成する。

1. Flow Field
   - density / velocity / curl を保持する
2. Force Injection
   - 動き、カーソル、境界、発火点から力を足す
3. Image Advection
   - 背後の画像を velocity field に沿って bilinear sample で移流させる

## Recommended First Slice

### Phase 1: Fluid Field Viewer

**目標**: solver の状態を見える化する。

- velocity vector の可視化
- density の heatmap 表示
- curl の強い場所を渦として見せる
- デバッグ用の overlay を用意する

### Phase 2: Advection Warp

**目標**: 画像を流体場で引きずる。

- 逆向きサンプリングで移流させる
- bilinear interpolation を使う
- velocity が強い場所ほど歪みを大きくする
- alpha と premultiplied state を壊さない

### Phase 3: Vortex Injection

**目標**: 流体に渦を入れて、能動的に動かす。

- マウスドラッグで force を注入する
- moving object の軌跡を source として使う
- 画面端やマスク境界で swirl を作る
- 文字 / ロゴの輪郭からも flow を発生させる

### Phase 4: Stylized Ink / Water Modes

**目標**: 見た目を用途別に分ける。

- `ink diffusion`
- `water vortex`
- `smoke swirl`
- `gel stretch`

### Phase 5: Presentation / Integration

**目標**: 実運用で使える入口にする。

- effect stack から呼べるようにする
- preview でも render でも同じ見え方に寄せる
- strength / viscosity / diffusion / vorticity の UI を揃える

## In Scope

- 2D stable fluids ベースの solver
- velocity / density / curl の可視化
- bilinear advection
- mouse / motion / boundary force injection
- ink / water / smoke 的な stylized warp
- CPU reference と GPU path の段階導入

## Out Of Scope

- 完全な Navier-Stokes の厳密再現
- 3D fluid simulation
- 超高解像度での常時フル精度 solver
- 物理的に正確な粘性・表面張力の完全実装
- 大規模な fluid editor UI

## Implementation Notes

- 最初は `FluidSolver2D` をそのまま使うのが安全
- まずは `FluidVisualizer` で見える化し、次に image advection を足す
- 動きベースの入力は `Particle` / `Layer motion` / `cursor` のいずれかから注入できる
- 重いので、最初から全画面高解像度で回さず、preview 用の低解像度経路を持つ

## Success Criteria

- 背景が「流れている理由」を持って見える
- 文字やロゴの軌跡に沿って液体的な歪みが出る
- 渦の発生と収束が視覚的に分かる
- 静的 displacement とは違う、動的な映像になっている
- 既存の fluid solver を壊さず effect 化できる

## Likely Touch Points

- [ArtifactCore/include/Physics/FluidSolver2D.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/Physics/FluidSolver2D.ixx)
- [ArtifactCore/src/ImageProcessing/FluidVisualizer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/ImageProcessing/FluidVisualizer.cppm)
- [ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx)
- [ArtifactCore/include/Particle/FluidForce.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/Particle/FluidForce.ixx)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)
- [Artifact/App/shaders/ShaderInterop_Weather.h](X:/Dev/ArtifactStudio/Artifact/App/shaders/ShaderInterop_Weather.h)

## Related

- [docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md)
- [docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md)
