# MILESTONE: Mesh Instancing Infrastructure for DiligentEngine

**Date**: 2026-04-26
**Status**: Planned
**Priority**: High
**Related**: MILESTONE_M11_SOFTWARE_RENDER_PIPELINE, EFFECT_SYSTEM_SPECIFICATION

---

## 概要

C4D MoGraph ライクなクローンシステムを AE 風アプリで実現するため、DiligentEngine を用いたメッシュ用インスタンシング基盤を構築する。
既存の `ParticleRenderer`（Structured Buffer + `SV_InstanceID`）の実績を活かし、任意のメッシュを GPU インスタンス描画できる共通基盤を整備する。

---

## 背景と現状

### 既存資産
| コンポーネント | 場所 | 内容 |
|---|---|---|
| `ParticleRenderer` | `ArtifactCore/include/Graphics/ParticleRenderer.ixx` | Structured Buffer + `NumInstances` でのパーティクル描画 |
| `Mesh` | `ArtifactCore/include/Mesh/Mesh.ixx` | `RenderData`（positions, normals, uvs, indices）を持つがインスタンシング未対応 |
| `CloneData` / Effectors | `Artifact/include/Effects/Clone/CloneCore.ixx` | CPU 側クローンデータ生成済み |
| `ArtifactCloneLayer` | `Artifact/include/Layer/ArtifactCloneLayer.ixx` | スタブのみ、`draw()` 未実装 |

### 課題
- メッシュ用 `DrawIndexedInstanced` 基盤が不存在
- `Mesh::RenderData` をそのまま GPU バッファへアップロードする仕組みがない
- クローンごとの transform/color/weight を GPU に渡すインスタンスデータ設計が未決

---

## 目標設計

### 1. MeshRenderer（新規）
パーティクルと同様の Structured Buffer 方式でメッシュをインスタンス描画する。

```
MeshRenderer
├── initialize(maxInstances, meshReference)   // メッシュ参照を固定
├── setInstanceData(const std::vector<InstanceData>& instances)
├── prepare(IDeviceContext*)
├── draw(IDeviceContext*, instanceCount)
├── setViewProjection(view, proj)
└── Impl
    ├── pMeshBuffer_           // Vertex/Index Buffer (メッシュ固定)
    ├── pInstanceBuffer_       // Structured Buffer (インスタンスごとに更新)
    ├── pConstantBuffer_       // View/Proj 行列
    └── pPSO_
```

### 2. InstanceData 構造体
```cpp
struct InstanceData {
    float transform[16];   // QMatrix4x4 を float[16] で渡す
    float color[4];        // RGBA
    float weight;          // エフェクター影響度
    float timeOffset;      // タイムリマップ用
    float padding[2];     // 16byte alignment
};
```

### 3. シェーダー設計
パーティクルと同様、`SV_InstanceID` で `StructuredBuffer<InstanceData>` にアクセス。

```hlsl
StructuredBuffer<InstanceData> g_Instances : register(t0);

PS_Input VSMain(VS_Input In) {
    InstanceData inst = g_Instances[In.InstanceID];
    float4 worldPos = mul(float4(In.Pos, 1.0), inst.transform);
    // ... view/proj 変換
}
```

---

## 実装マイルストーン

### Phase 1: メッシュ GPU バッファ基盤
**目標**: `Mesh::RenderData` を Diligent Buffer にアップロードし、通常描画できるようにする。

- [ ] `MeshRenderer::createMeshBuffers()` を実装
  - `RenderData.positions` → Vertex Buffer
  - `RenderData.normals` → Optional Normal Buffer (または interleaved)
  - `RenderData.uvs` → UV Buffer
  - `RenderData.indices` → Index Buffer
- [ ] 最小シェーダー（頂点変換のみ）で単体メッシュ描画を確認

### Phase 2: インスタンスデータ基盤
**目標**: `InstanceData` 用 Structured Buffer を作成し、インスタンスごとの transform を反映する。

- [ ] `InstanceData` 構造体を `ArtifactCore/include/Graphics/` に定義
- [ ] `MeshRenderer::createInstanceBuffer(maxInstances)` を実装
- [ ] `setInstanceData()` で CPU→GPU アップロード
- [ ] シェーダーで `g_Instances[InstanceID]` を参照するよう修正

### Phase 3: PSO と描画パイプライン
**目標**: `DrawIndexedInstanced` を発行し、複数インスタンスを描画する。

- [ ] `createPSO()` で頂点レイアウトを定義（位置、法線、UV）
- [ ] `draw()` で `DrawIndexedAttribs::NumInstances` を設定
- [ ] View/Proj 行列を Constant Buffer 経由で渡す

### Phase 4: クローンレイヤー統合
**目標**: `ArtifactCloneLayer::draw()` から `MeshRenderer` を呼び出し、C4D MoGraph ライクなクローンを描画する。

- [ ] `ArtifactCloneLayer` に `MeshRenderer` を保持（Impl に追加）
- [ ] `draw()` で `generateCloneData()` → `InstanceData` に変換 → `MeshRenderer::draw()`
- [ ] ソースレイヤーのメッシュ（Shape/Text/SVG等）を `MeshRenderer` に渡す要件を定義

### Phase 5: エフェクター統合
**目標**: `AbstractCloneEffector` の結果を GPU インスタンシングに反映する。

- [ ] `TransformCloneEffector` → `InstanceData.transform`
- [ ] `RandomCloneEffector` / `NoiseCloneEffector` → `InstanceData.transform + timeOffset`
- [ ] `colorOffset` → `InstanceData.color`

---

## ファイル構成（予定）

```
ArtifactCore/include/Graphics/
├── MeshRenderer.ixx          (新規) メッシュインスタンシング描画
├── InstanceData.h             (新規) インスタンスデータ構造体
└── ParticleRenderer.ixx       (既存) 参考実装

ArtifactCore/src/Graphics/
├── MeshRenderer.cppm          (新規)
└── ParticleRenderer.cppm      (既存)

Artifact/include/Layer/
└── ArtifactCloneLayer.ixx     (拡張) MeshRenderer を使用
```

---

## 依存関係

- `Mesh::generateRenderData()` が正しく動作すること
- DiligentEngine の `DrawIndexedInstanced` が利用可能であること
- `GpuContext`（`ArtifactCore/include/Graphics/GPUcomputeContext.ixx`）が参照できること

---

## 成功条件

1. 単一メッシュを 1000 インスタンス以上、60fps で描画できる
2. 各インスタンスが個別の transform/color を持てる
3. `CloneData`（CPU）から `InstanceData`（GPU）への変換が 1 フレーム内で完了する
4. 既存の `ParticleRenderer` と API 設計を合わせる（統一感のあるコードベース）
