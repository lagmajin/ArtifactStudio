# MILESTONE: SceneNode Feature Expansion

> 2026-04-20 作成

## 目的

`ArtifactCore::SceneNode` を DCC 標準のシーングラフノード (Maya Outliner / Blender Outliner / 3ds Max Scene Explorer 相当) に引き上げる。

現状の `SceneNode` はヒエラルキー・SRT 変換・mesh/material 参照のみを持つ最小実装であり、UI 層からの参照もない。このマイルストーンで「制作に使えるシーンノード」へ段階的に拡張する。

---

## 現状

[`ArtifactCore/include/Scene/SceneNode.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Scene/SceneNode.ixx)
[`ArtifactCore/src/Scene/SceneNode.cppm`](X:/Dev/ArtifactStudio/ArtifactCore/src/Scene/SceneNode.cppm)

| カテゴリ | 既存機能 |
|---|---|
| Identity | `name` (UniString) |
| Hierarchy | `addChild`, `removeChild`, `removeFromParent`, `childCount`, `child`, `parent`, `children`, `descendants`, `path`, `findByName` |
| Transform | Local SRT: `position`, `rotation` (QQuaternion), `rotationEuler`, `scale`, `localMatrix`, `worldMatrix` (dirty-flag cache), `worldPosition` |
| Content | `Mesh` 参照, `Material` 参照, `visible` フラグ |
| Utility | `boundingBox` (world-space AABB), `totalNodeCount` |

**課題**: ノード種別なし、シリアライズなし、アニメーション接続なし、選択/ロック状態なし、UI 層からの参照ゼロ。

---

## 関連する既存型 (SceneNode 未接続)

| 型 | 場所 | 備考 |
|---|---|---|
| `Camera` | `ArtifactCore/include/Transform/Camera.ixx` | GLM ベース、シーンノードではない |
| `Light` | `ArtifactCore/include/Transform/Light.ixx` | Directional/Point/Spot/Ambient、シーンノードではない |
| `Mesh` | `ArtifactCore/include/Mesh/Mesh.ixx` | N-gon メッシュ、Skinning 対応済み |
| `Material` | `ArtifactCore/include/Material/Material.ixx` | PBR + MaterialX |
| `AnimationDynamics` | `ArtifactCore/include/Animation/AnimationDynamics.ixx` | Spring-damper / lag follower |
| `TransformHelper` | `ArtifactCore/include/Transform/TransformHelper.ixx` | AE 風 2D/3D 変換ユーティリティ |
| `Tag` | `ArtifactCore/include/Utils/Tag.ixx` | タグユーティリティ、未接続 |

---

## Phase 1: Core Foundations (~20-30h)

### 1-1 Node Type System

- `enum class SceneNodeType { Null, Mesh, Camera, Light, Locator, Joint, Curve, Group }`
- `type()` getter とコンストラクタ引数
- `Camera` / `Light` をシーンノードとして扱える土台

### 1-2 Serialization (JSON)

- `toJson()` / `fromJson()` — 再帰的に子孫を含む階層全体
- シリアライズ対象: name, type, transform (P/R/S), visibility, mesh path, material path, children
- 既存のプロジェクト JSON 保存経路 (`ArtifactAbstractLayer::toJson` 等) に合わせる

### 1-3 Selection / Lock / Template State

- `isSelected`, `isLocked`, `isTemplated` フラグ
- Outliner / Viewport 操作に必須

### 1-4 Display Layer Assignment

- `displayLayerId` / `displayLayerName`
- 可視性・カラーオーバーライドのグルーピング

**完了条件:**
- ノード種別で分類できる
- 階層ごと save/load できる
- 選択・ロック・テンプレート表示が扱える

---

## Phase 2: Transform Enhancements (~15-20h)

### 2-1 Rotation Order

- `enum class RotationOrder { XYZ, XZY, YXZ, YZX, ZXY, ZYX }`
- `buildLocalMatrix()` で適用

### 2-2 Pivot / Center Override

- `pivotPosition` (QVector3D), `pivotRotation` (QQuaternion)
- ローカル行列計算時にオフセットとして適用

### 2-3 Inherits Transform Toggle

- `setInheritsTransform(bool)` / `inheritsTransform()`
- false の場合 `worldMatrix() == localMatrix()`

### 2-4 Transform Order Control (optional)

- SRT / RTS 順序の切り替え enum

**完了条件:**
- DCC 相当の変換柔軟性が揃う
- 既存の SRT デフォルト動作は変わらない

---

## Phase 3: Scene Graph Container (~15-20h)

### 3-1 Scene Class

- ルートノードの所有権管理
- `addNode`, `removeNode`, `findNodeByPath`, `findNodeById`
- シーン全体のバウンディングボックス
- シーン全体の save/load

### 3-2 Node ID System

- UUID または採番 ID (名前は一意ではないため)
- `findById` を主経路、`findByName` は利便用として残す

**完了条件:**
- シーン単位でノード木を管理できる
- ID による安定した参照が可能

---

## Phase 4: Animation & Property Binding (~25-35h)

### 4-1 Property Binding

- `position`, `rotation`, `scale` を既存の `PropertyRegistry` に接続
- キーフレームカーブが変換値を駆動できるようにする
- 指定フレーム時間での値評価

### 4-2 Animation Clip Integration

- ノードのプロパティをターゲットにする `AnimationClip` 参照
- プロパティごとのアニメーションチャンネルマッピング

### 4-3 Dynamics Channel Attachment

- `AnimationDynamics` (spring-damper, lag follower) をノード変換に接続
- 既存の `AnimationDynamics.ixx` を配線するだけ

**完了条件:**
- SceneNode の変換がキーフレームでアニメーション可能
- dynamics チャンネルをアタッチできる

---

## Phase 5: Camera/Light as Scene Nodes (~10-15h)

### 5-1 CameraNode

- `type=Camera` の SceneNode が `Camera` インスタンスを保持
- FOV, near/far, projection type をプロパティとして公開

### 5-2 LightNode

- `type=Light` の SceneNode が `Light` インスタンスを保持
- type, color, intensity, attenuation をプロパティとして公開

**完了条件:**
- 既存の `Camera` / `Light` がシーングラフに配置できる
- viewport からシーン内のカメラ・ライトを参照できる

---

## Phase 6: Constraints & Advanced (~20-30h)

### 6-1 Constraint System

- 基本 `Constraint` クラス (`evaluate(SceneNode*)` 仮想関数)
- ParentConstraint, PointConstraint, OrientConstraint, AimConstraint

### 6-2 Tags / Metadata

- 既存の `Tag` / `MultipleTag` を SceneNode に接続
- `addTag`, `hasTag`, `removeTag`, `tags()`

### 6-3 Render Stats

- ノード単位のレンダリングフラグ: shadows, reflections, primary visibility, motion blur

**完了条件:**
- 制約付きノードが扱える
- タグ・メタデータ・レンダリング統計が設定できる

---

## 実装順

1. Phase 1: Core Foundations (type, serialization, selection/lock, display layer)
2. Phase 2: Transform (rotation order, pivot, inherits transform)
3. Phase 3: Scene Container (Scene class, node IDs)
4. Phase 4: Animation Binding (property binding, clips, dynamics)
5. Phase 5: Camera/Light Nodes
6. Phase 6: Constraints & Advanced

---

## 変更しないこと

- 既存の SRT デフォルト動作 (Phase 2 で opt-in 拡張)
- 既存の `Mesh` / `Material` のインターフェース
- 既存の `Camera` / `Light` の standalone 利用経路
- `SceneNode` の PIMPL 構造

---

## リスク

- Phase 4 で property registry と接続する際、循環依存が起きやすい → read-only adapter から始める
- Phase 3 の Scene class が既存の render pipeline と衝突しないよう、まずはデータコンテナに限定する
- Phase 5 で Camera/Light の型差異 (GLM vs Qt) を吸収する adapter が必要

---

## 関連マイルストーン

- [`M-CP-1 Camera Projection`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_CAMERA_PROJECTION_2026-03-31.md) — CameraNode と連携
- [`M-CP-2 3D Viewport Stabilization`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md) — シーングラフ統合で安定化
- [`M-LL-1 Light Linking System`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHT_LINKING_2026-03-31.md) — LightNode が前提
- [`M-MAT-1 3D Material System`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md) — MeshNode と連携
- [`M-UI-9 3D Model Review`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md) — シーングラフ表示に利用
- [`M-CE-GZ-1 ImGuizmo Direct Code Port`](X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_IMGUIZMO_DIRECT_CODE_2026-04-09.md) — SceneNode 変換操作と連携

---

## 見積合計

| Phase | 時間 | 優先度 |
|---|---|---|
| P1: Core Foundations | 20-30h | **Must** |
| P2: Transform | 15-20h | **Must** |
| P3: Scene Container | 15-20h | **Must** |
| P4: Animation Binding | 25-35h | **Important** |
| P5: Camera/Light Nodes | 10-15h | **Important** |
| P6: Constraints/Advanced | 20-30h | **Deferred** |

**合計: 105-150h**
