#include "ExternalRenderJobSchema.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace ArtifactRenderer {
namespace {

bool requireObject(const QJsonObject& parent, const QString& key, QStringList* errors)
{
    if (!parent.value(key).isObject()) {
        if (errors) {
            errors->append(QStringLiteral("Missing object: %1").arg(key));
        }
        return false;
    }
    return true;
}

bool requireString(const QJsonObject& parent, const QString& key, QStringList* errors)
{
    const QString value = parent.value(key).toString().trimmed();
    if (value.isEmpty()) {
        if (errors) {
            errors->append(QStringLiteral("Missing string: %1").arg(key));
        }
        return false;
    }
    return true;
}

bool requirePositiveInt(const QJsonObject& parent, const QString& key, QStringList* errors)
{
    const int value = parent.value(key).toInt(0);
    if (value <= 0) {
        if (errors) {
            errors->append(QStringLiteral("Expected positive integer: %1").arg(key));
        }
        return false;
    }
    return true;
}

bool requireNumberGreaterThanZero(const QJsonObject& parent, const QString& key, QStringList* errors)
{
    const double value = parent.value(key).toDouble(0.0);
    if (value <= 0.0) {
        if (errors) {
            errors->append(QStringLiteral("Expected positive number: %1").arg(key));
        }
        return false;
    }
    return true;
}

int arraySize(const QJsonObject& parent, const QString& key)
{
    const QJsonValue value = parent.value(key);
    return value.isArray() ? value.toArray().size() : 0;
}

} // namespace

QJsonObject RenderJobSummary::toJson() const
{
    QJsonObject composition;
    composition.insert(QStringLiteral("id"), compositionId);
    composition.insert(QStringLiteral("name"), compositionName);
    composition.insert(QStringLiteral("frameStart"), frameStart);
    composition.insert(QStringLiteral("frameEnd"), frameEnd);
    composition.insert(QStringLiteral("fps"), fps);

    QJsonObject output;
    output.insert(QStringLiteral("path"), outputPath);
    output.insert(QStringLiteral("format"), outputFormat);
    output.insert(QStringLiteral("width"), outputWidth);
    output.insert(QStringLiteral("height"), outputHeight);

    QJsonObject counts;
    counts.insert(QStringLiteral("layers"), layerCount);
    counts.insert(QStringLiteral("effects"), effectCount);
    counts.insert(QStringLiteral("assets"), assetCount);
    counts.insert(QStringLiteral("transportedLayers"), layers.size());

    QJsonObject root;
    root.insert(QStringLiteral("version"), version);
    root.insert(QStringLiteral("jobId"), jobId);
    root.insert(QStringLiteral("mode"), mode);
    root.insert(QStringLiteral("composition"), composition);
    root.insert(QStringLiteral("output"), output);
    root.insert(QStringLiteral("snapshotCounts"), counts);
    root.insert(QStringLiteral("backend"), backend);
    root.insert(QStringLiteral("cacheMode"), cacheMode);
    root.insert(QStringLiteral("retryCount"), retryCount);
    if (!cancelFile.isEmpty()) {
        root.insert(QStringLiteral("cancelFile"), cancelFile);
    }
    if (!summaryFile.isEmpty()) {
        root.insert(QStringLiteral("summaryFile"), summaryFile);
    }
    if (!eventLogFile.isEmpty()) {
        root.insert(QStringLiteral("eventLogFile"), eventLogFile);
    }
    return root;
}

