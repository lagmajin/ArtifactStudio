# MILESTONE: AI Workflow Automation Phase 1

作成日: 2026-04-21

## 目的

既存の `Artifact.AI.WorkspaceAutomation` を、AI の作業文脈取得と安全な workspace 操作用の入口として固める。

この段階では新しい操作体系は作らない。  
今ある snapshot / create / rename / import / queue / basic layer edit を、そのまま AI から扱いやすい形で安定化する。

---

## 現状把握

`Artifact/include/AI/WorkspaceAutomation.ixx` には既に次がある。

- `workspaceSnapshot`
- `projectSnapshot`
- `currentCompositionSnapshot`
- `selectionSnapshot`
- `renderQueueSnapshot`
- `listCompositions`
- `listProjectItems`
- `listCurrentCompositionLayers`
- `listRenderQueueJobs`
- project / composition / layer / asset / render queue の基本操作

つまり Phase 1 の主題は「新規実装」ではなく、「AI からの利用に耐える形へ整理すること」になる。

---

## Phase 1-1: Snapshot Contract

### Goal
AI が毎回同じ構造で文脈を読めるようにする。

### Tasks
- `workspaceSnapshot` の構造を固定する
- `projectSnapshot` / `currentCompositionSnapshot` / `selectionSnapshot` / `renderQueueSnapshot` の粒度をそろえる
- snapshot に policy や action hint を混ぜない
- read-only context と execution context を分ける

### Suggested Fields
- `project`
- `currentComposition`
- `selection`
- `renderQueue`
- `counts`
- `activeIds`
- `warnings`

---

## Phase 1-2: List and Lookup Tools

### Goal
AI が候補を列挙して、人間に説明できるようにする。

### Tasks
- composition list
- project item tree
- current composition layer list
- render queue job list
- item / layer / queue job lookup

### Notes
- `findProjectItemById`
- `projectItemPathById`
- `renderQueueJobByIndex`
- `renderQueueJobStatusAt`
- `renderQueueJobProgressAt`
- `renderQueueJobErrorMessageAt`

---

## Phase 1-3: Low-Risk Mutations

### Goal
安全性の高い編集を AI が呼べるようにする。

### Tasks
- `createProject`
- `createComposition`
- `importAssetsFromPaths`
- `selectLayer`
- `renameLayerInCurrentComposition`
- `duplicateLayerInCurrentComposition`
- `moveLayerInCurrentComposition`
- `setLayerVisibleInCurrentComposition`
- `setLayerLockedInCurrentComposition`
- `setLayerSoloInCurrentComposition`
- `setLayerShyInCurrentComposition`

### Notes
- ここは `dry-run` なしでもよいが、将来的には `AI Safe Write Tools` へ接続する
- まずは「壊しにくい」操作に限定する

---

## Phase 1-4: Queue Control

### Goal
レンダーキューを AI から確認・起動できるようにする。

### Tasks
- `addRenderQueueForCurrentComposition`
- `addRenderQueueForComposition`
- `addAllCompositionsToRenderQueue`
- `startRenderQueueAt`
- `pauseRenderQueueAt`
- `cancelRenderQueueAt`
- `resetRenderQueueJobForRerun`
- `resetCompletedAndFailedRenderQueueJobsForRerun`

### Notes
- render queue 変更は later phase で confirmation 化する
- job status と error message の返却を重視する

---

## Phase 1-5: Removal Surfaces

### Goal
破壊的操作の入口を把握し、後段の safety gate に接続する。

### Tasks
- `removeLayerFromCurrentComposition`
- `removeCompositionWithRenderQueueCleanup`
- `removeProjectItemById`
- `removeAllAssets`
- `removeRenderQueueAt`
- `removeAllRenderQueues`

### Notes
- これらは AI Safe Write Tools の confirmation へつなぐ
- まずは利用可能性を確認し、既定では自動実行しない

---

## Phase 1-6: Test Coverage

### Goal
tool schema と基本の invoke 経路が壊れないようにする。

### Tasks
- `WorkspaceAutomation` が schema に出ることを確認する
- snapshot / lookup / queue / rename / import の基本呼び出しを確認する
- invalid / missing / malformed 呼び出しを拒否する

### Related
- `Artifact/src/Test/ArtifactTestAIToolBridge.cppm`

---

## Completion Criteria

- AI が workspace snapshot を 1 回で取れる
- list / lookup / basic edit / queue control が利用できる
- 破壊的操作が後段安全化の対象として識別できる
- schema と invoke path の回帰が見える

## Notes

- 既存実装を壊さず、AI 向けに整えることが主目的
- ここで新しい UI を増やさない
- Phase 2 以降で dry-run / confirmation を厚くする
