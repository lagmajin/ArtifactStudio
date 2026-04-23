# Phase 2 実行メモ: Scope Tracer Core

> 2026-04-21 作成

## 目的

軽量な scope 記録の本体を作る。

ここで `Render / Decode / UI / Event` という主レーンを共通化する。

## 重点対象

- `ArtifactCore/include/Diagnostics/*`
- `ArtifactCore/include/Utils/PerformanceProfiler.ixx`
- `Artifact/src/Render/*`
- `Artifact/src/Playback/*`

## やること

- `begin/end` の軽量 scope を記録する
- thread-local に近い低オーバーヘッド構造にする
- フレーム境界で集計できるようにする

## 完了条件

- scope 名が記録できる
- `Render / Decode / UI / Event` を同じ model で扱える

## File Tickets

- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_EXECUTION_2026-04-21.md)
- `Trace.Scope`
- hot path hook

