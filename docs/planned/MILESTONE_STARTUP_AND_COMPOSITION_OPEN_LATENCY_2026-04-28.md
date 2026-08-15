# マイルストーン: Startup / Composition Open Latency Reduction

> 2026-04-28 作成

**ステータス:** In Progress
**最終更新:** 2026-08-15

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
  - `CompositionCreatedEvent` 後は Timeline を immediate surface とし、Dope Sheet を restorable lazy dock として登録する
  - composition 作成直後の dock activate / focus / splitter 再計算を整理する
  - `ArtifactCreateCompositionDialog` の入力初期化と名前生成を軽量化する
  - 作成完了後に必要な state 同期だけを残す

- 完了条件:
  - dialog から作成した後、window が出るまでの待ちが短くなる
  - 作成直後の dock churn が減る

## Long-Term Architecture: Restorable Dock Shells, On-Demand Content

ADS layout restore requires every dock to be registered before the saved state
is restored. It does not require every dock's content widget to be constructed.
All optional surfaces will therefore use a stable dock shell and an on-demand
content factory.

- Register the stable dock ID, title, tab group, and initial placement at startup.
- Keep the content in `addLazyDockedWidgetTabbedWithId()` or
  `addLazyDockedWidgetFloating()` until the dock becomes the active tab.
- A factory obtains the current composition, selection, and frame from the
  existing service/event-bus state once at construction. It must not introduce
  a new central signal route.
- Never change a dock's ID, tab group, or splitter relationship when replacing
  its placeholder. This keeps saved layouts valid whether content has been
  created or not.
- Keep only the first-paint essentials eager: Composition Viewer, Project,
  Inspector, and Properties. Reclassify these only after measured startup data
  confirms that their visibility contract can remain intact.

The first migration is the composition-created Dope Sheet. Timeline remains
eager because it is the immediate post-create editing surface; the Dope Sheet
is registered for layout restoration but constructed only when selected.

### Migration Order

1. Composition-created auxiliary surfaces: Dope Sheet first.
2. Hidden startup tabs: Contents Viewer, Project Memo, Clip Buffer, Shortcut
   Helper, notes, debug, and test surfaces.
3. Optional composition-bound views: software composition and layer views,
   with factory-time synchronization from the current composition.
4. Always-visible candidates: make changes only after startup measurements and
   saved-layout restore coverage exist.
5. Maintain startup, composition-create, first-activation, and saved-layout
   restore regression measurements for every migration.

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

## Update 2026-08-15

- `ArtifactIRenderer::initialize()` に `ScopedStartupTimer`／`StartupProfiler` があり、Device、RayTracing、PrimitiveRenderer、Shader／PSO 初期化の時間をログ／レポート化する経路がある。`ArtifactCompositionRenderController` と `ArtifactCompositionEditor` にも startup timer、deferred initialization、viewport retry の計測・ログが存在する。
- `ArtifactMainWindow` には stable dock shell と lazy widget の状態、startup layout freeze／apply、保存レイアウト復元後の deferred refresh が実装されている。optional dock の内容を後から生成する設計基盤は、本文の Long-Term Architecture と整合する。
- `AppMain` では geometry restore、ADS dock restore、layout finalize の計測ログ、startup parallelism の warmup 復帰が確認できる。初回表示前後の重い処理を観測する基礎は実装済みである。
- Composition 作成直後についても editor／viewport の deferred sync・retry・debounce 経路は存在するが、Timeline／Dope Sheet の全てが本文どおり lazy 化されていること、作成から first visible frame までの一貫した計測点、dock churn の削減効果はコード検索だけでは受入れ確定できない。
- 数値基準（startup／create-composition／first-paint の baseline と改善値）は現行文書・コードから確認できない。よって現状は `Phase 1 instrumentation and startup shell: substantially implemented / Phase 2〜4 deferred path: partial / Phase 5 numeric regression evidence: pending` とする。体感改善や ms 目標は実測なしに完了扱いしない。
