# MILESTONE: Physics/Simulation Production Hardening

**日付**: 2026-08-04
**現状**: 8 ソルバーすべてプロトタイプ品質（平均 45%）。MPM と SoftBody は `PhysicsSystem` 経由で layer 統合済み。他は孤立。
**目標**: MPM + SoftBody を最初の production-ready ソルバーにし、Boids + Fluid + Pyro を後続で GPU 加速 + レンダリング統合する。

## 現状サマリ

| ソルバー | 行数 | CPU/GPU | Layer統合 | 品質 |
|---------|------|---------|-----------|------|
| **MPM** | 1,100 | CPU only | ✅ PhysicsSystem | 最高品質・APIC+Corotated+Plasticity+Fracture |
| **SoftBody** | 730 | CPU only | ✅ PhysicsSystem | Verlet+PBD・Tearing・Remeshing |
| **Pyro** | 1,270 | CPU (TBB一部) | ❌ 独立 | 3D Gas・Combustion・FrameCache |
| **Boids** | 1,340 | CPU+GPU(別) | ❌ 孤立 | 全機能揃っているが layer 未接続 |
| **Fluid** | 350 | CPU only | ✅ PhysicsSystem | Stable Fluids・render path 不在 |
| **Sand** | 400 | CPU+GPU(別) | ❌ 孤立 | Cellular Automaton |
| **Physics2D** | 470 | CPU (Box2D) | ✅ PhysicsSystem | friction/restitution broken |
| **Fracture** | 240 | CPU | ✅ Geometry.Fracture | 2D Polygon only |

---

## Phase 1: MPM 2D Production Hardening

### 1.1 GPU Compute バックエンド

`ArtifactCore/src/Physics/MpmSolver2D.cppm` を GPU 対応化。現行の CPU `stepOnce()` と並行して GPU パスを追加する。

```cpp
// 追加する enum
enum class MpmBackend { CPU, GPU };

// MpmSolver2D に追加
void stepOnce(float dt);                    // 既存 CPU
void stepOnceGPU(float dt, Diligent::IDeviceContext* ctx);  // 新規 GPU
void setBackend(MpmBackend backend);
MpmBackend backend() const;
```

GPU パス実装ステップ:

1. **P2G (Particle to Grid)**: HLSL compute shader。粒子→グリッドノードへの質量・運動量転送。17点（または9点）quadratic B-spline kernel。既存 CPU の `p2gKernel()` を参照実装とする。

2. **Grid Solve**: グリッド速度の更新（外力適用）、pressure solve（Jacobi 反復）、境界条件。既存 `computeGridForces()` + `updateGridVelocities()` + `applyBoundaryConditions()` を HLSL 化。

3. **G2P (Grid to Particle)**: グリッド→粒子への速度・変形勾配更新。既存 `g2pKernel()` を HLSL 化。

4. **Plasticity + Fracture**: `applyPlasticity()` と `checkFracture()` は粒子ごとの分岐が多いため、まず CPU 側で実行し、GPU→CPU readback 後に処理。後の反復で GPU 化。

5. **Buffer Management**: `Diligent::IBuffer` で particle positions, velocities, deformation gradients, masses を保持。グリッドは `RWTexture2D<float4>` で実装。

### 1.2 Render Pipeline 統合

MPM 粒子を描画可能にする。

1. **ParticleRenderData 出力**: `captureRenderData()` メソッドを追加。現在位置・速度・応力テンソルを `std::vector<ParticleRenderPoint>` に出力。

```cpp
struct ParticleRenderPoint {
    float x, y;
    float vx, vy;        // 速度（色マッピング用）
    float stress;         // フォンミーゼス応力（可視化用）
    uint32_t materialId;  // マテリアル種別
};
```

2. **GPUParticleRenderer**: `ArtifactRenderer` に点群レンダリングパスを追加。`Diligent::IBuffer` → vertex shader → point sprite。色は velocity magnitude または stress で tint。

3. **Composition View 連携**: `ArtifactCompositionEditor` が MPM layer を検出し、`GPUParticleRenderer` 経由で粒子を描画する経路を確立。

### 1.3 Layer UX

1. **MPM Layer 作成ダイアログ**: `Layer → New → Physics → MPM 2D`。マテリアル選択（Flesh/Foam/HardRubber/Wood プリセット）、解像度、サブステップ数、境界タイプ。

