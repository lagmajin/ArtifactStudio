module;

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <variant>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

export module ArtifactPr.SequenceExporter;

import ArtifactPr.SequenceExporter;
import ArtifactPr.SequenceCompositor;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Encoder.FFmpegEncoder;
import Codec.FFmpegVideoDecoder;
import Video.VideoFrame;
import NLE.Core;
import Frame.Range;
import Frame.Position;

namespace ArtifactPr {

namespace {

/// ProgramMonitorPanel::isStillImagePath と同じ判定。
bool isStillImagePath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    static const std::set<QString> imageExts = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("webp")
    };
    return imageExts.contains(suffix);
}

QSize parseResolution(const QString& resolution)
{
    const QStringList parts = resolution.split(QChar('x'));
    bool okW = false;
    bool okH = false;
    const int w = parts.value(0).toInt(&okW);
    const int h = parts.value(1).toInt(&okH);
    if (okW && okH && w > 0 && h > 0) {
        return QSize(w, h);
    }
    return QSize(1920, 1080);
}

} // namespace

// =====================================================================
// SequenceTimelineRenderer
// =====================================================================

SequenceTimelineRenderer::SequenceTimelineRenderer(const RenderPlan& plan)
{
    store_ = std::make_unique<ArtifactCore::NLE::NLEProjectStore>();
    if (!store_->loadFromJson(plan.nleSnapshot)) {
        store_.reset();
        return;
    }
    canvasSize_ = QSize(
        static_cast<int>(std::lround(plan.qualityScale * parseResolution(plan.resolution).width())),
        static_cast<int>(std::lround(plan.qualityScale * parseResolution(plan.resolution).height())));
    canvasSize_ = QSize(std::max(16, canvasSize_.width()), std::max(16, canvasSize_.height()));
}

SequenceTimelineRenderer::~SequenceTimelineRenderer() = default;

void SequenceTimelineRenderer::collectActiveClips(int64_t frame,
                                                  QVector<ActiveClip>& out) const
{
    out.clear();
    if (!store_) {
        return;
    }

    // trackOrder の並び = 下→上 (rebuildLegacySnapshotFromNLE の videoTracks 順と一致)
    for (const auto& sequenceId : store_->sequenceIds()) {
        const auto* sequence = store_->sequence(sequenceId);
        if (!sequence) {
            continue;
        }
        for (const auto& trackId : sequence->trackOrder) {
            const auto* track = store_->track(trackId);
            if (!track || track->kind != ArtifactCore::NLE::TrackKind::Video || !track->enabled) {
                continue;
            }
            if (track->mute) {
                continue;
            }
            for (const auto& clipId : track->clipOrder) {
                const auto* clip = store_->clip(clipId);
                if (!clip || !clip->enabled || !clip->timelineRange.isValid()) {
                    continue;
                }
                // timelineRange は [start, end] 扱い。プレビューの
                // frame >= start && frame < start+duration に揃える。
                const int64_t start = clip->timelineRange.start();
                const int64_t end = start + std::max<int64_t>(0, clip->timelineRange.duration());
                if (frame < start || frame >= end) {
                    continue;
                }
                ActiveClip entry;
                entry.clip = clip;
                const auto* source = store_->source(clip->sourceId);
                if (!source) {
                    continue;
                }
                entry.sourceFile =
                    source->useProxy && !source->proxyUri.isEmpty()
                        ? source->proxyUri
                        : source->uri;
                if (!entry.sourceFile.isEmpty()) {
                    out.append(entry);
                }
            }
        }
    }
}

