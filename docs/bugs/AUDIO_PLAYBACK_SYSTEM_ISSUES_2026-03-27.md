# オーディオ再生システム 現状の課題 (2026-03-27)

**作成日:** 2026-03-27  
**ステータス:** 調査完了  
**関連コンポーネント:** AudioRenderer, ArtifactAudioLayer, ArtifactPlaybackEngine, AudioMixer

---

## 概要

現在のオーディオ再生システムは、再生の基盤は実装済みだが、UI 統合・機能・精度に複数の課題がある。

---

## 問題点一覧

### ★★★ 問題 1: オーディオレイヤーの UI 統合が不十分

**場所:** `Artifact/src/Layer/ArtifactAudioLayer.cppm`

**問題:**
- `ArtifactAudioLayer` は存在するが、UI での表示が極めて不十分
- waveform 表示が未実装
- プロパティパネルで基本的な情報（duration, sample rate, channels）が表示されない
- タイムライン上でオーディオレイヤーとビデオレイヤーの区別が付きにくい

**影響:**
- ユーザーがオーディオレイヤーを視覚的に把握できない
- 編集中にどのレイヤーがオーディオか分かりにくい
- 音声のタイミング調整が困難

**現状:**
```cpp
// ArtifactAudioLayer は持っている情報
- sourcePath
- volume
- muted
- hasAudio()
- getAudio()  // PCM 供給
```

**不足している情報:**
- duration（秒数）
- sample rate
- channel count
- waveform データ
- デコード状態（loaded / error / missing）

---

### ★★★ 問題 2: オーディオミキサーの機能不足

**場所:** `Artifact/src/Audio/ArtifactAudioMixer.cppm`

**問題:**
- マスターボリューム/ミュートのみ実装
- チャンネルストリップ（個別レイヤー制御）が未実装
- エフェクトスロットが未使用
- パン（左右定位）機能がない
- オーディオメーター（レベル表示）がない

**影響:**
- 個別レイヤーの音量調整ができない
- エフェクト（EQ, compressor, reverb）を適用できない
- 音の定位を制御できない
- レベルを確認できない

**実装済み:**
```cpp
// ArtifactAudioMixer
- masterVolume
- masterMute
```

**未実装:**
```cpp
// 必要な機能
- channelStrips[]  // 各レイヤーのボリューム/パン/ミュート
- effects[]        // エフェクトスロット
- meters[]         // レベルメーター
- solo             // ソロ機能
```

---

### ★★ 問題 3: 音声/映像同期の精度不足

**場所:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm:314-330`

**問題:**
- フレームベースの同期のみ
- サンプル精度の同期が不十分
- オーディオクロックとの連携が限定的
- 50ms 以上のずれがある場合のみ補正

**影響:**
- 長時間再生で音声がずれる可能性
- フレームレート変動時に同期が乱れる
- プロフェッショナルな編集には精度不足

**現状のコード:**
```cpp
// syncWithAudioClock()
double diff = audioTime - currentEngineTime;

