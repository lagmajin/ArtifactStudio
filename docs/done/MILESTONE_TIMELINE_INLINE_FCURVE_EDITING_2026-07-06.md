# Timeline Inline F-Curve Editing

**Date**: 2026-07-06
**Status**: Completed
**Window**: Current dev branch
**Parent**: `M-IC Timeline Inline F-Curve Editing`

`ArtifactTimelineTrackPainterView` に、inline F-curve 表示と右ペイン内のベジェ関連表示を統合した。既存の curve editor 側のハンドル表現と整合する形で、タイムライン marker のツールチップと描画を拡張している。

## Evidence

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`
- `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm`
- `Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`

## Result

- keyframe marker の tooltip に bezier state が出る
- selected keyframe に bezier handle が描画される
- handle の hit test / edit 導線が既存 timeline surface に載っている

