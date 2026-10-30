#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>

namespace {

QString workerPath()
{
    return qEnvironmentVariable("ARTIFACT_PROXY_WORKER_EXECUTABLE");
}

struct WorkerRunResult {
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    QByteArray standardOutput;
    QByteArray standardError;
};

WorkerRunResult runWorker(const QStringList& arguments,
                          const int timeoutMilliseconds = 15000)
{
    QProcess process;
    process.start(workerPath(), arguments);
    if (!process.waitForStarted(5000)) {
        return {};
    }
    if (!process.waitForFinished(timeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    return {process.exitCode(), process.exitStatus(),
            process.readAllStandardOutput(), process.readAllStandardError()};
}

QJsonObject firstJsonLine(const QByteArray& output)
{
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            return document.object();
        }
        return {};
    }
    return {};
}

QJsonObject jsonLineOfType(const QByteArray& output, const QString& type)
{
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray& line : lines) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("type")).toString() == type) {
                return object;
            }
        }
    }
    return {};
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QJsonObject requestBase(const QString& jobId,
                        const QString& sourcePath,
                        const QString& outputPath,
                        const QString& temporaryOutputPath)
{
    return {{QStringLiteral("protocolVersion"), 1},
            {QStringLiteral("jobId"), jobId},
            {QStringLiteral("sourcePath"), sourcePath},
            {QStringLiteral("outputPath"), outputPath},
            {QStringLiteral("temporaryOutputPath"), temporaryOutputPath},
            {QStringLiteral("scale"), 0.5},
            {QStringLiteral("backend"), QStringLiteral("ffmpeg")}};
}

TEST(ProxyWorkerProtocolTest, MissingRequestArgumentReturnsStructuredFailure)
{
    ASSERT_FALSE(workerPath().isEmpty());
    const WorkerRunResult result = runWorker({});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 1);
    const QJsonObject failure = firstJsonLine(result.standardOutput);
    EXPECT_EQ(failure.value(QStringLiteral("type")).toString(), QStringLiteral("failed"));
    EXPECT_TRUE(failure.value(QStringLiteral("jobId")).toString().isEmpty());
    EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
              QStringLiteral("Missing --request argument"));
}

TEST(ProxyWorkerProtocolTest, MalformedJsonReturnsStructuredFailure)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString requestPath = directory.filePath(QStringLiteral("request.json"));
    ASSERT_TRUE(writeBytes(requestPath, QByteArrayLiteral("{invalid")));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 1);
    const QJsonObject failure = firstJsonLine(result.standardOutput);
    EXPECT_EQ(failure.value(QStringLiteral("type")).toString(), QStringLiteral("failed"));
    EXPECT_TRUE(failure.value(QStringLiteral("jobId")).toString().isEmpty());
    EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
              QStringLiteral("Invalid request JSON"));
}

TEST(ProxyWorkerProtocolTest, InvalidBatchJobsEachFailAndRetainBatchSummary)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString requestPath = directory.filePath(QStringLiteral("batch.json"));
    const QJsonObject request{
        {QStringLiteral("protocolVersion"), 2},
        {QStringLiteral("jobs"), QJsonArray{
            QJsonObject{{QStringLiteral("jobId"), QStringLiteral("broken-job-a")}},
            QJsonObject{{QStringLiteral("jobId"), QStringLiteral("broken-job-b")}}}}};
    ASSERT_TRUE(writeBytes(requestPath,
        QJsonDocument(request).toJson(QJsonDocument::Compact)));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 0);
    const QList<QByteArray> lines = result.standardOutput.split('\n');
    QJsonObject failuresById;
    QJsonObject summary;
    QStringList eventTypes;
    QStringList failedJobIds;
    for (const QByteArray& line : lines) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject message = document.object();
        const QString type = message.value(QStringLiteral("type")).toString();
        eventTypes.push_back(type);
        if (type == QStringLiteral("failed")) {
            const QString jobId = message.value(QStringLiteral("jobId")).toString();
            failuresById.insert(jobId, message);
            failedJobIds.push_back(jobId);
        } else if (type == QStringLiteral("batchCompleted")) {
            summary = message;
        }
    }
    ASSERT_EQ(eventTypes.size(), 3);
    EXPECT_EQ(eventTypes, QStringList({QStringLiteral("failed"),
                                       QStringLiteral("failed"),
                                       QStringLiteral("batchCompleted")}));
    EXPECT_EQ(failedJobIds, QStringList({QStringLiteral("broken-job-a"),
                                         QStringLiteral("broken-job-b")}));
    ASSERT_EQ(failuresById.size(), 2);
    for (const QString& jobId : {QStringLiteral("broken-job-a"),
                                 QStringLiteral("broken-job-b")}) {
        const QJsonObject failure = failuresById.value(jobId).toObject();
        EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
                  QStringLiteral("Invalid proxy job request")) << jobId.toStdString();
    }
    EXPECT_EQ(summary.value(QStringLiteral("succeeded")).toInt(-1), 0);
    EXPECT_EQ(summary.value(QStringLiteral("total")).toInt(-1), 2);
    EXPECT_EQ(eventTypes.last(), QStringLiteral("batchCompleted"));
}

