# MILESTONE: Shape Layer Enhancement

**Date**: 2026-04-28 / 2026-06-24 完了
**Status**: ✅ Complete
**Priority**: High

---

## 成果

### Phase 2: Shape Tool / Parameter UX
- ✅ Stroke Cap quick control (Flat/Round/Square コンボボックス)
- ✅ Stroke Join quick control (Miter/Round/Bevel コンボボックス)
- ✅ Stroke Align quick control (中央/内側/外側 コンボボックス)
- ✅ Dash Pattern quick control (カンマ区切りテキストフィールド)
- ✅ ArtifactMainWindow との双方向同期

### Phase 3: Path Editing Feel
- ✅ Bezier path vertex delete（コンテキストメニュー "Delete Point"）
- ✅ Smooth/Corner toggle（コンテキストメニュー "Make Smooth" / "Make Corner"）
- ✅ Open/Close toggle for bezier path
- ✅ Polygon ↔ Custom Path conversion（双方向コンテキストメニュー）
- ✅ Open/Close toggle for custom polygon

### Phase 4: Shape Operators
- ✅ `removeShapeOperatorAt(int index)` — 個別 operator 削除
- ✅ `moveShapeOperator(int from, int to)` — operator 並び替え
- ✅ "Manage Operators" コンテキストサブメニュー（削除・上へ・下へ）

### Phase 5: Rendering / Cache / Performance
- ⏳ 調査フェーズ — AI 主導には不向きなため別途

---

## 変更ファイル

| ファイル | 変更内容 |
|---|---|
| `ArtifactToolOptionsBar.ixx` | `setShapeOptions` に stroke cap/join/align/dash パラメータ追加 |
| `ArtifactToolOptionsBar.cppm` | 各コントロールの UI + シグナル接続 + 同期 |
| `ArtifactMainWindow.cppm` | stroke/cap/join/align/dash の双方向同期ハンドラ追加 |
| `ArtifactShapeLayer.ixx` | `removeShapeOperatorAt` / `moveShapeOperator` 追加 |
| `ArtifactShapeLayer.cppm` | 上記の実装 + dirty/mark/changed emit |
| `ArtifactRenderLayerWidgetv2.cppm` | コンテキストメニューに Path/Polygon 編集・変換・Operator管理を追加 |

## Non-Goals
- Line workflow 底上げ（別途）
- Phase 5 の詳細実装
