# Milestone: Photoshop-like Image Editing in ArtifactCore (2026-04-11)

**最終更新:** 2026-08-15

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

## 2026-07-25 実装監査

`ArtifactPaintLayer`／`ArtifactBrushTool` のブラシ・消しゴム、フレーム別バッファと undo、`LayerMask`、`ArtifactCloneLayer`、多数の `ArtifactCore` ImageProcessing／OpenCV フィルター、Color Correction 系エフェクト、既存レイヤー／マスク導線は確認した。一方、pressure／tilt 入力、独立した `BrushEngine`／`SelectionEngine`／`FilterEngine`／`AdjustmentEngine`／`LayerEffectEngine` API、矩形・楕円・lasso 選択、クローン stamp の直接編集、Photoshop 風の統合 UI、GPU／CPU 選択と性能一致は確認できない。したがって基盤機能は分散して部分実装されているが、本マイルストーンの統合完了と success criteria は未達・未検証とする。

## Update 2026-08-15

現行コードを再確認した。`ArtifactPaintLayer` はフレーム単位の画像バッファ、ブラシストローク、消しゴム相当のストローク、undo、JSON化を持ち、`ArtifactBrushTool` から編集操作へ接続されている。`applyCloneStampAtFrame()`／`applyCloneStampFromLayerAtFrame()` と `ArtifactCloneLayer` により、クローン処理の実装も存在する。`LayerMask` はマスクパスの追加・削除・有効／ロック状態・アルファ合成・画像適用を実装している。

また、`ArtifactCore` の OpenCV フィルター群、`OpenCVRotoBrushEngine` のフレーム更新・マスク伝播・マスク精緻化、`ArtifactCreativeEffects` のカラー補正／クローン系処理は利用可能な基盤として確認できる。これは「画像へ処理を適用する」機能の蓄積であり、Phase 1・3 の一部とマスク系の基礎は前進している。

ただし、独立した `BrushEngine`／`SelectionEngine`／`FilterEngine`／`AdjustmentEngine`／`LayerEffectEngine` の公開契約、矩形・楕円・lasso 選択を画像編集へ接続する導線、非破壊の調整レイヤー／効果スタック、pressure／tilt 対応、Photoshop風の統合UI、GPU／CPUバックエンドの選択と性能検証は未確認である。現状の判定は「画像ペイント・マスク・フィルターの分散実装は進行、統合編集環境は未完了」とする。

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