bool SequenceTimelineRenderer::decodeSourceFrame(const QString& filePath,
                                                 const int64_t sourceFrame,
                                                 const bool stillImage,
                                                 ArtifactCore::ImageF32x4_RGBA& out)
{
    if (stillImage) {
        auto it = stillCache_.find(filePath);
        if (it == stillCache_.end()) {
            ArtifactCore::ImageF32x4_RGBA image;
            if (!image.load(filePath)) {
                return false;
            }
            it = stillCache_.emplace(filePath, std::move(image)).first;
        }
        out = it->second.DeepCopy();
        return true;
    }

    auto capIt = captures_.find(filePath);
    if (capIt == captures_.end()) {
        ArtifactCore::FFmpegVideoDecoder decoder;
        if (!decoder.openFile(filePath)) {
            return false;
        }
        capIt = captures_.emplace(filePath, std::move(decoder)).first;
        lastDecodedFrame_[filePath] = -1;
    }
    ArtifactCore::FFmpegVideoDecoder& decoder = capIt->second;

    // MediaFrameDecoder と同じ逐次読み fast path。
    // 未記録 = 初回要求 → 常に exact-frame デコード。記録済みなら
    // 「直前+1」は逐次 read()、それ以外は seek + PTS ウォーク付きデコード
    // （BACKWARD シーク着地点はキーフレームのため、単発 decodeNext では
    // 要求フレームの手前の絵を掴む）。
    const bool hasLast = lastDecodedFrame_.count(filePath) > 0;
    const int64_t expected = hasLast ? lastDecodedFrame_.at(filePath) + 1 : -1;
    const auto decoded = (hasLast && sourceFrame == expected)
        ? decoder.decodeNextVideoFrameRaw()
        : decoder.decodeFrameAtRaw(std::max<int64_t>(0, sourceFrame));
    const auto* cpuFrame = std::get_if<ArtifactCore::CpuVideoFrame>(&decoded);
    if (!cpuFrame || !cpuFrame->isValid()) {
        captures_.erase(capIt);
        lastDecodedFrame_.erase(filePath);
        return false;
    }
    lastDecodedFrame_[filePath] = sourceFrame;

    // Core デコーダ出力は RGB24 (stride 考慮) → canonical RGBA8 へ詰め替え
    cv::Mat rgb24(cpuFrame->meta.height, cpuFrame->meta.width, CV_8UC3,
                  cpuFrame->bytes.data(), cpuFrame->strideBytes);
    cv::Mat rgba8;
    cv::cvtColor(rgb24, rgba8, cv::COLOR_RGB2RGBA);
    out.setFromRGBA8(rgba8.data, rgba8.cols, rgba8.rows);
    return true;
}

bool SequenceTimelineRenderer::renderFrame(const int64_t frame,
                                           ArtifactCore::ImageF32x4_RGBA& out)
{
    QVector<ActiveClip> active;
    collectActiveClips(frame, active);

    QVector<ArtifactPr::CompositeLayer> layers;
    layers.reserve(active.size());
    for (const ActiveClip& entry : active) {
        const auto* clip = entry.clip;
        // プレビュー (requestPreviewFrame) と同じソースフレーム式。
        int64_t sourceFrame = clip->sourceRange.isValid()
            ? clip->sourceRange.start() +
                  static_cast<int64_t>(std::llround(
                      static_cast<double>(frame - clip->timelineRange.start()) * clip->speed))
            : frame;
        if (clip->reversed) {
            sourceFrame = clip->sourceRange.end() - (sourceFrame - clip->sourceRange.start());
            sourceFrame = std::max<int64_t>(clip->sourceRange.start(),
                                            std::min(sourceFrame, clip->sourceRange.end() - 1));
        }
        sourceFrame = std::max<int64_t>(sourceFrame, 0);

        ArtifactCore::ImageF32x4_RGBA decoded;
        const bool still = isStillImagePath(entry.sourceFile);
        if (!decodeSourceFrame(entry.sourceFile, sourceFrame, still, decoded)) {
            continue;
        }

        ArtifactPr::CompositeLayer layer;
        layer.frame = std::move(decoded);
        layer.opacity = clip->opacity;
        layers.append(layer);
    }

    const ArtifactCore::FloatRGBA background(0.0f, 0.0f, 0.0f, 1.0f);
    out = ArtifactPr::composeSequenceLayers(canvasSize_, layers, background);
    return !out.isEmpty();
}

// =====================================================================
// exportSequence
// =====================================================================

