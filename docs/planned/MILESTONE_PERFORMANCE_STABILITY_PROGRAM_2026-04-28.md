# マイルストーン: Performance Stability Program

> 2026-04-28 作成

## 目的

ArtifactStudio 全体のパフォーマンス問題を、個別のバグ修正ではなく、長期的な改善プログラムとして扱う。

このマイルストーンは、startup の重さ、composition editor の frame drop、dock / widget churn、decode / cache / render の遅延を、同じ観測と改善の枠で進めるための上位目標。

狙いは「速くする」だけではなく、

- どこが遅いかを app 内で読める
- どこで待つべきでないかを切り分けられる
- 再発したときにすぐ原因へ戻れる

状態にすること。

---

## Scope

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactToolBar.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Graphics/*`
- `ArtifactCore/src/Image/*`
- `ArtifactCore/src/Playback/*`
- `ArtifactCore/src/Thread/*`

---

## Non-Goals

- 低レベル backend の全面再設計をこの文書だけで終わらせない
- 速度改善のために可観測性を捨てない
- 一度に全部の重さを消そうとしない

---

## Background

現在の重さは単独原因ではなく、複数の層が重なっている。

例:

- startup 時の worker burst
- dock / workspace mode の show-hide churn
- render pipeline の lazy init
- media decode / upload / cache の初期コスト
- overlay / inspector / timeline の再描画連鎖
- icon / resource lookup の失敗ログ

そのため、単発の最適化ではなく、**測定 -> 切り分け -> 先送り -> 再構成 -> 回帰防止** のループを回す必要がある。

---

## Guiding Principles

- Measure first
  - まず計測できるようにする
- Keep hot paths boring
  - hot path には新しい分岐や重い整形を足さない
- Defer non-essential work
  - 初回表示に不要なものは遅延させる
- Reduce churn
  - 何度も同じ state を再適用しない
- Preserve observability
  - 最適化しても trace / debug surface を失わない

---

## Phase 1: Unified Perf Observation

- 目的:
  - startup / render / decode / UI churn を同じ観測面で追えるようにする

- 作業項目:
  - `Trace Timeline` と `Frame Debug` の役割を整理する
  - startup burst と frame timing を同じ app surface から読めるようにする
  - `render / decode / ui / worker` の lane を固定する
  - perf log の出し方を揃える

- 完了条件:
  - どの重さが startup 由来か frame 由来かを切り分けられる
  - perf log がアプリ内から読める

## Phase 2: Startup Budget Control

- 目的:
  - 起動直後の重い処理を整理する

- 作業項目:
  - dock / widget の eager generation を減らす
  - AI / inspector / note / secondary views の遅延生成を進める
  - shader / pipeline / cache warmup をバックグラウンド化する
  - 初回 composition open と first paint を優先する

- 完了条件:
  - 起動時の burst が減る
  - 初回操作前に不要な UI が生成されない

## Phase 3: Workspace / Dock Churn Reduction

- 目的:
  - show-hide / layout reapply / focus churn を減らす

- 作業項目:
  - workspace mode の再適用を idempotent にする
  - dock visibility の state change を必要時だけにする
  - layout / tabify / focus 再計算の回数を減らす
  - workspace / toolbar / editor の state 正本を一本化する

- 完了条件:
  - workspace 変更時に widget が不必要に反応しない
  - 使わない UI を隠す処理で全体が揺れない

## Phase 4: Render Hot Path Simplification

- 目的:
  - composition frame / overlay / gizmo の hot path を軽くする

- 作業項目:
  - `renderOneFrameImpl()` の再描画条件を整理する
  - overlay / gizmo / HUD の描画を必要分だけにする
  - 重い初期化を render loop から外す
  - cache hit / fallback path の差を読みやすくする

- 完了条件:
  - frame ごとの無駄な再描画が減る
  - overlay を足しても render cost が急増しない

## Phase 5: Media / Cache / Decode Throughput

- 目的:
  - video / audio / image / asset cache の初回遅延を減らす

- 作業項目:
  - video decode の初回失敗や再試行を追いやすくする
  - cache miss / upload / fallback の可視化を整える
  - lazy load と prefetch の優先度を整理する
  - GPU upload と CPU decode の責務を分ける

- 完了条件:
  - media 系の初回表示が安定する
  - 何が遅いかをログだけでなく surface でも読める

## Phase 6: Regression Guardrails

- 目的:
  - いったん速くしたものが戻らないようにする

- 作業項目:
  - perf profile の基準値を残す
  - startup / render / decode の代表ケースを比較できるようにする
  - debug log のノイズを減らし、重要な warning を埋もれさせない
  - 長時間利用で悪化する churn を検出しやすくする

- 完了条件:
  - 変更の前後で perf を比較できる
  - regression を見つける導線がある

---

## Related Milestones

- `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`
- `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md`
- `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_STARTUP_AND_COMPOSITION_OPEN_LATENCY_2026-04-28.md`

---

## Success Criteria

- startup が軽くなるだけでなく、どこが重いかを読める
- workspace / dock churn が減る
- render hot path が安定し、overlay の追加で崩れにくくなる
- media / cache / decode の遅延が追跡可能になる
- perf regression を再発しにくくなる
