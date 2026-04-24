# Phase 4 Execution: Export / Report / Share / Support

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の Phase 4 を、診断内容を共有しやすい `export / report / share` の流れにまとめるための実行メモ。

この段階では、App Debugger を「見る画面」から「共有できる診断面」へ寄せる。

---

## 方針

1. export は read-only snapshot から作る
2. report は text-first でよい
3. frame bundle と crash bundle は責務を分ける
4. 保存先と timestamp を明示する

---

## 現状の土台

- [`Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Playback/ArtifactPlaybackEngine.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)

---

## 実装タスク

### 1. Bundle モデルを固める

追加候補:

- `FrameDebugBundle`
- `FrameDebugBundleEntry`
- `FrameDebugExportTarget`

責務:

- capture / compare / queue / playback の要約を 1 つにまとめる
- share 可能な text report を生成する
- 保存ファイルの識別子を保持する

### 2. Export Path を作る

やること:

- bundle export の起点を用意する
- failed frame を 1 回で保存できるようにする
- 保存先と日時を report に含める

### 3. Diagnostics Summary を整える

やること:

- warnings / errors / fallback / failed frame をまとめる
- frame debug から diagnostics へ飛べるようにする
- text report を copy しやすくする

### 4. Support Bridge を整理する

やること:

- frame bundle と crash bundle の違いを明示する
- support 共有時の最小情報を固定する
- auto-save の条件を後で拡張できるようにする

---

## レイアウト方針

- 上部: export state / save target / timestamp
- 中央: current frame の要約
- 右側: copyable report / raw bundle text
- 下部: share / support notes

補助ルール:

- report は長くなりすぎないようにする
- 共有用の text は summary first にする
- crash bundle と frame bundle を同じ枠に押し込まない

---

## 表示ルール

- `export` は保存行為として明示する
- `report` はコピー可能な text として明示する
- `share` は support 用の送付データとして明示する
- `failed frame` は保存優先度を上げる
- `crash bundle` は別用途として区別する

---

## 実装メモ

- `AppDebuggerWidget` に export / copy report の入口を寄せる
- `ArtifactDebugConsoleWidget` は report の main surface にする
- `ProfilerPanelWidget` は summary と warning の補助にする
- `FrameDebugViewWidget` は bundle の内容確認に使えるようにする
- 保存した bundle の path を UI で再参照しやすくする

---

## Work Tickets

### P4-T1 Bundle Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx`

内容:

- `FrameDebugBundle` / `FrameDebugBundleEntry` / `FrameDebugExportTarget` を追加する
- capture / compare / queue / playback の要約を束ねる
- 共有用 report の土台にする

完了条件:

- bundle の構造が 1 つの型で読める

### P4-T2 Export Path

対象:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- failed frame の保存起点を作る
- frame bundle を保存できるようにする
- 保存先と timestamp を report に入れる

完了条件:

- 1 フレームを bundle として保存できる

### P4-T3 Diagnostics Summary

対象:

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- warnings / errors / fallback / failed frame をまとめる
- copyable report を整える
- frame debug から diagnostics へ飛べるようにする

完了条件:

- 診断情報を 1 つの report に集約できる

### P4-T4 Support Bridge

対象:

- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

内容:

- frame bundle と crash bundle の関係を説明できるようにする
- auto-save の条件を後で拡張できるようにする
- support 共有時の最小情報を固定する

完了条件:

- support 向けの共有導線がぶれない

---

## 完了条件

- frame bundle を保存できる
- copyable report を出せる
- failed frame の保存履歴を追える
- diagnostics summary と frame debug がつながる
- share する最小情報が固定される

---

## 変更しないこと

- 既存の crash handling の意味
- 既存の diagnostics の表示順
- 既存の render / playback 操作
- QtCSS と新しい global signal/slot

---

## リスク

- export を重くしすぎると診断時の応答が鈍る
- report が長くなりすぎると共有しづらい
- crash bundle と混ぜすぎると責務がぼける

---

## 次の Phase への橋渡し

Phase 4 が終わると、App Debugger は「見やすい」だけでなく「再現して共有できる」診断面として成立しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
