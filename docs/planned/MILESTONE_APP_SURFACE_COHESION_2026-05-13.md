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
- playback / transport の入口整理

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
- playback / transport の操作入口を shell 視点で揃える

---

## Playback Control Integration Slice

### Why

- `docs/WIDGET_MAP.md` では `ArtifactCompositionEditor` が `editor shell / playback controls / surface orchestration` の owner であり、transport の context router として扱う前提になっている
- 一方で実装上は `ArtifactPlaybackControlWidget` と `ArtifactTimelineWidget` の両方が `ArtifactPlaybackService` を直接叩く箇所を持ち、操作の入口と UI 反映の責務が surface ごとに散っている
- surface cohesion の観点では、見た目の統一だけでなく `play / pause / seek / step / loop / in-out` がどこから触っても同じ経路を通ることを先に固定したい

### Target

- transport command の authority を `ArtifactPlaybackService` に寄せたまま、surface ごとの UI は shell から同じ action vocabulary を参照する
- `ArtifactCompositionEditor` を playback context router として固定し、Playback Control / Timeline / viewer footer が別々の判断を持たないようにする
- widget ごとの直接操作を減らし、`current / selection / status` と同様に `transport` も横断 surface の共通文法として扱う

### Non-Goals

- playback engine / render backend の再設計
- 新しい global signal/slot の追加
- transport ごとの独自ショートカット実装の増設

### Execution Notes

1. `ArtifactPlaybackControlWidget` は button wiring と state presentation に寄せ、再生可否や対象 composition の判断は shell / service 側へ寄せる
2. `ArtifactTimelineWidget` の space / JKL / scrub-preview 導線は、直接 service を叩く箇所を見直し、共有 command へ収束できるものから先に寄せる
3. `ArtifactCompositionEditor` は active composition と playback context の owner として、transport surface の routing point を明示する
4. `ArtifactPlaybackService` は command authority と state authority を維持し、widget は state mirror として扱う
5. `Playback Control`, `Timeline`, `Composition footer` の status wording は `Current / Status / Next action` の surface 文法に揃える

### Done When

- playback control widget と timeline shortcut が同じ command 名と同じ実行先を通る
- active composition を切り替えた時に transport surface の対象が 1 系統で追える
- playhead / loop / speed / in-out の UI 反映差分を widget 固有ロジックで説明しなくてよくなる
- surface をまたいでも `transport` の語彙と状態表示が読み直し不要になる

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

### Playback / Transport Checklist

- [ ] `ArtifactCompositionEditor` を transport context router として文書上で固定する
- [ ] `ArtifactPlaybackControlWidget` の responsibility を `UI wiring / state mirror` に寄せる
- [ ] `ArtifactTimelineWidget` の playback shortcut と scrub preview の command 経路を棚卸しする
- [ ] `ArtifactPlaybackService` に残す authority と widget 側に残す presentation を分けて書く
- [ ] `Playback Control` / `Timeline` / `Composition footer` の status wording を比較して揃える
- [ ] transport 操作が surface ごとに別仕様に見えないことを確認する

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
- [`MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md`](./MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md)
- [`MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`](./MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md)
- [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](../bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md)
- [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md)
- [`../COMPOSITION_EDITOR_CONTRACT.md`](../COMPOSITION_EDITOR_CONTRACT.md)

---

## Next Step

Phase 1 の対象 surface と、共通にする見出し語彙を先に固定する。
必要なら diagnostics 側は [`MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`](./MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md) に渡し、surface 側では `current / recent / selection / status` を先に揃える。
floating 系の表示不良は [`BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`](../bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md) に整理済み。
長期の対処順は [`MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md`](./MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md) に切り出した。
Phase 4 の仕上げメモは [`MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md`](./MILESTONE_APP_SURFACE_COHESION_PHASE4_EXECUTION_2026-05-17.md) に追加済み。
