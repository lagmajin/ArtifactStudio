#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <array>
#include <memory>
#include <utility>

import Artifact.Render.Queue.Encoder;
import Artifact.Render.Queue.Job;

namespace {

QStringList* capturedMessages = nullptr;

void captureQtMessages(QtMsgType, const QMessageLogContext&,
                       const QString& message)
{
    if (capturedMessages) {
        capturedMessages->push_back(message);
    }
}

QCoreApplication* ensureApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactVideoEncoderBackendFallbackContractTest";
    static char* argv[] = {appName, nullptr};
    static QCoreApplication app(argc, argv);
    return &app;
}

class QtMessageCapture final {
public:
    explicit QtMessageCapture(QStringList& messages)
        : previousHandler_(qInstallMessageHandler(captureQtMessages))
    {
        capturedMessages = &messages;
    }

    ~QtMessageCapture()
    {
        capturedMessages = nullptr;
        qInstallMessageHandler(previousHandler_);
    }

    QtMessageCapture(const QtMessageCapture&) = delete;
    QtMessageCapture& operator=(const QtMessageCapture&) = delete;

private:
    QtMessageHandler previousHandler_ = nullptr;
};

std::unique_ptr<Artifact::IVideoEncodeBackend> createInvalidCodecBackend(
    const QString& preference, const QString& outputPath,
    QString* backendName, QString* errorMessage, QStringList* messages)
{
    ensureApplication();
    Artifact::ArtifactRenderJob job;
    job.encoderBackend = preference;
    job.codec = QStringLiteral("artifact_invalid_codec");
    job.outputPath = outputPath;
    job.resolutionWidth = 64;
    job.resolutionHeight = 64;
    job.frameRate = 24.0;

    QtMessageCapture capture(*messages);
    return Artifact::createVideoEncodeBackend(job, backendName, errorMessage);
}

void expectBackendCloseFailure(
    std::unique_ptr<Artifact::IVideoEncodeBackend>& backend)
{
    if (backend) {
        QString closeError;
        EXPECT_FALSE(backend->close(&closeError))
            << "An invalid codec must not finalize a valid video stream";
    }
}

} // namespace

TEST(VideoEncoderBackendFallbackContractTest,
     AutomaticSelectionTriesHardwareVulkanNativeThenPipe)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());

    QStringList messages;
    QString backendName;
    QString errorMessage;
    auto backend = createInvalidCodecBackend(
        QStringLiteral("auto"),
        outputDirectory.filePath(QStringLiteral("fallback.mp4")),
        &backendName, &errorMessage, &messages);
    expectBackendCloseFailure(backend);

    const QString log = messages.join(QLatin1Char('\n'));
    const std::array<QString, 5> expectedStages{
        QStringLiteral("GPU FFmpeg backend failed; falling back to native"),
        QStringLiteral("pipe-hw backend failed; falling back to pipe"),
        QStringLiteral("pipe-vulkan backend failed; falling back to pipe"),
        QStringLiteral("[Encode][Native] backend attempt failed"),
        QStringLiteral("auto backend fell back to pipe encoding")};
    int previousPosition = -1;
    for (const QString& stage : expectedStages) {
        const int position = log.indexOf(stage);
        ASSERT_GE(position, 0) << "Missing fallback stage: "
                               << stage.toStdString() << "\n" << log.toStdString();
        EXPECT_GT(position, previousPosition)
            << "Fallback stage order changed at: " << stage.toStdString()
            << "\n" << log.toStdString();
        previousPosition = position;
    }

    if (backend) {
        EXPECT_EQ(backendName, QStringLiteral("pipe"));
    } else {
        EXPECT_TRUE(backendName.isEmpty());
        EXPECT_FALSE(errorMessage.isEmpty());
    }
}

TEST(VideoEncoderBackendFallbackContractTest,
     ExplicitHardwarePreferencesFallbackDirectlyToPipe)
{
    ensureApplication();
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());

    for (const auto& [preference, expectedFailureStage] : {
             std::pair{QStringLiteral("pipe-hw"),
                       QStringLiteral("pipe-hw backend failed; falling back to pipe")},
             std::pair{QStringLiteral("pipe-vulkan"),
                       QStringLiteral("pipe-vulkan backend failed; falling back to pipe")}}) {
        QStringList messages;
        QString backendName;
        QString errorMessage;
        auto backend = createInvalidCodecBackend(
            preference,
            outputDirectory.filePath(preference + QStringLiteral(".mp4")),
            &backendName, &errorMessage, &messages);
        expectBackendCloseFailure(backend);

        const QString log = messages.join(QLatin1Char('\n'));
        ASSERT_TRUE(log.contains(expectedFailureStage)) << log.toStdString();
        EXPECT_FALSE(log.contains(
            QStringLiteral("GPU FFmpeg backend failed"))) << log.toStdString();
        EXPECT_FALSE(log.contains(
            QStringLiteral("[Encode][Native] backend attempt failed")))
            << log.toStdString();
        if (backend) {
            EXPECT_EQ(backendName, QStringLiteral("pipe"));
        } else {
            EXPECT_TRUE(backendName.isEmpty());
            EXPECT_FALSE(errorMessage.isEmpty());
        }
    }
}

TEST(VideoEncoderBackendFallbackContractTest,
     ExplicitNativePreferenceDoesNotTryOtherBackends)
{
    ensureApplication();
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());

    QStringList messages;
    QString backendName;
    QString errorMessage;
    auto backend = createInvalidCodecBackend(
        QStringLiteral("native"),
        outputDirectory.filePath(QStringLiteral("native.mp4")),
        &backendName, &errorMessage, &messages);

    EXPECT_FALSE(backend);
    EXPECT_TRUE(backendName.isEmpty());
    EXPECT_FALSE(errorMessage.isEmpty());
    const QString log = messages.join(QLatin1Char('\n'));
    EXPECT_TRUE(log.contains(
        QStringLiteral("[Encode][Native] backend attempt failed")))
        << log.toStdString();
    EXPECT_FALSE(log.contains(QStringLiteral("GPU FFmpeg backend failed")))
        << log.toStdString();
    EXPECT_FALSE(log.contains(QStringLiteral("pipe-hw backend failed")))
        << log.toStdString();
    EXPECT_FALSE(log.contains(QStringLiteral("pipe-vulkan backend failed")))
        << log.toStdString();
    EXPECT_FALSE(log.contains(QStringLiteral("backend fell back to pipe")))
        << log.toStdString();
}
