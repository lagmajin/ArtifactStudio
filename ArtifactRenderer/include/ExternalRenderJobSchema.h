#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ArtifactRenderer {

struct RenderJobSummary {
    int version = 0;
    QString jobId;
    QString mode;
    QString compositionId;
    QString compositionName;
    int frameStart = 0;
    int frameEnd = 0;
    double fps = 0.0;
    QString outputPath;
    QString outputFormat;
    int outputWidth = 0;
    int outputHeight = 0;
    int layerCount = 0;
    int effectCount = 0;
    int assetCount = 0;
    QJsonObject compositionSnapshot;
    QJsonArray layers;
    QString backend = QStringLiteral("auto");
    QString cacheMode = QStringLiteral("write");
    int retryCount = 0;
    QString cancelFile;
    QString summaryFile;
    QString eventLogFile;

    QJsonObject toJson() const;
};

class ExternalRenderJobSchema {
public:
    static constexpr int CurrentVersion = 1;

    static bool validate(const QJsonObject& root, QStringList* errors);
    static RenderJobSummary summarize(const QJsonObject& root);
};

} // namespace ArtifactRenderer
