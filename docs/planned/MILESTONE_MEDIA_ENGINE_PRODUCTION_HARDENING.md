# MILESTONE: Media Engine Production Hardening

**日付**: 2026-08-15
**最終更新:** 2026-08-15
**現状**: 動画デコード・シーク・カラーパイプライン・フレームキャッシュ・プロキシが独立して存在するが、いずれも決定的なボトルネックを抱えている。
**目標**: ハードウェアデコード有効化 → フレーム精度シーク → カラーパイプライン直結 → キャッシュ統合 → 非同期プロキシ生成の順に各レイヤーを実用レベルに引き上げる。

## 現状サマリ（5監査の統合）

| 領域 | 現状 | 決定的な穴 |
|------|------|-----------|
| **HWデコード** | Vulkan パスは全コード実装済みだが `directVulkanVideoFramesEnabled() = false` で実質無効 | timeline semaphore sync 不足。全デコードが CPU swscale 経由 |
| **シーク** | `av_seek_frame(AVSEEK_FLAG_BACKWARD)` + 8フレーム以内逐次最適化 | GOP index 不在。最悪ケース（1時間 H.264, GOP=300）で **~2.5秒** |
| **カラーパイプライン** | swscale → CV_8UC3→CV_32FC4→sRGB decode/encode の5コピー | AVColorSpace 完全無視。二重 sRGB 変換。VideoFrameColorInfo 未使用 |
| **フレームキャッシュ** | レイヤー単位 120フレーム LRU + PlaybackService 128フレーム | **Global FrameCache が disabled**。スクラブ方向検知・先読み不在 |
| **プロキシ** | 切り替えロジック・UI・シリアライズ完備 | 同期 QProcess（UIスレッドブロッキング）。外部 ffmpeg バイナリ依存。非同期生成不在。エクスポート時自動フル解像度切替不在 |

## 現行コード照合（2026-08-15）

- **P1 は未完了**。`MediaImageFrameDecoder` に Vulkan HW フレーム選択と安全な CPU ダウンロードの分岐はあるが、`directVulkanVideoFramesEnabled()` は timeline semaphore／Diligent 側の所有権同期不足を理由に `false` のまま。現状は直接 GPU 提示ではない。
- **P2 は未完了**。`FFMpegVideoDecoder` は `av_seek_frame(..., AVSEEK_FLAG_BACKWARD)` 後に逐次デコードする実装で、Keyframe Index は確認できない。
- **P3 は部分実装**。`DecodedVideoFrame` に色空間・レンジ・primaries・transfer のメタデータを保持するが、FFmpeg の `sws_getContext` は固定 RGB24／BILINEAR で、色空間反映と二重変換回避は未完了。
- **P4 は部分実装**。`ArtifactVideoLayer` の小容量 LRU と `ArtifactPlaybackService` の RAM preview／ディスクキャッシュ、世代無効化、キャンセル、再生中 fallback は存在する。一方、Global FrameCache は無効のままで、Encoded Packet Cache とスクラブ方向ベースの先読みは未実装。
- **P5 は未完了**。現行のプロキシ生成は linked FFmpeg encoder／非同期管理／エクスポート時の一時フル解像度切替まで統合されていない。

したがって、このマイルストーンは「動画基盤が存在する」段階から、P1〜P5 の hardening を残す **未完了（P3/P4 の一部のみ実装済み）** と判定する。

## Update 2026-08-15

現行コードと上記の統合監査を再確認した。動画対応は既存のデコード・キャッシュ・proxy 基盤があるものの、直接 GPU 提示、keyframe index、色空間を尊重した変換、global cache／先読み、非同期 proxy 生成のいずれも完了条件を満たす証拠はない。

- `directVulkanVideoFramesEnabled()` が無効のため、Vulkan の実装部品が存在しても GPU 直結完了とは扱わない。
- seek は backward seek 後の逐次デコード、proxy は同期／外部 FFmpeg 依存の経路が残っている。
- 現状判定は **P3/P4 の部分実装のみ、P1/P2/P5 は未完了** を維持する。動画を優先対象へ戻す判断はせず、静止画・連番・シェイプ・合成・3D の基盤安定化後に再評価する。

