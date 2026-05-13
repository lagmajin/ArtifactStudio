# Harness Engineering / Goal-First Working Loop

**Date**: 2026-05-12

`Debug Render Harness` や `App Debugger` のような診断 surface を、実装の往復を短くする作業ハーネスとして扱うための上位マイルストーン。

このマイルストーンは新しい診断 UI を増やすことが目的ではない。
目的は、Artifact での実装ループを `goal -> preset/scenario -> observation -> compare -> next action` に揃えること。

---

## Goal

- 何を証明したいかを先に固定できるようにする
- 観測結果を text-first で共有しやすくする
- 失敗を `ok / skipped / failed / degraded / pending` で機械的に分類できるようにする
- 同じ入力で何度でも比較できる形にする

---

## Scope

### In

- harness report の goal-first 形式
- scenario / preset の固定
- expected / actual / next action の明文化
- failure taxonomy の統一
- copy / save / share の導線整理

### Out

- 新しい backend の追加
- heavy automation / diff engine の全面導入
- product UI への責務混入
- global wiring の増設

---

## Design Rules

1. 固定入力を持つ
2. 固定観測を持つ
3. 失敗名を曖昧にしない
4. 比較のための report identity を持つ
5. 実装の次の判断を短く返す
6. product UI と harness UI の責務を混ぜない

---

## Phases

### Phase 1: Contract Alignment

- `goal / expected / actual / next action` を report contract に追加する
- `Debug Render Harness` の report template を整理する
- `FrameDebugSnapshot` との接続語彙を揃える

### Phase 2: Scenario Catalog

- 代表的な scenario を固定する
- preset 切替時の reset ルールを明示する
- capture bundle に残す項目を絞る

### Phase 3: App Debugger Bridge

- `AppDebuggerWidget` の first-glance summary と harness report を同じ読み方に寄せる
- copy / save / pin / compare の quick action を統一する

### Phase 4: Regression Loop

- smoke checklist を goal ベースにする
- failure reason の再現と共有を簡単にする
- 次の修正候補が report から読めるようにする

---

## Related Docs

- [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)
- [`MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](./MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)
- [`../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_REPORT_TEMPLATE_2026-04-30.md)
- [`../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md)

---

## Next Step

Phase 1 の実行メモを作って、goal-first report の項目を `Debug Render Harness` と `App Debugger` で同じ語彙に揃える。
