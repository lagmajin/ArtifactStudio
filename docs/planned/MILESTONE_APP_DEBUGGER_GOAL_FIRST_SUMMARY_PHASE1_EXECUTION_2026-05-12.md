# App Debugger Goal-First Summary - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)

---

## Phase 1 Goal

`AppDebuggerWidget` の上部サマリを `goal / now / warning / next` に固定する。

この段階では色や装飾よりも、見出しの意味を揃えることを優先する。

---

## Scope

### In

- top strip の見出し固定
- summary 文法の固定
- warning-first ordering
- raw text の補助化

### Out

- quick action の全面再設計
- report bundle の新規形式
- backend / diagnostics model の全面変更

---

## Tasks

### 1. Goal Summary

- 画面全体の目的を 1 行で言えるようにする
- harness report の `goal` と同じ語彙を使う

### 2. Now / Warning / Next

- `now` は現在値
- `warning` は異常の要点
- `next` は次の一手

を個別に見えるようにする

### 3. Summary Ordering

- warning があるときは先に出す
- 通常時は静かな summary にする
- raw dump は折りたたむ

### 4. Bridge Consistency

- `Debug Render Harness`
- `Frame Debug View`
- `ArtifactDebugConsoleWidget`

で同じ言い回しを保つ

---

## Done Criteria

- 上部だけ見て、今の焦点が分かる
- warning があるときに先に見える
- harness report と App Debugger の語彙がぶれない