### フレームの旅路（現状の全コピー）

```
AVFrame (YUV420P)
  → [Copy 1] sws_scale: YUV→RGB24 (8bit, SWS_BILINEAR, 色空間無視)
  → [Copy 2] cv::cvtColor: RGB→BGRA + convertTo CV_32F (0-1正規化, transfer未適用)
  → [Copy 3] setFromCVMat: clone() → ImageF32x4_RGBA (VRAM非存在)
  → [Copy 4] convertSurfacePixels: 盲目的 sRGB decode→linear→encode (二重変換)
  → [Copy 5] CreateTexture: USAGE_IMMUTABLE + GPU driver copy
```

---

## Phase 1: Vulkan ハードウェアデコード有効化

`MediaImageFrameDecoder.cppm` の `directVulkanVideoFramesEnabled()` が返す `false` を解除し、HW decode → GPU texture の直接パスを完成させる。

### 1.1 現状のコード（既に実装済み、無効化されているだけ）

`MediaImageFrameDecoder.cppm:274`:
```cpp
bool MediaImageFrameDecoder::directVulkanVideoFramesEnabled() const {
    return false;  // ← これが問題。コードは全部ある
}
```

既に実装済みのパス:
- `chooseBestDecoderPixelFormat()` — `AV_PIX_FMT_VULKAN` を優先
- `av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN)` — デバイス初期化
- `makeGpuVideoFrameFromFrame()` — Vulkan VkImage ハンドル抽出
- `GPUTextureCacheManager::acquireOrCreate(GpuVideoFrame)` — `CreateTextureFromVulkanImage()` 呼出
- `GpuVideoFrame` → `ITextureView` (SRV) → `drawSpriteTransformed()` の GPU 直接描画パス

### 1.2 Timeline Semaphore 同期の実装

現在 `directVulkanVideoFramesEnabled()` が false を返している理由（line 267-274 のコメント）:
> "Diligent bridge lacks timeline semaphore support"

FFmpeg の Vulkan HW decoder はデコード完了時に timeline semaphore を signal する。Diligent がこの semaphore を wait できれば、デコード→レンダリングの同期が成立する。

**修正内容**: `GPUTextureCacheManager::acquireOrCreate(GpuVideoFrame)` 内で:

```cpp
// GpuVideoFrame 内の VulkanVideoFrameHandle から semaphore を取得
const auto& vkFrame = gpuFrame.vulkanHandle();

// Diligent の IRenderDeviceVk 経由で timeline semaphore wait を発行
auto* deviceVk = static_cast<Diligent::IRenderDeviceVk*>(device_);
deviceVk->AddWaitSemaphore(
    vkFrame.timelineSemaphore,   // VkSemaphore
    vkFrame.timelineValue,       // uint64_t wait value
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
);

// 既存の CreateTextureFromVulkanImage 呼出（既に実装済み）
Diligent::TextureDesc texDesc;
texDesc.Name = "VideoFrame_GPU";
texDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
texDesc.Width = gpuFrame.width;
texDesc.Height = gpuFrame.height;
texDesc.Format = vkFormatToDiligent(vkFrame.format);
texDesc.MipLevels = 1;

Diligent::ITextureVk* textureVk = nullptr;
deviceVk->CreateTextureFromVulkanImage(
    vkFrame.image, vkFrame.imageMemory,
    vkFrame.imageLayout, texDesc, &textureVk
);
```

`IRenderDeviceVk` に `AddWaitSemaphore` 相当の機能がない場合は、DiligentEngine の fork に最小限の追加が必要（または `ICommandQueueVk` 経由）。

**代替案（リスク低）**: timeline semaphore を追加せず、既存の `downloadHwFrameToCpuVideoFrame()` パス（`av_hwframe_transfer_data`）を使いつつ、**転送先を CPU メモリではなく D3D11 ステージングテクスチャにする**パスを追加する。

### 1.3 HW Decode ポリシー選択

MediaPlaybackController にバックエンド選択を追加:

