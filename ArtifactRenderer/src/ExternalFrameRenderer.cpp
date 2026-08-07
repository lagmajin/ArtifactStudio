#include "ExternalFrameRenderer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QImage>
#include <QRect>
#include <QPainter>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QtGlobal>

namespace ArtifactRenderer {

QJsonObject FrameRenderResult::toJson() const
{
    QJsonObject object{
        {QStringLiteral("ok"), ok},
        {QStringLiteral("canceled"), canceled},
        {QStringLiteral("cacheHit"), cacheHit},
        {QStringLiteral("frameNumber"), frameNumber},
        {QStringLiteral("attempts"), attempts},
        {QStringLiteral("paintedLayerCount"), paintedLayerCount},
        {QStringLiteral("unsupportedLayerCount"), unsupportedLayerCount},
        {QStringLiteral("backend"), backend},
        {QStringLiteral("outputFile"), outputFile},
        {QStringLiteral("errorMessage"), errorMessage}
    };
    QJsonArray painted;
    for (const auto& kind : paintedLayerKinds) painted.append(kind);
    QJsonArray unsupported;
    for (const auto& kind : unsupportedLayerKinds) unsupported.append(kind);
    object.insert(QStringLiteral("paintedLayerKinds"), painted);
    object.insert(QStringLiteral("unsupportedLayerKinds"), unsupported);
    return object;
}

QJsonObject FrameRangeRenderResult::toJson() const
{
    QJsonObject object{
        {QStringLiteral("ok"), ok},
        {QStringLiteral("canceled"), canceled},
        {QStringLiteral("firstFrame"), firstFrame},
        {QStringLiteral("lastFrame"), lastFrame},
        {QStringLiteral("framesRendered"), framesRendered},
        {QStringLiteral("paintedLayerCount"), paintedLayerCount},
        {QStringLiteral("unsupportedLayerCount"), unsupportedLayerCount},
        {QStringLiteral("errorMessage"), errorMessage}
    };
    QJsonArray files;
    for (const auto& path : outputFiles) files.append(path);
    object.insert(QStringLiteral("outputFiles"), files);
    return object;
}

static bool renderWithBlenderCycles(const ExternalRenderJobSchema& job, QString* errorMessage)
{
    const QString blender = qEnvironmentVariable("ARTIFACT_BLENDER_EXECUTABLE", QStringLiteral("blender"));
    const QString script = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("artifact_blender_cycles.py"));
    if (!QFileInfo::exists(script)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Blender Cycles adapter script not found: %1").arg(script);
        }
        return false;
    }

    const QString jobPath = job.raw.value(QStringLiteral("diagnostics"))
        .toObject().value(QStringLiteral("jobFile")).toString();
    if (jobPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cycles backend requires diagnostics.jobFile");
        }
        return false;
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(blender, {
        QStringLiteral("--background"), QStringLiteral("--python-exit-code"), QStringLiteral("1"),
        QStringLiteral("--python"), script,
        QStringLiteral("--"), QStringLiteral("--job"), jobPath
    });
    if (!process.waitForStarted(10000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to start Blender: %1").arg(blender);
        }
        return false;
    }
    while (!process.waitForFinished(200)) {
        if (!job.cancelFile.trimmed().isEmpty() && QFileInfo::exists(job.cancelFile)) {
            process.terminate();
            if (!process.waitForFinished(3000)) {
                process.kill();
                process.waitForFinished(3000);
            }
            if (errorMessage) {
                *errorMessage = QStringLiteral("Blender Cycles rendering cancelled");
            }
            return false;
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Blender Cycles exited with code %1").arg(process.exitCode());
        }
        return false;
    }
    return true;
}

static QString frameFileName(const ExternalRenderJobSchema& job, int frameNumber)
{
    const QString safeName = job.compositionName.trimmed().isEmpty()
        ? QStringLiteral("render")
        : job.compositionName.trimmed().simplified().replace(' ', '_');
    return QStringLiteral("%1_%2.png").arg(safeName).arg(frameNumber, 4, 10, QChar('0'));
}

