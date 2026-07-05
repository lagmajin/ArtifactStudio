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