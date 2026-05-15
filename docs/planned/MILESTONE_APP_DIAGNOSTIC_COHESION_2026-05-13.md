# Milestone: App Diagnostic Cohesion

> 2026-05-13 作成

ArtifactStudio 全体の diagnostics を、画面ごとに別の言い方や別の深さで散らさず、同じ文法で読めるようにするマイルストーン。

この文書は、Project Health / Problem View / App Debugger / Frame Debug View / Harness report / status chip / overlay warning をひとつの診断体験として揃えるための上位枠とする。

---

## Goal

- 問題が起きた時に、何が悪いかより先に「何を見ればいいか」が分かるようにする
- health / warning / error / fallback / stale / missing の語彙を揃える
- App Debugger と harness report と frame debug の読み方を合わせる
- 診断の主役を raw dump ではなく短い summary にする

---

## Scope

### In

- goal / now / warning / next の診断文法
- project health と problem view の語彙統一
- App Debugger の summary と trace の役割分担
- Frame Debug View の固定フレーム診断
- overlay warning と inline warning の優先順位
- harness report の読み方の統一

### Out

- 新しい global debug bus
- 既存の診断 surface を増殖させること
- 単なるログ出力の追加
- render backend の低レベル改造

---

## Design Rules

1. `goal` は 1 文で言えること
2. `warning` は詳細より先に置くこと
3. `next` は 1 個に絞ること
4. `raw trace` は補助であり本体ではないこと
5. `error` は観測可能で再現可能な語彙で書くこと

---

## Phases

### Phase 1: Vocabulary Alignment

- Project Health / Problem View / App Debugger の見出し語彙を揃える
- `warning` と `error` の使い分けを固定する
- status chip の色と意味を診断側で統一する

### Phase 2: Summary-First Diagnostics

- 先頭に goal-first summary を出す
- detailed trace を後ろに回す
- problem view から debugger へ同じ読み方で飛べるようにする

### Phase 3: Frame and Harness Bridge

- Frame Debug View の要約を harness report と合わせる
- frame 単位の失敗理由を短く返す
- compare / pin / copy の位置を揃える

### Phase 4: Cross-Surface Warning Consistency

- asset / timeline / composition / render / playback の warning 表示をそろえる
- inline warning と dedicated diagnostics panel の責務を固定する
- 観測と操作の距離を短くする

---

## Success Criteria

- 問題を見た時の言い方が surface ごとに変わらない
- summary から詳細へ同じ文脈で降りられる
- App Debugger と Project Health と Frame Debug View が同じ語彙で読める
- warning が埋もれず、raw dump が主役にならない
- 失敗時に次の調査先がすぐ分かる

---

## Related Docs

- [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)
- [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)

---

## Next Step

Phase 1 で使う warning / error / next の文言セットを先に固定する。
