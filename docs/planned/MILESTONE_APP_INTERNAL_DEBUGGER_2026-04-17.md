# App Internal Debugger Milestone

ArtifactStudio 本体に組み込む、アプリ専用の内蔵デバッガを定義するマイルストーン。

このデバッガは「任意の外部プロセスをデバッグするもの」ではなく、
`Artifact` アプリ自身の状態・描画・再生・イベント・診断を 1 つの surface に集約して読むためのものとする。

## Goal

- アプリの現在状態を、実行中に止めずに観測できるようにする
- Composition / Layer / Playback / Render Queue / Asset / Diagnostics を横断して追えるようにする
- 問題再現時に、ログ・イベント・状態スナップショット・フレーム単位の記録を同じ場所から取得できるようにする
- 既存の profiler / diagnostics / event bus / inspection surface を統合し、重複した debug UI を増やさない

## Non-Goals

- 外部プロセスの一般的なデバッガ機能
- OS レベルのアタッチ、ブレークポイント、メモリ編集、逆アセンブル
- 任意コード実行を許す危険なスクリプトコンソール
- QtCSS を使った専用 debug テーマの追加
- 新しい公開 signal/slot の大量追加

## Design Principles

- Read-first
  - 最初は観測と記録に集中し、操作系は後から足す
- Workspace-bound
  - アプリ内の現在プロジェクトと現在コンポジションを中心に見る
- Event-driven, not polling-heavy
  - 既存の `EventBus` と状態更新経路を優先する
- Overlay-friendly
  - 既存の profiler overlay / viewer HUD / timeline overlay に寄せる
- No new global wiring
  - 新しい中央集権 signal/slot バスは作らない

## Proposed Shape

### 1. Debug Shell

アプリ右上または View menu から開く、ドッキング可能な `App Debugger` パネルを想定する。

主なタブ:

- `Trace`
  - 最近の app event / layer event / composition event / render event
  - frame number と timestamp を持つ時系列
  - filter / search / pause / pin
- `State`
  - 現在プロジェクト
  - 現在コンポジション
  - 選択レイヤー
  - 現在フレーム
  - 再生状態
  - 使用中の backend / render path
- `Frame`
  - 単一フレームの入力 / 出力 / selection / backend / queue の固定表示
  - failed frame / rerender candidate / readback summary
  - compare / scrub / step
- `Crash`
  - crash bundle の保存
  - 最終 snapshot
  - recent trace
  - diagnostics report
  - 共有用テキストの生成
- `Diagnostics`
  - validation result
  - warning / error aggregation
  - copyable text report

### 2. Debug Snapshot Model

アプリ状態を UI から読むための軽量スナップショットを定義する。

含める候補:

- app version / build mode
- current project path / asset root
- current composition id / name / size / fps
- current layer id / type / visibility / solo / lock / opacity
- playback current frame / playing / paused
- current render backend / fallback state
- render queue job count / selected job / failed frame count
- last diagnostics summary
- recent event ring buffer
- current frame bundle id / source frame / target frame
- last crash capture path / timestamp

### 3. Event Timeline

イベントを生ログで垂れ流すのではなく、意味のあるイベントだけを短く記録する。

例:

- project changed
- composition changed
- selection changed
- layer mutated
- playback seek / play / stop
- render queue state changed
- frame capture started / completed
- crash bundle captured
- failed frame detected
- asset folder changed

### 4. Inspectors

選択対象ごとに inspector を切り替える。

- Composition inspector
- Layer inspector
- Render queue inspector
- Asset inspector
- Playback inspector
- Frame inspector
- Crash inspector

各 inspector は read-only を基本にし、必要な場合だけ安全な操作を出す。

## Architecture

### Core Side

`ArtifactCore` 側に、アプリ内デバッグ用の読み取り API を持たせる。

候補:

- `DebugSnapshot`
- `DebugEventRecord`
- `DebugTraceBuffer`
- `DebugFrameBundle`
- `DebugCaptureRequest`
- `DebugReport`
- `DebugCrashBundle`

責務:

- 現在状態の収集
- ring buffer への記録
- frame bundle の固定
- 検証結果の要約
- UI 向けの構造化データ提供
- crash bundle の生成

### App Side

`Artifact` 側に、デバッグ UI と既存 view の統合を持たせる。

候補:

- `AppDebuggerWidget`
- `AppDebuggerDock`
- `AppDebuggerController`

責務:

- trace の表示
- state snapshot の表示
- frame bundle の表示
- diagnostics report のコピー
- crash bundle の保存
- debug capture の実行
- overlay の on/off 切替

### Integration Path

- 既存の `EventBus` を購読してイベントを集約する
- 既存の profiler overlay / panel を利用し、機能を複製しない
- render / playback / queue / asset / composition の既存 service から snapshot を組み立てる
- UI 更新は既存の state change 経路を読むだけに寄せる

## Phase Plan

### Phase 1: Event Trace Core

- recent event ring buffer を追加する
- composition / layer / playback / queue / render のイベントを1本に集約する
- filter / search / pause / pin を実装する
- event と frame number / timestamp を持たせる
- 既存 profiler の要約を trace に接続する

### Phase 2: State Inspector

- 現在プロジェクト / composition / layer / playback を1画面で読む
- render backend / queue / asset root / diagnostics summary を読む
- read-only の状態表示を整理する
- VS にない制作文脈の強みを出す

### Phase 3: Frame Debugger

- 単一フレームの固定表示を実装する
- current frame / source frame / target frame / output frame を結びつける
- failed frame / rerender candidate / readback summary を見る
- compare / scrub / step を最小限で用意する

### Phase 4: Crash Dump and Bundle Export

- crash bundle を自動保存できるようにする
- final snapshot / recent trace / diagnostics report をまとめる
- 保存先とタイムスタンプを記録する
- support 向けの copyable report を生成する

## UI Surfaces

- App Debugger Dock
- Profiler Overlay integration
- Diagnostics panel integration
- Timeline / Viewer / Layer Solo View 上の lightweight debug badges
- Copy report action

## Success Criteria

- 現在のアプリ状態を、複数の画面を開き直さずに読める
- 障害時に「何が選ばれていて、何が再生されていて、どの backend が使われているか」が分かる
- event trace から frame 単位の原因追跡ができる
- state inspector で VS にない app-specific context を読める
- frame debugger で 1 フレームだけを固定して追える
- crash bundle で落ちた直後の文脈を回収できる
- 既存の render / playback / asset / queue の操作導線を壊さない
- 新しい signal/slot の追加なしで成立する

## Risks

- debug UI を増やしすぎると、逆に状態が見えにくくなる
- snapshot を取りすぎると重くなる
- render / playback のリアルタイム状態と UI 更新が競合する
- unsafe な操作を入れると、本番用アプリの安定性を壊す
- frame debugger を深くしすぎると、通常利用の軽快さを損なう

## Recommended First Cut

1. Event trace
2. State inspector
3. Frame debugger
4. Crash bundle export

## Current Position

このマイルストーンは、`profiler` と `diagnostics` と `inspection surface` を束ねる上位層として置く。

既存の `Composition Editor` / `Contents Viewer` / `Layer Solo View` / `Render Queue` にはそれぞれ debug 情報があるため、
まずはそれらを横串で読める app-level debugger を作り、後から control を足すのが安全。

特に重視するのは、② ログ＆イベントトレース、③ 状態インスペクタ、④ フレームデバッガ、⑥ クラッシュダンプの4本である。

## Related

- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`

この文書はアプリ内デバッグ全体の上位設計であり、
`Frame Debug View` はその中の「1 フレームを固定して追う」実行計画として切り出したものにする。