ExportResult exportSequence(const RenderPlan& plan,
                            const ExportSettings& settings,
                            const std::atomic_bool& cancel,
                            const std::function<void(int percent)>& onProgress)
{
    ExportResult result;
    if (!plan.isValid()) {
        result.error = QStringLiteral("Invalid render plan");
        return result;
    }
    if (settings.outputPath.trimmed().isEmpty()) {
        result.error = QStringLiteral("Output path is empty");
        return result;
    }
    if (settings.width <= 0 || settings.height <= 0 || settings.fps <= 0.0) {
        result.error = QStringLiteral("Invalid export dimensions or frame rate");
        return result;
    }

    SequenceTimelineRenderer renderer(plan);
    if (!renderer.isValid()) {
        result.error = QStringLiteral("Failed to restore sequence snapshot");
        return result;
    }

    const int64_t firstFrame = plan.startFrame;
    const int64_t lastFrame = plan.endFrame;
    const int64_t totalFrames = lastFrame - firstFrame + 1;
    if (totalFrames <= 0) {
        result.error = QStringLiteral("Empty render range");
        return result;
    }

    ArtifactCore::FFmpegEncoder encoder;

    if (settings.format.isVideoFile()) {
        ArtifactCore::FFmpegEncoderSettings encodeSettings;
        encodeSettings.width = settings.width;
        encodeSettings.height = settings.height;
        encodeSettings.fps = settings.fps;
        encodeSettings.crf = std::clamp(static_cast<int>(std::lround(40.0 - 0.3 * settings.quality)), 14, 40);
        switch (settings.format.value) {
        case ExportFormat::Value::H264Mp4:
            encodeSettings.videoCodec = QStringLiteral("h264");
            encodeSettings.container = QStringLiteral("mp4");
            break;
        case ExportFormat::Value::HevcMp4:
            encodeSettings.videoCodec = QStringLiteral("hevc");
            encodeSettings.container = QStringLiteral("mp4");
            break;
        case ExportFormat::Value::ProResMov:
            encodeSettings.videoCodec = QStringLiteral("prores");
            encodeSettings.container = QStringLiteral("mov");
            break;
        case ExportFormat::Value::DnxhdMov:
            encodeSettings.videoCodec = QStringLiteral("dnxhd");
            encodeSettings.container = QStringLiteral("mov");
            break;
        default:
            break;
        }

        if (!encoder.open(settings.outputPath, encodeSettings)) {
            result.error = encoder.lastError();
            return result;
        }
    } else if (settings.format.isImageSequence()) {
        ArtifactCore::FFmpegImageSequenceSettings seqSettings;
        seqSettings.width = settings.width;
        seqSettings.height = settings.height;
        seqSettings.startFrame = static_cast<int>(firstFrame);
        seqSettings.padding = 4;
        seqSettings.jpegQuality = std::clamp(settings.quality, 1, 100);
        if (settings.format.value == ExportFormat::Value::PngSequence) {
            seqSettings.format = QStringLiteral("png");
        } else {
            seqSettings.format = QStringLiteral("jpeg");
        }

        QString pattern = settings.outputPath;
        if (!pattern.contains(QStringLiteral("%04d"))) {
            const int dot = pattern.lastIndexOf(QChar('.'));
            const QString stem = dot > 0 ? pattern.left(dot) : pattern;
            const QString ext = dot > 0 ? pattern.mid(dot) : QStringLiteral(".png");
            pattern = stem + QStringLiteral("%04d") + ext;
        }
        if (!encoder.openImageSequence(pattern, seqSettings)) {
            result.error = encoder.lastError();
            return result;
        }
    } else {
        result.error = QStringLiteral("Unsupported export format");
        return result;
    }

    ArtifactCore::ImageF32x4_RGBA frame;
    for (int64_t f = firstFrame; f <= lastFrame; ++f) {
        if (cancel.load(std::memory_order_relaxed)) {
            encoder.close();
            result.framesWritten = static_cast<int>(f - firstFrame);
            result.error = QStringLiteral("Cancelled by user");
            return result;
        }

        frame = {};
        if (renderer.renderFrame(f, frame)) {
            if (!encoder.addImage(frame)) {
                result.error = encoder.lastError();
                encoder.close();
                return result;
            }
            ++result.framesWritten;
        }
        // レンダ失敗フレームはスキップ (黒欠落より継続優先)。

        if (onProgress) {
            const int percent = static_cast<int>(
                (static_cast<double>(f - firstFrame + 1) / static_cast<double>(totalFrames)) * 100.0);
            onProgress(std::clamp(percent, 0, 100));
        }
    }

    encoder.close();

    result.success = result.framesWritten > 0;
    if (!result.success && result.error.isEmpty()) {
        result.error = QStringLiteral("No frames were rendered");
    }
    return result;
}

} // namespace ArtifactPr