```cpp
enum class DecodeBackend {
    Auto,       // HW優先 → fallback CPU
    CPU,        // 強制CPU（現状のデフォルト）
    Vulkan,     // Vulkan HW decode
    D3D11VA     // 将来
};

void setDecodeBackend(DecodeBackend backend);  // 既存 setDecoderBackend を拡張
DecodeBackend effectiveDecodeBackend() const;   // Auto の解決結果を返す
```

`MediaImageFrameDecoder::directVulkanVideoFramesEnabled()` を backend 設定で動的に切り替え:

```cpp
bool MediaImageFrameDecoder::directVulkanVideoFramesEnabled() const {
    return backendPolicy_ == DecodeBackend::Vulkan ||
           (backendPolicy_ == DecodeBackend::Auto && vulkanDeviceAvailable_);
}
```

### 1.4 完了条件

- [ ] `DecodeBackend::Vulkan` 設定時、4K H.264 動画の `avcodec_receive_frame()` が `AV_PIX_FMT_VULKAN` を返す
- [ ] `GpuVideoFrame` → `ITextureView` の直接パスを通り、`CreateTextureFromVulkanImage` が成功する
- [ ] CPU パスと GPU パスで画質が一致する（ピクセル単位比較）
- [ ] 4K 動画の連続再生でフレームドロップが CPU パス比で **50% 以上削減**
- [ ] `Auto` モードで HW 非対応コーデック（VP8 等）が CPU fallback する

---

## Phase 2: フレーム精度シーク — Keyframe Index

### 2.1 問題

現在のシーク: `av_seek_frame(AVSEEK_FLAG_BACKWARD)` → 直前キーフレーム着地 → 最大 299 フレームをデコード → 目的フレーム。1時間 H.264（GOP=300）で最悪 **2.5秒**。

### 2.2 Keyframe Index の構築

`MediaSource` にキーフレームインデックスを追加:

```cpp
// MediaSource.ixx に追加
struct KeyframeIndex {
    struct Entry {
        int64_t frameNumber;      // フレーム番号
        int64_t pts;              // プレゼンテーションタイムスタンプ
        int64_t bytePosition;     // ファイル内バイト位置
        int64_t dts;              // デコードタイムスタンプ
    };
    
    std::vector<Entry> entries;           // フレーム番号順
    int64_t totalFrames = 0;
    float avgFrameDuration = 0.0f;
    bool complete = false;
    
    // 目的フレームに最も近いキーフレームを二分探索
    Entry nearestKeyframeBefore(int64_t targetFrame) const;
    
    // 目的フレームまでの距離（デコード必要フレーム数）
    int64_t framesToDecode(int64_t targetFrame) const;
};

class MediaSource {
    // ... 既存 ...
    bool buildKeyframeIndex();
    const KeyframeIndex& keyframeIndex() const;
    bool hasKeyframeIndex() const;
    
private:
    std::optional<KeyframeIndex> keyframeIndex_;
};
```

構築アルゴリズム（`MediaSource::buildKeyframeIndex()`）:

```cpp
bool MediaSource::buildKeyframeIndex() {
    KeyframeIndex index;
    
    // 1. avformat_find_stream_info の内部インデックスを活用
    AVStream* stream = formatContext_->streams[videoStreamIndex_];
    
    // 2. ストリーム全体を走査し、キーフレームのみを収集
    AVPacket* pkt = av_packet_alloc();
    while (av_read_frame(formatContext_, pkt) >= 0) {
        if (pkt->stream_index == videoStreamIndex_ && (pkt->flags & AV_PKT_FLAG_KEY)) {
            KeyframeIndex::Entry entry;
            entry.bytePosition = pkt->pos;
            entry.pts = pkt->pts;
            entry.dts = pkt->dts;
            // frameNumber は pts / avgFrameDuration で概算
            index.entries.push_back(entry);
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    
    // 3. 再シークして通常の読み取り位置に戻す
    av_seek_frame(formatContext_, videoStreamIndex_, 0, AVSEEK_FLAG_BACKWARD);
    
    index.complete = true;
    keyframeIndex_ = std::move(index);
    return true;
}
```

### 2.3 Frame-Accurate Seek の実装

`MediaPlaybackController::decodeVideoFrameDirectAtFrameRaw()` の seek パスを keyframe index 使用に変更:

