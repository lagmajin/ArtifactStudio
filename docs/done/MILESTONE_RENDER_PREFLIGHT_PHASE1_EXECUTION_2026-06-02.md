# Render Preflight / Output Safety Check - Phase 1 Execution

**Date**: 2026-06-02

**Source**: [`MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`](./MILESTONE_RENDER_PREFLIGHT_2026-06-02.md)

**Status**: Completed

---

## Phase 1 Goal

`render preflight` の入口を先に固定し、出力前に `Error` と `Warning` を分けて読めるようにする。

この段階では、時間予測や sample probe まで広げず、まずは **静的検査** と **既存診断結果の再利用** を優先する。

---

## Scope

### In

- static preflight
- project / composition / render settings validation
- render queue / export dialog への summary 表示
- Error / Warning の分離

### Out

- sample render probe
- ML 予測
- automatic repair
- render backend の改造

---

## Current Boundary Note

- `Project Health` の検証基盤はそのまま再利用する
- render preflight は新しい global validation bus にしない
- `ArtifactRenderQueueService` と `ArtifactProjectHealthChecker` を中心にまとめる
- Warning は案内、Error はブロックという役割を固定する

---

## First Files

1. `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`
2. `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm`
3. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
4. `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`
5. `Artifact/src/Diagnostics/AppValidationRules.cppm`
6. `Artifact/src/Render/ArtifactRenderQueueService.cppm`

---

## Validation Entry Points

1. render queue add / edit
2. export dialog open / confirm
3. render start

まずこの 3 点で同じ preflight result を読めることを確認する。

---

## Tasks

### 1. Static Validation

- missing asset
- output preset mismatch
- frame range invalid
- audio / video の明らかな不整合
- project / composition の基本状態確認

### 2. Result Shape

- severity
- category
- message
- description
- fixHint

を共通化する

### 3. Surface Routing

- render queue manager
- output settings dialog
- problem view
- app debugger

で同じ結果を読めるようにする

---

## Recommended Order

1. `RenderQueueService` から preflight result を返せるようにする
2. `RenderQueueManagerWidget` で summary を出す
3. `ArtifactRenderOutputSettingDialog` に warning / error を見せる
4. `ProblemView` と `AppDebugger` へ同じ結果を回す

---

## Done Criteria

- render 前に止めるべきものが見える
- warning と error が同じ文法で読める
- queue / export dialog / debugger / problem view で結果が食い違わない

## Note

This execution memo is retained only as a historical slice. The canonical completion record is the `done` milestone linked above.

---

## Next Step

Phase 2 では、時間見積もりと VRAM 見積もりを足す。
