> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md](MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md)

**最終更新:** 2026-08-15

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

---

## Static audit follow-up (2026-07-25)

現行ソースを確認した結果、Phase 1〜3 は基盤実装まで進んでいるが、契約の完全統一と実運用確認は未完了と判断する。

| 項目 | 現状 | 判定 |
|---|---|---|
| goal-first summary | `FrameDebugViewWidget` に `goal / now / warning / next` の first-glance 表示がある | 実装済み（Frame Debug） |
| report identity | Harness が `reportId`、`createdAt`、`preset`、viewport を出力する | 実装済み |
| scenario / preset | Harness に particle/video/blend/overlay/mixed-media の固定シーンと fixture 記述がある | 実装済み（Harness） |
| expected / actual / next action | Harness report は状態・診断・failure reason を出力するが、4項目の共通フィールドとしては未定義 | 部分実装 |
| failure taxonomy | Harness の status/failure summary、FrameDebugSnapshot の failure/skipped/resource 状態は存在するが、全 surface の語彙統一は未確認 | 部分実装 |
| copy / save / share | Harness の Copy/Save Report と App Debugger の report/history/export 基盤がある | 部分実装 |
| App Debugger bridge | Frame Debug の要約・capture/history/compare は存在するが、Harness report と同一テンプレートではない | 部分実装 |
| regression loop | report から観測結果と次の修正候補を読む基盤はあるが、goal ベース smoke checklist と自動比較は未確認 | 未完了 |

### Current boundary

- 追加の global wiring や product UI への責務混入は確認できない。
- `FrameDebugSnapshot`、Harness report、App Debugger の各要約は同じ診断情報を扱うが、共有 report contract として一つに定義されていない。
- 次の実装単位は、`goal / expected / actual / next action` と `ok / skipped / failed / degraded / pending` を共有する report contract、および Harness/App Debugger 間の copy・save・compare の運用確認。

**判定**: Phase 1 基盤実装済み・Phase 2 シナリオ基盤実装済み・Phase 3 部分実装・Phase 4 未完了。

## Update 2026-08-15

- `DebugRenderHarnessWidget` の report に `goal / expected / actual / nextAction` を共通の Summary 項目として追加した。
- report status は `ok / failed / pending / degraded` の分類へ正規化し、既存の failure／snapshot 診断を再利用する。
- App Debugger 側の同一テンプレート化、copy/save/compare の完全統合、runtime smoke checklist は継続課題。

### 2026-08-15 follow-up

- App Debugger の Capture Details にも `goal / expected / actual / nextAction` を追加し、Harness report と同じ text-first 語彙で比較状況を読めるようにした。
- Capture／baseline compare の詳細は既存表示へ追記するだけに留め、global wiring や新しい signal は追加していない。
- status taxonomy の完全統合と runtime smoke checklist は未確認。