```cpp
// 既存の seek path を置き換え
if (!canContinueSequentially) {
    if (directMediaSource_->hasKeyframeIndex()) {
        // NEW: キーフレームインデックス使用
        const auto& idx = directMediaSource_->keyframeIndex();
        auto nearest = idx.nearestKeyframeBefore(targetFrame);
        
        // キーフレームに直接シーク（AVSEEK_FLAG_BYTE でバイト位置指定）
        av_seek_frame(formatContext, videoStreamIndex, 
                       nearest.bytePosition, AVSEEK_FLAG_BYTE);
        directVideoDecoder_->flush();
        
        // キーフレームから目的フレームまで sequential decode
        int64_t framesToGo = targetFrame - nearest.frameNumber;
        // 最大でも nearest からの距離（通常 60 フレーム未満）
    } else {
        // FALLBACK: 既存の AVSEEK_FLAG_BACKWARD path
        directMediaSource_->seek(targetMs);
        directVideoDecoder_->flush();
    }
}
```

### 2.4 インデックスキャッシュ

構築したキーフレームインデックスを `.kfi` ファイルにキャッシュ:

```cpp
// MediaSource.cppm
static constexpr auto kIndexFileExtension = ".kfi";

bool MediaSource::loadKeyframeIndexFromCache() {
    QString indexPath = sourcePath_ + kIndexFileExtension;
    QFile file(indexPath);
    if (!file.exists()) return false;
    
    // ソースファイルの modification time を比較し、キャッシュが最新か検証
    QFileInfo sourceInfo(sourcePath_);
    QFileInfo indexInfo(indexPath);
    if (indexInfo.lastModified() < sourceInfo.lastModified()) return false;
    
    // バイナリ読み込み
    QDataStream stream(&file);
    KeyframeIndex idx;
    stream >> idx;
    keyframeIndex_ = std::move(idx);
    return true;
}

void MediaSource::saveKeyframeIndexToCache() {
    if (!keyframeIndex_ || !keyframeIndex_->complete) return;
    
    QString indexPath = sourcePath_ + kIndexFileExtension;
    QFile file(indexPath);
    file.open(QIODevice::WriteOnly);
    QDataStream stream(&file);
    stream << *keyframeIndex_;
}
```

### 2.5 完了条件

- [ ] 1時間 H.264（GOP=300）のシーク時間が **2.5秒 → 100ms 未満** に短縮
- [ ] キーフレームインデックス構築がバックグラウンドで行われ、UI をブロックしない（BackgroundTask 使用）
- [ ] `.kfi` キャッシュがソースファイル更新を検出して自動再構築
- [ ] インデックス不在時は既存 seek パスにフォールバック
- [ ] backward seek（逆方向スクラブ）でも frame-accurate なシークが動作

---

## Phase 3: カラーパイプライン直結

### 3.1 問題

- swscale YUV→RGB が AVColorSpace/AVColorPrimaries/AVColorTransferCharacteristic を完全無視
- BT.2020 HDR コンテンツが BT.709 として decode される
- `VideoFrameColorInfo` が死にメタデータ（設定されるが誰も読まない）
- sRGB→linear→sRGB の二重変換
- 5 回のデータコピー

### 3.2 swscale の色空間認識

`makeCpuVideoFrameFromFrame()` の `sws_getContext` 呼出を修正:

```cpp
// 現状（FFmpegVideoDecoder.cppm:196-199）
swsCtx_ = sws_getContext(
    codecContext->width, codecContext->height, codecContext->pix_fmt,
    codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
    SWS_BILINEAR, nullptr, nullptr, nullptr);

// 修正後
int srcColorspace = (frame->colorspace != AVCOL_SPC_UNSPECIFIED)
    ? frame->colorspace : guessColorspace(codecContext->width, codecContext->height);
int srcPrimaries = (frame->color_primaries != AVCOL_PRI_UNSPECIFIED)
    ? frame->color_primaries : AVCOL_PRI_BT709;
int srcTransfer = (frame->color_trc != AVCOL_TRC_UNSPECIFIED)
    ? frame->color_trc : AVCOL_TRC_BT709;

int dstColorspace = AVCOL_SPC_RGB;
int dstPrimaries = AVCOL_PRI_BT709;   // 作業色空間
int dstTransfer = AVCOL_TRC_IEC61966_2_1;  // sRGB

const int* srcCoeffs = sws_getCoefficients(srcColorspace);
const int* dstCoeffs = sws_getCoefficients(dstColorspace);

swsCtx_ = sws_getContext(
    codecContext->width, codecContext->height, codecContext->pix_fmt,
    codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
    SWS_BILINEAR | SWS_ACCURATE_RND,  // SWS_ACCURATE_RND で色精度向上
    nullptr, srcCoeffs, dstCoeffs);
```

