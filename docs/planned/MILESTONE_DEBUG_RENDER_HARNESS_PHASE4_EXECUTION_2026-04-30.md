# Debug Render Harness - Phase 4 Execution

**Date**: 2026-04-30  
**Source**: [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)  
**Parent Phase 3**: [`MILESTONE_DEBUG_RENDER_HARNESS_PHASE3_EXECUTION_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_PHASE3_EXECUTION_2026-04-30.md)

---

## Phase 4 Goal

smoke を継続的な regression gate にする。

この段階では新しい描画機能を追加せず、再現した不具合が次回以降も同じ基準で落ちるかを確認できる形にする。

---

## Scope

### In

- particle smoke gate
- video smoke gate
- transparent output gate
- skipped reason gate
- support / release note

### Out

- harness feature expansion
- new UI polish
- backend replacement
- deep automated comparison

---

## Tasks

### 1. Gate Definitions

- particle visible
- video frame visible
- transparent output must have explicit reason
- skipped draw must not be silent

### 2. Gate Records

- pass / fail / skipped を記録する
- bundle id と timestamp を残す
- when / why / where を report に残す

### 3. Release Note Bridge

- critical render/media bugs の修正時には smoke で確認する
- 修正報告から smoke check へ戻れるようにする
- support sharing 用に短い note を生成する

### 4. Manual Gate Workflow

- harness を開く
- preset を選ぶ
- expected visible / skipped / failed を確認する
- report を保存する

---

## Gate Matrix

### Particle

- visible over dark background
- visible over light background
- no silent skip when alive count > 0

### Video

- frame 0 visible
- seek mid-frame visible
- transparent output explained
- decode pending not mistaken for failure

### Blend

- overlay / blend output visible when inputs exist
- empty input path is explicit

---

## Done Criteria

- each smoke has a named gate outcome
- failure reasons are recorded consistently
- release support can refer to a saved bundle
- regression checks are repeatable without re-learning the app

