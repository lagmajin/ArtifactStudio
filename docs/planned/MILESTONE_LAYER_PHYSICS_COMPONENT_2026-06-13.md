# 実装案: M-LYR-PHYS Layer Physics Component

**最終更新:** 2026-10-20

**ステータス:** 部分完了（Collision 設定の Inspector/JSON、RigidBody/SoftBody bounds 同期、Circle collider 再構築、Polygon collider の Core〜UI 導線、component.joint レイヤー間ジョイント（Distance/Pin）、Fluid/Liquid の Components 専用編集項目を実装。Fluid/Liquid の Core・D3D12描画契約テストとD3D12/Vulkan headless parityを確認。通常アプリ画面でのruntime確認、ジョイントのアンカー編集・revolute角制限、Fracture/Fluid の共通 component 評価統合、複数レイヤー接触のruntime parityは未完了）

### 現行コード監査 (2026-08-15)

### 2026-09-05 Spring / Rope 接続点

- Components → Jointの `Type=3` はSpring、`Type=4` はRope。Target Layerに一意な名前またはIDを指定する。名前はsetterでIDへ解決して保存し、改名に追従する。旧データの名前参照は引き続き読み取るが重複名は接続しない。
- Owner / Target Anchor X/Yは各レイヤーのlocalBounds中心からのlocal px offset。両者をcomposition座標から各bodyのlocal座標へ変換する。length=0は両接続点の作成時距離を採用する。
- Springは既存Box2D 3.1.1のenableSpringを有効化してhertz/dampingRatioを使う。Ropeはhertz=0と長さ上限で伸び切った時だけ拘束する。既存Distance/Pin/Hingeの番号は維持する。
- 2DレイヤーのCollision / Joint有効bodyをcomposition共有worldへ統一。相手も有効なら実body同士を接続して反作用を返し、相手が無効ならowner所有のstatic proxyを使う。片側のJoint設定だけで双方向に作用する。3Dレイヤーへの接続は対象外。接続先ドロップダウン／pick-whip、VP接続点ハンドルは未実装。通常Properties面には露出せずComponents面を使う。
- CoreのNamedVectorでowner別joint IDを管理。接続解除・target変更・body再生成時は関連jointだけを破棄し、他の接続は維持する。削除済みlayerのbody/proxyは評価時に除去する。
- 剛体の初期位置はinitial値を含むsnapshotから取得し、描画には初期transformと剛体の差分移動・回転を合成する。body座標をレイヤー位置へ直接代入せず、radian/degrees混同を避ける。シミュレーション中のscale/anchorは初期値を維持し、明示的なbounds同期時に再構築する。
- setFramePosition / goToFrameを同じclockへ統合。1〜8フレームの前進は固定刻みで当該compositionだけを進め、各stepで固定targetを更新する。逆行／大きなseek／明示resetは移動先の編集値から再生成する。完全なrigid snapshot復元・過去からの決定論的replayではない。非一様scale親のshear collider精度、階層layoutやmodifierとの併用は実機検証が残る。
- Collisionの `Body Type` はDynamic / Static / Kinematic。Staticは衝突する固定物、Kinematicは編集Transformに追従し他のdynamic bodyへ力を伝える。Jointの `Type=5` はSlider（Prismatic）で、Axis、移動範囲、motor速度／最大力を持つ。
- `Break Force` が0より大きいjointは、fixed step後のBox2D constraint forceが閾値以上なら切断する。切断状態はランタイム専用で保存しない。再生開始・巻き戻し・大きなseek・joint設定変更で再接続できる。破断イベントの見た目／音／particle発生は未実装。
- 再生中のSelectionツールでは、選択済みDynamicレイヤーを左ドラッグするとランタイム専用のBox2D Mouse Jointで操作できる。保存済みjointとは別所有のため、既存のSpring/Rope/Sliderを置換せず、リリース時に破棄する。Static/Kinematic、ギズモ編集、パン／オービットは対象外。
- Box2Dのtransient contact bufferをfixed step直後にCoreでコピーし、`LayerRigidBodyContactState` として各レイヤーへ反映する。開始／終了／hit件数はstep単位、active接触数は継続状態、最大接近速度・直近の相手／hit位置／法線を参照できる。床などレイヤー外の静的shapeは相手LayerIDがnilになる。保存・Undo・新規signalは使わない。
- CPU物理のみ。GPU backend・同期の変更なし。ビルド／テスト／実機確認は未実行。振動・たるみ・接続点・Undo・保存復元の検証が残る。
- 根拠: [Box2D Distance Joint](https://box2d.org/documentation/group__distance__joint.html)、[3.1.1 solver（MIT）](https://github.com/erincatto/box2d/blob/v3.1.1/src/distance_joint.c)。既存依存APIを使用し、外部コードの複製は行っていない。

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

## 2026-08-31 進捗（通常レイヤーの落下を Box2D へ接続）

- `component.collision.enabled` が有効なレイヤーは、transform 評価時に `RigidBody2D` を自動生成するよう接続。
- `Physics2D` にレイヤーごとの床 collider を再利用登録する `setStaticFloor()` を追加。床の高さは `collisionFloorY`、未設定時はコンポジション下端を使用する。
- レイヤーの `gravityY` を Box2D world に反映し、剛体が存在する場合は従来の layer-local gravity / floor clamp を適用しないよう整理。
- 床 proxy は `cloneIndex=-3` として、joint proxy（`-2`）およびレイヤー自身の dynamic body と区別する。
- 通常の Collision レイヤーは Composition 単位の rigid world を共有し、body の `ownerLayerId` で各レイヤーの同期対象を識別するよう拡張。ジョイント専用レイヤーは既存の layer world を維持する。
- 未確認: 複数レイヤー間の衝突結果、物理 snapshot のスクラブ復帰、ビルド／実機ランタイム挙動。
- 未検証: ビルド・ランタイム（ユーザー指示待ち）。

## 2026-08-31 進捗（レイヤー単体の落下調整）

- 既存の `physics.linearDamping` を Inspector 上で `Air Drag` として明示し、固定値ではなく Box2D rigid body に反映するよう変更。
- `physics.angularDamping`（`Angular Drag`）と `physics.gravityScale`（`Gravity Scale`）を追加。どちらも JSON 保存／復元と既存 body への即時反映に対応。
- 物理 body を使わない既存の layer-local gravity 経路でも `Gravity Scale` を重力加速度へ乗算する。
- 未検証: ビルド・ランタイム（ユーザー指示待ち）。

## 2026-08-31 進捗（重力パラメータの操作性）

- `physics.fallProfile` を追加。`Custom / Light / Normal / Heavy / Floaty` の5段階で `Gravity Scale`、`Air Drag`、`Angular Drag` をまとめて設定する。
- 個別の倍率・抵抗を編集すると profile は自動で `Custom` に戻る。profile選択時は関連プロパティのキャッシュも更新し、Inspector 再構築を待たずに数値を同期する。
- 生の `physics.gravityY` は `World Gravity Y (Advanced)` として表示し、既存の Box2D world がある場合は共有 world の重力へ即時適用する。通常のレイヤー調整は `Fall Profile` / `Gravity Scale` を使う。
- 未検証: ビルド・ランタイム（ユーザー指示待ち）。

## 2026-10-19 進捗（2D Fluid / Liquid の編集導線）

- Components 専用面に `Fluid` と `Liquid` の property group を追加。Stable Fluids の格子設定と、粒子液体の充填率・流入・重力・表面張力・描画色／泡色などを既存 setter の値域に合わせて編集できる。
- Component property cache の routed metadata に Fluid / Liquid の全項目を追加し、`setLayerPropertyValue()` の component 経路で成功した編集を cache へ反映する。Inspector の値表示と Undo 比較に使う cached property が古いまま残る問題を防ぐ。
- `PhysicsLodBridgeContractTest` に Components 面の公開項目、色型、編集後の cache 更新を確認する契約テストを追加。Core 側には `FluidSolver2D` snapshot 復元後の決定性と、解像度不一致 snapshot の拒否テストを追加。
- `ArtifactPhysicsLodBridgeContractTest.LiquidLayerRendersAndWritesCoffeeColoredPngCapture` を Diligent D3D12 headless renderer で実行。フレーム0と12の GPU readback を比較し、コーヒー色画素と時間経過による画素変化を確認、両 PNG を `%TEMP%/ArtifactStudioFluidCapture/` に出力する。
- キャプチャで泡が大きく目立ち表面が平板に見えたため、Fluid コンポーネントの泡量既定値を下げ、描画粒子を縮小・減光。Core 側の既存表面密度を使った陰影に方向光と局所ハイライトを加え、GPUキャプチャテストに明暗幅条件を追加した（最終確認中）。
- キャプチャ単体テストは成功。テスト実行ファイル全体は既存 Physics LOD テスト5件で失敗しており、LOD設定の期待値と現行値のずれを別途確認する必要がある。実アプリ UI での操作確認は未実施。
- 追加確認: 面の三角形ごとの頂点色を補間する `drawGradientTriangleLocal()` を既存 Diligent 描画経路へ追加し、格子三角形の境界を目立たせない陰影に変更。メッシュがある場合は容器内のぼかし粒子を隠し、輪郭と泡だけを残す。フレーム0/12の GPU capture と単体テストが成功し、時間変化と陰影を確認。
- 色確認で `RenderConfig::MainRTVFormat` が sRGB target (`TEX_FORMAT_RGBA8_UNORM_SRGB`) と確認。Liquid/foam property は QColor の sRGB 値として保存されるため、描画直前に linear 化しないと coffee brown が薄い tan に見える。液体、輪郭、泡の描画入力を linear 化し、テストで描画色平均が property color に近いことを確認する assertion を追加。輪郭描画も capture で有効化。
- キャプチャ fixture は 320×320 の中央に 220×220 レイヤーを置き、フレーム0/12の両方で Transform3D を設定。少量の開口流入 (24 particles/s, speed 1.2) を加え、既定の輪郭 opacity 0.42 で色・動き・描画範囲を確認する。画素差分は 1000 pixels 超、水平の余白を保つことも assertion にする。
- 視覚確認: 初回は流入・foamAmount 設定を別の property contract test に誤って置いており、capture fixture に設定が届いていなかった。設定を GPU capture fixture へ移し、foamAmount 1.0 では大きな泡リングで画面が汚れること、既定相当の0.35では中央の注ぎ流れと液面接触部の泡が出ることを確認。frame12 の泡量あり／0 の GPU readback を比較し、ハイライトを固定した状態で1788画素が変化（泡差分範囲 x=136..183, y=110..156）。PNG を `liquid-coffee-frame-12.png` / `liquid-coffee-frame-12-no-foam.png` に出力し、テストでは差分50画素超を要求する。
- Core に `foamOffsetY` を追加。密度場と液面三角形は元粒子位置から作り、泡の評価も元位置の露出を使いつつ、泡描画点だけを液面へ移動できる。レイヤーの矩形容器では充填面より上の stream particle のみを液面へ投影し、泡の detached halo を抑える。Core test は投影前後で foam point が12単位移動し、triangle geometry が不変であることを確認（Liquid系18件 pass）。
- 同一フレームの泡あり／なし比較で、大きな明るいhaloは陰影ではなく泡particleの画面占有が主因と確認。泡描画を `size * 0.42` / `alpha * 0.48` へ縮小・減光し、差分が泡の局所だけに出ることをGPU capture testで再確認。
- コーヒー色の面シェーディングは白いspecularを使わず、頂点ごとの上下方向linear color勾配（上部1.0、下部0.48相当）へ変更。320×320 capture の中心画素は上部 (89,53,29) から下部 (75,46,25) へ変化し、GPU capture testはpass。ただし画面全体では勾配がまだ弱く、液面は平板に見える。
- `PhysicsSystem` から登録済み `FluidSolver2D` へ反映する明示LOD iteration budgetを1..12にclamp。`ReducedAndMinimalLODApplyFluidIterationBudgetsToRegisteredSolver` はpass。関連契約テスト全体は12件中8件passで、残る4件（Material 3件、SoftBody 1件）は今回の流体変更範囲外の失敗。
- 液面輪郭の上向きに近い区間（横方向に長く、surface bounds の上側58%以内）へ、液体色と foam 色を混ぜた細いCrema反射を追加。D3D12 capture で中央の流れから液面へつながる暖色の縁取りを確認し、GPU capture test は成功。輪郭全体の色・幅と既存のコンテナ線は維持。
- 最新の泡サイズ・不透明度と上下色勾配を含む `LiquidLayerRendersAndWritesCoffeeColoredPngCapture` を D3D12 / Vulkan で再実行。Vulkan log で NVIDIA GeForce RTX 4070 Ti を選択し、両 backend の frame12 PNG SHA-256 が `5D352ED5E9BD4F53E4273EF65BD6174C7D0E967C4D12F173012AE7BCDC639FC6` で一致。Diligent描画経路の最終調整後 parity を確認。
- 泡の粒子サンプルから速度をCoreのsurface snapshotへ保持し、アプリ側では `VelocityAligned` billboard と軽い横ストレッチで描画。落下流の接触点に泡が横へ広がることをGPU captureで計測し、差分領域の幅が高さを上回る契約を追加。Core物理テストは30/30成功。D3D12/Vulkan capture testも両方成功し、frame12 PNG SHA-256 は `4D5E80264E4DE6B57EBA2B8649248F24D5C8C07DAA57F29E3C5464B891621A10` で一致。
- 画面キャプチャでは接触点の泡が横長のクレマ状になり、以前の広いハローは抑制されている。一方、液体プールは依然として平坦で角張っており、水面の屈折・厚み・反射シェーディングは未実装。通常アプリのviewport表示は未確認。
- 液体の頂点ごとに密度値をCore surface snapshotへ残し、GPU色補間に使って厚みの控えめな暗化を追加。輪郭線の幅・不透明度も下げた。密度勾配を使う方向性ハイライトも試したが、密度場の三角形分割が斑点として見えるため採用せず、現在は滑らかな上下色勾配へ戻している。Core physics 31/31、Fluid/Liquid app contract 3/3成功。D3D12/Vulkan frame12 PNG SHA-256 は `D0D34E9A8F73FDFFBCA2512FEEE7BBF47351C430C2A5BE796AE1900FAD8483BA` で一致。なお画像はまだ平坦で、通常viewportでの見え方は未確認。
- 密度勾配法線は粒子由来の三角形斑点を出したため撤回を維持し、座標ベースの低振幅な contact reflection band を追加。frame12 no-foam capture の接触中心画素が同じ高さの横方向プール画素より明るいことを契約テスト化。D3D12/Vulkanで capture test pass、PNG SHA-256 `4200A72CF5B4430B540992D9DCCA77FD7D3E9A7D731B87B35FB5E2BD784CE050` で一致。画像上では流れの下に柔らかな crema 状ハイライトが出るが、実アプリviewportは未確認。
- 固定bbox高さに置いていたcontact bandを、surface contourの横向き上縁セグメントの幅重み付き中心へ追従させた。追加処理は既存surface snapshotの最大20,000 contour segmentsを一度走査するだけで、確保はない。GPU契約3/3 pass、D3D12/Vulkanのframe12 PNG SHA-256 `D7C3453DC39C002EAB6137EC019BC48063332D171E3C5DFB5DC5C4D661529D5F` 一致。固定位置版に比べ、fill形状が変わった際もハイライト位置がsurface contourから決まる設計になったが、別fill値での位置追従試験と通常viewport確認は今後。
- 最新 capture はコーヒー色に近づき、泡haloは縮小したが、液面は角張って平坦。D3D12/Vulkan headless parityは確認済み。通常アプリ UI の見え方、実アプリでの操作・保存再読込は引き続き未検証。
- Core の Liquid テスト17件を実行。初回は `LargeInputLodSamplingMatchesExplicitlySampledInput` が、LOD の `sqrt(stride)` による粒子半径補正を explicit 側へ適用していない期待値の不整合で失敗。参照入力を同じ有効面積へ補正し、比較対象を表面三角形と輪郭に絞って修正後17/17 pass。foam behavior は独立した LiquidSurface テストで確認。
- 2026-10-20 に Artifact.exe を Debug 構成で再ビルドし、液体描画コードを含む executable のリンク完了を確認。Computer Use の Node kernel は reset 後も「kernel assets のパスが見つからない」ため、通常アプリ画面の起動・目視検証は未実施。
- `LiquidSolver2D::update()` の毎回生成される前サブステップ位置 `std::vector` を solver 所有の `NamedVector<LiquidContainerPoint2D>` 作業領域に置換し、最終サブステップ変位から速度を再構成するテストを追加。`ArtifactCorePhysicsDeterminismTest` 全29件（Liquid 19件を含む）が29/29 pass。Artifact.exe と `ArtifactPhysicsLodBridgeContractTest` を再ビルドし、Components設定・GPU描画・Fluid LOD上限の3契約も3/3 pass。
- 今回は位置バッファ1個を再利用化したが、neighbor pair探索の `std::map` / `std::vector` は各距離反復内で再構築されるまま。allocation数・CPU時間の実測は未実施で、fluid solver全体をallocation-freeとは扱わない。
- `LiquidLayerRendersAndWritesCoffeeColoredPngCapture` に泡なしの充填率差分キャプチャを追加。0.62から0.35へ変更したフレーム12のpool edge位置が20px超下がることをD3D12/Vulkan GPU readbackで確認し、両テスト成功。通常充填capture hash `D7C3453DC39C002EAB6137EC019BC48063332D171E3C5DFB5DC5C4D661529D5F` と低充填capture hash `A34E4F950D99F103138F372BD8373B1B0BB6E004A9858FEDF2B5BD2B3598E21D` がbackend間で一致。輪郭追従contact bandの入力輪郭が充填率変更に追従することを確認したが、contact band自身の中心画素位置は未計測。
- Vulkan画像のx=width/3列を調べ、fill .62で液体色が始まるy=154、fill .35でy=203を確認（約49px低下）。液面位置は視覚・数値の両方で分離できる一方、泡なし低充填画像のcontact reflection自体は弱く、中心位置とコントラストの直接計測が次の課題。
- 反射中心の候補選択を、矩形容器では `1-liquidFillAmount` から得る変換済み公称面の近傍へ変更。液体全体のboundsに注ぎ流れが含まれても、低いpool surfaceが候補帯から外れにくくした。GPUテストに低充填泡なし時の液面直下中央 vs 側方の輝度差8以上を追加しD3D12/Vulkanでpass。通常 / no-foam / lower-fill画像のbackend間ハッシュはそれぞれ `42153CA0E765CBB7F6829FDDA5115AC768B54B90DEFC11C146C2C3DBF8CBC3DE`, `C47EF2FD257542B0F22548C6A6373FB8146FE933B5AF6897A21433B7F921A9D5`, `711559CA4523724FCC994C009E83A86D06FA14CA7B5AC5580DE578C41383F798` で一致。回転・polygon容器と通常viewportは未確認。
- 反射中心と頂点シェーディングの横／縦座標を、描画matrixから得るlayer X/Y軸への投影値へ統一。25度回転した低充填画像でコーヒー色画素数・明暗幅を確認するcapture assertionを追加。D3D12/Vulkanの回転画像hash `9AA5018F58128BD2F318439241640947763FEA5A81950661C972021E9CCF7CA1` が一致し、描画契約テストは両backendでpass。回転時の液面傾斜角・通常UI viewportは未検証。
- Coreの `LiquidSolver2D::buildSurfaceSnapshot()` 内で三角形clipごとに構築していた3つの一時`std::vector`を、上限4出力頂点・2交点のstack `std::array`へ置換。Core PhysicsDeterminism 31/31 pass。Artifact描画契約もD3D12/Vulkanでpassし、通常 / no-foam / lower-fill / rotatedの4 PNGはbackend間で一致、かつ変更前の保存済み画像と同一で幾何・描画結果を維持。density map・surface cell set等の他のsnapshot確保と性能profileは未検証。