bool ExternalRenderJobSchema::validate(const QJsonObject& root, QStringList* errors)
{
    bool ok = true;

    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != CurrentVersion) {
        ok = false;
        if (errors) {
            errors->append(QStringLiteral("Unsupported job version: %1").arg(version));
        }
    }

    ok = requireString(root, QStringLiteral("jobId"), errors) && ok;
    ok = requireString(root, QStringLiteral("mode"), errors) && ok;
    ok = requireObject(root, QStringLiteral("composition"), errors) && ok;
    ok = requireObject(root, QStringLiteral("output"), errors) && ok;
    ok = requireObject(root, QStringLiteral("snapshot"), errors) && ok;

    const QJsonObject composition = root.value(QStringLiteral("composition")).toObject();
    ok = requireString(composition, QStringLiteral("id"), errors) && ok;
    ok = requireString(composition, QStringLiteral("name"), errors) && ok;
    ok = requireNumberGreaterThanZero(composition, QStringLiteral("fps"), errors) && ok;

    const int frameStart = composition.value(QStringLiteral("frameStart")).toInt(0);
    const int frameEnd = composition.value(QStringLiteral("frameEnd")).toInt(0);
    if (frameEnd <= frameStart) {
        ok = false;
        if (errors) {
            errors->append(QStringLiteral("Expected frameEnd to be greater than frameStart"));
        }
    }

    const QJsonObject output = root.value(QStringLiteral("output")).toObject();
    ok = requireString(output, QStringLiteral("path"), errors) && ok;
    ok = requireString(output, QStringLiteral("format"), errors) && ok;
    ok = requirePositiveInt(output, QStringLiteral("width"), errors) && ok;
    ok = requirePositiveInt(output, QStringLiteral("height"), errors) && ok;

    return ok;
}

RenderJobSummary ExternalRenderJobSchema::summarize(const QJsonObject& root)
{
    const QJsonObject composition = root.value(QStringLiteral("composition")).toObject();
    const QJsonObject output = root.value(QStringLiteral("output")).toObject();
    const QJsonObject quality = root.value(QStringLiteral("quality")).toObject();
    const QJsonObject diagnostics = root.value(QStringLiteral("diagnostics")).toObject();
    const QJsonObject snapshot = root.value(QStringLiteral("snapshot")).toObject();

    RenderJobSummary summary;
    summary.version = root.value(QStringLiteral("version")).toInt(0);
    summary.jobId = root.value(QStringLiteral("jobId")).toString();
    summary.mode = root.value(QStringLiteral("mode")).toString();
    summary.compositionId = composition.value(QStringLiteral("id")).toString();
    summary.compositionName = composition.value(QStringLiteral("name")).toString();
    summary.frameStart = composition.value(QStringLiteral("frameStart")).toInt(0);
    summary.frameEnd = composition.value(QStringLiteral("frameEnd")).toInt(0);
    summary.fps = composition.value(QStringLiteral("fps")).toDouble(0.0);
    summary.outputPath = output.value(QStringLiteral("path")).toString();
    summary.outputFormat = output.value(QStringLiteral("format")).toString();
    summary.outputWidth = output.value(QStringLiteral("width")).toInt(0);
    summary.outputHeight = output.value(QStringLiteral("height")).toInt(0);
    summary.layerCount = arraySize(snapshot, QStringLiteral("layers"));
    summary.effectCount = arraySize(snapshot, QStringLiteral("effects"));
    summary.assetCount = arraySize(snapshot, QStringLiteral("assets"));
    summary.layers = snapshot.value(QStringLiteral("layers")).toArray();
    summary.compositionSnapshot = snapshot.value(QStringLiteral("composition")).toObject();
    summary.backend = quality.value(QStringLiteral("backend")).toString(QStringLiteral("auto")).trimmed().toLower();
    summary.cacheMode = diagnostics.value(QStringLiteral("cacheMode")).toString(QStringLiteral("write")).trimmed().toLower();
    summary.retryCount = std::max(0, diagnostics.value(QStringLiteral("retryCount")).toInt(0));
    summary.cancelFile = diagnostics.value(QStringLiteral("cancelFile")).toString();
    summary.summaryFile = diagnostics.value(QStringLiteral("summaryFile")).toString();
    summary.eventLogFile = diagnostics.value(QStringLiteral("eventLogFile")).toString();
    return summary;
}

} // namespace ArtifactRenderer
