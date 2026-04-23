# Phase 4 実行メモ: Export and Diagnostics

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md) の Phase 4 を、サポート共有できる capture bundle と診断レポートに落とし込むための実行メモ。

この段階では、見える化だけでなく、再現と共有までを 1 つの流れにまとめる。

---

## 方針

1. export は read-only snapshot から作る
2. report は text-first でよい
3. crash bundle と frame bundle を無理に混ぜない
4. 保存先と timestamp を明示する

---

## 現状の土台

- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
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
- 共有用の text report を生成できるようにする
- 保存ファイルの識別子を保持する

### 2. Export Path を作る

候補ファイル:

- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)

やること:

- bundle export の起点を用意する
- failed frame を 1 回で保存できるようにする
- 保存先と日時を report に含める

### 3. Diagnostics Summary を整える

候補ファイル:

- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)

やること:

- warnings / errors / fallback / failed frame をまとめる
- frame debug から diagnostics へ飛べるようにする
- text report を copy しやすくする

### 4. 既存の crash / support 導線へ接続する

やること:

- frame bundle と crash bundle の関連を説明できるようにする
- support 共有時の最小情報を固定する
- auto-save の条件は後で拡張できるようにする

---

## 実装順

1. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
2. `Artifact/src/Render/ArtifactRenderQueueService.cppm`
3. `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
4. `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
5. `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

---

## 完了条件

- frame bundle を保存できる
- copyable report を出せる
- failed frame の保存履歴を追える
- diagnostics summary と frame debug がつながる

---

## 変更しないこと

- 既存の crash handling の意味
- 既存の diagnostics の表示順
- 既存の render / playback 操作

---

## リスク

- export を重くしすぎると診断時の応答が鈍る
- report が長くなりすぎると共有しづらい
- crash bundle と混ぜすぎると責務がぼける

---

## 関連文書

- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE2_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE2_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE3_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE3_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)

---

## Work Tickets

### P4-T1 Bundle Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx` 新規

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