static QImage buildDiagnosticFrame(const ExternalRenderJobSchema& job, int frameNumber, int progress)
{
    const int width = qMax(16, job.width);
    const int height = qMax(16, job.height);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(24, 28, 36));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(QRect(0, 0, width, height), QColor(36, 42, 54));
    painter.setBrush(QColor(90, 140, 220, 220));
    painter.setPen(Qt::NoPen);
    const int offset = (frameNumber - job.frameStart) % qMax(1, width / 3);
    painter.drawRoundedRect(QRect(width / 10 + offset / 2, height / 6, width * 4 / 5, height * 2 / 3), 18, 18);

    painter.setPen(QColor(245, 245, 245));
    painter.drawText(QRect(0, 0, width, height / 4), Qt::AlignCenter, QStringLiteral("ArtifactRenderer"));
    painter.drawText(QRect(0, height / 4, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("%1").arg(job.compositionName));
    painter.drawText(QRect(0, height / 2, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("Frame %1 / %2").arg(frameNumber).arg(job.frameEnd - 1));
    painter.drawText(QRect(0, (height * 3) / 4, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("Progress %1%").arg(progress));
    return image;
}

static QImage buildDiagnosticFrame(const RenderJobSummary& summary, int frameNumber, int progress)
{
    const int width = qMax(16, summary.outputWidth);
    const int height = qMax(16, summary.outputHeight);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(36, 42, 54));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(90, 140, 220, 220));
    painter.setPen(Qt::NoPen);
    const int offset = (frameNumber - summary.frameStart) % qMax(1, width / 3);
    painter.drawRoundedRect(QRect(width / 10 + offset / 2, height / 6,
                                  width * 4 / 5, height * 2 / 3), 18, 18);
    painter.setPen(QColor(245, 245, 245));
    painter.drawText(QRect(0, 0, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("ArtifactRenderer"));
    painter.drawText(QRect(0, height / 4, width, height / 4), Qt::AlignCenter,
                     summary.compositionName);
    painter.drawText(QRect(0, height / 2, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("Frame %1 / %2").arg(frameNumber).arg(summary.frameEnd - 1));
    painter.drawText(QRect(0, (height * 3) / 4, width, height / 4), Qt::AlignCenter,
                     QStringLiteral("Progress %1%").arg(progress));
    return image;
}

static bool writePng(const QString& path, const QImage& image, QString* errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!image.save(path, "PNG")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write PNG: %1").arg(path);
        }
        return false;
    }
    return true;
}

static bool writeJsonAtomic(const QString& path, const QJsonObject& object, QString* errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open JSON file: %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit JSON file: %1").arg(path);
        }
        return false;
    }
    return true;
}

static bool appendEventJson(const QString& path, const QJsonObject& object, QString* errorMessage)
{
    if (path.trimmed().isEmpty()) {
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open event log: %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.write("\n");
    if (!file.flush()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to flush event log: %1").arg(path);
        }
        return false;
    }
    return true;
}

QString ExternalFrameRenderer::resolveOutputFile(const RenderJobSummary& summary,
                                                 int frameNumber, int frameCount)
{
    if (frameCount <= 1) return summary.outputPath;
    const QString base = summary.compositionName.trimmed().isEmpty()
        ? QStringLiteral("render")
        : summary.compositionName.trimmed().simplified().replace(' ', '_');
    const QString extension = QStringLiteral("png");
    return QDir(summary.outputPath).filePath(
        QStringLiteral("%1_%2.%3").arg(base).arg(frameNumber, 4, 10, QChar('0')).arg(extension));
}

FrameRenderResult ExternalFrameRenderer::renderPngFrame(
    const RenderJobSummary& summary, const RenderExecutionOptions& options,
    int frameNumber, int frameCount)
{
    FrameRenderResult result;
    result.frameNumber = frameNumber;
    result.backend = options.backend.trimmed().isEmpty() ? summary.backend : options.backend;
    const QString backend = result.backend.trimmed().toLower();
    if (backend != QStringLiteral("auto") && backend != QStringLiteral("diagnostic") &&
        backend != QStringLiteral("software")) {
        result.errorMessage = QStringLiteral("Unsupported PNG backend: %1").arg(result.backend);
        return result;
    }
    result.outputFile = resolveOutputFile(summary, frameNumber, frameCount);
    if (summary.outputWidth <= 0 || summary.outputHeight <= 0 ||
        frameNumber < summary.frameStart || frameNumber >= summary.frameEnd) {
        result.errorMessage = QStringLiteral("Invalid frame or output dimensions");
        return result;
    }
    for (int attempt = 0; attempt <= qMax(0, options.retryCount); ++attempt) {
        result.attempts = attempt + 1;
        if (!options.cancelFile.trimmed().isEmpty() && QFileInfo::exists(options.cancelFile)) {
            result.canceled = true;
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }
        if (options.resumeExistingFrames && QFileInfo::exists(result.outputFile)) {
            result.ok = true;
            result.cacheHit = true;
            return result;
        }
        if (writePng(result.outputFile,
                     buildDiagnosticFrame(summary, frameNumber,
                                          frameCount > 1
                                              ? ((frameNumber - summary.frameStart) * 100) /
                                                    qMax(1, frameCount)
                                              : 100),
                     &result.errorMessage)) {
            result.ok = true;
            result.paintedLayerCount = summary.layerCount;
            return result;
        }
    }
    return result;
}

FrameRenderResult ExternalFrameRenderer::renderFirstPngFrame(const RenderJobSummary& summary)
{
    RenderExecutionOptions options;
    options.backend = summary.backend;
    options.cancelFile = summary.cancelFile;
    options.retryCount = summary.retryCount;
    return renderPngFrame(summary, options, summary.frameStart, 1);
}

