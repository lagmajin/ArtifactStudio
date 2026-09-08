# BlenderライクKeymap Settings / Context実装ログ

**最終更新:** 2026-09-07

## 対象

1. Settings内のKeymapページ
2. Viewport／Timelineの入力コンテキスト判定
3. Composition ViewportのBlenderライクな変形モーダル

## 実装

- 既存 `ShortcutBindings` の編集表とJSON preset経路を再利用。
- preset選択、context列／絞り込み、検索、同一context競合表示を追加。
- Blender presetでは既存actionに対応済みの選択・手・ズーム・Timeline slide・Fitキーだけを変更。Viewportの`R`はtransform modal用に予約し、Rotate Toolの単独キーは空にする。
- `InputOperator::processKeyPress(QWidget*, ...)` にfocus ownerとactive modalの境界を追加。文字入力、数値入力、combo、buttonでは未修飾の単一キーをsurface shortcutへ流さない。
- Composition viewportの既存 `Viewport.Composition` 経路を維持。
- Timeline右ペインに `Panel.Timeline.Right` focus contextを追加。左ペインの既存 `Panel.Timeline.Left` は維持。
- `ArtifactCompositionRenderWidget` は選択レイヤーを対象に、既存の`CompositionRenderController` gizmo modal経路へ`G`（移動）／`R`（回転）／`S`（拡大縮小）を接続した。
- モーダル中は`Modal.Transform` contextを有効にし、`X`／`Y`／`Z`で軸拘束、数値・小数・符号入力とBackspaceで数値指定、Shiftで精密操作（マウス差分0.1倍）、Ctrlでスナップ、Enterまたは左クリックで確定、Escまたは右クリック、focus離脱で取消する。マウス移動は既存gizmoへ渡す。
- 確定時は既存の`GizmoTransformUndoCommand`または`GizmoGroupTransformUndoCommand`を1回だけpushする。キャンセル時は開始時のtransform値とkeyframe状態を復元し、Undo履歴を追加しない。

## 対象外／確認待ち

- context metadataは現行 `ShortcutId` の責務に基づくUI分類。将来action registryへ統合する余地がある。
- shortcut値の永続化は既存JSON import/exportと実行中singletonの範囲を維持。自動保存形式の拡張は行っていない。
- ユーザー指示によりビルド、テスト、実画面検証は未実施。
