# Keyboard Overlay Milestone (2026-03-30)

`KeyboardOverlayDialog` を、単なる空ダイアログではなく、アプリ内ショートカットを俯瞰できる軽量 overlay として完成させるためのマイルストーン。
`ApplicationSettingDialog` の shortcut 一覧や `PlaybackShortcuts` の実装とつなぎ、操作を覚えやすくする。

## Goal

- 現在のショートカットを一覧で見られるようにする
- カテゴリ別に整理された compact overlay を表示できるようにする
- `Help` メニューや `Ctrl+/` 系の導線からすぐ開けるようにする
- 設定画面の shortcut 一覧とデータを共有する
- 単なるヘルプではなく、制作中に参照しやすい常駐寄りの UI にする

## Scope

- `ArtifactWidgets/src/Dialog/KeyboardOverlayDialog.cppm`
- `ArtifactWidgets/include/Dialog/KeyboardOverlayDialog.ixx`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/Menu`
- `Artifact/src/Service/ArtifactPlaybackShortcuts.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`

## Non-Goals

- 完全な command palette の置き換え
- ショートカット編集機能の全面実装
- Keymap システムの再設計
- 単なる static help page 化

## Phases

### Phase 1: Dialog Core

- `KeyboardOverlayDialog` の UI を実装する
- compact / regular の表示モードを用意する
- overlay opacity と always-on-top を実際に反映する

### Phase 2: Shortcut Data Binding

- `PlaybackShortcuts` から shortcut data を集約する
- `ApplicationSettingDialog` の一覧と表示元を揃える
- action / shortcut / category / description を表示する

### Phase 3: App Integration

- `Help` メニューから開けるようにする
- 必要なら `Ctrl+/` や `F1` 系の導線を追加する
- modal にしすぎず、作業を止めない表示を目指す

### Phase 4: Search / Filter / Focus

- shortcut 名や action 名で絞り込めるようにする
- カテゴリごとに折りたためるようにする
- よく使う shortcut を先頭固定できるようにする

## Recommended Order

1. `Phase 1: Dialog Core`
2. `Phase 2: Shortcut Data Binding`
3. `Phase 3: App Integration`
4. `Phase 4: Search / Filter / Focus`

## Current Status

- `KeyboardOverlayDialog` は宣言されているが、中身は未実装に近い
- 起動導線もまだメイン UI に繋がっていない
- shortcut のデータ自体は既にアプリ内に存在するので、まずは表示層を作るのが最短

## Validation Checklist

- overlay を開くと主要 shortcut がカテゴリ別に見える
- 設定画面と表示内容が食い違わない
- 画面を邪魔しすぎず、必要時にすぐ参照できる
- compact モードで制作中でも読みやすい
- ヘルプ的だが、制作中の参照にも耐える
