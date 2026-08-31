module;

#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
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

import ArtifactPr.AudioPreviewMixer;
import ArtifactPr.ClipEffects;
import Audio.Segment;
import Audio.Mixer;
import Audio.Bus;
import Media.Encoder.FFmpegAudioDecoder;
import AudioRenderer;

namespace ArtifactPr {

AudioPreviewMixer::AudioPreviewMixer()
    : renderer_(std::make_unique<ArtifactCore::AudioRenderer>()),
      mixer_(std::make_unique<ArtifactCore::AudioMixer>())
{
    renderer_->setLevelCallback([this](const ArtifactCore::AudioLevelData& data) {
        pushLevel(data.leftPeak, data.rightPeak);
    });

    tickTimer_ = new QTimer();
    tickTimer_->setInterval(40);
    QObject::connect(tickTimer_, &QTimer::timeout, tickTimer_, [this]() { onTick(); });
}

AudioPreviewMixer::~AudioPreviewMixer()
{
    stop();
    delete tickTimer_;
}

void AudioPreviewMixer::setLevelCallback(LevelCallback callback)
{
    QMutexLocker lock(&callbackMutex_);
    levelCallback_ = std::move(callback);
}

void AudioPreviewMixer::pushLevel(float leftPeak, float rightPeak)
{
    LevelCallback callback;
    {
        QMutexLocker lock(&callbackMutex_);
        callback = levelCallback_;
    }
    if (callback) {
        // renderer のコールバックは出力スレッドから来る。UI へは queued で投げる。
        QMetaObject::invokeMethod(tickTimer_, [callback, leftPeak, rightPeak]() {
            callback(leftPeak, rightPeak);
        }, Qt::QueuedConnection);
    }
}

void AudioPreviewMixer::setClips(const QVector<PreviewAudioClip>& clips)
{
    clips_ = clips;

    // 再生に必要なクリップ音声をこの場でロード (デコード済みキャッシュ
    // clipCache_ があるため 2 回目以降は再利用)。
    for (auto& entry : clips_) {
        loadClipAudioFor(entry);
    }
    rebuildBuses();
}

/// AudioMixer のバスをクリップ構成に合わせて作り直す。
/// 各クリップバスはマスタへ接続する。バス削除 API が無いため mixer ごと作り直す。
void AudioPreviewMixer::rebuildBuses()
{
    mixer_ = std::make_unique<ArtifactCore::AudioMixer>();
    for (auto& entry : clips_) {
        if (entry.segments.isEmpty()) continue;
        const QString busName = QStringLiteral("clip:%1").arg(entry.clip.id);
        entry.bus = mixer_->createBus(busName);
        if (entry.bus) {
            mixer_->connect(entry.bus, mixer_->getMasterBus());
        }
    }
}

const QVector<float>* AudioPreviewMixer::waveformPeaks(const QString& filePath)
{
    PreviewAudioClip probe;
    probe.filePath = filePath;
    auto it = clipCache_.constFind(clipCacheKey(probe));
    if (it != clipCache_.constEnd()) {
        return &it.value().waveformPeaks;
    }

    ClipAudio entry;
    entry.clip.filePath = filePath;
    loadClipAudio(entry);
    if (entry.segments.isEmpty()) {
        return nullptr;
    }
    it = clipCache_.insert(clipCacheKey(probe), std::move(entry));
    return &it.value().waveformPeaks;
}

/// キャッシュキーは filePath + EQ パラメータ。同一ファイルでも EQ 設定が
/// 異なるクリップは別キャッシュになる。
QString AudioPreviewMixer::clipCacheKey(const PreviewAudioClip& clip)
{
    return clip.filePath
        + QStringLiteral("|eq=%1,%2,%3")
              .arg(QString::number(clip.eqLowDb),
                   QString::number(clip.eqMidDb),
                   QString::number(clip.eqHighDb));
}

/// clips_ の entry に対応するデコード済み音声を clipCache_ から写す
/// (無ければデコードしてキャッシュへ入れる)。
void AudioPreviewMixer::loadClipAudioFor(ClipAudio& entry)
{
    if (entry.clip.filePath.isEmpty()) return;
    if (!entry.segments.isEmpty()) return;

    const QString cacheKey = clipCacheKey(entry.clip);
    if (auto it = clipCache_.find(cacheKey); it != clipCache_.end()) {
        entry.segments = it.value().segments;
        entry.totalFrames = it.value().totalFrames;
        entry.sampleRate = it.value().sampleRate;
        entry.waveformPeaks = it.value().waveformPeaks;
        return;
    }

    ClipAudio fresh;
    fresh.clip = entry.clip;
    loadClipAudio(fresh);
    if (fresh.segments.isEmpty()) return;
    entry.segments = fresh.segments;
    entry.totalFrames = fresh.totalFrames;
    entry.sampleRate = fresh.sampleRate;
    entry.waveformPeaks = fresh.waveformPeaks;
    clipCache_.insert(cacheKey, std::move(fresh));
}

void AudioPreviewMixer::loadClipAudio(ClipAudio& out)
{
    std::unique_ptr<ArtifactCore::FFmpegAudioDecoder> decoder =
        std::make_unique<ArtifactCore::FFmpegAudioDecoder>();
    if (!decoder->openFile(out.clip.filePath)) {
        return;
    }
    out.sampleRate = decoder->sampleRate() > 0 ? decoder->sampleRate() : 48000;

    qint64 frames = 0;
    ArtifactCore::AudioSegment seg;
    while (decoder->decodeNextSegment(seg)) {
        const int segFrames = seg.frameCount();
        if (segFrames > 0) {
            frames += segFrames;
            out.segments.push_back(std::move(seg));
        }
        if (out.segments.size() >= 4096) break;  // 異常長ガード
    }
    decoder->closeFile();
    out.totalFrames = frames;

    // audioEqualizer エフェクト (3 帯 biquad)。セグメントを跨いで
    // フィルタ状態を連続させるためチャンネル毎に 1 インスタンス使う。
    ArtifactCore::ClipEqualizer eqLeft;
    eqLeft.lowDb = out.clip.eqLowDb;
    eqLeft.midDb = out.clip.eqMidDb;
    eqLeft.highDb = out.clip.eqHighDb;
    eqLeft.sampleRate = out.sampleRate;
    eqLeft.configure();
    if (eqLeft.active) {
        ArtifactCore::ClipEqualizer eqRight = eqLeft;
        for (auto& segment : out.segments) {
            const int segFrames = segment.frameCount();
            if (segFrames <= 0) continue;
            if (!segment.channelData.isEmpty()) {
                eqLeft.process(segment.channelData[0].data(), segFrames);
            }
            if (segment.channelCount() > 1) {
                eqRight.process(segment.channelData[1].data(), segFrames);
            }
        }
    }

    // waveform peak envelope (0.5 秒バケット) — EQ 適用後のデータから計算。
    for (const auto& segment : out.segments) {
        const int segFrames = segment.frameCount();
        const int channels = segment.channelCount();
        if (segFrames <= 0 || channels <= 0) continue;
        const int bucketFrames = static_cast<int>(
            static_cast<double>(out.sampleRate) * kPeakBucketSeconds);
        for (int start = 0; start < segFrames; start += bucketFrames) {
            const int end = std::min(segFrames, start + bucketFrames);
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                const float* data = segment.constData(ch);
                for (int i = start; i < end; ++i) {
                    peak = std::max(peak, std::fabs(data[i]));
                }
            }
            out.waveformPeaks.append(std::clamp(peak, 0.0f, 1.0f));
        }
    }
}

