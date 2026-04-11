# レイヤー自動配置ユーティリティの実装
**マイルストーン**: M-LA-2 Layer Auto-Arrange Utilities
**作成日**: 2026-04-10
**見積もり**: 6-8h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のレイヤーパネルで、複数のレイヤーを素早く整列・配置できるユーティリティ。
散らかったレイヤーを一瞬で整理できるようにする。

## 機能仕様

### 配置コマンド (Layer > Arrange)
- `Bring to Front` (最前面へ)
- `Bring Forward` (1つ前面へ)
- `Send Backward` (1つ背面へ)
- `Send to Back` (最背面へ)

### 整列コマンド (Layer > Align)
- `Align Left Edges`
- `Align Horizontal Centers`
- `Align Right Edges`
- `Align Top Edges`
- `Align Vertical Centers`
- `Align Bottom Edges`

### 分布コマンド (Layer > Distribute)
- `Distribute Horizontal Centers`
- `Distribute Vertical Centers`
- `Distribute Spacing` (等間隔配置)

### 実装要件
- 複数レイヤー選択対応
- アンカーポイント基準の整列
- バウンディングボックス考慮
- undo/redo 対応

### 実装場所
- `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- キーボードショートカット対応

## 技術的考慮
- 選択レイヤーの座標計算
- 親レイヤーのトランスフォーム考慮
- パフォーマンス: 大量レイヤー対応

## AEとの差別化
- より詳細な分布オプション
- キーボードショートカット充実
- 整列時のプレビュー表示

## テストケース
- 複数レイヤーの正確な整列
- 親子関係のあるレイヤーの処理
- undo/redo の動作確認