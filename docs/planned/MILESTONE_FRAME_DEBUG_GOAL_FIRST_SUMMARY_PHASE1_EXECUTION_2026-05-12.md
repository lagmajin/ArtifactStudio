# Frame Debug Goal-First Summary - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md)

---

## Phase 1 Goal

`FrameDebugViewWidget` の上部サマリを `goal / frame / warning / next` に固定する。

この段階では compare の高度化よりも、現在フレームを短く読めることを優先する。

---

## Scope

### In

- top summary の文法固定
- warning-first ordering
- pass / resource / compare の簡略見出し
- copyable summary の整備

### Out

- deep compare engine
- new GPU probes
- renderer redesign

---

## Tasks

### 1. Goal Summary

- 何を見ているかを 1 文で言えるようにする
- harness / app debugger との語彙を揃える

### 2. Frame / Warning / Next

- frame 番号と timestamp を先頭に置く
- warning がある場合は最初に出す
- next は 1 個に絞る

### 3. Compare Bridge

- current / previous / target を読めるようにする
- compare 状態が summary から見えるようにする

### 4. Copy Compatibility

- summary を copy しても意味が落ちない
- report / bundle と同じ見出しを使う

---

## Done Criteria

- 開いた瞬間に今のフレームの判断が読める
- warning があれば先に見える
- App Debugger と Harness の語彙とぶれない