FrameRangeRenderResult ExternalFrameRenderer::renderPngFrameRange(
    const RenderJobSummary& summary, const RenderExecutionOptions& options,
    const std::function<void(const FrameRenderResult&, int, int)>& progressCallback)
{
    FrameRangeRenderResult result;
    result.firstFrame = summary.frameStart;
    result.lastFrame = summary.frameEnd - 1;
    if (summary.frameEnd <= summary.frameStart) {
        result.errorMessage = QStringLiteral("Invalid frame range");
        return result;
    }
    const int total = summary.frameEnd - summary.frameStart;
    for (int frame = summary.frameStart; frame < summary.frameEnd; ++frame) {
        FrameRenderResult frameResult = renderPngFrame(summary, options, frame, total);
        if (frameResult.ok) {
            ++result.framesRendered;
            result.outputFiles.push_back(frameResult.outputFile);
            result.paintedLayerCount = qMax(result.paintedLayerCount,
                                            frameResult.paintedLayerCount);
        } else {
            result.errorMessage = frameResult.errorMessage;
            result.canceled = frameResult.canceled;
            if (progressCallback) progressCallback(frameResult, frame - summary.frameStart + 1, total);
            return result;
        }
        if (progressCallback) progressCallback(frameResult, frame - summary.frameStart + 1, total);
    }
    result.ok = true;
    return result;
}

bool renderExternalJob(const ExternalRenderJobSchema& job, QString* errorMessage)
{
    if (!job.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid render job");
        }
        return false;
    }

    if (job.backend == QStringLiteral("cycles") || job.backend == QStringLiteral("blender-cycles")) {
        return renderWithBlenderCycles(job, errorMessage);
    }

    const QJsonObject summary{
        {QStringLiteral("jobId"), job.jobId},
        {QStringLiteral("composition"), job.compositionName},
        {QStringLiteral("transportedLayerCount"), job.raw.value(QStringLiteral("snapshot")).toObject()
                .value(QStringLiteral("layers")).toArray().size()},
        {QStringLiteral("frameStart"), job.frameStart},
        {QStringLiteral("frameEnd"), job.frameEnd},
        {QStringLiteral("componentSimulationBakePresent"),
         job.componentSimulationBakePresent},
        {QStringLiteral("componentSimulationBakeValid"),
         job.componentSimulationBakeValid},
        {QStringLiteral("componentSimulationBakeUsableForStart"),
         job.componentSimulationBakeUsableForStart},
        {QStringLiteral("componentSimulationBakeFrameCount"),
         job.componentSimulationBakeFrameCount},
        {QStringLiteral("outputPath"), job.outputPath}
    };

    if (!job.summaryFile.trimmed().isEmpty()) {
        writeJsonAtomic(job.summaryFile, summary, nullptr);
    }

    auto appendEvent = [&](const QString& eventName, int progress) {
        QJsonObject event{
            {QStringLiteral("event"), eventName},
            {QStringLiteral("jobId"), job.jobId},
            {QStringLiteral("progress"), progress},
            {QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
        };
        if (eventName == QStringLiteral("renderStarted")) {
            event.insert(QStringLiteral("componentSimulationBakePresent"),
                         job.componentSimulationBakePresent);
            event.insert(QStringLiteral("componentSimulationBakeValid"),
                         job.componentSimulationBakeValid);
            event.insert(QStringLiteral("componentSimulationBakeUsableForStart"),
                         job.componentSimulationBakeUsableForStart);
            event.insert(QStringLiteral("componentSimulationBakeFrameCount"),
                         job.componentSimulationBakeFrameCount);
        }
        appendEventJson(job.eventLogFile, event, nullptr);
    };

    const bool sequence = job.isSequenceOutput();
    appendEvent(QStringLiteral("renderStarted"), 0);
    if (sequence) {
        QDir().mkpath(job.outputPath);
        const int totalFrames = qMax(1, job.frameEnd - job.frameStart);
        for (int frame = job.frameStart; frame < job.frameEnd; ++frame) {
            if (!job.cancelFile.trimmed().isEmpty() && QFileInfo::exists(job.cancelFile)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Cancelled");
                }
                return false;
            }
            const int progress = ((frame - job.frameStart) * 100) / totalFrames;
            appendEvent(QStringLiteral("renderProgress"), progress);
            const QImage image = buildDiagnosticFrame(job, frame, progress);
            const QString path = QDir(job.outputPath).filePath(frameFileName(job, frame));
            if (!writePng(path, image, errorMessage)) {
                return false;
            }
        }
    } else {
        const int frame = job.frameStart;
        appendEvent(QStringLiteral("renderProgress"), 50);
        const QImage image = buildDiagnosticFrame(job, frame, 50);
        if (!writePng(job.outputPath, image, errorMessage)) {
            return false;
        }
    }

    appendEvent(QStringLiteral("renderCompleted"), 100);
    return true;
}

} // namespace ArtifactRenderer
