# Critical Render / Project Health Worklog - 2026-06-04

## Goal

`Critical Render / Media Stability Program` と `Project Health / Problem View Wiring` を並走で進めた内容を、後からそのまま再開できる形で記録する。

## Current Status

- 実装の接続はかなり揃っている
- `Problem View` / `Health Dashboard` / `App Debugger` / `Render preflight` は共通診断経路に寄せた
- ただし **ビルド確認は未実施**
- そのため、完了判定ではなく再開用ログとして残す

## What Changed

### Project health / diagnostics

- `Artifact/src/Service/ArtifactProjectService.cpp`
  - `currentProjectDiagnostics()`
  - `currentProjectHealthSummaryText()`
  - `currentProjectHealthStateToken()`
  - app-side validation diagnostics を `ProjectHealthReport` と合成するように整理

- `Artifact/include/Service/ArtifactProjectService.ixx`
  - 上記 API を宣言追加

### Render preflight

- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
  - render 開始前の preflight を共通化
  - `formatPreflightSummary()`
  - `formatPreflightDetails()`
  - error がある job は開始しないように変更

- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
  - `QStringList` と上記 formatter API を宣言追加

- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
- `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm`
  - preflight の表示を共通 formatter に差し替え

### Problem View / Health Dashboard

- `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`
  - `currentProjectDiagnostics()` を読むように変更
  - `ProjectChangedEvent` / `CurrentCompositionChangedEvent` / `LayerChangedEvent` を購読して自動更新
  - `showEvent()` で再スキャン

- `Artifact/include/Widgets/ArtifactProblemViewWidget.ixx`
  - `showEvent()` 宣言追加

- `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`
  - diagnostics ベースで状態判定
  - 同じイベントで自動更新
  - `showEvent()` で再評価

### App Debugger / Project Manager

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
  - `currentProjectHealthStateToken()`
  - `currentProjectHealthSummaryText()`
  - health 表示をサービス側へ寄せた

- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
  - `projectHealthText()` をサービス要約へ差し替え

### Render blocking

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - render preflight の blocking 判定を diagnostics ベースへ寄せた

## Notes

- QtCSS の新規追加はしていない
- `QColorDialog` の新規追加はしていない
- 新しい公開 signal/slot は追加していない
- `QImage` の新規採用はしていない
- サブモジュールのコミット / push はしていない

## Verification Status

- `git diff --check` で致命的な崩れは見えていない
- **ビルドはまだ回していない**
- ここが再開時の最優先確認ポイント

## Resume Checklist

1. `Artifact` のビルド確認を実行する
2. もしコンパイルエラーが出たら、その箇所だけを最小修正する
3. `Problem View` / `Health Dashboard` / `App Debugger` / `Render preflight` の表示差分を必要なら揃える
4. 問題なければこの worklog を「完了」へ更新する

## Related Documents

- `docs/MILESTONE_ROADMAP_CURRENT.md`
- `docs/planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`
- `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md`
- `docs/planned/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`