`guessColorspace()`:
```cpp
static int guessColorspace(int width, int height) {
    if (width > 1920 || height > 1080) return AVCOL_SPC_BT2020_NCL;
    if (width > 1280 || height > 720) return AVCOL_SPC_BT709;
    return AVCOL_SPC_BT470BG;  // BT.601 for SD
}
```

### 3.3 VideoFrameColorInfo を SurfaceColorDescriptor に反映

`cpuVideoFrameSurfaceDescriptor(RGB24)` で color_primaries / color_trc を渡す:

```cpp
// 現状（ArtifactVideoLayer.cppm:176-198）
SurfaceColorDescriptor cpuVideoFrameSurfaceDescriptor(VideoFramePixelFormat fmt) {
    SurfaceColorDescriptor descriptor;
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    descriptor.alphaMode = SurfaceAlphaMode::Opaque;
    // transferKnown = false → sRGB 盲目的変換の原因
    return descriptor;
}

// 修正後
SurfaceColorDescriptor cpuVideoFrameSurfaceDescriptor(
    VideoFramePixelFormat fmt, const VideoFrameColorInfo& colorInfo) {
    SurfaceColorDescriptor descriptor;
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    descriptor.alphaMode = SurfaceAlphaMode::Opaque;
    
    // color_primaries → SurfaceColorPrimaries
    switch (colorInfo.colorPrimaries) {
    case AVCOL_PRI_BT709:     descriptor.primaries = ColorPrimaries::BT709; break;
    case AVCOL_PRI_BT2020:    descriptor.primaries = ColorPrimaries::BT2020; break;
    case AVCOL_PRI_BT470BG:   descriptor.primaries = ColorPrimaries::BT601_625; break;
    case AVCOL_PRI_SMPTE170M: descriptor.primaries = ColorPrimaries::BT601_525; break;
    default:                  descriptor.primariesKnown = false; break;
    }
    
    // transfer → SurfaceTransferFunction
    switch (colorInfo.colorTransfer) {
    case AVCOL_TRC_BT709:
    case AVCOL_TRC_IEC61966_2_1:
        descriptor.transfer = TransferFunction::SRGB; break;
    case AVCOL_TRC_SMPTE2084:
        descriptor.transfer = TransferFunction::PQ; break;
    case AVCOL_TRC_ARIB_STD_B67:
        descriptor.transfer = TransferFunction::HLG; break;
    case AVCOL_TRC_LINEAR:
        descriptor.transfer = TransferFunction::Linear; break;
    default:
        descriptor.transferKnown = false; break;
    }
    
    return descriptor;
}
```

これにより `convertSurfacePixels` が適切な transfer 関数を適用する（sRGB decode、PQ decode等）。

### 3.4 二重 sRGB 変換の解消

`convertImageForUpload` 側で `SurfaceColorDescriptor` の transfer が既に適用済み（linear）であることを示すフラグを追加:

```cpp
// convertImageForUpload に渡す前に
if (image.colorDescriptor().transferKnown) {
    // 既に linear に decode 済み → convertSurfacePixels は transfer decode をスキップ
    descriptor.transferAlreadyApplied = true;
}
```

### 3.5 コピー回数削減（オプション・次善）

現状 5 コピーを 3 コピーに:

