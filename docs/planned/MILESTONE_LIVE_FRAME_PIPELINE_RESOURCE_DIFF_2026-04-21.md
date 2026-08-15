# Live Frame Pipeline / Resource Watcher / State Diff Tracker Milestone

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

現行コードには、このマイルストーンの基盤となる `RenderGraph` / `CompiledRenderGraph`、リソースの `Transient` / `Persistent` / `External` lifetime、pass の read/write 定義、allocation slot と lifetime range が実装されている。`FrameDebugSnapshot` も pass・resource・attachment・preview・render graph diagnostic・GPU timing を保持し、`ArtifactCompositionRenderController` から記録されている。

診断 UI 側では `FramePipelineViewWidget`、`FrameResourceInspectorWidget`、`FrameStateDiffWidget` が `AppDebuggerWidget` に接続され、フレームの pipeline、resource、前回 snapshot との差分、選択 resource の preview を確認できる。したがって、スナップショット型の pipeline/resource/state-diff 機能は実装済み、または実装基盤ありと判定する。

一方、提案していた「常時稼働」の完全形、すなわち全フレームの自動収集、UAV/RTV や read-after-write の汎用 hazard 検出、barrier 不整合の診断、任意 resource の MIP/slice/ピクセル検査、実行中の低オーバーヘッド監視としての安定運用までは現行コードから確認できない。現状は `FrameDebugSnapshot` と render graph diagnostic を入口にした capture/比較 UI が中心であり、RenderDoc 相当の live watcher としては未完了とする。

**判定:** Phase 1（render graph/lifetime のデータモデル、frame snapshot、pipeline/resource/state-diff の診断 UI）は実装済み。Phase 2（常時監視、汎用 hazard/barrier 検出、詳細な texture/buffer inspection、性能・保持期間の検証）は未完了。

ArtifactStudio 本体に組み込む、常時稼働の描画構造可視化・リソース監視・状態差分追跡をまとめたマイルストーン。

この機能は RenderDoc のような「結果のスナップショット」ではなく、実行中に構造と壊れ始めた瞬間を追うための内蔵ツールとする。

## Goal

- フレームごとの Pass DAG を見られるようにする
- RT / Texture / Buffer のライフタイムを常時追えるようにする
- UAV / RTV 衝突や barrier の不整合を見つけやすくする
- 任意のテクスチャや render target をライブで確認できるようにする
- 前フレームとの差分から、状態が壊れ始めたフレームを自動検出できるようにする
- `renderScheduled_` 系の「いつから壊れたか分からない」バグの切り分けを助ける

## Non-Goals

- 外部 RenderDoc の完全代替
- GPU ドライバ内部の完全な命令列可視化
- ブレークポイントやメモリ編集を含む一般的な debugger
- QtCSS を使った専用 theme の追加
- 新しい公開 signal/slot の大量追加

## Design Principles

- Read-first
  - まずは観測と比較に集中し、修復操作は後回しにする
- Always-on friendly
  - 必要な情報を軽量に持ち、常時見える面を優先する
- Structure over screenshots
  - 結果画像よりも、ノード / edge / lifetime / hazard を優先して読む
- Diff-first
  - 「壊れた」ではなく「いつから壊れた」を追えるようにする
- No new global wiring
  - 中央集権の signal/slot バスは増やさない

## Proposed Shape

### 1. Frame Graph / Pipeline View

フレームごとの render pipeline を、ノードと依存関係で可視化する。

表示候補:

- Pass DAG
  - どの node が何を read / write するか
  - pass の順序だけでなく依存 edge を見せる
- Lifetime band
  - RT / Texture / Buffer の生成から解放まで
  - re-use / alias / transient の見分け
- Hazard flags
  - UAV / RTV の衝突
  - barrier が必要そうな箇所
  - read-after-write / write-after-read の候補
- Backend summary
  - CPU / GPU / fallback / partial eval の差
  - ROI や partial eval の効き方

### 2. Always-on Resource Inspector

RenderDoc のスナップショットではなく、実行中に常時開いておける resource inspector を持つ。

表示候補:

- 任意 texture / render target のライブプレビュー
- MIP / array / slice 切り替え
- ピクセル検査
  - RGBA
  - linear / sRGB
  - readback summary
- 任意 buffer の要約
- mask / ROI / partial eval の関連表示

### 3. State Diff Tracker

「どのフレームから壊れたか」を自動で追う差分レイヤー。

表示候補:

- 前フレームとの差分
- PSO / CB / SRV / UAV の変化ログ
- hazard の増減
- fallback に落ちたフレームの検出
- 直前の正常フレームとの比較

### 4. Timeline / Diagnostics Integration