void AudioPreviewMixer::play(FramePosition frame)
{
    playheadFrame_ = frame;
    framesFed_ = 0.0;
    feedCounter_ = 0;

    bool anyAudio = false;
    for (const auto& entry : clips_) {
        if (!entry.segments.isEmpty()) { anyAudio = true; break; }
    }

    if (!anyAudio) return;
    if (!renderer_->isDeviceOpen()) {
        if (!renderer_->openDevice(ArtifactCore::AudioBackendType::WASAPI)) {
            pushLevel(-60.0f, -60.0f);
            return;
        }
    }
    renderer_->clearBuffer();
    renderer_->start();
    if (!renderer_->isActive()) {
        renderer_->closeDevice();
        pushLevel(-60.0f, -60.0f);
        return;
    }
    playing_ = true;
    tickTimer_->start();
}

void AudioPreviewMixer::pause()
{
    playing_ = false;
    tickTimer_->stop();
    if (renderer_->isDeviceOpen()) {
        renderer_->clearBuffer();
        renderer_->stop();
    }
    pushLevel(-60.0f, -60.0f);
}

void AudioPreviewMixer::stop()
{
    pause();
    if (renderer_->isDeviceOpen()) {
        renderer_->closeDevice();
    }
    playheadFrame_ = 0;
}