1. swscale → RGB24 → `CpuVideoFrame`（Copy 1: 維持）
2. **CV_8UC3 → 8-bit RGBA 非対称 upload バッファに直接変換**（Copy 2: cv::cvtColor の代わりに pack-to-upload-buffer。8→8bit なので 32-float 中間バッファ不要）
3. `CreateTexture`（Copy 3: GPU driver、これは必須）

これにより `cv::cvtColor + convertTo + convertSurfacePixels` の 3 ステップを 1 ステップに統合。

### 3.6 完了条件

- [ ] BT.709 / BT.2020 / BT.601 のソース動画が正しい色で表示される
- [ ] `SurfaceColorDescriptor` に `transferKnown = true` が伝播し、upload パスで二重 sRGB 変換が行われない
- [ ] HDR（PQ/HLG）動画が正しい transfer function で decode される
- [ ] 色空間付き動画と色空間なし動画の表示が一貫している

---

## Phase 4: 統合フレームキャッシュ + スクラブ先読み

### 4.1 Global FrameCache の再有効化

`ArtifactPlaybackService.cppm:165` で disabled になっている `FrameCache` を再有効化:

```cpp
// ArtifactPlaybackService.cppm 内
// 現状: // std::unique_ptr<FrameCache> frameCache_;  // FrameCache module is disabled
// 変更後:
std::unique_ptr<FrameCache> frameCache_;
```

ただし単純に有効化するだけでは不十分。現行の `ArtifactFrameCache.cppm` は以下の制限がある:
- `ImageF32x4_RGBA` を値として保持（1フレーム最大 ~64MB for 4K float RGBA）
- 512MB budget → 4K で ~8フレームしかキャッシュできない

### 4.2 キャッシュ階層の設計

```
Tier 1: GPU Texture Cache (GPUTextureCacheManager::m_spriteTexCache)
  └─ 既存。Diligent ITexture、content hash キー。VRAM 内。
  └─ 容量: GPU メモリに依存（通常 2-8GB）
  └─ ヒット時: 0 copy → drawSpriteTransformed() 直接

Tier 2: Decoded Frame Pool (Per-layer FrameCache, 120 frames)
  └─ ImageF32x4_RGBA、VRAM 非存在
  └─ ヒット時: ImageF32x4_RGBA → GPU upload (1 copy)
  
Tier 3: Encoded Packet Cache (GOP buffer)
  └─ 新規。デコード済みビットストリームを保持し、再デコードを高速化
  └─ AVPacket をリングバッファで保持（1フレーム ~50-200KB、低メモリ）
  └─ ヒット時: AVPacket → avcodec_send_packet → avcodec_receive_frame (~2ms)
```

Tier 1 は既存。Tier 2 も既存。Tier 3 を追加する。

### 4.3 Encoded Packet Cache（Tier 3）

```cpp
// MediaSource に追加
class PacketCache {
public:
    struct CachedPacket {
        AVPacket* packet;
        int64_t frameNumber;
    };
    
    explicit PacketCache(size_t maxPackets = 600);  // ~10秒分 @60fps
    
    void store(int64_t frameNumber, AVPacket* packet);
    AVPacket* find(int64_t frameNumber) const;
    bool has(int64_t frameNumber) const;
    void clear();
    size_t size() const;
    
private:
    std::deque<CachedPacket> packets_;
    std::unordered_map<int64_t, size_t> frameIndex_;
    size_t maxPackets_;
};
```

シーク時に、キーフレームから目的フレームまでの全パケットを packet cache に蓄積。パケットキャッシュにヒットした場合は demuxer read をスキップして直接 decoder に送る。

### 4.4 スクラブ方向検知 + 先読み

`MediaPlaybackController` にスクラブ方向検知を追加:

```cpp
class ScrubPredictor {
public:
    void recordFrameAccess(int64_t frameNumber, int64_t timestampUs);
    
    enum class Direction { Forward, Backward, Stationary, Unknown };
    Direction currentDirection() const;
    float currentSpeedFps() const;       // スクラブ速度（fps）
    
    // 次にアクセスされる可能性の高いフレームを予測（最大 N フレーム）
    std::vector<int64_t> predictNextFrames(int count) const;
    
private:
    static constexpr int kHistorySize = 8;
    struct AccessRecord {
        int64_t frame;
        int64_t timestampUs;
    };
    std::deque<AccessRecord> history_;
};
```

