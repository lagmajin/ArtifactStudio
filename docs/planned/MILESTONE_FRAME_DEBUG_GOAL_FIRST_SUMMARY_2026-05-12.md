# Frame Debug / Goal-First Summary

**Date**: 2026-05-12

`Frame Debug View` を、単なる pass / resource inspector ではなく、`goal -> frame -> warning -> next` の順でフレーム単位の判断を返す面へ寄せるためのマイルストーン。

このマイルストーンは、`App Debugger` と `Debug Render Harness` のあいだで同じ語彙を使えるようにする橋渡しでもある。

---

## Goal

- どのフレームを見ているかを短く示す
- そのフレームの warning を先に見せる
- 前後フレーム比較の入口を読めるようにする
- harness report と同じ文法で summary を作る

---

## Scope

### In

- frame summary の goal-first 化
- warning-first ordering
- pass / resource / compare / capture の導線整理
- copyable summary の整備

### Out

- new render backend
- deep GPU inspector
- product UI redesign
- global event bus の追加

---

## Design Rules

1. frame summary は 1 画面で読めること
2. warning は詳細より先に出すこと
3. compare は現在フレームの理解を邪魔しないこと
4. copy した summary がそのまま共有できること
5. raw dump は補助に回すこと

---

## Phases

### Phase 1: Summary Vocabulary

- `goal / frame / warning / next` の語彙を固定する
- `pass / resource / attachment / compare` の見出しを整理する
- `FrameDebugSnapshot` の要点を短く読む

### Phase 2: Comparison Bridge

- current / previous / target を明示する
- compare の結果を summary から見えるようにする
- scrub / step / pin の入口を揃える

### Phase 3: Report Bridge

- `Debug Render Harness` の report と読み方を合わせる
- `AppDebuggerWidget` の summary と接続する
- saved bundle を再参照できるようにする

---

## Related Docs

- [`MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`](./MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md)
- [`MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md`](./MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md)
- [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)

---

## Next Step

Phase 1 実行メモを作って、`FrameDebugViewWidget` の top summary を `goal / frame / warning / next` に揃える。
