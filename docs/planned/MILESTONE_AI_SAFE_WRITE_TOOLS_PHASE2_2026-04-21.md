# MILESTONE: AI Safe Write Tools Phase 2

作成日: 2026-04-21

## 目的

AI の write surface に dry-run / confirmation / execution plan を導入し、破壊的操作を安全に扱えるようにする。

この段階では、既存 service への呼び出し自体を増やすよりも、呼び出し前後の安全性を固める。

---

## Phase 2-1: Dry-run Result

### Goal
実行前に影響範囲を返す。

### Contents
- `operationName`
- `targetIds`
- `riskLevel`
- `affectedCounts`
- `wouldChange`
- `wouldFail`
- `warnings`

### Risk Levels
- `Low`
- `Medium`
- `High`

---

## Phase 2-2: Confirmation Payload

### Goal
人間の最終確認に必要な情報を返す。

### Contents
- operation name
- targets
- reason for confirmation
- estimated impact
- undo availability
- preview message

### Notes
- destructive action は confirmation を必須にする
- prompt 文と UI 文言を同じ意味にする

---

## Phase 2-3: Execution Plan

### Goal
write 実行を 1 つの plan として扱う。

### Contents
- input snapshot
- dry-run summary
- confirmation result
- service call list
- post-execution snapshot

### Notes
- `ArtifactProjectService`
- `ArtifactEffectService`
- `ArtifactRenderQueueService`
- これらへの対応を plan に記録する

---

## Phase 2-4: Removal Gate

### Goal
remove 系操作を安全化する。

### Tasks
- remove layer
- remove composition
- remove project item
- remove render queue job
- remove all assets / queues
- confirmation 必須

### Notes
- まずは AI が「消してよいか」を説明できるようにする
- 実行は人間の確認後に限定する

---

## Phase 2-5: Feedback and Logging

### Goal
失敗や中断の理由を追えるようにする。

### Tasks
- operation log
- confirmation log
- failure reason summary
- retry suggestion

### Output
- `SafeWriteAuditEntry`
- `SafeWriteFailureSummary`

---

## Completion Criteria

- dry-run が影響範囲を返す
- confirmation payload が UI と AI 両方で使える
- execution plan が write の流れを表す
- removal 系が confirmation gate を通る
- 実行ログが残る

## Notes

- ここでは新しい編集機能は増やさない
- 既存 service の wrapper に留める
- 低リスク操作と破壊的操作をはっきり分ける
