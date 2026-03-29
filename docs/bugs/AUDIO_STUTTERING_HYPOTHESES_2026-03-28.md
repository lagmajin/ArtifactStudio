# オーディオ再生「ブツブツ」仮説 (2026-03-28)

## 症状

オーディオレイヤーの再生は動作するが、音がブツブツ切れる（ stuttering ）。

---

## パイプライン

```
PlaybackEngine (Producer)
  → updateAudio()    [ワーカースレッド, 4ms sleep]
    → composition->getAudio()  [WAV デコード + リサンプル]
    → audioRenderer->enqueue() [RingBuffer へ書き込み]

WASAPIBackend (Consumer)
  → renderLoop()     [別スレッド, 2ms sleep]
    → GetCurrentPadding()  [バッファ空き確認]
    → GetBuffer()          [WASAPI バッファへ書き込み]
    → ReleaseBuffer()
```

---

## 仮説

### ★★★ 1. WASAPI renderLoop がポーリング（イベント駆動ではない）

**場所:** `WASAPIBackend.cppm:112`

```cpp
while (!stopRequested) {
    pump();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));  // ← 固定 2ms sleep
}
```

WASAPI 共有モードでは、OS ミキサーが独自のタイミングで `IAudioRenderClient` を使用する。
2ms の固定 sleep では **OS の消費タイミングと同期しない**。

- OS が 2ms より速く音声を消費 → バッファが満杯 → `GetCurrentPadding()` が `bufferFrameCount` 近辺 → `framesToWrite` が 0 または極小 → **音声が届かない**
- 逆に OS のスケジューリングが遅い → バッファが空 → **アンダーラン**

**正しい実装:** `IAudioClient::SetEventHandle()` + `WaitForSingleObject()` でイベント駆動。
または `GetBuffer()` をコールバック内で呼び出す（WASAPI のイベントモード）。

### ★★ 2. デコードワーカースレッドの 4ms sleep

**場所:** `ArtifactPlaybackEngine.cppm:245`

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(4));
```

30fps で 1 フレームあたり `48000/30 = 1600` サンプル必要。
ワーカースレッドが 4ms sleep → RingBuffer の充填速度が OS の消費速度に追いつかない場合がある。

特に再生開始直後やシーク後にバッファが空の状態で、ワーカースレッドが sleep 中だとアンダーランが発生。

### ★★ 3. 開始時のプリロール不足

**場所:** `ArtifactPlaybackEngine.cppm:394-398`

```cpp
// Start threshold: 16 frames worth OR 0.5 seconds
const size_t audioStartBufferedFrames_ = static_cast<size_t>(
    std::max<int>(samplesPerFrame * 16, audioSampleRate_ / 2));
```

0.5 秒分のプリロールでは、OS のスケジューリング jitter（特に Windows の共有モード）に対して不十分な場合がある。
1〜2 秒分のプリロールが推奨。

### ★ 4. RingBuffer のサンプル単位コピー

**場所:** `AudioRingBuffer.cppm:67-77`

```cpp
// 1 サンプルずつコピー + モジュロ演算
for (size_t i = 0; i < frames; ++i) {
    for (int ch = 0; ch < data.channelCount(); ++ch) {
        channels_[ch][(w + i) % capacity_] = data.channelData()[ch][i];  // ← モジュロ演算毎回
    }
}
```

キャッシュに不親和。ブロックコピー (`memcpy` or `std::copy`) に比べて遅い。

### ★ 5. AudioSegment の毎フレームヒープアロケーション

**場所:** `ArtifactPlaybackEngine.cppm:375`

```cpp
AudioSegment segment;  // ← 毎フレーム新しい QVector<QVector<float>> を作成
if (!composition_->getAudio(segment, ...)) {
    break;
}
audioRenderer_->enqueue(segment);
```

毎フレーム `QVector<QVector<float>>` のアロケーション/デアロケーション。
GC 圧力が高くなる。

### ★ 6. 50ms のオーディオクロック補正閾値が粗い

**場所:** `ArtifactPlaybackEngine.cppm:420`

```cpp
if (std::abs(diff) > 0.05) {  // 50ms = 1.5 フレーム (30fps)
    // クロック補正
}
```

50ms 以内のドリフトは補正されない。20ms のドリフトでも「ブツブツ」に聞こえる。

---

## 対策案

### 対策1: WASAPI をイベント駆動に変更（最重要）

```cpp
// open() 内:
HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
audioClient->SetEventHandle(eventHandle);
audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, ...);

// renderLoop():
while (!stopRequested) {
    WaitForSingleObject(eventHandle, 100);  // OS が音声を必要とするまで待機
    pump();  // ここで GetBuffer/ReleaseBuffer
}
```

### 対策2: プリロール量を増やす

```cpp
const size_t audioStartBufferedFrames_ = std::max(
    samplesPerFrame * 32,          // 32 フレーム分
    audioSampleRate_ * 2           // 2 秒分 ← 増量
);
```

### 対策3: ワーカースレッドの sleep を条件付きに

```cpp
// RingBuffer の空きが十分ある場合のみ sleep
if (audioRenderer_->bufferedFrames() >= audioTargetBufferedFrames_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
}
// 空きが少なければ即座に次のフレームをデコード
```

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `ArtifactCore/src/Audio/WASAPIBackend.cppm` | 67-114 | renderLoop — 2ms ポーリングループ |
| `ArtifactCore/src/Audio/WASAPIBackend.cppm` | 120-201 | open — Shared モード初期化 |
| `ArtifactCore/src/Audio/AudioRingBuffer.cppm` | 59-81 | write — サンプル単位コピー |
| `ArtifactCore/src/Audio/AudioRingBuffer.cppm` | 83-113 | read — アンダーラン時サイレンス返却 |
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | 55-99 | audioCallback — アンダーランカウント |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 245 | ワーカースレッド 4ms sleep |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 309-405 | updateAudio — デコードプロデューサ |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 394-398 | プリロール閾値 (0.5 秒) |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 408-432 | syncWithAudioClock — 50ms 閾値 |
