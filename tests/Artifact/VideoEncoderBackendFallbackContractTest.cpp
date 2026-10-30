#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <array>
#include <memory>

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

} // namespace

TEST(VideoEncoderBackendFallbackContractTest,
     AutomaticSelectionTriesHardwareVulkanNativeThenPipe)
{
    ensureApplication();
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());

    Artifact::ArtifactRenderJob job;
    job.encoderBackend = QStringLiteral("auto");
    job.codec = QStringLiteral("artifact_invalid_codec");
    job.outputPath = outputDirectory.filePath(QStringLiteral("fallback.mp4"));
    job.resolutionWidth = 64;
    job.resolutionHeight = 64;
    job.frameRate = 24.0;

    QStringList messages;
    std::unique_ptr<Artifact::IVideoEncodeBackend> backend;
    QString backendName;
    QString errorMessage;
    {
        QtMessageCapture capture(messages);
        backend = Artifact::createVideoEncodeBackend(
            job, &backendName, &errorMessage);
        if (backend) {
            QString closeError;
            EXPECT_FALSE(backend->close(&closeError))
                << "An invalid codec must not finalize a valid video stream";
        }
    }

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
