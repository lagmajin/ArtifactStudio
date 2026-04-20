# Phase 3 実行メモ: Compare / Scrub / Step

> 2026-04-20 作成

## 目的

[`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md) の Phase 3 を、フレーム比較と 1-frame 操作まで含めて実用化するための実行メモ。

この段階では、固定した 1 フレームを前後に追いながら、差分の当たりを付けられるようにする。

---

## 方針

1. compare は read-only
2. scrub は既存の timeline 操作を壊さない
3. step は playback engine の正規経路に寄せる
4. A/B を見せるが、複雑な diff UI は後回しにする

---

## 現状の土台

- [`Artifact/include/Playback/ArtifactPlaybackEngine.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Playback/ArtifactPlaybackEngine.ixx)
- [`Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx)
- [`Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`](X:/Dev/ArtifactStudio/Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm)

---

## 実装タスク

### 1. Compare State を固める

追加候補:

- `FrameDebugCompareState`
- `FrameDebugCompareMode`

責務:

- current / previous / next を追えるようにする
- A/B の比較対象を保持する
- capture id ベースで差分を追えるようにする

### 2. Scrub を frame debug に接続する

候補ファイル:

- [`Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm)
- [`Artifact/src/Playback/ArtifactPlaybackEngine.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)

やること:

- scrub で capture を切り替える
- frame debug の pin 状態と scrub 状態を整合させる
- 既存の current frame 表示を崩さない

### 3. Step を playback の正規経路に寄せる

候補ファイル:

- [`Artifact/src/Playback/ArtifactPlaybackEngine.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

やること:

- 1 フレーム forward / backward を実装する
- pause 中の step と playback 中の step を分けて扱う
- capture の固定フレームと同期する

### 4. Compare 表示を UI に載せる

候補ファイル:

- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)

やること:

- A/B compare を summary で見せる
- 前後フレームの切り替えを分かりやすくする
- first cut は text based でよい

---

## 実装順

1. `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
2. `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
3. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
5. `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

---

## Work Tickets

### P3-T1 Compare State Model

対象:

- `ArtifactCore/include/Frame/FrameDebug.ixx` 新規

内容:

- `FrameDebugCompareState` / `FrameDebugCompareMode` を追加する
- current / previous / next を追えるようにする
- capture id ベースの比較を可能にする

完了条件:

- compare の状態を 1 つの型で持てる

### P3-T2 Playback Step Control

対象:

- `Artifact/include/Playback/ArtifactPlaybackEngine.ixx`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

内容:

- 1 フレーム forward / backward を実装する
- pause 中と playback 中の step を分ける
- pin した capture と current frame を同期する

完了条件:

- playback の正規経路で 1-frame step ができる

### P3-T3 Scrub Integration

対象:

- `Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`

内容:

- scrub で capture を切り替える
- current frame 表示と debug pin を整合させる
- 既存の cache overlay と共存させる

完了条件:

- scrub が frame debug の compare 起点として動く

### P3-T4 Compare UI

対象:

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- A/B compare を summary で見せる
- 前後フレームの差分を text で読めるようにする
- first cut は軽い表示でよい

完了条件:

- compare 対象が UI から分かる

---

## 完了条件

- 前フレーム / 現フレーム / 次フレームを切り替えられる
- scrub と step が frame debug の文脈で動く
- A/B compare の対象が分かる
- 既存の playback と timeline の挙動が壊れない

---

## 変更しないこと

- 再生速度の意味
- 既存の scrub bar の基本操作
- 既存の playhead 表示

---

## リスク

- compare を深くしすぎると UI が重くなる
- scrub と step の権威が複数あると状態がずれる
- playback 中の stepping を雑に扱うと再生挙動が壊れる

---

## 次の Phase への橋渡し

Phase 3 が終わると、Phase 4 で capture bundle と report を出すときに、再現操作をそのまま保存しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE2_EXECUTION_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE2_EXECUTION_2026-04-20.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
