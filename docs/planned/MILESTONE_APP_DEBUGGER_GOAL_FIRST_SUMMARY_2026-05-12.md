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

### Phase 1: Summary Vocabulary

- top strip に `goal / now / warning / next` を置く
- `status` と `shortReason` をその語彙に接続する
- `Frame / State / Trace` の役割を短い見出しで固定する

### Phase 2: Action Proximity

- copy / pin / compare / filter を summary に近づける
- quick actions を summary-first に並べる
- 長い report は折りたたみや補助タブへ寄せる

### Phase 3: Danger-First Ordering

- failed pass
- missing resource
- fallback path
- stale cache
- playback stall

を通常の詳細より前に出す

### Phase 4: Report Bridge

- `Debug Render Harness` の report と同じ読み方に揃える
- `Frame Debug View` でも同じ語彙を再利用する
- 保存された bundle からも同じ summary が読めるようにする

---

## Related Docs

- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](./MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md`](./MILESTONE_APP_DEBUGGER_FIRST_GLANCE_LAYOUT_2026-04-24.md)
- [`MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md`](./MILESTONE_APP_DEBUGGER_QUICK_ACTIONS_2026-04-24.md)
- [`MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`](./MILESTONE_HARNESS_ENGINEERING_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)

---

## Next Step

Phase 1 実行メモを作って、`goal / now / warning / next` を App Debugger の上部サマリ文法として固定する。