2. **Property Widget**: `MpmSolver2D` のパラメータ（Young's modulus, Poisson ratio, plasticity yield, hardening, fracture strain）を Inspector に露出。

3. **Collider 設定**: 既存の `resolveColliders()` が `PhysicsSystem` 経由で Box2D コライダーを受け取る。これを Property Editor で設定可能にする。

### 1.4 完了条件

- [ ] GPU パスが CPU パスと同等の結果を出す（visual comparison）
- [ ] 粒子が Composition View に表示される
- [ ] Layer 作成 → パラメータ編集 → シミュレーション実行 → 結果表示 の完全なワークフローが動作
- [ ] MPM がサブステップ 64 でリアルタイム（≥30fps @ 10K particles）動作

---

## Phase 2: SoftBody Production Hardening

### 2.1 固定ステップシミュレーションクロック

`SoftBodySolver` は現在 `dt` を外部から受け取るが、フレームレート非依存の固定ステップに移行する。

```cpp
void setFixedTimeStep(float dt);      // 例: 1/60 = 0.0167
void setMaxSubSteps(int maxSteps);    // 例: 4（フレームスキップ時に複数回実行）
void step(float frameDt);             // 固定ステップで subdivide して実行
```

これにより、composition のフレームレートが 24/30/60fps いずれでも同じ物理結果が得られる。

### 2.2 Frame Cache / Seeking 対応

既存の `captureSnapshot()` / `restoreSnapshot()` を拡張し、フレームキャッシュを実装する。

```cpp
struct SoftBodySnapshot {
    int64_t frameIndex;
    std::vector<Vec2> positions;
    std::vector<Vec2> prevPositions;
    std::vector<float> accumulatedStrain;
};
```

`PhysicsSystem` が管理する ring buffer（現行 480 フレーム）に SoftBody のスナップショットを追加。`seekToFrame()` で任意フレームにジャンプ可能にする。

### 2.3 GPU Deformation Grid

マイルストーン `MILESTONE_PROFESSIONAL_SOFT_BODY_2026-07-11.md` の P5 を実装。

`SoftBodySolver` の格子点を GPU テクスチャとして出力し、image/video layer のテクスチャを UV 変形させる compute shader を作成。

```hlsl
// SoftBodyDeformCS.hlsl
Texture2D<float4> sourceTexture;
StructuredBuffer<float2> nodePositions;
StructuredBuffer<int3> meshTriangles;
RWTexture2D<float4> outputTexture;

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    float2 uv = float2(tid.xy) / float2(outputWidth, outputHeight);
    // barycentric → 変形後UV → sourceTexture sample
    float2 deformedUV = computeDeformedUV(uv, nodePositions, meshTriangles);
    outputTexture[tid.xy] = sourceTexture.SampleLevel(sampler, deformedUV, 0);
}
```

### 2.4 自己衝突の最適化

現行の `resolveSelfCollision()` は O(constraints × points) の brute force。空間ハッシュグリッドを導入する。

```cpp
struct SpatialHashGrid {
    float cellSize;
    std::unordered_map<uint64_t, std::vector<int>> cells;
    
    void build(const std::vector<Vec2>& points);
    std::vector<int> query(const Vec2& point, float radius) const;
};
```

### 2.5 完了条件

- [ ] 固定ステップシミュレーションが全フレームレートで一貫した結果を出す
- [ ] フレームキャッシュ + シークが動作する
- [ ] GPU deformation grid が image layer を変形させる
- [ ] 自己衝突の空間ハッシュグリッドが O(N) で動作

---

## Phase 3: Fluid + Boids レイヤー統合

### 3.1 FluidSolver2D GPU + レンダリング

1. **GPU Pressure Solve**: Jacobi → Conjugate Gradient に移行し、HLSL compute shader で実装
2. **密度フィールドのテクスチャ出力**: `RWTexture2D<float4>` に密度場を書き込み、composition compositor が読み取れるようにする
3. **Layer 統合**: `FluidComponent` を `ILayerComponent` として実装。`PhysicsSystem` 経由で既に統合済みだが、専用の Property Widget と Layer 作成ダイアログを追加

### 3.2 Boids Layer 統合

