# MILESTONE: Light Linking System

> 2026-03-31 作成

## 目的

3D scene での light の影響を layer ごとに制御し、柔軟な lighting setup を実現する。

## 背景

3D rendering では、特定のライトが特定のオブジェクトにのみ影響を与える "Light Linking" が重要。
Maya や Unreal Engine のようなツールで標準的な機能だが、Artifact ではまだ実装されていない。
3D レイヤーとライトレイヤーの統合において、lighting control の基盤が必要。

## 対象

- Light-to-Object linking (include/exclude)
- Light groups and categories
- Per-layer light influence control
- Light linking visualization
- Default lighting behavior

## 実装方針

### 原則

1. Light layer を拡張して linking properties を追加
2. 3D model layer に light influence を設定
3. デフォルトは全てのライトが全てのオブジェクトに影響
4. UI はシンプルに保つ

### 対象 API

- `ArtifactLightLayer` の linking properties
- `Artifact3DLayer` の light influence settings
- Renderer の light filtering

## Phase 1: Light Layer Linking Properties

### 目的

Light layer に linking control を追加する。

### 作業項目

- Light layer の "Include List" / "Exclude List" properties
- Layer ID のリスト管理
- "Link All" / "Link None" モード

### 完了条件

- Light layer の Inspector で linking を設定できる

## Phase 2: 3D Layer Light Influence

### 目的

3D layer 側からも light influence を制御する。

### 作業項目

- 3D layer の "Affected By Lights" property
- Light layer の選択 UI
- Light group 概念の導入 (optional)

### 完了条件

- 3D layer がどのライトに影響されるかを設定できる

## Phase 3: Renderer Light Filtering

### 目的

Renderer で light linking を適用する。

### 作業項目

- Light list の filtering ロジック
- Composition renderer の light application 更新
- 3D layer 描画時の light filtering

### 完了条件

- Light linking が rendering に反映される

## Phase 4: UI Integration

### 目的

Light linking を視覚的に管理する。

### 作業項目

- Linking visualization (outliner での表示)
- Quick linking shortcuts
- Linking state の feedback

### 完了条件

- Light linking の状態が UI でわかりやすい

## Phase 5: Advanced Features

### 目的

高度な lighting control を追加する。

### 作業項目

- Light groups (tag-based grouping)
- Light intensity per object
- Shadow linking (separate from light linking)

### 完了条件

- 複雑な lighting setup が可能

## 連携先

- `Artifact/src/Layer/ArtifactLightLayer.cppm`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Layer/ArtifactLightLayer.ixx`
- `Artifact/include/Layer/Artifact3DModelLayer.ixx`

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5