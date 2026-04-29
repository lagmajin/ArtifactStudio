# マイルストーン: Startup / Composition Open Latency Reduction

> 2026-04-28 作成

## 目的

ウィンドウ表示時の初期化遅延と、ダイアログから composition を作成した直後の表示遅延を、個別の改善ではなく一つの実行計画として扱う。

このマイルストーンは、次の 2 つを主対象にする。

- アプリ起動時の widget / dock / renderer 生成が重い問題
- composition 作成後に window / dock / timeline が出るまで待たされる問題

狙いは、見た目の完成度を下げずに、初回表示を速くすること。

---

## Scope

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Dialog/ArtifactCreateCompositionDialog.cppm`
- `Artifact/src/Controller/TimelineViewProvider.cpp`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

---

## Non-Goals

- render 品質の劣化を許容してまで起動を速くすること
- backend や shader path の全面再設計をこの段階で終えること
- 一度に全部の dock を消すこと

---

## Background

現状の遅さは単独の原因ではなく、複数の重い処理が初回表示に集まっていることで起きやすい。

主な候補は次の通り。

- `ArtifactCompositionEditor` の同期初期化
- `CompositionRenderController` の初回 setup / swapchain 再構築
- composition 作成後の timeline dock 即時生成
- dialog 側の composition 設定 UI の初期構築
- project / composition 名検索や state 同期の初回コスト

---

## Guiding Principles

- Make the first paint cheap
  - 初回表示に必要なものだけ先に作る
- Defer what can wait
  - タイムライン、補助パネル、debug panel は後回しにできる
- Measure each boundary
  - どこで待ったかをログで読めるようにする
- Keep behavior stable
  - 遅延化しても、見え方や操作手順は壊さない

---

## Phase 1: Startup Cost Mapping

- 目的:
  - 何がウィンドウ表示を遅くしているかを切り分ける

- 作業項目:
  - `ArtifactCompositionEditor` の constructor / `showEvent` に計測点を置く
  - `ArtifactCreateCompositionDialog` の constructor / accept 経路に計測点を置く
  - `CompositionCreatedEvent` 後の dock 追加経路に計測点を置く
  - `ArtifactTimelineWidget` 初期化の重さを可視化する

- 完了条件:
  - 「どの初期化が何 ms か」がログで追える
  - 起動時と composition 作成時の重い箇所を分離して見られる

## Phase 2: Window Bootstrap Budget

- 目的:
  - main window 表示時の同期生成を減らす

- 作業項目:
  - 初回表示に不要な dock を lazy 生成へ移す
  - 補助 panel / inspector / debug 系の起動を遅延化する
  - `ArtifactCompositionEditor` の初期化を「表示に必要な最小構成」と「後続構成」に分ける
  - `showEvent` からの重い setup を必要最小限にする

- 完了条件:
  - window を開いた瞬間の固まり方が減る
  - 初回表示前に不要な widget が大量生成されない

## Phase 3: Composition Create to First Visible Frame

- 目的:
  - composition 作成直後に editor / timeline が見えるまでの待ちを減らす

- 作業項目:
  - `CompositionCreatedEvent` 後の timeline 生成を lazy 化する
  - composition 作成直後の dock activate / focus / splitter 再計算を整理する
  - `ArtifactCreateCompositionDialog` の入力初期化と名前生成を軽量化する
  - 作成完了後に必要な state 同期だけを残す

- 完了条件:
  - dialog から作成した後、window が出るまでの待ちが短くなる
  - 作成直後の dock churn が減る

## Phase 4: Editor Initialization Split

- 目的:
  - composition editor を 1 回で全部組み立てない

- 作業項目:
  - render controller 初期化を段階化する
  - toolbar / overlay / profiler / auxiliary widgets を遅延生成に寄せる
  - 初回 paint に必要なオブジェクトだけ先に作る
  - composition 変更時の再設定を idempotent にする

- 完了条件:
  - editor の初期構築時間が短くなる
  - 同じ composition を開き直したときの再初期化が軽い

## Phase 5: Regression Guardrails

- 目的:
  - 一度速くしたものが戻らないようにする

- 作業項目:
  - startup / create-composition / first-paint の基準時間を残す
  - 主要な初期化ポイントの perf log を維持する
  - lazy 化した UI が必要時に確実に出ることを確認できる導線を用意する

- 完了条件:
  - 変更前後を比較できる
  - 再発したときに戻る場所がある

---

## Related Milestones

- `docs/planned/MILESTONE_PERFORMANCE_STABILITY_PROGRAM_2026-04-28.md`
- `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md`
- `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`

---

## Success Criteria

- ウィンドウ表示時の体感遅延が減る
- composition 作成後の初回表示が速くなる
- 重い初期化箇所がログで見える
- lazy 化しても必要な UI が欠けない
- 再発防止の基準が残る