予測に基づく先読み:

```cpp
void MediaPlaybackController::prefetchPredictedFrames() {
    auto predicted = scrubPredictor_.predictNextFrames(5);
    
    for (int64_t frame : predicted) {
        // Tier 3 (packet cache) に対象パケットがあるか確認
        // なければ demuxer read + cache
        
        // Tier 2 (frame cache) に対象フレームがあるか確認
        // なければ decode + cache（BackgroundTask で非同期実行）
        scheduleBackgroundDecode(frame);
    }
}
```

### 4.5 キャッシュ統合完了条件

- [ ] Global FrameCache が有効化され、4K 動画のスクラブで frame cache ヒット率が **60% 以上**
- [ ] Packet Cache (Tier 3) が有効で、キーフレームからの再デコード時間が **半分以下** に短縮
- [ ] スクラブ方向検知が正しく動作し、先読みフレームの **80% 以上** が実際にアクセスされる
- [ ] メモリ使用量が設定された budget 内に収まる

---

## Phase 5: 非同期プロキシ生成 + エクスポート時自動切替

### 5.1 同期 QProcess → Linked FFmpeg Libs

`ArtifactProxyManager::generateProxy()` を外部 `ffmpeg` バイナリから linked `libavcodec`/`libavformat`/`libswscale` に置き換える:

```cpp
// 新規: ArtifactCore/src/Media/FFmpegProxyEncoder.cppm
class FFmpegProxyEncoder {
public:
    struct Config {
        QString sourcePath;
        QString outputPath;
        float scaleFactor;          // 0.25, 0.5, 1.0
        QString codec = "libx264";
        int crf = 23;
        QString preset = "fast";
    };

    bool encode(const Config& config);

private:
    bool openInput(const QString& path);
    bool setupOutput(const Config& config);
    bool transcodeLoop();

    AVFormatContext* inputCtx_  = nullptr;
    AVFormatContext* outputCtx_ = nullptr;
    AVCodecContext*  decoderCtx_ = nullptr;
    AVCodecContext*  encoderCtx_ = nullptr;
    SwsContext*      scalerCtx_  = nullptr;
    bool             cancelled_  = false;
};
```

`transcodeLoop()` 内:
```cpp
while (av_read_frame(inputCtx_, packet) >= 0 && !cancelled_) {
    avcodec_send_packet(decoderCtx_, packet);
    while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
        // リサイズ
        sws_scale_frame(scalerCtx_, scaledFrame, frame);
        // エンコード
        avcodec_send_frame(encoderCtx_, scaledFrame);
        while (avcodec_receive_packet(encoderCtx_, encPkt) >= 0) {
            av_interleaved_write_frame(outputCtx_, encPkt);
        }
    }
}
av_write_trailer(outputCtx_);
```

**利点**: 外部バイナリ不要、進捗コールバック可能、キャンセル可能。

### 5.2 BackgroundTask 統合

```cpp
// ArtifactProxyManager に非同期生成を追加
void ArtifactProxyManager::generateProxyAsync(
    const QString& sourcePath,
    ProxyServiceQuality quality,
    std::function<void(bool success, const QString& proxyPath)> onComplete)
{
    auto* task = new BackgroundTypedTask<ProxyGenerationResult>(
        [this, sourcePath, quality]() -> ProxyGenerationResult {
            FFmpegProxyEncoder encoder;
            ProxyGenerationResult result;
            result.success = encoder.encode({
                .sourcePath = sourcePath,
                .outputPath = proxyFilePath(sourcePath, quality),
                .scaleFactor = scaleFactor(quality)
            });
            result.outputPath = proxyFilePath(sourcePath, quality);
            return result;
        }
    );
    
    QObject::connect(task, &BackgroundTypedTask<ProxyGenerationResult>::finished,
        [onComplete](const ProxyGenerationResult& result) {
            onComplete(result.success, result.outputPath);
        });
    
    BackgroundWorkerPool::instance()->enqueue(task);
}
```

### 5.3 エクスポート時自動フル解像度切替

