#pragma once

#include "ExternalRenderJobSchema.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace ArtifactRenderer {

struct FrameRenderResult {
    bool ok = false;
    bool canceled = false;
    bool cacheHit = false;
    int frameNumber = 0;
    int attempts = 0;
    int paintedLayerCount = 0;
    int unsupportedLayerCount = 0;
    QString backend;
    QString outputFile;
    QStringList paintedLayerKinds;
    QStringList unsupportedLayerKinds;
    QString errorMessage;

    QJsonObject toJson() const;
};

struct FrameRangeRenderResult {
    bool ok = false;
    bool canceled = false;
    int firstFrame = 0;
    int lastFrame = 0;
    int framesRendered = 0;
    int paintedLayerCount = 0;
    int unsupportedLayerCount = 0;
    QStringList outputFiles;
    QString errorMessage;

    QJsonObject toJson() const;
};

struct RenderExecutionOptions {
    QString backend = QStringLiteral("auto");
    QString cancelFile;
    bool resumeExistingFrames = false;
    int retryCount = 0;
    QString sourceBaseDirectory;
};

class ExternalFrameRenderer {
public:
    static FrameRenderResult renderFirstPngFrame(const RenderJobSummary& summary);
    static FrameRangeRenderResult renderPngFrameRange(
        const RenderJobSummary& summary,
        const RenderExecutionOptions& options,
        const std::function<void(const FrameRenderResult&, int, int)>& progressCallback);

private:
    static QString resolveOutputFile(const RenderJobSummary& summary, int frameNumber, int frameCount);
    static FrameRenderResult renderPngFrame(
        const RenderJobSummary& summary,
        const RenderExecutionOptions& options,
        int frameNumber,
        int frameCount);
};

} // namespace ArtifactRenderer
