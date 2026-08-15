# Menu to App Integration Milestone

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`ArtifactEditMenu` は Undo／Redo、clipboard、selection、playback のサービス経路と context-based enabled state を持ち、`ArtifactRenderMenu` は current composition／queue job に応じて add／start／pause／cancel／clear を更新する。`ArtifactLayerMenu` と Animation menu も project／composition／layer／keyframe の状態に応じて action を有効化し、主要な command routing は現行コードに存在する。

一方、全 menu と shortcut が常に同一 command service 経路を共有すること、checked／enabled state の更新タイミング、menu 間の state 一貫性、再起動後・runtime での操作受入れは未検証である。したがって主要 routing と context state は実装済み、横断的な command 契約と runtime 検証は pending と判定する。

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

---

## Next Execution Slice

Phase 1 は、各 menu action がどの state の正本を読むかを先に固定する。

### Phase 1A の着手点

1. File / Composition / Edit / View / Layer / Render / Help の action inventory を作る
2. 各 action を `Service / Project / Playback / Selection / UI` のどれに属するか決める
3. placeholder action を明示して未実装と実装済みを分ける
4. menu 側に残すロジックを最小限にする

### Phase 1 完了条件

- action ごとに責務の持ち主が説明できる
- menu 側に残すロジックが最小限になる
- placeholder と実装済みが見分けやすい

### Phase 2A の着手点

1. project 有無 / current composition 有無 / current layer / selection / clipboard / undo stack / playback state を enabled state の入力にする
2. action ごとの無効条件を共通化する
3. menu 表示時に stale enabled state を出しにくくする
4. `ArtifactMainWindow` からの明示 refresh を前提にする

### Phase 2 完了条件

- project があるだけで全部有効にならない
- action ごとの無効条件が一貫する
- enabled state が UI surface ごとにズレにくい

### Phase 3 への前提

- shared command routing は ownership と enabled state が固まってから入れる
- cross-panel synchronization は menu state が安定してから詰める

---

## Static audit follow-up (2026-07-25)

現行ソースを静的確認した範囲では、メニュー登録と主要な enabled / checked state は既に各メニューへ実装されている。File は project 有無、Composition は current composition、View は composition / viewport、Render は composition / queue、Animation は current layer を入力にしている。Time menu は `ArtifactPlaybackService` を正本として再生・停止・移動・ループ状態を扱っており、再生系の一元化も部分的に進んでいる。

一方で、action ownership の全体 inventory と共通 command dispatcher は確認できず、各 menu の `QAction::triggered` ラムダから個別サービス／UI処理へ接続する構成が残る。Edit の undo/redo、clipboard、selection に対する全 action の入力条件も共通表としては未整理で、menu 間の refresh 契約と cross-panel 同期を実行時に確認できない。

### Audit status

- Phase 1: 部分実装 — menu ごとの action と責務は存在するが、全体 inventory / ownership map は未作成
- Phase 2: 部分実装 — 主要 menu に context-aware enabled / checked 更新あり。共通化・stale state 検証は未完了
- Phase 3: 部分実装 — `ArtifactPlaybackService` の利用は確認できるが、全 action の shared routing は未確認
- Phase 4: 未確認 — MainWindow、Project、Timeline、Inspector 間の状態同期を実行時に検証できていない
- Phase 5: 未着手相当 — menu diagnostics と全体 polish の完了根拠なし

次の実装候補は、既存 action を壊さずに inventory を文書化し、menu 表示時の state refresh の入口と undo / selection / clipboard の共通入力を整理すること。
