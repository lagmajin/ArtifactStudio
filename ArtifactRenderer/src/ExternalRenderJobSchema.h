#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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

} // namespace ArtifactRenderer
