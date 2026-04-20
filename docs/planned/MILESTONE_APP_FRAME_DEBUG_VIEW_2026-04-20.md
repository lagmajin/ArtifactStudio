# App Frame Debug View Milestone

ArtifactStudio 本体に組み込む、`簡易 RenderDoc` 風のフレームデバッグビューを定義するマイルストーン。

この機能は外部プロセスをデバッグするものではなく、`Artifact` アプリ自身が描画した 1 フレームを、実行中に止めずに観測・比較・追跡するためのものとする。

`App Internal Debugger` の中でも、とくに「frame を固定して原因を追う」部分を独立させた実行計画として扱う。

## Goal

- 単一フレームを固定して、入力 / 中間状態 / 出力を同じ画面で追えるようにする
- render pass / layer / attachment / readback / fallback の関係を、フレーム単位で読めるようにする
- 問題フレームの再現時に、比較・スクラブ・ステップ・保存を最小操作で行えるようにする
- 既存の profiler / diagnostics / playback / render queue の情報を再利用し、重複した debug surface を増やさない

## Non-Goals

- OS レベルのアタッチ、ブレークポイント、メモリ編集、逆アセンブル
- 外部 RenderDoc そのものの代替実装
- 任意コード実行を許す危険なデバッグコンソール
- GPU ドライバ内部の完全な命令列可視化
- QtCSS を使った専用 debug テーマの追加
- 新しい公開 signal/slot の大量追加

## Design Principles

- Read-first
  - 最初は観測・固定・比較に集中し、破壊的な操作は後回しにする
- Frame-bound
  - フレーム番号と timestamp を軸に、描画状態を束ねる
- Backend-aware
  - CPU / GPU / fallback の差を hidden にせず表示する
- Capture-light
  - hot path を重くしない固定サイズの記録に寄せる
- No new global wiring
  - 中央集権の signal/slot バスは増やさない

## Implementation Targets

### Core Read Surface

- `ArtifactCore/include/Frame/*`
- `ArtifactCore/include/Render/*`
- `ArtifactCore/include/Playback/*`

想定する既存連携先:

- `ArtifactRenderQueueService`
- `ArtifactPlaybackEngine`
- `CompositionRenderer` / render path の要約情報

