# オーディオ終盤プチフリ/ノイズ原因分析 (2026-03-28)

## 症状

オーディオレイヤーのプレビュー再生で、最後のほうになるほどプチフリ、ノイズが増える。
WASAPI 共有デバイス使用時。

---

## 原因の流れ

```
再生ループが endFrame に到達
  → updateAudio() が呼ばれる
    → getAudio() が false を返す (オーディオデータ尽きた)
    → audioExhausted = true で break
    → プロデューサ停止

WASAPI コンシューマは継続
  → ringBuffer を drain
  → avail が framesToWrite より少くなる
    → audioCallback: memset(buffer, 0) → 部分的にデータコピー
    → [オーディオ][サイレンス][オーディオ][サイレンス] の繰り返し
  → バッファがさらに少なくなる
    → read() が avail=0 を返す
    → buffer 全体がサイレンス
  → ノイズ: SPSC リングバッファのread/write が競合状態
```

## 仮説

### ★★★ 1. プロデューサ停止後の部分アンダーラン（主要原因）

**場所:** `AudioRenderer.cppm:64-98`

```cpp
void audioCallback(float* buffer, int frames, int channelsRequested) {
    std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
    const bool success = ringBuffer->read(segment, frames);
    const int availableFrames = segment.frameCount();

    if (success && availableFrames > 0) {
        if (availableFrames < frames) {
            // 部分アンダーラン: データの一部だけコピー
            // 残りは memset でゼロ (サイレンス) のまま
        }
        // ... コピー処理 ...
    } else {
        // 完全アンダーラン: 全部サイレンス
    }
}
```

**問題の仕組み:**
1. プロデューサが `audioTargetBufferedFrames_` (= 48000*2 = 96000 サンプル ≈ 2秒) まで埋める
2. `getAudio()` が false を返す → プロデューサ停止
3. WASAPI が 50ms バッファ (2400 frames) ずつ消費
4. 残りが 96000 → 93600 → ... → 2000 と減少
5. `framesToWrite` = 2400 だが avail < 2400 になる瞬間が発生
6. **[2000 サンプルの実データ][400 サンプルのサイレンス]** → ブツブツ
7. 残りが 0 になると完全サイレンス → 最後までノイズだらけに聞こえる

**根本原因:** プロデューサが突然停止するのに対し、
コンシューマは継続的にデータを要求する。
部分アンダーランの度にサイレンスが挿入される。

### ★★ 2. WASAPI のポーリング方式でサイレンス間隔が不規則

**場所:** `WASAPIBackend.cppm:119-126`

```cpp
while (!stopRequested.load(std::memory_order_acquire)) {
    if (WaitForSingleObject(stopEvent, 2) == WAIT_OBJECT_0) break;
    pump();
}
```

2ms ごとのポーリング。バッファ残量が少なくなった時:
- ポーリングのタイミングによっては `GetCurrentPadding()` が不正確な値を返す
- サイレンス間隔が不規則 → ノイズとして認識される

### ★ 3. 再生終了時に ringBuffer がクリアされない

**場所:** `ArtifactPlaybackEngine.cppm:361-371`

```cpp
if (audioSeekPending_.exchange(false)) {
    audioRenderer_->clearBuffer();  // seek 時のみクリア
    audioNextFrame_ = currentFrame;
}
```

再生終了 (endFrame 到達) 時に ringBuffer をクリアする処理がない。
プロデューサは停止するが、コンシューマは残りのバッファを drain し続ける。
残量が不安定な状態が長く続く。

### ★ 4. SPSC ringBuffer の read/write 競合（終盤で発生しやすい）

**場所:** `AudioRingBuffer.cppm:62-113`

終盤でバッファ残量が少なくなると、read/write の head/tail が近づく。
この状態で部分アンダーランが発生すると:
- `read()` が avail を読み取る → avail < requested
- `write()` が同時に走る可能性 (プロデューサがまだ最終フレームを enqueue 中)
- SPSC の整合性は保たれるが、読み取り結果が不安定

---

## 対策案

### 対策1: 再生終了時に ringBuffer を強制クリア（最重要）

```cpp
// ArtifactPlaybackEngine の再生ループ終了時:
if (reachedEndFrame) {
    audioRenderer_->stop();
    audioRenderer_->clearBuffer();
    audioRenderer_->closeDevice();
}
```

→ コンシューマが drain を続けるのを即座に停止。
→ 終盤の不安定な部分アンダーランが発生しない。

### 対策2: プロデューサが尽きた時にサイレンスパディング

```cpp
// updateAudio() 内:
if (audioExhausted) {
    // 残りのバッファをサイレンスで埋める
    AudioSegment silence;
    silence.channelCount = audioChannels;
    silence.sampleRate = audioSampleRate_;
    const size_t remaining = audioRenderer_->bufferedFrames();
    if (remaining < audioTargetBufferedFrames_) {
        silence.frameCount = audioTargetBufferedFrames_ - remaining;
        silence.channelData.resize(silence.channelCount);
        for (auto& ch : silence.channelData) ch.resize(silence.frameCount, 0.0f);
        audioRenderer_->enqueue(silence);
    }
}
```

→ 部分アンダーランを完全アンダーラン (均一サイレンス) に変換。
→ ブツブツではなくスムーズなフェードアウト。

### 対策3: バッファ残量が少なくなったら徐々にフェードアウト

```cpp
// audioCallback 内:
if (avail < frames && avail > 0) {
    // フェードアウト: 最後のサンプルを徐々にゼロに
    float fadeGain = 1.0f;
    float fadeStep = 1.0f / static_cast<float>(avail);
    for (int i = 0; i < avail; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            buffer[i * channels + ch] *= fadeGain;
        }
        fadeGain -= fadeStep;
    }
}
```

→ 部分アンダーラン時にクリッピングノイズを回避。
→ スムーズな終了。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | 55-99 | audioCallback — 部分アンダーラン処理 |
| `ArtifactCore/src/Audio/AudioRenderer.cppm` | 67 | memset buffer to 0 |
| `ArtifactCore/src/Audio/AudioRingBuffer.cppm` | 83-113 | read — 部分読み取り |
| `ArtifactCore/src/Audio/AudioRingBuffer.cppm` | 59-81 | write — SPSC 書き込み |
| `ArtifactCore/src/Audio/WASAPIBackend.cppm` | 78-119 | renderLoop — ポーリング |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 373-391 | updateAudio — プロデューサループ |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 381 | getAudio 失敗時の break |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 404-408 | 低バッファ警告 |
