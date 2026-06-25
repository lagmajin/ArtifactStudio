# 動画レイヤー プレビュー再生 黒フレーム点滅調査レポート

**調査日:** 2026-06-25
**現象:** 動画レイヤーのプレビュー再生で、黒と最初のフレームが交互に点滅する

---

## 1. 現象

- 再生時に動画レイヤーが「黒 → 最初のフレーム → 黒 → 最初のフレーム …」と点滅する
- 停止時は正常にフレームが表示される
- 2 フレーム目以降が表示されない

---

## 2. 原因

**フレームデコードが非同期** で、再生ティック（33ms）内にデコードが追不上ないため、1 フレームごとに「フレームあり / フレームなし」が交互に発生する。

### 2.1 再生フローの概要

```
TransportBarWidget (33ms QTimer)
  → setCurrentFrame(frame + 1)
  → FrameChangedEvent 発行
  → CompositionRenderController::renderOneFrameImpl()
    → layer->goToFrame(currentFrame)          // フレーム番号を即更新 + 非同期デコード起動
    → drawLayerForCompositionView()           // すぐ描画を試みる
      → isFrameCached() → false（デコード未完了）
      → GPU テクスチャパス → 失敗
      → sync フォールバック → decoding_ が true で空を返す
      → 黒フレーム
```

### 2.2 具体的なコードパス

#### goToFrame() で非同期デコード起動 (`ArtifactVideoLayer.cppm:1692-1704`)

```cpp
void ArtifactVideoLayer::goToFrame(int64_t frameNumber) {
    impl_->currentTimelineFrame_ = frameNumber;       // 即更新
    impl_->currentSourceFrame_ = timelineFrameToSourceFrame(this, frameNumber);
    if (needsDecode) {
        decodeCurrentFrame();                          // 非同期でバックグラウンドスレッドに投げる
    }
}
```

#### decodeCurrentFrame() でバックグラウンドデコード開始 (`ArtifactVideoLayer.cppm:1053-1061`)

```cpp
impl_->decoding_ = true;
impl_->decodeFuture_ = QtConcurrent::run(
    &sharedBackgroundThreadPool(),
    [ctrl, sourceFrame, ...]() -> ImageF32x4_RGBA {
        const DecodedVideoFrame rawDecoded =
            ctrl->getVideoFrameAtFrameDirectRaw(sourceFrame);  // 重い処理
        ...
    });
```

#### drawLayerForCompositionView() でデコード完了を待たずに描画 (`ArtifactCompositionRenderController.cppm:2914-2960`)

```cpp
// isFrameCached() → false（まだデコード中）
if (!hasRasterizer && currentFrameReady) { ... return; }  // ← 通過しない

// GPU テクスチャパスも失敗
// sync フォールバック
frameBuffer = videoLayer->decodeFrameToImageBuffer(layer->currentFrame());
```

#### decodeFrameToImageBuffer() で decoding_ ガードにより空を返す (`ArtifactVideoLayer.cppm:1260-1262`)

```cpp
if (impl_->decoding_.load()) {
    impl_->lastDecodeState_ = QStringLiteral("decode-pending");
    return ArtifactCore::ImageF32x4_RGBA();  // ← 空を返す → 黒
}
```

### 2.3 ボトルネック: getVideoFrameAtFrameDirectRaw()

`MediaPlaybackController.cppm:988-1054` が **毎フレーム seek + flush + デコード** を行う:

```cpp
std::lock_guard<std::mutex> lock(impl_->directDecodeMutex_);
const int64_t targetMs = static_cast<int64_t>((frameNumber / impl_->fps_) * 1000.0);
impl_->prepareFfmpegSeekReset(targetMs, false);  // MediaReader 停止 + デコーダー flush
// → packets を最大 512 個読んでデコード
impl_->restoreFfmpegReaderState(wasPlaying);     // MediaReader 再開
```

高解像度動画では 1 フレームのデコードが 33ms を超えることがあり、常に 1 フレーム以上遅れる。

### 2.4 cancelPendingDecode() の問題 (`ArtifactVideoLayer.cppm:487-496`)

```cpp
void cancelPendingDecode() {
    decodeGeneration_.fetch_add(1, std::memory_order_acq_rel);
    decoding_ = false;
    decodeTargetFrame_ = -1;
    decodeRetryPending_ = false;
    decodeFuture_ = QFuture<ImageF32x4_RGBA>();  // フューチャーをリセット
}
```

- キャンセル時に `currentFrameBuffer_` をクリアしない
- 但是: `decodeGeneration_` ガードにより、キャンセルされたデコード結果は破棄される
- 結果: キャンセル後に `currentFrameBuffer_` が stale になるか、未設定のままになる

---

## 3. 点滅パターンの分析

| ティック | 状態 | 表示 |
|---------|------|------|
| N | フレーム 0 のデコード未完了 | **黒** |
| N+1 | フレーム 0 デコード完了、キューにヒット | **最初のフレーム** |
| N+2 | フレーム 1 のデコード未完了 | **黒** |
| N+3 | フレーム 1 デコード完了（遅延） | **古いフレーム** |
| ... | 同様に遅延が蓄積 | 黒と古いフレームの点滅 |

「最初のフレームだけ」が表示される理由:
- フレーム 0 は `loadFromPath()` 時にデコード・キューイングされる
- それ以降のフレームは再生開始後の非同期デコードに依存し、追いつかない

---

## 4. 関連ファイル

| ファイル | 役割 |
|---------|------|
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 動画レイヤー本体。デコード管理、フレームキャッシュ、描画 |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | FFmpeg ベースのデコード。seek + flush + packet 読み込み |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | コンポジション描画ループ。レイヤーごとの draw 呼び出し |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | GPU テクスチャアップロード、スプライト描画 |
| `ArtifactPr/src/TransportBarWidget.cppm` | 再生タイマー（33ms tick） |

---

## 5. 修正案

詳細は別ファイル (`PROPOSAL_VIDEO_LAYER_FIX_2026-06-25.md`) を参照。
