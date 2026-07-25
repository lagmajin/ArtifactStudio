# MILESTONE: RAM Preview Range Policy and Priority - Phase 1 Execution

作成日: 2026-05-31
対象: [MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md)

## Goal

`RAM preview` の warmup order を、まずは `priority vocabulary` と `reason API` として固定する。

この Phase 1 では scheduler の全面再設計までは入らず、`なぜこの frame を先に作るのか` を service と diagnostics の両方で説明できる状態まで進める。

## Why Phase 1 First

- state wording が揃っても、request order が曖昧なままだと体感品質が安定しない
- いきなり prewarm 実装へ入ると、surface ごとに別の優先規則を埋め込みやすい
- 先に `priority reason` を固定すると、timeline / debugger / future scheduler が同じ truth を読める

## Target Files

- [ArtifactPlaybackService.cppm](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm)
- [ArtifactPlaybackService.ixx](X:/Dev/ArtifactStudio/Artifact/include/Service/ArtifactPlaybackService.ixx)
- [ArtifactTimelineWidget.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)
- [AppDebuggerWidget.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

## Task Slice

### 1. Priority Vocabulary Lock

固定したい語彙:

- `immediate`
- `near`
- `directional`
- `safety-backfill`
- `work-area`
- `out-of-range`
- `unknown`

やること:

- playback service 側に priority note / reason helper を置ける形にする
- `requested / ready / failed` の state vocabulary と混ざらないように分離する
- UI が自由文を組み立てず、service 側の語彙を読む前提へ寄せる

### 2. Reason API Draft

やること:

- `frame -> priority reason` を返す read-only API を service 側に置く
- まずは `current frame / near band / work-area / out-of-range` を説明できればよい
- 実際の scheduler が未完成でも、暫定 policy を reason として返せる形にする

候補:

- `ramPreviewPriorityReason(frame, context)`
- `ramPreviewPriorityNote(frame, context)`
- `ramPreviewPriorityBand(frame, context)`

Phase 1 では名前の最終形より、`1 か所から読める` ことを優先する。

### 3. Diagnostics Surface Hook

やること:

- timeline tooltip から priority reason を読めるようにする
- debugger に `current frame priority` か `sample frame priority` を出せる導線を作る
- render controller 側でも fallback explanation と priority explanation を混同しないようにする

### 4. Context Inputs Audit

最低限使う context:

- current composition
- current frame
- playback running state
- playback direction
- work area range
- composition frame bounds

やること:

- これらが service 側で既に取れるか確認する
- UI 側から新しい状態を注入せずに済む形を優先する

## Expected Outcome

- `なぜ今この frame が優先なのか` を短い語彙で説明できる
- state reason と priority reason が別物として整理される
- future prewarm scheduler の入口が service 側にできる

## Not Yet

- 実際の prewarm queue の全面再配線
- disk cache を含む priority 統合
- loop wraparound の完全実装
- reverse playback の最適化仕上げ

## Done For Phase 1

- priority vocabulary を 1 枚の文書と 1 つの helper 群で説明できる
- timeline / debugger のどちらか少なくとも 1 面で priority reason を読める
- `state not ready` と `priority low` が別理由として区別される

## Validation Checklist

- [ ] `immediate / near / directional / safety-backfill / out-of-range` の語彙が code 側に定着している
- [ ] priority reason が service から取れる
- [ ] timeline か debugger で priority reason を表示できる
- [ ] `failed` と `low priority` が同じ表示にならない
- [ ] `work-area` と `out-of-range` を分けて扱える

## Suggested First Implementation Order

1. `ArtifactPlaybackService` に priority helper の型と note を追加
2. current frame 基準の簡易判定を実装
3. work area / comp bounds の分岐を追加
4. timeline tooltip へ表示
5. debugger に sample 表示を足す

## Follow-Up

Phase 2 では、ここで固定した vocabulary を使って `playback direction bias` を実際の request ordering に反映する。

---

## Static audit follow-up (2026-07-25)

現行コードを確認した。子サブモジュールの対象ファイルは編集せず、親から参照できる現状だけを記録する。ビルド・実機 UI 確認は未実施。

### 確認できた実装

- `ArtifactPlaybackService` 側に priority note/reason helper があり、`immediate`、`near`、`directional`、`safety-backfill`、`work-area`、`out-of-range`、`unknown` の語彙を分離して扱っている。
- priority reason と `requested/ready/failed` の state reason は別 helper 群になっている。
- current frame、再生状態、方向、work area、composition bounds を使うための service 状態と、priority build queue の ordering が存在する。

### 未確認・残課題

| 完了条件 | 判定 |
|---|---|
| code 側で priority vocabulary が定着 | ソース上は確認済み |
| service から frame priority reason を取得 | ソース上は確認済み |
| timeline/debugger の表示 | service state の通知基盤はあるが、両面で同じ reason を表示する実行確認は未実施 |
| failed と low priority の区別 | helper 上は分離。UI 表示の実機確認待ち |
| work-area と out-of-range の区別 | policy/helper 上は分離。境界値の実行確認待ち |

### Phase 1 判定

priority vocabulary と service 側 reason API は実装済み。Done 判定に必要な timeline/debugger 表示と境界値の実行確認が残るため、Phase 1 は「部分実装／検証待ち」とする。

### 追加静的確認

上記判定後のソース再確認で、`ArtifactTimelineTrackPainterView.cppm` は `ramPreviewPriorityReason(currentFrame)` を tooltip hint に追加し、`AppDebuggerWidget.cppm` は priority band／reason と priority state を診断文字列へ出力していることを確認した。したがって「timeline/debugger 表示」は静的には実装済みで、残るのは boundary 値、failed と low priority の表示分離、reverse／loop／scrub の実機動作確認である。Phase 1 は「実装済み／実行検証待ち」に更新する。
