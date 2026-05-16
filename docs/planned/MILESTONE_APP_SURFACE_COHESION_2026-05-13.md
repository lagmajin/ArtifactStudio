# Milestone: App Surface Cohesion

> 2026-05-13 作成

ArtifactStudio 全体の surface を、画面ごとの寄せ集めではなく「同じアプリの続き」として読めるように揃えるマイルストーン。

この文書は、Project / Asset / Timeline / Composition / Contents Viewer / Inspector / Debugger にまたがる、現在地・選択・最近使った項目・状態 chip・次の行動の見せ方を統一するための上位枠とする。

---

## Goal

- どの画面でも「今どこにいて、何が選ばれていて、次に何をすればいいか」がすぐ分かるようにする
- `current / recent / favorites / sources / selection / status` の語彙を画面横断で揃える
- 主要な header / summary strip / empty state / chip を共通の考え方で並べる
- 画面ごとの見た目の差ではなく、アプリ全体の迷いにくさを上げる

---

## Scope

### In

- current context の見出し
- summary strip
- recent / favorite / source の短い提示
- selection summary
- empty state の文法
- status chip / badge の役割分担
- view 間の基本導線

### Out

- 単一機能の深掘りだけを目的にすること
- 新しい中央集権 signal/slot の導入
- QtCSS 前提の全面リスタイル
- render backend の刷新

---

## Design Rules

1. `current` は 1 行で読めること
2. `recent` は 3 件前後で十分にすること
3. `selection` は件数と要点を短くまとめること
4. `status` は chip で先に見せること
5. `empty state` は余白ではなく案内として扱うこと

---

## Phases

### Phase 1: Context Header Unification

- current / recent / selection の語彙を揃える
- Asset Browser, Timeline, Composition Editor, Contents Viewer の header を比べやすくする
- 現在地の表示名と tooltip の基準を決める

### Phase 2: Summary Strip Harmonization

- summary strip の高さと情報量を揃える
- recent / favorites / sources / sync / status を短く出す
- 画面ごとの情報過多と情報不足を減らす

### Phase 3: Empty State and Action Proximity

- empty state に次の一手を持たせる
- import / open / select / inspect / navigate の導線を近づける
- 何も選ばれていない時でも迷いにくくする

### Phase 4: Cross-Surface Finish

- Project / Asset / Timeline / Composition / Debugger の文法を最後に合わせる
- 画面遷移しても同じアプリとして読めるようにする
- 余白、見出し、chip、補助文の扱いを揃える

---

## Execution Checklist

### Phase 1 Checklist

- [ ] `current / recent / selection / status` の語彙を surface ごとに洗い出す
- [ ] `ArtifactAssetBrowser` の header を基準文法として固定する
- [ ] `ArtifactProjectManagerWidget` の current / selection 表示を短く揃える
- [ ] `ArtifactCompositionEditor` の現在地表示と tooltip を整える
- [ ] `ArtifactTimelineWidget` の見出し語彙を composition 側と揃える
- [ ] `AppDebuggerWidget` の summary 文言を診断側の基準に合わせる
- [ ] 1 画面目を見た時に「今どこにいるか」が 1 行で読めるかを確認する

### Phase 2 Checklist

- [ ] summary strip の高さ基準を決める
- [ ] `recent` を 3 件前後に収める
- [ ] `favorites / sources / sync / status` の出し方を短くする
- [ ] 画面ごとの情報量の差をメモして、過不足を減らす
- [ ] summary strip が header を押しつぶしていないか確認する
- [ ] surface ごとの情報順序を `current -> recent -> selection -> status` に寄せる

### Phase 3 Checklist

- [ ] empty state の文法を surface ごとに揃える
- [ ] `import / open / select / inspect / navigate` の導線を各画面で近づける
- [ ] 選択なし状態の文言を統一する
- [ ] 余白ではなく案内として読める empty state にする
- [ ] 何も選ばれていない時でも次の行動が 1 つ見えるか確認する
- [ ] empty state の文言が diagnostics 側の warning と混ざらないか確認する

---

## Success Criteria

- 画面を変えても「今の位置」を読み直すコストが減る
- recent / favorites / sources / selection が似た文法で読める
- empty state が画面ごとにバラけない
- 主要な surface で「次に何をすればいいか」が短く伝わる
- 画面の個性よりもアプリ全体の流れが先に見える

---

## Related Docs

- [`MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`](./MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md)
- [`MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md`](./MILESTONE_ASSET_BROWSER_LEFT_PANE_HUB_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)
- [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](../bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md)
- [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md)
- [`../COMPOSITION_EDITOR_CONTRACT.md`](../COMPOSITION_EDITOR_CONTRACT.md)

---

## Next Step

Phase 1 の対象 surface と、共通にする見出し語彙を先に固定する。
必要なら diagnostics 側は [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md) に渡し、surface 側では `current / recent / selection / status` を先に揃える。
floating 系の表示不良は [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](../bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md) に整理済み。
長期の対処順は [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md) に切り出した。
