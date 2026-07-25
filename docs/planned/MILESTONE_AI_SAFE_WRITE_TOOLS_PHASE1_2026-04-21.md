# MILESTONE: AI Safe Write Tools Phase 1

作成日: 2026-04-21

## 目的

既存 service 群に対する AI 用 write surface を、低リスク操作から順に固定する。

この段階では confirmation UI や dry-run を全面導入しない。  
まずは「どの操作が既存 service のどのメソッドに対応するか」を明確にし、AI bridge から呼べる形に揃える。

---

## 現状把握

既存の主要 surface はすでにある。

- `ArtifactProjectService`
- `ArtifactEffectService`
- `ArtifactRenderQueueService`

それぞれに rename / duplicate / import / move / queue / pause / cancel / set property 系の操作があるため、Phase 1 では新しい業務ロジックを作る必要はほぼない。

---

## Phase 1-1: Low-Risk Mappings

### Goal
AI が最も壊しにくい操作を使えるようにする。

### Mappings
- `createProject` -> project service
- `createComposition` -> project service
- `importAssetsFromPaths` -> project service
- `renameLayerInCurrentComposition` -> project service
- `duplicateLayerInCurrentComposition` -> project service
- `moveLayerInCurrentComposition` -> project service
- `selectLayer` -> project service
- `setLayerVisibleInCurrentComposition` -> project service
- `setLayerLockedInCurrentComposition` -> project service
- `setLayerSoloInCurrentComposition` -> project service
- `setLayerShyInCurrentComposition` -> project service

### Notes
- 低リスク操作を先に AI へ出す
- 既存 undo / redo に乗るものを優先する

---

## Phase 1-2: Render Queue Mappings

### Goal
AI がレンダーを安全に起動・制御できるようにする。

### Mappings
- `addRenderQueueForCurrentComposition`
- `addRenderQueueForComposition`
- `addAllCompositionsToRenderQueue`
- `startRenderQueueAt`
- `pauseRenderQueueAt`
- `cancelRenderQueueAt`
- `resetRenderQueueJobForRerun`
- `resetCompletedAndFailedRenderQueueJobsForRerun`

### Notes
- job 状態と error message を AI context に返す
- start / pause / cancel は確認対象にする

---

## Phase 1-3: Effect Mappings

### Goal
AI がエフェクト操作を安全に実行できるようにする。

### Mappings
- `addEffectToLayerInCurrentComposition`
- `removeEffectFromLayerInCurrentComposition`
- `setEffectEnabledInLayerInCurrentComposition`
- `moveEffectInLayerInCurrentComposition`
- `setEffectProperty`

### Notes
- effect property edit は提案系の出力先としても使える
- remove / move は later phase で confirmation 化する

---

## Phase 1-4: Removal Routing

### Goal
破壊的操作を後段の safety gate に確実に繋ぐ。

### Mappings
- `removeLayerFromComposition`
- `removeComposition`
- `removeCompositionWithRenderQueueCleanup`
- `removeProjectItem`
- `removeProjectItemById`
- `removeAllAssets`
- `removeRenderQueueAt`
- `removeAllRenderQueues`

### Notes
- Phase 1 では「存在を把握する」ことが主目的
- 実行は later phase の confirmation 経由に寄せる

---

## Phase 1-5: Snapshot-Driven Feedback

### Goal
write 実行後の状態変化を AI が説明できるようにする。

### Tasks
- write 前 snapshot
- write 後 snapshot
- changed ids
- failed ids
- human readable message

### Output
- `SafeWriteResult`
- `SafeWriteChangeSummary`

---

## Completion Criteria

- 低リスク操作の write mapping が固定されている
- render queue の基本操作が AI bridge から呼べる
- effect 操作の入口が見える
- removal 系が安全化対象として分離されている
- write 前後で説明可能な結果が返る

## Notes

- この段階で UI を増やしすぎない
- 既存 service の薄いラッパーとして保つ
- dry-run / confirmation の本格化は Phase 2 に回す

## 2026-07-25 実装監査

Phase 1 の操作入口は `WorkspaceAutomation` の descriptor / invoke routing として登録され、`ArtifactCore::CommandIR` / `CommandResult` および `CommandIRExecutor` から一部の low-risk、effect、render queue 操作を呼び出せる状態になっている。`undoLabel` と成功／失敗メッセージも既存の command result に含まれる。

一方、Phase 1-5 の `SafeWriteResult` / `SafeWriteChangeSummary`、write 前後 snapshot、changed／failed ids をまとめる専用 DTO はまだ存在しない。また、Phase 1 の全 mapping が一つの専用 registry に網羅されているわけではなく、WorkspaceAutomation と CommandIR の対応範囲に依存している。したがって、Phase 1-1〜1-4 は「主要 mapping 実装済み・未網羅」、Phase 1-5 は未実装として扱う。runtime の実操作確認も未実施。
