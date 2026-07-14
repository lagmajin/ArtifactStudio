#include "ExternalRenderJobSchema.h"

#include <QJsonDocument>
#include <QSet>

#include <cmath>

namespace ArtifactRenderer {

static QString stringValue(const QJsonObject& object, const char* key)
{
    return object.value(QString::fromLatin1(key)).toString();
}

static int intValue(const QJsonObject& object, const char* key, int fallback = 0)
{
    return object.value(QString::fromLatin1(key)).toInt(fallback);
}

static bool isSha256Hex(const QString& value)
{
    if (value.size() != 64) {
        return false;
    }
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (!((code >= '0' && code <= '9') ||
              (code >= 'a' && code <= 'f') ||
              (code >= 'A' && code <= 'F'))) {
            return false;
        }
    }
    return true;
}

static bool validateComponentSimulationBake(
    ExternalRenderJobSchema& job, QString* errorMessage)
{
    if (!job.componentSimulationBakePresent) {
        return true;
    }
    const QJsonObject& bake = job.componentSimulationBake;
    const QJsonArray frames = bake.value(QStringLiteral("frames")).toArray();
    if (bake.value(QStringLiteral("version")).toInt() != 1 ||
        bake.value(QStringLiteral("compositionId")).toString() !=
            job.compositionId ||
        !isSha256Hex(
            bake.value(QStringLiteral("descriptorHash")).toString()) ||
        std::abs(bake.value(QStringLiteral("frameRate")).toDouble() -
                 job.fps) > 0.000001 ||
        frames.isEmpty() || frames.size() > 120) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Invalid layer component simulation bake header");
        }
        return false;
    }

    QSet<qlonglong> frameNumbers;
    constexpr qsizetype kMaxLayersPerFrame = 100000;
    for (const QJsonValue& frameValue : frames) {
        if (!frameValue.isObject()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Invalid layer component simulation bake frame");
            }
            return false;
        }
        const QJsonObject frame = frameValue.toObject();
        bool ok = false;
        const qlonglong frameNumber =
            frame.value(QStringLiteral("frame")).toString().toLongLong(&ok);
        const QJsonValue layersValue = frame.value(QStringLiteral("layers"));
        if (!ok || frameNumbers.contains(frameNumber) ||
            !layersValue.isArray() ||
            layersValue.toArray().size() > kMaxLayersPerFrame) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "Invalid layer component simulation bake frame payload");
            }
            return false;
        }
        frameNumbers.insert(frameNumber);
    }
    bool currentOk = false;
    const qlonglong currentFrame =
        bake.value(QStringLiteral("currentFrame")).toString()
            .toLongLong(&currentOk);
    if (!currentOk || !frameNumbers.contains(currentFrame)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Invalid layer component simulation bake current frame");
        }
        return false;
    }
    job.componentSimulationBakeFrameCount = frames.size();
    job.componentSimulationBakeUsableForStart =
        frameNumbers.contains(static_cast<qlonglong>(job.frameStart)) ||
        frameNumbers.contains(static_cast<qlonglong>(job.frameStart) - 1);
    return true;
}

ExternalRenderJobSchema parseExternalRenderJob(const QJsonObject& object, QString* errorMessage)
{
    ExternalRenderJobSchema job;
    job.raw = object;
    job.version = intValue(object, "version", 0);
    job.jobId = stringValue(object, "jobId");
    job.mode = stringValue(object, "mode");

    const QJsonObject composition = object.value(QStringLiteral("composition")).toObject();
    job.compositionId = stringValue(composition, "id");
    job.compositionName = stringValue(composition, "name");
    job.frameStart = intValue(composition, "frameStart", 0);
    job.frameEnd = intValue(composition, "frameEnd", 0);
    job.fps = composition.value(QStringLiteral("fps")).toDouble(30.0);

    const QJsonObject output = object.value(QStringLiteral("output")).toObject();
    job.outputPath = stringValue(output, "path");
    job.outputFormat = stringValue(output, "format");
    job.width = intValue(output, "width", 0);
    job.height = intValue(output, "height", 0);

    const QJsonObject diagnostics = object.value(QStringLiteral("diagnostics")).toObject();
    job.summaryFile = stringValue(diagnostics, "summaryFile");
    job.eventLogFile = stringValue(diagnostics, "eventLogFile");
    job.cancelFile = stringValue(diagnostics, "cancelFile");

    const QJsonObject snapshot =
        object.value(QStringLiteral("snapshot")).toObject();
    const QJsonObject compositionSnapshot =
        snapshot.value(QStringLiteral("composition")).toObject();
    const QJsonValue bakeValue = compositionSnapshot.value(
        QStringLiteral("layerComponentSimulationBake"));
    job.componentSimulationBakePresent = !bakeValue.isUndefined();
    if (job.componentSimulationBakePresent && bakeValue.isObject()) {
        job.componentSimulationBake = bakeValue.toObject();
    }
    job.componentSimulationBakeValid =
        (!job.componentSimulationBakePresent || bakeValue.isObject()) &&
        validateComponentSimulationBake(job, errorMessage);

    if (!job.isValid() && errorMessage) {
        if (errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Invalid external render job schema");
        }
    }
    return job;
}

bool ExternalRenderJobSchema::isValid() const
{
    return version > 0 &&
           !jobId.trimmed().isEmpty() &&
           !outputPath.trimmed().isEmpty() &&
           width > 0 &&
           height > 0 &&
           frameEnd > frameStart &&
           componentSimulationBakeValid;
}

bool ExternalRenderJobSchema::isSequenceOutput() const
{
    const QString fmt = outputFormat.trimmed().toLower();
    return fmt == QStringLiteral("png") ||
           fmt == QStringLiteral("exr") ||
           fmt == QStringLiteral("tiff") ||
           fmt == QStringLiteral("jpeg") ||
           fmt == QStringLiteral("jpg") ||
           fmt == QStringLiteral("bmp") ||
           fmt == QStringLiteral("webp");
}

QJsonObject ExternalRenderJobSchema::toSummaryJson() const
{
    const int transportedLayerCount = raw.value(QStringLiteral("snapshot"))
        .toObject()
        .value(QStringLiteral("layers"))
        .toArray()
        .size();
    return QJsonObject{
        {QStringLiteral("version"), version},
        {QStringLiteral("jobId"), jobId},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("compositionId"), compositionId},
        {QStringLiteral("compositionName"), compositionName},
        {QStringLiteral("frameStart"), frameStart},
        {QStringLiteral("frameEnd"), frameEnd},
        {QStringLiteral("fps"), fps},
        {QStringLiteral("outputPath"), outputPath},
        {QStringLiteral("outputFormat"), outputFormat},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
        {QStringLiteral("transportedLayerCount"), transportedLayerCount},
        {QStringLiteral("componentSimulationBakePresent"),
         componentSimulationBakePresent},
        {QStringLiteral("componentSimulationBakeValid"),
         componentSimulationBakeValid},
        {QStringLiteral("componentSimulationBakeUsableForStart"),
         componentSimulationBakeUsableForStart},
        {QStringLiteral("componentSimulationBakeFrameCount"),
         componentSimulationBakeFrameCount}
    };
}

} // namespace ArtifactRenderer
