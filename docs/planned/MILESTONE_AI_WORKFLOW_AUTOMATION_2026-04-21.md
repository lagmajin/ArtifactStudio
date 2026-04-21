# MILESTONE: AI Workflow Automation

作成日: 2026-04-21

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
