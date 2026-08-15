# Milestone: App Diagnostic Cohesion

> 2026-05-13 作成
> 2026-08-15 現行コード監査

ArtifactStudio 全体の diagnostics を、画面ごとに別の言い方や別の深さで散らさず、同じ文法で読めるようにするマイルストーン。

この文書は、Project Health / Problem View / App Debugger / Frame Debug View / Harness report / status chip / overlay warning をひとつの診断体験として揃えるための上位枠とする。

---

## Goal

- 問題が起きた時に、何が悪いかより先に「何を見ればいいか」が分かるようにする
- health / warning / error / fallback / stale / missing の語彙を揃える
- App Debugger と harness report と frame debug の読み方を合わせる
- 診断の主役を raw dump ではなく短い summary にする

---

## Scope

### In

- goal / now / warning / next の診断文法
- project health と problem view の語彙統一
- App Debugger の summary と trace の役割分担
- Frame Debug View の固定フレーム診断
- overlay warning と inline warning の優先順位
- harness report の読み方の統一

### Out

- 新しい global debug bus
- 既存の診断 surface を増殖させること
- 単なるログ出力の追加
- render backend の低レベル改造

---

## Design Rules

1. `goal` は 1 文で言えること
2. `warning` は詳細より先に置くこと
3. `next` は 1 個に絞ること
4. `raw trace` は補助であり本体ではないこと
5. `error` は観測可能で再現可能な語彙で書くこと

---

## Phases

### Phase 1: Vocabulary Alignment

- Project Health / Problem View / App Debugger の見出し語彙を揃える
- `warning` と `error` の使い分けを固定する
- status chip の色と意味を診断側で統一する

### Phase 2: Summary-First Diagnostics

- 先頭に goal-first summary を出す
- detailed trace を後ろに回す
- problem view から debugger へ同じ読み方で飛べるようにする

### Phase 3: Frame and Harness Bridge

- Frame Debug View の要約を harness report と合わせる
- frame 単位の失敗理由を短く返す
- compare / pin / copy の位置を揃える

### Phase 4: Cross-Surface Warning Consistency

- asset / timeline / composition / render / playback の warning 表示をそろえる
- inline warning と dedicated diagnostics panel の責務を固定する
- 観測と操作の距離を短くする

---

## Success Criteria

- 問題を見た時の言い方が surface ごとに変わらない
- summary から詳細へ同じ文脈で降りられる
- App Debugger と Project Health と Frame Debug View が同じ語彙で読める
- warning が埋もれず、raw dump が主役にならない
- 失敗時に次の調査先がすぐ分かる

---

## Related Docs

- [`MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`](./MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`](../MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md)
- [`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`](./MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md)
- [`../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md`](../technical/HARNESS_ENGINEERING_PRINCIPLES_2026-05-12.md)

---

## Next Step

Phase 1 の実行メモは親文書へ統合済み。

## 2026-05-31 Survey Note

- `RAM preview` は diagnostics cohesion の代表例になっている
- 最近の parity 作業で `ready-missing-image` を state として分離し始めたため、今後は `ready / playable / pending / failed / fallback` を diagnostics 語彙として固定しやすい
- App Debugger / timeline tooltip / footer / render fallback reason の wording を同じ辞書で見る slice が必要
- 関連する横断整理は [MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PLAYBACK_STATE_CONTRACT_AND_TRANSPORT_COHESION_2026-05-31.md) を参照

---

## Static audit follow-up (2026-07-25)

現行の diagnostics widget、project health、frame debug、playback reason helper を照合した。ビルド・実機表示は未確認。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. Vocabulary Alignment | ProjectDiagnostic/DiagnosticEngine、Project Health、App Debugger、frame diagnostics が存在し、playback の `requested/ready/failed/fallback` も定義されている。全 surface の warning/error/chip 表示統一は未確認。 | 部分実装 |
| 2. Summary-First | goal-first summary、health/dashboard、debugger summary の基盤がある。raw trace より summary が常に先に出ることは未確認。 | 部分実装 |
| 3. Frame / Harness Bridge | FrameState/Resource/Pipeline/Debug view と harness/debug widgets が存在する。frame failure reason の共通表現と compare/pin/copy の統一は未確認。 | 部分実装 |
| 4. Cross-Surface Warning | asset/timeline/composition/render/playback に個別 warning/fallback 表示がある。inline と dedicated panel の責務統一は未確認。 | 未完了 |

### 現在の判定

診断の基盤と playback 状態語彙は進展しているが、summary-first と surface 横断の表示整合が残る。全体は「部分実装／統合確認待ち」とする。

## 2026-08-15 現行コード監査

- `ProjectDiagnostic`／`DiagnosticEngine`、Core の `DiagnosticSnapshot`／`TraceSnapshot`、Frame Debug の pipeline／resource／state 表現が存在し、severity と failure reason を構造化して扱える。
- App Debugger、Frame Debug View、Fallback Diagnostics、Project／Render／Playback 系の各 widget に summary、filter、snapshot、diagnostic detail の導線を確認した。
- `stateReason` を含む frame pipeline 診断や playback の ready／pending／failed／fallback 語彙は整備されているが、全 surface が同じ summary-first 表示順・色・next action を共有することはコード上で確認できない。
- raw trace／ログと UI summary の責務境界、compare／pin／copy の共通導線、runtime 表示順は未検証。

判定: **診断モデル、構造化 snapshot、主要 Debugger／Frame Debug UI は実装済み。横断語彙・summary-first・warning 表示順・runtime 検証は pending。**

## Update 2026-08-15

主要 surface の現行コードを再照合した。`ProjectDiagnostic`／`DiagnosticEngine`、`DiagnosticSnapshot`／`TraceSnapshot`、App Debugger／Frame Debug／Fallback Diagnostics の構造化表示は存在する。Playback の `ready`／`pending`／`failed`／`fallback` も状態語彙として利用されている。

- ただし surface 横断で `goal / now / warning / next`、色、優先順位、copy／compare／pin の導線が一つの共有 contract になっている証拠はない。
- raw trace と summary の責務境界、Project Health／Problem View／Frame Debug の表示順、runtime での warning 可読性も未検証。
- 判定は **診断基盤は実装済み、cross-surface cohesion と runtime 検証は未完了** を維持する。
