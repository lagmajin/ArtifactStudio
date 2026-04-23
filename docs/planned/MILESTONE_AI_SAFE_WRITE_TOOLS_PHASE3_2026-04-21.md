# MILESTONE: AI Safe Write Tools - Phase 3 Execution

作成日: 2026-04-21

## 目的

AI の write 実行を、あとから追跡できる形で運用可能にする。

ここでは新しい編集能力を増やすのではなく、既存の safe write surface に監査・確認・失敗要約を足して、運用時の見通しを良くする。

---

## 重点対象

- `Artifact/include/Service/ArtifactProjectService.ixx`
- `Artifact/include/Service/ArtifactEffectService.ixx`
- `Artifact/include/Render/ArtifactRenderQueueService.ixx`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE2_2026-04-21.md`

---

## やること

### 1. Operation log

- どの tool が呼ばれたかを残す
- dry-run と実行を区別して記録する
- 変更対象の種類と件数を要約する

### 2. Confirmation log

- confirmation を出した理由を残す
- user approval の有無を記録する
- destructive action は明示的に区別する

### 3. Failure summary

- 失敗時の要因を短くまとめる
- missing target / invalid state / policy reject / service failure を分ける
- UI と AI context で同じ文言を使えるようにする

### 4. Policy summary

- 何が安全で、何が確認必須かを tool description と揃える
- 破壊的操作の扱いを固定する
- prompt context に渡す説明を簡潔にする

---

## 完了条件

- write 実行の履歴が追える
- confirmation の理由が読める
- 失敗理由が UI と AI context で一致する
- 安全な操作と破壊的操作の境界が説明できる

---

## File Tickets

- [`docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE3_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE3_2026-04-21.md)
- `WriteToolResult`
- `WriteToolConfirmation`
- `WriteToolExecutionPlan`

