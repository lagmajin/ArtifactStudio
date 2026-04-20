# Phase 1 実行メモ: Frame Capture Contract

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md) の Phase 1 を、既存の描画・再生・キュー運用を崩さずに進めるための実行メモ。

この段階では見た目の UI よりも、`1 フレームを固定して読める契約` を先に固める。

---

## 方針

1. 既存の render / playback / queue の経路は残す
2. 新しい capture 契約は read-only から始める
3. frame / pass / resource / attachment を最小要約で記録する
4. まずは `Frame Debug View` より先に、既存 widget から見える summary を用意する

---

## 現状の土台

- [`Artifact/include/Render/ArtifactIRenderer.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactIRenderer.ixx)
- [`Artifact/include/Render/ArtifactRenderQueueService.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactRenderQueueService.ixx)
- [`Artifact/include/Render/ArtifactFrameCache.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Render/ArtifactFrameCache.ixx)
- [`Artifact/include/Playback/ArtifactPlaybackEngine.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Playback/ArtifactPlaybackEngine.ixx)
- [`Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx)
- [`Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx)
- [`Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx)
- [`Artifact/include/Widgets/Diagnostics/ArtifactDebugConsoleWidget.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Diagnostics/ArtifactDebugConsoleWidget.ixx)
- [`Artifact/include/Widgets/Diagnostics/ProfilerPanelWidget.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Diagnostics/ProfilerPanelWidget.ixx)
- [`ArtifactCore/include/Frame/Frame.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Frame/Frame.ixx)
- [`ArtifactCore/include/Frame/FramePosition.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Frame/FramePosition.ixx)
- [`ArtifactCore/include/Frame/FrameRange.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Frame/FrameRange.ixx)

---

## 実装タスク

### 1. Capture Contract を定義する

追加候補:

- `FrameDebugSnapshot`
- `FrameDebugCapture`
- `FrameDebugPassRecord`
- `FrameDebugResourceRecord`
- `FrameDebugCaptureRequest`
- `FrameDebugCompareState`
- `FrameDebugBundle`

責務:

- frame 単位の総合要約を 1 つの型で持つ
- pass / resource / attachment の最小要約を含める
- compare と export に必要な識別子を保持する
- backend 非対応時の empty / unavailable を表現する

### 2. 既存 service から snapshot を組み立てる

候補ファイル:

- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Playback/ArtifactPlaybackEngine.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)

やること:

- current frame / playing / paused / seek state を読む
- failed frame / rerender candidate / queue metadata を読む
- attachment / readback / output の要約を作る
- 既存の frame cache 情報があれば、それを補助情報として接続する

### 3. 既存 UI に summary を露出する

候補ファイル:

- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm)
- [`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm)

やること:

- まずは text summary で frame capture を見せる
- scrub / step の起点を既存 widget から押せるようにする
- 完全な view ではなく、最小の読める表面を先に置く

### 4. 保存・比較のための識別子を固定する

やること:

- capture id / bundle id / previous capture id を固定する
- compare target を frame number だけでなく capture id でも追えるようにする
- failed frame を 1 回で再参照できるようにする

---

## 実装順

1. `Artifact/include/Render/ArtifactIRenderer.ixx`
2. `Artifact/include/Render/ArtifactRenderQueueService.ixx`
3. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
4. `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
5. `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
6. `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
7. `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

---

## Work Tickets

### P1-T1 Core Frame Debug Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx` 新規

内容:

- `FrameDebugSnapshot` / `FrameDebugCapture` / `FrameDebugPassRecord` / `FrameDebugResourceRecord` を定義する
- capture id / bundle id / compare target id を持たせる
- read-only の JSON 変換補助を用意する

完了条件:

- 1 フレーム分の構造化データを 1 箇所に置ける

### P1-T2 Renderer Summary Hook

対象:

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

内容:

- attachment / texture / readback の最小要約を取れるようにする
- render output の参照情報を frame summary へ渡せるようにする
- 既存描画経路は変えない

完了条件:

- renderer から frame debug 用の要約を引ける

### P1-T3 Queue Summary Hook

対象:

- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

内容:

- failed frame / rerender candidate / queue metadata を読めるようにする
- export 側の frame summary を組み立てられるようにする
- 既存の job model は変えない

完了条件:

- queue から frame debug 用の失敗要約を引ける

### P1-T4 Playback Summary Hook

対象:

- `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

内容:

- current frame / playing / paused / seek state を読めるようにする
- step と scrub の起点をまとめられるようにする
- frameChanged の既存挙動は維持する

完了条件:

- playback から frame debug 用の状態を読める

### P1-T5 Composition Summary Hook

対象:

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

内容:

- current frame の render summary を組み立てる
- render path / backend / fallback の状態を露出する
- frame pin の起点を用意する

完了条件:

- composition render controller から frame summary を作れる

### P1-T6 Display Fallback

対象:

- `Artifact/include/Widgets/Diagnostics/ArtifactDebugConsoleWidget.ixx`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/include/Widgets/Diagnostics/ProfilerPanelWidget.ixx`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

内容:

- text summary の fallback を置く
- frame debug の最小状態を読めるようにする
- UI の見た目は壊さない

完了条件:

- 専用 view なしでも summary が読める

---

## 完了条件

- 1 フレームの summary を読み出せる
- pass / resource / attachment の最小要約が揃う
- failed frame と rerender candidate を追える
- scrub / step の起点が既存 UI から触れる
- 既存の描画結果や再生挙動は変わらない

---

## 変更しないこと

- 既存の描画アルゴリズム
- 既存の playback 速度や再生状態の意味
- 既存の render queue job model
- 既存 UI の見た目と操作感

---

## リスク

- capture を取りすぎると hot path が重くなる
- summary の粒度が揃わないと compare が読みにくくなる
- 既存 widget に summary を載せすぎると UI が窮屈になる

---

## 次の Phase への橋渡し

Phase 1 が終わると、Phase 2 で pass / resource inspector を独立 surface として切り出しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
