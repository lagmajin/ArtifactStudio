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
- `docs/done/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`
- `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md`
- `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`

---

## 2026-06-15 差分整理メモ（コード読み込みのみ）

ユーザーから「マイルストーン実装していきたい」と依頼があったが、AGENTS.md / taste で「ビルドは明示依頼時のみ」と指定されているため、`Artifact` のフルビルドはかけずに Resume Checklist の前段としてコード状態を再確認した。

### A-1 Resume Checklist 進捗

- **Step 1 (ビルド確認)**: 未実行。ユーザーからの明示依頼待ち。
- **Step 2 (コンパイルエラー最小修正)**: 未着手。
- **Step 3 (Problem View / Health Dashboard / App Debugger / Render preflight の表示差分)**: コード上は接続済み。
  - `currentProjectDiagnostics()` / `currentProjectHealthSummaryText()` / `currentProjectHealthStateToken()` の 3 API が 4 surface 共通で利用されている。
  - `formatPreflightSummary` / `formatPreflightDetails` が RenderQueue / Menu / Dialog 3 箇所で共有されている。
  - `ProjectChangedEvent` / `CurrentCompositionChangedEvent` / `LayerChangedEvent` を ProblemView / HealthDashboard が購読、`showEvent()` で再評価するパスも入っている。
  - `ArtifactCompositionRenderController.cppm:6912` で preflight=blocked 経路も diagnostics ベースで実装されている。
- **Step 4 (worklog を「完了」へ更新)**: Step 1 が通るまで保留。

### worklog 内の参照パスの訂正

- 旧 worklog 本文に `ArtifactProjectService.cpp` と書かれているが、実装は `Artifact/src/Service/ArtifactProjectService.cppm` に存在する（`.cpp` ではない）。
- 同じ .cppm 化は `ArtifactAudioService.cpp` / `ArtifactClipboardService.cpp` / `ArtifactEffectService.cpp` にも波及しているが、本 worklog の Resume Checklist 対象は ProjectService のみなので、ここでは ProjectService のみ訂正扱いとする。

### 周辺バックログの実装状態（2026-06-15 時点）

ユーザーから「別のマイルストーンも見たい」と依頼があったため、ロードマップ上の関連候補を並行でコード確認した。**いずれも backlog 文書の方が古い**：

- **B-1 `sampleSpeedGraph()`**: `docs/REPO_BACKLOG_TRIAGE_2026-06-15.md` は「ArtifactCurveEditorWidget.cppm:815 のコメントのみ」と記載しているが、現状 `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm:35`（無名 namespace 内の宣言）と `:580`（`ArtifactCurveEditorWidget::setSpeedGraph` 内）で実装・接続済み。`ArtifactCore::sampleSpeedGraph` ではなく、同翻訳単位内の `sampleSpeedGraph` を呼んでいる。backlog 記述のアップデートが必要。
- **D-1 Shape Operators 6 種**: backlog は「interface のみ。コアな 1〜2 個を薄く実装してから残り展開」と記載しているが、現状以下が実装・接続済み。
  - `ArtifactCore/include/Shape/TrimPaths.ixx`（完全実装、`process` を持つ）
  - `ArtifactCore/include/Shape/Repeater.ixx`（完全実装）
  - `ArtifactCore/include/Shape/AeOperators.ixx`（MergePaths / OffsetPaths / PuckerBloat / Twist / RoundedCorners / WigglePaths / ZigZag / HandDrawnWobble が完全実装）
  - `Artifact/include/Layer/ArtifactShapeLayer.ixx` に `addShapeOperator(ShapeOperatorType)` / `clearShapeOperators()` / `shapeOperatorCount()` / `shapeOperatorTypeAt()` の API 宣言あり。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` に 9 種のファクトリ + `applyShapeOperators()` + `buildProcessedPainterPaths()` の配線済み。
  - `out/build/x64-Debug` および `x64-Release` 配下に ifc / ddi / modmap が出力されている。
  - backlog 記述のアップデートが必要。Phase 4 残作業は「UI から addShapeOperator を呼ぶ経路」ではなく、UI 側の取り回し確認レベルに縮退している可能性が高い。
- **M-UI-6 / M-UI-7 / M-FE-9 / M-IR-8**: `docs/REPO_BACKLOG_TRIAGE_2026-06-15.md` と `docs/MILESTONE_ROADMAP_CURRENT.md` で ID だけ登場するが、説明文書が `docs/planned/` / `docs/archived/` / `docs/MILESTONE_*_2026-04-27.md` 配下のどこにも残っていない。要件が文書化されていない状態。

### 次のセッションで再開するときに確認すべき点

1. Resume Checklist Step 1: ユーザーから「ビルドして」と明示依頼があるか確認する（明示がない限り走らせない）。
2. backlog 文書のアップデート粒度を判断する（B-1, D-1 は status だけ書き換える 1 行メモで足りる）。
3. M-UI-* / M-FE-9 / M-IR-8 の定義文書を探す旅にでない。無いなら新規に起こす前にユーザーに「設計メモを起こす or この ID は棚卸し対象に回す」の確認を取る。
