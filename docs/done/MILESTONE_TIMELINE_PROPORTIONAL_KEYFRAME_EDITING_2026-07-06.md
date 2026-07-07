# Timeline Proportional Keyframe Editing

**Date**: 2026-07-06
**Status**: Completed
**Window**: Current dev branch
**Parent**: `M-TL-17 Timeline Proportional Keyframe Editing`

`ArtifactTimelineTrackPainterView` に Blender 風の proportional editing を導入し、selected keyframe の time move と area drag / edge resize に減衰を配る導線を実装した。

## Evidence

- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm`

## Result

- `O` で on/off を切り替えられる
- `[` `]` で半径を変えられる
- marker drag で周辺 keyframe に weight を配れる
- area drag / edge resize でも proportional falloff が効く