### App Read Surface

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`

想定する既存連携先:

- `CompositionRenderController`
- `ArtifactPlaybackEngine`
- `ArtifactDebugConsoleWidget`
- `ProfilerPanelWidget`
- `ArtifactRenderQueueService`

### New UI Surface

- `FrameDebugViewWidget`
- `FrameDebugDock`
- `FrameDebugController`

この 3 つは新規追加候補だが、最初から分離しすぎず `App Debugger` 既存 surface への dock 追加で始めてもよい。

## Proposed Shape

### 1. Frame Debug Shell

`App Debugger` から開ける、または単独で dock できる `Frame Debug View` を想定する。

主な表示領域:

- `Frame Summary`
  - frame number / timestamp / playback state
  - current project / composition / selection / backend
  - failed frame / rerender candidate / stale state
- `Pass Graph`
  - render pass の順序
  - pass ごとの入力 / 出力 / attachment
  - clear / resolve / readback の要約
- `Resource View`
  - texture / buffer / surface / cache の参照一覧
  - source / intermediate / final output の関係
  - read-only のサムネイルまたは軽量プレビュー
- `Compare`
  - 現在フレームと前後フレームの比較
  - A/B 切り替え
  - 差分の要点表示
- `Capture`
  - フレーム固定
  - 保存
  - bundle export
  - copyable report
- `History`
  - 最近の capture
  - failed frame の自動保存履歴
  - scrub / step の履歴

主要な表示責務は次の既存クラスに寄せる。

- `CompositionRenderController`
  - 現在フレームの render summary
  - render path / backend / fallback の状態
  - frame 固定の起点
- `ArtifactPlaybackEngine`
  - current frame / playing / paused / seek state
  - step / scrub / frame jump の制御
- `ArtifactRenderQueueService`
  - failed frame / rerender candidate / queue 状態
  - capture bundle の材料
- `ArtifactIRenderer`
  - attachment / texture / readback の要約
  - render output の参照情報

### 2. Frame Capture Model

UI から読むための軽量なフレーム単位スナップショットを定義する。

含める候補:

- frame id / timestamp / duration
- current project / composition / layer selection
- playback position / playing / paused / seek state
- render backend / fallback state / capability flag
- render pass sequence / pass status / pass count
- attachment / input / output の参照情報
- readback summary / missing resource / stale resource
- diagnostics summary / warnings / errors
- compare target id / previous capture id

対応付けの目安:

- `FrameDebugSnapshot`
  - 画面に出す 1 フレーム分の総合要約
- `FrameDebugCapture`
  - 実際に固定されたフレームの記録
- `FrameDebugPassRecord`
  - pass ごとの順序と状態
- `FrameDebugResourceRecord`
  - texture / buffer / surface の参照一覧
- `FrameDebugCompareState`
  - A/B 比較用の現在状態
- `FrameDebugCaptureRequest`
  - capture / pin / export の要求
- `FrameDebugBundle`
  - support 共有用 bundle

### 3. Pass and Resource Inspector

RenderDoc っぽく見せたい部分の中心だが、実体は `Artifact` 向けの read-only inspector とする。

表示候補:

- pass 名と順序
- 各 pass の入力 texture / buffer
- output attachment の参照先
- cache hit / miss
- fallback が入った箇所
- 失敗時の最後の正常フレームとの差

実装の中心候補:

- `ArtifactCompositionRenderController`
  - frame summary の元データ
  - pass / output の要約生成
- `ArtifactRenderQueueService`
  - failed frame / queue metadata
- `ArtifactIRenderer`
  - readback / attachment 参照の収集
- `ArtifactDebugConsoleWidget`
  - 既存の診断テキスト流し込み口
- `ProfilerPanelWidget`
  - 補助的な perf trace の表示

### 4. Frame Compare and Step

単に見るだけでなく、前後フレームを追えるようにする。

表示候補:

- 前フレーム / 現フレーム / 次フレーム
- scrub で frame を切り替える
- step で 1 フレームずつ進める
- A/B compare で差分の当たりを付ける

操作の担当候補:

- `ArtifactPlaybackEngine`
  - step / frame advance / frame retreat
- `ArtifactTimelineScrubBar`
  - frame scrub
- `FrameDebugController`
  - compare state の保持と切り替え
- `FrameDebugViewWidget`
  - compare UI と固定表示

## Architecture

### Core Side

`ArtifactCore` 側に、フレームデバッグ用の読み取り API を持たせる。

候補:

- `FrameDebugSnapshot`
- `FrameDebugCapture`
- `FrameDebugPassRecord`
- `FrameDebugResourceRecord`
- `FrameDebugCompareState`
- `FrameDebugCaptureRequest`
- `FrameDebugBundle`

責務:

- frame 単位の状態収集
- pass / resource / attachment の要約
- fixed-size ring buffer への記録
- compare 用の差分材料生成
- bundle export 用の構造化データ提供

既存の実装候補:

- `ArtifactCore/include/Render/ArtifactRenderQueueService.ixx`
- `ArtifactCore/include/Playback/*`
- `ArtifactCore/include/Frame/*`

### Data Flow

1. `ArtifactPlaybackEngine` が現在フレームと再生状態を持つ
2. `CompositionRenderController` が frame summary を組み立てる
3. `ArtifactRenderQueueService` が failed frame と queue metadata を返す
4. `ArtifactIRenderer` の要約から attachment / resource 情報を取る
5. `FrameDebugController` が `FrameDebugSnapshot` へまとめる
6. `FrameDebugViewWidget` が表示と compare を担当する

### App Side

`Artifact` 側に、フレームデバッグ UI と既存 view の統合を持たせる。

候補:

- `FrameDebugViewWidget`
- `FrameDebugDock`
- `FrameDebugController`

責務:

- capture の開始 / 停止 / 固定
- frame summary / pass graph / resource view の表示
- compare / scrub / step の操作
- 保存と report 生成
- failed frame の即時参照

既存 UI への寄せ先:

- `ArtifactDebugConsoleWidget`
  - まずは text / summary の fallback 表示を置く
- `ProfilerPanelWidget`
  - 簡易 trace / timing の補助表示
- `ArtifactCompositionRenderWidget`
  - viewport からの frame pin / step 起点
- `ArtifactTimelineScrubBar`
  - frame scrub の入力起点

### Integration Path

- 既存の `EventBus` と render / playback / queue の state change を読む
- 既存の profiler overlay / diagnostics panel を再利用する
- render path ごとの中間状態は、無理に全部見せず必要な要約から始める
- UI 更新は既存の state change 経路を読むだけに寄せる

## Phase Plan

### Phase 1: Frame Capture Contract

対象:

- `ArtifactCore` の frame / render / playback read surface
- `ArtifactPlaybackEngine`
- `CompositionRenderController`
- `ArtifactRenderQueueService`

作業項目:

- frame 単位の capture データ構造を定義する
- pass / attachment / resource の最小要約を作る
- fixed-size ring buffer で保持できるようにする
- backend 非対応時の empty / unavailable 状態を決める

### Phase 2: Pass / Resource Inspector

対象:

- `ArtifactCompositionRenderController`
- `ArtifactIRenderer`
- `ArtifactRenderQueueService`
- `ArtifactDebugConsoleWidget`

作業項目:

- pass sequence を一覧できるようにする
- 各 pass の input / output / attachment を表示する
- resource 参照と cache 状態を追えるようにする
- failed frame の要点を読めるようにする

### Phase 3: Compare / Scrub / Step

対象:

- `ArtifactPlaybackEngine`
- `ArtifactTimelineScrubBar`
- `FrameDebugController`
- `FrameDebugViewWidget`

作業項目:

- 前後フレームの比較を実装する
- frame scrub と 1-frame step を追加する
- A/B compare で差分を追えるようにする
- 固定フレームの保存と呼び出しを整理する

### Phase 4: Export and Diagnostics

対象:

- `FrameDebugController`
- `ArtifactDebugConsoleWidget`
- `ArtifactRenderQueueService`
- `ProfilerPanelWidget`

作業項目:

- capture bundle を保存できるようにする
- copyable report を生成する
- failed frame の自動保存を検討する
- support 向けに再現手順と bundle をまとめる

## UI Surfaces

- App Debugger Dock
- Frame Debug View Dock
- Diagnostics panel integration
- Render / Playback overlay integration
- Copy report action

## Success Criteria

- 1 フレームの描画内容を、実行中に止めずに追える
- pass / resource / attachment の関係が読める
- failed frame を固定して前後比較できる
- compare / scrub / step が最小操作で使える
- backend 差や fallback を UI で隠しすぎない
- 既存の render / playback / diagnostics 導線を壊さない
- 新しい signal/slot の追加なしで成立する

## Risks

- capture を細かくしすぎると hot path が重くなる
- pass / resource を全部見せようとすると UI が読みにくくなる
- backend ごとの差異を均一化しすぎると、原因追跡に必要な情報が消える
- compare を深くしすぎると、通常利用の軽快さを損なう
- unsafe な操作を入れると、本番用アプリの安定性を壊す

## Recommended First Cut

1. Frame Capture Contract
2. Pass / Resource Inspector
3. Compare / Scrub / Step
4. Export and Diagnostics

## Current Position

このマイルストーンは `App Internal Debugger` の frame タブを実体化するための下位計画として置く。

`Composition Editor` / `Contents Viewer` / `Render Queue` / `Playback` にはそれぞれ断片的な debug 情報があるため、
まずは 1 フレームに絞って `何が入力で、何が中間で、何が出力か` を読めるようにするのが安全。

とくに重視するのは、① フレーム固定、② パス/リソースの検査、③ 比較/ステップ、④ 保存/診断の 4 本である。

## Related

- `docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE1_EXECUTION_2026-04-20.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE2_EXECUTION_2026-04-20.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE3_EXECUTION_2026-04-20.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_PHASE4_EXECUTION_2026-04-20.md`
