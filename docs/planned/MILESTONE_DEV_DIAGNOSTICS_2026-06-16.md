# M-DEBUG-1 Dev Diagnostics Foundation Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/DebugRenderHarnessWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/FrameResourceInspectorWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/FrameStateDiffWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`,
      `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`,
      `Artifact/src/Widgets/ArtifactProblemViewWidget.cppm`,
      `Artifact/src/Render/ArtifactIRenderer.cppm`,
      `Artifact/src/Service/ArtifactPlaybackService.cppm`,
      `Artifact/src/Service/ArtifactRenderQueueService.cppm`,
      `Artifact/src/Project/ArtifactProjectHealthChecker.cppm`
位置づけ: Diagnostics widget は完備済み (REPORT 確認)。本 milestone は **Performance overlay / GPU profiler / RenderDoc / qDebug 制御** の 4 機能を追加。
参照:
- `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2
- `docs/analysis/REPORT_CE_RENDER_ROI_2026-06-16.md` §5
- `docs/planned/MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`
- `docs/planned/MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`
- `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md`
- `docs/planned/MILESTONE_PERF_HOTPATH_2026-06-16.md`

---

## 1. 目的

`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 で抽出:

- Performance overlay (FPS / frame time / GPU time): 0 hit
- GPU profiling (Nsight / GPA): 0 hit
- RenderDoc integration: 0 hit
- Bug report dialog: 0 hit
- Crash dump analyzer: 0 hit

`REPORT_CE_RENDER_ROI_2026-06-16.md` §5 で Diagnostics widget 9 種は完備済みと確認。本 milestone は **未着手の 4 機能** を追加する。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/Diagnostics/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理

### 2.1 既存 Diagnostics widget (完備)

| Widget | 件数 |
|---|---:|
| DebugRenderHarnessWidget | 21 |
| ProblemViewWidget | 17 |
| FrameDebugViewWidget | 17 |
| TraceTimelineWidget | 18 |
| FrameResourceInspectorWidget | 16 |
| ProfilerPanelWidget | 16 |
| AppDebuggerWidget | 15 |
| FramePipelineViewWidget | 15 |
| FrameStateDiffWidget | 13 |

→ widget 階層は揃っている。

### 2.2 未実装 (0 hit)

- Performance overlay (FPS / frame time)
- GPU profiling (Nsight / GPA)
- RenderDoc integration
- Bug report dialog
- Crash dump analyzer
- GPU memory tracker
- Frame timing breakdown

---

## 3. 設計の柱

### 3.1 Performance Overlay

`Artifact/src/Widgets/Diagnostics/PerformanceOverlay.cppm`:

```cpp
class PerformanceOverlay : public QWidget {
public:
    explicit PerformanceOverlay(QWidget* parent = nullptr);

    // 表示位置
    void setAnchor(Anchor a);  // TopLeft / TopRight / BottomLeft / BottomRight

    // 表示内容
    void setShowFPS(bool on);
    void setShowFrameTime(bool on);
    void setShowGpuTime(bool on);
    void setShowMemory(bool on);

    // 統計
    void updateMetrics(const PerfMetrics& m);
};
```

- 60 fps で更新
- FPS / frame time (ms) / GPU time / memory (MB)
- `theme token` 経由の色
- drag で位置変更可能

### 3.2 GPU Profiler

`ArtifactCore/src/Render/GpuProfiler.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct GpuProfileMarker {
    QString name;
    double startMs;       // GPU clock
    double durationMs;
    QString category;      // "Render" / "Compute" / "Copy"
};

class GpuProfiler {
public:
    static GpuProfiler& instance();

    void beginMarker(const QString& name, const QString& category);
    void endMarker(const QString& name);

    // query
    QList<GpuProfileMarker> markersSince(int64_t sinceMs) const;

    // 集計
    double totalGpuTimeMs(int64_t sinceMs) const;
};
```

- Diligent の `BeginDebugGroup` / `EndDebugGroup` ベース
- 必要に応じて Nsight / GPA 用の拡張 hook

### 3.3 RenderDoc Integration

`ArtifactCore/src/Render/RenderDocHook.ixx`:

```cpp
class RenderDocHook {
public:
    static RenderDocHook& instance();

    bool isAvailable() const;
    bool beginCapture(const QString& label);
    bool endCapture();
};
```

- 起動時に `renderdoc.dll` の存在を検出
- 利用可能なら `beginCapture` / `endCapture` API を公開
- 利用不可なら no-op (silent fallback)
- 依存は **optional**

### 3.4 qDebug 制御

`ArtifactCore/src/Utils/DebugLog.ixx` を新規追加:

```cpp
namespace ArtifactCore {

enum class DebugCategory {
    Render,
    Cache,
    IO,
    Project,
    AI,
    UI,
    Timeline,
    Audio,
    Video,
};

class DebugLog {
public:
    static DebugLog& instance();

    void enable(DebugCategory cat, bool on);
    bool isEnabled(DebugCategory cat) const;

    void log(DebugCategory cat, const QString& msg);
};

} // namespace ArtifactCore
```

