# マイルストーン: 診断 / デバッグ機能監査 (2026-07-04)

> VS Code Debug / Chrome DevTools / Blender System Info / Unity Profiler 比較。

## 🔴 P0

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Frame Debugger（1フレームの全描画コール内訳）** | Unity/RenderDoc | ⚠️ FrameDebugSnapshot あり |
| **Performance Profiler（CPU/GPU タイムライン）** | Unity/Chrome | ✅ ProfilerPanelWidget あり |
| **GPU Time 計測** | Unity | ⚠️ beginFrameGpuProfiling 未接続 |
| **EventBus ログ表示** | - | ✅ EventBusDebugger あり |

## 🟡 P1

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Live Memory Allocator Trace** | Unity | ❌ |
| **Texture Memory 使用量一覧** | Unity/RenderDoc | ❌ |
| **Draw Call 数 / バッチ数 カウンター** | Unity | ❌ |
| **Shader Compile ログ** | Unity | ❌ |
| **Network Request ログ** | Chrome DevTools | ❌ |
| **Console / REPL** | VS Code/Blender | ⚠️ DebugConsoleWidget あり |
| **Crash Report 自動生成** | VS Code | ❌ |

---

## Static audit follow-up (2026-07-25)

現行の Diagnostics widgets と Core Diagnostics を照合した。P0 の表示・snapshot 基盤は想定より進んでいるが、GPU 計測の接続と P1 のリソース／ログ機能には未実装項目が残る。

| 機能 | 現行確認 | 判定 |
|---|---|---|
| Frame Debugger | `FrameDebugSnapshot`、Frame Debug view、pipeline/resource/diff view が存在 | 部分〜基盤実装 |
| Performance Profiler | `ProfilerPanelWidget` と frame/render cost の表示基盤が存在 | 部分実装 |
| GPU Time | renderer / snapshot に GPU timing 項目はあるが、設計書記載の `beginFrameGpuProfiling` 接続は未確認 | 部分実装 |
| EventBus ログ | EventBus debugger の実装が存在 | 実装済み |
| Trace / Crash | Core Trace、CrashHandler、crash report parser、TraceTimeline が存在 | 基盤実装 |
| Live Memory / Texture Memory | allocation の一覧・自動追跡・texture 使用量 surface は未確認 | 未実装 |
| Draw Call / Batch counter | FrameDebug の render cost fields はあるが、独立した常時カウンターは未確認 | 部分実装 |
| Shader Compile / Network log | 専用の診断 surface は未確認 | 未実装 |
| Console / REPL | Debug Console はあるが、REPL としての完全な実行導線は未確認 | 部分実装 |
| Crash report auto-generation | crash の収集・解析基盤はあるが、自動 report UI 連携は未完了 | 部分実装 |

**判定**: P0 の診断基盤は部分的に実装済み、P1 の memory / shader / network 項目は未着手。GPU timing は項目の存在だけでは接続完了と扱わない。
