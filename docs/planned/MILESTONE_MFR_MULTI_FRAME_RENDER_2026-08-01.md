# マルチフレームレンダリング（MFR）実装マイルストーン

**最終更新:** 2026-08-21
**日付**: 2026-08-01
**ベース**: After Effects 2022 "Multi-Frame Rendering" (MFR) + Artifact の既存レンダリング基盤
**現状**: MFR のジョブ契約・並列 dispatcher・memory budget・retry/cancel/progress と Render Queue の設定・呼出し基盤は実装済み。ただし通常の実レンダーループでは `useMfr = false` の経路が残っており、実MFR有効化、thread-safety、出力順序、GPU resource競合、性能向上は未受入。
**狙い**: 複数フレームを同時にレンダリングし、CPUコア数を活かした高速化（4-8倍の速度向上を目標）

---

## Update 2026-08-21: 共有可能状態の洗い出し(ワーカーごとスナップショット前提)

MFR 有効化の前提である「ワーカーごとに `cloneCompositionSnapshot()` を持たせる」方式を採った場合に、なお競合する共有可変状態を実コードで洗い出した。結論: **CPU MFR は「レイヤー単位のキャッシュ分離」を Phase 0 として片付ければ成立する**。GPU MFR は Phase 3 以降(コマンドバッファ分離が必要)。

### 洗い出し結果一覧

| # | 共有状態 | 所有者 | 危険度 | 根拠 | スナップショット複製で解消されるか |
|---|---|---|---|---|---|
| S1 | FormParticleLayer の mutable キャッシュ群(cacheDirty / cachedSignature / cachedFrame / cachedRenderData / sourceImage / cachedBasePoints 等12種) | レイヤーImpl | **高** | `ArtifactFormParticleLayer.cppm:352-363` — const 描画パスから全書き換え | ✅ レイヤーごと fromJson 複製で分離 |
| S2 | ImageLayer の mutable キャッシュ群(sequenceSource_ / cache_ / cacheBuffer_ / crop キャッシュ) | レイヤーImpl | **高** | `ArtifactImageLayer.cppm:861-872`、`toQImage()` が read-modify-write | ✅ 同上 |
| S3 | SolidImageLayer の mutable 描画キャッシュ(cachedImage_ / cachedColor_ / gradient* 8種) | レイヤーImpl | **高** | `ArtifactSolidImageLayer.cppm:94-105`、draw() 毎 compare&update | ✅ 同上 |
| S4 | ShapeLayer の遅延ジオメトリ/画像キャッシュ(cachedImage_ / cachedShapePoints_ / nativeGeometry 等10種+dirty flag 約40箇所の手動管理) | レイヤーImpl | **高** | `ArtifactShapeLayer.cppm:1010-1028` | ✅ 同上(dirty flag 漏れは別問題として常在リスク) |
| S5 | ParticleLayer の cachedFrame / cachedFrameNumber / lastTime / playing | レイヤーImpl | **高→低** | `ArtifactParticleLayer.cppm:360-363` — GPU パスは goToFrame 全再計算で決定論的。フォールバック QImage キャッシュのみ競合 | ✅ 同上(GPU パスはフレーム独立) |
| S6 | TextLayer の renderedBuffer_ 遅延再ラスタライズ(const_cast で更新) | レイヤー | **高** | `ArtifactTextLayer.ixx:124`、`currentFrameBuffer()` L2674-2676 | ✅ 同上 |
| S7 | VideoLayer の frameCache_ / decodeFuture_ / **非 atomic openRequestId_** | レイヤーImpl | **高** | `ArtifactVideoLayer.cppm:439-440,1026-1031` | ⚠️ 複製で分離されるが、デコードワーカーと AssetManager ペイロード経由で接続(下記 S9) |
| S8 | AssetManager への書き込み(acquireSource / releaseSource / publishDecodedPayload / localizeSource) | グローバルシングルトン | **高** | Image: `ArtifactImageLayer.cppm:106,1266,1306...` Video: `ArtifactVideoLayer.cppm:1054,1090-1104...` | ❌ 解消されない — 参照カウントとペイロード map への並列書き込み。AssetManager 側の mutex 保護確認が必須 |
| S9 | static AsyncAssetReadScheduler(画像読み込み共有キュー) | プロセス共通 | 中〜高 | `ArtifactImageLayer.cppm:705` 関数内 static | ❌ 解消されない — key 衝突と待ち行列飽和。同時実行数に応じたワーカー数見直しが必要 |
| S10 | RenderContextRegistry(グローバル snapshots_ map、無保護) | シングルトン | 中〜高 | 登録: `ArtifactRenderQueueService.cppm:1357-1388`(現在は compositionFrameStateMutex_ 内)、Registry 本体 `ArtifactRenderContext.ixx:491-498` に mutex なし | ❌ 解消されない — Registry に mutex 追加 or フレーム番号込み key の衝突回避 |
| S11 | ArtifactIRenderer::cmdBuf_(RenderCommandBuffer、無保護) | Renderer Impl | **高**(GPU時のみ) | `ArtifactIRenderer.cppm:461,1483,1594-1599` — drawParticles が targetRTV 書換+append を無ロック | ❌ GPU MFR の主ブロッカー。CPU パスでは不使用 |
| S12 | gpuRenderer_ 単一インスタンス / gpuSurfaceCache(フレーム毎 clear) | QueueService Impl | **高**(GPU時のみ) | `ArtifactRenderQueueService.cppm:6719-6723` の clear は逐次前提 | ❌ GPU MFR はワーカーごとの device context 分離が必要(Phase 3) |
| S13 | AbstractComposition thumbnailCache_(mutable) | コンポジションImpl | 中 | `ArtifactAbstractComposition.cppm:1171-1173` | ✅ 複製で分離(getThumbnail を MFR 中に呼ぶ経路がなければ無害) |
| S14 | VideoLayer の関数内 static ProxyManager | プロセス共通 | 中 | `ArtifactVideoLayer.cppm:2261` | ❌ 生成は thread-safe、内部状態は要確認 |
| S15 | SolidImageLayer の非 atomic ログカウンタ(drawLogSamples++) | 関数内 static | 低 | `ArtifactSolidImageLayer.cppm:521` | 技術的に UB、ログ欠落のみ |

