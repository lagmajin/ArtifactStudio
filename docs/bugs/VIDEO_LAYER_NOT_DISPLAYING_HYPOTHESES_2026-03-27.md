# ArtifactVideoLayer 表示されない問題 仮説レポート (2026-03-27)

**作成日:** 2026-03-27  
**ステータス:** 調査中  
**関連コンポーネント:** ArtifactVideoLayer, MediaPlaybackController, CompositionRenderController

---

## 現象

ArtifactVideoLayer がコンポジションビューに表示されない。

---

## 慎重に立てる仮説（7 つ）

### ★★★ 仮説 1: `isLoaded_` フラグが `false` のまま

**場所:** `ArtifactVideoLayer.cppm:264, 297, 406`

**根拠:**
```cpp
// loadFromPath() で openMediaFile() が失敗すると false
if (!impl_->playbackController_->openMediaFile(normalizedPath)) {
    impl_->isLoaded_ = false;  // ← ここ
    return false;
}

// decodeCurrentFrame() は isLoaded_ = false なら早期リターン
if (!impl_->isLoaded_) {
    return;  // ← デコードしない
}

// draw() も isLoaded_ = false なら早期リターン
if (!impl_->videoEnabled_ || !impl_->isLoaded_) return;  // ← 描画しない
```

**確認すべき点:**
- `loadFromPath()` の戻り値をチェックしているか
- `isLoaded()` の値をログ出力
- `openMediaFile()` が実際に成功しているか

**発生確率:** **高い**

---

### ★★★ 仮説 2: 非同期デコードが完了していない

**場所:** `ArtifactVideoLayer.cppm:305-315, 420-450`

**根拠:**
```cpp
// loadFromPath() は非同期デコードを起動するだけ
impl_->decodeFuture_ = QtConcurrent::run([ctrl, this]() -> QImage {
    QImage frame = ctrl->getVideoFrameAtFrameDirect(0);
    impl_->decoding_ = false;
    return frame;  // ← null が返る可能性
});

// draw() で完了チェックがある
if (!impl_->decoding_.load() && impl_->decodeFuture_.isFinished()
    && impl_->decodeTargetFrame_ >= 0) {
    QImage loaded = impl_->decodeFuture_.result();
    if (!loaded.isNull()) {
        impl_->currentQImage_ = loaded;  // ← null なら代入されない
    }
}

// currentQImage_ が null なら描画されない
if (impl_->currentQImage_.isNull()) return;
```

**問題:**
- 初回デコードが失敗すると `currentQImage_` は null のまま
- 2 回目のデコード試行がない

**確認すべき点:**
- `decodeFuture_.isFinished()` の状態
- デコード結果のフレームサイズ
- `decoding_` フラグが降りているか

**発生確率:** **高い**

---

### ★★★ 仮説 3: `getVideoFrameAtFrameDirect()` が null を返す

**場所:** `MediaPlaybackController.cppm:730-735`

**根拠:**
```cpp
QImage MediaPlaybackController::getVideoFrameAtFrameDirect(int64_t frameNumber) {
    if (!impl_) {
        return QImage();  // ← null
    }
    return backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
}
```

**さらに辿ると:**
```cpp
// FFmpegPlaybackBackend::getVideoFrameAtFrameDirect()
QImage FFmpegPlaybackBackend::getVideoFrameAtFrameDirect(Impl& impl, int64_t frameNumber) {
    return impl.decodeVideoFrameDirectAtFrame(frameNumber);  // ← ここ
}

// decodeVideoFrameDirectAtFrame() で失敗する可能性
// - seek 失敗
// - デコーダー初期化失敗
// - フレーム読み取り失敗
```

**確認すべき点:**
- `impl_` が null でないか
- バックエンドはどちらか（MediaFoundation / FFmpeg）
- デバッグログにエラーが出ていないか

**発生確率:** **中〜高い**

---

### ★★ 仮説 4: フレーム範囲外と判定されている

**場所:** `ArtifactVideoLayer.cppm:433-440`

**根拠:**
```cpp
if (targetFrame < inPoint() || targetFrame >= outPoint() ||
    sourceFrame < 0 ||
    (impl_->streamInfo_.frameCount > 0 && sourceFrame >= impl_->streamInfo_.frameCount)) {
    impl_->currentQImage_ = QImage();  // ← null に
    impl_->lastDecodedFrame_ = targetFrame;
    return;
}
```

**問題:**
- `inPoint()` / `outPoint()` が正しく設定されていない
- `sourceFrame` の計算が間違っている
- `streamInfo_.frameCount` が 0 または不正な値

**確認すべき点:**
- `inPoint()` と `outPoint()` の値
- 現在のフレーム番号
- `streamInfo_.frameCount` の値

**発生確率:** **中**

---

### ★★ 仮説 5: `videoEnabled_` が `false`

**場所:** `ArtifactVideoLayer.cppm:749`

**根拠:**
```cpp
void ArtifactVideoLayer::draw(ArtifactIRenderer* renderer)
{
    if (!impl_->videoEnabled_ || !impl_->isLoaded_) return;  // ← 早期リターン
    // ...
}
```

**問題:**
- デフォルトで `videoEnabled_ = true` になっているか
- `setHasVideo(false)` が意図せず呼ばれていないか

