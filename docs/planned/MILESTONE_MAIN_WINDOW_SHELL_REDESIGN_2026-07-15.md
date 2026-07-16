# Main Window Shell Redesign (2026-07-15)

**ステータス:** Implemented — runtime verification pending

## Goal

Composition Viewer を QADS の可動Dockではなく、常設の中央 workspace host に固定する。Project、Inspector、Timeline、補助ビューだけを周辺Dockとして永続化し、保存レイアウトが中央ビューをフローティング化・置換しないようにする。

## Scope

- `ArtifactMainWindow` の中央領域を構造固定の `ArtifactCentralDock` とする。
- Composition Viewer は `ArtifactCentralWidgetHost` の直接の子として管理する。
- Software Composition View、Layer Solo View、Layer View (Software)、Contents Viewer を中央タブ群から外す。
- 既存のDock保存状態を新layout versionで無効化する。

## Non-Goals

- QADS本体のforkや変更。
- Project View、Timeline、Inspectorの責務変更。
- 新しいsignal/slot経路の追加。

## Acceptance Criteria

- 新規コンポジション作成後、Composition Viewerはメインウィンドウ中央に残る。
- 保存レイアウトの復元でComposition Viewerが独立ウィンドウにならない。
- Timelineは下部Dock、補助ビューは周辺Dockとして表示される。
- 中央Viewerを開くための新しい浮動Widgetや空のArtifactウィンドウが生成されない。

## Implementation Notes

- 2026-07-15: `setCentralWorkspace()` を追加し、Composition ViewerをQADS Dock widgetから分離した。
- 2026-07-15: layout versionを11へ更新し、旧Dock graphを再利用しないようにした。
- 2026-07-15: 中央workspaceは可視状態を維持し、workspace mode切替でも空の中央領域にならないようにした。
- 2026-07-15: 全パネル表示切替・一括closeでも中央workspaceを対象外にした。

## Verification Pending

- Windows runtimeでの新規コンポジション作成、再起動、Dock復元。
- Composition Viewerの初期表示、Timeline下部表示、補助ビュー表示。