TEST(ProxyWorkerProtocolTest, PreCancelledRequestPreservesTemporaryOutput)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.bin"));
    const QString outputPath = directory.filePath(QStringLiteral("proxy.mp4"));
    const QString temporaryOutputPath = directory.filePath(
        QStringLiteral("proxy.partial.mp4"));
    const QString cancelPath = directory.filePath(QStringLiteral("cancel"));
    ASSERT_TRUE(writeBytes(sourcePath, QByteArrayLiteral("source placeholder")));
    ASSERT_TRUE(writeBytes(cancelPath, QByteArrayLiteral("cancel")));
    ASSERT_TRUE(writeBytes(temporaryOutputPath,
                           QByteArrayLiteral("preserve-before-cancel")));
    QJsonObject request = requestBase(QStringLiteral("cancelled-job"), sourcePath,
                                      outputPath, temporaryOutputPath);
    request.insert(QStringLiteral("cancelPath"), cancelPath);
    const QString requestPath = directory.filePath(QStringLiteral("cancel.json"));
    ASSERT_TRUE(writeBytes(requestPath,
        QJsonDocument(request).toJson(QJsonDocument::Compact)));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 1);
    const QJsonObject failure = firstJsonLine(result.standardOutput);
    EXPECT_EQ(failure.value(QStringLiteral("type")).toString(), QStringLiteral("failed"));
    EXPECT_EQ(failure.value(QStringLiteral("jobId")).toString(),
              QStringLiteral("cancelled-job"));
    EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
              QStringLiteral("Proxy job was cancelled before start"));
    QFile untouchedTemporaryOutput(temporaryOutputPath);
    ASSERT_TRUE(untouchedTemporaryOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(untouchedTemporaryOutput.readAll(),
              QByteArrayLiteral("preserve-before-cancel"));
    EXPECT_FALSE(QFileInfo::exists(outputPath));
}

TEST(ProxyWorkerProtocolTest,
     NativeGenerationFailurePreservesFinalOutputAndRemovesPartialOutput)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("corrupt-video.bin"));
    const QString outputPath = directory.filePath(QStringLiteral("proxy.mp4"));
    const QString temporaryOutputPath = directory.filePath(
        QStringLiteral("proxy.partial.mp4"));
    ASSERT_TRUE(writeBytes(sourcePath, QByteArrayLiteral("not a media container")));
    const QByteArray previousOutput = QByteArrayLiteral("previous valid proxy");
    ASSERT_TRUE(writeBytes(outputPath, previousOutput));
    ASSERT_TRUE(writeBytes(temporaryOutputPath,
                           QByteArrayLiteral("stale partial proxy")));

    QJsonObject request = requestBase(QStringLiteral("corrupt-video-job"),
                                      sourcePath, outputPath,
                                      temporaryOutputPath);
    request.insert(QStringLiteral("backend"), QStringLiteral("native"));
    const QString requestPath = directory.filePath(QStringLiteral("corrupt.json"));
    ASSERT_TRUE(writeBytes(requestPath,
        QJsonDocument(request).toJson(QJsonDocument::Compact)));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(jsonLineOfType(result.standardOutput,
                             QStringLiteral("started"))
                  .value(QStringLiteral("jobId")).toString(),
              QStringLiteral("corrupt-video-job"));
    EXPECT_EQ(jsonLineOfType(result.standardOutput,
                             QStringLiteral("progress"))
                  .value(QStringLiteral("jobId")).toString(),
              QStringLiteral("corrupt-video-job"));
    const QJsonObject failure = jsonLineOfType(
        result.standardOutput, QStringLiteral("failed"));
    EXPECT_EQ(failure.value(QStringLiteral("type")).toString(),
              QStringLiteral("failed"));
    EXPECT_EQ(failure.value(QStringLiteral("jobId")).toString(),
              QStringLiteral("corrupt-video-job"));
    EXPECT_TRUE(failure.value(QStringLiteral("reason")).toString().startsWith(
        QStringLiteral("FFmpeg C API proxy generation: ")));
    QFile preservedOutput(outputPath);
    ASSERT_TRUE(preservedOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(preservedOutput.readAll(), previousOutput);
    EXPECT_FALSE(QFileInfo::exists(temporaryOutputPath));
}

