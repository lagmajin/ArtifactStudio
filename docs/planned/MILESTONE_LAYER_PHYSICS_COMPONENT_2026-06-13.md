# 実装案: M-LYR-PHYS Layer Physics Component

**最終更新:** 2026-08-25

**ステータス:** 部分完了（Collision 設定の Inspector/JSON、RigidBody/SoftBody bounds 同期、Circle collider 再構築、Polygon collider の Core〜UI 導線、component.joint レイヤー間ジョイント（Distance/Pin）を実装。ジョイントのアンカー編集・revolute角制限、Fracture/Fluid の共通 component 化、複数レイヤー接触の runtime parity、ビルド／ランタイム検証は未完了）

### 現行コード監査 (2026-08-15)

- `ArtifactAbstractLayer` は `component.collision.*` を Component descriptor、Property、JSON 保存／復元へ接続し、Box／Circle の bounds 同期、floor／composition bounds、collision output を持つ。
- `PhysicsSystem` は layer ID 単位の rigid world／soft body／collider／snapshot 管理を提供し、`ArtifactAbstractLayer` の RigidBody／SoftBody enable・disable・sync 経路から利用されている。
- SoftBody は `createSoftBodyGrid()`、collider 登録、LOD／snapshot の基盤まで確認できる。旧「共有 PhysicsWorld 不在」は現状には適用しない。
- Polygon collider の通常レイヤー導線は 2026-08-25 に実装（下記進捗参照）。Fracture／Fluid／Constraint の共通 component 化、複数レイヤー接触の runtime parity、Inspector の専用 Physics surface、ビルド・実機検証は未完了または未確認。

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

## 2026-07-11 進捗

- `component.collision.enabled/shape/width/height/radius/offsetX/offsetY` のInspector・JSON経路を追加済み
- SoftBody/RigidBodyのbounds同期がコライダー設定を参照するよう接続済み
- RigidBody初期生成時のCircle形状、および形状変更時の対象body再構築を追加
- 既存のlayer-local spring / gravity / floor collisionは維持
- 未実施: 複数レイヤー間の共有PhysicsWorld、Polygon collider、実行時のビルド／ランタイム検証

## 2026-08-25 進捗（Polygon collider 導線）

- `component.collision.shape` に `3=Polygon` を追加。tooltip enum（`0=Auto Bounds, 1=Box, 2=Circle, 3=Polygon`）、hard/soft range、setter・JSON復元のclampを 0..3 へ拡張。従来tooltip末尾の `.` がラベルに混入する問題も解消。
- `ArtifactAbstractLayer` に仮想 `collisionOutlineLocalPoints()` を追加（既定は空）。`ArtifactShapeLayer` がoverrideし、custom path頂点 ≥3 / custom polygon ≥3 / 通常形状は `buildRenderablePoints()` の輪郭を返す。operator stack適用時とLineは空を返しauto-boundsへフォールバック。
- 新helper `layerCollisionPolygonLocalPoints()` が shape==3 のoutlineに offsetX/Y を適用して返す。`layerCollisionLocalBounds()` は shape==3 でoutline bboxを返す（outline無しはauto-boundsフォールバック）。
- `syncSoftBodyPhysicsColliderToBounds()`: outline ≥3 で `SoftBodyCollider::Type::Polygon` + interleaved `polygonPoints` を登録。それ以外は従来どおりBox。
- `syncRigidBodyPhysicsToBounds()`: shape==3 で `Physics2D::addPolygonBody()` を使用（Box2D v3 hull最大8頂点のため8点へ等間隔ダウンサンプル、生成失敗時はBoxフォールバック）。`addPolygonBody` にfriction/restitution引数を追加しhull失敗時にbodyを破棄するよう修正。
- Composition の `evaluateLayerCollisionPairs()`: source outlineをsource transform → target逆変換でtarget局所空間へマップし `MpmCollider2D::Type::Polygon` を登録。outline無しはBoxプロキシ。
- Clone instance 経路 `collisionLocalBounds()` はoutline bboxをconservative proxyとして使用（vertex-exact判定はソルバー側）。
- PhysicsSystem の死に経路だった deprecated グローバル流体（`initFluid` / `getFluidSolver()` / `fluidSolver_` 系メンバーとupdate/clear経路）を削除。per-layer `createFluidSolver(layerId)` が唯一の流体登録経路。
- 未検証: ビルド・ランタイム（ユーザー指示待ち）。SoftBody経路の shape==2（Circle）は従来どおりBox扱い（既存挙動維持、Circle化は別判断）。

## 2026-08-25 進捗（component.joint レイヤー間ジョイント）

- 新component `builtin.joint` / `artifact.component.joint`（Dynamics/Composition, order 510, collision依存）を追加。`component.joint.enabled/type(0=Distance,1=Pin)/targetLayer(string)/length/stiffness(hertz)/damping(ratio)` のproperty group・Inspector Components面フィルタ・JSON保存/復元・descriptor settingsを実装。
- joint有効化時に `enableRigidBodyPhysics()` を呼ぶため、監査で「到達不能」とされていたリジッド(Box2D)実行経路に初の実利用入口ができた。
- Core `Physics2D`: `addStaticAnchor()`、joint id 管理、`removeJoint/clearJoints/getJoints` を追加。body再構築時・無効化時はjointを明示破棄してid鮮度を保証。
- Composition `evaluateJointConstraints()` を `setFramePosition`/`goToFrame` のcollision評価直後に配線。target layer名解決 → target中心をowner局所空間へマップ → 静的アンカー(cloneIndex==-2)生成/追従 → signature変更時のみjoint再構築。Distance length=0 は作成時距離を採用。リジッド読み取り/body選択はproxyを除外して自レイヤーdynamic bodyを選択。
- 制約: rigid worldはsnapshot非対応（スクラブ復帰は未対応）、target名重複時は先勝ち、アンカー手動オフセットとrevolute角制限は未実装。`Physics2D` world重力 {0,-9.8} の符号は要ランタイム確認。
- 未検証: ビルド・ランタイム（ユーザー指示待ち）。