### 好材料(MFR に向いている点)

- **GPUTextureCacheManager / makeGpuImageUploadBuffer へのアクセスは Layer 配下ゼロ** — GPU アップロードはレンダラ側に分離済みで、レイヤーは CPU バッファ止まり
- **thread_local の隠れた使用は検出ゼロ**
- **ArtifactCompositionLayer(プリコンポ)は mutable/static キャッシュ皆無** — 委譲設計で最もクリーン
- パーティクル/コンポーネントシミュレーションは `goToFrame()` 全再計算方式で**フレーム独立・決定論的**
- 出力バッファ(outputBuffer map)+コンシューマループは既に「並列生成・順序復元」設計済み(`ArtifactRenderQueueService.cppm:6680-6689`)
- farm 経路(RenderFarmMaster+checkpoint 復旧)も useMfr 連動で既に配線済み

### Phase 0(新設): 共有状態の分離 — MFR 有効化の前段

1. **S8 AssetManager 書き込みの監査**: publish/acquire/release の mutex 保護を確認し、不足なら AssetManager 側で保護(レイヤー個別対応より安全)
2. **S9 AsyncAssetReadScheduler**: 同時実行数を MFR concurrency と合算しても飽和しないようキュー容量を見直す
3. **S10 RenderContextRegistry**: map 操作への mutex 追加(小修正)
4. **S15 ログカウンタ**: atomic 化(小修正)
5. **複製完全性の受入テスト**: `cloneCompositionSnapshot() → renderSingleFrame` が逐次結果とピクセル一致することを全レイヤー種別で確認(S1-S7 が正しく複製されることの証明)。※以前の監査で判明済みの「CompositionContext/3Dカメラが toJson で失われる」問題がここで顕在化するため、先に `MILESTONE_COMPOSITION_API_HARDENING` P1 の保存拡張が必要

### 既存フェーズとの整合

- Phase 1(ワーカーごとスナップショット): 上記 Phase 0 完了後に着手可能。`compositionFrameStateMutex_` の撤去条件は「S1-S7 が複製で分離されたこと」+「S8/S10 が保護されたこと」
- Phase 2(MFRDispatcher 統合): 変更なし
- Phase 3(GPU 対応): S11/S12 の解消が本体。DX12 デバイスコンテキスト多重化は AGENTS.md のシビアコード規約に従い慎重に
- Phase 4(適応型)/Phase 5(UI): 変更なし

---

## 現行コード監査 (2026-08-15)

`Core.Render.MFR.Dispatcher` は frame range／step、hardware concurrency、memory limit による同時数制限、依存フレーム時の逐次 fallback、retry、continue-on-error、cancel、progress、elapsed／speedup 集計を実装している。Render Queue 側には MFR 設定フィールドと dispatcher 呼び出し基盤があるが、`ArtifactRenderQueueService` の実レンダー部分には `const bool useMfr = false` が残り、通常経路でのMFR実行は無効化されている。一方、shared composition／GPU resource の並列安全性、実際のファイル書き出し順序、複数レイヤー種別での runtime 性能は未検証である。

## Update 2026-08-15

- `Core.Render.MFR.Dispatcher` の並列数・memory budget・依存フレーム逐次 fallback・retry／cancel／progress 集計と、Render Queue の設定／呼出し基盤を再確認。
- `ArtifactRenderQueueService` の実レンダーループには `useMfr = false` が残っており、通常の実ファイル出力で MFR が有効とは判定できない。
- shared composition／GPU resource の thread-safety、出力順序、複数レイヤー種別の性能、実 MFR と逐次結果の一致は未検証。実装済みなのは dispatcher foundation まで。
- ※2026-08-21: 上記「shared composition の thread-safety 未検証」は冒頭の洗い出し表(S1-S15)で静的に特定済み。残るは Phase 0 の保護実装と runtime 受入。

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
