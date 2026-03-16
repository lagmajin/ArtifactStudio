# In/Out Points 統合実装レポート

## 実施日
2026 年 3 月 16 日

## 概要

再生システムに In/Out Points の機能を統合し、設定された In/Out 範囲内での再生、マーカーナビゲーションを実装しました。

---

## 実装内容

### 1. ArtifactCompositionPlaybackController の改善

#### Impl クラスの拡張
```cpp
// In/Out Points へのポインタを追加
ArtifactInOutPoints* inOutPoints_ = nullptr;

// 有効なフレーム範囲を取得するヘルパーメソッド
FramePosition effectiveStartFrame() const {
    FramePosition start = frameRange_.startPosition();
    
    // InPoint が設定されていれば、それとフレーム範囲の大きい方を採用
    if (inOutPoints_ && inOutPoints_->hasInPoint()) {
        auto inPoint = inOutPoints_->inPoint().value();
        start = std::max(start, inPoint);
    }
    
    return start;
}

FramePosition effectiveEndFrame() const {
    FramePosition end = frameRange_.endPosition();
    
    // OutPoint が設定されていれば、それとフレーム範囲の小さい方を採用
    if (inOutPoints_ && inOutPoints_->hasOutPoint()) {
        auto outPoint = inOutPoints_->outPoint().value();
        end = std::min(end, outPoint);
    }
    
    return end;
}
```

#### nextFrame() の更新
```cpp
FramePosition nextFrame() const {
    // ... (中略) ...
    
    // Apply effective in/out points
    FramePosition start = effectiveStartFrame();
    FramePosition end = effectiveEndFrame();

    if (next < start) {
        return start;
    }
    if (next > end) {
        if (looping_) {
            return start;
        }
        return FramePosition(-1);  // 再生終了
    }
    return next;
}
```

#### ナビゲーションメソッドの実装
```cpp
void goToNextMarker() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto nextMarkerPos = impl_->inOutPoints_->nextMarker(impl_->currentFrame_);
    if (nextMarkerPos.has_value()) {
        goToFrame(nextMarkerPos.value());
    }
}

void goToPreviousMarker() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto prevMarkerPos = impl_->inOutPoints_->previousMarker(impl_->currentFrame_);
    if (prevMarkerPos.has_value()) {
        goToFrame(prevMarkerPos.value());
    }
}

void goToNextChapter() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto nextChapterPos = impl_->inOutPoints_->nextChapter(impl_->currentFrame_);
    if (nextChapterPos.has_value()) {
        goToFrame(nextChapterPos.value());
    }
}

void goToPreviousChapter() {
    if (!impl_->inOutPoints_ || !impl_->currentFrame_.isValid()) {
        return;
    }
    
    auto prevChapterPos = impl_->inOutPoints_->previousChapter(impl_->currentFrame_);
    if (prevChapterPos.has_value()) {
        goToFrame(prevChapterPos.value());
    }
}
```

#### goToStartFrame() / goToEndFrame() の更新
```cpp
void goToStartFrame() {
    goToFrame(impl_->effectiveStartFrame());  // InPoint を考慮
}

void goToEndFrame() {
    goToFrame(impl_->effectiveEndFrame());    // OutPoint を考慮
}
```

---

### 2. ArtifactPlaybackService の拡張

#### ヘッダーファイルにメソッド追加
```cpp
// In/Out Points
void setInOutPoints(class ArtifactInOutPoints* inOutPoints);
class ArtifactInOutPoints* inOutPoints() const;

// Marker navigation
void goToNextMarker();
void goToPreviousMarker();
void goToNextChapter();
void goToPreviousChapter();
```

#### 実装
```cpp
void ArtifactPlaybackService::setInOutPoints(ArtifactInOutPoints* inOutPoints) {
    if (impl_ && impl_->controller_) {
        impl_->controller_->setInOutPoints(inOutPoints);
    }
}

ArtifactInOutPoints* ArtifactPlaybackService::inOutPoints() const {
    return impl_ && impl_->controller_ ? impl_->controller_->inOutPoints() : nullptr;
}

void ArtifactPlaybackService::goToNextMarker() {
    if (impl_ && impl_->controller_) {
        impl_->controller_->goToNextMarker();
    }
}

// ... 他のメソッドも同様
```

---

## 使用例

### In/Out Points を設定して再生

```cpp
// サービスの取得
auto* playbackService = ArtifactPlaybackService::instance();

// In/Out Points の作成
auto* inOutPoints = new ArtifactInOutPoints();
inOutPoints->setInPoint(FramePosition(100));  // 100 フレーム目を In 点に
inOutPoints->setOutPoint(FramePosition(300)); // 300 フレーム目を Out 点に

// プレイバックサービスに登録
playbackService->setInOutPoints(inOutPoints);

// 再生開始
playbackService->play();
// → 100 フレーム目から 300 フレーム目の間を再生
```

