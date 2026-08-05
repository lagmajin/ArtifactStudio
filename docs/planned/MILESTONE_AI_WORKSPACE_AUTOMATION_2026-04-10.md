> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md](MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md)

# AI Workspace Automation Milestone

**進捗状態:** Phase 1〜3 は実装済み。schema／mutation／confirmation の static 検証済み、runtime 実行確認待ち。

### 実装状況（2026-07-25 確認）

WorkspaceAutomation の登録、project／composition／selection／render queue snapshot、asset import、layer／effect／keyframe 操作、render queue 操作、Python／CommandIR bridge、破壊操作の confirmation message と AI tool schema 検査を確認した。残課題は実際の UI 起動状態での invocation、失敗時の復旧、長い mutation chain の runtime 検証。

## Goal

Build a structured AI tool surface for workspace-level editing in ArtifactStudio.

The first target is read-oriented inspection, then safe write actions that map to existing project and render-queue services:

- inspect current project, active composition, selection, and render queue
- create projects and compositions
- import assets
- edit layers by renaming, moving, duplicating, and removing them
- queue and start renders

## Scope

- `WorkspaceAutomation` describable registered in the AI tool registry
- stable JSON snapshots for project, composition, selection, and render queue
- safe wrapper methods for common workspace actions
- tests for schema exposure and basic invocation paths

## Phases

### Phase 1

- register the workspace automation tool host
- expose snapshot methods
- expose project create/import helpers

### Phase 2

- expose layer editing helpers
- expose render queue queueing and start helpers
- add regression coverage for mutation paths

### Phase 3

- add dry-run/confirmation metadata for destructive operations
- add richer payloads for asset and composition selection