既存の `ProfilerPanelWidget` / `FrameDebugViewWidget` / `ArtifactDebugConsoleWidget` から辿れるように統合する。

表示候補:

- frame graph の要約
- selected resource のライブ情報
- diff tracker の判定結果
- warning / error の簡易集約

## Implementation Targets

### Core Side

`ArtifactCore` 側に、フレーム構造・リソース・差分用の軽量モデルを持たせる。

候補:

- `FramePipelineGraph`
- `FramePipelineNode`
- `FrameResourceLifetime`
- `FrameResourceView`
- `FrameHazardRecord`
- `FrameStateDiff`
- `FrameStateDiffChange`

### App Side

`Artifact` 側に、表示と既存 surface への接続を持たせる。

候補:

- `FramePipelineViewWidget`
- `FrameResourceInspectorWidget`
- `FrameStateDiffWidget`
- `FramePipelineController`

### Existing Integration Points

- `ArtifactCompositionRenderController`
  - pass graph / backend summary / ROI summary の元データ
- `ArtifactRenderQueueService`
  - queue metadata / failed frame / job state
- `ArtifactIRenderer`
  - attachment / readback / resource 要約
- `ArtifactFrameCache`
  - lifetime / cache hit / reuse 情報
- `ProfilerPanelWidget`
  - 常時見える perf / pipeline summary
- `ArtifactDebugConsoleWidget`
  - text fallback の診断窓口

## Phase Plan

### Phase 1: Frame Graph / Pipeline View

- pass DAG のデータモデルを定義する
- pass の read / write / dependency を収集する
- RT / texture / buffer の lifetime を記録する
- UAV / RTV / barrier hazard の簡易フラグを追加する
- ROI / partial eval / composition effect の関係を読めるようにする

### Phase 2: Always-on Resource Inspector

- 任意 resource をライブで選んで見られるようにする
- MIP / array / slice / channel view を切り替えられるようにする
- pixel inspect の read-only 表示を追加する
- mask / ROI / transient resource の関連を見える化する

### Phase 3: State Diff Tracker

- 前フレームとの差分を自動取得する
- PSO / CB / SRV / UAV の変更履歴を取る
- 壊れ始めたフレームを判定する
- renderScheduled_ のような再描画不整合を追いやすくする

### Phase 4: Diagnostics Integration

- `ProfilerPanelWidget` に pipeline / diff summary を載せる
- `FrameDebugViewWidget` に resource / hazard / diff の表示を足す
- `ArtifactDebugConsoleWidget` に失敗判定の要約を出す
- 既存の app debugger surface から開ける導線を揃える

## Success Criteria

- 任意のフレームで、何が read / write されたかを追える
- 常時表示の resource inspector で、RenderDoc を起動しなくても問題の当たりが付く
- 壊れ始めた瞬間が diff で分かる
- ROI / partial eval / composition effect の問題が、構造として追いやすくなる
- 既存の diagnostics surface を壊さずに統合できる

## Related

- `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE1_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE2_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE3_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE4_2026-04-21.md`
- Phase 1-4 execution memo は本書に統合済み

---

## Static audit follow-up (2026-07-25)

現行の FramePipelineView、FrameResourceInspector、FrameStateDiff、FrameDebug snapshot と renderer diagnostics を照合した。ビルド・実機の常時監視は未実施。

| Phase | 現状 | 判定 |
|---|---|---|
| 1. Frame Graph / Pipeline | `FramePipelineViewWidget` が frame/pass/resource/attachment、graph・hazard・backend summary を表示する。完全な read/write DAG、lifetime、UAV/RTV barrier の収集網羅は未確認。 | 部分実装 |
| 2. Always-on Resource Inspector | `FrameResourceInspectorWidget` が resource/attachment/preview/readback 要約を表示する。任意 live resource の MIP/array/slice/channel と pixel inspect の実運用は未確認。 | 部分実装 |
| 3. State Diff Tracker | `FrameStateDiffWidget` が previous/current snapshot、compare state、resource/pass/density 差分を表示する。PSO/CB/SRV/UAV の完全履歴と壊れ始めた frame の自動判定は未確認。 | 部分実装 |
| 4. Diagnostics Integration | Frame Debug、Profiler、Debug Console、Harness への summary/copy/filter/report 導線がある。常時表示 surface と warning vocabulary の完全統一は未確認。 | 部分実装／統合待ち |

### 現在の判定

Phase 1〜4 の UI と snapshot/diff 基盤は実装済み部分が多いが、live GPU resource の網羅的取得と hazard/history の実行確認が残る。マイルストーンは「部分実装／実行確認待ち」とする。
