# MILESTONE: AI Safe Write Tools

作成日: 2026-04-21

## 目的

AI が見つけた提案を、確認付きで安全に編集へ反映できるようにする。

このマイルストーンは AI に自由な編集権限を与えるものではない。  
既存の `*Service` 経路を薄く再利用し、dry-run / confirmation / undo を前提にした安全な write surface を作る。

---

## 原則

- 直接 low-level API を触らせない
- 既存の `ArtifactProjectService` / `ArtifactEffectService` / render queue service を優先する
- 破壊的操作は必ず確認を返す
- 実行前に dry-run を返せるようにする
- 可能な操作は既存 undo / redo の上に載せる

---

## 対象操作

### 1. Basic Mutations
- layer rename
- composition rename
- asset import
- layer select
- composition select
- layer duplicate
- layer move

### 2. Render Queue Actions
- queue render job
- start render job
- inspect job state
- list failed / pending / completed jobs

### 3. Workspace Operations
- create project
- create composition
- organize assets
- batch move / batch rename
- selection-driven bulk action

### 4. Limited Destructive Actions
- remove layer
- remove composition
- remove effect
- remove asset
- これらは confirmation metadata と dry-run を必須にする

---

## Phase 1: Service Mapping

### Goal
AI 用の write tool を、既存サービスに対して明確に対応付ける。

### Tasks
- `ArtifactProjectService` の mutation surface を整理する
- `ArtifactRenderQueueService` の queue / start surface を整理する
- `ArtifactEffectService` の reorder / duplicate / remove surface を整理する
- tool registry から呼べる操作名を固定する

### Output
- `WriteToolDescriptor`
- `WriteToolRequest`
- `WriteToolResult`
- `WriteToolConfirmation`

### Related
- `Artifact/include/Service/ArtifactProjectService.ixx`
- `Artifact/include/Service/ArtifactEffectService.ixx`
- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE1_2026-04-21.md`

---

## Phase 2: Dry-run and Confirmation

### Goal
破壊的操作を、実行前に止められるようにする。

### Tasks
- dry-run で影響範囲を返す
- confirmation で人間の最終確認を要求する
- 取り消し可能な操作だけを既定の write tool にする
- 失敗理由を UI と AI context に同じ形式で返す

### Output
- `WriteToolDryRunResult`
- `WriteToolConfirmationPayload`
- `WriteToolExecutionPlan`

### Related
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE2_2026-04-21.md`

---

## Phase 3: Workspace Automation Surface

### Goal
小さな編集操作を、まとまったワークフローにできるようにする。

### Tasks
- project setup
- composition setup
- asset organization
- selection-driven batch edit
- render queue preparation

### Notes
- ここでは新しい UI を増やしすぎない
- 既存の project / composition / queue surface に寄せる

---

## Phase 4: Execution Governance

### Goal
AI の write 実行を監査可能にする。

### Tasks
- operation log
- confirmation log
- failure reason summary
- permissions / policy summary
- prompt context への反映

### Related
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE3_2026-04-21.md`

---

## Completion Criteria

- layer / composition / asset の基本編集を安全に呼べる
- render queue を AI 経由で安全に起動できる
- destructive action に dry-run と confirmation がある
- 実行結果が UI と AI context で同じ意味を持つ
- 既存サービス経路を壊さない

## Notes

- まずは rename / import / queue のような低リスク操作から始める
- remove 系は後回しでもよい
- AI write tool は service wrapper として保つ
