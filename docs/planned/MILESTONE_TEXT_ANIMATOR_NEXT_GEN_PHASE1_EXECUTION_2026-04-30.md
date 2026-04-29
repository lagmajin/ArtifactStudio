# MILESTONE: Text Animator Next Gen - Phase 1 Execution

**Date**: 2026-04-30  
**Source**: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)  
**Parent Execution**: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md)

---

## Phase 1 Goal

Inspector 側の animator 操作をまず固める。
この段階では timeline や preset へ広げず、UI の見通しと責務の整理に集中する。

---

## Scope

### In

- animator type の選択導線
- add / remove / rename の操作導線
- selector 設定の一覧性
- animator ごとの enable / disable 視認性

### Out

- timeline keyframe 連携
- preset browser
- 新しい selector の追加
- GPU text work

---

## Tasks

### 1. Type Picker

- animator type を選ぶ導線を統一する
- 既存の property group と重複しないようにする
- 追加時の初期値を分かりやすくする

### 2. Animator List Surface

- どの animator が何個あるかを見やすくする
- enable / disable の状態を一目で分かるようにする
- rename の入口を近くに置く

### 3. Selector Overview

- Range Selector / Wiggly Selector のまとまりを整理する
- Start / End / Offset / Shape / Units の見え方を揃える
- Inspector の折りたたみ単位を見直す

---

## Done Criteria

- animator の種類と数が Inspector だけで把握できる
- add / remove / rename が迷わずできる
- selector 設定が長くなっても読める
- 次の Phase 2 で timeline exposure に入れる状態になる

