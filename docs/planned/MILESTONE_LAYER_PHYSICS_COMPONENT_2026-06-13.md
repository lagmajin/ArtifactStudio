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

// 物理コンポーネント設定
struct PhysicsComponentSettings {
    PhysicsSimulationType type = PhysicsSimulationType::None;

    // RigidBody用
    bool kinematic = false;
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.5f;

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