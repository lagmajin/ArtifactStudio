# App Diagnostic Cohesion - Phase 1 Execution

**Date**: 2026-05-13

**Source**: [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)

---

## Phase 1 Goal

Project Health / Problem View / App Debugger / Frame Debug View の見出し語彙を、まずは `goal / now / warning / next` の形で揃える。

この段階では診断の細部を増やすよりも、surface ごとに「何が先頭に来るべきか」を一致させることを優先する。

---

## Scope

### In

- goal-first summary
- now / warning / next の固定
- warning-first ordering
- Project Health と App Debugger の語彙統一
- Frame Debug View の見出しの読み方統一

### Out

- new global debug bus
- diagnostics surface の増殖
- raw log 主体への置き換え
- render backend の大改造

---

## Current Focus Surfaces

Phase 1 では、まず次の surface を優先する。

1. `ArtifactProjectHealthDashboard`
2. `ArtifactProblemViewWidget`
3. `AppDebuggerWidget`
4. `FrameDebugViewWidget`

この順番で、まず summary の先頭語彙を合わせる。

---

## Shared Summary Template

Phase 1 で揃える診断 summary は、まずこの形を標準にする。

```text
goal: <1 sentence>
now: <current state in 1 short line>
warning: <main issue or "none">
next: <single next action>
```

補助情報はこの後ろに回し、raw trace は折りたたみか詳細領域に寄せる。

---

## First Pass Order

1. `ArtifactProjectHealthDashboard`
2. `ArtifactProblemViewWidget`
3. `AppDebuggerWidget`
4. `FrameDebugViewWidget`

この順で、各 surface の top strip と status chip を見比べる。
まず `warning` の出し方を揃え、次に `goal` と `next` の短さを揃える。

---

## Surface Roles

- `Project Health`: 今の project 全体の health を短く返す
- `Problem View`: 失敗理由と次の一手を短く返す
- `App Debugger`: state / trace / frame を同じ summary 文法で束ねる
- `Frame Debug View`: 1 フレーム単位の warning と比較導線を返す

同じ summary を別の surface で再掲してもよいが、役割は変えない。
`warning` は先頭に出し、`trace` は補助へ回す。

---

## Vocabulary Set

Phase 1 でまず固定する文言は次の4つ。

- `goal`
- `now`
- `warning`
- `next`

必要に応じて `status` を chip に落とすが、summary の主語はこの4語に寄せる。

---

## Working Rules

1. `goal` は 1 文で言う
2. `warning` は詳細より先に出す
3. `next` は 1 個に絞る
4. `now` は現在の状態だけを短く出す
5. raw trace は補助に回す

---

## Tasks

### 1. Summary Vocabulary

- `goal / now / warning / next` の文法を surface ごとに見比べる
- `status` と `shortReason` をこの語彙に寄せる
- 見出しが変わっても意味が変わらないようにする

### 2. Warning Ordering

- warning がある時は先に見せる
- 問題がない時は静かな summary にする
- Error と warning の役割を固定する

### 3. Bridge Consistency

- Project Health
- Problem View
- App Debugger
- Frame Debug View

で同じ読み方を保つ

### 4. Copyable Summary

- そのまま copy しても意味が落ちない short summary を持たせる
- long text は補助に回す
- report / bundle の見出し語彙を揃える

---

## First Files

1. `Artifact/src/Widgets/ArtifactProjectHealthDashboard.cppm`
2. `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`
3. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
4. `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
5. `ArtifactCore/include/Diagnostics/DiagnosticEngine.ixx`

---

## Done Criteria

- 主要 diagnostics surface の先頭を見た時に、今の焦点が短く読める
- warning がある時に先頭へ出る
- Project Health と App Debugger と Frame Debug View の語彙がぶれない
- raw dump より summary が先に読まれる

---

## Next Step

Phase 2 では、summary から詳細へ降りる導線を揃える。
