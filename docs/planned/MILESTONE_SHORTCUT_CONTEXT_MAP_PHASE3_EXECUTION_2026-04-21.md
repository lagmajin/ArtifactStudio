# M-SC-2 Phase 3 Execution
**作成日:** 2026-04-21

## Execution Notes

- `KeyMap::loadPreset("Blender", inputOp)` を workspace preset の入口にする
- editor UI は検索、競合検出、再割当、default revert の 4 機能をまず持つ
- `ShortcutBindings` は共通の軽量ショートカットに残し、context 別のものは `InputOperator` 側で管理する
