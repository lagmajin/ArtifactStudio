# Debug Render Harness

`DiligentEngine` / `ArtifactIRenderer` を本番経路のまま残しつつ、粒子・動画・ブレンド・オーバーレイの最小再現を独立に検証するためのデバッグ用レンダリングハーネス。

これは別 backend の再実装ではなく、**同じ Diligent backend を使う薄い検証 surface** として扱う。

## Goal

- particle / video / blend / overlay を最小シーンで切り分ける
- 「描けない」の原因を UI / layer / renderer / backend に分解する
- `FrameDebugSnapshot` と同じ粒度で失敗理由を残す
- smoke case を固定して、再現後の比較を簡単にする

## Non-Goals

- 本番レンダラーの全面置き換え
- 別 backend の並行実装
- QtCSS を使った専用 debug テーマの追加
- OS レベルの RenderDoc 代替
- 既存の layer / effect モデルを壊す再設計

## Design Principles

- Same backend, smaller scene
  - Diligent / DX12 の本流を変えず、入力だけ最小化する
- Readable failures
  - `no RTV`, `decode failed`, `empty particle`, `transparent output` を同じ画面で読めるようにする
- Capture first
  - 失敗時はログだけでなく snapshot を残す
- No new global wiring
  - 中央集権の signal/slot は増やさない

## Harness Shape

### 1. Render Device Layer

- 既存 `ArtifactIRenderer` の device/context を流用
- swapchain / viewport / clear color だけをハーネス側で管理
- Diligent backend の差分を増やさない

### 2. Scene Presets

最小シーンを固定プリセットとして用意する。

- `particle-only`
- `video-only`
- `blend-only`
- `overlay-only`
- `mixed-media`

各プリセットは「何を描かないか」を明示する。
- 契約詳細: [`../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md)

### 3. Capture and Report

失敗時に以下を bundle 化する。

- `FrameDebugSnapshot`
- particle draw state
- video decode state
- render backend / viewport / RTV state
- last error / skipped reason
- selected preset / scene params
- 文字列テンプレート: [`../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md)

### 4. Verification Surface

- 画面上の合否表示
- `Frame Debug` への summary 連携
- 保存可能な smoke report

## Implementation Targets

### Core / Renderer

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/include/Frame/FrameDebug.ixx`

### App / Diagnostics

- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### Future UI Surface

- `DebugRenderHarnessWidget`
- `DebugScenePresetPanel`
- `DebugCaptureReportPanel`

## Phase Plan

### Phase 1: Contract

- harness の責務を固定
- scene preset の一覧を決める
- snapshot に残す項目を確定する
- 詳細: 本書の Phase 1 節を参照

### Phase 2: Minimal Scene

- particle-only / video-only を先に作る
- overlay-only を追加する
- no-RTV / no-frame / decode-fail の見え方を揃える
- 詳細: 本書の Phase 2 節を参照

### Phase 3: Capture

- smoke report を保存できるようにする
- 失敗時の last error を bundle に含める
- frame debug から遷移できるようにする
- 詳細: 本書の Phase 3 節を参照

### Phase 4: Regression Gate

- particle smoke
- video smoke
- transparent output smoke
- expected skipped reason の確認
- 詳細: 本書の Phase 4 節を参照

## Acceptance Criteria

- 本番 UI から切り離して smoke scene を開ける
- particle / video の失敗理由が 1 画面で読める
- `FrameDebugSnapshot` と矛盾しない report が出る
- 既存 renderer の責務を壊さずに検証できる

## Current Status

- `DebugRenderHarnessWidget` は既に実装済み
- `AppMain` から独立 dock として開ける
- `AppDebuggerWidget` にも harness tab を追加し、frame debug snapshot を流し込める状態にした
- harness report を clipboard / file に保存できるようにした

## Related

- [`MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](./MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)
- [`MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_PHASE1_TRIAGE_2026-04-30.md`](./MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_PHASE1_TRIAGE_2026-04-30.md)
- [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)
- [`../technical/DEBUG_RENDER_HARNESS_SMOKE_CHECKLIST_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_SMOKE_CHECKLIST_2026-04-30.md)
- [`../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md)
- [`../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md)
- Phase 3 / Phase 4 は本書に統合済み
