module;

#include <QString>
#include <QSize>
#include <QVector>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

export module ArtifactPr.SequenceExporter;

import ArtifactPr.EditorEngine;
import ArtifactPr.SequenceCompositor;
import Image.ImageF32x4_RGBA;
import Encoder.FFmpegEncoder;
import Codec.FFmpegVideoDecoder;
import NLE.Core;
import Frame.Range;
import Frame.Position;

export namespace ArtifactPr {

/// エクスポート出力形式。
struct ExportFormat {
    enum class Value {
        H264Mp4,
        HevcMp4,
        ProResMov,
        DnxhdMov,
        PngSequence,
        JpegSequence,
    };
    Value value = Value::H264Mp4;

    bool isVideoFile() const
    {
        return value == Value::H264Mp4 || value == Value::HevcMp4 ||
               value == Value::ProResMov || value == Value::DnxhdMov;
    }
    bool isImageSequence() const
    {
        return value == Value::PngSequence || value == Value::JpegSequence;
    }
};

/// ExportDialog から収集する設定一式。
struct ExportSettings {
    QString outputPath;          // 動画: ファイルパス / 連番: "%04d" を含む pattern
    ExportFormat format;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    int quality = 80;            // 1-100 (quality → crf 変換は exportSequence 内)
};

struct ExportResult {
    bool success = false;
    QString error;
    int framesWritten = 0;
};

/// RenderPlan の凍結スナップショットから timeline フレームを同期レンダリングする。
///
/// ProgramMonitorPanel のプレビュー組立て (requestPreviewFrame / onFrameDecoded) と
/// 同じ意味論: 映像トラックの muted/solo 無視なし・clip.enabled / speed / reversed /
/// opacity を反映し、composeSequenceLayers で fit 合成する。
/// トランジションは未合成 (プレビューと同一挙動)。
class SequenceTimelineRenderer {
public:
    explicit SequenceTimelineRenderer(const RenderPlan& plan);
    ~SequenceTimelineRenderer();

    SequenceTimelineRenderer(const SequenceTimelineRenderer&) = delete;
    SequenceTimelineRenderer& operator=(const SequenceTimelineRenderer&) = delete;

    bool isValid() const { return store_ != nullptr; }

    /// plan レンジ内の 1 フレームを合成して out へ返す。失敗時 false。
    bool renderFrame(int64_t frame, ArtifactCore::ImageF32x4_RGBA& out);

private:
    struct ActiveClip {
        const ArtifactCore::NLE::Clip* clip = nullptr;
        QString sourceFile;
    };

    void collectActiveClips(int64_t frame, QVector<ActiveClip>& out) const;
    bool decodeSourceFrame(const QString& filePath,
                           int64_t sourceFrame,
                           bool stillImage,
                           ArtifactCore::ImageF32x4_RGBA& out);

    std::unique_ptr<ArtifactCore::NLE::NLEProjectStore> store_;
    QSize canvasSize_{1920, 1080};

    // 同期デコーダキャッシュ (worker thread 専有、mutex 不要)。
    std::map<QString, ArtifactCore::FFmpegVideoDecoder> captures_;
    std::map<QString, int64_t> lastDecodedFrame_;
    std::map<QString, ArtifactCore::ImageF32x4_RGBA> stillCache_;
};

/// シーケンスをエクスポートする (blocking)。worker thread から呼ぶこと。
/// cancel は 1 フレーム毎に検査される。onProgress は 0-100。
ExportResult exportSequence(const RenderPlan& plan,
                            const ExportSettings& settings,
                            const std::atomic_bool& cancel,
                            const std::function<void(int percent)>& onProgress);

} // namespace ArtifactPr
