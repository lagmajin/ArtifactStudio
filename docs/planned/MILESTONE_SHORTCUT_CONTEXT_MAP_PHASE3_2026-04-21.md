# M-SC-2 Phase 3 Preset / Editor Integration
**作成日:** 2026-04-21

## Goal

context 別 keymap を preset / editor UI から扱えるようにする。

## Tasks

- `Blender / Default / Custom / Workspace` preset を context ベースで保存する
- `ArtifactShortcutEditorDialog` から widget / region ごとの keymap を編集する
- 競合表示と revert を入れる
- `loadPreset()` の適用単位を明確にする

## Output

- context 別 shortcut editor
- preset の保存/読み込みルール
- workspace 単位の keymap 差し替え手順
