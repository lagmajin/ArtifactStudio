# パーティクル・流体シミュレーション 参照ガイド

> ArtifactStudio の Cloner / Particle Emitter / Fluid エフェクトのアルゴリズム参照先
> 個人研究・小規模プロジェクトを含む

---

## 参照プロジェクト一覧

### 🔴 Tier 1: アルゴリズムの教科書として即参考になる

| プロジェクト | ⭐ | 言語 | ライセンス | 手法 | 何が優れているか |
|---|---|---|---|---|---|
| **[PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics)** | 2.2k | C++ | **MIT** | PBD, XPBD, PBF (Position Based Fluids) | **剛体・布・流体・ロッドを統一フレームワークで扱う**。論文多数の参照実装。PBD はリアルタイムVFXの事実上の標準。 |
| **[GridFluidSim3D](https://github.com/rlguy/GridFluidSim3D)** | 828 | C++11 | **Zlib** | PIC/FLIP (Eulerian grid) | **Bridson の教科書の完全実装**。380コミットと充実。C++ API + Pythonバインド。メッシュ出力。 |
| **[incremental_mpm](https://github.com/nialltl/incremental_mpm)** | 396 | C#/Unity | **MIT** | MLS-MPM | **解説記事付き**。弾性体+流体を数百行で実装。学習用として完璧。 |

### 🟠 Tier 2: GPU / プロダクション向け

| プロジェクト | ⭐ | 言語 | ライセンス | 手法 | 何が優れているか |
|---|---|---|---|---|---|
| **[Taichi](https://github.com/taichi-dev/taichi)** | 28.3k | Python/C++ | Apache 2.0 | MPM, SPH, PIC, FEM | GPUコンピューティングフレームワーク。**MPM 流体/弾性体 88行のコード例あり**。examples/simulation/ に全デモ。 |
| **[OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb)** | 3.3k | C++ | Apache 2.0 | Sparse Volume | 業界標準のボリュームデータ構造。煙・火・霧の保存/操作に。DreamWorks 発。NanoVDB で GPU 対応。 |
| **[Blender Mantaflow](https://projects.blender.org/blender/blender/src/branch/main/intern/mantaflow)** | — | C++/Python | GPL | FLIP + グリッド | プロダクション品質の流体シミュレーション。ただしライセンスが GPL。 |

### 🟡 Tier 3: WebGL/デモ系（アルゴリズム学習用）

| プロジェクト | ⭐ | 言語 | 手法 |
|---|---|---|---|
| **[waves](https://github.com/dli/waves)** | 1.1k | WebGL/JS | FFT海洋波 |
| **[WebGL Fluid Simulation](https://github.com/PavelDoGreat/WebGL-Fluid-Simulation)** | 15k+ | WebGL/JS | Eulerian N-S |

---

## 主要アルゴリズム早見表

| 手法 | 用途 | リアルタイム性 | ArtifactStudio 適合度 |
|---|---|---|---|
| **PBD / XPBD** | 布、ロープ、紐、ソフトボディ | ★★★ 高速 | ★★★★★ Cloner物理変形に最適 |
| **PBF (Position Based Fluids)** | 液体（粒子ベース） | ★★☆ | ★★★★ Particle Emitter × Fluid |
| **PIC/FLIP** | 液体（グリッド+粒子ハイブリッド） | ★★☆ | ★★★ 高品質液体 |
| **MPM (Material Point Method)** | 雪、砂、弾性体、粘性流体 | ★☆☆ (GPU可) | ★★★★ 多様な物質表現 |
| **SPH** | 自由表面流体 | ★★☆ | ★★★★ GPU並列化しやすい |
| **Eulerian Grid (N-S)** | 煙、火、霧 | ★★☆ | ★★★ ボリュームエフェクト |
| **FFT Ocean** | 海面 | ★★★ | ★★★★ 即実装可 |

---

## 各プロジェクトの ArtifactStudio 応用ポイント

### PositionBasedDynamics (MIT)

```
PositionBasedDynamics/
├── Simulation/          ← シミュレーションループ
├── Common/              ← 数学ライブラリ
└── Demos/               ← 全デモシーン
    ├── ClothDemo/       ← 布
    ├── FluidDemo/       ← 流体
    ├── RigidBodyDemo/   ← 剛体
    └── RodDemo/         ← ロッド/紐
```

**ArtifactStudio 移植方針**:
- PBD/PBF は制約ベースなので GPU (Compute Shader) との親和性が高い
- 各粒子の位置更新は完全に独立 → 並列化容易
- 近傍探索 (Spatial Hashing) がボトルネック。GPU で Z-order / Morton code ベースのソートで高速化

### GridFluidSim3D (Zlib)

```
アルゴリズム: PIC/FLIP
1. 粒子→グリッド転送 (velocity, mass)
2. グリッド上で圧力ポアソン方程式 (PCG ソルバー)
3. グリッド→粒子転送 (FLIP or PIC blending)
4. 粒子移動 (advection)
5. 表面抽出 (Marching Cubes → PLY)
```

**ArtifactStudio 移植方針**:
- PCG ソルバーは GPU 実装が難しい → 簡易版の Jacobi 反復で近似
- 表面抽出 (Marching Cubes) は GPU で geometry shader または compute shader
- 出力はメッシュ → ArtifactStudio の 3D レイヤーパイプラインへ

### Taichi MPM 88行コード (Apache 2.0)

```python
import taichi as ti
ti.init(arch=ti.gpu)

# パーティクル + グリッド
n_particles = 8192
x = ti.Vector.field(2, float, n_particles)  # 位置
v = ti.Vector.field(2, float, n_particles)  # 速度
C = ti.Matrix.field(2, 2, float, n_particles)  # アフィン速度
J = ti.field(float, n_particles)  # 変形勾配行列式
grid_v = ti.Vector.field(2, float, (128, 128))  # グリッド速度
grid_m = ti.field(float, (128, 128))  # グリッド質量

@ti.kernel
def substep():
    # 1. パーティクル→グリッド (P2G)
    for p in x:
        base = (x[p] * inv_dx - 0.5).cast(int)
        fx = x[p] * inv_dx - base.cast(float)
        w = [0.5*(1.5-fx)**2, 0.75-(fx-1)**2, 0.5*(fx-0.5)**2]
        # stress = 変形勾配から計算
        # grid_v[base] += w * (M*p*v + stress) の形
    # 2. グリッド更新 (境界条件)
    # 3. グリッド→パーティクル (G2P)
    # 4. 粒子移動
```

**ArtifactStudio 移植方針**:
- この 88行のロジックを HLSL Compute Shader に翻訳
- P2G, G2P は並列リダクションが必要 → atomic add で実装
- 変形勾配に基づく物質モデル (Neo-Hookean, 砂, 液体) はパラメータで切り替え

---

## GPU粒子システムの設計パターン（全プロジェクト共通）

```
┌─────────────────────────────────────────┐
│  Particle Buffer (StructuredBuffer)     │
│  {pos, vel, life, size, color, ...}     │
└──────────────┬──────────────────────────┘
               │
  ┌────────────▼───────────┐
  │ 1. Emit Pass           │ ← 新規粒子生成 (Compute Shader)
  │    lifetimeリセット     │
  └────────────┬───────────┘
               │
  ┌────────────▼───────────┐
  │ 2. Simulation Pass     │ ← 物理更新 (Compute Shader)
  │   重力/風/抗力/衝突     │
  │   近傍探索 (SpatialHash)│
  │   死亡粒子除去 (Stream C)│
  └────────────┬───────────┘
               │
  ┌────────────▼───────────┐
  │ 3. Sort Pass           │ ← 深度/距離ソート (Bitonic/Radix)
  │   透明度合成のため       │
  └────────────┬───────────┘
               │
  ┌────────────▼───────────┐
  │ 4. Render Pass         │ ← インスタンス描画 or 頂点生成
  │   ビルボード/メッシュ    │
  └─────────────────────────┘
```

---

## 実装優先順位（パーティクル・流体編）

| 順位 | 項目 | 参照元 | 難度 | 備考 |
|---|---|---|---|---|
| 1 | GPU パーティクル基盤 (Emit/Sim/Render) | — | 中 | 全パーティクル系の土台 |
| 2 | PBD 布/ロープ | PositionBasedDynamics | 中 | 距離制約+曲げ制約 |
| 3 | SPH 流体 (基本) | — | 中-高 | 密度+圧力+粘度カーネル |
| 4 | MLS-MPM 弾性/流体 | incremental_mpm, Taichi | 高 | 統一物質モデル |
| 5 | PIC/FLIP 液体 | GridFluidSim3D | 高 | グリッドベース高品質 |
| 6 | FFT 海洋波 | waves | 低-中 | シェーダーで完結 |
| 7 | Eulerian 煙/火 | WebGL Fluid Sim | 中 | ボリュームレンダリング必要 |