TEST(ProxyWorkerProtocolTest,
     NativeGenerationCompletesProtocolAndAtomicallyReplacesFinalOutput)
{
    ASSERT_FALSE(workerPath().isEmpty());
    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QString sourcePath = QDir::cleanPath(QDir(sourceDirectory).filePath(
        QStringLiteral("../ArtifactCore/fixtures/walkway-orbit-cc0.mp4")));
    ASSERT_TRUE(QFileInfo(sourcePath).isFile())
        << "The existing CC0 video fixture is required for worker success coverage";

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString outputPath = directory.filePath(QStringLiteral("proxy.mp4"));
    const QString temporaryOutputPath =
        directory.filePath(QStringLiteral("proxy.partial.mp4"));
    const QString backupOutputPath = outputPath +
        QStringLiteral(".native-success-job.previous");
    const QByteArray previousOutput = QByteArrayLiteral("previous valid proxy");
    ASSERT_TRUE(writeBytes(outputPath, previousOutput));
    ASSERT_TRUE(writeBytes(temporaryOutputPath,
                           QByteArrayLiteral("stale partial proxy")));

    QJsonObject request = requestBase(QStringLiteral("native-success-job"),
                                      sourcePath, outputPath,
                                      temporaryOutputPath);
    request.insert(QStringLiteral("backend"), QStringLiteral("native"));
    request.insert(QStringLiteral("scale"), 0.125);
    const QString requestPath = directory.filePath(QStringLiteral("success.json"));
    ASSERT_TRUE(writeBytes(requestPath,
        QJsonDocument(request).toJson(QJsonDocument::Compact)));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath}, 60000);
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    if (result.exitCode != 0) {
        const QJsonObject failure = jsonLineOfType(
            result.standardOutput, QStringLiteral("failed"));
        if (failure.value(QStringLiteral("reason")).toString().contains(
                QStringLiteral("Encoder not found"))) {
            GTEST_SKIP() << "This FFmpeg build has no native H.264 encoder";
        }
    }
    ASSERT_EQ(result.exitCode, 0)
        << result.standardError.toStdString()
        << result.standardOutput.toStdString();

    QStringList eventTypes;
    QList<double> progressFractions;
    const QList<QByteArray> lines = result.standardOutput.split('\n');
    for (const QByteArray& line : lines) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject message = document.object();
        if (message.value(QStringLiteral("jobId")).toString() !=
            QStringLiteral("native-success-job")) {
            continue;
        }
        const QString type = message.value(QStringLiteral("type")).toString();
        eventTypes.push_back(type);
        if (type == QStringLiteral("progress")) {
            progressFractions.push_back(
                message.value(QStringLiteral("fraction")).toDouble(-1.0));
        }
    }
    ASSERT_GE(eventTypes.size(), 4);
    EXPECT_EQ(eventTypes.first(), QStringLiteral("started"));
    EXPECT_EQ(eventTypes.last(), QStringLiteral("completed"));
    EXPECT_FALSE(eventTypes.contains(QStringLiteral("failed")));
    ASSERT_FALSE(progressFractions.isEmpty());
    EXPECT_DOUBLE_EQ(progressFractions.first(), 0.0);
    EXPECT_DOUBLE_EQ(progressFractions.last(), 1.0);
    for (qsizetype index = 1; index < progressFractions.size(); ++index) {
        EXPECT_LE(progressFractions[index - 1], progressFractions[index]);
    }

    const QJsonObject completed = jsonLineOfType(
        result.standardOutput, QStringLiteral("completed"));
    EXPECT_EQ(completed.value(QStringLiteral("outputPath")).toString(), outputPath);
    EXPECT_EQ(completed.value(QStringLiteral("backendRequested")).toString(),
              QStringLiteral("native"));
    EXPECT_EQ(completed.value(QStringLiteral("backend")).toString(),
              QStringLiteral("native"));
    const QString selectedEncoder =
        completed.value(QStringLiteral("encoder")).toString();
    ASSERT_FALSE(selectedEncoder.isEmpty());
    const bool selectedHardwareEncoder =
        selectedEncoder == QStringLiteral("h264_nvenc") ||
        selectedEncoder == QStringLiteral("h264_qsv") ||
        selectedEncoder == QStringLiteral("h264_amf") ||
        selectedEncoder == QStringLiteral("h264_mf");
    EXPECT_EQ(completed.value(QStringLiteral("hardwareAccelRequested")).toBool(),
              false);
    EXPECT_EQ(completed.value(QStringLiteral("hardwareEncoderUsed")).toBool(),
              selectedHardwareEncoder);
    const QJsonArray encoderCandidates =
        completed.value(QStringLiteral("encoderCandidates")).toArray();
    EXPECT_TRUE(encoderCandidates.contains(QJsonValue(selectedEncoder)));
    ASSERT_TRUE(QFileInfo(outputPath).isFile());
    const qint64 outputBytes = QFileInfo(outputPath).size();
    EXPECT_GT(outputBytes, 0);
    EXPECT_EQ(completed.value(QStringLiteral("outputBytes")).toInteger(-1),
              outputBytes);
    QFile generatedOutput(outputPath);
    ASSERT_TRUE(generatedOutput.open(QIODevice::ReadOnly));
    EXPECT_NE(generatedOutput.readAll(), previousOutput);
    EXPECT_FALSE(QFileInfo::exists(temporaryOutputPath));
    EXPECT_FALSE(QFileInfo::exists(backupOutputPath));
}

