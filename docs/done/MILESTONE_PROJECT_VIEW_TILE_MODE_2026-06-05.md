# Project View Tile Mode

**Date**: 2026-06-05
**Status**: Completed
**Parent**: `ArtifactProjectManagerWidget`

`ArtifactProjectManagerWidget` に `List / Tile` 切替を載せ、Project View を構造確認のまま tile presentation でも扱えるようにした。選択同期を壊さず、`ArtifactProjectView::PresentationMode::Tile` に接続されている。

## Evidence

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx`

## Result

- list / tile を切り替えられる
- selection と current item が保持される
- tile mode の presentation state が view に反映される

