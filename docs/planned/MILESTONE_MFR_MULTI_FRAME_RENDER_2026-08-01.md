# マルチフレームレンダリング（MFR）実装マイルストーン

**日付**: 2026-08-01
**ベース**: After Effects 2022 "Multi-Frame Rendering" (MFR) + Artifact の既存レンダリング基盤
**現状**: `BackgroundTaskWorkerPool`, `Parallel::For`, `ThreadPool` が既に実装。単一フレームのパフォーマンスは高いが、レンダーキューは1フレームずつ逐次処理。
**狙い**: 複数フレームを同時にレンダリングし、CPUコア数を活かした高速化（4-8倍の速度向上を目標）

---

## MFR とは

AE 2022 の目玉機能。従来は1フレームずつ逐次レンダリングしていたのを、複数フレームを同時に並列レンダリングすることで、CPU コア数に応じた劇的な速度向上を実現。

```
逐次レンダリング: Frame0 → Frame1 → Frame2 → Frame3 ... (4コア中1コアのみ使用)
MFR:              Frame0─┐
                  Frame1─┼─→ 4コア同時実行（理論上4倍速）
                  Frame2─┤
                  Frame3─┘
```

---

## Phase 1: タスクディスパッチャー

### Step 1.1 — MFR ジョブ構造

**新規**: `ArtifactCore/include/Render/MFR/MFRJob.ixx`

```cpp
export module Core.Render.MFR.Job;

export namespace ArtifactCore::Render::MFR {

using FrameTask = std::function<bool(int frameNumber)>;

struct MFRJobConfig {
    int startFrame = 0;
    int endFrame = 100;
    int maxConcurrentFrames = 0; // 0 = CPUコア数自動
    int frameStep = 1;
    
    // メモリ管理
    size_t maxMemoryMB = 8192;           // MFR全体のメモリ上限
    size_t estimatedMemoryPerFrame = 256; // 推定量（MB）
    
    // 依存関係
    bool framesAreDependent = false; // true の場合逐次処理にフォールバック
    
    // エラー処理
    int maxRetryCount = 2;
    bool continueOnError = false;    // 1フレーム失敗でも継続
};

struct MFRProgress {
    std::atomic<int> completedFrames{0};
    std::atomic<int> failedFrames{0};
    int totalFrames = 0;
    
    float percentComplete() const {
        return totalFrames > 0
            ? 100.0f * (completedFrames + failedFrames) / totalFrames
            : 0.0f;
    }
};

struct MFRFrameResult {
    int frameNumber;
    bool success;
    QString errorMessage;
    double elapsedMs;
    size_t peakMemoryBytes;
};

struct MFRJobResult {
    std::vector<MFRFrameResult> frames;
    double totalElapsedMs;
    size_t totalPeakMemoryBytes;
    int completedCount, failedCount;
    float speedupVsSequential; // 何倍速かったか
};

} // namespace
```

### Step 1.2 — MFR ディスパッチャー

**新規**: `ArtifactCore/include/Render/MFR/MFRDispatcher.ixx`

```cpp
export module Core.Render.MFR.Dispatcher;

export namespace ArtifactCore::Render::MFR {

class MFRDispatcher {
public:
    MFRDispatcher();
    ~MFRDispatcher();

    /// メインエントリーポイント。ジョブを実行し全フレーム完了までブロック
    MFRJobResult executeBlocking(
        const MFRJobConfig& config,
        FrameTask frameTask, // 1フレームのレンダリング関数
        std::function<void(const MFRProgress&)> progressCallback = nullptr
    );

    /// 非同期実行（将来のワーカースレッド実装用）
    std::future<MFRJobResult> executeAsync(
        const MFRJobConfig& config,
        FrameTask frameTask
    );

    /// キャンセル
    void cancel();
    bool isCancelled() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace
```

### Step 1.3 — ディスパッチャー実装

**新規**: `ArtifactCore/src/Render/MFR/MFRDispatcher.cppm`

