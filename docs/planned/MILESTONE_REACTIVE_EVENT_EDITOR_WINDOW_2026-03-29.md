# Reactive Event Editor Window Milestone

Reactive Event System のルールを編集するための独立ウィンドウを作るマイルストーン。

最初は独立ウィンドウとして始め、後から Inspector や Composition Editor へ統合できる前提で設計する。
左ペインだけは owner-draw を前提にし、それ以外は既存の Qt ウィジェットで組む方針にする。

## Goal

- ルール、条件、アクション、ログを 1 画面で扱えるようにする
- Target Tree / Rule List / Inspector / Event Log を同時に見られるようにする
- reactive event の追加、無効化、並べ替え、診断を行えるようにする
- Core の `ReactiveEvents` 定義と UI をつなぐ

## Initial Layout

```text
+---------------------------------------------------------------+
| Target Tree | Event Rules                     | Inspector      |
|-------------+---------------------------------+----------------|
| Comp A      | [x] FadeInTrigger               | Name: ...      |
|  └ Layer 1  | When: Opacity > 0.5             | Condition: ... |
|  └ Layer 2  | Do:   Enable Glow on Layer 2    | Action: ...    |
|  └ Layer 3  |---------------------------------| Cooldown: 0    |
|             | [ ] OutOfFrameBreak             | Last Fired:120 |
|             | When: Out Of Frame              | Source: Layer1 |
|             | Do:   Trigger Fracture          | Target: Layer3 |
+---------------------------------------------------------------+
| Event Log: [120] FadeInTrigger fired ...                      |
+---------------------------------------------------------------+
```

## Scope

- `Artifact/src/Widgets/ReactiveEventEditorWindow.cppm`
- `Artifact/src/Widgets/ReactiveEventEditorWindow.ixx`
- `Artifact/src/Widgets/ReactiveEventRuleListWidget.cppm`
- `Artifact/src/Widgets/ReactiveEventTargetTreeWidget.cppm`
- `Artifact/src/Widgets/ReactiveEventInspectorWidget.cppm`
- `Artifact/src/Widgets/ReactiveEventLogWidget.cppm`
- `ArtifactCore/include/Reactive/ReactiveEvents.ixx`
- `ArtifactCore/src/Reactive/ReactiveEvents.cppm`

## Non-Goals

- reactive engine の実評価ロジックをここで完成させること
- composition editor への完全統合を最初から目指すこと
- 複雑なノードグラフ化をいきなり入れること

## Phase 1: Basic Window

- 独立ウィンドウとして開ける
- Target Tree を左ペインの owner-draw で表示する
- Event Rules を既存 Qt の list/tree/table で一覧表示する
- Inspector で選択中ルールの詳細を編集する
- Event Log を下部に出す

## UI Direction

- 左ペイン
  - owner-draw で階層、状態、選択、バッジを強く見せる
- 中央ペイン
  - 既存 Qt ウィジェットで rule list / editor を組む
- 右ペイン
  - 既存 Qt ウィジェットで inspector を組む
- 下ペイン
  - event log は軽量な Qt widget で十分

## Phase 2: Rule Editing

- ルール追加 / 削除 / 複製
- enabled 切替
- condition / action の編集
- cooldown / delay / once の編集

## Phase 3: Diagnostic Visibility

- source / target / last fired を表示する
- どの条件で発火したか分かるようにする
- エラーや無効参照をログに出す

## Phase 4: Integration Hooks

- composition / layer selection と同期する
- Inspector / Layer Solo View から開けるようにする
- 将来は dockable 化できるようにする