- render path の `qDebug()` を `DebugLog::log(DebugCategory::Render, ...)` に置換
- 各 category を enable / disable 可能
- `qDebug()` を render path から排除 (`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` の qDebug in render 0 hit を **明示的に維持**)

### 3.5 GPU Memory Tracker

`ArtifactCore/src/Render/GpuMemoryTracker.ixx`:

```cpp
struct GpuMemoryEntry {
    QString name;
    size_t bytes;
    QString category;  // "Texture" / "Buffer" / "Pipeline"
};

class GpuMemoryTracker {
public:
    static GpuMemoryTracker& instance();

    void registerAllocation(const QString& name, size_t bytes, const QString& category);
    void releaseAllocation(const QString& name);
    void registerPeak(const QString& name, size_t bytes);

    size_t currentBytes() const;
    size_t peakBytes() const;
    QList<GpuMemoryEntry> entries() const;
};
```

- `ArtifactIRenderer` の texture / buffer 確保で hook
- Performance overlay で表示

### 3.6 Frame Timing Breakdown

`ArtifactCore/src/Render/FrameTiming.ixx`:

```cpp
struct FrameTiming {
    double totalMs;
    double gpuMs;
    double cpuMs;
    double ioMs;
    double decodeMs;
    double composeMs;
    double uploadMs;
    double presentMs;
    int    drawCalls;
    int    dispatchCalls;
    int    triangles;
    int    textureBindings;
};

class FrameTimingRecorder {
public:
    void beginFrame();
    void endFrame();
    FrameTiming last() const;

    void beginSection(const QString& name);
    void endSection(const QString& name);
};
```

- 各 section の時間記録
- Performance overlay に統合

---

## 4. フェーズ計画

### Phase 1: DebugLog (P0, 1 セッション)

- `ArtifactCore/src/Utils/DebugLog.ixx` 新規
- 既存 `qDebug()` のうち render path のものを置換
- ApplicationSettingDialog に category 設定ページ

**Done criteria:**
- render path に `qDebug()` 呼出が 0
- category 単位で enable / disable が可能

### Phase 2: Frame Timing (P0, 1 セッション)

- `FrameTimingRecorder` 実装
- `ArtifactCompositionRenderController::renderOneFrame` で計測
- Performance overlay に表示

**Done criteria:**
- frame 単位の timing が記録される
- 各 section (gpu / cpu / io / decode 等) が分離

### Phase 3: Performance Overlay (P0, 1 セッション)

- `PerformanceOverlay` widget
- FPS / frame time / GPU time / memory 表示
- `ArtifactMainWindow` に toggle button

**Done criteria:**
- overlay が FPS を 60 fps で更新
- 表示項目を選択可能

### Phase 4: GPU Profiler (P0, 1〜2 セッション)

- `GpuProfiler` 実装
- Diligent `BeginDebugGroup` ベース
- render path に marker 埋込

**Done criteria:**
- 20+ marker が frame 内に埋込
- Performance overlay に統合

### Phase 5: GPU Memory Tracker (P1, 1 セッション)

- `GpuMemoryTracker` 実装
- `ArtifactIRenderer` の alloc / free に hook
- Performance overlay に統合

**Done criteria:**
- texture / buffer 確保で自動 track
- peak bytes 表示

### Phase 6: RenderDoc Integration (P1, 1 セッション)

- `RenderDocHook` 実装
- `renderdoc.dll` optional 検出
- menu に `Capture Frame` 追加

**Done criteria:**
- RenderDoc 利用可能時に capture 起動
- 利用不可時に silent fallback

### Phase 7: Bug Report + Crash Dump (P2, 別 milestone 推奨)

- Bug report dialog 追加
- Crash dump analyzer 追加
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_BUG_REPORT_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md` | 下位。本 milestone は performance overlay 追加。 |
| `MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md` | 下位。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_PERF_HOTPATH_2026-06-16.md` | 修正前後比較の計測先。 |

---

## 6. 不変条件 (Guardrails)

- 既存 Diagnostics widget 9 種は **温存**
- 新規 signal-slot 接続は `metricsUpdated / markerAdded` の 2 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- RenderDoc は **optional 依存**
- GPU profiler の marker 埋込は **render path の外** に閉じた wrapper 経由
- Performance overlay の更新は **60 fps 上限**

---

## 7. Done Criteria (全体)

- Performance overlay が FPS / frame time / GPU time / memory を 60 fps 表示
- FrameTiming が frame 単位で記録
- GpuProfiler が 20+ marker を埋込
- GpuMemoryTracker が texture / buffer を自動 track
- RenderDoc 統合が optional で動作
- DebugLog が category 単位で enable / disable
- render path に `qDebug()` が 0
- 既存 Diagnostics widget 9 種が温存
- 新規 `QImage` / `setStyleSheet` / signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` §2.2 と `REPORT_CE_RENDER_ROI_2026-06-16.md` §5 を正式 milestone に起こした。