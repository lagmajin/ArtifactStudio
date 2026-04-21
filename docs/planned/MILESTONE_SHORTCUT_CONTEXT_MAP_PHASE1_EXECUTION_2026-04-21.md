# M-SC-2 Phase 1 Execution
**作成日:** 2026-04-21

## Execution Notes

- `InputOperator::activeContext()` は workspace / modal の最終解決に使う
- widget-specific keymap は `registerWidgetKeyMap(QWidget*, KeyMap*)` を継続利用する
- 既存の menu / shortcut / playback routing は壊さず、context 名だけを先に固定する
- まずは `Global`, `Workspace.Timeline`, `Viewport.Composition`, `Panel.LayerTree`, `Panel.AssetBrowser`, `Panel.Inspector` を最低限にする
