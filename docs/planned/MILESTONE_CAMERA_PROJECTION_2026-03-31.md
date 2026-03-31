# MILESTONE: Camera Projection Integration

> 2026-03-31 作成

## 目的

3D rendering のために camera の projection を適切に扱い、perspective / orthographic の両方をサポートする。

## 背景

現在の Artifact では 2D カメラが主だが、3D レイヤーを追加するにあたって camera の projection matrix を扱う必要がある。
Primitive 3D Render Path の続きとして、camera と 3D viewport の基盤を作る。

## 対象

- Perspective projection (FOV, aspect ratio)
- Orthographic projection (ortho width/height)
- Camera frustum (near/far clip planes)
- Viewport integration (render target size)
- View / Projection matrix の計算

## 実装方針

### 原則

1. Camera layer を拡張して 3D projection を追加
2. 既存の 2D camera と共存させる
3. Diligent の matrix 計算を利用
4. viewport size と連動

### 対象 API

- `ArtifactCameraLayer` の拡張
- `ArtifactIRenderer::setViewProjectionMatrix()`
- Composition renderer の camera 統合

## Phase 1: Camera Layer Projection Mode

### 目的

Camera layer に projection mode を追加する。

### 作業項目

- `ProjectionMode` enum (Perspective, Orthographic)
- Camera layer の projection properties 追加
- FOV, near/far clip, ortho size の設定

### 完了条件

- Camera layer の Inspector で projection mode を選択できる

## Phase 2: Matrix Calculation

### 目的

View / Projection matrix を正しく計算する。

### 作業項目

- Perspective matrix の計算 (FOV, aspect, near/far)
- Orthographic matrix の計算 (width, height, near/far)
- View matrix の計算 (camera position/rotation)

### 完了条件

- カメラの transform から正しい matrix が生成される

## Phase 3: Renderer Integration

### 目的

Renderer に projection matrix を渡す。

### 作業項目

- `ArtifactIRenderer` に projection matrix 設定 API 追加
- `CompositionRenderController` で camera matrix を適用
- 3D レイヤーの描画で camera matrix を使用

### 完了条件

- 3D レイヤーが camera の projection で正しく表示される

## Phase 4: Viewport Sync

### 目的

Viewport size と projection を同期する。

### 作業項目

- Aspect ratio の自動計算
- Viewport resize 時の matrix 更新
- Composition size と camera の連動

### 完了条件

- Window resize で projection が正しく更新される

## Phase 5: Gizmo Integration

### 目的

3D gizmo を camera projection で表示する。

### 作業項目

- Gizmo renderer の matrix 同期
- Camera frustum visualization (optional)

### 完了条件

- 3D gizmo が camera view で正しく表示される

## 連携先

- `Artifact/src/Layer/ArtifactCameraLayer.cppm`
- `Artifact/include/Layer/ArtifactCameraLayer.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5