# 実装案: M-LYR-PHYS Layer Physics Component

> 2026-06-13 作成  
> 物理シミュレーションをレイヤーのコンポーネントとして統合（エフェクトではない）

---

## 既存物理システム

| モジュール | 用途 |
|-----------|------|
| `Physics2D.ixx` | Box2D v3ラップ、RigidBody2D、Physics2D world |
| `FractureEngine.ixx` | ポリゴン分割（Voronoi/直線切断） |
| `FluidSolver2D.ixx` | 2D流体シミュレーション |
| `SoftBodySolver.cppm` | ソフトボディシミュレーション |

---

## 現状整理

いま `Artifact.Layer.Physics` にある `physics.enabled` は、レイヤー変形に対する
spring / damping / follow-through の補助であり、**当たり判定そのものは持っていない**。

したがって「落下して、床や他レイヤーに当たって止まる」挙動を作るには、
レイヤー物理とは別に **Collider / RigidBody の境界** を追加する必要がある。

最小構成は次の2層に分ける。

1. **Layer-local physics**
   - 既存の `physics.*` はここに残す
   - 見た目の慣性、追従、揺れを担当する
2. **Collision physics**
   - `Physics2D` または同等の剛体 world を使う
   - 形状はまず AABB / Box / Circle のどれかに限定する
   - レイヤーの `localBounds()` か `transformedBoundingBox()` を collider 初期形状に流用する

この切り分けを維持すると、既存のアニメーション物理を壊さずに
「落とす」「ぶつける」を段階追加できる。

---

## レイヤーコンポーネント設計

### ArtifactLayerPhysicsComponent.ixx

```cpp
module;
#include <memory>
#include <vector>
#include <QMatrix4x4>

export module Artifact.Layer.Physics;

import Artifact.Layer.Abstract;
import Physics2D;
import Physics.Fracture;
import Physics.SoftBody;
import Utils.Id;

export namespace Artifact {

// 物理シミュレーションの種類
enum class PhysicsSimulationType {
    None,
    RigidBody,        // 剛体（落下・衝突）
    SoftBody,         // ソフトボディ（布・液体）
    Fracture,         // 破砕（爆発・衝撃）
    Fluid,            // 流体（液体・気体)
    Constraint,       // 固定・距離維持など
};

enum class ColliderShapeType {
    Box,
    Circle,
    Polygon,
};

// 物理コンポーネント設定
struct PhysicsComponentSettings {
    PhysicsSimulationType type = PhysicsSimulationType::None;

    // RigidBody用
    bool kinematic = false;
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.5f;
    bool useCollision = false;
    ColliderShapeType colliderShape = ColliderShapeType::Box;
    float colliderWidth = 0.0f;
    float colliderHeight = 0.0f;
    float colliderRadius = 0.0f;
    QVector2D colliderOffset{0.0f, 0.0f};
    float bounce = 0.0f;

    // SoftBody/Cloth用
    int subdivisions = 4;
    float stiffness = 1.0f;
    float damping = 0.1f;

    // Fracture用
    int fragmentCount = 8;
    float impulseForce = 1000.0f;
    QVector2D impulseDirection{1, 0};

    // Fluid Simulation用
    float viscosity = 0.1f;
    float pressure = 1.0f;
};

class ArtifactLayerPhysicsComponent {
private:
    class Impl;
    Impl* impl_;

public:
    ArtifactLayerPhysicsComponent(ArtifactAbstractLayer* parentLayer);
    ~ArtifactLayerPhysicsComponent();

    // 設定
    PhysicsComponentSettings settings() const;
    void setSettings(const PhysicsComponentSettings& settings);

    // 物理シミュレーション
    void enablePhysics(PhysicsSimulationType type);
    void disablePhysics();
    bool isPhysicsEnabled() const;

    // シミュレーションステップ（外部から呼び出し）
    void simulateStep(float deltaTime);

    // レイヤー変換に物理結果を適用
    QMatrix4x4 physicsTransform() const;

    // 破砕シミュレーション
    void triggerFracture();
    std::vector<ArtifactAbstractLayerPtr> createFragmentLayers() const;

    // イベント
    void physicsUpdated() W_SIGNAL(physicsUpdated);
};

} // namespace Artifact
```

