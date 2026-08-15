# MILESTONE: AI Workflow Automation

作成日: 2026-04-21
最終更新: 2026-08-15

**進捗状態:** Phase 1〜3 は実装済み、Phase 4 は batch rename／batch move など一部実装。runtime 検証と共通 write schema の強化が残る。

### 実装状況（2026-07-25 確認）

`WorkspaceAutomation` の統合 snapshot、project／composition／layer／effect／keyframe／render queue 操作、confirmation message、queue 状態・エラー取得、AI schema／static invocation 検査を確認した。残課題は Phase 2 の統一 SafeWriteResult／dry-run payload、Phase 4 の batch import／relink／render、実 UI と長時間 queue の runtime 検証。

## 2026-08-15 現行コード監査

- `WorkspaceAutomation` の workspace／project／composition／selection／render queue snapshot と、layer／effect／keyframe／queue 操作の schema・static invocation 経路を確認した。
- batch keyframe／layer property、project item rename／move、variation export、render queue 状態取得などの bulk surface が存在し、Undo／SafeWrite 契約を利用する経路もある。
- destructive workflow では dry-run／confirmation／audit の共通契約が使われるが、全 batch 操作が同じ execution plan と rollback 方針に統一されているかは未確認。
- batch import／relink／render の一貫した workflow、長時間 queue の pause／resume／再実行、UI と AI の状態表示、runtime 検証は未完了。

判定: **Snapshot、基本 workspace 操作、bulk keyframe／property、SafeWrite governance の主要基盤は実装済み。全 batch workflow、queue 長時間運用、UI／runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`WorkspaceAutomation` は workspace／project／composition／selection／render queue の統合snapshot、layer／effect／keyframe／queue操作の schema・static invocation、bulk keyframe／layer property、project item rename／move、variation export、queue状態取得を提供する。destructive workflowには dry-run／confirmation／audit と Undo／SafeWrite 経路がある。

未完了・未検証なのは、全batch操作を共通execution plan／rollbackへ統一すること、batch import／relink／render、queueのpause／resume／再実行を含む長時間運用、UIとAIの状態表示、runtime検証である。主要基盤は実装済み、全workflow受入れは pending とする。

## Update 2026-08-15

`WorkspaceAutomation` に `batchRelinkFootageByPath(QVariantList)` を追加した。既存の `relinkFootageByPath()` を再利用し、各項目の `oldFilePath`／`newFilePath`、成功・失敗件数、失敗理由を schemaVersion 付き `QVariantMap` で返す。空パスは `PATH_REQUIRED` として処理し、既存の個別 relink 経路は変更していない。

batch import／render の共通 execution plan／rollback、長時間 queue の再実行、UI／runtime受入れは引き続き pending。

## 目的

AI が単発の提案や単発編集ではなく、ワークスペース全体の作業手順を扱えるようにする。

このマイルストーンは `WorkspaceAutomation` を中心に、project / composition / selection / render queue をまとめて操作できるようにする。  
新しい操作体系を増やすのではなく、既存の安全な service 経路を束ねて再利用する。

---

## 中核

### 1. Workspace Snapshot
- project snapshot
- composition snapshot
- selection snapshot
- render queue snapshot
- 1 回の問い合わせで作業文脈を回収する

### 2. Workspace Actions
- project create / rename / import / cleanup
- composition create / rename / switch / duplicate
- layer create / rename / move / duplicate / select
- render queue enqueue / start / pause / cancel / inspect

### 3. Governance
- dry-run
- confirmation metadata
- failure reason summary
- undo / redo との整合

---

## Phase 1: Snapshot Consolidation

### Goal
作業文脈を 1 つのまとまりで返す。

### Tasks
- `workspaceSnapshot`
- `projectSnapshot`
- `currentCompositionSnapshot`
- `selectionSnapshot`
- `renderQueueSnapshot`
- 返却形式を JSON / QVariantMap ベースで安定化する

### Related
- `Artifact/include/AI/WorkspaceAutomation.ixx`
- `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE1_2026-04-21.md`

---

## Phase 2: Safe Workspace Edits

### Goal
AI が安全に workspace を編集できるようにする。

### Tasks
- rename / duplicate / move / select / import
- layer editing の基本操作
- composition editing の基本操作
- destructive action は confirmation 必須

### Related
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE2_2026-04-21.md`
- `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE2_2026-04-21.md`

---

## Phase 3: Render Queue Workflow

### Goal
レンダー周りを AI から追えるようにする。

### Tasks
- queue snapshot
- job status / progress / error summary
- queue / start / pause / cancel
- rerun / reset

### Related
- `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE3_2026-04-21.md`

### Related
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Test/ArtifactTestAIToolBridge.cppm`

---

## Phase 4: Bulk and Batch Automation

### Goal
反復操作をまとめて扱えるようにする。

### Tasks
- batch import
- batch rename
- batch relink
- batch render
- selection-driven bulk edit

---

## Completion Criteria

- workspace snapshot が 1 回で取れる
- layer / composition / render queue の基本操作が AI から使える
- destructive operation に確認情報が付く
- render queue の状態を AI が説明できる
- 既存の service 経路だけで操作が完結する

## Notes

- ここは新しい UI より既存 service の再利用を優先する
- `WorkspaceAutomation` を主軸にする
- tool surface は read から write へ自然に繋がる必要がある
