# Milestone: Photoshop-like Image Editing in ArtifactCore (2026-04-11)

## Overview
ArtifactCore に Photoshop ライクな画像編集機能を追加し、レイヤーベースの画像編集を強化する。主にブラシツール、選択ツール、フィルター、色調整、レイヤー効果を実装。

## Goals
- 基本的なペイントツール (ブラシ、消しゴム、クローン)
- 選択とマスク機能
- リアルタイム適用可能なフィルター (ぼかし、シャープネス、ノイズなど)
- 色調整ツール (レベル、カーブ、色相/彩度)
- レイヤー効果 (ドロップシャドウ、グロー、ベベル)

## Implementation Phases

### Phase 1: Core Painting Tools
- `BrushEngine`: ブラシペイントの基盤 (ソフトネス、ハードネス、サイズ、圧力対応)
- Brush types: Hard brush, Soft brush, Texture brushes
- Pressure sensitivity and tilt support
- Blend modes: Normal, Multiply, Screen, Overlay, etc.
- `EraserTool`: 消しゴム機能 (brushのopacity inverse)
- `CloneStampTool`: クローンツール (source point指定)
- API: `paintOnLayer(layer, brush, position, pressure)`

### Phase 2: Selection and Masking
- `SelectionEngine`: 矩形/楕円選択、ラスー選択
- `Mask`: レイヤーマスクと選択マスクの統合
- API: `createSelection(shape)`, `applyMask(layer, mask)`

### Phase 3: Filters and Adjustments
- `FilterEngine`: ぼかし、シャープネス、ノイズ、芸術的フィルター
- `AdjustmentEngine`: レベル、カーブ、色相/彩度調整
- OpenCV/G-API との統合強化
- API: `applyFilter(layer, filterType, params)`, `applyAdjustment(layer, adjustmentType, params)`

### Phase 4: Layer Effects
- `LayerEffectEngine`: ドロップシャドウ、インナーグロウ、ベベルなど
- GPU/CPU バックエンドの選択
- API: `addLayerEffect(layer, effectType, params)`

## Dependencies
- OpenCV for image processing
- DiligentEngine for GPU acceleration
- Existing `CreativeEffect` framework

## Estimation
- Phase 1: 20-30h
- Phase 2: 15-20h
- Phase 3: 25-35h
- Phase 4: 20-30h

Total: 80-115h

## Success Criteria
- 基本的なPhotoshop風編集が可能
- パフォーマンスがレイヤー編集に影響しない
- UI統合が容易なAPI設計