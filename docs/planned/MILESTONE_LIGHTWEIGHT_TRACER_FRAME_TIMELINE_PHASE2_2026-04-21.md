> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md](MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)

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

- 親文書へ統合済み
- `Trace.Scope`
- hot path hook

---

## Static audit follow-up (2026-07-25)

`Diagnostics.Trace` と `PerformanceProfiler` の現行実装を照合した。ビルド・実機計測は未実施。

| 完了条件 | 現状 | 判定 |
|---|---|---|
| scope 名が記録できる | `TraceRecorder::recordScope()` と `TraceScopeRecord` が name、domain、start/end、frame、thread を保持する。`ProfileScope` の begin/end RAII も確認した。 | 実装済み |
| Render / Decode / UI / Event を同じ model で扱える | `TraceDomain` に各 domain が定義され、frame debug pass から domain へ変換する経路がある。 | 実装済み／網羅性確認待ち |
| thread-local に近い低オーバーヘッド構造 | `PerformanceProfiler` は `thread_local ThreadState` を使用する。一方 `TraceRecorder` の常時計測負荷は未計測。 | 部分実装／性能確認待ち |
| フレーム境界で集計できる | profiler の `beginFrame()`／`endFrame()`、Trace の frame snapshot と bounded snapshot を確認した。 | 実装済み／性能確認待ち |

### 現在の判定

Scope の共通モデルとフレーム集計基盤はコード上実装済み。全 hot path の記録網羅性と常時計測の負荷は未検証のため、Phase 2 は「実装済み／性能・実行確認待ち」とする。