### Collision settings 追加案

- `useCollision`
  - collider を使うかどうか
- `colliderShape`
  - `Box` / `Circle` / `Polygon`
- `colliderWidth`, `colliderHeight`, `colliderRadius`
  - shape ごとのサイズ
- `colliderOffset`
  - レイヤー原点からの collider オフセット
- `bounce`
  - 反発
- `friction`
  - 接触面の摩擦

### 解決ルール

- まずは `Physics2D` の既存剛体 world を再利用する
- レイヤーの `localBounds()` を初期 collider の基準にする
- 3D / 変形後の厳密メッシュ衝突は後回しにする
- 既存の spring / follow-through は衝突解決の前段として残す
- ソフトボディは `PhysicsSystem` 経由で `createSoftBody()` し、`registerSoftBodyCollider()` で床や箱を与える
- レイヤー側の入口は `enableSoftBodyPhysics()` / `disableSoftBodyPhysics()` / `syncSoftBodyPhysicsColliderToBounds()` で最小配線する
- 格子初期化は `createSoftBodyGrid()` と `enableSoftBodyPhysicsGrid()` で bounds から自動生成する

---

## Layer への統合

### ArtifactAbstractLayer への追加

```cpp
// Layer.ixx へ追加
class ArtifactAbstractLayer {
    // ... 既存コード ...

    // Physics Component
    ArtifactLayerPhysicsComponent* physicsComponent() const;
    void setPhysicsComponent(std::unique_ptr<ArtifactLayerPhysicsComponent> comp);
    bool hasPhysicsComponent() const;

    // 物理シミュレーション有効フラグ
    bool isPhysicsEnabled() const;
    void setPhysicsEnabled(bool enabled);

protected:
    std::unique_ptr<ArtifactLayerPhysicsComponent> physicsComponent_;
};

// 改造: draw() / effectiveTransform() に物理考慮
QMatrix4x4 effectiveTransform() const {
    QMatrix4x4 result = getLocalTransform();
    if (physicsComponent_ && physicsComponent_->isPhysicsEnabled()) {
        result = result * physicsComponent_->physicsTransform();
    }
    return result;
}
```

---

## ウィジェット連携

### ArtifactToolOptionsBar への追加

```
[物理] ツール選択時に表示:

Simulation Type: [Rigid Body ▼]
- Rigid Body
- Soft Body
- Fracture
- Fluid
- Disabled

[剛体]
Mass: [1.0] Density: [1.0]
Friction: [0.3] Bounce: [0.5]

[ソフトボディ]
Subdivisions: [4]
Stiffness: [1.0] Damping: [0.1]

[破砕]
Fragment Count: [8]
Impulse Force: [1000]
```

### コンポジションエディタ

- 物理レイヤー選択中にギズモ表示
- "Simulate" ボタンで手動ステップ
- Timeline に物理キー追加（回転/位置記録）
- ヒットテスト: 物理形状に応じた選択

---

## 作業順

| フェーズ | 内容 | 作業時間 |
|---------|------|--------|
| Phase 1 | `Artifact.Layer.Physics` モジュール作成 | 2h |
| Phase 2 | RigidBody レイヤー統合、Physics2D 参照 | 3h |
| Phase 3 | PhysicsComponent を Layer へ追加 | 2h |
| Phase 4 | ToolOptionsBar へ物理UI追加 | 2h |
| Phase 5 | 破砕/ソフトボディサポート | 3-4h |
| **合計** | | 12-13h |

---

## コード参照

- `ArtifactCore/include/Physics/2D/Physics2D.ixx`
- `ArtifactCore/include/Physics/FractureEngine.ixx`
- `ArtifactCore/include/Physics/FluidSolver2D.ixx`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`

---

## 関連マイルストーン

- `MILESTONE_LAYER_MODIFIER_SYSTEM_2026-06-13.md` - 変形システム（物理コンポーネントと併用）
- `MILESTONE_MESH_INSTANCING_2026-04-26.md` - インスタンス描画（RigidBody座に適用）
