# Milestone: QSS Decommission / CommonStyle Path to QCommonStyle (2026-04-03)

**Status:** Draft  
**Goal:** `QSS` を UI の主責務から外し、最終的に `QCommonStyle` を土台にした共通スタイル基盤へ移行する。

---

## Why This Now

`QSS` は短期的には便利だが、責務が増えるほど「どの見た目が誰の所有物か」が追いにくくなる。  
すでに `theme token` / `QPalette` / owner-draw / `QProxyStyle` 系の土台があり、ここから **QSS を新規追加しない方針** に切り替えるのが現実的。

この milestone は、既存の theme 整備と `QSS` 削減をまとめて、最終的に `QCommonStyle` を採用できる状態へ持っていくための移行計画。

---

## Scope

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Dock/*`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Widgets/PropertyEditor/*`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/Widgets/Timeline/*`
- `Artifact/src/Widgets/Menu/*`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- `ArtifactWidgets/src`

---

## Non-Goals

- 全 widget を一括で `QSS` ゼロにする
- 既存の UI を一気に大改造する
- OS 標準ルックへ戻す
- サブモジュールを大規模に触る

---

## Design Principles

- `QSS` は新規追加しない
- まず theme token / palette / owner-draw で説明できる見た目を増やす
- `QProxyStyle` ベースの共通 style を育て、最終的に `QCommonStyle` を土台へ寄せる
- widget-local style は「例外」として明示する
- 余白、枠線、selection、hover を style ownership の観点で整理する

---

## Phases

### Phase 1: QSS Freeze And Surface Inventory

- `setStyleSheet()` の新規追加を止める
- 既存 `QSS` を inline / widget-local / global に分類する
- 残す `QSS` と削る `QSS` を用途ごとに棚卸しする
- 共通 style に逃がせる箇所を抽出する

**Done when:**

- どの widget が `QSS` を所有しているか説明できる
- 新規 `QSS` を足さずに見た目調整する方針に切り替わる

---

### Phase 2: CommonStyle Foundation

- アプリ共通の style manager を作る
- `Dock` / `Toolbar` / `Tab` / `Button` / `ComboBox` / `Splitter` の基本外観を統一する
- palette mapping を明示し、色の source of truth を `QSS` に戻さない
- 可能なものは `QPalette` に寄せる

**Done when:**

- 主要 chrome が共通 style で揃う
- `QSS` がなくても基本の見た目が成立する

---

### Phase 3: Widget Migration

- `Property` / `Inspector` / `Render Queue` / `Asset Browser` / `Timeline` から順に移す
- widget-local `QSS` を共通 style または owner-draw に置換する
- hover / selection / disabled / focus の表現を共通化する
- widget ごとの直書き色を減らす

**Done when:**

- 主要 widget が theme token と common style で説明できる
- widget-local `QSS` は例外だけになる

---

### Phase 4: QCommonStyle Promotion

- 共通 style の基底を `QCommonStyle` に切り替えられるか確認する
- `QProxyStyle` 依存が残る部分を洗い出す
- `QCommonStyle` でも必要な見た目が成立するように、custom paint の境界を明確化する
- 最終的に `QCommonStyle` を app-level standard とする

**Done when:**

- app の標準 style が `QCommonStyle` ベースになる
- 例外的な描画だけが custom paint として残る

---

## Suggested Order

1. Phase 1: QSS Freeze And Surface Inventory
2. Phase 2: CommonStyle Foundation
3. Phase 3: Widget Migration
4. Phase 4: QCommonStyle Promotion

---

## Relationship To Existing Milestones

- `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`
- `docs/planned/MILESTONE_QSS_REDUCTION_2026-03-31.md`
- `docs/planned/MILESTONE_UI_THEME_SYSTEM_ROLLOUT_2026-04-02.md`
- `docs/planned/MILESTONE_QSS_EXORCISM_PROPERTY_THEME_2026-04-02.md`
- `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`

This milestone is the execution path that takes the existing theme work and turns it into a long-term `QCommonStyle` ownership model.

---

## Current Status

- `QSS` はまだ多い
- ただし palette / custom draw / common widget 化へ移せる部分は既に見えている
- まずは `QSS` を増やさない規律を作り、共通 style の責務を育てるのが先
- `QCommonStyle` はゴールであって、初手ではない
- theme preset を JSON で差し替えられる入口を追加し、global style の切替と見た目プリセットの分離を進めた
