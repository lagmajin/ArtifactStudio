# MILESTONE: 3D Material System

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