# Debug Render Harness - Phase 3 Execution

**Date**: 2026-04-30  
**Source**: [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)  
**Parent Phase 2**: [`MILESTONE_DEBUG_RENDER_HARNESS_PHASE2_EXECUTION_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_PHASE2_EXECUTION_2026-04-30.md)

---

## Phase 3 Goal

失敗した smoke をその場限りにせず、再比較できる capture bundle と report として残す。

この段階ではレンダラー自体を増やさず、`FrameDebugSnapshot` を中心に `what happened` を保存する。

---

## Scope

### In

- capture bundle
- text-first report
- save / export path
- diagnostics summary bridge
- failure reason retention

### Out

- automatic regression gate
- new render backend
- heavy binary export format
- crash handler redesign

---

## Tasks

### 1. Capture Bundle Shape

bundle に入れるものを固定する。

- `FrameDebugSnapshot`
- selected preset
- particle draw state
- video decode state
- viewport / RTV / backend state
- last error / skipped reason
- optional note / tag

### 2. Text Report Shape

report は text-first でよい。

- header に date / preset / frame / composition
- summary に `ok / skipped / failed`
- media section に particle / video / blend を並べる
- last error と skipped reason を最後にまとめる

### 3. Save / Export Path

- 失敗したときに 1 回で保存できる
- 保存先と timestamp を report に出す
- 共有時に bundle id が分かるようにする

### 4. Diagnostics Bridge

- `FrameDebugViewWidget` から report を読める
- `ArtifactDebugConsoleWidget` から copy できる
- `AppDebuggerWidget` の summary から「保存済み bundle」を追える

---

## Report Fields

### Common

- bundle id
- created at
- preset name
- frame number
- composition name
- selected layer name
- render backend

### Media State

- particle status
- video decode status
- blend status
- overlay status
- visible / skipped / failed reason

### Diagnostics

- trace frame count
- last error
- cache / RTV / viewport notes
- source frame / target frame for video

---

## Done Criteria

- failed smoke を report として保存できる
- bundle report を copy しやすい
- particle / video / blend の原因が bundle に残る
- frame debug と diagnostics で同じ情報を読める

