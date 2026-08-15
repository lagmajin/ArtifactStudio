# Project Health / Problem View - Phase 1 Execution

**Date**: 2026-05-12
**最終更新**: 2026-08-15

**Source**: [`../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)

**Status**: Core／app 診断経路と主要 UI は実装済み、入口間の runtime 一致確認待ち
**Order**: 1 of 3

---

## Phase 1 Goal

`DiagnosticEngine` と `ProjectDiagnostic` の入口を app 側で揃える。

この段階では UI の完成よりも、load / save / render preflight で同じ検証結果を読めることを優先する。

---

## Scope

### In

- diagnostics core
- app-side validation hook
- problem view / dashboard source unification
- render preflight error blocking

### Out

- full widget redesign
- auto repair
- CI integration

---

## Current Boundary Note

- `ProjectDiagnostic` と `DiagnosticEngine` は Core 側の result source として維持する
- app 側で触るのは `ArtifactProjectHealthChecker` と `ArtifactProblemViewWidget` / `ArtifactProjectHealthDashboard`
- まずは load / save / render preflight の 3 点で同じ validation 結果を読めるようにする
- Warning は案内、Error はブロックという役割を固定する

---

## First Files

1. `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
2. `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`
3. `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`
4. `ArtifactCore/include/Diagnostics/DiagnosticEngine.ixx`
5. `ArtifactCore/include/Diagnostics/ProjectDiagnostic.ixx`

---

## Validation Entry Points

1. project load
2. project save
3. render preflight

この 3 点で同じ result shape を読むことを先に確認する。

---

## Tasks

### 1. Core Result Path

- `ProjectDiagnostic` の result shape を固定する
- `DiagnosticEngine` の検証結果を app 側へ渡せるようにする

### 2. App Validation Hook

- project load / save の前後で検証を走らせる
- render preflight で Error をブロックする

### 3. Source Unification

- `ArtifactProblemViewWidget`
- `ArtifactProjectHealthDashboard`

が同じ結果ソースを読むようにする

---

## Recommended Order

1. `DiagnosticEngine` の result shape を固定する
2. load / save の前後で validation hook を通す
3. `ProblemView` と `HealthDashboard` を同じ結果へ寄せる
4. render preflight の Error blocking を入れる

---

## First Move

1. `ProjectDiagnostic` / `DiagnosticEngine` の返す shape を揃える
2. `ArtifactProjectHealthChecker` を load / save 前後へつなぐ
3. `ProblemView` と `HealthDashboard` の source を合わせる
4. その後に render preflight をブロック条件へ寄せる

---

## Done Criteria

- 問題検出の入口が1本になる
- エラーはレンダー前に止まる
- problem view と dashboard の結果が食い違わない

## 2026-07-25 実装監査

`ProjectDiagnostic`／`DiagnosticEngine`、app validation rules、ProjectHealth→Diagnostic の変換、missing／matte／circular／expression／performance issue と fix action の基盤は確認した。Problem View／Health Dashboard／load・save・render preflight の三入口が完全に同一 result source を使い、Error を常に render 前に block すること、表示結果が一致することは静的検索だけでは断定できない。したがって Phase 1 の Core／app 基盤は部分実装、入口統一とUI／runtime整合は未検証とする。

## 2026-08-15 現行コード監査

- `ArtifactProjectService::currentProjectDiagnostics()` が Project Health を `ProjectDiagnostic` へ変換し、Problem View／Health Dashboard／App Debugger がこの診断モデルを参照する経路を確認した。
- Render Queue は preflight の Error を job failure に反映し、output dialog／queue surface に error・warning summary と details を表示する実装がある。
- load／save／render の全ケースで同じ診断結果・表示・block 条件になること、runtime での refresh 順序と Error blocking の受入れは未検証。

判定: **診断基盤、主要 UI、render preflight の Error 経路は実装済み。入口間の完全な結果一致と runtime 検証は pending。**

## Update 2026-08-15

現行コードを再確認した。`ArtifactProjectService::currentProjectDiagnostics()` を中心に Project Health／Problem View／App Debugger が構造化診断を参照し、Render Queue の preflight は Error を job failure として扱う経路がある。

- Warning／Error の表示とブロック責務は主要導線に存在するが、load／save／render の全入口が同一タイミング・同一 snapshot を読むことは静的コードだけでは保証できない。
- Problem View と Health Dashboard の refresh 順序、stale diagnostics の破棄、Error blocking 後の再評価・復旧は runtime 受入が未確認。
- 判定は **Phase 1 の診断基盤と主要 UI は実装済み、入口統一の実運用検証は未完了** を維持する。
