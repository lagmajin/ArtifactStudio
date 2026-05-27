#include "ExternalRenderJobSchema.h"
#include "ExternalFrameRenderer.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <cstdio>

namespace {

enum ExitCode {
    Ok = 0,
    InvalidArguments = 2,
    FileReadFailed = 3,
    JsonParseFailed = 4,
    SchemaValidationFailed = 5,
    RenderFailed = 6,
    RenderCanceled = 7
};

void writeLine(FILE* stream, const QString& line)
{
    QTextStream out(stream);
    out << line << Qt::endl;
}

void writeJsonLine(FILE* stream, const QJsonObject& object)
{
    writeLine(stream, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

bool ensureParentDirectory(const QString& filePath, QString* errorMessage)
{
    const QFileInfo info(filePath);
    QDir dir = info.dir();
    if (dir.exists()) {
        return true;
    }
    if (dir.mkpath(QStringLiteral("."))) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Failed to create directory: %1").arg(dir.absolutePath());
    }
    return false;
}

bool appendJsonLine(const QString& filePath, const QJsonObject& object, QString* errorMessage)
{
    if (filePath.trimmed().isEmpty()) {
        return true;
    }
    if (!ensureParentDirectory(filePath, errorMessage)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open event log: %1").arg(filePath);
        }
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.write("\n");
    return true;
}

bool writeJsonFile(const QString& filePath, const QJsonObject& object, QString* errorMessage)
{
    if (filePath.trimmed().isEmpty()) {
        return true;
    }
    if (!ensureParentDirectory(filePath, errorMessage)) {
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open JSON output: %1").arg(filePath);
        }
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit JSON output: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("artifact-renderer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Artifact external renderer job runner."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption jobOption(
        {QStringLiteral("j"), QStringLiteral("job")},
        QStringLiteral("Path to an external render job JSON file."),
        QStringLiteral("job.json"));
    const QCommandLineOption validateOnlyOption(
        QStringLiteral("validate-only"),
        QStringLiteral("Validate the job and exit without rendering."));
    const QCommandLineOption dumpSummaryOption(
        QStringLiteral("dump-summary"),
        QStringLiteral("Write a compact job summary JSON to stdout."));
    const QCommandLineOption cancelFileOption(
        QStringLiteral("cancel-file"),
        QStringLiteral("Cancel rendering when this sentinel file exists."),
        QStringLiteral("path"));
    const QCommandLineOption backendOption(
        QStringLiteral("backend"),
        QStringLiteral("Renderer backend selector. Phase 4 supports auto/cpu/diagnostic."),
        QStringLiteral("name"));
    const QCommandLineOption resumeOption(
        QStringLiteral("resume"),
        QStringLiteral("Skip existing output frames instead of overwriting them."));
    const QCommandLineOption retryCountOption(
        QStringLiteral("retry-count"),
        QStringLiteral("Retry failed frame writes this many times."),
        QStringLiteral("count"));
    const QCommandLineOption summaryFileOption(
        QStringLiteral("summary-file"),
        QStringLiteral("Write final render result JSON to this path."),
        QStringLiteral("path"));
    const QCommandLineOption eventLogOption(
        QStringLiteral("event-log"),
        QStringLiteral("Mirror render events to this JSON Lines file."),
        QStringLiteral("path"));

    parser.addOption(jobOption);
    parser.addOption(validateOnlyOption);
    parser.addOption(dumpSummaryOption);
    parser.addOption(cancelFileOption);
    parser.addOption(backendOption);
    parser.addOption(resumeOption);
    parser.addOption(retryCountOption);
    parser.addOption(summaryFileOption);
    parser.addOption(eventLogOption);

    if (!parser.parse(QCoreApplication::arguments())) {
        writeLine(stderr, parser.errorText());
        return InvalidArguments;
    }
    parser.process(app);

    const QString jobPath = parser.value(jobOption).trimmed();
    if (jobPath.isEmpty()) {
        writeLine(stderr, QStringLiteral("Missing required option: --job"));
        return InvalidArguments;
    }

    QFile file(jobPath);
    if (!file.open(QIODevice::ReadOnly)) {
        writeLine(stderr, QStringLiteral("Failed to open job file: %1").arg(jobPath));
        return FileReadFailed;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        writeLine(stderr, QStringLiteral("Failed to parse job JSON: %1").arg(parseError.errorString()));
        return JsonParseFailed;
    }

    QStringList validationErrors;
    const QJsonObject root = document.object();
    if (!ArtifactRenderer::ExternalRenderJobSchema::validate(root, &validationErrors)) {
        writeLine(stderr, QStringLiteral("Invalid render job:"));
        for (const QString& error : validationErrors) {
            writeLine(stderr, QStringLiteral("  - %1").arg(error));
        }
        return SchemaValidationFailed;
    }

    const ArtifactRenderer::RenderJobSummary summary =
        ArtifactRenderer::ExternalRenderJobSchema::summarize(root);
    const QString cancelFile = parser.isSet(cancelFileOption)
        ? parser.value(cancelFileOption).trimmed()
        : summary.cancelFile;
    const QString backend = parser.isSet(backendOption)
        ? parser.value(backendOption).trimmed().toLower()
        : summary.backend;
    const bool resumeExistingFrames = parser.isSet(resumeOption)
        || summary.cacheMode == QStringLiteral("resume");
    bool retryCountOk = true;
    const int retryCount = parser.isSet(retryCountOption)
        ? std::max(0, parser.value(retryCountOption).toInt(&retryCountOk))
        : summary.retryCount;
    if (!retryCountOk) {
        writeLine(stderr, QStringLiteral("Invalid retry count: %1").arg(parser.value(retryCountOption)));
        return InvalidArguments;
    }
    const QString summaryFile = parser.isSet(summaryFileOption)
        ? parser.value(summaryFileOption).trimmed()
        : summary.summaryFile;
    const QString eventLogFile = parser.isSet(eventLogOption)
        ? parser.value(eventLogOption).trimmed()
        : summary.eventLogFile;
    if (!eventLogFile.isEmpty()) {
        QString eventLogError;
        if (!ensureParentDirectory(eventLogFile, &eventLogError)) {
            writeLine(stderr, eventLogError);
            return InvalidArguments;
        }
        QFile eventLog(eventLogFile);
        if (!eventLog.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            writeLine(stderr, QStringLiteral("Failed to initialize event log: %1").arg(eventLogFile));
            return InvalidArguments;
        }
    }

    if (parser.isSet(dumpSummaryOption) || parser.isSet(validateOnlyOption)) {
        const QJsonDocument summaryDocument(summary.toJson());
        writeLine(stdout, QString::fromUtf8(summaryDocument.toJson(QJsonDocument::Compact)));
        return Ok;
    }

    auto emitEvent = [&](FILE* stream, const QJsonObject& event) {
        writeJsonLine(stream, event);
        QString eventLogError;
        if (!appendJsonLine(eventLogFile, event, &eventLogError)) {
            writeLine(stderr, eventLogError);
        }
    };

    emitEvent(stdout, QJsonObject{
        {QStringLiteral("event"), QStringLiteral("renderStarted")},
        {QStringLiteral("jobId"), summary.jobId},
        {QStringLiteral("backend"), backend.isEmpty() ? QStringLiteral("auto") : backend},
        {QStringLiteral("resumeExistingFrames"), resumeExistingFrames},
        {QStringLiteral("retryCount"), retryCount},
        {QStringLiteral("frameStart"), summary.frameStart},
        {QStringLiteral("frameEnd"), summary.frameEnd},
        {QStringLiteral("totalFrames"), std::max(1, summary.frameEnd - summary.frameStart)}
    });

    ArtifactRenderer::RenderExecutionOptions renderOptions;
    renderOptions.backend = backend;
    renderOptions.cancelFile = cancelFile;
    renderOptions.resumeExistingFrames = resumeExistingFrames;
    renderOptions.retryCount = retryCount;
    renderOptions.sourceBaseDirectory = QFileInfo(jobPath).absolutePath();

    const ArtifactRenderer::FrameRangeRenderResult renderResult =
        ArtifactRenderer::ExternalFrameRenderer::renderPngFrameRange(
            summary,
            renderOptions,
            [&](const ArtifactRenderer::FrameRenderResult& frameResult, int framesRendered, int totalFrames) {
                const int progress = totalFrames > 0
                    ? static_cast<int>((static_cast<double>(framesRendered) / static_cast<double>(totalFrames)) * 100.0)
                    : 100;
                emitEvent(stdout, QJsonObject{
                    {QStringLiteral("event"), QStringLiteral("renderProgress")},
                    {QStringLiteral("jobId"), summary.jobId},
                    {QStringLiteral("frameNumber"), frameResult.frameNumber},
                    {QStringLiteral("framesRendered"), framesRendered},
                    {QStringLiteral("totalFrames"), totalFrames},
                    {QStringLiteral("progress"), progress},
                    {QStringLiteral("backend"), frameResult.backend},
                    {QStringLiteral("cacheHit"), frameResult.cacheHit},
                    {QStringLiteral("attempts"), frameResult.attempts},
                    {QStringLiteral("paintedLayerCount"), frameResult.paintedLayerCount},
                    {QStringLiteral("unsupportedLayerCount"), frameResult.unsupportedLayerCount},
                    {QStringLiteral("outputFile"), frameResult.outputFile}
                });
            });
    if (!renderResult.ok) {
        const int exitCode = renderResult.canceled ? RenderCanceled : RenderFailed;
        const QJsonObject failedEvent{
            {QStringLiteral("event"), renderResult.canceled ? QStringLiteral("renderCanceled") : QStringLiteral("renderFailed")},
            {QStringLiteral("jobId"), summary.jobId},
            {QStringLiteral("exitCode"), exitCode},
            {QStringLiteral("result"), renderResult.toJson()}
        };
        emitEvent(stderr, failedEvent);
        QString summaryError;
        if (!writeJsonFile(summaryFile, failedEvent, &summaryError)) {
            writeLine(stderr, summaryError);
        }
        return exitCode;
    }

    const QJsonObject completedEvent{
        {QStringLiteral("event"), QStringLiteral("renderCompleted")},
        {QStringLiteral("jobId"), summary.jobId},
        {QStringLiteral("exitCode"), Ok},
        {QStringLiteral("result"), renderResult.toJson()}
    };
    QString summaryError;
    if (!writeJsonFile(summaryFile, completedEvent, &summaryError)) {
        const QJsonObject failedEvent{
            {QStringLiteral("event"), QStringLiteral("renderFailed")},
            {QStringLiteral("jobId"), summary.jobId},
            {QStringLiteral("exitCode"), RenderFailed},
            {QStringLiteral("error"), summaryError},
            {QStringLiteral("result"), renderResult.toJson()}
        };
        emitEvent(stderr, failedEvent);
        return RenderFailed;
    }

    emitEvent(stdout, completedEvent);

    return Ok;
}
