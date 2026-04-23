# マイルストーン: Toolbar / App Integration

> 2026-04-17 作成

## 目的

`ArtifactToolBar` を単なる操作ボタン列ではなく、アプリ層の command surface として整理する。

このマイルストーンは、toolbar の見た目を大きく変えることではなく、
**どの操作がどの service / state を正本に持つか** を固定し、
toolbar・menu・shortcut・view state の整合を取りやすくすることを目的にする。

## 原則

- Qt の新規 signal / slot は増やさない
- toolbar から app への接続は、既存の command / service / event bus / 明示 refresh を優先する
- toolbar 側にビジネスロジックを溜めない
- `QAction` は UI surface、state の正本は app/service 側に置く
- menu と toolbar と shortcut で同じ command を使う

## Scope

- `Artifact/src/Widgets/ArtifactToolBar.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- `Artifact/src/Widgets/ArtifactStatusBar.cpp`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Service/*`
- `Artifact/src/Tool/*`

## Non-Goals

- toolbar の全面的な再デザイン
- 新しいグローバル signal/slot の追加
- shortcut 体系の全面置換
- command system の別実装を新規に増やすこと

## Background

現状の toolbar は、tool 選択や zoom / grid / guide の表示切替を担っているが、
アプリ全体の state と結びつく部分はまだ散らばりやすい。

特に次のような状態は、toolbar 単体の都合ではなく app state として見た方が安定する。

- current tool
- workspace mode
- current composition / current layer
- playback / zoom / grid / guide
- selection / editable state
- diagnostics / activity feedback

また、toolbar の更新は signal 駆動で増やすより、既存の state 更新ポイントで明示的に反映する方が、
このコードベースの制約に合っている。

## Phases

### Phase 1: Action Ownership Map

- 目的:
  - toolbar action の所有者と責務を固定する

- 作業項目:
  - `home / select / hand / zoom / move / rotation / scale / camera / pan behind / shape / pen / text / brush / clone stamp / eraser / puppet` の ownership を整理する
  - `zoom in / zoom out / 100% / fit / grid / guide / view mode` の state source を決める
  - toolbar で完結する action と app service へ委譲する action を分ける

- 完了条件:
  - 各 action がどの state を読むか説明できる
  - toolbar 側に隠れた業務ロジックが残らない

### Phase 2: State Synchronization Without New Signals

- 目的:
  - app state の変化を toolbar に安全に反映する

- 作業項目:
  - `ArtifactMainWindow` もしくは既存の state update 経路から toolbar へ明示 refresh を呼ぶ
  - `setCurrentTool()`, `setWorkspaceMode()`, `setZoomLevel()`, `setGridVisible()`, `setGuideVisible()` の役割を再整理する
  - toolbar の checked / enabled / tooltip を pull できるようにする

- 完了条件:
  - toolbar が stale state を表示しにくい
  - 新規 signal/slot を足さずに見た目の同期ができる

### Phase 3: Shared Command Routing

- 目的:
  - menu / toolbar / shortcut の実行経路を共通化する

- 作業項目:
  - toolbar action と menu action が同じ command を呼ぶようにする
  - tool selection と view toggle を command registry または service に寄せる
  - `EventBus` や既存 service を使って、tool change をアプリ全体へ伝える

- 完了条件:
  - 同じ操作が menu と toolbar で別実装にならない
  - command 名を追うだけで実行先が分かる

### Phase 4: Cross-Panel Feedback

- 目的:
  - toolbar 操作の結果が、他パネルの状態表示にも自然に反映される

- 作業項目:
  - timeline / inspector / project view / layer solo view / composition editor の追従を整理する
  - active tool と current selection の見え方を統一する
  - view mode や workspace mode の切替で、各 panel が古い state を残さないようにする

- 完了条件:
  - toolbar で tool を切り替えたときのアプリ全体の反応が一貫する
  - どの panel から見ても「今の操作モード」が分かる

### Phase 5: Toolbar Diagnostics and Polish

- 目的:
  - 実運用で toolbar の状態を読みやすくする

- 作業項目:
  - tooltip / status text / icon state の整理
  - disabled 理由の見え方を揃える
  - compact mode と full mode の差を整理する
  - 既存の diagnostics surface と接続して、現在の tool / workspace / zoom を追いやすくする

- 完了条件:
  - toolbar の意図が読める
  - debug 時に今の state が追いやすい

## Recommended Order

1. Phase 1: Action Ownership Map
2. Phase 2: State Synchronization Without New Signals
3. Phase 3: Shared Command Routing
4. Phase 4: Cross-Panel Feedback
5. Phase 5: Toolbar Diagnostics and Polish

## Related Milestones

- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`