```cpp
MFRJobResult MFRDispatcher::executeBlocking(
    const MFRJobConfig& config,
    FrameTask frameTask,
    std::function<void(const MFRProgress&)> progressCallback)
{
    int concurrency = config.maxConcurrentFrames > 0
        ? config.maxConcurrentFrames
        : std::max(1, (int)std::thread::hardware_concurrency() - 2); // 2コア余らせる

    // メモリ制約を考慮した同時実行数調整
    if (config.maxMemoryMB > 0 && config.estimatedMemoryPerFrame > 0) {
        int memoryLimited = config.maxMemoryMB / (int)config.estimatedMemoryPerFrame;
        concurrency = std::min(concurrency, std::max(1, memoryLimited));
    }

    int totalFrames = (config.endFrame - config.startFrame) / config.frameStep;
    if (totalFrames <= 0) return {};

    MFRProgress progress;
    progress.totalFrames = totalFrames;

    std::vector<MFRFrameResult> results(totalFrames);
    std::atomic<int> nextFrame{0};
    std::atomic<bool> cancelled{false};

    auto worker = [&](int workerId) {
        while (true) {
            int idx = nextFrame.fetch_add(1);
            if (idx >= totalFrames || cancelled.load()) break;

            int frame = config.startFrame + idx * config.frameStep;
            auto start = std::chrono::steady_clock::now();

            bool ok = frameTask(frame);

            MFRFrameResult result;
            result.frameNumber = frame;
            result.success = ok;
            result.elapsedMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();

            results[idx] = result;
            if (ok) progress.completedFrames.fetch_add(1);
            else progress.failedFrames.fetch_add(1);

            if (progressCallback && (idx % 10 == 0 || idx == totalFrames - 1)) {
                progressCallback(progress);
            }

            if (!ok && !config.continueOnError) {
                cancelled.store(true);
                break;
            }
        }
    };

    std::vector<std::thread> workers;
    for (int i = 0; i < concurrency; ++i) {
        workers.emplace_back(worker, i);
    }
    for (auto& w : workers) w.join();

    MFRJobResult jobResult;
    jobResult.frames = results;
    jobResult.completedCount = progress.completedFrames;
    jobResult.failedCount = progress.failedFrames;
    jobResult.speedupVsSequential = totalFrames > 0
        ? (double)totalFrames / std::max(1, concurrency)
        : 1.0;

    return jobResult;
}
```

---

## Phase 2: レンダーキュー統合

### Step 2.1 — ArtifactRenderJob に MFR サポート追加

**変更**: `Artifact/src/Render/ArtifactRenderQueueService.cppm`

```cpp
class ArtifactRenderJob {
    // ... existing fields ...
    
    // MFR
    bool multiFrameEnabled = true;
    int mfrConcurrentFrames = 0;     // 0 = auto
    int mfrMemoryLimitMB = 8192;
};
```

### Step 2.2 — renderComposition の MFR 対応

```cpp
bool renderComposition(const ArtifactRenderJob& job, const ArtifactCompositionPtr& comp) {
    if (!job.multiFrameEnabled || job.startFrame == job.endFrame) {
        return renderSequential(job, comp); // 既存のパス
    }

    MFRJobConfig mfrCfg;
    mfrCfg.startFrame = job.startFrame;
    mfrCfg.endFrame = job.endFrame;
    mfrCfg.maxConcurrentFrames = job.mfrConcurrentFrames;
    mfrCfg.maxMemoryMB = job.mfrMemoryLimitMB;

    MFRDispatcher dispatcher;
    
    // 1フレームをレンダリングするラムダ
    auto frameTask = [&](int frame) -> bool {
        // composition snapshot をスレッドローカルに確保
        thread_local ArtifactCompositionPtr clonedComp = comp->cloneForRendering();
        clonedComp->goToFrame(frame);
        return renderSingleFrame(clonedComp, frame, job);
    };

    MFRJobResult result = dispatcher.executeBlocking(mfrCfg, frameTask,
        [&](const MFRProgress& p) {
            job->progress = p.percentComplete();
            Q_EMIT progressChanged(job->id, job->progress);
        });

    return result.failedCount == 0;
}
```

---

## Phase 3: メモリ最適化

### Step 3.1 — Composition スナップショット共有

全ワーカーがベースコンポジションの読み取り専用コピーを共有し、レンダリング結果の書き出しだけ別バッファにする:

