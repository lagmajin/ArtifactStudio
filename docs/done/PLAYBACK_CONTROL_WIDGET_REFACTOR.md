# 再生コントロールウィジェット リファクタリングレポート

## 実施日
2026 年 3 月 16 日

## 概要

`ArtifactPlaybackControlWidget` を全面的にリファクタリングし、新しい `ArtifactPlaybackEngine` と完全に統合しました。

---

## 変更内容

### 1. ウィジェット構成の再設計

**新規ウィジェット:**

| ウィジェット | 説明 |
|------------|------|
| `ArtifactPlaybackControlWidget` | メインの再生操作 UI |
| `ArtifactPlaybackInfoWidget` | フレーム・タイムコード表示 |
| `ArtifactPlaybackSpeedWidget` | 再生速度コントロール |

---

### 2. ArtifactPlaybackControlWidget

#### UI 構成

```
┌─────────────────────────────────────────────────────────────────┐
│  [▶] [⏸] [⏹]  │  [⏮] [◀] [▶] [⏭]  │  [<<] [>>]  │  [🔁] [I] [O] [×] │
│   再生制御       シーク操作         フレーム操作       オプション       │
└─────────────────────────────────────────────────────────────────┘
```

#### ボタン一覧

| ボタン | ショートカット | 機能 |
|-------|--------------|------|
| ▶ Play | `Space` | 再生開始 |
| ⏸ Pause | - | 一時停止 |
| ⏹ Stop | - | 停止 |
| ⏮ Seek Start | `Home` | 先頭へ移動 |
| ◀ Seek Previous | `PageUp` | 前のマーカーへ |
| ▶ Seek Next | `PageDown` | 次のマーカーへ |
| ⏭ Seek End | `End` | 末尾へ移動 |
| << Step Backward | `←` | 1 フレーム戻る |
| >> Step Forward | `→` | 1 フレーム進む |
| 🔁 Loop | `L` | ループ再生トグル |
| I | `I` | In 点設定 |
| O | `O` | Out 点設定 |
| × Clear | - | In/Out クリア |

#### シグナル

```cpp
// 再生制御
void playRequested();
void pauseRequested();
void stopRequested();

// シーク操作
void seekStartRequested();
void seekEndRequested();
void seekPreviousRequested();
void seekNextRequested();

// フレーム操作
void stepForwardRequested();
void stepBackwardRequested();

// 設定
void loopToggled(bool enabled);
void playbackSpeedChanged(float speed);

// In/Out Points
void inPointSet();
void outPointSet();
void inoutCleared();
```

---

### 3. ArtifactPlaybackInfoWidget

#### 表示項目

```
┌──────────────────────────────────────────────────────────────┐
│  Frame: 125 / 300  │  00:00:04:05  │  30.00 fps  │  1.00x  │  Dropped: 0  │
└──────────────────────────────────────────────────────────────┘
```

- **フレーム表示**: 現在フレーム / 総フレーム数
- **タイムコード**: HH:MM:SS:FF 形式
- **FPS**: フレームレート
- **速度**: 再生速度（逆再生時は "REV" 表示）
- **ドロップ**: ドロップフレーム数

#### メソッド

```cpp
void setCurrentFrame(int64_t frame);
void setTotalFrames(int64_t frames);
void setFrameRate(float fps);
void setPlaybackSpeed(float speed);
void setDroppedFrames(int64_t count);
```

---

### 4. ArtifactPlaybackSpeedWidget

#### UI 構成

```
┌─────────────────────────────────────────────────────────┐
│  Speed: [1.0x ▼]  │  ──────●──────  │  [1.00x]  │
│    プリセット         スライダー         数値入力      │
└─────────────────────────────────────────────────────────┘
```

#### プリセット

- 0.25x (1/4 速)
- 0.5x (1/2 速)
- 1.0x (通常)
- 2.0x (倍速)
- -1.0x (逆再生)

---

## ArtifactPlaybackService との連携

### 双方向接続

```cpp
// UI → Service
void ArtifactPlaybackControlWidget::play() {
    if (auto* service = ArtifactPlaybackService::instance()) {
        service->play();
    }
    Q_EMIT playRequested();
}

// Service → UI
QObject::connect(service, &ArtifactPlaybackService::playbackStateChanged,
    this, [this](PlaybackState state) {
        updatePlaybackState(state);
    });
```

### 状態同期

| UI 状態 | Service プロパティ | 同期方法 |
|--------|------------------|---------|
| 再生中 | `isPlaying()` | `playbackStateChanged` |
| ループ | `isLooping()` | `loopingChanged` |
| 速度 | `playbackSpeed()` | `playbackSpeedChanged` |
| フレーム | `currentFrame()` | `frameChanged` |

