# App Surface Cohesion - Phase 1 Execution

**Date**: 2026-05-13

**Source**: [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)

---

## Phase 1 Goal

Project / Asset / Timeline / Composition / Debugger の header と summary の読み方を、まずは `current / recent / selection / status` の語彙で揃える。

この段階では全面リデザインよりも、各 surface の先頭 1 画面を見た時に「今どこにいるか」がすぐ分かることを優先する。

---

## Scope

### In

- current context line
- recent / selection summary
- status chip / note の役割整理
- empty state の入口文
- Asset Browser の hub-style summary

### Out

- 全 surface の完全統一
- QtCSS ベースの新テーマ
- 新しい global signal/slot
- backend / core 再設計

---

## Current Focus Surfaces

Phase 1 では、まず次の surface を優先する。

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

この順番で、「current の見え方」と「次の行動」が読めるかを揃える。

---

## Working Rules

1. current は 1 行で短く出す
2. recent は 3 件前後に抑える
3. selection は件数と要点だけを先に出す
4. status は chip / note で先に見せる
5. empty state は案内として扱う

---

## Execution Order

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

この順で進める。
先に `Asset Browser` を基準文法として固めてから、他 surface をそこへ寄せる。

---

## Tasks

### 1. Header Vocabulary

- current / recent / selection / status の文言を surface ごとに見比べる
- 同じ意味に別の言い方を使わないようにする
- tooltip も同じ語彙で寄せる
- header の先頭に来る語を `current` に寄せる
- 補助情報は `recent` と `selection` に分ける
- `status` は chip / badge 側へ逃がす

### 2. Asset Browser Hub

- 左ペインの `Library Hub` を phase 1 の基準面として扱う
- recent / favorites / sources の短い提示を固定する
- folder navigation の入口を同じ文脈で見せる
- 画面上部で `current` の位置を先に見せる
- `recent` は 3 件前後までに収める
- `selection` は件数と代表名だけにする

### 3. Selection Summary

- 選択件数を短く見せる
- current selection がない時の文言を統一する
- 何も選ばれていない時に不安になりにくい文法へ寄せる
- `0 selected` 相当の表記を surface ごとに変えない
- 複数選択時は件数を先に出す
- 詳細な選択内容は summary の後ろへ回す

### 4. Empty State

- 空時の案内を surface ごとに作る
- 何もない時でも「次に何をすればいいか」が読めるようにする
- empty state は説明文ではなく action prompt として書く
- `import / open / select / inspect / navigate` のどれか 1 つを先頭に置く
- diagnostics の warning とは見た目を分ける

---

## First Files

1. `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
2. `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
3. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
4. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
5. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

---

## Validation Checklist

- [ ] Asset Browser を開いたとき `current` が 1 行で読める
- [ ] Project Manager の selection summary が長くならない
- [ ] Composition Editor の現在地表示が tooltip なしでも通じる
- [ ] Timeline の header 語彙が Composition Editor とぶつからない
- [ ] App Debugger の summary が diagnostics 側の語彙とぶれない
- [ ] empty state が「空白」ではなく「次の行動」として読める

---

## Step 3: Composition Editor / Timeline

`Composition Editor` と `Timeline` は、Phase 1 の最後に同じ文法へ寄せる。
ここでは操作そのものを変えず、header / selection / status の読み方だけを揃える。

### Composition Editor Draft

- `current: Composition Editor`
- `recent: <recent comps or selections>`
- `selection: <count> layers`
- `status: playback / mask / transform`

### Timeline Draft

- `current: Timeline`
- `recent: <recent comps or active sequences>`
- `selection: <count> clips or layers`
- `status: keyframe / range / sync`

### Composition Editor Checklist

- [ ] current line を 1 行で読めるようにする
- [ ] selection summary を layer count 中心にする
- [ ] status chip を playback / mask / transform に寄せる
- [ ] empty state を `Open a composition to start editing` 系の案内にする
- [ ] tooltip がなくても現在地が分かるか確認する

### Timeline Checklist

- [ ] current line を timeline の場所として 1 行で示す
- [ ] selection summary を clip / layer count 中心にする
- [ ] status chip を keyframe / range / sync に寄せる
- [ ] empty state を `Select a layer to continue` 系の案内にする
- [ ] header が Composition Editor と同じ順序で読めるか確認する

---

## Step 4: App Debugger

`App Debugger` は surface cohesion の最後に置く。
ここでは `current / recent / selection / status` ではなく、診断側の `goal / now / warning / next` を優先する。

### App Debugger Draft

- `goal: inspect current app state`
- `now: <current state in 1 short line>`
- `warning: <main issue or "none">`
- `next: <single next action>`

### App Debugger Checklist

- [ ] top strip が `goal / now / warning / next` で読める
- [ ] `status` は chip に退避し、summary の主語を増やさない
- [ ] `warning` がある時は先頭へ出る
- [ ] `next` を 1 個に絞る
- [ ] `Frame` / `State` / `Trace` の見出しを短く保つ
- [ ] surface cohesion 側の `status` と diagnostic 側の `warning` を混同しない

### App Debugger Bridge Note

- `goal` は調査目的
- `now` は現在の状態
- `warning` は問題の要点
- `next` は次の一手

---

## Draft Vocabulary

Phase 1 でまず固定する語彙のたたき台。
実装時は surface ごとの固有名を入れてよいが、順序は崩さない。

### Shared Header Pattern

```text
current: <one line>
recent: <up to 3 items>
selection: <count + short note>
status: <chip or badge>
```

### Asset Browser

- `current: Library Hub`
- `recent: <up to 3 recent folders>`
- `selection: <count> selected`
- `status: sync / missing / favorite`

### Project Manager

- `current: Project View`
- `recent: <recent comps or imported items>`
- `selection: <count> items`
- `status: missing / unused / synced`

### Composition Editor

- `current: Composition Editor`
- `recent: <recent comps or selections>`
- `selection: <count> layers`
- `status: playback / mask / transform`

### Timeline

- `current: Timeline`
- `recent: <recent comps or active sequences>`
- `selection: <count> clips or layers`
- `status: keyframe / range / sync`

### App Debugger

- `current: App Debugger`
- `recent: <recent reports or captures>`
- `selection: <count> frames or items`
- `status: goal / warning / next`

### Empty State Pattern

```text
<action> to continue
<one short hint>
```

Examples:

- `Import files to continue`
- `Select an item to inspect details`
- `Open a composition to start editing`

---

## Progress Note - 2026-05-17

- `ArtifactAssetBrowser`: Library Hub の `Current / Recent / Selection / Status` 表示を Phase 1 語彙へ寄せた
- `ArtifactProjectManagerWidget`: header を `Current: Project View`、sync chip を `Status` 表記へ寄せた
- `ArtifactCompositionEditor`: info overlay の layer/current 表示を `Current / Selection` 表記へ寄せた
- `ArtifactTimelineWidget`: 未選択時の current と selection summary を Timeline 語彙へ寄せた
- `AppDebuggerWidget`: overview summary を `Goal / Now / Warning / Next` 表記へ寄せた

---

## Related Investigation

- Floating mode の Composition Editor 白画面については [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](../bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md) を参照する。
- 長期の対処順は [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md) を参照する。

---

## Done Criteria

- 主要 surface の先頭を見た時に、今の位置が短く読める
- recent / selection / status の文言が surface ごとにバラけない
- Asset Browser の hub-style summary が横断文法の基準になる
- empty state が単なる余白ではなく、案内として機能する

---

## Next Step

Phase 2 では、summary strip の高さと情報量をさらに揃える。
