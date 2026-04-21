# Live Frame Pipeline / Resource Watcher / State Diff Tracker Milestone

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
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE1_EXECUTION_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE2_EXECUTION_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE3_EXECUTION_2026-04-21.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE4_EXECUTION_2026-04-21.md`