void AudioPreviewMixer::seek(FramePosition frame)
{
    playheadFrame_ = frame;
    framesFed_ = 0.0;
    if (renderer_->isDeviceOpen()) {
        renderer_->clearBuffer();
    }
}

void AudioPreviewMixer::onTick()
{
    if (!playing_) return;

    // timeline フレーム位置 → 48kHz サンプル換算でチャンク供給。
    // エンジン currentFrame を基準に追従するため、tick 毎に現在位置を進める。
    constexpr double kAssumedFps = 30.0;
    const double framesPerTick = kAssumedFps * tickTimer_->interval() / 1000.0;
    playheadFrame_ += static_cast<FramePosition>(framesPerTick);

    int queuedFrames = 0;
    while (queuedFrames < 2048) {
        // 現在の供給位置に重なるクリップを収集し、クリップ毎バスへ addInput →
        // AudioMixer.process でミックス (Core バスグラフ経由)。
        const FramePosition chunkStartTimeline = playheadFrame_
            + static_cast<FramePosition>(framesFed_);
        const int chunkFrames48k = kChunkFrames;

        bool any = false;
        ArtifactCore::AudioSegment mixed;
        mixed.sampleRate = 48000;
        mixed.layout = ArtifactCore::AudioChannelLayout::Stereo;
        mixed.setFrameCount(chunkFrames48k);

        for (auto& entry : clips_) {
            if (entry.segments.isEmpty()) continue;
            if (!entry.bus) continue;
            const double rate = static_cast<double>(entry.sampleRate) / 48000.0;
            // クリップ内での相対サンプル位置 (48kHz 換算 → 元レート換算)
            const qint64 clipOffsetSamples = static_cast<qint64>(
                static_cast<double>(chunkStartTimeline - entry.clip.startFrame + entry.clip.sourceIn)
                * rate);
            if (chunkStartTimeline < entry.clip.startFrame
                || clipOffsetSamples >= entry.totalFrames) {
                continue;
            }
            const double vol = entry.clip.volume;

            // このクリップ分のチャンクを組み立ててバスへ入力する
            bool clipHasAudio = false;
            ArtifactCore::AudioSegment seg48k;
            seg48k.sampleRate = 48000;
            seg48k.layout = ArtifactCore::AudioChannelLayout::Stereo;
            seg48k.setFrameCount(chunkFrames48k);
            auto& left = seg48k.channelData[0];
            auto& right = seg48k.channelData[1];
            for (int i = 0; i < chunkFrames48k; ++i) {
                left[i] = 0.0f;
                right[i] = 0.0f;
            }

            for (int i = 0; i < chunkFrames48k; ++i) {
                const qint64 srcIdx = clipOffsetSamples
                    + static_cast<qint64>(static_cast<double>(i) / rate);
                if (srcIdx >= entry.totalFrames) break;
                // セグメント列から該当サンプルを引く
                qint64 acc = 0;
                for (const auto& seg : entry.segments) {
                    const int fc = seg.frameCount();
                    if (acc + fc > srcIdx) {
                        const int local = static_cast<int>(srcIdx - acc);
                        if (seg.channelCount() > 0 && local < fc) {
                            float l = seg.constData(0)[local];
                            float r = seg.channelCount() > 1 ? seg.constData(1)[local] : l;
                            left[i] = static_cast<float>(l * vol);
                            right[i] = static_cast<float>(r * vol);
                            clipHasAudio = true;
                        }
                        break;
                    }
                    acc += fc;
                }
            }

            if (clipHasAudio) {
                any = true;
                entry.bus->addInput(seg48k, 1.0f);
            }
        }

        if (!any) {
            // この位置に音声クリップ無し。次へ進む (無音チャンクは enqueue しない)
            framesFed_ += static_cast<double>(chunkFrames48k) / 48000.0 * kAssumedFps;
            if (framesFed_ > 1000000.0) break;   // 安全弁
            continue;
        }

        // バスグラフをトポロジカルソート順に処理してマスタ出力へ
        mixer_->process(mixed);
        if (!renderer_->enqueue(mixed)) break;
        queuedFrames += chunkFrames48k;
        framesFed_ += static_cast<double>(chunkFrames48k) / 48000.0 * kAssumedFps;
    }
}

} // namespace ArtifactPr