TEST(ProxyWorkerProtocolTest,
     InvalidSingleJobFieldsAreRejectedBeforeExistingOutputsAreTouched)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.bin"));
    ASSERT_TRUE(writeBytes(sourcePath, QByteArrayLiteral("source placeholder")));

    struct InvalidField final {
        QString name;
        QJsonValue value;
        bool removeSource = false;
    };
    const QList<InvalidField> invalidFields{
        {QStringLiteral("scale"), 0.0},
        {QStringLiteral("scale"), 1.01},
        {QStringLiteral("qualityPreset"), QStringLiteral("ultra")},
        {QStringLiteral("jobId"), QStringLiteral("")},
        {QStringLiteral("backend"), QStringLiteral("unsupported-backend")},
        {QStringLiteral("sourcePath"), QStringLiteral("missing-source.bin"), true}};

    int caseIndex = 0;
    for (const InvalidField& invalidField : invalidFields) {
        const QString outputPath = directory.filePath(
            QStringLiteral("proxy-%1.mp4").arg(caseIndex));
        const QString temporaryOutputPath = directory.filePath(
            QStringLiteral("proxy-%1.partial.mp4").arg(caseIndex));
        const QByteArray previousOutput = QByteArrayLiteral("keep-final-output");
        const QByteArray previousTemporary = QByteArrayLiteral("keep-temporary-output");
        ASSERT_TRUE(writeBytes(outputPath, previousOutput));
        ASSERT_TRUE(writeBytes(temporaryOutputPath, previousTemporary));

        QJsonObject request = requestBase(
            QStringLiteral("invalid-field-%1").arg(caseIndex), sourcePath,
            outputPath, temporaryOutputPath);
        request.insert(invalidField.name, invalidField.value);
        if (invalidField.removeSource) {
            request.insert(QStringLiteral("sourcePath"),
                           directory.filePath(invalidField.value.toString()));
        }
        const QString requestPath = directory.filePath(
            QStringLiteral("invalid-field-%1.json").arg(caseIndex));
        ASSERT_TRUE(writeBytes(requestPath,
            QJsonDocument(request).toJson(QJsonDocument::Compact)));

        const WorkerRunResult result = runWorker(
            {QStringLiteral("--request"), requestPath});
        ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
        EXPECT_EQ(result.exitCode, 1);
        const QJsonObject failure = firstJsonLine(result.standardOutput);
        EXPECT_EQ(failure.value(QStringLiteral("type")).toString(),
                  QStringLiteral("failed"));
        const QString expectedJobId = invalidField.name == QStringLiteral("jobId")
            ? QString()
            : QStringLiteral("invalid-field-%1").arg(caseIndex);
        EXPECT_EQ(failure.value(QStringLiteral("jobId")).toString(), expectedJobId);
        EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
                  QStringLiteral("Invalid proxy job request"));
        QFile existingOutput(outputPath);
        ASSERT_TRUE(existingOutput.open(QIODevice::ReadOnly));
        EXPECT_EQ(existingOutput.readAll(), previousOutput);
        QFile existingTemporaryOutput(temporaryOutputPath);
        ASSERT_TRUE(existingTemporaryOutput.open(QIODevice::ReadOnly));
        EXPECT_EQ(existingTemporaryOutput.readAll(), previousTemporary);
        ++caseIndex;
    }
}

