#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ArtifactRenderer {

struct ExternalRenderJobSchema {
    int version = 0;
    QString jobId;
    QString mode;
    QString compositionId;
    QString compositionName;
    int frameStart = 0;
    int frameEnd = 0;
    double fps = 30.0;
    QString outputPath;
    QString outputFormat;
    QString backend = QStringLiteral("diagnostic");
    int width = 0;
    int height = 0;
    QString summaryFile;
    QString eventLogFile;
    QString cancelFile;
    QJsonObject componentSimulationBake;
    bool componentSimulationBakePresent = false;
    bool componentSimulationBakeValid = true;
    bool componentSimulationBakeUsableForStart = false;
    int componentSimulationBakeFrameCount = 0;
    QJsonObject raw;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isSequenceOutput() const;
    [[nodiscard]] QJsonObject toSummaryJson() const;
};

[[nodiscard]] ExternalRenderJobSchema parseExternalRenderJob(const QJsonObject& object, QString* errorMessage);

// Compatibility payload used by the reusable diagnostic-frame helper.  New
// external renderer jobs use ExternalRenderJobSchema above.
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

} // namespace ArtifactRenderer
