# Harness Engineering - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)

---

## Phase 1 Goal

report contract を goal-first に揃える。

この段階では自動比較や重い automation を増やさず、text-first report と summary の語彙を固定する。

---

## Scope

### In

- goal / expected / actual / next action の定義
- report header の固定
- failure taxonomy の固定
- capture/save/copy との整合

### Out

- regression gate の自動化
- 新しい preset の大量追加
- backend 差し替え

---

## Tasks

### 1. Report Vocabulary

- goal を 1 文で書けるようにする
- expected と actual を並べる
- next action を 1 つに絞る

### 2. Summary Alignment

- `status` と `shortReason` を goal 文脈に合わせる
- `skipped` / `failed` / `degraded` を report 内で明示する

### 3. Copy / Save Compatibility

- copy した text だけで次の行動が分かるようにする
- save した bundle でも同じ vocabulary を読めるようにする

### 4. Bridge Points

- `DebugRenderHarnessWidget`
- `AppDebuggerWidget`
- `FrameDebugViewWidget`

の 3 面で同じ語彙を使う前提を作る

---

## Done Criteria

- report を見れば何を証明したいかが読める
- report を copy しても意味が落ちない
- failure reason と next action が分離している

