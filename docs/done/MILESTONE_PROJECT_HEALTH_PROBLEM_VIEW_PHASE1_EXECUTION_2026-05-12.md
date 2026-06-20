# Project Health / Problem View - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)

**Status**: 完了
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
