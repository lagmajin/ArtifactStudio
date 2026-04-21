# マイルストーン: Lightweight Tracer / Frame Timeline

> 2026-04-21 作成

## 目的

ArtifactStudio に、超軽量のトレーシング基盤を組み込む。

狙いは「重い外部 profiler の代替」ではなく、アプリ内で即座に読める観測面を持つこと。

このマイルストーンは次の 4 段で構成する。

- ① Crash Stack Capture
- ② Scope Tracer Core
- ③ Frame Timeline Visualization
- ④ Thread Trace

## 背景

今の diagnostics は profiler と event bus debugger が中心だが、次の視点が不足している。

- フレームごとに `Render / Decode / UI / Event` を並べて読む視点
- thread id 付きで lock の取得・解放を追う視点
- crash 時の stack をその場で残す視点

これらを separate feature にせず、同じ trace model でまとめる。

## Goal

- frame 単位で scope を帯にして見せる
- render / decode / ui / event を同じ timeline 上に並べる
- thread id 付きの event log を持つ
- lock acquire / release を記録して deadlock 調査を楽にする
- 既存の profiler / debug console / event bus debugger と重複しない

## Non-Goals

- 外部 Tracy の完全再実装
- OS 全体への常時フック
- デバッガアタッチ
- 重い文字列整形の常時実行
- 新しい signal/slot の大量追加

## Design Principles

- Ultra-light
  - 記録は最小コストにする
- Frame-aware
  - frame boundary で切って読めるようにする
- Thread-aware
  - thread id を必ず残す
- Lock-aware
  - mutex 取得/解放を追えるようにする
- Read-first
  - まずは観測、次に分析、最後に制御

## Proposed Shape

### Module Layout

最初の実装では、次のモジュール名を使う想定にする。

- `Core.Diagnostics.Trace`
- `Core.Diagnostics.Trace.Crash`
- `Core.Diagnostics.Trace.Scope`
- `Core.Diagnostics.Trace.Timeline`
- `Core.Diagnostics.Trace.Thread`

まずは `Core.Diagnostics.Trace` を umbrella として置き、必要に応じて leaf module を分ける。

ファイル名の候補:

- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `ArtifactCore/include/Diagnostics/TraceCrash.ixx`
- `ArtifactCore/include/Diagnostics/TraceScope.ixx`
- `ArtifactCore/include/Diagnostics/TraceTimeline.ixx`
- `ArtifactCore/include/Diagnostics/TraceThread.ixx`

### 1. Crash Stack Capture

- crash 時の stack を保存
- minidump までは後回し
- report に frame / thread / last scope を含める

### 2. Scope Tracer Core

- `begin/end` の軽量 scope
- `Render / Decode / UI / Event` のラベルを共通化
- スレッドローカルに近い軽量な記録

### 3. Frame Timeline Visualization

- frame ごとに scope を横並び表示
- `Render / Decode / UI / Event` をレーン分け
- 1 フレーム内の時間配分を一目で読めるようにする
- `ProfilerPanelWidget` から辿れるようにする

### 4. Thread Trace

- thread id 付きログ
- lock acquire / release 記録
- deadlock / starvation の見つけやすさを上げる

## Naming Rules

- `Trace` は「記録するもの」全体の umbrella
- `Crash` は crash stack / crash report
- `Scope` は begin/end の軽量計測
- `Timeline` は frame ごとの可視化
- `Thread` は thread id / lock 追跡

`Profiler` は既存の timing surface として残し、`Trace` はより低レベルな記録面として扱う。

## Implementation Targets

### Core Trace Surface

- `ArtifactCore/include/Diagnostics/*`
- `ArtifactCore/include/Utils/PerformanceProfiler.ixx`
- `ArtifactCore/include/Core/*`

### App Trace Surface

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/EventBusDebuggerWidget.cppm`
- `Artifact/src/AppMain.cppm`

## Suggested Execution Order

1. crash stack capture
2. scope tracer core
3. frame timeline visualization
4. thread trace / lock trace

## Related Docs

- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
- [`docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md`](X:/Dev/ArtifactStudio/docs/analysis/CORE_MODULE_MISSING_FEATURES_2026-04-19.md)
- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_EXECUTION_2026-04-21.md)
- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_EXECUTION_2026-04-21.md)
- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE3_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE3_EXECUTION_2026-04-21.md)