// 50ms 以上ずれていたら補正
if (std::abs(diff) > 0.05) {  // ← 閾値が大きい
    // 時間を書き換えて追従
}
```

**改善案:**
- 1ms 以下の精度で同期
- 常時小さな補正を適用（スループ処理）
- ドリフト補正の追加

---

### ★★ 問題 4: オーディオデコードのキャッシュ不足

**場所:** `ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm`

**問題:**
- デコードした PCM のキャッシュがない
- 再生のたびにデコードし直す
- シーク時に毎回デコードが発生

**影響:**
- CPU 使用率が高い
- シーク時のレイテンシが大きい
- 複数レイヤー再生でパフォーマンス低下

**現状:**
```cpp
// デコードのたびに FFmpeg を呼び出し
bool decodeNextFrame(AudioBufferQueue& queue) {
    // 毎回 av_read_frame() + avcodec_receive_frame()
}
```

**改善案:**
- デコード済み PCM をキャッシュ
- プリフェッチ機能
- 循環バッファで効率的な管理

---

### ★ 問題 5: エラーハンドリングが不十分

**場所:** `ArtifactCore/src/Audio/WASAPIBackend.cppm`

**問題:**
- デバイス接続切れの検出がない
- デコードエラーの詳細な報告がない
- リトライ機構がない

**影響:**
- オーディオデバイス変更時にクラッシュ
- エラー理由がユーザーに伝わらない
- 一時的なエラーで再生停止

**現状:**
```cpp
// エラー時のログ出力のみ
qWarning() << "[AudioRenderer] underflow";
```

**改善案:**
- デバイス変更の自動検出
- エラー理由の UI 表示
- 自動リトライ（3 回まで）

---

### ★ 問題 6: マルチチャンネル対応不足

**場所:** `ArtifactCore/src/Audio/AudioDownMixer.cppm`

**問題:**
- ステレオダウンミックスのみ実装
- 5.1ch / 7.1ch サラウンド未対応
- モノラル出力の品質が低い

**影響:**
- サラウンド音源が正しく再生できない
- 映画/ゲーム素材の編集が困難

---

### ★ 問題 7: オーディオエフェクトが未実装

**場所:** `ArtifactCore/include/Audio/AudioEffect.ixx`

**問題:**
- インターフェースのみで実体がない
- 基本的なエフェクト（EQ, compressor）もない
- VST/AU プラグインホスティングなし

**影響:**
- 音質調整ができない
- 音声のノイズリダクションができない
- 本格的なオーディオ編集ができない

---

## 優先度別改善案

### P0（必須）

| 修正 | 工数 | 効果 |
|------|------|------|
| オーディオレイヤー UI 統合 | 6-8h | 視認性向上 |
| チャンネルストリップ実装 | 4-6h | 個別制御可能に |
| サンプル精度同期 | 4-6h | 同期精度向上 |

### P1（重要）

| 修正 | 工数 | 効果 |
|------|------|------|
| オーディオキャッシュ | 3-4h | パフォーマンス向上 |
| エラーハンドリング改善 | 2-3h | 安定性向上 |
| Waveform 表示 | 4-6h | 編集性向上 |

### P2（推奨）

| 修正 | 工数 | 効果 |
|------|------|------|
| マルチチャンネル対応 | 6-8h | サラウンド対応 |
| 基本エフェクト実装 | 8-12h | 音質調整可能に |
| ドリフト補正 | 2-3h | 長時間再生安定 |

---

## 関連マイルストーン

- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md` - オーディオレイヤー統合
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md` - オーディオパイプライン完成
- `docs/planned/MILESTONES_BACKLOG.md` - M-AU-1〜3

---

## 技術的詳細

### オーディオレイヤーの現状

```cpp
// ArtifactAudioLayer が持っているもの
class ArtifactAudioLayer {
    QString sourcePath;
    float volume;
    bool muted;
    
    bool loadFromPath(const QString& path);
    bool getAudio(AudioSegment& out, ...);
    bool hasAudio() const;
};

// 不足しているもの
class ArtifactAudioLayer {
    // ❌ 不足
    float duration;              // 秒数
    int sampleRate;              // サンプルレート
    int channelCount;            // チャンネル数
    QVector<float> waveform;     // Waveform データ
    AudioLoadState state;        // loaded/error/missing
    QString errorMessage;        // エラーメッセージ
};
```

### 同期精度の問題

```cpp
// 現状：50ms 閾値
if (std::abs(diff) > 0.05) {  // 50ms = 1.5 フレーム (30fps)
    // 補正
}

// 改善案：1ms 閾値 + 常時補正
if (std::abs(diff) > 0.001) {  // 1ms
    // 小さな補正を適用
    currentFrame_ += diff * frameRate_ * 0.1;  // 10% ずつ追従
}
```

### オーディオキャッシュの設計案

```cpp
class AudioCache {
    struct CachedFrame {
        int64_t frameNumber;
        AudioSegment pcm;
        qint64 timestamp;
    };
    
    QMap<int64_t, CachedFrame> cache_;
    int maxFrames = 300;  // 10 秒分 (30fps)
    
    bool getCached(int64_t frame, AudioSegment& out);
    void cache(int64_t frame, const AudioSegment& pcm);
    void prefetch(int64_t startFrame, int count);
};
```

---

## 実装順序の推奨

1. **オーディオレイヤー UI 統合** - 視認性向上が最優先
2. **チャンネルストリップ** - 個別制御を可能に
3. **サンプル精度同期** - 品質向上
4. **オーディオキャッシュ** - パフォーマンス向上
5. **Waveform 表示** - 編集性向上
6. **エラーハンドリング** - 安定性向上

---

**文書終了**
