# MILESTONE: AI Safe Write Tools

作成日: 2026-04-21
最終更新: 2026-08-15

**進捗状態:** Phase 1／3、汎用 dry-run／execution plan、confirmation・Undo、監査ログの主要経路は実装済み。UI／runtime 統合検証が残る。

### 実装状況（2026-07-25 確認）

WorkspaceAutomation／CommandIR の service wrapper、破壊操作の confirmation message、各種 Undo command／macro、batch rename／move、render queue 操作を確認した。残課題は `WriteToolDryRunResult`／`WriteToolExecutionPlan`／`SafeWriteAuditEntry` 相当の共通契約、operation／confirmation log、UI と AI context の統一 payload。

## 2026-08-15 現行コード監査

- `CommandIR` に `SafeWriteDryRunResult`、`SafeWriteConfirmationPayload`、`SafeWriteExecutionPlan`、risk level、undo availability の契約を確認した。
- `WorkspaceAutomation` は remove layer／composition／asset／project item／render queue の dry-run、confirmation gate、Undo 可能性の表示、成功／拒否の audit entry を持つ。
- `SafeWriteAuditLog` は in-memory snapshot と JSON save／load の tool を公開しているため、旧来の「共通契約／監査ログ未実装」という記述は更新が必要。
- ただし全 write 操作が同一 execution plan に揃っているか、UI の confirmation dialog と AI context の payload が同一表示になるか、監査ログの実運用・runtime 検証は未確認。

判定: **Phase 1〜4 の core 契約・主要 destructive gate・Undo・監査ログは実装済み。全操作の統一、UI／AI payload 整合、runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`CommandIR` は `SafeWriteDryRunResult`、`SafeWriteConfirmationPayload`、`SafeWriteExecutionPlan`、risk level、undo availability を持つ。`WorkspaceAutomation` には layer／composition／asset／project item／render queue の dry-run、confirmation gate、Undo可否表示、成功／拒否の audit entry があり、`SafeWriteAuditLog` の JSON save／load も公開されている。

未完了・未検証なのは、全write操作を同一execution planへ統一すること、UI confirmation dialogとAI contextのpayload一致、監査ログの長期運用、runtime検証である。Core契約と主要gateは実装済み、全操作整合と受入れは pending とする。

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