---

## スタイリング

### CSS スタイル

```css
/* ボタン基本スタイル */
QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px;
}

/* ホバー時 */
QToolButton:hover {
    background-color: rgba(255, 255, 255, 0.1);
    border: 1px solid rgba(255, 255, 255, 0.2);
}

/* 押下時 */
QToolButton:pressed {
    background-color: rgba(255, 255, 255, 0.2);
}

/* チェック時（ループボタンなど） */
QToolButton:checked {
    background-color: rgba(100, 150, 255, 0.3);
    border: 1px solid rgba(100, 150, 255, 0.5);
}

/* スライダー */
QSlider::groove:horizontal {
    background: #3e3e42;
    height: 4px;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #6496ff;
    width: 12px;
    margin: -4px 0;
    border-radius: 6px;
}
```

---

## 使用例

### メインウィンドウへの統合

```cpp
// メインウィンドウのコンストラクタ
auto* playbackControl = new ArtifactPlaybackControlWidget();
auto* playbackInfo = new ArtifactPlaybackInfoWidget();
auto* speedControl = new ArtifactPlaybackSpeedWidget();

// レイアウトに追加
auto* bottomLayout = new QVBoxLayout();
bottomLayout->addWidget(playbackControl);
bottomLayout->addWidget(playbackInfo);
bottomLayout->addWidget(speedControl);

// サービスと接続
auto* playbackService = ArtifactPlaybackService::instance();

QObject::connect(playbackService, &ArtifactPlaybackService::frameChanged,
    playbackInfo, &ArtifactPlaybackInfoWidget::setCurrentFrame);

QObject::connect(playbackService, &ArtifactPlaybackService::frameRangeChanged,
    playbackInfo, [playbackInfo](const FrameRange& range) {
        playbackInfo->setTotalFrames(range.duration());
    });

QObject::connect(playbackService, &ArtifactPlaybackService::droppedFrameDetected,
    playbackInfo, &ArtifactPlaybackInfoWidget::setDroppedFrames);
```

### キーボードショートカット

```cpp
// メインウィンドウの keyPressEvent
void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        playbackControl->togglePlayPause();
        break;
    case Qt::Key_Home:
        playbackControl->seekStart();
        break;
    case Qt::Key_End:
        playbackControl->seekEnd();
        break;
    case Qt::Key_I:
        playbackControl->setInPoint();
        break;
    case Qt::Key_O:
        playbackControl->setOutPoint();
        break;
    }
}
```

---

## 改善点

### Before

- ❌ シグナル/スロット接続が不完全
- ❌ Service との双方向連携なし
- ❌ UI 要素が不足
- ❌ スタイリングなし
- ❌ ドキュメント不足

### After

- ✅ 完全な Service 統合
- ✅ 双方向状態同期
- ✅ 豊富な UI 要素（3 ウィジェット）
- ✅ モダンなスタイリング
- ✅ 包括的ドキュメント

---

## テストチェックリスト

### 基本機能
- [ ] 再生/停止/一時停止ボタンが機能
- [ ] Space キーで再生/一時停止トグル
- [ ] ボタンの有効/無効状態が正しく更新

### シーク操作
- [ ] Home/End で先頭/末尾へ移動
- [ ] PageUp/PageDown でマーカー間移動
- [ ] 矢印キーで 1 フレーム移動

### In/Out Points
- [ ] I キーで In 点設定
- [ ] O キーで Out 点設定
- [ ] ループ再生が In/Out 範囲で動作

### 表示更新
- [ ] フレーム表示が更新
- [ ] タイムコードが正確
- [ ] ドロップフレーム数が表示

### パフォーマンス
- [ ] 60fps で UI が更新
- [ ] ボタン操作に遅延なし
- [ ] メモリリークなし

---

## 変更ファイル

| ファイル | 変更内容 |
|---------|---------|
| `ArtifactPlaybackControlWidget.ixx` | 全面的な書き換え（3 ウィジェット定義） |
| `ArtifactPlaybackControlWidget.cppm` | 全面的な書き換え（実装） |

---

## 関連ドキュメント

- [MULTITHREADED_PLAYBACK_ENGINE.md](./MULTITHREADED_PLAYBACK_ENGINE.md)
- [IN_OUT_POINTS_INTEGRATION.md](./IN_OUT_POINTS_INTEGRATION.md)
- [TIMELINE_KEYBOARD_SHORTCUTS.md](./TIMELINE_KEYBOARD_SHORTCUTS.md)
