# Asset System

**Date**: 2026-03-12
**Status**: Completed
**Parent**: `ArtifactProjectManagerWidget` / `ArtifactAssetBrowser`

`ArtifactAssetBrowser` と `ArtifactProjectManagerWidget` を同じ asset system の別面として揃え、browser ↔ project の同期と状態表示を実用ラインまで通した。

## Evidence

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`

## Result

- browser ↔ project selection sync がある
- sync chip が両側にある
- missing / unused / relink 状態が読める
- import / metadata / relink / recovery の導線がある

