# MILESTONE: 3D Material System

**Status:** Phase 1〜4 implemented, Phase 5/runtime verification pending

> 2026-03-31 作成

## 目的

3D objects の material を定義し、適切な shading と appearance を実現する。

## 背景

3D rendering の質を決める重要な要素が material system。
Primitive 3D Render Path の solid shading の続きとして、material による見た目の制御が必要。
基本的な diffuse/specular/reflection のサポートから始め、texture mapping も含む。

## 対象

- Basic material properties (diffuse color, specular, roughness)
- Texture mapping (diffuse map, normal map)
- Material assignment to 3D objects
- Simple PBR-like shading
- Material inspector integration

## 実装方針

### 原則

1. Material を独立した asset として扱う
2. 3D layer に material を assign
3. GPU shader での material application
4. シンプルな UI から始める

### 対象 API

- `ArtifactCore::Material3D` class
- `Artifact3DLayer::setMaterial()`
- Shader の material parameter

## Phase 1: Basic Material Properties

### 目的

Material の基本 properties を定義する。

### 作業項目

- Material class の作成 (diffuse, specular, roughness)
- Color picker integration
- Material asset management

### 完了条件

- Material の color properties を設定できる

## Phase 2: Texture Support

### 目的

Texture mapping を追加する。

### 作業項目

- Diffuse texture assignment
- Texture coordinate generation
- UV mapping basics

### 完了条件

- 3D objects に texture を適用できる

## Phase 3: Shader Integration

### 目的

Material を GPU shader で使用する。

### 作業項目

- Material parameter を shader に渡す
- Basic lighting calculation
- Material switching in renderer

### 完了条件

- Material が rendering に反映される

## Phase 4: Material Assignment

### 目的

3D layer に material を assign する。

### 作業項目

- 3D layer の material property
- Material browser integration
- Default material handling

### 完了条件

- 各 3D object に異なる material を設定できる

## Phase 5: Advanced Features

### 目的

高度な material features を追加する。

### 作業項目

- Normal mapping
- Specular mapping
- Material presets

### 完了条件

- 基本的な PBR material が使用可能

## 連携先

- `ArtifactCore/include/Material/Material3D.ixx`
- `ArtifactCore/src/Material/Material3D.cppm`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Next Execution Slice

Phase 1 は material の器を先に作り、見た目の自由度を少しずつ上げる。

### Phase 1A の着手点

1. `Material3D` の基本 property を切り出す
2. diffuse / specular / roughness を最小セットとして定義する
3. material assignment の前に inspector 側の編集導線を確認する
4. 既存の solid 表示を壊さない範囲で導入する

### Phase 1 完了条件

- material の色属性を設定できる
- material の基本値が 1 つの構造にまとまる
- solid 表示の既存挙動を壊さない

### Phase 2 の前提

- texture support は material の基本属性が安定してから入れる
- shader integration は parameter 契約が固まってから進める
- material assignment は editor 側の編集導線が見えてから重ねる

### Phase 3 への波及

- renderer に渡す material parameter の形を先に揃える
- camera / projection と material の責務を混ぜない
- simple PBR-like shading は後段で検討する

## Implementation Status (2026-07-25)

- `ArtifactCore::Material` が base color、metallic、roughness、opacity、emission、normal/occlusion を保持する。
- `Artifact3DModelLayer` が material を保存・復元し、Property Editor の編集値を反映する。
- base-color、metallic-roughness、normal、emission、occlusion、opacity texture の読み込み経路がある。
- `MeshRenderer` が material constant buffer と PBR-like shader を使用して描画する。
- Phase 5 の高度な normal/specular mapping、preset UI、および実機確認は未完了。

## Static audit follow-up (2026-07-25)

- The implemented core type is `ArtifactCore::Material` (not the originally proposed `Material3D` name). It is assigned by `Artifact3DModelLayer`, persisted in JSON, exposed through the existing Property Editor, and included in the layer material signature/cache boundary.
- `MeshRenderer` binds base-color, opacity, metallic-roughness, normal, emission, and occlusion textures and sends the material factors through a GPU constant buffer. The shader applies tangent-space normal mapping, metallic/roughness response, occlusion, emission, alpha, and scene/studio lighting.
- Phases 1-4 are supported by static source evidence. The remaining scope is Phase 5 preset/material-browser work plus runtime verification; no build or runtime execution was performed under the repository policy.
