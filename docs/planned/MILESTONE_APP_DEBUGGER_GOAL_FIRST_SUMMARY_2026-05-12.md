# App Debugger / Goal-First Summary

**Date**: 2026-05-12

`AppDebuggerWidget` を、単に diagnostics を並べる面ではなく、`goal -> now -> warning -> next` の順で見通せる作業面へ寄せるためのマイルストーン。

このマイルストーンは見た目の派手さよりも、調査中に「次に何を見ればいいか」を短く返すことを優先する。

---

## Goal

- 画面を開いた瞬間に、今何を見ればいいか分かるようにする
- `goal / expected / actual / next action` の語彙を harness 側と揃える
- 重要な warning を詳細より先に読めるようにする
- copy / pin / compare / filter の近接配置を保つ

---

## Scope

### In

- top strip の goal-first summary
- warning-first ordering
- now / next の固定
- report / summary / quick actions の整合
- text dump の補助化

### Out

- new backend
- product editing UI の redesign
- global event wiring の追加
- raw dump 主体の設計

---

## Design Rules

1. `goal` は 1 文で言えること
2. `now` は現在値だけを短く見せること
3. `warning` はあれば先頭に出すこと
4. `next` は 1 個に絞ること
5. `raw text` は補助に回すこと

---

## Phases

### Phase 1-4: Summary / Action / Danger / Report

- `goal / now / warning / next` を上部サマリの軸にする
- copy / pin / compare / filter を summary に近づける
- failed pass / missing resource / fallback / stale cache / playback stall を前景化する
- `Debug Render Harness` と `Frame Debug View` の report 語彙を揃える

---

## Related Docs

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](./MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](./MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](./MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
- [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)

---

## Next Step

Phase 1 の実行メモは親文書へ統合済み。

---

## Static audit follow-up (2026-07-25)

現行の `AppDebuggerWidget`、`FrameDebugViewWidget`、`DebugRenderHarnessWidget` を確認した。ビルド・実機表示は未実施。

| 要件 | 現状 | 判定 |
|---|---|---|
| goal/now/warning/next の上部 summary | Frame Debug View に `goal / now / warning / next` の summary 文言が実装されている。 | ソース上確認済み |
| warning-first ordering | failed pass、warning resource、density warning を summary/detail に出す処理がある。全 debugger surface の順序統一は未確認。 | 部分実装 |
| copy/pin/compare/filter | Frame debug の compare、console の copy/filter、report の導線が存在する。pin と全 surface の近接配置は未確認。 | 部分実装 |
| raw text の補助化 | summary label と detail text の二層構造がある。 | ソース上確認済み |
| harness/report 語彙 | harness と frame debug が snapshot/summary/report を共有するが、warning/next の完全な辞書統一は未確認。 | 部分実装 |

### 現在の判定

Phase 1〜4 の UI 基盤は実装済み部分が多いが、App Debugger 全体の summary ordering、pin/compare/filter の横断導線、harness との語彙統一は確認待ち。マイルストーンは「部分実装／統合確認待ち」とする。

## 2026-07-29 実装マーク

- App Debugger の状態 summary に `warning / next`、density warning、failure / fallback、capture compare の情報が存在することをコード上確認した。
- Frame Debug View との snapshot 共有、capture history / pinned 表示、copyable report も既存導線として確認済み。
- ただし `goal / now / warning / next` の画面横断 ordering と harness の完全な語彙統一は未検証のため、マイルストーン全体は `Partial implementation / integration verification pending` を維持する。
