module;

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>

export module ArtifactPr.AudioPreviewMixer;

import Audio.Segment;
import Audio.Mixer;
import Audio.Bus;
import Media.Encoder.FFmpegAudioDecoder;
import AudioRenderer;
import Memory.SharedPtr;

export namespace ArtifactPr {

using FramePosition = int64_t;

/// ミキシング対象の 1 音声クリップ (durationFrames は timeline フレーム数)。
/// volume には audioGain エフェクト係数を含む (syncAudioClips 側で乗算済み)。
struct PreviewAudioClip {
    QString id;
    QString filePath;
    FramePosition startFrame = 0;
    FramePosition durationFrames = 0;
    FramePosition sourceIn = 0;
    double volume = 1.0;
    double eqLowDb = 0.0;
    double eqMidDb = 0.0;
    double eqHighDb = 0.0;
};

/// シーケンス音声プレビューのミキサー/出力エンジン。
///
/// 構成: クリップごとに FFmpegAudioDecoder で事前デコード (48kHz stereo float
/// 正規化済み) → 再生位置のクリップ分だけ AudioRenderer(WASAPI) へ
/// QTimer でチャンク enqueue。複数トラックの重なりは renderer 側バッファでなく
/// ここで float 加算ミックスしてから enqueue する。
/// signal を新規追加しないため、UI への通知は std::function callback で行う。
class AudioPreviewMixer {
public:
    /// (leftPeakDb, rightPeakDb) を UI thread で受け取るコールバック。
    using LevelCallback = std::function<void(float, float)>;

    AudioPreviewMixer();
    ~AudioPreviewMixer();

    AudioPreviewMixer(const AudioPreviewMixer&) = delete;
    AudioPreviewMixer& operator=(const AudioPreviewMixer&) = delete;

    void setLevelCallback(LevelCallback callback);

    /// 現在の timeline 音声レイアウトを差し替える (sequenceChanged 時に呼ぶ)。
    void setClips(const QVector<PreviewAudioClip>& clips);

    /// 波形 peak envelope 取得用 (filePath → 正規化 peak 列、0.5 秒分解能)。
    const QVector<float>* waveformPeaks(const QString& filePath);

    /// 再生制御。frame は timeline フレーム位置。
    void play(FramePosition frame);
    void pause();
    void stop();
    void seek(FramePosition frame);
    bool isPlaying() const { return playing_; }

private:
    struct ClipAudio {
        PreviewAudioClip clip;
        QVector<ArtifactCore::AudioSegment> segments;
        qint64 totalFrames = 0;      // 48kHz 換算の全フレーム数
        int sampleRate = 48000;
        QVector<float> waveformPeaks; // 0.5 秒ごとの正規化 peak
        ArtifactCore::SharedPtr<ArtifactCore::AudioBus> bus;  // クリップ毎バス (AudioMixer)
    };

    void loadClipAudio(ClipAudio& out);
    void loadClipAudioFor(ClipAudio& entry);
    static QString clipCacheKey(const PreviewAudioClip& clip);
    void rebuildBuses();
    void onTick();
    void pushLevel(float leftPeak, float rightPeak);

    QVector<ClipAudio> clips_;
    std::unique_ptr<ArtifactCore::AudioRenderer> renderer_;
    std::unique_ptr<ArtifactCore::AudioMixer> mixer_;   // Core バスグラフ
    QTimer* tickTimer_ = nullptr;
    QHash<QString, ClipAudio> clipCache_;

    bool playing_ = false;
    FramePosition playheadFrame_ = 0;
    double framesFed_ = 0.0;          // enqueue 済み timeline フレーム数
    int feedCounter_ = 0;
    LevelCallback levelCallback_;
    QMutex callbackMutex_;

    static constexpr int kChunkFrames = 512;   // 48kHz 換算チャンク
    static constexpr double kPeakBucketSeconds = 0.5;
};

} // namespace ArtifactPr
