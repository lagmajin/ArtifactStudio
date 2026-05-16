# App Surface Cohesion - Phase 3 Execution

**Date**: 2026-05-13

**Source**: [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)

---

## Phase 3 Goal

empty state を、ただの余白ではなく「次に何をするか」を案内する surface として揃える。

この段階では見出しや strip の情報量よりも、何も選ばれていない時に迷いにくいかを優先する。

---

## Scope

### In

- empty state の文法
- action prompt の短文化
- `import / open / select / inspect / navigate` の導線
- selected / unselected の文言統一
- diagnostics warning との見た目分離

### Out

- summary strip の再設計
- global interaction model の刷新
- new global signal/slot
- backend / core 再設計

---

## Current Focus Surfaces

Phase 3 では、次の surface を順に見る。

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

Phase 1 と Phase 2 で固めた文法を、空状態でも崩さないようにする。

---

## Working Rules

1. empty state は説明文ではなく案内にする
2. 次の一手は 1 個に絞る
3. diagnostics の warning と同じ見た目にしない
4. action prompt は短くする
5. selection がない時でも不安にしない

---

## Shared Empty Pattern

Phase 3 で揃える empty state は、まずこの形を標準にする。

```text
<action> to continue
<one short hint>
```

---

## First Pass Order

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

この順で、空状態の案内と次アクションを見比べる。

---

## Surface Roles

- `Asset Browser`: import / open / navigate の入口を案内する
- `Project Manager`: select / inspect / open の入口を案内する
- `Composition Editor`: open / select / edit の入口を案内する
- `Timeline`: select / inspect / navigate の入口を案内する
- `App Debugger`: inspect / compare / open の入口を案内する

同じ文言をそのまま再掲してもよいが、surface ごとの主動作は崩さない。

---

## Draft Empty Templates

Phase 3 で揃える empty state のたたき台。

### Asset Browser Draft

- `Import files to continue`
- `Select a folder to browse assets`

### Project Manager Draft

- `Open a project to continue`
- `Select an item to inspect details`

### Composition Editor Draft

- `Open a composition to start editing`
- `Select a layer to continue`

### Timeline Draft

- `Select a layer to continue`
- `Navigate to a timeline item`

### App Debugger Draft

- `Open a report to inspect details`
- `Compare captures to continue`

---

## Tasks

### 1. Empty State Grammar

- empty state の文を action-first にする
- 1 文目で次の行動が分かるようにする
- 2 文目は短い補助 hint に限定する

### 2. Action Proximity

- import / open / select / inspect / navigate の入口を近づける
- 画面ごとの主要 action を 1 個に絞る
- 余計な説明を足さない

### 3. Selection None State

- selection が 0 の時の文言を統一する
- `nothing selected` 系の表記を surface ごとに増やしすぎない
- 何もない状態でも前に進める文にする

### 4. Diagnostic Separation

- diagnostics warning とは色と役割を分ける
- empty state を warning の代わりにしない
- error の代替表示にしない

---

## Validation Checklist

- [ ] どの surface でも empty state が action-first で読める
- [ ] 次の一手が 1 個に絞られている
- [ ] selection なし状態の文言が surface ごとにバラけない
- [ ] empty state と diagnostics warning が混ざらない
- [ ] 何もなくても「次に何をすればいいか」が分かる

---

## Related Docs

- [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [`MILESTONE_APP_SURFACE_COHESION_PHASE1_EXECUTION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_PHASE1_EXECUTION_2026-05-13.md)
- [`MILESTONE_APP_SURFACE_COHESION_PHASE2_EXECUTION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_PHASE2_EXECUTION_2026-05-13.md)
- [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)

---

## Next Step

Phase 4 では、Project / Asset / Timeline / Composition / Debugger の文法を最後に合わせる。
