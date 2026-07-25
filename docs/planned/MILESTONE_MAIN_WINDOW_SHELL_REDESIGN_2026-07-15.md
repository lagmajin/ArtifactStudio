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

## 2026-07-25 実装監査

- `ArtifactMainWindow` は `ArtifactCentralDock` と `ArtifactCentralWidgetHost` を作成し、Composition Viewer を中央 host の子として `setCentralWorkspace()` で管理している。
- `AppMain.cppm` の layout version は 11 で、旧 layout state を破棄してから新しい dock state を復元する経路が存在する。
- 中央 workspace は `setDockVisible()`／`activateDock()`／workspace mode 切替時にも空にしない保護が実装されている。
- Timeline の下部 Dock と Contents Viewer などの周辺 Dock 登録もコード上で確認できる。
- 新規コンポジション作成後の実ウィンドウ表示、再起動後の保存レイアウト復元、浮動化・空ウィンドウ生成の不在は runtime 未検証のため、ステータスは `Implemented — runtime verification pending` のままとする。
