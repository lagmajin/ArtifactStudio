# Property Widget Row Alignment / Inspector Layout

**Date**: 2026-04-14
**Status**: Completed
**Parent**: `ArtifactPropertyWidget` / `PropertyEditor`

`ArtifactPropertyWidget` の行揃えと inspector 向けの row chrome が実装済みなので、この milestone を完了扱いにする。  
row bg / hover / keyframe 表現が owner-draw 側に寄っており、value column と keyframe / reset / badge 周りの整列も段階移行が進んでいる。

## Evidence

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `docs/planned/MILESTONES_BACKLOG.md`

## Result

- row bg / hover / keyframe chrome が owner-draw 化されている
- keyframe / reset / badge / value column の整列が inspector 方向に寄っている
- property editor row の見た目責務が widget 側へ寄っている
