#include "ExternalRenderJobSchema.h"

#include <QJsonDocument>

namespace ArtifactRenderer {

static QString stringValue(const QJsonObject& object, const char* key)
{
    return object.value(QString::fromLatin1(key)).toString();
}

static int intValue(const QJsonObject& object, const char* key, int fallback = 0)
{
    return object.value(QString::fromLatin1(key)).toInt(fallback);
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

    if (!job.isValid() && errorMessage) {
        *errorMessage = QStringLiteral("Invalid external render job schema");
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
           frameEnd > frameStart;
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
        {QStringLiteral("transportedLayerCount"), transportedLayerCount}
    };
}

} // namespace ArtifactRenderer