`ArtifactRenderQueueService` のエクスポート処理開始時に、全 VideoLayer の proxy quality を一時的に `Full` に設定:

```cpp
// RenderQueueService の export job 開始時
struct ProxyOverride {
    ArtifactVideoLayer* layer;
    ProxyQuality originalQuality;
};

std::vector<ProxyOverride> proxyOverrides_;
for (auto* layer : composition->videoLayers()) {
    if (layer->proxyQuality() != ProxyQuality::Full &&
        layer->proxyQuality() != ProxyQuality::None) {
        proxyOverrides_.push_back({layer, layer->proxyQuality()});
        layer->setProxyQuality(ProxyQuality::Full);  // 強制フル解像度
    }
}

// エクスポート完了後に復元
for (auto& override : proxyOverrides_) {
    override.layer->setProxyQuality(override.originalQuality);
}
```

### 5.4 プロキシ管理 UI 改善

- **プロキシ生成進捗バー**: バックグラウンドタスクの進捗を表示
- **プロキシキャッシュサイズ管理**: 総サイズ・アイテム数の表示、手動クリーンアップ
- **プロキシ自動生成オプション**: インポート時に自動生成（設定で無効化可能）

### 5.5 完了条件

- [ ] プロキシ生成が UI をブロックせずバックグラウンドで完了する
- [ ] プロキシ生成の進捗が表示され、キャンセル可能
- [ ] 外部 `ffmpeg` バイナリ不要（linked libs のみで動作）
- [ ] エクスポート時に自動でフル解像度に切り替わる
- [ ] プロキシストレージの総サイズが表示・管理可能

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm` | `directVulkanVideoFramesEnabled()` 解除、timeline semaphore wait |
| P1 | `ArtifactCore/src/Graphics/GPUTextureCacheManager.cppm` | Vulkan semaphore wait 追加 |
| P1 | `Artifact/src/Playback/ArtifactPlaybackController.cppm` | `DecodeBackend` 拡張 |
| P2 | `ArtifactCore/include/Media/MediaSource.ixx` | `KeyframeIndex` 構造体、`buildKeyframeIndex()` |
| P2 | `ArtifactCore/src/Media/MediaSource.cppm` | インデックス構築・キャッシュ実装 |
| P2 | `Artifact/src/Playback/ArtifactPlaybackController.cppm` | seek 最適化 |
| P3 | `ArtifactCore/src/Codec/FFmpegVideoDecoder.cppm` | `sws_getContext` 色空間引数 |
| P3 | `Artifact/src/Layer/ArtifactVideoLayer.cppm` | `cpuVideoFrameSurfaceDescriptor()` に colorInfo 反映 |
| P3 | `ArtifactCore/src/Image/SurfacePixelConversion.cppm` | 二重 sRGB 変換防止 |
| P4 | `Artifact/src/Playback/ArtifactPlaybackService.cppm` | Global FrameCache 再有効化 |
| P4 | `ArtifactCore/include/Media/PacketCache.ixx` | 新規: Encoded Packet Cache |
| P4 | `Artifact/src/Playback/ArtifactPlaybackController.cppm` | ScrubPredictor、先読み |
| P5 | 新規 `ArtifactCore/src/Media/FFmpegProxyEncoder.cppm` | Linked libs プロキシエンコーダ |
| P5 | `Artifact/src/Proxy/ProxyService.cppm` | 非同期生成 + BackgroundTask |
| P5 | `Artifact/src/Render/ArtifactRenderQueueService.cppm` | エクスポート時 proxy override |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P2: Keyframe Index | **P0** | 中 | 最大の即効性。2.5秒→100ms。純粋追加でリスク最小 |
| P5: Async Proxy | **P0** | 中 | 同期 QProcess のブロッキングを解消。外部バイナリ依存排除 |
| P1: HW Decode 有効化 | **P0** | 大 | パフォーマンス最大の改善余地。ただし timeline semaphore は要検討 |
| P3: カラーパイプライン | **P1** | 中 | BT.2020/PQ の正しい表示。いずれ必須 |
| P4: キャッシュ統合 | **P1** | 大 | キャッシュ階層全体の再設計。P1+P2+P3 の後に着手 |