```cpp
class CompositionMFRContext {
public:
    CompositionMFRContext(const ArtifactCompositionPtr& source);

    /// ワーカースレッド用の軽量ビューを取得
    ArtifactCompositionPtr acquireFrameView(int frame);

    /// 使用済みビューを返却（プールに戻す）
    void releaseFrameView(const ArtifactCompositionPtr& view);

private:
    ArtifactCompositionPtr master_;
    std::vector<ArtifactCompositionPtr> frameClones_; // 再利用可能なクローンプール
    std::mutex poolMutex_;
};
```

### Step 3.2 — GPU リソースの多重化

複数フレームを同時にレンダリングする場合、GPU テクスチャや RTV をフレームごとに個別確保:

```cpp
struct FrameRenderResources {
    std::unique_ptr<GPUTexture> renderTarget;
    std::unique_ptr<GPUTexture> depthTarget;
};

class MFROverlayPool {
public:
    FrameRenderResources acquire(int frameIndex);
    void release(int frameIndex);
    void resizeForConcurrency(int count);
    
private:
    std::vector<FrameRenderResources> pool_;
};
```

---

## Phase 4: 適応型同時実行数

### Step 4.1 — メモリ使用量モニタリング

```cpp
class MFRMemoryMonitor {
public:
    /// 現在のシステム空きメモリ（MB）を取得
    static size_t availableSystemMemory();

    /// 1フレームあたりの推奨メモリ使用量を更新
    void recordFrameMemoryUsage(size_t bytes);

    /// 最適な同時実行数を計算
    int optimalConcurrency(int maxConcurrency, size_t memoryLimitMB);
};
```

### Step 4.2 — 適応型調整

フレーム1の実際のメモリ使用量を計測し、それに基づいて同時実行数を動的調整:

```
1. 最初の10フレームを concurrency=1 で実行し、メモリ使用量を計測
2. 平均メモリ使用量 > 1/4 memoryLimit → concurrency = 2
3. 平均メモリ使用量 > 1/2 memoryLimit → concurrency = 1
4. 平均メモリ使用量 < 1/8 memoryLimit → concurrency++
```

---

## Phase 5: プログレス UI

### Step 5.1 — MFR プログレスパネル

```
┌── Rendering 4 frames in parallel ─────────────────┐
│ ████████████████░░░░░░░░░░░░░░░░ 45% (18/40)       │
│                                                     │
│ Frame 3: ✓ (1.2s)   Frame 7: ✓ (1.4s)              │
│ Frame 5: ⏳ 75%     Frame 8: ⏳ 42%                 │
│                                                     │
│ Estimated: 12s remaining                            │
│ Speed: 3.2x faster than sequential                  │
│ Memory: 1.2 GB / 8 GB                               │
│                                                     │
│ [Cancel]                                            │
└─────────────────────────────────────────────────────┘
```

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/Render/MFR/MFRJob.ixx` | 新規 | ジョブ構造体 |
| P1 | `ArtifactCore/include/Render/MFR/MFRDispatcher.ixx` | 新規 | ディスパッチャー |
| P1 | `ArtifactCore/src/Render/MFR/MFRDispatcher.cppm` | 新規 | 実装 |
| P2 | `ArtifactRenderQueueService.cppm` | 変更 | MFR 統合 |
| P3 | `ArtifactCore/include/Render/MFR/CompositionMFRContext.ixx` | 新規 | コンポジション共有 |
| P3 | `ArtifactCore/src/Render/MFR/MFROverlayPool.cppm` | 新規 | GPU リソースプーリング |
| P4 | `ArtifactCore/include/Render/MFR/MFRMemoryMonitor.ixx` | 新規 | メモリモニター |
| P4 | `ArtifactCore/src/Render/MFR/MFRMemoryMonitor.cppm` | 新規 | 実装 |
| P5 | `ArtifactRenderQueueManagerWidget.cppm` | 変更 | プログレス UI |

---

## 検証チェックリスト

- [ ] 4スレッドで 100フレームのレンダリング時間が逐次の 1/3 以下
- [ ] 全フレームの出力結果が逐次レンダリングとピクセルレベルで一致
- [ ] メモリ使用量が limit を超えない
- [ ] エラーフレームをスキップし他のフレームに影響しない（continueOnError=true）
- [ ] キャンセルが即座に反映される
- [ ] プログレスが正しく報告される
- [ ] キャッシュフレーム + MFR が競合しない（FrameCache のスレッドセーフ確認）
- [ ] GPU リソースの競合がない（スレッドごとに別 DeviceContext）
