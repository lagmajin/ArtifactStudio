#include "ExternalRenderJobSchema.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QTextStream>

namespace ArtifactRenderer {
bool renderExternalJob(const ExternalRenderJobSchema& job, QString* errorMessage);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    const int jobIndex = args.indexOf(QStringLiteral("--job"));
    const bool validateOnly = args.contains(QStringLiteral("--validate-only"));
    const bool dumpSummary = args.contains(QStringLiteral("--dump-summary"));
    if (jobIndex < 0 || jobIndex + 1 >= args.size()) {
        QTextStream(stderr) << "usage: artifact-renderer --job <job.json>\n";
        return 2;
    }

    QFile jobFile(args.at(jobIndex + 1));
    if (!jobFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "failed to open job file\n";
        return 3;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jobFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QTextStream(stderr) << "invalid job json\n";
        return 4;
    }

    QString errorMessage;
    const ArtifactRenderer::ExternalRenderJobSchema job = ArtifactRenderer::parseExternalRenderJob(doc.object(), &errorMessage);
    if (!job.isValid()) {
        QTextStream(stderr) << errorMessage << '\n';
        return 5;
    }

    if (dumpSummary) {
        QTextStream(stdout) << QString::fromUtf8(QJsonDocument(job.toSummaryJson()).toJson(QJsonDocument::Compact)) << '\n';
        return 0;
    }

    if (validateOnly) {
        QTextStream(stdout) << "ok\n";
        return 0;
    }

    if (!ArtifactRenderer::renderExternalJob(job, &errorMessage)) {
        QTextStream(stderr) << errorMessage << '\n';
        return 6;
    }

    return 0;
}
