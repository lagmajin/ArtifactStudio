# App Surface Cohesion - Phase 2 Execution

**Date**: 2026-05-13

**Source**: [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)

---

## Phase 2 Goal

各 surface の summary strip を、見出しの延長ではなく「今の状態を 1 回で読める帯」として揃える。

この段階では文言の完全統一よりも、`current -> recent -> selection -> status` の順で目線が流れることを優先する。

---

## Scope

### In

- summary strip の高さ基準
- information density の上限
- `recent` の件数制限
- `selection` と `status` の見せ方
- chip / badge の簡潔化

### Out

- empty state の全面再設計
- deep copyable report 形式の刷新
- new global signal/slot
- backend / core 再設計

---

## Current Focus Surfaces

Phase 2 では、次の surface を順に見る。

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

Phase 1 で固めた語彙を、同じ高さの summary strip に乗せる。

---

## Working Rules

1. `current` を最初に読む
2. `recent` は 3 件前後で止める
3. `selection` は件数と代表だけにする
4. `status` は chip / badge に逃がす
5. summary strip は header を押しつぶさない

---

## Shared Strip Pattern

Phase 2 で揃える summary strip は、まずこの形を標準にする。

```text
current | recent | selection | status
```

読み順は左から右、または上から下のどちらでもよいが、意味の順序は変えない。

---

## First Pass Order

1. `ArtifactAssetBrowser`
2. `ArtifactProjectManagerWidget`
3. `ArtifactCompositionEditor`
4. `ArtifactTimelineWidget`
5. `AppDebuggerWidget`

この順で、各 surface の summary strip の高さと情報量を見比べる。

---

## Surface Roles

- `Asset Browser`: current の場所、最近の素材、選択件数、同期状態を短く出す
- `Project Manager`: current project、recent items、selection summary、missing / unused を出す
- `Composition Editor`: current composition、recent selection、layer count、playback / mask 状態を出す
- `Timeline`: current timeline、recent sequence、clip / layer count、keyframe / range 状態を出す
- `App Debugger`: current debug context、recent report、frame / item count、goal / warning / next の補助を出す

同じ情報を複数 surface に再掲してもよいが、strip の高さは揃える。

---

## Draft Strip Template

Phase 2 で揃える summary strip のたたき台。

```text
current: <one line>
recent: <up to 3 items>
selection: <count + short note>
status: <chip or badge>
```

### Asset Browser Draft

- `current: Library Hub`
- `recent: <3 recent folders or assets>`
- `selection: <count> selected`
- `status: sync / missing`

### Project Manager Draft

- `current: Project View`
- `recent: <3 recent comps or imported items>`
- `selection: <count> items`
- `status: missing / unused`

### Composition Editor Draft

- `current: Composition Editor`
- `recent: <3 recent comps or selections>`
- `selection: <count> layers`
- `status: playback / mask`

### Timeline Draft

- `current: Timeline`
- `recent: <3 recent comps or sequences>`
- `selection: <count> clips or layers`
- `status: keyframe / range`

### App Debugger Draft

- `current: App Debugger`
- `recent: <3 recent reports or captures>`
- `selection: <count> frames or items`
- `status: goal / warning / next`

---

## Tasks

### 1. Strip Height

- summary strip の高さを surface ごとに見比べる
- 1 画面で 2 行以上に膨らみすぎないようにする
- header と strip の間隔を揃える

### 2. Information Density

- recent の件数を 3 件前後に制限する
- selection の詳細を詰め込みすぎない
- status は chip / badge に逃がす

### 3. Ordering

- `current -> recent -> selection -> status` の順を保つ
- surface ごとに語彙が増えても順序を崩さない
- diagnostics 側の `goal / now / warning / next` と混線しない

### 4. Visual Finish

- chip の色やサイズを過剰に増やさない
- summary strip が本文より強くなりすぎない
- 空状態との境界を分かりやすくする

---

## Validation Checklist

- [ ] どの surface でも current が最初に読める
- [ ] recent が 3 件前後に収まっている
- [ ] selection が件数中心で短い
- [ ] status が chip / badge として一目で分かる
- [ ] summary strip の高さが surface ごとにバラけない
- [ ] diagnostics の summary と見た目が混ざらない

---

## Progress Note - 2026-05-17

- `ArtifactAssetBrowser`: Library Hub の summary 行に高さ上限を入れ、current / recent / selection が膨らみすぎないようにした
- `ArtifactProjectManagerWidget`: summary を `Recent -> Selection -> Status -> Search` の順へ寄せ、chrome label の高さを抑えた
- `ArtifactCompositionEditor`: info overlay の selection 行に短い playback status を追加した
- `ArtifactTimelineWidget`: header strip の順序を `Current -> Recent -> Selection -> Status` に寄せ、各 label の高さを抑えた
- `AppDebuggerWidget`: overview summary の高さ上限を設定し、diagnostic strip が本文を押し下げすぎないようにした

---

## Related Docs

- [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [`MILESTONE_APP_SURFACE_COHESION_PHASE1_EXECUTION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_PHASE1_EXECUTION_2026-05-13.md)
- [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)

---

## Next Step

Phase 3 では、empty state と action proximity を揃える。