**確認すべき点:**
- `hasVideo()` の戻り値
- コンストラクタでの初期値

**発生確率:** **低〜中**

---

### ★ 仮説 6: CompositionRenderController 側の描画経路の問題

**場所:** `ArtifactCompositionRenderController.cppm:537-559`

**根拠:**
```cpp
if (const auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
    const QImage frame = videoLayer->currentFrameToQImage();
    // ...
    if (!frame.isNull()) {
        applySurfaceAndDraw(frame, localRect, hasRasterizerEffectsOrMasks(layer));
        return;
    }
    // null の場合は fallback へ
}

// Fallback
layer->draw(renderer);  // ← VideoLayer::draw() が呼ばれる
```

**問題:**
- `applySurfaceAndDraw()` 内部で失敗している可能性
- `localRect` が不正な値

**確認すべき点:**
- `videoDebugOut` のログ出力
- `localRect` の値

**発生確率:** **低**

---

### ★ 仮説 7: メモリ/パフォーマンス問題でデコードがスキップされている

**場所:** `ArtifactVideoLayer.cppm:417-425`

**根拠:**
```cpp
// デコード中はスキップ
if (impl_->decoding_.load()) {
    if (impl_->decodeFuture_.isFinished() && impl_->decodeTargetFrame_ == targetFrame) {
        // 結果を取り込むが、null ならそのまま
        QImage loaded = impl_->decodeFuture_.result();
        if (!loaded.isNull()) {
            impl_->currentQImage_ = loaded;
        }
    }
    return;  // ← デコード中であれば何もしない
}
```

**問題:**
- デコードが常に走っている状態
- 結果が null でも再試行しない

**発生確率:** **低**

---

## デバッグ手順

### 手順 1: ログ確認

```cpp
// VideoLayer のログを有効化
qCDebug(videoLayerLog) << "[VideoLayer] isLoaded=" << impl_->isLoaded_;
qCDebug(videoLayerLog) << "[VideoLayer] videoEnabled=" << impl_->videoEnabled_;
qCDebug(videoLayerLog) << "[VideoLayer] currentQImage_.isNull=" << impl_->currentQImage_.isNull();
qCDebug(videoLayerLog) << "[VideoLayer] decoding=" << impl_->decoding_.load();
```

### 手順 2: loadFromPath の戻り値確認

```cpp
bool loaded = videoLayer->loadFromPath(path);
qDebug() << "loadFromPath returned:" << loaded;
qDebug() << "isLoaded():" << videoLayer->isLoaded();
```

### 手順 3: デコード結果を確認

```cpp
// デコード完了を待つ
if (impl_->decodeFuture_.isFinished()) {
    QImage result = impl_->decodeFuture_.result();
    qDebug() << "Decoded frame size:" << result.size();
    qDebug() << "Decoded frame isNull:" << result.isNull();
}
```

### 手順 4: CompositionRenderController のデバッグログ

```cpp
// drawLayerForCompositionView() の videoDebugOut を確認
QString dbgOut;
drawLayerForCompositionView(layer, renderer, 1.0f, &dbgOut, ...);
if (!dbgOut.isEmpty()) {
    qDebug() << "[VideoDebug]" << dbgOut;
}
```

---

## 修正候補

### 修正候補 1: 再試行機構の追加

```cpp
// currentQImage_ が null の場合、再デコードを試みる
if (impl_->currentQImage_.isNull() && !impl_->decoding_.load()) {
    if (impl_->lastDecodedFrame_ < 0 || 
        impl_->lastDecodedFrame_ != targetFrame) {
        // 前回のデコードが失敗しているので再試行
        decodeCurrentFrame();
    }
}
```

### 修正候補 2: デバッグ出力の強化

```cpp
// draw() の冒頭に詳細ログ
qCDebug(videoLayerLog) << "[VideoLayer::draw]"
    << "isLoaded=" << impl_->isLoaded_
    << "videoEnabled=" << impl_->videoEnabled_
    << "currentQImage_.isNull=" << impl_->currentQImage_.isNull()
    << "decoding=" << impl_->decoding_.load()
    << "targetFrame=" << currentFrame()
    << "inPoint=" << inPoint()
    << "outPoint=" << outPoint();
```

### 修正候補 3: フォールバックの改善

```cpp
// getVideoFrameAtFrameDirect() が null を返した場合の代替処理
QImage decoded = ctrl->getVideoFrameAtFrameDirect(sourceFrame);
if (decoded.isNull()) {
    // 代替：現在のフレームを再度試行
    decoded = ctrl->getVideoFrameAtFrameDirect(0);
    qWarning() << "[VideoLayer] Fallback decode attempted";
}
```

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---------|-----|------|
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 256-320 | `loadFromPath()` |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 400-470 | `decodeCurrentFrame()` |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 746-780 | `draw()` |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 537-559 | VideoLayer 描画 |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 730-735 | `getVideoFrameAtFrameDirect()` |

---

## 次のアクション

1. **ログ出力を追加** - 各状態を詳細に記録
2. **戻り値チェック** - `loadFromPath()` の結果を確認
3. **デコード結果の確認** - フレームサイズと null チェック
4. **バックエンドの特定** - MediaFoundation / FFmpeg のどちらか

---

**文書終了**
