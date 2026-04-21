# M-SC-2 Phase 1 Context Model Freeze
**作成日:** 2026-04-21

## Goal

`InputOperator` の context 解決順と命名規約を固定する。

## Tasks

- `Modal -> Widget.Mode -> Widget -> Workspace -> Global` の優先順位を固定する
- `KeyMap::context()` に入れる命名規約を決める
- `Global / Workspace / Widget / Modal` の役割を文書化する
- 既存の `ShortcutBindings` と衝突しない共通ルールを作る

## Output

- context 名の一覧
- 優先順位の説明
- どの UI がどの階層に属するかの簡易表