1. **BoidsLayerComponent**: `ILayerComponent` 実装。`BoidsSwarmSystem` を所有し、`update()` を呼び出す
2. **CPU/GPU 経路統合**: 現在別クラス（`BoidsSwarmSystem` vs `BoidsGPUCompute`）の経路を統一。`BoidsComputeInterface` 抽象クラスを作成し、CPU/GPU 実装を `setComputeBackend()` で切り替え
3. **ParticleRenderData → GPUParticleRenderer**: Phase 1.2 で作成した `GPUParticleRenderer` にエージェント位置を供給
4. **Boids Layer 作成ダイアログ**: エージェント数、分離/整列/凝集の重み、捕食者/獲物設定、フォーメーション、フローフィールド

### 3.3 完了条件

- [ ] Fluid layer 作成 → 密度場表示 → レンダリング出力 が動作
- [ ] Boids layer 作成 → 群れシミュレーション → Composition View 表示 が動作
- [ ] 両方とも GPU バックエンドでリアルタイム動作（≥30fps）

---

## Phase 4: Pyro + Sand Tooling

### 4.1 PyroSimulation GPU バックエンド

1. `PyroBackendKind::GPUCompute` enum は存在するが実装がない。HLSL compute shader で semi-Lagrangian advection、Jacobi pressure solve、combustion を実装
2. Volume Renderer: 3D density/temperature フィールドを `Texture3D<float>` に出力し、レイマーチング volume shader で描画

### 4.2 SandSim2D Editor Tool

1. **Sand Layer 作成**: `PhysicsSystem` に `SandSolver2D` を統合（現在未統合）
2. **Paint Tool**: Composition View 上で砂/水/石/火/煙/酸をペイントするブラシツール
3. **GPU パス統合**: `SandGPUCompute` を `SandSolver2D` のバックエンドとして統合（現在別クラス）

---

## Phase 5: テスト・検証

### 5.1 物理ユニットテスト（新規）

現在ゼロのテストを追加:
- `FluidSolver2D` — density/velocity の質量保存、非圧縮性
- `MpmSolver2D` — 重力落下の位置誤差 < 1%
- `SoftBodySolver` — 距離制約の収束、tearing threshold
- `BoidsSwarmSystem` — 分離/整列/凝集ベクトルの符号

### 5.2 回帰テスト

- [ ] 全ソルバーが 1000 step のシミュレーション後も安定（発散しない）
- [ ] 固定シードで決定論的な結果（CPU パス）
- [ ] GPU パスが CPU パスと誤差 1% 以内

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/src/Physics/MpmSolver2D.cppm` | GPU backend 追加 |
| P1 | `ArtifactCore/include/Physics/MpmSolver2D.ixx` | `MpmBackend` enum, GPU メソッド宣言 |
| P1 | 新規 `ArtifactCore/src/Graphics/MpmCompute.hlsl` | P2G/Grid/G2P compute shaders |
| P1 | 新規 `ArtifactRenderer/src/GPUParticleRenderer.*` | 点群レンダリングパス |
| P1 | 新規 `Artifact/src/Widgets/Physics/MpmLayerDialog.*` | MPM Layer 作成 UI |
| P2 | `ArtifactCore/src/Physics/SoftBodySolver.cppm` | 固定ステップ、frame cache、spatial hash grid |
| P2 | 新規 `ArtifactCore/src/Graphics/SoftBodyDeformCS.hlsl` | テクスチャ変形 compute shader |
| P3 | `ArtifactCore/src/Physics/FluidSolver2D.cppm` | GPU pressure solve, texture output |
| P3 | `ArtifactCore/src/Crowd/BoidsSwarmSystem.ixx` | `BoidsComputeInterface` 抽象化 |
| P3 | 新規 `Artifact/src/Widgets/Physics/BoidsLayerDialog.*` | Boids Layer 作成 UI |
| P4 | `ArtifactCore/src/Simulation/PyroSimulation.cppm` | GPU backend 実装 |
| P4 | `ArtifactCore/src/Physics/SandSim2D.cppm` | PhysicsSystem 統合 |
| P4 | 新規 `Artifact/src/Widgets/Physics/SandPaintTool.*` | Sand ペイントツール |
| P5 | `tests/physics/` | 全ソルバーのユニットテスト |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: MPM GPU+Render+UX | **P0** | 大 | 最高品質ソルバー、GPU化で即実用 |
| P2: SoftBody 完成 | **P0** | 中 | 既に P1 完了、deformation grid が差別化要因 |
| P3: Fluid+Boids 統合 | **P1** | 中 | 既存コード豊富、layer 統合が主 |
| P4: Pyro+Sand | **P2** | 大 | GPU バックエンドが重い |
| P5: テスト | **P1** | 中 | 回帰保護、他 Phase と並行可能 |
