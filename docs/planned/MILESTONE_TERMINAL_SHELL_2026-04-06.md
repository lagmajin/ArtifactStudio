# Terminal Shell / Command Surface Milestone

Artifact に、今のデバッグコンソールとは別の、`cmd` / `PowerShell` っぽい対話ターミナルを用意する milestone.

## Goal

- デバッグログ表示とは別に、ユーザーがコマンドを打てる実用的なターミナル surface を持つ
- `cmd` / `PowerShell` / 可能なら `sh` のような外部コマンド実行を統一的に扱う
- ターミナルの出力、終了コード、実行中状態、履歴を UI で見やすくする
- 将来的に project tools / batch helpers / scripts / diagnostics の実行窓口に育てる

## Scope

- `Artifact/include/Widgets/PowerShellWidget.ixx`
- `Artifact/src/Widgets/PowerShellWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactConsoleWidget.cppm`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets` 配下の menu / dock / layout 連携

## Non-Goals

- 完全な OS terminal emulator の実装
- PTY ベースの高度な入出力制御
- shell 内部の補完エンジンや VT100 完全再現

## Background

現状すでに `PowerShellWidget` は存在するが、用途としてはまだ簡易なコマンド実行 UI に近い。
一方で、デバッグコンソールはログ閲覧の役割が強いので、
「見るための console」と「打つための terminal」を分けたほうが運用しやすい。

この milestone は、既存の `PowerShellWidget` をベースにしつつ、
ターミナルっぽい操作感と履歴・出力管理を整えることを狙う。

## Proposed Model

- `TerminalSession`
  - shell 種別、working directory、環境変数、実行状態
- `TerminalCommand`
  - 入力されたコマンドと実行結果
- `TerminalHistory`
  - 再実行・履歴検索・保存対象
- `TerminalProfile`
  - `cmd` / `PowerShell` / `sh` などの起動設定

## Phases

### Phase 1: Shell Surface

- `PowerShellWidget` をターミナル surface として整える
- コマンド入力欄、実行ボタン、停止ボタン、クリアボタンを整理する
- 現在の working directory を表示する
- 実行中状態と終了コードを表示する

### Phase 2: Session / History

- 履歴の上下移動と検索
- 複数コマンドの再実行
- shell profile の切り替え
- 実行ごとの output を折りたたみ表示できるようにする

### Phase 3: App Integration

- menu / shortcut / dock から terminal を開けるようにする
- debug console と役割分担する
- project tools / script runner / diagnostics への導線を足す

### Phase 4: Developer Workflow

- build / test / git / asset tool などの定型コマンドを扱いやすくする
- お気に入りコマンドやテンプレートを保存する
- 複数コマンドを順番に流す簡易 batch 機能を足す

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

## Current Status

- `Artifact/src/Widgets/PowerShellWidget.cppm` が既にある
- ただしまだ debug console と分離された「本格 terminal surface」としては未整理
- まずは Phase 1 の UI / 状態表示を固めるのが自然

