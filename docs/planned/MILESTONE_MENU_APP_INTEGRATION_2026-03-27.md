# Menu to App Integration Milestone

`File` / `Composition` / `Edit` / `View` / `Layer` / `Render` などの各メニューを、単独の action 群ではなくアプリ層の command routing 入口として整理するためのマイルストーン。

この文書は、各メニューの UI 文言や配置よりも、**どのサービス・どの state・どの command を触るか** を先に固定することを目的にする。

## Goal

- 各メニューがアプリ層の正しい command へ接続される
- enabled / disabled / checked state が project / selection / playback / clipboard / undo stack に追従する
- メニュー側にビジネスロジックを溜めない
- menu action と shortcut が同じ経路を使う
- playback / navigation shortcut は playback service に集約する

## Scope

- `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactHelpMenu.cppm`
- `Artifact/src/Widgets/Menu/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Service/*`

## Non-Goals

- メニューバーの全面再設計
- 全 action の見た目変更
- keyboard shortcut 体系の全面変更
- context menu の完全統一を一度にやること

## Background

現状は File / Composition / Edit / View / Layer / Render / Help それぞれに個別の整備が進んでいるが、アプリ層との接続条件はまだ分散しやすい。

たとえば `project created` なのか、`current composition` があるのか、`selection` があるのか、`undo stack` が空か、`clipboard` が空か、`playback` 中か、という判定はメニューごとに持つより、再利用できる形で集約した方が安全。

## Phases

### Phase 1: Command Ownership Map

- 目的:
  - 各メニュー action の所有者を固定する

- 作業項目:
  - File / Composition / Edit / View / Layer / Render / Help の action inventory を作る
  - 各 action が `Service` / `Project` / `Playback` / `Selection` / `UI` のどれに属するか決める
  - placeholder action を明示して未実装と実装済みを分ける

- 完了条件:
  - action ごとに責務の持ち主が説明できる
  - menu 側に残すロジックが最小限になる

### Phase 2: Context-Aware Enable State

- 目的:
  - メニュー表示時に正しい enabled state を出す

- 作業項目:
  - project 有無
  - current composition 有無
  - current layer / selection 有無
  - clipboard state
  - undo stack state
  - playback state

- 完了条件:
  - project があるだけで全部有効にならない
  - action ごとの無効条件が一貫する

### Phase 3: Shared Command Routing

- 目的:
  - menu と shortcut の実行先を共通化する

- 作業項目:
- File / Edit / Composition の command を service 化する
- 同じ操作を menu と shortcut で別実装にしない
- playback control widget と keyboard shortcut を同じ playback service に寄せる
- destructive action の confirmation を共通 helper に寄せる

- 進捗:
  - playback / navigation shortcut を `ArtifactPlaybackService` 経由へ寄せた

- 完了条件:
  - menu から実行した結果と shortcut から実行した結果が一致する
  - command 名を追うだけで実行先が分かる

### Phase 4: Cross-Panel Synchronization

- 目的:
  - メニュー操作後の UI 追従を安定させる

- 作業項目:
  - project view
  - timeline
  - composition viewer
  - inspector
  - layer solo view

- 完了条件:
  - menu 操作後に別パネルが古い state を表示しない
  - current selection / current composition が追従する

### Phase 5: Menu Polish and Diagnostics

- 目的:
  - 実運用で迷いにくいメニューにする

- 作業項目:
  - checked state の整理
  - submenu / separator / disabled placeholder の整理
  - 未実装 action の説明を分かりやすくする
  - menu diagnostics ログの追加

- 完了条件:
  - どの menu も「何を触るか」が読み取れる
  - 実装途中の action が事故りにくい

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- File / Composition / Edit は個別マイルストーンがある
- ただしメニュー群全体としての app integration ルールはまだ分散している
- まずは action ownership と enabled state を共通化するのが低コスト