TEST(ProxyWorkerProtocolTest, MediaFoundationAudioReencodeRejectionPreservesExistingOutput)
{
    ASSERT_FALSE(workerPath().isEmpty());
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.bin"));
    const QString outputPath = directory.filePath(QStringLiteral("proxy.mp4"));
    const QString temporaryOutputPath = directory.filePath(
        QStringLiteral("proxy.partial.mp4"));
    ASSERT_TRUE(writeBytes(sourcePath, QByteArrayLiteral("source placeholder")));
    ASSERT_TRUE(writeBytes(outputPath, QByteArrayLiteral("previous proxy")));
    ASSERT_TRUE(writeBytes(temporaryOutputPath,
                           QByteArrayLiteral("previous temporary proxy")));

    QJsonObject request = requestBase(QStringLiteral("unsupported-audio-job"), sourcePath,
                                      outputPath, temporaryOutputPath);
    request.insert(QStringLiteral("backend"), QStringLiteral("mediafoundation"));
    request.insert(QStringLiteral("audioReencode"), true);
    const QString requestPath = directory.filePath(QStringLiteral("audio.json"));
    ASSERT_TRUE(writeBytes(requestPath,
        QJsonDocument(request).toJson(QJsonDocument::Compact)));

    const WorkerRunResult result = runWorker(
        {QStringLiteral("--request"), requestPath});
    ASSERT_EQ(result.exitStatus, QProcess::NormalExit);
    EXPECT_EQ(result.exitCode, 1);
    const QJsonObject failure = firstJsonLine(result.standardOutput);
    EXPECT_EQ(failure.value(QStringLiteral("type")).toString(), QStringLiteral("failed"));
    EXPECT_EQ(failure.value(QStringLiteral("jobId")).toString(),
              QStringLiteral("unsupported-audio-job"));
    EXPECT_EQ(failure.value(QStringLiteral("reason")).toString(),
              QStringLiteral("Media Foundation backend does not support audio re-encoding"));

    QFile existingOutput(outputPath);
    ASSERT_TRUE(existingOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(existingOutput.readAll(), QByteArrayLiteral("previous proxy"));
    QFile existingTemporaryOutput(temporaryOutputPath);
    ASSERT_TRUE(existingTemporaryOutput.open(QIODevice::ReadOnly));
    EXPECT_EQ(existingTemporaryOutput.readAll(),
              QByteArrayLiteral("previous temporary proxy"));
}

} // namespace
