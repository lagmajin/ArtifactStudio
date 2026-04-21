# MILESTONE: AI Workflow Automation Phase 2

作成日: 2026-04-21

## 目的

`WorkspaceAutomation` の snapshot 返却と write 返却を、AI が安定して読める契約に揃える。

この段階では新しい操作は増やさない。  
既存の snapshot / list / lookup / write の返却形を整えて、dry-run や confirmation と繋ぎやすくする。

---

## Phase 2-1: Snapshot Schema

### Goal
workspace snapshot の構造を固定する。

### Current Base
現状の `workspaceSnapshot` は概ね次を返している。

- `project`
- `selection`
- `currentComposition`
- `renderQueue`

### Target Schema
- `project`
  - `available`
  - `projectName`
  - `projectPath`
  - `assetsPath`
  - `compositionCount`
  - `projectItemCount`
- `currentComposition`
  - `available`
  - `id`
  - `name`
  - `layerCount`
- `selection`
  - `available`
  - `activeCompositionId`
  - `currentLayerId`
  - `selectedLayerCount`
  - `selectedLayers`
- `renderQueue`
  - `available`
  - `jobCount`
  - `totalProgress`
  - `jobs`

### Notes
- 返却キーは固定する
- AIContext と UI 表示が同じ意味を持つようにする
- read-only と write result を混ぜない

---

## Phase 2-2: Derived Summary Fields

### Goal
AI が「何が起きているか」を短く返せるようにする。

### Tasks
- `counts`
- `activeIds`
- `warnings`
- `failureSummary`
- `selectionSummary`

### Suggested Fields
- `counts.compositions`
- `counts.layers`
- `counts.assets`
- `counts.renderQueueJobs`
- `activeIds.project`
- `activeIds.composition`
- `activeIds.layer`
- `warnings.missingProject`
- `warnings.noComposition`
- `warnings.emptySelection`

---

## Phase 2-3: Write Result Schema

### Goal
write tool の実行結果を統一する。

### Tasks
- 成功 / 失敗 / 部分成功を区別する
- changed ids を返す
- failed ids を返す
- human readable message を返す
- 次に必要な UI action を返す

### Output
- `SafeWriteResult`
- `SafeWriteChangeSummary`
- `SafeWriteNextAction`

---

## Phase 2-4: Dry-run Friendly Payloads

### Goal
実行前に影響範囲を見せる。

### Tasks
- before snapshot
- after preview snapshot
- affected ids
- risk level
- confirmation prompt

### Notes
- dry-run は output を read-only で返す
- 破壊的操作は confirmation とセットで扱う

---

## Phase 2-5: AI Context Alignment

### Goal
workspace snapshot と AIContext のズレをなくす。

### Tasks
- snapshot 由来の要約を AIContext に再利用する
- project summary を AIContext の単なる文章ではなく構造化された source として扱う
- tool result と prompt context の語彙を揃える

---

## Completion Criteria

- workspace snapshot の主要キーが固定されている
- counts / warnings / activeIds のような派生 summary がある
- write result に changed / failed / next action がある
- dry-run 用の before / preview が返せる
- AIContext と snapshot が同じ情報源を参照する

## Notes

- ここではまだ UI 追加はしない
- 既存の `WorkspaceAutomation` を壊さない
- Phase 3 以降で confirmation UI を厚くする
