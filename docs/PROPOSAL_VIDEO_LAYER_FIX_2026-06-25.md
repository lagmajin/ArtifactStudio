# 動画レイヤー プレビュー再生 黒フレーム点滅 修正案

**作成日:** 2026-06-25
**対象レポート:** `REPORT_VIDEO_LAYER_FLICKER_2026-06-25.md`

---

## 修正方針

非同期デコードの遅延によりフレームが描画できない問題を、以下の 3 つのアプローチで解決する。

---

## 修正案 A: draw 時にデコード完了を待つ（同期フォールバック強化）

**影響範囲:** 小（`ArtifactVideoLayer.cppm` のみ）
**リスク:** 低

### 変更内容

`draw()` メソッドで、フレームバッファが空の場合にデコード完了を **短時間待機** してから再取得する。

**ファイル:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`

```cpp
// 現在の draw() (1654-1658)
ArtifactCore::ImageF32x4_RGBA frameBuffer = cachedFrameImageBuffer(timelineFrame);
if (frameBuffer.isEmpty()) {
    frameBuffer = currentFrameImageBuffer();
}
if (frameBuffer.isEmpty()) return;  // ← 黒のまま

// 修正後
ArtifactCore::ImageF32x4_RGBA frameBuffer = cachedFrameImageBuffer(timelineFrame);
if (frameBuffer.isEmpty()) {
    frameBuffer = currentFrameImageBuffer();
}
// デコード中なら短時間待って再取得
if (frameBuffer.isEmpty() && impl_->decoding_.load()) {
    // 最大 16ms（半フレーム分）待機
    for (int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
        frameBuffer = currentFrameImageBuffer();
        if (!frameBuffer.isEmpty()) break;
    }
}
if (frameBuffer.isEmpty()) return;
```

### メリット
- 実装が簡単
- 描画スレッドを長時間ブロックしない（最大 16ms）

### デメリット
- 描画スレッドに CPU ワイトが発生
- 高解像度動画でデコードが 16ms 超の場合は効果なし

---

## 修正案 B: 同期デコードパスのガード緩和

**影響範囲:** 中（`ArtifactVideoLayer.cppm`）
**リスク:** 中

### 変更内容

`decodeFrameToImageBuffer()` の `decoding_` ガードを緩和し、非同期デコード結果を積極的に取り込む。

**ファイル:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`

```cpp
// 現在の decodeFrameToImageBuffer() (1260-1262)
if (impl_->decoding_.load()) {
    impl_->lastDecodeState_ = QStringLiteral("decode-pending");
    return ArtifactCore::ImageF32x4_RGBA();  // ← 空を返す
}

// 修正後
if (impl_->decoding_.load()) {
    // 非同期デコード結果がまだなら空を返すが、
    // 既に currentFrameBuffer_ が更新されていればそれを返す
    std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
    if (impl_->hasCurrentFrameBuffer_) {
        impl_->lastDecodeState_ = QStringLiteral("decode-pending-using-stale");
        return impl_->currentFrameBuffer_;
    }
    impl_->lastDecodeState_ = QStringLiteral("decode-pending");
    return ArtifactCore::ImageF32x4_RGBA();
}
```

### メリット
- 非同期デコード完了直後にバッファが利用可能になる
- 描画スレッドをブロックしない

### デメリット
- 1 フレーム遅れた表示になる可能性がある（stale バッファ）
- ただし黒よりはマシ

---

## 修正案 C: 先読み（prefetch）の強化

**影響範囲:** 中（`ArtifactVideoLayer.cppm`）
**リスク:** 中

### 変更内容

`goToFrame()` の先読みを、現在のフレーム +1 まで広げ、再生速度に依存しないようにする。

**ファイル:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`

```cpp
// 現在の goToFrame() (1706-1722)
// 再生中の先行デコード（Look-ahead prefetch）
if (impl_->playbackSpeed_ > 1.0 && impl_->playbackController_ && needsDecode) {
    const int64_t nextSource = currentSource + 2;
    ...
}

// 修正後
// 再生中の先行デコード（Look-ahead prefetch）
if (impl_->playbackController_ && needsDecode) {
    const int64_t nextSource = currentSource + 1;
    if (nextSource < impl_->streamInfo_.frameCount) {
        // キャッシュにない場合のみプリフェッチ
        if (!impl_->frameCache_.contains(nextSource)) {
            [[maybe_unused]] const auto prefetchFuture = QtConcurrent::run(
                &sharedBackgroundThreadPool(),
                [this, nextSource]() {
                    if (!impl_->playbackController_ || !impl_->isLoaded_) return;
                    const auto rawFrame =
                        impl_->playbackController_->getVideoFrameAtFrameDirectRaw(nextSource);
                    const auto decoded = decodedVideoFrameToImageF32x4_RGBA(rawFrame);
                    if (!decoded.isEmpty()) {
                        impl_->frameCache_.put(nextSource, decoded);
                    }
                });
        }
    }
}
```

### メリット
- 次のフレームが事前キャッシュされる
- 描画時のキーヒット率が向上する

### デメリット
- デコード負荷が 2 倍になる
- メモリ使用量が増加する

---

## 修正案 D: タイミングベースのフレーム保持（推奨）

**影響範囲:** 中（`ArtifactVideoLayer.cppm`）
**リスク:** 低

### 変更内容

デコード未完了時に、**直前の有効フレームを保持** して黒を防ぐ。`currentFrameBuffer_` をクリアしない設計に変更する。

**ファイル:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`

```cpp
// draw() の変更 (1638-1658)
const bool hasFrameBuffer = hasCurrentFrameBuffer();
if (!hasFrameBuffer && !impl_->decoding_.load() && impl_->decodeFuture_.isFinished()) {
    decodeCurrentFrame();
}

ArtifactCore::ImageF32x4_RGBA frameBuffer = cachedFrameImageBuffer(timelineFrame);
if (frameBuffer.isEmpty()) {
    frameBuffer = currentFrameImageBuffer();
}

// 修正: フレームバッファが空でも、直前のフレームがあれば 그것을 使用
if (frameBuffer.isEmpty() && hasFrameBuffer) {
    std::lock_guard<std::mutex> lock(impl_->frameStateMutex_);
    frameBuffer = impl_->currentFrameBuffer_;
}

if (frameBuffer.isEmpty()) return;
```

### メリット
- 黒フレームが完全に消える
- デコード遅延時に「前のフレームを表示」→ 体感上スムーズ
- 実装がシンプル

### デメリット
- 1 フレーム遅れた表示になる（体感上ほぼ気にならない）
- ループ再生時にラストフレーム → 最初フレームのジャンプが見える可能性

---

## 推奨

**修正案 D（タイミングベースのフレーム保持）** を採用する。

理由:
1. 黒フレームを確実に排除できる
2. 実装がシンプルでリスクが低い
3. デコード遅延は「前のフレームを表示」に変換され、体感上ほぼ問題にならない
4. 既存のキャッシュ機構と矛盾しない

必要に応じて **修正案 C（先読み強化）** を組み合わせ、遅延の蓄積を抑制する。

---

## 検証方法

1. 動画レイヤーを配置し、再生ボタンを押す
2. 黒フレームが表示されないことを確認
3. 停止 → 再生の切り替えで正常にフレームが表示されることを確認
4. シーク操作で任意のフレームが正しく表示されることを確認
5. ログで `decodeState` が `decode-pending-using-stale` とならないことを確認（修正案 B 採用時）
