# UI Layout Undo History

**Date**: 2026-07-07
**Status**: Completed
**Parent**: `Window / Dock Layout / UndoManager`

UI Layout Undo History が実装済みのため、この milestone を完了扱いにする。  
`closeDock` / `setDockVisible` / `moveDockToTabGroup` / `setDockSplitterSizes` のレイアウト変更が snapshot として undo 層に流れ、dock レイアウトの戻し操作ができるようになった。

## Evidence

- `Artifact/include/Undo/UndoManager.ixx`
- `Artifact/src/Undo/UndoManager.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactUndoHistoryWidget.cppm`
- `docs/planned/MILESTONE_UI_LAYOUT_UNDO_HISTORY_2026-06-07.md`

## Result

- panel close が undo 対象になった
- tab move が undo 対象になった
- splitter 変更が undo 対象になった
- undo history widget から履歴を確認できる