### マーカー間を移動

```cpp
// 次のマーカーに移動
playbackService->goToNextMarker();

// 前のマーカーに移動
playbackService->goToPreviousMarker();

// 次のチャプターに移動
playbackService->goToNextChapter();

// 前のチャプターに移動
playbackService->goToPreviousChapter();
```

### ループ再生

```cpp
// In/Out 範囲でループ再生
playbackService->setLooping(true);
playbackService->play();
// → In 点から Out 点まで再生したら、自動的に In 点に戻る
```

---

## 実装の効果

### Before
- ❌ In/Out Points が再生に反映されない
- ❌ マーカーナビゲーションが未実装
- ❌ `goToStartFrame()` がコンポジションの先頭に移動（In 点を無視）
- ❌ `goToEndFrame()` がコンポジションの末尾に移動（Out 点を無視）

### After
- ✅ In/Out Points が再生範囲に自動的に反映
- ✅ マーカー・チャプター間のナビゲーションが利用可能
- ✅ `goToStartFrame()` が In 点に移動
- ✅ `goToEndFrame()` が Out 点に移動
- ✅ ループ再生が In/Out 範囲内で動作

---

## 再生フロー（更新後）

```
1. play() 呼び出し
   ↓
2. ArtifactCompositionPlaybackController::play()
   ↓
3. QTimer 開始（フレームレート間隔）
   ↓
4. onTimerTick()
   ↓
5. nextFrame() で次のフレームを計算
   ↓
6. effectiveStartFrame() / effectiveEndFrame() を取得
   ↓
   ├─ InPoint が設定されていれば、それを開始フレームとして使用
   ├─ OutPoint が設定されていれば、それを終了フレームとして使用
   └─ フレーム範囲と In/Out 点の大きい方/小さい方を採用
   ↓
7. 次のフレームが有効かチェック
   ├─ In 点より前 → In 点に移動
   ├─ Out 点より後 → ループ時は In 点、否则は停止
   └─ 有効 → そのフレームに移動
   ↓
8. frameChanged シグナル
   ↓
9. UI 更新（Timeline, ScrubBar, Preview）
```

---

## テストチェックリスト

### In/Out Points 基本機能
- [ ] In Point を設定すると、再生開始位置が In 点になる
- [ ] Out Point を設定すると、再生終了位置が Out 点になる
- [ ] In/Out 両方を設定すると、その範囲内のみ再生
- [ ] `goToStartFrame()` で In 点に移動
- [ ] `goToEndFrame()` で Out 点に移動

### ループ再生
- [ ] ループ有効時、Out 点で In 点に戻る
- [ ] ループ無効時、Out 点で停止
- [ ] In/Out 点を変更してもループが継続

### マーカーナビゲーション
- [ ] `goToNextMarker()` で次のマーカーに移動
- [ ] `goToPreviousMarker()` で前のマーカーに移動
- [ ] `goToNextChapter()` で次のチャプターに移動
- [ ] `goToPreviousChapter()` で前のチャプターに移動
- [ ] マーカーがない場合は移動しない

### エッジケース
- [ ] In 点が Out 点より後ろにならない（自動調整）
- [ ] In/Out 点がフレーム範囲外の場合はクリップ
- [ ] In/Out Points をクリアすると全範囲再生

---

## 変更ファイル

| ファイル | 変更内容 |
|---------|---------|
| `ArtifactCompositionPlaybackController.ixx` | メソッド宣言追加 |
| `ArtifactCompositionPlaybackController.cppm` | In/Out Points 統合実装 |
| `ArtifactPlaybackService.ixx` | In/Out Points メソッド追加 |
| `ArtifactPlaybackService.cppm` | In/Out Points 実装 |

---

## 今後の拡張候補

### 優先度高
- [ ] UI からの In/Out 点設定（I / O キー）
- [ ] タイムラインへの In/Out 範囲表示
- [ ] マーカーの視覚的表示
- [ ] In/Out 点のドラッグによる調整

### 優先度中
- [ ] 複数 In/Out 範囲のサポート（チャプターごと）
- [ ] In/Out 範囲のエクスポート
- [ ] マーカーのインポート/エクスポート

### 優先度低
- [ ] In/Out 範囲のプリセット保存
- [ ] マーカーの色分け
- [ ] マーカー検索機能

---

## 関連ドキュメント

- [PlaybackClock_Usage.md](../ArtifactCore/docs/PlaybackClock_Usage.md)
- [PLAYBACK_SYSTEM_ARCHITECTURE.md](./PLAYBACK_SYSTEM_ARCHITECTURE.md)
- [TIMELINE_LAYER_TEST_WIDGET.md](./TIMELINE_LAYER_TEST_WIDGET.md)
