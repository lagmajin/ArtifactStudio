module;

#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

module ArtifactPr.SequenceAudioRenderer;

import ArtifactPr.EditorEngine;
import ArtifactPr.ClipEffects;
import Audio.Segment;
import Media.Encoder.FFmpegAudioDecoder;
import Audio.Render.Writer;

namespace ArtifactPr {

namespace {

struct DecodedClip {
    QVector<ArtifactCore::AudioSegment> segments;
    std::vector<qint64> segmentOffsets;   // セグメント先頭の累積フレーム (48kHz)
    qint64 totalFrames = 0;               // 48kHz 換算の全フレーム数
    int sampleRate = 48000;
};

bool decodeClip(const AudioClipPlan& clipPlan,
                DecodedClip& out,
                QString& error,
                const std::atomic_bool& cancel)
{
    ArtifactCore::FFmpegAudioDecoder decoder;
    if (!decoder.openFile(clipPlan.sourceFile)) {
        error = QStringLiteral("Failed to open audio source: %1").arg(clipPlan.sourceFile);
        return false;
    }
    out.sampleRate = decoder.sampleRate() > 0 ? decoder.sampleRate() : 48000;

    // EQ はクリップ単位でデコード直後に適用する (セグメントを跨いで
    // フィルタ状態を連続させるためチャンネル毎に 1 インスタンス)。
    ArtifactCore::ClipEqualizer eqLeft;
    ArtifactCore::ClipEqualizer eqRight;
    eqLeft.lowDb = clipPlan.eqLowDb;
    eqLeft.midDb = clipPlan.eqMidDb;
    eqLeft.highDb = clipPlan.eqHighDb;
    eqLeft.sampleRate = out.sampleRate;
    eqLeft.configure();
    eqRight = eqLeft;

    ArtifactCore::AudioSegment seg;
    while (decoder.decodeNextSegment(seg)) {
        if (cancel.load(std::memory_order_relaxed)) {
            error = QStringLiteral("Cancelled by user");
            return false;
        }
        const int frames = seg.frameCount();
        if (frames <= 0) continue;
        if (eqLeft.active) {
            if (!seg.channelData.isEmpty()) {
                eqLeft.process(seg.channelData[0].data(), frames);
            }
            if (seg.channelCount() > 1) {
                eqRight.process(seg.channelData[1].data(), frames);
            }
        }
        out.segmentOffsets.push_back(out.totalFrames);
        out.totalFrames += frames;
        out.segments.push_back(std::move(seg));
        if (out.segments.size() >= 8192) break;   // 異常長ガード
    }
    decoder.closeFile();
    return true;
}

/// デコード済みクリップから 48kHz 座標の 1 サンプルを取得する。
float clipSampleAt(const DecodedClip& clip, int channel, qint64 index)
{
    if (index < 0 || index >= clip.totalFrames) return 0.0f;
    auto it = std::upper_bound(clip.segmentOffsets.begin(),
                               clip.segmentOffsets.end(), index);
    if (it == clip.segmentOffsets.begin()) return 0.0f;
    const qsizetype segmentIndex = (it - clip.segmentOffsets.begin()) - 1;
    const ArtifactCore::AudioSegment& seg = clip.segments[segmentIndex];
    const qint64 local = index - clip.segmentOffsets[segmentIndex];
    if (channel >= seg.channelCount()) {
        channel = 0;
    }
    const auto& channelData = seg.channelData[channel];
    if (local >= channelData.size()) return 0.0f;
    return channelData[local];
}

} // namespace

AudioRenderResult renderSequenceAudio(const RenderPlan& plan,
                                      double fps,
                                      const QString& wavOutputPath,
                                      int bitDepth,
                                      const std::atomic_bool& cancel,
                                      const std::function<void(int percent)>& onProgress)
{
    AudioRenderResult result;
    if (fps <= 0.0) {
        result.error = QStringLiteral("Invalid frame rate");
        return result;
    }
    if (wavOutputPath.trimmed().isEmpty()) {
        result.error = QStringLiteral("Output path is empty");
        return result;
    }

    constexpr qint64 kOutputRate = 48000;
    constexpr qint64 kBlockFrames = kOutputRate;   // 1 秒ブロック

    const double spanFrames = static_cast<double>(plan.endFrame - plan.startFrame + 1);
    const qint64 totalOutFrames = static_cast<qint64>(
        std::ceil(spanFrames / fps * static_cast<double>(kOutputRate)));
    if (totalOutFrames <= 0) {
        result.error = QStringLiteral("Empty render range");
        return result;
    }

    const int clipCount = plan.audioClips.size();

    // ---- デコード (進捗 0-60) ----
    QVector<DecodedClip> decoded(clipCount);
    for (int i = 0; i < clipCount; ++i) {
        if (cancel.load(std::memory_order_relaxed)) {
            result.error = QStringLiteral("Cancelled by user");
            return result;
        }
        if (!decodeClip(plan.audioClips[i], decoded[i], result.error, cancel)) {
            return result;
        }
        if (onProgress) {
            onProgress(std::clamp(
                static_cast<int>(static_cast<double>(i + 1) * 60.0
                                 / static_cast<double>(std::max(1, clipCount))),
                0, 60));
        }
    }

    // ---- ミックス + WAV 書き出し (進捗 60-100) ----
    // AudioWriter::setBitDepth は openFile 前に呼ぶ必要がある。
    ArtifactCore::AudioWriter writer;
    writer.setBitDepth(bitDepth == 24 ? 24 : 16);
    writer.openFile(wavOutputPath);

    for (qint64 blockStart = 0; blockStart < totalOutFrames; blockStart += kBlockFrames) {
        if (cancel.load(std::memory_order_relaxed)) {
            result.error = QStringLiteral("Cancelled by user");
            return result;
        }
        const int blockFrames = static_cast<int>(
            std::min<qint64>(kBlockFrames, totalOutFrames - blockStart));

        ArtifactCore::AudioSegment mixed;
        mixed.sampleRate = static_cast<int>(kOutputRate);
        mixed.layout = ArtifactCore::AudioChannelLayout::Stereo;
        mixed.channelData = {
            QVector<float>(blockFrames, 0.0f),
            QVector<float>(blockFrames, 0.0f),
        };

        for (int i = 0; i < clipCount; ++i) {
            const AudioClipPlan& clipPlan = plan.audioClips[i];
            const DecodedClip& dec = decoded[i];
            if (dec.totalFrames <= 0) continue;

            // timeline frame → 出力 48kHz サンプル座標
            const qint64 outStart = static_cast<qint64>(std::llround(
                static_cast<double>(clipPlan.startFrame - plan.startFrame)
                / fps * static_cast<double>(kOutputRate)));
            const qint64 outDuration = static_cast<qint64>(std::llround(
                static_cast<double>(clipPlan.durationFrames)
                / fps * static_cast<double>(kOutputRate)));
            const qint64 outEnd = outStart + outDuration;
            const qint64 sourceOffset = static_cast<qint64>(std::llround(
                static_cast<double>(clipPlan.sourceIn)
                / fps * static_cast<double>(kOutputRate)));

            const qint64 begin = std::max(blockStart, outStart);
            const qint64 end = std::min(blockStart + blockFrames, outEnd);
            for (qint64 outSample = begin; outSample < end; ++outSample) {
                // デコーダ出力は 48kHz 正規化のため rate は実質 1.0
                const double rate = static_cast<double>(dec.sampleRate)
                    / static_cast<double>(kOutputRate);
                const qint64 srcIdx = sourceOffset
                    + static_cast<qint64>(std::llround(
                        static_cast<double>(outSample - outStart) / rate));
                const int blockIndex = static_cast<int>(outSample - blockStart);
                const float l = clipSampleAt(dec, 0, srcIdx);
                const float r = clipSampleAt(dec, 1, srcIdx);
                mixed.channelData[0][blockIndex] += l * static_cast<float>(clipPlan.volume);
                mixed.channelData[1][blockIndex] += r * static_cast<float>(clipPlan.volume);
            }
        }

        writer.write(mixed);
        if (onProgress) {
            const int percent = 60 + static_cast<int>(
                static_cast<double>(blockStart + blockFrames) * 40.0
                / static_cast<double>(totalOutFrames));
            onProgress(std::clamp(percent, 0, 100));
        }
    }

    writer.closeFile();
    result.success = true;
    return result;
}

} // namespace ArtifactPr
